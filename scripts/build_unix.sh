#!/usr/bin/env bash
#
# Convenience build script for Linux / macOS.
# Usage:
#   ./scripts/build_unix.sh            # configure + build (Debug)
#   ./scripts/build_unix.sh --run      # ... and run afterwards
#   ./scripts/build_unix.sh --release  # Release build
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

RUN=0
CONFIG="Debug"
for arg in "$@"; do
  case "$arg" in
    --run)     RUN=1 ;;
    --release) CONFIG="Release" ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

# --- Locate vcpkg (manifest mode) -------------------------------------------
VCPKG_ROOT="${VCPKG_ROOT:-}"
if [ -z "$VCPKG_ROOT" ]; then
  for c in "$HOME/vcpkg" "/opt/vcpkg" "/usr/local/vcpkg"; do
    if [ -f "$c/scripts/buildsystems/vcpkg.cmake" ]; then
      VCPKG_ROOT="$c"
      break
    fi
  done
fi

CMAKE_ARGS=(-S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG")
if [ -n "$VCPKG_ROOT" ] && [ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
  CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")
else
  echo "warning: vcpkg not found (set VCPKG_ROOT). Relying on system packages." >&2
fi

echo "Configuring TankGame in $BUILD_DIR"
cmake "${CMAKE_ARGS[@]}"

echo "Building TankGame ($CONFIG)"
cmake --build "$BUILD_DIR" --config "$CONFIG" --target TankGame

if [ "$RUN" -eq 1 ]; then
  APP_DIR="$BUILD_DIR/TankGame"
  echo "Running TankGame"
  ( cd "$APP_DIR" && ./TankGame )
fi
