#!/usr/bin/env python3
"""Inspect the compiled ARM objects, not a separate replica of their storage."""
import argparse
import pathlib
import struct
import subprocess


def main():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument("--nm", required=True)
    ap.add_argument("--build", type=pathlib.Path, required=True)
    args = ap.parse_args()
    elf = (args.build / "libsm64ds_android.so").read_bytes()
    assert elf[:6] == b"\x7fELF\x01\x01", "JNI library is not little-endian ELF32"
    assert struct.unpack_from("<H", elf, 18)[0] == 40, "JNI library is not ARM"
    listing = subprocess.check_output([
        args.nm, "--defined-only", "--print-size", "--format=posix",
        str(args.build / "libsm64ds_android_game_flow.a")], text=True)
    symbols = {}
    owner = ""
    for line in listing.splitlines():
        if line.endswith(":"):
            owner = line
        else:
            row = line.split()
            if len(row) == 4:
                symbols.setdefault(row[0], []).append((owner, int(row[2], 16), int(row[3], 16)))
    groups = {
        "port_scene_title_state": ("data_0209b33c", "data_0209b340", "data_0209b34b", "data_0209b34e", "data_0209b3d8"),
        "port_scene_touch_state": ("data_020a80cc", "data_020a80e4"),
        "port_level_save_state": ("data_0209caa0", "data_0209cab4", "data_0209cad2", "data_0209cae4", "data_0209caf4"),
    }
    for backing, aliases in groups.items():
        assert len(symbols[backing]) == 1
        owner, base, length = symbols[backing][0]
        ds_base = int(aliases[0].split("_")[1], 16)
        for name in aliases:
            assert len(symbols[name]) == 1, f"duplicate definition: {name}"
            member_owner, offset, size = symbols[name][0]
            assert member_owner == owner, f"{name} is outside its backing object"
            assert offset - base == int(name.split("_")[1], 16) - ds_base, name
            assert 0 <= offset - base < length and offset - base + size <= length, name
        print(f"{backing}: {len(aliases)} ELF aliases at exact DS offsets, {length} bytes")
    # These must resolve to actual native source definitions, never the PC
    # symbol-name forwarders that become recursive under native Itanium ABI.
    for name in ("_ZN2GX13SetBankForTexEt", "_ZN2GX13SetBankForOBJEt",
                 "_ZN3G3X6SetFogEbiii", "_ZN15ModelComponents6RenderEP9Matrix4x3P7Vector3",
                 "_ZN8SaveData16SetDefaultValuesEP12FileSaveData",
                 "_ZN8SaveData18SetDefaultValuesMgEP16MinigameSaveData"):
        assert not any("scene_boot.cpp.o" in row[0] for row in symbols.get(name, [])), name
    for name in ("port_scene_begin", "port_scene_tick", "port_stage_a_boot", "port_stage_create",
                 "StartFile", "LoadLevel", "_ZN9dScDSMT_c13InitResourcesEv",
                 "_ZN9dScDSMT_c8BehaviorEv", "_ZN8dScene_c16SpawnIfNecessaryEv",
                 "_ZN5Stage13InitResourcesEv", "_ZN5Stage8BehaviorEv", "_ZN5Stage6RenderEv"):
        assert len(symbols.get(name, [])) == 1, f"missing or duplicate real game definition: {name}"
    assert len(symbols.get("port_vt_scene_rom", [])) == 1
    assert "stage_bridges.cpp.o" in symbols["port_vt_scene_rom"][0][0]
    assert len(symbols.get("_ZTV8dScene_c", [])) == 1
    assert "BeforeInitResourcesEv.cpp.o" in symbols["_ZTV8dScene_c"][0][0]
    assert not any("level_boot.cpp.o" in row[0] for row in
                   symbols.get("_ZN7fBase_c19BeforeInitResourcesEv", []))
    print("ARM32 objects and real scene/Stage/StartFile definitions verified; game link/runtime still required")


if __name__ == "__main__":
    main()
