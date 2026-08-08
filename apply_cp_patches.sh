#!/usr/bin/env bash
# Apply (or preview) CircuitPython usdl2 integration patches from displayif (unix only).
#
# Out-of-tree substitute for Adafruit Extending CircuitPython (no upstream PR):
#   Learn / design-guide step              This script
#   shared-bindings/<mod>/                 copy spike → CP shared-bindings/usdl2/
#   shared-module/<mod>/                   copy spike → CP shared-module/usdl2/
#   enable CIRCUITPY_* / mpconfig          CIRCUITPY_USDL2 in variant mk + mpconfig
#   list sources in port Makefile          variant .mk SRC lists + SRC_PATTERNS
#   build CircuitPython                    caller runs make (or workspace build_cp.sh)
# Conceptual refs:
#   https://learn.adafruit.com/extending-circuitpython
#   https://docs.circuitpython.org/en/latest/docs/design_guide.html
#
# Usage:
#   ./apply_cp_patches.sh --dry-run [--port PORT] [--variant VARIANT]
#   ./apply_cp_patches.sh --apply [--port PORT] [--variant VARIANT]
#   ./apply_cp_patches.sh --status [--port PORT] [--variant VARIANT]
#
# Environment:
#   CP_DIR          CircuitPython tree (default: sibling circuitpython/ next to this repo)
#   WORKSPACE_DIR   Parent of displayif (default: parent of this repo)
#   PORT            Must be unix (default: unix); other ports skip with exit 0
#   VARIANT         Unix variant (default: coverage)
#
# Standalone: clone circuitpython + displayif as siblings, then:
#   ./apply_cp_patches.sh --apply --port unix --variant coverage
#   cd ../circuitpython/ports/unix && make -j VARIANT=coverage

set -euo pipefail

DISPLAYIF_MOD_DIR=$(cd "$(dirname "$0")" && pwd)
USDL2_MOD_DIR="$DISPLAYIF_MOD_DIR"
WORKSPACE_DIR="${WORKSPACE_DIR:-$(cd "$DISPLAYIF_MOD_DIR/.." && pwd)}"

PORT="${PORT:-unix}"
BOARD="${BOARD:-}"
VARIANT="${VARIANT:-coverage}"
MODE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run|--apply|--status|--force-apply) MODE="$1"; shift ;;
        --port)    PORT="$2"; shift 2 ;;
        --board)   BOARD="$2"; shift 2 ;;
        --variant) VARIANT="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,16p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1 (try --help)" >&2
            exit 1
            ;;
    esac
done

MODE="${MODE:---dry-run}"
case "$MODE" in
    --force-apply) MODE=--apply ;;
    --dry-run|--apply|--status) ;;
    *)
        echo "Unknown mode: $MODE (try --help)" >&2
        exit 1
        ;;
esac

# displayif usdl2 CircuitPython integration is unix-only.
if [[ "$PORT" != unix ]]; then
    echo "displayif usdl2 apply_cp_patches: port=$PORT is not unix; skipping"
    exit 0
fi

# Resolve CircuitPython: CP_DIR, else sibling circuitpython/ under WORKSPACE_DIR.
if [[ -n "${CP_DIR:-}" && -d "${CP_DIR}/ports" ]]; then
    CP_DIR=$(cd "$CP_DIR" && pwd)
elif [[ -d "$WORKSPACE_DIR/circuitpython/ports" ]]; then
    CP_DIR=$(cd "$WORKSPACE_DIR/circuitpython" && pwd)
else
    echo "CircuitPython tree not found (set CP_DIR, or place circuitpython/ next to this repo under $WORKSPACE_DIR)." >&2
    exit 1
fi
SPIKE_DIR="$DISPLAYIF_MOD_DIR/src/circuitpython_spike"
SPIKE_MANIFEST="$SPIKE_DIR/copy_manifest.txt"

MARKER_TAG="displayif-usdl2 begin (apply_cp_patches.sh)"
MARKER_BEGIN="# >>> $MARKER_TAG"
MARKER_END="# >>> displayif-usdl2 end"

markers_for_file() {
    local file="$1"
    case "$file" in
        *.h)
            echo "/* >>> $MARKER_TAG */"
            echo "/* >>> displayif-usdl2 end */"
            ;;
        *)
            echo "$MARKER_BEGIN"
            echo "$MARKER_END"
            ;;
    esac
}

