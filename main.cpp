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