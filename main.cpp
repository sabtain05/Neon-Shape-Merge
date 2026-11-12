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