repair_invalid_header_markers() {
    local file="$1"
    [ -f "$file" ] || return 0
    case "$file" in
        *.h) ;;
        *) return 0 ;;
    esac
    if ! grep -qF "# >>> displayif-usdl2" "$file"; then
        return 0
    fi
    if [ "$DRY_RUN" = 1 ]; then
        echo "  [dry-run] repair invalid # markers in $file"
        return 0
    fi
    python3 - "$file" "$MARKER_TAG" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
tag = sys.argv[2]
text = path.read_text()
text = text.replace(f"# >>> {tag}", f"/* >>> {tag} */")
text = text.replace("# >>> displayif-usdl2 end", "/* >>> displayif-usdl2 end */")
path.write_text(text)
PY
    log "  repaired header markers: $file"
}

DRY_RUN=0
APPLY=0
if [ "$MODE" = "--dry-run" ]; then DRY_RUN=1; fi
if [ "$MODE" = "--apply" ]; then APPLY=1; fi

log() { echo "$*"; }

patch_block_present() {
    local file="$1"
    local needle="${2:-displayif-usdl2 begin}"
    [ -f "$file" ] && grep -qF "$needle" "$file"
}

insert_block_before_line() {
    local file="$1"
    local anchor="$2"
    local block="$3"
    local needle="${4:-displayif-usdl2 begin}"
    repair_invalid_header_markers "$file"
    if patch_block_present "$file" "$needle"; then
        log "  skip (already patched): $file"
        return 0
    fi
    if [ "$DRY_RUN" = 1 ]; then
        echo "  [dry-run] insert block into $file before: $anchor"
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
path.write_text(text.replace(anchor, insert + anchor, 1))
PY
}

insert_block_after_line() {
    local file="$1"
    local anchor="$2"
    local block="$3"
    local needle="${4:-displayif-usdl2 begin}"
    repair_invalid_header_markers "$file"
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
}

insert_raw_after_line() {
    local file="$1"
    local anchor="$2"
    local line="$3"
    if grep -qF "$line" "$file" 2>/dev/null; then
        log "  skip (already present): $file"
        return 0
    fi
    if [ "$DRY_RUN" = 1 ]; then
        echo "  [dry-run] insert into $file after: $anchor"
        return 0
    fi
    python3 - "$file" "$anchor" "$line" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
anchor = sys.argv[2]
line = sys.argv[3]

text = path.read_text()
if line in text:
    sys.exit(0)
if anchor not in text:
    raise SystemExit(f"anchor not found in {path}: {anchor!r}")
path.write_text(text.replace(anchor, anchor + "\n" + line, 1))
PY
}

copy_spike_files() {
    python3 - "$SPIKE_DIR" "$CP_DIR" "$SPIKE_MANIFEST" "$DRY_RUN" <<'PY'
import filecmp
import shutil
import sys
from pathlib import Path

spike_dir, cp_dir, manifest, dry = sys.argv[1:5]
dry_run = dry == "1"

def copy_one(rel_dir: str, filename: str) -> None:
    rel = f"{rel_dir}/{filename}"
    src = Path(spike_dir)
    dst = Path(cp_dir)
    for part in rel_dir.split("/"):
        src /= part
        dst /= part
    src /= filename
    dst /= filename
    if not src.is_file():
        raise SystemExit(f"missing spike file: {src}")
    if dst.is_file() and filecmp.cmp(src, dst, shallow=False):
        print(f"  unchanged: {rel}")
        return
    if dry_run:
        verb = "update" if dst.is_file() else "create"
        print(f"  [dry-run] {verb} {rel}")
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    print(f"  copied: {rel}")

for raw in Path(manifest).read_text().splitlines():
    line = raw.split("#", 1)[0].strip()
    if not line:
        continue
    rel_dir, filename = line.split("\t", 1)
    copy_one(rel_dir.strip(), filename.strip())
PY
}

if [ ! -f "$SPIKE_MANIFEST" ]; then
    echo "Missing spike manifest: $SPIKE_MANIFEST" >&2
    exit 1
fi

PORT_DIR="$CP_DIR/ports/$PORT"
VARIANT_MK="$PORT_DIR/variants/$VARIANT/mpconfigvariant.mk"
VARIANT_H="$PORT_DIR/variants/$VARIANT/mpconfigvariant.h"
DEFNS_MK="$CP_DIR/py/circuitpy_defns.mk"
MPCONFIG_MK="$CP_DIR/py/circuitpy_mpconfig.mk"
PORT_MK="$PORT_DIR/Makefile"

