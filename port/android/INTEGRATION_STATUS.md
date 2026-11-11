# Android integration evidence — 2026-11-11

Branch: `android/game-port`, continuing `26ef77e563a19f678a5bf9ffa9af3fe5b48aa285`.
No playable APK exists. The running JNI `.so` still contains renderer/input/ROM
services; the complete game is not linked into it. No ROM or Nintendo media
was available, fetched, committed or included in test artifacts.

## Implemented in this iteration

* `hal/scene_boot.cpp`, `hal/level_boot.cpp`, `hal/stage_bridges.cpp` and
  `hal/title_vtable_seat14d.cpp` now compile under NDK r27c, ARMv7/API 28.
  AAPCS removes the x86 EDX dummy argument, including callbacks with real
  parameters; D1 dispatch removes the MSVC scalar-delete flag. ARM fader
  setters use native destructor-pair ordering and the real setter bodies.
* x86 fader trampolines retain their PC implementation and use normal native
  argument passing on ARM. Windows debug-register watchpoints are diagnostic
  only; requesting one on Android prints its unsupported status. Level timing
  uses `CLOCK_MONOTONIC` on Android.
* Seven MSVC symbol-name bridges are excluded on ARM because their destination
  has the same native Itanium symbol and would recurse. Android must resolve
  the actual originals instead. ActorBase heap faces explicitly call fBase_c.
* Scene title state (176 bytes), touch state (56 bytes) and the save block
  (812 bytes) each use a complete backing object and ELF aliases at DS offsets.
  Cross-symbol reads no longer cross a C++ array's declared end. `dsstate.ld`
  preserves ordered section contributions; wholesale PC save-state capture is
  **not** implemented on Android.
* The manually seated Scene vtable is named `port_vt_scene_rom` on Android;
  it is distinct from the compiler-generated `_ZTV8dScene_c`, which has the
  Itanium prefix. The strict link caught their original collision.
* CMake includes original `dScDSMT_c` factory, InitResources, Behavior, Render,
  CleanupResources, destructor and OnPendingDestroy; Stage construction;
  Stage's actual InitResources, Behavior and Render translation units;
  scene spawning; StartFile/LoadLevel/SetPlayerGlobals/SetNumPlayers; base actor
  heap/lifecycle hooks; Scene lifecycle; and Fader/Brightness/Color methods.
  MMIO-writing sources pass through PC's `hostgen.py`. No `src/` file changed.
* Android's existing adapter now continues the committed target through real
  Stage creation, collider at Stage+0x91c, cast preparation, and
  `port_stage_a_boot(collider, 1)`. This is the PC's host-assisted boot path,
  which uses a subset of Stage loading; it does not yet establish a successful
  invocation of the complete ROM Stage::InitResources. The adapter ends
  in `level_loop_required`, **not** running gameplay, until the gameplay frame
  loop is integrated. It never re-ticks the dead title or boots twice.

## Verification

* NDK r27c Release cross-build passed: renderer/input JNI `.so`, all component
  `.a` archives, and both Android test executables. Android executables were
  compiled, **not run** on Android.
* ELF checks passed for ARM32, all twelve packed aliases, real game definitions,
  native/manual Scene vtable separation, and absence of recursive bridges.
* All existing host CI tests passed: synthetic ASMP import/catalog/BLZ,
  touch/stylus, PadData/Ctrl/frame order, ROM-clean SHA assembly/refusals,
  readable-memory guard, and title finish/commit ordering.
* New tests passed: actual original StartFile/LoadLevel translation units with
  synthetic save data; Stage boot ordering and target/refusal paths; the actual
  Android adapter's successful and refused handoffs, with observation hooks at
  unavailable game/window seams; actual PC SDAT parser/sequencer/mixer with
  synthetic samples/bytecode (chunk continuity, clock, pan, mute, release).
  These tests do **not** establish real ROM boot or AAudio playback.
* All Android Java sources compiled against API 35; ROM reconstruction tests
  passed with Java 8 source/target compatibility.
* `include/types.h` only selects the platform's `size_t` on Android. Its
  preprocessed output is unchanged for the old ROM and MSVC branches. The
  affected-source tool reports 4,305 consumers.
* ROM byte verification remains unavailable: `prepush_linkcheck.py` reports
  ERROR because `extracted/arm9_dec.bin` is absent. It is not a byte mismatch
  verdict. `port_refcheck.py` also fails on the existing PC CMake parser's
  unsupported `cmake_parse_arguments`; no sources were renamed or moved.

