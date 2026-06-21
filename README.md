# Tank Game — Vulkan base project

A small, **self-contained Vulkan starter project**. Right now it just renders the
simple base scene (two spinning, textured quads with depth testing) so you have a
known-good window + render loop to build on. The plan is to grow this into a simple
driving **tank game** with shooting at randomly generated targets — but none of that
game logic exists yet.

This folder is intentionally independent of the surrounding tutorial repo: it has its
own `CMakeLists.txt`, its own CMake find-modules (`cmake/`), its own shader and asset,
and its own `vcpkg.json`. You can copy/move the whole `tank-game/` folder somewhere
else and it will still build.

## What it does today

- Creates a GLFW window and a Vulkan instance (validation layers on in Debug builds).
- Sets up a swap chain with depth buffering and **dynamic rendering** (no render passes).
- Draws geometry with a perspective camera, an MVP uniform buffer, and a sampled texture.
- Handles window resize / swap-chain recreation.

The renderer is based on the tutorial's depth-buffering chapter because it is the
simplest *complete 3D* setup (camera + depth + textures) — a good foundation for a game.

## Prerequisites

1. **Vulkan SDK 1.4.335 or newer** — https://vulkan.lunarg.com/
   Provides the Vulkan loader, validation layers, the `vulkan_raii.hpp` header, and the
   `slangc` shader compiler. Make sure the `VULKAN_SDK` environment variable is set
   (the SDK installer does this on Windows).
2. **vcpkg** — https://github.com/microsoft/vcpkg
   Used in *manifest mode*: the libraries in `vcpkg.json` are installed automatically the
   first time CMake configures the project. Set the `VCPKG_ROOT` environment variable to
   your vcpkg checkout.
3. **CMake 3.29+** and a **C++20** compiler (MSVC 2022, recent Clang, or GCC 13+).

## Build & run

### Windows (PowerShell)

```powershell
# from the tank-game folder
./scripts/build_windows.ps1            # configure + build (Debug)
./scripts/build_windows.ps1 -Run       # build, then run
./scripts/build_windows.ps1 -Config Release -Run
```

The script auto-detects vcpkg from `VCPKG_ROOT`, then `C:\vcpkg`. The first configure
will take a while because vcpkg builds the dependencies.

Optionally, pre-install the dependencies (and warm the vcpkg binary cache) up-front:

```bat
scripts\install_dependencies_windows.bat
```

This is not required — the `vcpkg.json` manifest makes CMake install everything on the
first configure — but it makes that first configure fast.

### Linux / macOS

```bash
chmod +x scripts/build_unix.sh   # first time only
./scripts/build_unix.sh --run
```

> macOS note: Vulkan runs on top of MoltenVK (included in the Vulkan SDK). It works, but
> the project is primarily developed and tested on Windows.

### Manual CMake (any platform)

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug --target TankGame
# run from the output dir so shaders/ and textures/ resolve:
cd build/TankGame && ./TankGame      # Windows: .\Debug\TankGame.exe
```

## Project layout

```
tank-game/
├─ CMakeLists.txt        # self-contained build (single TankGame executable)
├─ vcpkg.json            # dependency manifest (auto-installed by vcpkg)
├─ cmake/                # bundled Find*.cmake helpers (keeps the project portable)
├─ src/
│  └─ main.cpp           # the whole renderer (single file for now)
├─ shaders/
│  └─ shader.slang       # Slang vertex + fragment shaders -> compiled to slang.spv
├─ assets/
│  └─ textures/
│     └─ texture.jpg     # the sample texture
└─ scripts/
   ├─ build_windows.ps1
   ├─ build_unix.sh
   └─ install_dependencies_windows.bat
```

At runtime the executable expects `shaders/slang.spv` and `textures/texture.jpg`
**relative to its working directory**. CMake puts both in `build/TankGame/`, and the
build scripts run the game from there.

## Dependencies (via vcpkg)

| Package          | Used for                                   | Used yet? |
|------------------|--------------------------------------------|-----------|
| `glfw3`          | window + input                             | yes       |
| `glm`            | vectors / matrices                         | yes       |
| `stb`            | loading the `.jpg` texture                 | yes       |
| `tinyobjloader`  | `.obj` model loading                       | ready     |
| `tinygltf`       | `.gltf` / `.glb` model loading             | ready     |
| `ktx[vulkan]`    | `.ktx2` compressed textures                | ready     |
| `nlohmann-json`  | JSON (levels / config)                     | ready     |
| `openal-soft`    | audio (engine sounds, shots)               | ready     |

The "ready" libraries are installed and available so you don't have to touch the
dependency setup later; only `glfw3`, `glm`, and `stb` are linked into the simple base.
Wire the others into `target_link_libraries` in `CMakeLists.txt` as you need them.

## Where the game logic will go (later)

This is just the scaffold. When you're ready, likely next steps are:

1. Replace the hardcoded quads with a simple ground plane + a tank mesh.
2. Add a camera that follows the tank, plus WASD / arrow-key driving input.
3. Spawn random target objects and add projectile shooting + collision.
4. Split `main.cpp` into smaller units (renderer, input, game state) as it grows.
