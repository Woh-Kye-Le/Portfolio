#!/usr/bin/env bash
# ===============================================
# Installs everything the CI scripts need on an Ubuntu/Debian build node (used by Jenkins, runnable locally)
# Usage: bash tools/ci/install-deps.sh
#   Checks every package first and only installs the missing ones, so a ready node needs no root and
#   nothing is changed. Installing needs root or passwordless sudo for apt-get; without it the script
#   stops and prints the command to run once by hand.
# Libraries come with the repo (apps/desktop/libraries/) or are downloaded by CMake during the build
# (GLFW, GoogleTest), so only system tools and headers are installed here.
# ===============================================
set -euo pipefail

# Package - what needs it
PACKAGES=(
    build-essential  # gcc/g++/make: Linux build
    cmake            # every build
    ninja-build      # faster builds (build.sh falls back to make without it)
    ccache           # reuses compile results between builds (build.sh works without it, just slower)
    git              # checkout, lint.sh file list
    python3          # run-clang-tidy, lint-targets.py
    libgl1-mesa-dev  # OpenGL headers and library
    liblua5.4-dev    # Lua scripting (the repo only carries the Windows library)
    libx11-dev       # imgui_impl_glfw calls Xlib
    libxrandr-dev    # GLFW (X11)
    libxinerama-dev  # GLFW (X11)
    libxcursor-dev   # GLFW (X11)
    libxi-dev        # GLFW (X11)
    mingw-w64        # Windows cross-build
    clang-format     # lint.sh
    clang-tidy       # lint.sh
    cppcheck         # static-analysis.sh
)

echo "==============================================="
echo "  Atelier3D CI dependencies"
echo "==============================================="

if ! command -v dpkg-query >/dev/null 2>&1; then
    echo "ERROR: this script supports Ubuntu/Debian nodes (dpkg/apt-get not found)." >&2
    echo "Install the equivalents of: ${PACKAGES[*]}" >&2
    exit 1
fi

# Check first, so a node that already has everything never needs root
MISSING=()
for pkg in "${PACKAGES[@]}"; do
    if dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
        echo "  ok       $pkg"
    else
        echo "  missing  $pkg"
        MISSING+=("$pkg")
    fi
done

if [ "${#MISSING[@]}" -gt 0 ]; then
    echo "Installing ${#MISSING[@]} missing package(s): ${MISSING[*]}"
    # Already root: run apt-get directly. Otherwise sudo, but only if it won't ask for a password
    # (-n), because Jenkins can't type one and the build would just hang
    if [ "$(id -u)" -eq 0 ]; then
        SUDO=()
    elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
        SUDO=(sudo -n)
    else
        echo "ERROR: installing needs root, and $(whoami) cannot run sudo without a password." >&2
        echo "Run this once on the build node, then start the build again:" >&2
        echo "  sudo apt-get update && sudo apt-get install -y ${MISSING[*]}" >&2
        exit 1
    fi
    # stops apt from opening config prompts in the middle of a CI run
    export DEBIAN_FRONTEND=noninteractive
    "${SUDO[@]}" apt-get update
    "${SUDO[@]}" apt-get install -y --no-install-recommends "${MISSING[@]}"
fi

# Versions in the log, so a failure caused by a tool upgrade is easy to spot
echo "Tool versions:"
cmake --version | head -n 1
g++ --version | head -n 1
x86_64-w64-mingw32-g++ --version | head -n 1
clang-format --version
clang-tidy --version | grep -i version | head -n 1
cppcheck --version
if command -v ninja >/dev/null 2>&1; then echo "ninja $(ninja --version)"; fi

echo "All CI dependencies are installed"
