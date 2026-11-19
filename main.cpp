#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstdio>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

constexpr float BOX_W = 600.0f;
constexpr float BOX_H = 800.0f;
constexpr float GRAVITY = 2000.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;
constexpr int SUBSTEPS = 8;
constexpr float RESTITUTION = 0.2f;
constexpr float FRICTION = 0.99f;
constexpr float DROP_CD = 0.5f;

const Color BG_COLOR = { 10, 15, 30, 255 };
const Color PANEL_BG = { 15, 20, 40, 255 };
const Color BOX_COLOR = { 40, 60, 100, 255 };
const Color NEON_CYAN = { 0, 255, 255, 255 };

const Color LVL_C[10] = {
    { 0, 255, 255, 255 },
    { 255, 0, 255, 255 },
    { 0, 255, 0, 255 },
    { 255, 255, 0, 255 },
    { 255, 165, 0, 255 },
    { 255, 105, 180, 255 },
    { 138, 43, 226, 255 },
    { 0, 250, 154, 255 },
    { 255, 215, 0, 255 },
    { 255, 255, 255, 255 }
};

const float LVL_R[10] = { 16.0f, 21.0f, 27.0f, 35.0f, 46.0f, 60.0f, 78.0f, 101.0f, 131.0f, 170.0f };

#if defined(PLATFORM_WEB)
EM_JS(int, GetTotalPlayersJS, (), {
    return window.globalTotalPlayers || 0;
});

EM_JS(void, GetRecentPlayerJS, (int index, char* buffer, int maxLen), {
    var name = "";
    if (window.globalRecentPlayers && window.globalRecentPlayers.length > index) {
        name = window.globalRecentPlayers[index] || "";
    }
    for (var i = 0; i < name.length && i < maxLen - 1; i++) {
        HEAP8[buffer + i] = name.charCodeAt(i);
    }
    HEAP8[buffer + Math.min(name.length, maxLen - 1)] = 0;
});
#else
int GetTotalPlayersJS() { return 1204; }
void GetRecentPlayerJS(int index, char* buffer, int maxLen) {
    std::vector<std::string> mock = {"Sabtain05", "NeonKing", "ShapeMaster", "VercelDev", "Player_992"};
    std::string name = (index < 5) ? mock[index] : "";
    snprintf(buffer, maxLen, "%s", name.c_str());
}
#endif

enum State { TITLE, PLAYING, GAMEOVER };

struct Circle {
    Vector2 p, v;
    int lvl;
    float r, m;
    bool dead;
    float scaleAnim;
};

struct Particle {
    Vector2 p, v;
    Color c;
    float life, maxLife, r;
};

struct FloatingText {
    Vector2 p;
    std::string text;
    Color c;
    float life, maxLife;
};

struct MergeEvent {
    Vector2 p;
    int nextLvl;
    Vector2 v1, v2;
};

class Game {
public:
    State state = TITLE;
    std::vector<Circle> circles;
    std::vector<Particle> particles;
    std::vector<FloatingText> texts;
    
    RenderTexture2D target;
    int scrW = 1020;
    int scrH = 960;
    bool isPortrait = false;
    Vector2 vMouse = { 0, 0 };

    int nextLvl = 0;
    float dropTimer = 0.0f;
    int score = 0;
    int highScore = 0;
    int combo = 0;
    float comboTimer = 0.0f;
    float dangerTimer = 0.0f;
    float screenShake = 0.0f;
    float acc = 0.0f;

    Game() {
        isPortrait = GetScreenHeight() > GetScreenWidth();
        scrW = isPortrait ? 720 : 1020;
        scrH = isPortrait ? 1200 : 960;
        target = LoadRenderTexture(scrW, scrH);
        SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        LoadScore();
        PickNext();
    }

    ~Game() {
        UnloadRenderTexture(target);
    }

    void CheckOrientation() {
        bool nowPortrait = GetScreenHeight() > GetScreenWidth();
        if (nowPortrait != isPortrait) {
            isPortrait = nowPortrait;
            scrW = isPortrait ? 720 : 1020;
            scrH = isPortrait ? 1200 : 960;
            UnloadRenderTexture(target);
            target = LoadRenderTexture(scrW, scrH);
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        }
    }

