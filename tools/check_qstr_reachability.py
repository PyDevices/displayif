#!/usr/bin/env python3
"""Refuse an MP_QSTR_ name that only exists when a usermod's INTERFACE define is set.

The rule this enforces, and why, is docs/qstrs-and-usermod-defines.md. In one
line: MicroPython's CMake QSTR pass builds its preprocessor flags from the
*port target's* own COMPILE_DEFINITIONS, so a define a usermod supplies through
``target_compile_definitions(... INTERFACE ...)`` reaches the compile and never
reaches ``makeqstrdefs.py``. A name referenced only behind such a define is
therefore never collected, and the port fails with::

    error: 'MP_QSTR_register_lvgl_decoder' undeclared

while the same source builds on unix, where the Makefile ports fold
``CFLAGS_USERMOD`` into ``CFLAGS`` and the QSTR pass reads ``CFLAGS``.

What this script does: reads every ``micropython.cmake`` here for the macros
this repo hands out as INTERFACE defines, then walks ``src/`` tracking the
preprocessor conditional stack, and reports an ``MP_QSTR_*`` that appears only
inside a branch taken when one of those macros is set.

A name is not reported when it *also* appears unguarded **in the same file** --
the QSTR pass collects it from there and the guarded reference rides along -- or
when it is in :data:`CORE_POOL` below.

Same file, not same repo, and the difference is the whole check. Every name in
``src/ports/samd/mod_i80bus.c`` is also referenced by the esp32 backend and by
``src/ports/common/notimpl/mod_i80bus.c``, and none of those is compiled into a
SAMD51 build -- so a repo-wide "is it referenced anywhere" test passes the bug
this script exists to catch. It was written that way first, and it did.

Limits worth knowing. It reads text, not semantics: it does not evaluate
``#if`` arithmetic, and it assumes an ``#else`` of a positive guard is safe
(the extraction pass takes that branch, so anything in it *is* collected).
A macro that the port itself also defines -- ESP-IDF defines ``ESP_PLATFORM``
for every component -- is over-reported rather than under-reported, which is
the direction to err in.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

#: Names MicroPython has in its static qstr pool (``py/qstrdefs.h``), so they
#: exist on every build whether anything here collects them or not. Keep this
#: list short and add to it only with the reason: a name is in the pool because
#: MicroPython's own core uses it, and that can change between versions.
#: ``__init__`` is here for ``src/jpegio/jpegio.c``'s LVGL decoder registration.
CORE_POOL = frozenset({"__init__"})

QSTR = re.compile(r"MP_QSTR_(\w+)")
IFACE_BLOCK = re.compile(
    r"target_compile_definitions\s*\([^)]*?\bINTERFACE\b(?P<body>[^)]*)\)",
    re.IGNORECASE | re.DOTALL,
)
MACRO = re.compile(r"(?<![\w$])([A-Za-z_]\w*)")


def interface_macros(root: Path) -> set[str]:
    """Every macro this repo hands a port through an INTERFACE compile define."""
    macros: set[str] = set()
    for cmake in sorted(root.rglob("micropython.cmake")):
        text = cmake.read_text(errors="replace")
        for block in IFACE_BLOCK.finditer(text):
            # Drop quoted values first: a stub message is a sentence, and every
            # word in it would otherwise read as a macro name.
            body = re.sub(r'"[^"]*"', "", block.group("body"))
            for line in body.splitlines():
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                for word in line.split():
                    name = word.split("=", 1)[0].strip()
                    if MACRO.fullmatch(name):
                        macros.add(name)
    return macros


def _positive_for(condition: str, macros: set[str]) -> set[str]:
    """Which of *macros* this condition requires to be SET to be taken.

    ``#if defined(X)``, ``#if X``, ``#ifdef X`` are positive; ``#ifndef X`` and
    ``!defined(X)`` are not, and neither is an ``#else`` of a positive guard --
    the QSTR pass takes those branches, so names inside them are collected.
    """
    hits = set()
    for macro in macros:
        for match in re.finditer(r"(?<![\w$])" + re.escape(macro) + r"(?![\w$])", condition):
            before = condition[: match.start()]
            # "!defined(X)" / "! X" -- the nearest thing to the left that is not
            # whitespace, an open paren or the word "defined".
            lead = re.sub(r"(defined|\s|\()+$", "", before)
            if lead.endswith("!"):
                continue
            hits.add(macro)
    return hits


def scan(root: Path, macros: set[str]) -> tuple[dict[str, list[str]], int]:
    """Return {qstr name: [places it is guarded and nothing else collects it]}.

    Judged per file, because a file is what the QSTR pass reads and a sibling
    backend is not in this build.
    """
    unreachable: dict[str, list[str]] = {}
    seen = 0
    for path in sorted(root.rglob("*.[ch]")):
        guarded: dict[str, list[str]] = {}
        unguarded: set[str] = set()
        stack: list[bool] = []  # is this frame taken only when a macro is set?
        for lineno, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
            stripped = line.strip()
            directive = re.match(r"#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", stripped)
            if directive:
                kind, rest = directive.group(1), directive.group(2)
                if kind in ("if", "ifdef"):
                    stack.append(bool(_positive_for(rest, macros)))
                    continue
                if kind == "ifndef":
                    stack.append(False)
                    continue
                if kind == "elif":
                    if stack:
                        stack[-1] = bool(_positive_for(rest, macros))
                    continue
                if kind == "else":
                    if stack:
                        stack[-1] = False
                    continue
                if kind == "endif":
                    if stack:
                        stack.pop()
                    continue
            at_risk = any(stack)
            for name in QSTR.findall(line):
                if at_risk:
                    guarded.setdefault(name, []).append(
                        f"{path.relative_to(REPO)}:{lineno}"
                    )
                else:
                    unguarded.add(name)
        seen += len(guarded)
        for name, places in guarded.items():
            if name not in unguarded and name not in CORE_POOL:
                unreachable.setdefault(name, []).extend(places)
    return unreachable, seen


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=str(REPO / "src"), help="tree to scan")
    parser.add_argument("--list-macros", action="store_true",
                        help="print the INTERFACE defines found and stop")
    args = parser.parse_args()

    macros = interface_macros(REPO)
    if args.list_macros:
        print("\n".join(sorted(macros)))
        return 0
    if not macros:
        print("no INTERFACE compile definitions found -- is this the right repo?",
              file=sys.stderr)
        return 2

    bad, seen = scan(Path(args.root), macros)
    if not bad:
        print(f"qstr reachability: OK ({len(macros)} INTERFACE defines, "
              f"{seen} guarded references, all reachable)")
        return 0

    print("A qstr is referenced only where an INTERFACE define is set, so the "
          "CMake QSTR pass will not collect it and the port build will fail "
          "with 'MP_QSTR_<name>' undeclared.")
    print("See docs/qstrs-and-usermod-defines.md.\n")
    for name, places in sorted(bad.items()):
        print(f"  MP_QSTR_{name}")
        for place in places:
            print(f"      {place}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
