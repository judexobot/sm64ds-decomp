#!/usr/bin/env python3
"""Inspect the compiled ARM objects, not a separate replica of their storage."""
import argparse
import pathlib
import struct
import subprocess
import source_gen


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
        "port_touch_records": ("data_020a0de8", "data_020a0de9", "data_020a0dea", "data_020a0deb"),
        "port_card_state": ("data_020a8180", "data_020a81bc", "data_020a8760"),
        "port_oam_main": ("data_0209e674", "data_0209e67c", "data_0209e694"),
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
    for alias, backing, length in (("data_020a4d38", "_ZN6Memory16rootHeapIteratorE", 32),
                                    ("_ZN6Memory14defaultHeapPtrE", "data_020a0ea0", 4),
                                    ("data_0209f2d8", "data_0209f2d8_c", 1)):
        assert len(symbols[alias]) == len(symbols[backing]) == 1
        assert symbols[alias][0][:2] == symbols[backing][0][:2], alias
        assert symbols[alias][0][2] == length, alias
    for name in ("data_0209f49c", "data_0209f49e", "data_0209f4a0", "data_0209f4ac", "data_0209f4ae"):
        assert len(symbols.get(name, [])) == 1 and symbols[name][0][2] == 16 * 0x18, name
    for name in ("__aeabi_idiv", "_ZN4Heap7DestroyEv"):
        assert not any("actor_vtables.cpp.o" in row[0] for row in symbols.get(name, [])), name
    deallocate = symbols.get("_ZN6Memory10DeallocateEPv", [])
    assert len(deallocate) == 1 and "fs.cpp.o" not in deallocate[0][0], "recursive native FS deallocate bridge"
    assert len(symbols.get("port_slot0_rom_init", [])) == 1
    assert len(symbols.get("_ZN3OAM5ResetEv", [])) == 1
    assert not any("model_host.cpp.o" in row[0] for row in symbols.get("_ZN2GX7LoadTexEPKvjj", []))
    assert not any("stage_globals.cpp.o" in row[0] for row in symbols.get("__aeabi_uidiv", []))
    for name in ("UnloadArchive", "_ZN5Stage14LoadGraphics2DEbi", "_ZN5Stage7LoadFogEv",
                 "_ZN5Stage23LoadTextureTransformersEv", "_ZN11ShadowModel8CleanAllEv"):
        assert not any("stage_slot0.cpp.o" in row[0] for row in symbols.get(name, [])), name
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
    for raw in ("_ZN5Stage9PS_RenderEv", "_ZN5Stage9LC_RenderEv", "_ZN5Stage19RenderVsModeNewStarEv",
                "_ZN3OAM6RenderEbP7OamAttriiiiP9Matrix2x2", "_ZN7Message7DisplayEj"):
        assert len(symbols.get(raw, [])) == 1, raw
        assert "stage_frame.cpp.o" not in symbols[raw][0][0], f"native forwarder or PC seam: {raw}"
    player = source_gen.ROOT / "src/actors/Player.cpp"
    parts = source_gen.ordinal_parts(player.read_text())
    assert len(parts) == 301, "Player original function inventory changed; review native isolation"
    for name, _ in parts:
        assert len(symbols.get(name, [])) == 1, f"missing or duplicate Player definition: {name}"
    print("All 301 original Player functions and original pause/clear/VS draw bodies are defined")
    print("ARM32 objects and real scene/Stage/StartFile definitions verified; game link/runtime still required")


if __name__ == "__main__":
    main()
