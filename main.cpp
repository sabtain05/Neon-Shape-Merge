#include "raylib.h"
#include"raymath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

constexpr int SCR_W = 720;
constexpr int SCR_H = 920;
constexpr float BOX_W = 600.0f;
constexpr float BOX_H = 800.0f;
constexpr float BOX_X = (SCR_W - BOX_W) / 2.0f;
constexpr float BOX_Y = (SCR_H - BOX_H) / 2.0f + 20.0f;
constexpr float DANGER_Y = BOX_Y + 80.0f;
constexpr float GRAVITY = 2000.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;
constexpr int SUBSTEPS = 8;
constexpr float RESTITUTION = 0.2f;
constexpr float FRICTION = 0.99f;
constexpr float DROP_CD = 0.5f;

const Color BG_COLOR = { 10,15,30,255 };
const Color BOX_COLOR = {40,60,100,255};

const Color LVL_C[10] = {
    {0,255,255,255},
    {255,0,255,255},
    {0,255,0,255},
    {255,255,0,255},
    {255,165,0,255},
    {255,105,180,255},
    {138,43,226,255},
    {0,250,154,255},
    {255,215,0,255},
    {255,255,255,255}
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