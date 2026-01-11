#include "rlutil.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <random>


// 俄羅斯方塊設定
const int FIELD_WIDTH = 12;
const int FIELD_HEIGHT = 22;

// 定義每個方塊的顏色
const int pieceColors[] = {
    rlutil::LIGHTCYAN,   // I (長條)
    rlutil::LIGHTMAGENTA,// T (T型)
    rlutil::YELLOW,      // O (正方)
    rlutil::LIGHTRED,    // Z (閃電)
    rlutil::LIGHTGREEN,  // S (反閃電)
    rlutil::BROWN,       // L (L型)
    rlutil::LIGHTBLUE    // J (反L型)
};

// 模擬 Markdown 解析與顯示的函式
void drawMarkdown(int x, int y, const std::string& text) {
    rlutil::locate(x, y);
    if (text.size() >= 2 && text.substr(0, 2) == "# ") {
        rlutil::setColor(rlutil::LIGHTCYAN);
        std::cout << text;
    } else if (text.size() >= 4 && text.substr(0, 2) == "**" && text.substr(text.size()-2) == "**") {
        rlutil::setColor(rlutil::YELLOW);
        std::cout << text.substr(2, text.size() - 4);
    } else {
        rlutil::setColor(rlutil::GREY);
        std::cout << text;
    }
    rlutil::resetColor();
}

struct HighScore {
    std::string name;
    int score;
};

class TetrisGame {
public:
    TetrisGame() : gen(rd()), dis(0, 6) {
        // 初始化方塊形狀 (4x4 字串表示)
        tetromino = {
            "..X...X...X...X.", // I
            "..X..XX...X.....", // T
            ".....XX..XX.....", // O
            "..X..XX..X......", // Z
            ".X...XX...X.....", // S
            ".X...X...XX.....", // L
            "..X...X..XX....."  // J
        };
        field.resize(FIELD_WIDTH * FIELD_HEIGHT);
    }

    void Run() {
        rlutil::saveDefaultColor();
        rlutil::setConsoleTitle("RLUtil Tetris");
        rlutil::hidecursor();

        bool bQuitGame = false;
        while (!bQuitGame) {
            ResetGame();
            GameLoop();
            bQuitGame = GameOverScreen();
        }

        rlutil::showcursor();
    }

private:
    std::vector<unsigned char> field;
    std::vector<std::string> tetromino;
    std::vector<HighScore> highScores;
    const std::string SCORE_FILE = "highscores.txt";

    // Game State
    int nCurrentPiece;
    int nNextPiece;
    int nCurrentRotation;
    int nCurrentX;
    int nCurrentY;
    int nScore;
    int nSpeed;
    int nSpeedCounter;
    bool bPaused;
    int nHoldPiece;
    bool bHeld;
    std::vector<int> vLines;

    // RNG
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;

    void ResetGame() {
        // 初始化地圖邊界
        std::fill(field.begin(), field.end(), 0);
        for (int x = 0; x < FIELD_WIDTH; x++)
            for (int y = 0; y < FIELD_HEIGHT; y++)
                field[y * FIELD_WIDTH + x] = (x == 0 || x == FIELD_WIDTH - 1 || y == FIELD_HEIGHT - 1) ? 9 : 0;

        nCurrentPiece = dis(gen);
        nNextPiece = dis(gen);
        nCurrentRotation = 0;
        nCurrentX = FIELD_WIDTH / 2 - 2;
        nCurrentY = 0;
        nScore = 0;
        nSpeed = 20;
        nSpeedCounter = 0;
        bPaused = false;
        nHoldPiece = -1;
        bHeld = false;
        vLines.clear();
    }

    int rotate(int x, int y, int r) {
        switch (r % 4) {
            case 0: return y * 4 + x;
            case 1: return 12 + y - (x * 4);
            case 2: return 15 - (y * 4) - x;
            case 3: return 3 - y + (x * 4);
        }
        return 0;
    }

    bool doesPieceFit(int nTetromino, int nRotation, int nPosX, int nPosY) {
        for (int px = 0; px < 4; px++)
            for (int py = 0; py < 4; py++) {
                int pi = rotate(px, py, nRotation);
                int posIndex = (nPosY + py) * FIELD_WIDTH + (nPosX + px);

                if (tetromino[nTetromino][pi] != '.') {
                    if (nPosX + px < 0 || nPosX + px >= FIELD_WIDTH ||
                        nPosY + py < 0 || nPosY + py >= FIELD_HEIGHT) {
                        return false;
                    }
                    if (field[posIndex] != 0)
                        return false;
                }
            }
        return true;
    }

    void DrawStaticUI() {
        drawMarkdown(30, 2, "# 俄羅斯方塊 (Tetris)");
        drawMarkdown(30, 4, "**控制方式:**");
        drawMarkdown(30, 5, "- [Arrow Keys] 移動");
        drawMarkdown(30, 6, "- [Up/Z] 旋轉");
        drawMarkdown(30, 7, "- [Down] 加速");
        drawMarkdown(30, 8, "- [Space] 快速落下");
        drawMarkdown(30, 9, "- [P] 暫停");
        drawMarkdown(30, 10, "- [C] 暫存 (Hold)");
        drawMarkdown(30, 11, "- [Esc] 結束遊戲");
        drawMarkdown(30, 16, "**下一個:**");
        drawMarkdown(30, 23, "**暫存 (Hold):**");
    }

