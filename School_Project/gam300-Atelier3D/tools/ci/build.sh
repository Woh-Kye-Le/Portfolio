#!/usr/bin/env bash
# ===============================================
# CI Build Script for Linux (used by Jenkins, runnable locally)
# Usage: bash tools/ci/build.sh [Debug|Release|Package] [linux|windows]
#   linux   - native Linux build (default)
#   windows - Windows .exe cross-compiled with MinGW-w64 (needs: apt install mingw-w64)
# ===============================================
# Stop on the first failing command, on unset variables, and on failures inside pipes
set -euo pipefail

CONFIG="${1:-Release}"
TARGET="${2:-linux}"
# Repo root, worked out from where this script is, so it works from any current directory
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# The desktop app is the CMake project
SOURCE_DIR="$ROOT/apps/desktop"

EXTRA_ARGS=()
case "$TARGET" in
    linux)
        BUILD_DIR="$ROOT/apps/desktop/build/ci-$CONFIG"
        ;;
    windows)
        BUILD_DIR="$ROOT/apps/desktop/build/ci-windows-$CONFIG"
        # The toolchain file points CMake at the MinGW compilers instead of the Linux gcc
        EXTRA_ARGS=(-DCMAKE_TOOLCHAIN_FILE="$SOURCE_DIR/cmake/mingw-w64-x86_64.cmake")
        ;;
    *)
        echo "ERROR: unknown target '$TARGET' (expected linux or windows)" >&2
        exit 1
        ;;
esac

# Kept between builds and outside the workspace, so wiping the workspace does not lose them
CACHE_ROOT="${ATELIER_CI_CACHE:-$HOME/.cache/atelier3d-ci}"

echo "==============================================="
echo "  Atelier3D CI build - $CONFIG ($TARGET)"
echo "==============================================="
cmake --version

# Always start from a clean build directory so every run is repeatable
rm -rf "$BUILD_DIR"

# Ninja builds a lot faster than make; use it when the node has it
if command -v ninja >/dev/null 2>&1; then
    EXTRA_ARGS+=(-G Ninja)
fi

# ccache gives back the result of any compile it has seen before, so a clean build only really compiles what changed
if command -v ccache >/dev/null 2>&1; then
    export CCACHE_DIR="$CACHE_ROOT/ccache"
    export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-20G}"
    mkdir -p "$CCACHE_DIR"
    # So the statistics printed at the end are for this build alone
    ccache --zero-stats >/dev/null
    # ASSIMP_BUILD_USE_CCACHE off: it would wrap every target in the project in ccache a second time
    # Precompile headers off: Jolt uses them, and ccache cannot reliably cache a compile that reads one
    EXTRA_ARGS+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
                 -DASSIMP_BUILD_USE_CCACHE=OFF -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON)
fi

# FetchContent's downloads, kept so they happen once and not on every build. One folder per build type, because
# FetchContent also builds in this folder. Its name must stay _deps: the Jenkins warning filters match on it.
DEPS_DIR="$CACHE_ROOT/deps/$TARGET-$CONFIG/_deps"
mkdir -p "$DEPS_DIR"
# Only the downloads are kept; the libraries' build output goes, like the rest of the build directory
rm -rf "$DEPS_DIR"/*-build
EXTRA_ARGS+=(-DFETCHCONTENT_BASE_DIR="$DEPS_DIR")

echo "[1/2] Configuring..."
# Single-config generators (Ninja/make) take the config here, at configure time
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" -DATELIER_BUILD_TESTS=ON "${EXTRA_ARGS[@]}"

echo "[2/2] Building..."
cmake --build "$BUILD_DIR" --parallel

# The hit rate shows whether the cache is working: low on the first build, high after that
if command -v ccache >/dev/null 2>&1; then
    ccache --show-stats
fi

echo "Build succeeded: $BUILD_DIR"