    float GetBoxX() const { return (720.0f - BOX_W) / 2.0f; }
    float GetBoxY() const { return 100.0f; }
    float GetDangerY() const { return GetBoxY() + 80.0f; }

    void LoadScore() {
#if !defined(PLATFORM_WEB)
        std::ifstream f("highscore.dat");
        if (f.is_open()) f >> highScore;
#endif
    }

    void SaveScore() {
#if !defined(PLATFORM_WEB)
        std::ofstream f("highscore.dat");
        if (f.is_open()) f << highScore;
#endif
    }

    void PickNext() {
        nextLvl = GetRandomValue(0, 4);
    }

    void SpawnParticles(Vector2 p, Color c, int count) {
        for (int i = 0; i < count; i++) {
            float ang = GetRandomValue(0, 360) * DEG2RAD;
            float spd = GetRandomValue(50, 200);
            particles.push_back({
                p,
                { cosf(ang) * spd, sinf(ang) * spd },
                c,
                1.0f, 1.0f,
                (float)GetRandomValue(3, 8)
            });
        }
    }

    void AddText(Vector2 p, std::string t, Color c) {
        texts.push_back({ p, t, c, 1.0f, 1.0f });
    }

    void Reset() {
        circles.clear();
        particles.clear();
        texts.clear();
        score = 0;
        combo = 0;
        comboTimer = 0.0f;
        dangerTimer = 0.0f;
        dropTimer = 0.0f;
        screenShake = 0.0f;
        state = PLAYING;
        PickNext();
    }

