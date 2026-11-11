#!/usr/bin/env python3
"""Generate native ARM game sources using the existing PC transformations.

Some recovered mwcc TUs intentionally keep conflicting block-scope extern
views. Clang diagnoses them when combined. For ordinal-marked TUs, isolate
each original definition AFTER PC hostgen, retaining the common declarations
and every original body. Never edit src/ or invent a function implementation.
"""
import argparse
import contextlib
import io
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "port/tools"))
import hostgen  # noqa: E402
import native_imports  # noqa: E402

ORDINAL = re.compile(r"^/\* ROM ordinal (\d+) -- ([^,]+), (0x[0-9a-f]+), size (0x[0-9a-f]+) \*/$", re.M)
SPLIT_TUS = {"src/actors/Player.cpp"}
PARTICLE_NEW = "_ZN8Particle6System3NewEjj5Fix12IiES2_S2_PK11Vector3_16fPNS_8CallbackE"
IMPORT_TUS = {
    "src/actors/da1up_c.cpp", "src/actors/daBmb_c.cpp",
    "src/actors/daGmch_c.cpp", "src/actors/daMip_c.cpp",
    "src/game/actors/d_a_obj_wc_obj02.cpp", "src/game/actors/d_a_obj_wc_obj05.cpp",
    "src/game/actors/d_a_propeller_heyho.cpp", "src/game/actors/d_a_wanwan.cpp",
}


def ordinal_parts(text):
    marks = list(ORDINAL.finditer(text))
    if not marks:
        return None
    ordinals = [int(m[1]) for m in marks]
    if ordinals != list(range(len(marks))):
        raise ValueError("ordinal sequence is not complete and contiguous")
    names = [m[2] for m in marks]
    if len(set(names)) != len(names):
        raise ValueError("duplicate ordinal symbol")
    prelude = text[:marks[0].start()]
    parts = []
    for i, mark in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        body = text[mark.start():end]
        if mark[2].startswith("func_"):
            # A free function's definition already declares itself. Some
            # recovered bodies redundantly redeclare their own name with a
            # conflicting pointer/return type, even though they never call it.
            # Remove only that redundant declaration; Clang then checks any
            # recursive call against the real definition's signature.
            declaration = r"\bextern\s+[^;{}\n]+\b" + re.escape(mark[2]) + r"\s*\([^;{}\n]*\)\s*;"
            body = re.sub(declaration, "", body)
        common = prelude
        if re.search(r"\bextern[^;\n]+\b" + PARTICLE_NEW + r"\(", body):
            # The TU prelude hoists one namespace view for the one qualified
            # caller. Other isolated functions carry their own C declaration.
            # Do not combine those distinct recovered return-type views.
            common = re.sub(r"^namespace Particle \{ extern \"C\" void " + PARTICLE_NEW + r"[^\n]+\n", "", common, flags=re.M)
        parts.append((mark[2], common + body))
    return parts


