#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <string>
#include <unordered_map>
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

const Color DARK_BG        = { 10, 15, 30, 255 };
const Color DARK_PANEL_BG  = { 15, 20, 40, 255 };
const Color DARK_BOX_COLOR = { 40, 60, 100, 255 };
const Color DARK_ACCENT    = { 0, 255, 255, 255 };

const Color LIGHT_BG        = { 242, 245, 252, 255 };
const Color LIGHT_PANEL_BG  = { 228, 234, 246, 255 };
const Color LIGHT_BOX_COLOR = { 180, 194, 220, 255 };
const Color LIGHT_ACCENT    = { 0, 132, 210, 255 };

const Color LVL_C_DARK[10] = {
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

const Color LVL_C_LIGHT[10] = {
    { 0, 188, 212, 255 },
    { 216, 27, 96, 255 },
    { 46, 160, 67, 255 },
    { 230, 175, 0, 255 },
    { 239, 108, 0, 255 },
    { 225, 65, 135, 255 },
    { 115, 45, 200, 255 },
    { 0, 168, 120, 255 },
    { 212, 155, 0, 255 },
    { 55, 65, 85, 255 }
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

EM_JS(void, SetPageThemeJS, (int isLight), {
    var bg = isLight ? "#f2f5fc" : "#0a0f1e";
    document.documentElement.style.backgroundColor = bg;
    document.body.style.backgroundColor = bg;
    var c = document.getElementById('canvas');
    if (c) c.style.backgroundColor = bg;
});

EM_JS(float, GetSafeInsetTopJS, (), {
    return window.safeInsetTop || 0;
});

EM_JS(float, GetSafeInsetBottomJS, (), {
    return window.safeInsetBottom || 0;
});

EM_JS(void, RenderSystemFontJS, (const char* textPtr, int fontSize, int bold, int* outW, int* outH, unsigned char** outPixels), {
    var text = UTF8ToString(textPtr);
    if (!text) text = " ";
    var canvas = window._fontCanvas || (window._fontCanvas = document.createElement('canvas'));
    var ctx = canvas.getContext('2d', { willReadFrequently: true });
    var weight = bold ? "700 " : "500 ";
    var fontStr = weight + fontSize + 'px -apple-system, BlinkMacSystemFont, "SF Pro Display", "SF Pro Text", "Helvetica Neue", Arial, sans-serif';
    ctx.font = fontStr;
    var metrics = ctx.measureText(text);
    var w = Math.max(2, Math.ceil(metrics.width) + 8);
    var h = Math.max(2, Math.ceil(fontSize * 1.35));
    canvas.width = w;
    canvas.height = h;
    ctx.clearRect(0, 0, w, h);
    ctx.font = fontStr;
    ctx.fillStyle = "#ffffff";
    ctx.textBaseline = "middle";
    ctx.fillText(text, 4, h / 2);
    var imgData = ctx.getImageData(0, 0, w, h).data;
    var ptr = _malloc(w * h * 4);
    HEAPU8.set(imgData, ptr);
    HEAP32[outW >> 2] = w;
    HEAP32[outH >> 2] = h;
    HEAP32[outPixels >> 2] = ptr;
});
#else
int GetTotalPlayersJS() { return 1204; }
void GetRecentPlayerJS(int index, char* buffer, int maxLen) {
    std::vector<std::string> mock = {"Sabtain05", "NeonKing", "ShapeMaster", "VercelDev", "Player_992"};
    std::string name = (index < 5) ? mock[index] : "";
    snprintf(buffer, maxLen, "%s", name.c_str());
}
void SetPageThemeJS(int isLight) { (void)isLight; }
float GetSafeInsetTopJS() { return 0.0f; }
float GetSafeInsetBottomJS() { return 0.0f; }
#endif

class SystemFontRenderer {
    std::unordered_map<std::string, Texture2D> cache;
public:
    ~SystemFontRenderer() {
        Clear();
    }

    void Clear() {
        for (auto& kv : cache) {
            UnloadTexture(kv.second);
        }
        cache.clear();
    }

    void Draw(const std::string& text, float x, float y, int size, Color color, bool center = false, bool bold = false) {
        if (text.empty()) return;
#if defined(PLATFORM_WEB)
        std::string key = std::to_string(size) + (bold ? "B:" : "R:") + text;
        auto it = cache.find(key);
        if (it == cache.end()) {
            if (cache.size() > 250) Clear();
            int w = 0, h = 0;
            unsigned char* pixels = nullptr;
            RenderSystemFontJS(text.c_str(), size, bold ? 1 : 0, &w, &h, &pixels);
            Image img = { pixels, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
            Texture2D tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            UnloadImage(img);
            it = cache.emplace(key, tex).first;
        }
        Texture2D tex = it->second;
        float drawX = center ? (x - tex.width * 0.5f) : (x - 4.0f);
        float drawY = y - (tex.height - size) * 0.5f;
        DrawTexture(tex, (int)drawX, (int)drawY, color);
#else
        int w = MeasureText(text.c_str(), size);
        float drawX = center ? (x - w * 0.5f) : x;
        DrawText(text.c_str(), (int)drawX, (int)y, size, color);
#endif
    }
};

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
    bool isLightMode = false;
    std::vector<Circle> circles;
    std::vector<Particle> particles;
    std::vector<FloatingText> texts;
    SystemFontRenderer font;
    
    RenderTexture2D target;
    int scrW = 1020;
    int scrH = 960;
    int lastWinW = 0;
    int lastWinH = 0;
    bool isPortrait = false;
    float topPad = 0.0f;
    float bottomPad = 0.0f;
    Vector2 vMouse = { 360.0f, 300.0f };
    bool wasTouching = false;

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
        UpdateViewportMetrics(true);
        LoadScore();
        PickNext();
        SetPageThemeJS(0);
    }

    ~Game() {
        UnloadRenderTexture(target);
    }

    Color GetBgColor() const      { return isLightMode ? LIGHT_BG : DARK_BG; }
    Color GetPanelBg() const      { return isLightMode ? LIGHT_PANEL_BG : DARK_PANEL_BG; }
    Color GetBoxBorder() const    { return isLightMode ? LIGHT_BOX_COLOR : DARK_BOX_COLOR; }
    Color GetAccentColor() const  { return isLightMode ? LIGHT_ACCENT : DARK_ACCENT; }
    Color GetPrimaryText() const  { return isLightMode ? Color{ 20, 28, 48, 255 } : WHITE; }
    Color GetMutedText() const    { return isLightMode ? Color{ 95, 110, 138, 255 } : GRAY; }
    Color GetBallColor(int lvl) const { return isLightMode ? LVL_C_LIGHT[lvl] : LVL_C_DARK[lvl]; }

    Rectangle GetThemeButtonRect() const {
        return { 360.0f - 62.0f, topPad + 16.0f, 124.0f, 38.0f };
    }

    void UpdateViewportMetrics(bool forceInit = false) {
        int winW = GetScreenWidth();
        int winH = GetScreenHeight();
        if (winW <= 0 || winH <= 0) return;

        bool nowPortrait = winH > winW;
        int newW = nowPortrait ? 720 : 1020;
        int newH = 960;

        if (nowPortrait) {
            float aspect = (float)winH / (float)winW;
            newH = (int)Clamp(720.0f * aspect, 1140.0f, 1620.0f);
        }

        float scale = fmin((float)winW / newW, (float)winH / newH);
        topPad = Clamp(GetSafeInsetTopJS() / fmax(scale, 0.1f), 0.0f, 90.0f);
        bottomPad = Clamp(GetSafeInsetBottomJS() / fmax(scale, 0.1f), 0.0f, 70.0f);

        if (forceInit || nowPortrait != isPortrait || newW != scrW || abs(newH - scrH) > 8) {
            isPortrait = nowPortrait;
            scrW = newW;
            scrH = newH;
            if (!forceInit) UnloadRenderTexture(target);
            target = LoadRenderTexture(scrW, scrH);
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
        }
        lastWinW = winW;
        lastWinH = winH;
    }

    float GetBoxX() const { return (720.0f - BOX_W) * 0.5f; }
    float GetBoxY() const { return topPad + 105.0f; }
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
        dropTimer = 0.15f;
        screenShake = 0.0f;
        state = PLAYING;
        PickNext();
    }

    void ToggleTheme() {
        isLightMode = !isLightMode;
        SetPageThemeJS(isLightMode ? 1 : 0);
    }

    void Update() {
        UpdateViewportMetrics(false);

        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        float scale = fmin((float)GetScreenWidth() / scrW, (float)GetScreenHeight() / scrH);
        int touchCount = GetTouchPointCount();
        bool isTouching = (touchCount > 0);
        Vector2 rawInput = isTouching ? GetTouchPosition(0) : GetMousePosition();

        if (isTouching || GetMouseDelta().x != 0.0f || GetMouseDelta().y != 0.0f || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            vMouse.x = (rawInput.x - (GetScreenWidth() - (scrW * scale)) * 0.5f) / scale;
            vMouse.y = (rawInput.y - (GetScreenHeight() - (scrH * scale)) * 0.5f) / scale;
        }

        bool tapPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || (isTouching && !wasTouching);
        bool touchReleased = (!isTouching && wasTouching);
        wasTouching = isTouching;

        Rectangle btnRect = GetThemeButtonRect();
        Rectangle hitRect = { btnRect.x - 12.0f, btnRect.y - 10.0f, btnRect.width + 24.0f, btnRect.height + 20.0f };
        if (tapPressed && CheckCollisionPointRec(vMouse, hitRect)) {
            ToggleTheme();
            return;
        }

        if (state == TITLE) {
            if (tapPressed || IsKeyPressed(KEY_SPACE)) Reset();
            return;
        }

        if (state == GAMEOVER) {
            if (tapPressed || IsKeyPressed(KEY_R)) Reset();
            return;
        }

        if (IsKeyPressed(KEY_R)) Reset();
        if (IsKeyPressed(KEY_T)) ToggleTheme();

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

        float maxPlayY = boxY + BOX_H + 20.0f;
        bool inPlayZone = isPortrait ? (vMouse.y >= topPad + 60.0f && vMouse.y <= maxPlayY) : (vMouse.x < 720.0f && vMouse.y >= topPad + 60.0f);
        bool triggerDrop = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || touchReleased;
        if (triggerDrop && dropTimer <= 0.0f && inPlayZone) {
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
            
            Color mCol = GetBallColor(m.nextLvl - 1);
            SpawnParticles(m.p, mCol, 20);
            AddText(m.p, "+" + std::to_string(pts) + (combo > 1 ? " (x" + std::to_string(combo) + ")" : ""), mCol);

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
                AddText(m.p, "SUPERNOVA! +10000", GetPrimaryText());
                SpawnParticles(m.p, GetPrimaryText(), 50);
            }
        }
    }

    void DrawGlowCircle(Vector2 p, float r, Color c, float scale) {
        float sr = r * scale;
        if (!isLightMode) {
            DrawCircleV(p, sr * 1.2f, Fade(c, 0.2f));
            DrawCircleV(p, sr * 1.1f, Fade(c, 0.4f));
            DrawCircleV(p, sr, c);
            DrawCircleLines((int)p.x, (int)p.y, sr, WHITE);
        } else {
            DrawCircleV({ p.x, p.y + 3.0f }, sr * 1.03f, Fade(BLACK, 0.12f));
            DrawCircleV(p, sr, c);
            DrawCircleLines((int)p.x, (int)p.y, sr, Fade(WHITE, 0.85f));
        }
    }

    void DrawThemeToggle() {
        Rectangle btn = GetThemeButtonRect();
        Color pillBg = isLightMode ? Color{ 218, 226, 242, 255 } : Color{ 22, 32, 58, 255 };
        Color pillBorder = isLightMode ? Color{ 150, 168, 200, 255 } : Fade(DARK_ACCENT, 0.55f);

        DrawRectangleRounded(btn, 0.5f, 12, pillBg);
        DrawRectangleRoundedLines(btn, 0.5f, 12, pillBorder);

        Vector2 iconPos = { btn.x + (isLightMode ? btn.width - 20.0f : 20.0f), btn.y + btn.height * 0.5f };
        if (isLightMode) {
            DrawCircleV(iconPos, 8.0f, Color{ 245, 158, 11, 255 });
            font.Draw("LIGHT", btn.x + 44.0f, btn.y + 10.0f, 16, GetPrimaryText(), true, true);
        } else {
            DrawCircleV(iconPos, 8.0f, DARK_ACCENT);
            DrawCircleV({ iconPos.x + 3.0f, iconPos.y - 2.0f }, 6.5f, pillBg);
            font.Draw("DARK", btn.x + 76.0f, btn.y + 10.0f, 16, WHITE, true, true);
        }
    }

    void Draw() {
        BeginTextureMode(target);
        ClearBackground(GetBgColor());

        float boxX = GetBoxX();
        float boxY = GetBoxY();
        float dangerY = GetDangerY();

        Vector2 camOff = { 0, 0 };
        if (screenShake > 0.0f) {
            camOff.x = GetRandomValue(-100, 100) * 0.01f * screenShake;
            camOff.y = GetRandomValue(-100, 100) * 0.01f * screenShake;
        }

        DrawRectangleRounded({ boxX + camOff.x - 5, boxY + camOff.y - 5, BOX_W + 10, BOX_H + 10 }, 0.1f, 10, GetBoxBorder());
        DrawRectangleRounded({ boxX + camOff.x, boxY + camOff.y, BOX_W, BOX_H }, 0.1f, 10, GetBgColor());

        DrawLineEx({ boxX + camOff.x, dangerY + camOff.y }, { boxX + BOX_W + camOff.x, dangerY + camOff.y }, 2, Fade(RED, 0.5f + 0.5f * sinf(GetTime() * 5.0f)));

        for (const auto& c : circles) {
            DrawGlowCircle(Vector2Add(c.p, camOff), c.r, GetBallColor(c.lvl), c.scaleAnim);
            Color numCol = (isLightMode && c.lvl == 9) ? WHITE : (isLightMode ? Color{ 255, 255, 255, 255 } : WHITE);
            font.Draw(std::to_string(c.lvl + 1), c.p.x + camOff.x, c.p.y + camOff.y - 10, 22, numCol, true, true);
        }

        for (const auto& p : particles) {
            DrawCircleV(Vector2Add(p.p, camOff), p.r * (p.life / p.maxLife), p.c);
        }

        if (state == PLAYING && dropTimer <= 0.0f) {
            float hoverX = Clamp(vMouse.x, boxX + LVL_R[nextLvl] + 5, boxX + BOX_W - LVL_R[nextLvl] - 5);
            Color nextCol = GetBallColor(nextLvl);
            DrawGlowCircle({ hoverX + camOff.x, boxY - LVL_R[nextLvl] + camOff.y - 12 }, LVL_R[nextLvl], nextCol, 1.0f);
            DrawLineEx({ hoverX + camOff.x, boxY + camOff.y }, { hoverX + camOff.x, boxY + BOX_H + camOff.y }, 1, Fade(nextCol, isLightMode ? 0.45f : 0.3f));
        }

        for (const auto& t : texts) {
            font.Draw(t.text, t.p.x + camOff.x, t.p.y + camOff.y, 22, Fade(t.c, t.life / t.maxLife), true, true);
        }

        font.Draw(TextFormat("SCORE: %d", score), 24, topPad + 14.0f, 28, GetPrimaryText(), false, true);
        font.Draw(TextFormat("HIGH: %d", highScore), 24, topPad + 50.0f, 20, GetMutedText(), false, false);
        
        font.Draw("NEXT", 720 - 125, topPad + 18.0f, 20, GetPrimaryText(), false, true);
        DrawGlowCircle({ 720 - 65.0f, topPad + 62.0f }, LVL_R[nextLvl] * 0.45f, GetBallColor(nextLvl), 1.0f);

        if (!isPortrait) {
            float panelX = 720.0f;
            float panelW = 300.0f;
            DrawRectangle((int)panelX, 0, (int)panelW, scrH, GetPanelBg());
            DrawLine((int)panelX, 0, (int)panelX, scrH, Fade(GetAccentColor(), 0.5f));
            
            font.Draw("GLOBAL NETWORK", panelX + 25, topPad + 36.0f, 20, GetAccentColor(), false, true);
            DrawLine((int)panelX + 25, (int)(topPad + 65.0f), (int)(panelX + panelW - 25), (int)(topPad + 65.0f), Fade(GetAccentColor(), 0.3f));
            
            font.Draw("Total Players", panelX + 25, topPad + 88.0f, 18, GetMutedText(), false, false);
            font.Draw(TextFormat("%d", GetTotalPlayersJS()), panelX + 25, topPad + 115.0f, 42, GetPrimaryText(), false, true);

            font.Draw("Recent Players", panelX + 25, topPad + 195.0f, 18, GetMutedText(), false, false);
            for (int i = 0; i < 5; i++) {
                char nameBuf[32] = {0};
                GetRecentPlayerJS(i, nameBuf, 32);
                font.Draw(nameBuf, panelX + 25, topPad + 232.0f + (i * 36), 20, GetPrimaryText(), false, false);
            }

            font.Draw("A Sabtain Ali production", 360.0f, scrH - bottomPad - 36.0f, 18, Fade(GetPrimaryText(), 0.45f), true, false);
        } else {
            float panelY = boxY + BOX_H + 24.0f;
            float availH = scrH - panelY - bottomPad;
            DrawRectangle(0, (int)panelY, scrW, scrH - (int)panelY, GetPanelBg());
            DrawLine(0, (int)panelY, scrW, (int)panelY, Fade(GetAccentColor(), 0.5f));

            font.Draw("GLOBAL NETWORK", 40, panelY + 18.0f, 20, GetAccentColor(), false, true);
            font.Draw("Total Players", 40, panelY + 54.0f, 18, GetMutedText(), false, false);
            font.Draw(TextFormat("%d", GetTotalPlayersJS()), 40, panelY + 82.0f, 44, GetPrimaryText(), false, true);

            font.Draw("Recent Players", 340, panelY + 18.0f, 18, GetMutedText(), false, false);
            int maxRows = (availH > 260.0f) ? 5 : 4;
            for (int i = 0; i < maxRows; i++) {
                char nameBuf[32] = {0};
                GetRecentPlayerJS(i, nameBuf, 32);
                font.Draw(nameBuf, 340, panelY + 50.0f + (i * 32), 20, GetPrimaryText(), false, false);
            }

            font.Draw("A Sabtain Ali production", scrW * 0.5f, scrH - bottomPad - 34.0f, 18, Fade(GetPrimaryText(), 0.45f), true, false);
        }

        float overlayH = isPortrait ? (boxY + BOX_H + 24.0f) : (float)scrH;
        Color overlayBg = isLightMode ? Fade(Color{ 242, 245, 252, 255 }, 0.84f) : Fade(BLACK, 0.75f);

        if (state == TITLE) {
            DrawRectangle(0, 0, 720, (int)overlayH, overlayBg);
            font.Draw("NEON SHAPE-MERGE", 360.0f, boxY + 310.0f, 42, GetAccentColor(), true, true);
            font.Draw("TAP TO START", 360.0f, boxY + 390.0f, 22, GetPrimaryText(), true, true);
        } else if (state == GAMEOVER) {
            DrawRectangle(0, 0, 720, (int)overlayH, overlayBg);
            font.Draw("GAME OVER", 360.0f, boxY + 290.0f, 48, RED, true, true);
            font.Draw(TextFormat("FINAL SCORE: %d", score), 360.0f, boxY + 370.0f, 28, GetPrimaryText(), true, true);
            font.Draw("TAP TO RESTART", 360.0f, boxY + 430.0f, 20, GetMutedText(), true, false);
        }
        DrawThemeToggle();

        EndTextureMode();
    }
};

Game* game = nullptr;

void UpdateDrawFrame() {
    game->Update();
    game->Draw();

    BeginDrawing();
    ClearBackground(game->GetBgColor());
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