USDL2_MOD_REL=$(python3 -c "import os; print(os.path.relpath('$USDL2_MOD_DIR', '$PORT_DIR'))")

log "CircuitPython: $CP_DIR"
log "workspace:     $WORKSPACE_DIR"
log "displayif/usdl2: $USDL2_MOD_DIR (as $USDL2_MOD_REL from port)"
log "variant:         $VARIANT"
log "mode:            $MODE"
log

if [ "$MODE" = "--status" ]; then
    SPIKE_INIT_C=$(python3 - "$SPIKE_MANIFEST" "$CP_DIR" <<'PY'
import sys
from pathlib import Path

manifest, cp_dir = sys.argv[1:3]
rel_dir, filename = Path(manifest).read_text().splitlines()[0].split("\t", 1)
p = Path(cp_dir)
for part in rel_dir.split("/"):
    p /= part
p /= filename.strip()
print(p)
PY
)
    report() {
        local label="$1"
        local file="$2"
        if [ ! -e "$file" ]; then
            echo "missing  $file"
        elif [ "$label" = "spike" ]; then
            echo "ok       $file"
        elif patch_block_present "$file"; then
            echo "patched  $file"
        else
            echo "pending  $file"
        fi
    }
    report spike "$SPIKE_INIT_C"
    report patch "$VARIANT_MK"
    if [ -f "$VARIANT_H" ]; then
        report patch "$VARIANT_H"
    fi
    report patch "$DEFNS_MK"
    report patch "$MPCONFIG_MK"
    report patch "$PORT_MK"
    exit 0
fi

log "==> Copy spike templates"
copy_spike_files
log

USDL2_ENABLE_BLOCK="CIRCUITPY_USDL2 = 1
CFLAGS += -DCIRCUITPY_USDL2=1"

variant_mk_anchor() {
    if [ "$VARIANT" = "coverage" ]; then
        if [ -f "$VARIANT_MK" ] && grep -qF '# >>> lv-circuitpython-mod end' "$VARIANT_MK"; then
            echo "# >>> lv-circuitpython-mod end"
        else
            echo "CIRCUITPY_MESSAGE_COMPRESSION_LEVEL = 1"
        fi
    elif [ "$VARIANT" = "standard" ]; then
        echo 'FROZEN_MANIFEST ?= $(VARIANT_DIR)/manifest.py'
    else
        echo $'-DCIRCUITPY_LOCALE=1 \\'
    fi
}

variant_mk_binding_anchor() {
    if grep -qF $'\tshared-bindings/lvgl/__init__.c \\' "$VARIANT_MK"; then
        echo $'\tshared-bindings/lvgl/__init__.c \\'
    elif grep -qF $'\tshared-bindings/pygraphics/__init__.c \\' "$VARIANT_MK"; then
        echo $'\tshared-bindings/pygraphics/__init__.c \\'
    else
        echo $'\tshared-bindings/jpegio/JpegDecoder.c \\'
    fi
}

variant_mk_module_anchor() {
    if grep -qF $'\tshared-module/lvgl/__init__.c \\' "$VARIANT_MK"; then
        echo $'\tshared-module/lvgl/__init__.c \\'
    elif grep -qF $'\tshared-module/pygraphics/__init__.c \\' "$VARIANT_MK"; then
        echo $'\tshared-module/pygraphics/__init__.c \\'
    else
        echo $'\tshared-module/jpegio/JpegDecoder.c \\'
    fi
}

variant_h_anchor() {
    if grep -qF '/* >>> lv-circuitpython-mod end */' "$VARIANT_H"; then
        echo '/* >>> lv-circuitpython-mod end */'
    else
        echo '#include "../mpconfigvariant_common.h"'
    fi
}

mpconfig_anchor() {
    if grep -qF '# >>> lv-circuitpython-mod end' "$MPCONFIG_MK" \
        && grep -qF 'CIRCUITPY_LVGL' "$MPCONFIG_MK"; then
        echo "# >>> lv-circuitpython-mod end"
    elif grep -qF 'CFLAGS += -DCIRCUITPY_LVGL=$(CIRCUITPY_LVGL)' "$MPCONFIG_MK"; then
        echo 'CFLAGS += -DCIRCUITPY_LVGL=$(CIRCUITPY_LVGL)'
    else
        echo 'CFLAGS += -DCIRCUITPY_LOCALE=$(CIRCUITPY_LOCALE)'
    fi
}