    void GameLoop() {
        bool bGameOver = false;
        bool bForceDown = false;

        rlutil::cls();
        DrawStaticUI();

        while (!bGameOver) {
            rlutil::msleep(50);
            if (!bPaused) nSpeedCounter++;
            bForceDown = (nSpeedCounter == nSpeed);

            // Input
            if (kbhit()) {
                int k = rlutil::getkey();
                if (k == rlutil::KEY_ESCAPE) bGameOver = true;
                if (k == 'p' || k == 'P') bPaused = !bPaused;

                if (!bPaused) {
                    if (k == rlutil::KEY_RIGHT) {
                        if (doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX + 1, nCurrentY)) nCurrentX++;
                    } else if (k == rlutil::KEY_LEFT) {
                        if (doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX - 1, nCurrentY)) nCurrentX--;
                    } else if (k == rlutil::KEY_DOWN) {
                        if (doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) nCurrentY++;
                    } else if (k == 'z' || k == 'Z' || k == rlutil::KEY_UP) {
                        if (doesPieceFit(nCurrentPiece, nCurrentRotation + 1, nCurrentX, nCurrentY)) nCurrentRotation++;
                    } else if (k == rlutil::KEY_SPACE) {
                        while (doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) nCurrentY++;
                        bForceDown = true;
                    } else if (k == 'c' || k == 'C') {
                        if (!bHeld) {
                            if (nHoldPiece == -1) {
                                nHoldPiece = nCurrentPiece;
                                nCurrentPiece = nNextPiece;
                                nNextPiece = dis(gen);
                            } else {
                                std::swap(nCurrentPiece, nHoldPiece);
                            }
                            nCurrentX = FIELD_WIDTH / 2 - 2;
                            nCurrentY = 0;
                            nCurrentRotation = 0;
                            bHeld = true;
                        }
                    }
                }
            }