    void Update() {
        CheckOrientation();

        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        float scale = fmin((float)GetScreenWidth() / scrW, (float)GetScreenHeight() / scrH);
        Vector2 rawInput = (GetTouchPointCount() > 0) ? GetTouchPosition(0) : GetMousePosition();
        vMouse.x = (rawInput.x - (GetScreenWidth() - (scrW * scale)) * 0.5f) / scale;
        vMouse.y = (rawInput.y - (GetScreenHeight() - (scrH * scale)) * 0.5f) / scale;

        if (state == TITLE) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_SPACE) || GetTouchPointCount() > 0) Reset();
            return;
        }

        if (state == GAMEOVER) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_R) || GetTouchPointCount() > 0) Reset();
            return;
        }

        if (IsKeyPressed(KEY_R)) Reset();

        if (screenShake > 0.0f) {
            screenShake -= dt * 60.0f;
            if (screenShake < 0.0f) screenShake = 0.0f;
        }

        if (comboTimer > 0.0f) {
            comboTimer -= dt;
            if (comboTimer <= 0.0f) combo = 0;
        }

        if (dropTimer > 0.0f) dropTimer -= dt;

        float boxX = GetBoxX();
        float boxY = GetBoxY();
        float dangerY = GetDangerY();
        float hoverX = Clamp(vMouse.x, boxX + LVL_R[nextLvl] + 5, boxX + BOX_W - LVL_R[nextLvl] - 5);

        bool inPlayZone = isPortrait ? (vMouse.y < 950.0f) : (vMouse.x < 720.0f);
        if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetTouchPointCount() > 0) && dropTimer <= 0.0f && inPlayZone) {
            circles.push_back({
                { hoverX, boxY - LVL_R[nextLvl] },
                { 0, 0 },
                nextLvl,
                LVL_R[nextLvl],
                LVL_R[nextLvl] * LVL_R[nextLvl],
                false,
                0.0f
            });
            dropTimer = DROP_CD;
            PickNext();
        }

        acc += dt;
        while (acc >= FIXED_DT) {
            StepPhysics(FIXED_DT);
            acc -= FIXED_DT;
        }

        bool danger = false;
        for (auto& c : circles) {
            if (c.scaleAnim < 1.0f) {
                c.scaleAnim += dt * 5.0f;
                if (c.scaleAnim > 1.0f) c.scaleAnim = 1.0f;
            }
            if (!c.dead && c.p.y - c.r < dangerY && c.v.y > -50.0f && c.v.y < 50.0f && c.p.y > boxY) {
                danger = true;
            }
        }

        if (danger) {
            dangerTimer += dt;
            if (dangerTimer > 2.0f) {
                state = GAMEOVER;
                if (score > highScore) {
                    highScore = score;
                    SaveScore();
                }
            }
        } else {
            dangerTimer = 0.0f;
        }

        for (auto& p : particles) {
            p.p.x += p.v.x * dt;
            p.p.y += p.v.y * dt;
            p.life -= dt;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p){ return p.life <= 0.0f; }), particles.end());

        for (auto& t : texts) {
            t.p.y -= 30.0f * dt;
            t.life -= dt;
        }
        texts.erase(std::remove_if(texts.begin(), texts.end(), [](const FloatingText& t){ return t.life <= 0.0f; }), texts.end());
    }

    void StepPhysics(float dt) {
        float sdt = dt / SUBSTEPS;
        float boxX = GetBoxX();
        float boxY = GetBoxY();
        std::vector<MergeEvent> merges;

        for (int step = 0; step < SUBSTEPS; step++) {
            for (auto& c : circles) {
                if (c.dead) continue;
                c.v.y += GRAVITY * sdt;
                c.p.x += c.v.x * sdt;
                c.p.y += c.v.y * sdt;

                if (c.p.x - c.r < boxX) {
                    c.p.x = boxX + c.r;
                    c.v.x *= -RESTITUTION;
                    c.v.y *= FRICTION;
                } else if (c.p.x + c.r > boxX + BOX_W) {
                    c.p.x = boxX + BOX_W - c.r;
                    c.v.x *= -RESTITUTION;
                    c.v.y *= FRICTION;
                }

                if (c.p.y + c.r > boxY + BOX_H) {
                    c.p.y = boxY + BOX_H - c.r;
                    c.v.y *= -RESTITUTION;
                    c.v.x *= FRICTION;
                }
            }

            for (size_t i = 0; i < circles.size(); i++) {
                if (circles[i].dead) continue;
                for (size_t j = i + 1; j < circles.size(); j++) {
                    if (circles[j].dead) continue;

                    Vector2 d = Vector2Subtract(circles[j].p, circles[i].p);
                    float dist2 = Vector2LengthSqr(d);
                    float rSum = circles[i].r + circles[j].r;

                    if (dist2 < rSum * rSum) {
                        float dist = sqrtf(dist2);
                        if (dist == 0.0f) dist = 0.1f;

                        if (circles[i].lvl == circles[j].lvl) {
                            circles[i].dead = true;
                            circles[j].dead = true;
                            merges.push_back({
                                Vector2Scale(Vector2Add(circles[i].p, circles[j].p), 0.5f),
                                circles[i].lvl + 1,
                                circles[i].v, circles[j].v
                            });
                            continue;
                        }

                        Vector2 n = Vector2Scale(d, 1.0f / dist);
                        float overlap = rSum - dist;

                        float totalM = circles[i].m + circles[j].m;
                        float ratioI = circles[j].m / totalM;
                        float ratioJ = circles[i].m / totalM;

                        circles[i].p = Vector2Subtract(circles[i].p, Vector2Scale(n, overlap * ratioI));
                        circles[j].p = Vector2Add(circles[j].p, Vector2Scale(n, overlap * ratioJ));

                        Vector2 relV = Vector2Subtract(circles[j].v, circles[i].v);
                        float vn = Vector2DotProduct(relV, n);

                        if (vn < 0.0f) {
                            float jImpulse = -(1.0f + RESTITUTION) * vn / (1.0f / circles[i].m + 1.0f / circles[j].m);
                            Vector2 impulse = Vector2Scale(n, jImpulse);
                            circles[i].v = Vector2Subtract(circles[i].v, Vector2Scale(impulse, 1.0f / circles[i].m));
                            circles[j].v = Vector2Add(circles[j].v, Vector2Scale(impulse, 1.0f / circles[j].m));
                        }
                    }
                }
            }
        }

        circles.erase(std::remove_if(circles.begin(), circles.end(), [](const Circle& c){ return c.dead; }), circles.end());

        for (const auto& m : merges) {
            combo++;
            comboTimer = 1.0f;
            int pts = m.nextLvl * m.nextLvl * 10 * combo;
            score += pts;
            
            SpawnParticles(m.p, LVL_C[m.nextLvl - 1], 20);
            AddText(m.p, "+" + std::to_string(pts) + (combo > 1 ? " (x" + std::to_string(combo) + ")" : ""), LVL_C[m.nextLvl - 1]);

            if (m.nextLvl >= 8) screenShake += (m.nextLvl - 7) * 5.0f;

            if (m.nextLvl < 10) {
                Vector2 avgV = Vector2Scale(Vector2Add(m.v1, m.v2), 0.5f);
                avgV.y -= 250.0f;
                circles.push_back({
                    m.p, avgV, m.nextLvl, LVL_R[m.nextLvl], LVL_R[m.nextLvl] * LVL_R[m.nextLvl], false, 0.0f
                });
            } else {
                screenShake += 20.0f;
                score += 10000;
                AddText(m.p, "SUPERNOVA! +10000", WHITE);
                SpawnParticles(m.p, WHITE, 50);
            }
        }
    }

    void DrawGlowCircle(Vector2 p, float r, Color c, float scale) {
        float sr = r * scale;
        DrawCircleV(p, sr * 1.2f, Fade(c, 0.2f));
        DrawCircleV(p, sr * 1.1f, Fade(c, 0.4f));
        DrawCircleV(p, sr, c);
        DrawCircleLines((int)p.x, (int)p.y, sr, WHITE);
    }

    void Draw() {
        BeginTextureMode(target);
        ClearBackground(BG_COLOR);

        float boxX = GetBoxX();
        float boxY = GetBoxY();
        float dangerY = GetDangerY();

        Vector2 camOff = { 0, 0 };
        if (screenShake > 0.0f) {
            camOff.x = GetRandomValue(-100, 100) * 0.01f * screenShake;
            camOff.y = GetRandomValue(-100, 100) * 0.01f * screenShake;
        }

        DrawRectangleRounded({ boxX + camOff.x - 5, boxY + camOff.y - 5, BOX_W + 10, BOX_H + 10 }, 0.1f, 10, BOX_COLOR);
        DrawRectangleRounded({ boxX + camOff.x, boxY + camOff.y, BOX_W, BOX_H }, 0.1f, 10, BG_COLOR);

        DrawLineEx({ boxX + camOff.x, dangerY + camOff.y }, { boxX + BOX_W + camOff.x, dangerY + camOff.y }, 2, Fade(RED, 0.5f + 0.5f * sinf(GetTime() * 5.0f)));

        for (const auto& c : circles) {
            DrawGlowCircle(Vector2Add(c.p, camOff), c.r, LVL_C[c.lvl], c.scaleAnim);
            DrawText(std::to_string(c.lvl + 1).c_str(), (int)(c.p.x + camOff.x - 5), (int)(c.p.y + camOff.y - 10), 20, WHITE);
        }

        for (const auto& p : particles) {
            DrawCircleV(Vector2Add(p.p, camOff), p.r * (p.life / p.maxLife), p.c);
        }

        if (state == PLAYING && dropTimer <= 0.0f) {
            float hoverX = Clamp(vMouse.x, boxX + LVL_R[nextLvl] + 5, boxX + BOX_W - LVL_R[nextLvl] - 5);
            DrawGlowCircle({ hoverX + camOff.x, boxY - LVL_R[nextLvl] + camOff.y - 15 }, LVL_R[nextLvl], LVL_C[nextLvl], 1.0f);
            DrawLineEx({ hoverX + camOff.x, boxY + camOff.y }, { hoverX + camOff.x, boxY + BOX_H + camOff.y }, 1, Fade(LVL_C[nextLvl], 0.3f));
        }

        for (const auto& t : texts) {
            DrawText(t.text.c_str(), (int)(t.p.x + camOff.x - MeasureText(t.text.c_str(), 20) / 2), (int)(t.p.y + camOff.y), 20, Fade(t.c, t.life / t.maxLife));
        }

        DrawText(TextFormat("SCORE: %d", score), 20, 20, 30, WHITE);
        DrawText(TextFormat("HIGH: %d", highScore), 20, 58, 20, GRAY);
        
        DrawText("NEXT", 720 - 110, 20, 20, WHITE);
        DrawGlowCircle({ 720 - 65.0f, 65.0f }, LVL_R[nextLvl] * 0.45f, LVL_C[nextLvl], 1.0f);

        if (!isPortrait) {
            float panelX = 720.0f;
            float panelW = 300.0f;
            DrawRectangle((int)panelX, 0, (int)panelW, scrH, PANEL_BG);
            DrawLine((int)panelX, 0, (int)panelX, scrH, Fade(CYAN, 0.5f));
            
            DrawText("GLOBAL NETWORK", (int)panelX + 25, 40, 20, CYAN);
            DrawLine((int)panelX + 25, 65, (int)(panelX + panelW - 25), 65, Fade(CYAN, 0.3f));
            
            DrawText("Total Players:", (int)panelX + 25, 90, 20, GRAY);
            DrawText(TextFormat("%d", GetTotalPlayersJS()), (int)panelX + 25, 120, 40, WHITE);

            DrawText("Recent Players:", (int)panelX + 25, 200, 20, GRAY);
            for (int i = 0; i < 5; i++) {
                char nameBuf[32] = {0};
                GetRecentPlayerJS(i, nameBuf, 32);
                DrawText(nameBuf, (int)panelX + 25, 240 + (i * 35), 20, WHITE);
            }

            DrawText("A Sabtain Ali production", 720 / 2 - MeasureText("A Sabtain Ali production", 20) / 2, scrH - 35, 20, Fade(WHITE, 0.4f));
        } else {
            float panelY = 930.0f;
            DrawRectangle(0, (int)panelY, scrW, scrH - (int)panelY, PANEL_BG);
            DrawLine(0, (int)panelY, scrW, (int)panelY, Fade(CYAN, 0.5f));

            DrawText("GLOBAL NETWORK", 40, (int)panelY + 20, 22, CYAN);
            DrawText("Total Players:", 40, (int)panelY + 60, 20, GRAY);
            DrawText(TextFormat("%d", GetTotalPlayersJS()), 40, (int)panelY + 90, 44, WHITE);

            DrawText("Recent Players:", 340, (int)panelY + 20, 20, GRAY);
            for (int i = 0; i < 4; i++) {
                char nameBuf[32] = {0};
                GetRecentPlayerJS(i, nameBuf, 32);
                DrawText(nameBuf, 340, (int)panelY + 55 + (i * 32), 22, WHITE);
            }

            DrawText("A Sabtain Ali production", scrW / 2 - MeasureText("A Sabtain Ali production", 20) / 2, scrH - 32, 20, Fade(WHITE, 0.4f));
        }

        if (state == TITLE) {
            DrawRectangle(0, 0, 720, isPortrait ? 930 : scrH, Fade(BLACK, 0.7f));
            DrawText("NEON SHAPE-MERGE", 720 / 2 - MeasureText("NEON SHAPE-MERGE", 40) / 2, 420, 40, {0, 255, 255, 255});
            DrawText("TAP TO START", 720 / 2 - MeasureText("TAP TO START", 24) / 2, 500, 24, WHITE);
        } else if (state == GAMEOVER) {
            DrawRectangle(0, 0, 720, isPortrait ? 930 : scrH, Fade(BLACK, 0.8f));
            DrawText("GAME OVER", 720 / 2 - MeasureText("GAME OVER", 50) / 2, 400, 50, RED);
            DrawText(TextFormat("FINAL SCORE: %d", score), 720 / 2 - MeasureText(TextFormat("FINAL SCORE: %d", score), 30) / 2, 480, 30, WHITE);
            DrawText("TAP TO RESTART", 720 / 2 - MeasureText("TAP TO RESTART", 22) / 2, 540, 22, GRAY);
        }

        EndTextureMode();
    }
};

Game* game = nullptr;

void UpdateDrawFrame() {
    game->Update();
    game->Draw();

    BeginDrawing();
    ClearBackground(BG_COLOR);
    float scale = fmin((float)GetScreenWidth() / game->scrW, (float)GetScreenHeight() / game->scrH);
    
    DrawTexturePro(game->target.texture,
        { 0.0f, 0.0f, (float)game->target.texture.width, (float)-game->target.texture.height },
        { (GetScreenWidth() - ((float)game->scrW * scale)) * 0.5f, (GetScreenHeight() - ((float)game->scrH * scale)) * 0.5f, (float)game->scrW * scale, (float)game->scrH * scale },
        { 0, 0 }, 0.0f, WHITE);
        
    EndDrawing();
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(1020, 960, "Neon Shape-Merge");
    SetTargetFPS(60);

    game = new Game();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    delete game;
    CloseWindow();
    return 0;
}