#!/usr/bin/env bash
# Apply (or preview) CircuitPython displayif integration patches.
#
# Usage:
#   ./apply_cp_displayif_patches.sh --dry-run --port PORT [--board BOARD]
#   ./apply_cp_displayif_patches.sh --apply --port PORT [--board BOARD]
#   ./apply_cp_displayif_patches.sh --status --port PORT [--board BOARD]
#
# Environment: WORKSPACE_DIR, CP_DIR, PORT, BOARD
#
# Supported CP ports: espressif (displayif ports/esp32), mimxrt, rp2 (when
# ports/rp2/circuitpython.mk exists).

set -euo pipefail

DISPLAYIF_MOD_DIR=$(cd "$(dirname "$0")" && pwd)
WORKSPACE_DIR="${WORKSPACE_DIR:-$(cd "$DISPLAYIF_MOD_DIR/.." && pwd)}"
CP_DIR="${CP_DIR:-$WORKSPACE_DIR/circuitpython}"
if [ ! -d "$CP_DIR/.git" ] && [ -d "$HOME/github/circuitpython/.git" ]; then
    CP_DIR="$HOME/github/circuitpython"
fi

PORT="${PORT:-}"
BOARD="${BOARD:-}"
MODE=""

MARKER_TAG="displayif-cmod begin (apply_cp_displayif_patches.sh)"
MARKER_BEGIN="# >>> $MARKER_TAG"
MARKER_END="# >>> displayif-cmod end"

DRY_RUN=0
APPLY=0

die() { echo "error: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run|--apply|--status) MODE="$1"; shift ;;
        --port)  PORT="$2"; shift 2 ;;
        --board) BOARD="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,14p' "$0"
            exit 0
            ;;
        *) die "Unknown argument: $1 (try --help)" ;;
    esac
done

MODE="${MODE:---dry-run}"
case "$MODE" in
    --dry-run) DRY_RUN=1 ;;
    --apply) APPLY=1 ;;
    --status) ;;
    *) die "Unknown mode: $MODE" ;;
esac

log() { echo "$*"; }

markers_for_file() {
    local file="$1"
    case "$file" in
        *.h)
            echo "/* >>> $MARKER_TAG */"
            echo "/* >>> displayif-cmod end */"
            ;;
        *)
            echo "$MARKER_BEGIN"
            echo "$MARKER_END"
            ;;
    esac
}

patch_block_present() {
    local file="$1"
    local needle="${2:-displayif-cmod begin}"
    [ -f "$file" ] && grep -qF "$needle" "$file"
}

insert_block_after_line() {
    local file="$1"
    local anchor="$2"
    local block="$3"
    local needle="${4:-displayif-cmod begin}"
    if patch_block_present "$file" "$needle"; then
        log "  skip (already patched): $file"
        return 0
    fi
    if [ "$DRY_RUN" = 1 ]; then
        echo "  [dry-run] insert block into $file after: $anchor"
        return 0
    fi
    local begin end
    begin=$(markers_for_file "$file" | sed -n '1p')
    end=$(markers_for_file "$file" | sed -n '2p')
    python3 - "$file" "$anchor" "$begin" "$end" "$block" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
anchor = sys.argv[2]
begin = sys.argv[3]
end = sys.argv[4]
block = sys.argv[5]

text = path.read_text()
if begin in text:
    sys.exit(0)
if anchor not in text:
    raise SystemExit(f"anchor not found in {path}: {anchor!r}")
insert = f"\n{begin}\n{block}\n{end}\n"
path.write_text(text.replace(anchor, anchor + insert, 1))
PY
    log "  patched: $file"
}

remove_marked_blocks() {
    local file="$1"
    local tag="$2"
    [ -f "$file" ] || return 0
    if ! grep -qF "$tag" "$file" 2>/dev/null; then
        return 0
    fi
    if [ "$DRY_RUN" = 1 ]; then
        echo "  [dry-run] remove marked blocks from $file"
        return 0
    fi
    python3 - "$file" "$tag" <<'PY'
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
tag = re.escape(sys.argv[2])
text = path.read_text()
patterns = [
    rf"\n?# >>> {tag}\n.*?\n# >>> displayif-cmod end\n?",
    rf"\n?/\* >>> {tag} \*/\n.*?\n/\* >>> displayif-cmod end \*/\n?",
]
for pat in patterns:
    text = re.sub(pat, "\n", text, count=0, flags=re.DOTALL)
path.write_text(text)
PY
}