## Full-game link: blocked

The current whole-archive link reports **952 unique unresolved symbols and
zero duplicates**. `check_game_link.py` preserves every linker reference in
`game-link.log` and the symbol inventory in `game-link.json`, and exits nonzero.
CI uploads those reports even on failure. Components are labelled NOT-APK.

Pending dependency families include:

* Original actor processing, player behavior, camera/model/render routines,
  input (`InitControllerMode`), graphics (`Initialise3dGraphics`, GX/GXS),
  allocation/Heap, save-data methods, scene/minigame/overlay call targets.
* Generated overlay mounts, including `port_ov009_patch`, `port_ov009_at`,
  `port_ov009_ds_base/end`, and the corresponding other level/actor overlays.
* ROM-clean generation: `port_romdata_load_all`, `port_romdata_expect_sha`,
  `port_romdata_expect_len`, `port_romdata_bytes`, `port_romdata_sha`.
* Remaining MSVC linker aliases and mwcc PMF dispatch sites. ARM PMFs have the
  required eight-byte width but that alone does not prove their bit encoding
  matches mwcc. Existing dispatch seams must be retained/adapted per site.
* Stage frame adapters `port_stage_rom_behavior`/`port_stage_rom_render` and
  `port_slot0_rom_init`. Android explicitly requires the real slot-6/slot-9
  routes, rather than the PC smoke harness's no-op occupants. Connecting the
  full InitResources path must avoid double-loading after the hosted boot.

After closing the link, the next runtime work remains the real gameplay frame
loop from `walk_window.cpp`: actor/physics updates, camera/GX/HUD/sub-screen
composition, level changes, VBlank, sound, persistent saves, and a dedicated
Android game thread with pause/resume/Surface teardown. None is claimed tested
merely because its containing archive compiles.

## Exact ROM-clean metadata

The PC CMake pipeline is the authority; metadata cannot be inferred from ASMP
alone or copied from a differently assembled build:

1. Run PC's exact `romdata.py` and `ovdata.py` emitters with `--rom-clean` on
   privately extracted user-ROM images. Each emits zeroed source arrays and
   private `.blobpart` plus metadata `.blobmap` files.
2. `port/tools/romblob.py <private romdata.bin> <romdata.manifest>
   <romdata_dispatch.c> <all exact-build .blobmap files>` consolidates the
   parts and stamps the dispatch with the whole SHA-256 and length.
3. `port/tools/romblob_verify.py` verifies the blob against the baked emitter
   layout. `port/tools/romblob_recipe.py <manifest> <romdata.recipe.tsv>`
   resolves every symbol against local ARM9/overlay images, verifies each SHA,
   exact coverage and the reconstructed whole hash, and refuses any mismatch.
4. Ship only the matching metadata containing no Nintendo asset bytes, and
   zeroed/generated code. Android
   reconstructs and verifies the actual resources privately from the user ROM.

This environment has no extracted user ROM and this fork has no public release
assets carrying the exact metadata. No manifest, recipe or hash was invented.

## Hardware evidence still required

No Galaxy A15/SM-A155M or other Android runtime is attached. Android 16/Mali-G57
performance, thermal stability, audio latency and 32-bit availability are
unverified. On the actual device, inspect `adb shell getprop ro.product.cpu.abilist32`
and `android.os.Build.SUPPORTED_32_BIT_ABIS` for `armeabi-v7a`, then install and
run the native ABI/runtime executables before asserting compatibility. ARM
hardware alone does not establish a 32-bit Android userspace.

## Files changed

`scene_boot.cpp`, `level_boot.cpp`, `stage_bridges.cpp`,
`title_vtable_seat14d.cpp`, `port_d16.h`, `dsstate_seg.h`, plus new
`host_call.h`, `host_actor_base.h`, `level_handoff_shared.h` under `port/hal/`;
`include/types.h`; `port/tests/walk_window.cpp` (shared collider calculation);
`port/android/CMakeLists.txt`, `game_scene_entry.cpp/.h`, `dsstate.ld`,
`check_game_layout.py`, `check_game_link.py`, README and this report;
`audio_clock_test.cpp`, `game_abi_test.cpp`, `game_scene_entry_test.cpp`,
`level_handoff_test.cpp`, `start_file_test.c` under `port/android/tests/`;
`.github/workflows/android-native.yml`.
