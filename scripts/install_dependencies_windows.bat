@echo off
REM ===========================================================================
REM Install script for Tank Game dependencies on Windows.
REM
REM This installs every library the project needs via vcpkg. It is optional:
REM because the project ships a vcpkg.json manifest, the dependencies are also
REM installed automatically the first time CMake configures with the vcpkg
REM toolchain file. Running this script up-front just makes that first configure
REM fast (and warms the binary cache).
REM
REM You still need the Vulkan SDK installed separately (see the note below).
REM ===========================================================================

echo Installing Tank Game dependencies for Windows...

:: Check if vcpkg is installed
where vcpkg >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo vcpkg not found. Please install vcpkg first.
    echo Visit https://github.com/microsoft/vcpkg for installation instructions.
    echo Typically, you would:
    echo 1. git clone https://github.com/Microsoft/vcpkg.git
    echo 2. cd vcpkg
    echo 3. .\bootstrap-vcpkg.bat
    echo 4. Add vcpkg to your PATH ^(and set VCPKG_ROOT^)
    exit /b 1
)

:: Enable binary caching for vcpkg
echo Enabling binary caching for vcpkg...
set VCPKG_BINARY_SOURCES=clear;files,%TEMP%\vcpkg-cache,readwrite

:: Create cache directory if it doesn't exist
if not exist %TEMP%\vcpkg-cache mkdir %TEMP%\vcpkg-cache

:: Install all dependencies at once using vcpkg in classic mode.
:: Keep this list in sync with vcpkg.json.
echo Installing all dependencies...
vcpkg install glfw3 glm stb tinyobjloader tinygltf nlohmann-json ktx[vulkan] openal-soft --triplet=x64-windows
if %ERRORLEVEL% neq 0 (
    echo Dependency installation failed.
    exit /b 1
)

:: Remind about Vulkan SDK
echo.
echo Don't forget to install the Vulkan SDK 1.4.335 or newer from https://vulkan.lunarg.com/
echo It provides the Vulkan loader, validation layers, and the slangc shader compiler.
echo.

echo All dependencies have been installed successfully!
echo You can now build the project, for example:
echo.
echo   scripts\build_windows.ps1 -Run
echo.
echo ...or with CMake directly:
echo   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%%VCPKG_ROOT%%\scripts\buildsystems\vcpkg.cmake
echo   cmake --build build --config Debug --target TankGame

exit /b 0
