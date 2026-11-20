#!/usr/bin/env bash
# ===============================================
# Packages a build into a downloadable archive with the editor, the player and the asset tool
# Usage: bash tools/ci/package.sh [Debug|Release] [linux|windows]   (run build.sh with the same arguments first)
# Output: apps/desktop/build/dist/Atelier3D-linux-<config>.tar.gz or apps/desktop/build/dist/Atelier3D-windows-<config>.zip
# ===============================================
set -euo pipefail

CONFIG="${1:-Release}"
TARGET="${2:-linux}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIST_DIR="$ROOT/apps/desktop/build/dist"
PKG_NAME="Atelier3D-$TARGET-$CONFIG"
PKG_DIR="$DIST_DIR/$PKG_NAME"

# Same build folders build.sh uses; Windows gets a .zip because that's what people can open there
case "$TARGET" in
    linux)
        BUILD_DIR="$ROOT/apps/desktop/build/ci-$CONFIG"
        ARCHIVE="$PKG_NAME.tar.gz"
        ;;
    windows)
        BUILD_DIR="$ROOT/apps/desktop/build/ci-windows-$CONFIG"
        ARCHIVE="$PKG_NAME.zip"
        ;;
    *)
        echo "ERROR: unknown target '$TARGET' (expected linux or windows)" >&2
        exit 1
        ;;
esac

# Only replace this target's package so linux and windows packages can sit side by side
rm -rf "$PKG_DIR" "$DIST_DIR/$ARCHIVE"
mkdir -p "$PKG_DIR"

# Executable: the editor (the unit test binary is not shipped); it creates games in projects/ next to itself
EXT=""
[ "$TARGET" = "windows" ] && EXT=".exe"
cp "$BUILD_DIR/bin/AtelierEditor$EXT" "$PKG_DIR/"

# What the editor runs from next to itself. Without the player a game cannot be exported (FindPlayerExecutable); without
# the asset tool models are not converted, and glTF, zips and most other formats cannot be imported at all.
TOOLS=(AtelierPlayer)
if ! grep -q '^ATELIER_BUILD_ASSET_TOOL:BOOL=OFF' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    TOOLS+=(AtelierAssetTool)
fi
for tool in "${TOOLS[@]}"; do
    if [ ! -f "$BUILD_DIR/bin/$tool$EXT" ]; then
        echo "ERROR: $BUILD_DIR/bin/$tool$EXT not found (the build should have made it next to the editor)" >&2
        exit 1
    fi
    cp "$BUILD_DIR/bin/$tool$EXT" "$PKG_DIR/"
done

# Everything the editor reads from next to itself (the build's POST_BUILD steps put these in bin/):
#   assets/     editor assets and the builtin shaders (assets/shaders/Builtin/) - without them nothing is drawn
#   templates/  the demo templates new games can start from (Project > New Game, or --new-game --template)
for folder in assets templates; do
    if [ ! -d "$BUILD_DIR/bin/$folder" ]; then
        echo "ERROR: $BUILD_DIR/bin/$folder not found (the build should have copied it next to the editor)" >&2
        exit 1
    fi
    cp -r "$BUILD_DIR/bin/$folder" "$PKG_DIR/"
done

# Shared libraries the build copied next to the editor (Windows: GLEW, FMOD, ... DLLs; Linux: FMOD's libfmod.so.14),
# so the editor starts on a PC without them. -L copies the file itself even where the build made a link.
shopt -s nullglob
if [ "$TARGET" = "windows" ]; then
    LIBS=("$BUILD_DIR"/bin/*.dll)
else
    LIBS=("$BUILD_DIR"/bin/*.so "$BUILD_DIR"/bin/*.so.*)
fi
shopt -u nullglob
for lib in "${LIBS[@]}"; do
    cp -L "$lib" "$PKG_DIR/"
done

# Fail here rather than ship a package that runs but cannot draw (as happened when assets/ was left out)
for required in assets/shaders/Builtin/Standard3D.vert assets/shaders/Builtin/Sky.frag \
                templates/demos/data/scenes/rendering.json; do
    if [ ! -f "$PKG_DIR/$required" ]; then
        echo "ERROR: package is missing $required" >&2
        exit 1
    fi
done
# Builds with FMOD audio need its library beside the editor (Windows: fmod.dll; Linux: libfmod.so.14)
if grep -q '^ATELIER_ENABLE_FMOD:BOOL=ON' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    FMOD_LIB="libfmod.so.14"
    [ "$TARGET" = "windows" ] && FMOD_LIB="fmod.dll"
    if [ ! -f "$PKG_DIR/$FMOD_LIB" ]; then
        echo "ERROR: package is missing $FMOD_LIB (the build has FMOD audio, which needs it)" >&2
        exit 1
    fi
fi

if [ "$TARGET" = "windows" ]; then
    # cmake -E tar can write zips, so the Linux node doesn't need the zip tool installed
    (cd "$DIST_DIR" && cmake -E tar cf "$ARCHIVE" --format=zip "$PKG_NAME")
else
    # tar.gz keeps the executable permission on the Linux binaries
    (cd "$DIST_DIR" && tar czf "$ARCHIVE" "$PKG_NAME")
fi

echo "Package created: $DIST_DIR/$ARCHIVE"
ls -la "$PKG_DIR"
