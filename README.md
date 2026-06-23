# Tank Game

A small 3D tank game I made for the Real-time Computer Graphics course. You drive a
tank around a grassy map and shoot floating crates to score points before the timer
runs out.

The rendering code started from the [Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/)
(the depth-buffering chapter, which is the first point where you have a real 3D scene
with a camera, depth testing and textures). I took that as a base and built the game
on top of it: models, a follow camera, input, bullets, crates and a little HUD.

## How to play

- **Arrow keys** — drive (up/down) and steer (left/right)
- **Space** — shoot
- **Enter** — start / restart a round
- **Controller** — R2 forward, L2 reverse, left stick steers, A shoots

Destroy as many crates as you can in 45 seconds. The best score is saved to
`highscore.txt`.

## Download & play (no build needed)

A prebuilt Windows version is committed to the repo under
[`dist/TankGame/`](dist/TankGame). Download that folder, then just run
`TankGame.exe` inside it — the needed DLLs, shaders, models and textures are all
in the folder. You only need a GPU/driver that supports Vulkan (no SDK install
required to play).

## Build & run

You need the **Vulkan SDK** (1.4+), **vcpkg** (with `VCPKG_ROOT` set), **CMake 3.29+**
and a **C++20** compiler.

### Windows (PowerShell)

```powershell
./scripts/build_windows.ps1 -Run
```

### Linux / macOS

```bash
chmod +x scripts/build_unix.sh   # first time only
./scripts/build_unix.sh --run
```

The first build is slow because vcpkg compiles the dependencies. The game must be run
from its output folder so it can find the `shaders/`, `models/` and `textures/` folders
next to the executable (the build scripts already do this).

## How the code is organized

`src/main.cpp` is the big file: it does all the Vulkan setup and the main loop. The
rest of the game is split into small classes so `main.cpp` doesn't have to know the
details:

| Class | File | What it does |
|-------|------|--------------|
| `Camera` | `camera.*` | Builds the view and projection matrices and smoothly follows the tank. |
| `GroundTileMap` | `ground_tile_map.*` | Builds the flat ground as a grid of tiles with a tiled grass texture. |
| `Tank` | `scene/tank.*` | Loads the tank model (`.gltf`) and gives back its mesh. |
| `TankController` | `scene/tank_controller.*` | Reads keyboard/controller input and moves the tank (position + rotation). |
| `Bullet` / `BulletSystem` | `scene/bullet.*` | A bullet flying forward; the system spawns them, moves them and deletes ones that fly too far. |
| `Crate` / `CrateSystem` | `scene/crate.*` | A floating, spinning crate; the system spawns them over time and removes the ones that get shot. |
| `Game` | `game.*` | The round logic: countdown, score, best score and the ImGui menus/HUD. |

The general idea: every frame `main.cpp` reads input, updates the tank, bullets and
crates, checks if any bullet hit a crate (a simple distance check), moves the camera,
and then records the Vulkan commands to draw everything.

## Some Vulkan ideas used here

Vulkan is very explicit — you have to set up almost everything yourself. The main
pieces from the tutorial that this game uses:

- **Instance & device** — the connection to the Vulkan library and the GPU we picked.
- **Swap chain** — the set of images we draw into and then show on screen. It has to be
  recreated when the window is resized.
- **Depth buffer** — an extra image storing how far each pixel is, so closer objects
  correctly cover the ones behind them (needed once you draw real 3D).
- **Graphics pipeline** — a fixed configuration of the shaders and render settings. Here
  the vertex and fragment shaders are written in Slang and compiled to SPIR-V.
- **Uniform buffer (MVP matrix)** — each object sends its `model`, `view` and
  `projection` matrices to the shader so the GPU knows where to put it on screen. There
  is one of these per object (tank, ground, each bullet, each crate).
- **Textures & samplers** — images loaded with `stb_image` and sampled in the fragment
  shader to color the surfaces (grass, tank, crate, bullet).
- **Descriptor sets** — the "binding" that tells a shader which uniform buffer and which
  texture to use for the object being drawn.
- **Command buffers & frames in flight** — drawing commands are recorded into a command
  buffer and submitted to the GPU. Two frames are processed at once so the CPU and GPU
  don't wait on each other.

## Libraries (via vcpkg)

| Library | Used for |
|---------|----------|
| `glfw3` | window + keyboard / controller input |
| `glm` | vectors and matrices |
| `stb` | loading texture images |
| `tinygltf` | loading the `.gltf` models (tank, crate, bullet) |
| `Dear ImGui` | the menus and on-screen score / timer |

## Assets

The 3D models in `assets/models/` (tank, crate, bullet) come with their own
`license.txt` files. The grass texture lives in `assets/textures/`.

## Credits

- Base renderer: [Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/)
- Built as a student project for the Real-time Computer Graphics course.
