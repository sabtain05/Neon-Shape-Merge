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

constexpr int SCR_W = 1020; 
constexpr int SCR_H = 960;
constexpr int GAME_AREA_W = 720;
constexpr float PANEL_X = 720.0f;
constexpr float PANEL_W = 300.0f;

constexpr float BOX_W = 600.0f;
constexpr float BOX_H = 800.0f;
constexpr float BOX_X = (GAME_AREA_W - BOX_W) / 2.0f;
constexpr float BOX_Y = (SCR_H - BOX_H) / 2.0f + 20.0f;
constexpr float DANGER_Y = BOX_Y + 80.0f;
constexpr float GRAVITY = 2000.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;
constexpr int SUBSTEPS = 8;
constexpr float RESTITUTION = 0.2f;
constexpr float FRICTION = 0.99f;
constexpr float DROP_CD = 0.5f;

const Color BG_COLOR = { 10, 15, 30, 255 };
const Color PANEL_BG = { 15, 20, 40, 255 };
const Color BOX_COLOR = { 40, 60, 100, 255 };
const Color CYAN = { 0, 255, 255, 255 };

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

    int totalGlobalPlayers = 1204;
    std::vector<std::string> recentPlayers = {
        "Sabtain05", "NeonKing", "ShapeMaster", "VercelDev", "Player_992"
    };

    Game() {
        target = LoadRenderTexture(SCR_W, SCR_H);
        SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        LoadScore();
        PickNext();
    }

    ~Game() {
        UnloadRenderTexture(target);
    }

    void LoadScore() {
#if !defined(PLATFORM_WEB)
        std::ifstream f("highscore.dat");
        if (f.is_open()) {
            f >> highScore;
        }
#endif
    }

    void SaveScore() {
#if !defined(PLATFORM_WEB)
        std::ofstream f("highscore.dat");
        if (f.is_open()) {
            f << highScore;
        }
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
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        float scale = fmin((float)GetScreenWidth() / SCR_W, (float)GetScreenHeight() / SCR_H);
        Vector2 rawMouse = GetMousePosition();
        vMouse.x = (rawMouse.x - (GetScreenWidth() - (SCR_W * scale)) * 0.5f) / scale;
        vMouse.y = (rawMouse.y - (GetScreenHeight() - (SCR_H * scale)) * 0.5f) / scale;

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

        float hoverX = Clamp(vMouse.x, BOX_X + LVL_R[nextLvl] + 5, BOX_X + BOX_W - LVL_R[nextLvl] - 5);

        if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || GetTouchPointCount() > 0) && dropTimer <= 0.0f) {
            circles.push_back({
                { hoverX, BOX_Y - LVL_R[nextLvl] },
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
            if (!c.dead && c.p.y - c.r < DANGER_Y && c.v.y > -50.0f && c.v.y < 50.0f && c.p.y > BOX_Y) {
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
        std::vector<MergeEvent> merges;

        for (int step = 0; step < SUBSTEPS; step++) {
            for (auto& c : circles) {
                if (c.dead) continue;
                c.v.y += GRAVITY * sdt;
                c.p.x += c.v.x * sdt;
                c.p.y += c.v.y * sdt;

                if (c.p.x - c.r < BOX_X) {
                    c.p.x = BOX_X + c.r;
                    c.v.x *= -RESTITUTION;
                    c.v.y *= FRICTION;
                } else if (c.p.x + c.r > BOX_X + BOX_W) {
                    c.p.x = BOX_X + BOX_W - c.r;
                    c.v.x *= -RESTITUTION;
                    c.v.y *= FRICTION;
                }

                if (c.p.y + c.r > BOX_Y + BOX_H) {
                    c.p.y = BOX_Y + BOX_H - c.r;
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
        DrawCircleLines(p.x, p.y, sr, WHITE);
    }

    void Draw() {
        BeginTextureMode(target);
        ClearBackground(BG_COLOR);

        Vector2 camOff = { 0, 0 };
        if (screenShake > 0.0f) {
            camOff.x = GetRandomValue(-100, 100) * 0.01f * screenShake;
            camOff.y = GetRandomValue(-100, 100) * 0.01f * screenShake;
        }

        DrawRectangleRounded({ BOX_X + camOff.x - 5, BOX_Y + camOff.y - 5, BOX_W + 10, BOX_H + 10 }, 0.1f, 10, BOX_COLOR);
        DrawRectangleRounded({ BOX_X + camOff.x, BOX_Y + camOff.y, BOX_W, BOX_H }, 0.1f, 10, BG_COLOR);

        DrawLineEx({ BOX_X + camOff.x, DANGER_Y + camOff.y }, { BOX_X + BOX_W + camOff.x, DANGER_Y + camOff.y }, 2, Fade(RED, 0.5f + 0.5f * sinf(GetTime() * 5.0f)));

        for (const auto& c : circles) {
            DrawGlowCircle(Vector2Add(c.p, camOff), c.r, LVL_C[c.lvl], c.scaleAnim);
            DrawText(std::to_string(c.lvl + 1).c_str(), c.p.x + camOff.x - 5, c.p.y + camOff.y - 10, 20, WHITE);
        }

        for (const auto& p : particles) {
            DrawCircleV(Vector2Add(p.p, camOff), p.r * (p.life / p.maxLife), p.c);
        }

        if (state == PLAYING && dropTimer <= 0.0f && vMouse.x < GAME_AREA_W) {
            float hoverX = Clamp(vMouse.x, BOX_X + LVL_R[nextLvl] + 5, BOX_X + BOX_W - LVL_R[nextLvl] - 5);
            DrawGlowCircle({ hoverX + camOff.x, BOX_Y - LVL_R[nextLvl] + camOff.y - 20 }, LVL_R[nextLvl], LVL_C[nextLvl], 1.0f);
            DrawLineEx({ hoverX + camOff.x, BOX_Y + camOff.y }, { hoverX + camOff.x, BOX_Y + BOX_H + camOff.y }, 1, Fade(LVL_C[nextLvl], 0.3f));
        }

        for (const auto& t : texts) {
            DrawText(t.text.c_str(), t.p.x + camOff.x - MeasureText(t.text.c_str(), 20) / 2, t.p.y + camOff.y, 20, Fade(t.c, t.life / t.maxLife));
        }

        DrawText(TextFormat("SCORE: %d", score), 20, 20, 30, WHITE);
        DrawText(TextFormat("HIGH: %d", highScore), 20, 60, 20, GRAY);
        
        DrawText("NEXT", GAME_AREA_W - 100, 20, 20, WHITE);
        DrawGlowCircle({ GAME_AREA_W - 60.0f, 70.0f }, LVL_R[nextLvl] * 0.5f, LVL_C[nextLvl], 1.0f);

        DrawText("A Sabtain Ali production", GAME_AREA_W / 2 - MeasureText("A Sabtain Ali production", 20) / 2, SCR_H - 30, 20, Fade(WHITE, 0.4f));

        DrawRectangle(PANEL_X, 0, PANEL_W, SCR_H, PANEL_BG);
        DrawLine(PANEL_X, 0, PANEL_X, SCR_H, Fade(CYAN, 0.5f));
        
        DrawText("GLOBAL NETWORK", PANEL_X + 25, 40, 20, CYAN);
        DrawLine(PANEL_X + 25, 65, PANEL_X + PANEL_W - 25, 65, Fade(CYAN, 0.3f));
        
        DrawText("Total Players:", PANEL_X + 25, 90, 20, GRAY);
        DrawText(TextFormat("%d", GetTotalPlayersJS()), PANEL_X + 25, 120, 40, WHITE);

        DrawText("Recent Players:", PANEL_X + 25, 200, 20, GRAY);
        for(int i = 0; i < 5; i++) {
            char nameBuf[32] = {0};
            GetRecentPlayerJS(i, nameBuf, 32);
            DrawText(nameBuf, PANEL_X + 25, 240 + (i * 35), 20, WHITE);
        }

        if (state == TITLE) {
            DrawRectangle(0, 0, GAME_AREA_W, SCR_H, Fade(BLACK, 0.7f));
            DrawText("NEON SHAPE-MERGE", GAME_AREA_W / 2 - MeasureText("NEON SHAPE-MERGE", 40) / 2, SCR_H / 2 - 100, 40, {0, 255, 255, 255});
            DrawText("CLICK TO START", GAME_AREA_W / 2 - MeasureText("CLICK TO START", 20) / 2, SCR_H / 2, 20, WHITE);
        } else if (state == GAMEOVER) {
            DrawRectangle(0, 0, GAME_AREA_W, SCR_H, Fade(BLACK, 0.8f));
            DrawText("GAME OVER", GAME_AREA_W / 2 - MeasureText("GAME OVER", 50) / 2, SCR_H / 2 - 100, 50, RED);
            DrawText(TextFormat("FINAL SCORE: %d", score), GAME_AREA_W / 2 - MeasureText(TextFormat("FINAL SCORE: %d", score), 30) / 2, SCR_H / 2 - 20, 30, WHITE);
            DrawText("CLICK TO RESTART", GAME_AREA_W / 2 - MeasureText("CLICK TO RESTART", 20) / 2, SCR_H / 2 + 40, 20, GRAY);
        }

        EndTextureMode();
    }
};

Game* game = nullptr;

void UpdateDrawFrame() {
    game->Update();
    game->Draw();

    BeginDrawing();
    ClearBackground(BLACK);
    float scale = fmin((float)GetScreenWidth() / SCR_W, (float)GetScreenHeight() / SCR_H);
    
    DrawTexturePro(game->target.texture,
        { 0.0f, 0.0f, (float)game->target.texture.width, (float)-game->target.texture.height },
        { (GetScreenWidth() - ((float)SCR_W * scale)) * 0.5f, (GetScreenHeight() - ((float)SCR_H * scale)) * 0.5f, (float)SCR_W * scale, (float)SCR_H * scale },
        { 0, 0 }, 0.0f, WHITE);
        
    EndDrawing();
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCR_W, SCR_H, "Neon Shape-Merge");
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