def native_calls(text, stem):
    # These PC patches deliver EDX as a dummy; native ARM must pass each real
    # argument immediately after self. Exact counts fail closed on source drift.
    if stem in ("func_0204335c", "func_02043288"):
        for before, after, count in (
            ("int (__fastcall *)(void *, void *)", "int (*)(void *)", 2),
            ("void (__fastcall *)(void *, void *, unsigned)", "void (*)(void *, unsigned)", 1),
            ("(self, 0)", "(self)", 2),
            ("(self, 0, code)", "(self, code)", 1),
        ):
            if text.count(before) != count:
                raise ValueError(f"{stem}: native Process patch drift: {before}")
            text = text.replace(before, after)
    elif stem == "Player":
        for before, after in (
            ("void (__fastcall *)(void *, void *, char *)", "void (*)(void *, char *)"),
            ("])(a, 0, self)", "])(a, self)"),
        ):
            if text.count(before) != 1:
                raise ValueError(f"Player: native interaction patch drift: {before}")
            text = text.replace(before, after)
    elif stem == "func_02016ff4":
        for before, after in (
            ("int (__fastcall *)(void *, void *, BMD_File *, int, int)", "int (*)(void *, BMD_File *, int, int)"),
            ("])(self, 0, file, c, d)", "])(self, file, c, d)"),
        ):
            if text.count(before) != 1:
                raise ValueError(f"{stem}: model callback patch drift: {before}")
            text = text.replace(before, after)
    elif stem == "func_02019144":
        before = "typedef volatile unsigned int vu32;"
        if text.count(before) != 1:
            raise ValueError("func_02019144: MMIO scalar declaration changed")
        # PC hostgen removes volatile from its MMIO proxy argument. This C TU
        # declares only vu32, so supply its exact nonvolatile scalar spelling.
        text = text.replace(before, "typedef unsigned int u32;\n" + before)
    elif stem == "func_0200d8c8":
        # The source documents cstd::fdiv's two scalar arguments, but its shared
        # header omits the declaration. Resolve the actual cstd_div.c function.
        text = 'extern "C" int _ZN4cstd4fdivEii(int, int);\n' + text
    elif stem == "func_ov007_020c7d60":
        # PC's pointer-MMIO rewrite also mistakes the last *declarator in this
        # comma-separated local list for a dereference. Restore that declaration;
        # the actual sqrt register read remains NTR_MMIO below.
        before = "int sqrt_raw, len, dot, s, rest, NTR_MMIO(int, 0x40002b4);"
        if text.count(before) != 1:
            raise ValueError("ov007 spring sqrt local declaration changed")
        text = text.replace(before, "int sqrt_raw, len, dot, s, rest, *p_sqrt;")
    elif stem == "daBombking_c":
        # Keep both initializations at their original execution points, but
        # declare the scalar locals before the C-style early-exit jumps.
        before = "int func_ov078_02124cf4(unsigned char* thiz)\n{"
        if text.count(before) != 1:
            raise ValueError("King Bob-omb early-exit function changed")
        text = text.replace(before, before + "\n    unsigned char* other;\n    short msg;")
        for before, after in (("unsigned char* other = *(unsigned char**)(thiz + 0x430);", "other = *(unsigned char**)(thiz + 0x430);"),
                              ("short msg = 0;", "msg = 0;")):
            if text.count(before) != 1:
                raise ValueError("King Bob-omb local lifetime patch drift")
            text = text.replace(before, after)
    elif stem == "d_a_obj_fl_koma_u":
        before = "void* ctor, void* dtor"
        if text.count(before) != 1:
            raise ValueError("firebar array constructor declaration changed")
        # The existing native __cxa_vec_ctor bridge accepts function pointers.
        text = text.replace(before, "void (*ctor)(void*), void (*dtor)(void*)")
    elif stem == "daYurei_Mucho_c":
        # Establish the shared header's C linkage before this class's redundant
        # vtable declaration. No declarations or bodies are removed.
        before = '#include "decl_common.h"'
        if text.count(before) != 1:
            raise ValueError("Snufit shared declarations changed")
        text = before + "\n" + text.replace(before, "")
    elif stem in ("d_a_obj_wc_obj02", "d_a_obj_wc_obj05"):
        raw = "func_ov029_021116e4" if stem.endswith("02") else "func_ov029_02111e60"
        before = f'extern "C" int {raw}(void *a, void *b, void *c)'
        if text.count(before) != 1:
            raise ValueError("WDW collision callback declaration changed")
        # Clang rejects this recovered function view alongside decl_common's
        # data/different-signature view, even in separate namespaces. Give its
        # implementation a distinct C++ identifier bound to the same ELF name.
        native = "port_native_" + raw
        signature = '(void *a, void *b, void *c)'
        after = f'extern "C" int {native}{signature} __asm__("{raw}");\nextern "C" int {native}{signature}'
        text = text.replace(before, after)
    elif stem == "d_a_wanwan":
        first = "    void _ZN5Sound15PlaySecretSoundEP8dActor_cPt(void *actor, u16 *snd);"
        last = "    extern s16 data_02082214[];"
        if text.count(first) != 1:
            raise ValueError("Chain Chomp local import block changed")
        start = text.index(first)
        end = text.index(last, start)
        declarations = text[start:end]
        lines = declarations.splitlines(keepends=True)
        if len(lines) != 17 or any(not line.rstrip().endswith(");") for line in lines):
            raise ValueError("Chain Chomp local prototype inventory changed")
        # Block-scope function prototypes are imports even without extern.
        # Explicit storage spelling lets the native view isolation see them.
        text = text[:start] + "".join("    extern " + line.lstrip() for line in lines) + text[end:]
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    if re.search(r"\b(__fastcall|__thiscall)\b", code):
        raise ValueError(f"{stem}: an unaudited x86 callback remains")
    return text


def outputs(source, out):
    rel = source.relative_to(ROOT)
    parts = ordinal_parts(source.read_text()) if rel.as_posix() in SPLIT_TUS else None
    if parts:
        return [out / rel.with_suffix("") / (name + ".cpp") for name, _ in parts]
    return [out / rel.with_suffix(".cpp")]


def generate(source, out):
    raw = source.read_text()
    if hostgen.is_asm(raw) and source.stem not in hostgen.ASM_EXCISION:
        raise ValueError(f"{source}: ARM assembly needs an explicit native implementation")
    hostgen.QUIET_VPTR = True
    with contextlib.redirect_stdout(io.StringIO()):
        staged, _ = hostgen.emit(source, out / ".pc-stage", ROOT, True)
    text = native_calls(staged.read_text(), source.stem)
    if source.relative_to(ROOT).as_posix() in IMPORT_TUS:
        text = native_imports.isolate(text)
    parts = ordinal_parts(text) if source.relative_to(ROOT).as_posix() in SPLIT_TUS else None
    expected = outputs(source, out)
    if parts:
        if len(parts) != len(expected):
            raise ValueError("hostgen changed the ordinal inventory")
        bodies = [body for _, body in parts]
    else:
        bodies = [text]
    for path, body in zip(expected, bodies):
        path.parent.mkdir(parents=True, exist_ok=True)
        origin = source.relative_to(ROOT).as_posix()
        result = f"// Android native source from {origin}; PC hostgen, declarations isolated when necessary.\n" + body
        if not path.exists() or path.read_text() != result:
            path.write_text(result)
    return expected


def main():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument("--source")
    ap.add_argument("--out", type=pathlib.Path)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--list-splits", action="store_true")
    args = ap.parse_args()
    if args.list_splits:
        print("\n".join(sorted(SPLIT_TUS)))
        return
    if not args.source or not args.out:
        ap.error("--source and --out are required")
    source = (ROOT / args.source).resolve()
    paths = outputs(source, args.out) if args.list else generate(source, args.out)
    if args.list:
        print("\n".join(str(p) for p in paths))
    else:
        print(f"Android original source: {args.source} -> {len(paths)} translation unit(s)")


if __name__ == "__main__":
    main()
