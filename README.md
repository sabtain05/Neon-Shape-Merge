# Neon Shape-Merge

A physics-based drop-and-merge puzzle game inspired by Suika Game and 2048. Built entirely in C++ with [Raylib](https://www.raylib.com/), featuring a custom 2D physics engine, zero external graphical assets (100% code-generated visuals), and a seamless WebAssembly port via Emscripten.

## Play Live
**https://neonshapemerge.vercel.app**

## Features
* **Custom Physics Engine:** Fully bespoke fixed-timestep physics with rigid body circle collisions, restitution, and positional correction.
* **Zero External Assets:** Every visual element, glow effect, and particle is drawn mathematically using Raylib primitives.
* **Combo System:** Chain merges together quickly to multiply your score.
* **Responsive Web Layout:** Built-in virtual resolution scaling ensures the game perfectly fits desktop windows and mobile browser screens without stretching or collapsing.
* **Cross-Platform:** Compiles natively for Windows (.exe) and exports to HTML5/WebAssembly for frictionless web hosting.

## Controls
* **Mouse / Touch:** Move the hovering circle.
* **Left Click / Tap:** Drop the circle.
* **R:** Restart the game.
* **P / ESC:** Pause (If implemented).

## Local Development Setup

### Prerequisites (Windows)
1. Install [MSYS2](https://www.msys2.org/).
2. Open the MSYS2 UCRT64 terminal and install the GCC toolchain and Raylib:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-raylib make

```

3. Add `C:\msys64\ucrt64\bin` to your Windows Environment Variables.

### Building for Desktop

Open the project in VS Code (ensure the Microsoft C/C++ extension is installed) and press **F5** or **Ctrl+Shift+B** to build and run the game natively using the provided `.vscode/` tasks.

### Building for Web (Emscripten)

1. Install and activate [Emscripten (emsdk)](https://emscripten.org/docs/getting_started/downloads.html).
2. Clone Raylib and build the web library:
```bash
git clone --depth 1 [https://github.com/raysan5/raylib.git](https://github.com/raysan5/raylib.git)
cd raylib/src
emmake make PLATFORM=PLATFORM_WEB -B
cd ../..

```


3. Compile the game to the `public/` directory:
```bash
em++ -o public/index.html main.cpp -Os -Wall raylib/src/libraylib.web.a -Iraylib/src -Lraylib/src -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 --shell-file shell.html -DPLATFORM_WEB

```


4. Serve locally to test:
```bash
cd public
python -m http.server 8000

```



## Modding & Tuning

At the top of `main.cpp`, you will find a list of `constexpr` values. You can easily tweak the game's difficulty and feel by modifying these:

* `GRAVITY`: Adjust how fast the shapes fall.
* `RESTITUTION` & `FRICTION`: Change how bouncy and slippery the circles are.
* `DROP_CD`: Alter the cooldown time between drops.
* `LVL_R` & `LVL_C`: Customize the radius scaling and neon colors of all 10 merge levels.

---

<div align= "center">
<strong>A Sabtain Ali production</strong>
</div>