remove_legacy_patches() {
    remove_marked_blocks "$1" "cmods-displayif begin (apply_cp_displayif_patches.sh)"
}

resolve_port() {
    [[ -n "$PORT" ]] || die "PORT is required (--port or env)"
    case "$PORT" in
        espressif|mimxrt) ;;
        rp2)
            if [ ! -f "$DISPLAYIF_MOD_DIR/ports/rp2/circuitpython.mk" ]; then
                die "rp2 not supported (missing $DISPLAYIF_MOD_DIR/ports/rp2/circuitpython.mk)"
            fi
            ;;
        *)
            die "Unsupported port: $PORT (supported: espressif, mimxrt, rp2)"
            ;;
    esac

    PORT_DIR="$CP_DIR/ports/$PORT"
    [[ -f "$PORT_DIR/Makefile" ]] || die "Invalid port: $PORT (no $PORT_DIR/Makefile)"
    PORT_MK="$PORT_DIR/Makefile"
}

port_makefile_anchor() {
    if grep -qF '# >>> displayif-cmod end' "$PORT_MK"; then
        echo '# >>> displayif-cmod end'
    elif grep -qF '# >>> lv-circuitpython-mod end' "$PORT_MK"; then
        echo '# >>> lv-circuitpython-mod end'
    elif grep -qF '# >>> usdl2-cmod end' "$PORT_MK"; then
        echo '# >>> usdl2-cmod end'
    elif grep -qF 'include ../../py/mkenv.mk' "$PORT_MK"; then
        echo 'include ../../py/mkenv.mk'
    elif grep -qF 'include ../../py/circuitpy_mkenv.mk' "$PORT_MK"; then
        echo 'include ../../py/circuitpy_mkenv.mk'
    else
        die "No known mkenv include anchor in $PORT_MK"
    fi
}

displayif_port_label() {
    case "$PORT" in
        espressif) echo "esp32 (CP espressif)" ;;
        mimxrt) echo "mimxrt" ;;
        rp2) echo "rp2" ;;
        *) echo "$PORT" ;;
    esac
}

# --- main ---

[ -d "$CP_DIR/.git" ] || die "CircuitPython tree not found at $CP_DIR"

resolve_port

DISPLAYIF_MOD_REL=$(python3 -c "import os; print(os.path.relpath('$DISPLAYIF_MOD_DIR', '$PORT_DIR'))")

log "CircuitPython: $CP_DIR"
log "workspace:     $WORKSPACE_DIR"
log "displayif:     $DISPLAYIF_MOD_DIR (as $DISPLAYIF_MOD_REL from port)"
log "port:            $PORT ($(displayif_port_label))"
[[ -n "$BOARD" ]] && log "board:           $BOARD"
log "mode:            $MODE"
log

if [ "$MODE" = "--status" ]; then
    if [ ! -e "$PORT_MK" ]; then
        echo "missing  $PORT_MK"
    elif patch_block_present "$PORT_MK"; then
        echo "patched  $PORT_MK"
    else
        echo "pending  $PORT_MK"
    fi
    if [ -f "$DISPLAYIF_MOD_DIR/ports/rp2/circuitpython.mk" ]; then
        echo "ok       $DISPLAYIF_MOD_DIR/ports/rp2/circuitpython.mk"
    else
        echo "missing  $DISPLAYIF_MOD_DIR/ports/rp2/circuitpython.mk"
    fi
    exit 0
fi

if [ "$APPLY" = 1 ] || [ "$DRY_RUN" = 1 ]; then
    log "==> Remove legacy cmods-displayif patches (if present)"
    remove_legacy_patches "$PORT_MK"
    log
fi

log "==> Patch port Makefile (circuitpython.mk)"
PORT_BLOCK="DISPLAYIF_MOD_DIR := \$(abspath $DISPLAYIF_MOD_REL)
include \$(DISPLAYIF_MOD_DIR)/circuitpython.mk"
insert_block_after_line "$PORT_MK" "$(port_makefile_anchor)" "$PORT_BLOCK"
log

if [ "$DRY_RUN" = 1 ]; then
    log "Dry run complete. Re-run with --apply to write changes."
elif [ "$APPLY" = 1 ]; then
    log "Patches applied."
    log
    log "Next:"
    log "  cd $CP_DIR/ports/$PORT"
    if [[ -n "$BOARD" ]]; then
        log "  make BOARD=$BOARD"
    else
        log "  make BOARD=<your_board>"
    fi
fi