            // Logic
            if (!bPaused && bForceDown) {
                nSpeedCounter = 0;
                if (doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                    nCurrentY++;
                } else {
                    LockPiece();
                    CheckLines();
                    
                    // New Piece
                    nCurrentX = FIELD_WIDTH / 2 - 2;
                    nCurrentY = 0;
                    nCurrentRotation = 0;
                    nCurrentPiece = nNextPiece;
                    nNextPiece = dis(gen);
                    bHeld = false;

                    if (!doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY))
                        bGameOver = true;
                }
            }

            Draw();
            if (!vLines.empty()) AnimateLines();
        }
    }

    void LockPiece() {
        for (int px = 0; px < 4; px++)
            for (int py = 0; py < 4; py++)
                if (tetromino[nCurrentPiece][rotate(px, py, nCurrentRotation)] != '.')
                    field[(nCurrentY + py) * FIELD_WIDTH + (nCurrentX + px)] = nCurrentPiece + 1;
    }

    void CheckLines() {
        for (int py = 0; py < 4; py++) {
            if (nCurrentY + py < FIELD_HEIGHT - 1) {
                bool bLine = true;
                for (int px = 1; px < FIELD_WIDTH - 1; px++)
                    if (field[(nCurrentY + py) * FIELD_WIDTH + px] == 0) { bLine = false; break; }
                
                if (bLine) {
                    for (int px = 1; px < FIELD_WIDTH - 1; px++)
                        field[(nCurrentY + py) * FIELD_WIDTH + px] = 8;
                    vLines.push_back(nCurrentY + py);
                }
            }
        }
        if (!vLines.empty()) {
            nScore += 25;
            nScore += (1 << vLines.size()) * 100;
            int newSpeed = 20 - (nScore / 500);
            nSpeed = (newSpeed < 2) ? 2 : newSpeed;
        }
    }

    void Draw() {
        // Field
        for (int x = 0; x < FIELD_WIDTH; x++)
            for (int y = 0; y < FIELD_HEIGHT; y++) {
                rlutil::locate(2 + x * 2, 2 + y);
                int val = field[y * FIELD_WIDTH + x];
                if (val == 0) { rlutil::setColor(rlutil::BLACK); std::cout << " ."; }
                else if (val == 9) { rlutil::setColor(rlutil::GREY); std::cout << "##"; }
                else if (val == 8) { rlutil::setColor(rlutil::WHITE); std::cout << "=="; }
                else { rlutil::setColor(pieceColors[val - 1]); std::cout << "[]"; }
            }

        // Ghost
        int nGhostY = nCurrentY;
        while (doesPieceFit(nCurrentPiece, nCurrentRotation, nCurrentX, nGhostY + 1)) nGhostY++;
        DrawPiece(nCurrentPiece, nCurrentRotation, nCurrentX, nGhostY, rlutil::DARKGREY);

        // Current
        DrawPiece(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY, pieceColors[nCurrentPiece]);

        // UI
        drawMarkdown(30, 13, "**分數: " + std::to_string(nScore) + "**");
        drawMarkdown(30, 14, "**等級: " + std::to_string(1 + (nScore / 500)) + "**");
        if (bPaused) drawMarkdown(10, 10, "**   暫停   **");

        // Next
        DrawPiecePreview(nNextPiece, 32, 18);

        // Hold
        if (nHoldPiece != -1) DrawPiecePreview(nHoldPiece, 32, 25);
    }

    void DrawPiece(int piece, int rot, int x, int y, int color) {
        rlutil::setColor(color);
        for (int px = 0; px < 4; px++)
            for (int py = 0; py < 4; py++)
                if (tetromino[piece][rotate(px, py, rot)] != '.') {
                    rlutil::locate(2 + (x + px) * 2, 2 + (y + py));
                    std::cout << "[]";
                }
    }

    void DrawPiecePreview(int piece, int x, int y) {
        for (int px = 0; px < 4; px++)
            for (int py = 0; py < 4; py++) {
                rlutil::locate(x + px * 2, y + py);
                if (tetromino[piece][rotate(px, py, 0)] != '.') {
                    rlutil::setColor(pieceColors[piece]);
                    std::cout << "[]";
                } else {
                    std::cout << "  ";
                }
            }
    }

    void AnimateLines() {
        for (int i = 0; i < 6; ++i) {
            for (auto& v : vLines) {
                rlutil::locate(4, 2 + v);
                rlutil::setColor(i % 2 == 0 ? rlutil::BLACK : rlutil::WHITE);
                for (int px = 1; px < FIELD_WIDTH - 1; px++) std::cout << "==";
            }
            rlutil::msleep(80);
        }
        for (auto& v : vLines)
            for (int px = 1; px < FIELD_WIDTH - 1; px++) {
                for (int py = v; py > 0; py--)
                    field[py * FIELD_WIDTH + px] = field[(py - 1) * FIELD_WIDTH + px];
                field[px] = 0;
            }
        vLines.clear();
    }

    void LoadHighScores() {
        highScores.clear();
        std::ifstream in(SCORE_FILE);
        if (in.is_open()) {
            std::string name;
            int score;
            while (in >> name >> score) highScores.push_back({name, score});
            in.close();
        }
        std::sort(highScores.begin(), highScores.end(), [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
    }

    void SaveHighScores() {
        std::ofstream out(SCORE_FILE);
        if (out.is_open()) {
            for (const auto& hs : highScores) out << hs.name << " " << hs.score << "\n";
        }
    }

    bool GameOverScreen() {
        rlutil::cls();
        drawMarkdown(10, 10, "# Game Over");
        drawMarkdown(10, 12, "**Final Score: " + std::to_string(nScore) + "**");

        LoadHighScores();
        if (highScores.size() < 5 || nScore > highScores.back().score) {
            drawMarkdown(10, 14, "**Congratulations! New Record!**");
            rlutil::locate(10, 16);
            rlutil::setColor(rlutil::WHITE);
            std::cout << "Enter Name: ";
            
            // 清除鍵盤緩衝區，避免遊戲結束時連打的按鍵被誤讀為名字
            while (kbhit()) getch();
            
            // Improved input handling
            std::string name = "";
            rlutil::showcursor();
            while (true) {
                int k = rlutil::getkey();
                if (k == rlutil::KEY_ENTER) break;
                if (k == 8 || k == 127) { // Backspace
                    if (!name.empty()) {
                        name.pop_back();
                        std::cout << "\b \b";
                    }
                } else if (k >= 32 && k <= 126) {
                    if (name.length() < 10) {
                        name += (char)k;
                        std::cout << (char)k;
                    }
                }
            }
            rlutil::hidecursor();
            
            if (name.empty()) name = "Player";
            highScores.push_back({name, nScore});
            std::sort(highScores.begin(), highScores.end(), [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
            if (highScores.size() > 5) highScores.resize(5);
            SaveHighScores();
        }

        rlutil::cls();
        drawMarkdown(10, 2, "# Leaderboard");
        for (size_t i = 0; i < highScores.size(); ++i) {
            drawMarkdown(10, 4 + i, std::to_string(i + 1) + ". " + highScores[i].name + " : " + std::to_string(highScores[i].score));
        }
        drawMarkdown(10, 12, "**Press [R] to Restart, or [ESC] to Quit**");

        while (true) {
            rlutil::msleep(10); // 避免忙碌迴圈佔用過多 CPU
            if (kbhit()) {
                int k = rlutil::getkey();
                if (k == 'r' || k == 'R') return false;
                if (k == rlutil::KEY_ESCAPE) return true;
            }
        }
    }
};

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001); // 設定控制台為 UTF-8 編碼，解決中文亂碼問題
#endif
    TetrisGame game;
    game.Run();
    return 0;
}