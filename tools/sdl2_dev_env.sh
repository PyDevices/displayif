# Source before Windows usdl2 builds (do not vendor SDL2 in-repo).
#
# Look for SDL2 next to the workspace parent (sibling of displayif/), e.g. cmods/.
_DISPLAYIF_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
_PARENT="$(cd "$_DISPLAYIF_ROOT/.." && pwd)"
_SDL2_ROOT="${_PARENT}/SDL2-2.30.10"
#
# MinGW (micropython.exe) — unpack SDL2-devel-*-mingw.zip:
export SDL2_DEV_MINGW="${SDL2_DEV_MINGW:-$_SDL2_ROOT}"
#
# Examples:
#   export SDL2_DEV="$SDL2_DEV_MINGW"
#   cd ../micropython/ports/windows && make USER_C_MODULES=../../..
#
# Or use cmods/build_mp.sh --port windows (auto-detects $WORKSPACE_DIR/SDL2-*).