port_mk_anchor() {
    if grep -qF '# >>> lv-circuitpython-mod end' "$PORT_MK"; then
        echo '# >>> lv-circuitpython-mod end'
    elif grep -qF 'include ../../py/mkenv.mk' "$PORT_MK"; then
        echo 'include ../../py/mkenv.mk'
    else
        echo 'include ../../py/circuitpy_mkenv.mk'
    fi
}

log "==> Patch unix variant mpconfigvariant.mk (enable flag)"
if [ ! -f "$VARIANT_MK" ]; then
    echo "Variant makefile not found: $VARIANT_MK" >&2
    exit 1
fi
insert_block_after_line "$VARIANT_MK" "$(variant_mk_anchor)" "$USDL2_ENABLE_BLOCK"
log

log "==> Patch unix variant mpconfigvariant.mk (module sources)"
insert_raw_after_line "$VARIANT_MK" "$(variant_mk_binding_anchor)" $'\tshared-bindings/usdl2/__init__.c \\'
insert_raw_after_line "$VARIANT_MK" "$(variant_mk_module_anchor)" $'\tshared-module/usdl2/__init__.c \\'
log "==> Patch unix variant mpconfigvariant.h (ifndef guard)"
if [ -f "$VARIANT_H" ]; then
    VARIANT_H_BLOCK="#ifndef CIRCUITPY_USDL2
#define CIRCUITPY_USDL2 (0)
#endif"
    insert_block_after_line "$VARIANT_H" "$(variant_h_anchor)" "$VARIANT_H_BLOCK"
fi
log

log "==> Patch py/circuitpy_mpconfig.mk (default off)"
MPCONFIG_BLOCK="CIRCUITPY_USDL2 ?= 0
CFLAGS += -DCIRCUITPY_USDL2=\$(CIRCUITPY_USDL2)"
insert_block_after_line "$MPCONFIG_MK" "$(mpconfig_anchor)" "$MPCONFIG_BLOCK"
log

log "==> Patch py/circuitpy_defns.mk"
DEFNS_PATTERNS_BLOCK="ifeq (\$(CIRCUITPY_USDL2),1)
SRC_PATTERNS += usdl2/%
endif"
if grep -qF '# >>> lv-circuitpython-mod end' "$DEFNS_MK"; then
    insert_block_after_line "$DEFNS_MK" "# >>> lv-circuitpython-mod end" "$DEFNS_PATTERNS_BLOCK" "SRC_PATTERNS += usdl2/%"
else
    insert_block_before_line "$DEFNS_MK" "ifeq (\$(CIRCUITPY_MATH),1)" "$DEFNS_PATTERNS_BLOCK" "SRC_PATTERNS += usdl2/%"
fi

if grep -qF $'\tlvgl/__init__.c \\' "$DEFNS_MK"; then
    insert_raw_after_line "$DEFNS_MK" $'\tlvgl/__init__.c \\' $'\tusdl2/__init__.c \\'
elif grep -qF $'\tpygraphics/__init__.c \\' "$DEFNS_MK"; then
    insert_raw_after_line "$DEFNS_MK" $'\tpygraphics/__init__.c \\' $'\tusdl2/__init__.c \\'
else
    insert_raw_after_line "$DEFNS_MK" $'\tjpegio/JpegDecoder.c \\' $'\tusdl2/__init__.c \\'
fi
log

log "==> Patch port Makefile (circuitpython.mk)"
PORT_BLOCK="USDL2_MOD_DIR := \$(abspath $USDL2_MOD_REL)
include \$(USDL2_MOD_DIR)/circuitpython.mk"
insert_block_after_line "$PORT_MK" "$(port_mk_anchor)" "$PORT_BLOCK"
log

if [ "$DRY_RUN" = 1 ]; then
    log "Dry run complete. Re-run with --apply to write changes."
elif [ "$APPLY" = 1 ]; then
    log "Patches applied."
    log
    log "Next:"
    log "  cd $PORT_DIR && make -j VARIANT=$VARIANT"
    log "  $PORT_DIR/build-$VARIANT/micropython $USDL2_MOD_DIR/tools/test_usdl2.py"
    log "See https://github.com/PyDevices/cmods for an easier way to build with other extensions."
fi
