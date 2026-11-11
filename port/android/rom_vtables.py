#!/usr/bin/env python3
"""Separate explicit DS address-point references from native C++ vtables.

The native compiler emits an Itanium offset/RTTI prefix. A recovered factory
that explicitly stores _ZTV<Class> expects the first DS method instead. Letting
a weak compiler vtable satisfy that raw reference would link but dispatch the
prefix as a function. Require the PC's manually seated/relocated table.
Only symbol names from config are used; no ROM bytes are read or emitted.
"""
import argparse
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPECIAL = {"_ZTV8dScene_c": "port_vt_scene_rom", "_ZTV5Stage": "port_vt_stage_rom",
           "_ZTV13ExpandingHeap": "port_vt_expanding_heap_rom"}


def table_names():
    names = set()
    for path in (ROOT / "config").rglob("symbols.txt"):
        for line in path.read_text().splitlines():
            match = re.match(r"(_ZTV\w+)\s+kind:", line)
            if match:
                names.add(match[1])
    return sorted(names)


def emit(path):
    lines = ["// Generated from config symbol names, with no Nintendo data.", "#pragma once"]
    for name in table_names():
        native = SPECIAL.get(name, "port_rom" + name)
        lines.extend((f"#ifndef {name}", f"#define {name} {native}", "#endif"))
    content = "\n".join(lines) + "\n"
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    emit(parser.parse_args().out)
