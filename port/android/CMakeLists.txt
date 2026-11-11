cmake_minimum_required(VERSION 3.22)
project(sm64ds_android_native LANGUAGES C CXX ASM)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "armeabi-v7a")
  message(FATAL_ERROR "The original game ABI requires Android armeabi-v7a")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_library(sm64ds_android_core STATIC
  ../ntr/io.cpp ../ntr/gx.cpp ../ntr/texture.cpp ../ntr/ppu.cpp
  ../ntr/lz77.cpp ../ntr/fs.cpp ../ntr/bmd.cpp ../ntr/hdtex.cpp
  ../ntr/smooth.cpp ../ntr/ppu_sub.cpp ../ntr/ppu_audit.cpp
  ../ntr/ipc.cpp ../ntr/runtime.cpp ../ntr/rt_android.cpp ../ntr/arm_switch.S
  rom_loader.cpp blz.cpp assets_extract.cpp touch_input.cpp)
target_include_directories(sm64ds_android_core PUBLIC ../ntr/include .. .)
target_compile_definitions(sm64ds_android_core PUBLIC SM64DS_PLATFORM_ANDROID=1 NTR_WIDE_RT=1)
target_link_libraries(sm64ds_android_core PUBLIC log)
add_executable(android_runtime_test tests/runtime_test.cpp)
target_link_libraries(android_runtime_test PRIVATE sm64ds_android_core)
add_executable(android_game_abi_test tests/game_abi_test.cpp)
target_include_directories(android_game_abi_test PRIVATE ../../include)
target_link_libraries(android_game_abi_test PRIVATE sm64ds_android_core)
target_compile_options(android_game_abi_test PRIVATE -Wno-invalid-offsetof -UNDEBUG)

# Android window / SurfaceView output and Java JNI bindings.
# This is the actual NTR renderer and input backend, NOT the full game link.
add_library(sm64ds_android SHARED
  native_bridge.cpp android_present.cpp)
target_link_libraries(sm64ds_android PRIVATE sm64ds_android_core android log)
target_include_directories(sm64ds_android PRIVATE ../ntr/include .. .)

# Direct replacement for the Windows WinMM SDAT output stage. This calls the
# actual game's 16-channel sd_mix_render from port/hal/sdat/mixer.cpp when the
# full game closure is linked. Keep the backend separate until that game link
# exists; compiling a .a does not imply playable audio yet.
add_library(sm64ds_android_audio STATIC out_android.cpp)
target_include_directories(sm64ds_android_audio PUBLIC .. .)
target_link_libraries(sm64ds_android_audio PUBLIC aaudio log)


# The REAL scene/title/minigame frame driver, not a sample renderer.
# It directly references port_scene_begin/port_scene_tick and the original
# NTR PadData globals in hal/scene_boot.cpp. Keep as a separate static archive
# until the Windows-hosted game's complete link closure can be ported: linking
# only the NDK core into libsm64ds_android.so is NOT a game build.
add_library(sm64ds_android_game_entry STATIC
  game_scene_entry.cpp scene_input.cpp)
target_include_directories(sm64ds_android_game_entry PUBLIC
  ../ntr/include .. .)
target_link_libraries(sm64ds_android_game_entry
  PRIVATE sm64ds_android_core)


# Original PC ROM frame clock + title-entry controller. Compile as game-side
# Android objects rather than copying their policy or linking a fake scene.
# These archives can carry unresolved original game symbols; the eventual
# complete game .so must resolve them against the whole port/hal and src set.
add_library(sm64ds_android_game_flow STATIC
  ../hal/rom_frame.cpp ../hal/title_entry.cpp
  ../hal/level_change.cpp ../hal/readable_guard.cpp
  ../hal/boot_os.cpp ../hal/romdata_loader.cpp ../hal/rom_main.cpp
  ../hal/boot_arms.cpp ../hal/os_thread.cpp
  ../hal/scene_boot.cpp ../hal/level_boot.cpp
  ../hal/stage_bridges.cpp ../../src/d_s_stage.cpp
  ../hal/title_vtable_seat14d.cpp
  ../hal/scene_actor_faces.cpp
  ../../src/d_s_dsmt.c
  ../../src/_ZN9dScDSMT_c8BehaviorEv.cpp
  ../../src/_ZN9dScDSMT_c6RenderEv.cpp
  ../../src/_ZN9dScDSMT_c16CleanupResourcesEv.cpp
  ../../src/_ZN9dScDSMT_c16OnPendingDestroyEv.cpp
  ../../src/_ZN9dScDSMT_cD1Ev.cpp
  ../../src/StartFile.c ../../src/LoadLevelNoReturn.c ../../src/LoadLevel.c
  ../../src/SetPlayerGlobals.c ../../src/SetNumPlayers.c
  ../../src/_ZN8dScene_c15SetSceneToSpawnEjj.cpp
  ../../src/_ZN8dScene_c16SpawnIfNecessaryEv.cpp
  ../../src/func_02043098.c ../../src/func_02043060.c
  ../../src/func_02043180.c ../../src/func_020433b8.c
  ../../src/func_02042fe4.c
  ../../src/_ZN7fBase_c9Virtual34Ejj.cpp ../../src/_ZN7fBase_c9Virtual38Ejj.cpp
  ../../src/_ZN7fBase_c19BeforeInitResourcesEv.cpp
  ../../src/DecIfAbove0_Short.c ../../src/SetNextStar.c
  ../../src/_ZN8dScene_c14StartSceneFadeEjjt.cpp
  ../../src/_ZN8dScene_c9SetFadersEP15FaderBrightness.cpp
  ../../src/engine/fader/_ZN5FaderD1Ev.cpp
  ../../src/engine/fader/_ZN5Fader13AdvanceInterpEv.cpp
  ../../src/engine/fader/_ZN15FaderBrightnessD1Ev.cpp
  ../../src/engine/fader/_ZN15FaderBrightness11AdvanceFadeEv.cpp
  ../../src/engine/fader/_ZN15FaderBrightness15SetBackwardTimeEj.cpp
  ../../src/engine/fader/_ZN15FaderBrightness14SetForwardTimeEj.cpp
  ../../src/engine/fader/_ZN15FaderBrightness9IsAtStartEv.cpp
  ../../src/engine/fader/_ZN15FaderBrightness7IsAtEndEv.cpp
  ../../src/engine/fader/_ZN15FaderBrightness20IsBetweenStartAndEndEv.cpp
  ../../src/engine/fader/_ZN15FaderBrightness8SetToEndEv.cpp
  ../../src/engine/fader/_ZN15FaderBrightness10SetToStartEv.cpp
  ../../src/engine/fader/_ZN10FaderColorD1Ev.cpp
  ../hal/sdat/consumer.cpp ../hal/sdat/mixer.cpp
  ../hal/sdat/sdat.cpp ../hal/sdat/sseq.cpp
  ../hal/sdat/sound_abi.cpp ../hal/sdat/sound_bss.cpp
  asset_root_refuse_android.cpp)
target_include_directories(sm64ds_android_game_flow PUBLIC
  ../ntr/include .. . ../hal ../../include)
target_link_options(sm64ds_android_game_flow INTERFACE
  "LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/dsstate.ld")
target_link_libraries(sm64ds_android_game_flow
  PRIVATE sm64ds_android_core)

# The same ROM-clean separation used by the shipping PC port:
# no Nintendo bytes in Android libraries. The generated romdata_dispatch.c
# and exact metadata-only PC release recipe must be supplied before link.
target_compile_definitions(sm64ds_android_game_flow PRIVATE PORT_ROM_CLEAN=1)
# Raw ROM address-point tables have no Itanium RTTI/offset prefix. Keep the
# manually seated Scene table distinct from the compiler's native vtable,
# emitted by the original BeforeInitResources key method. This macro renames
# explicit storage tokens only; compiler-generated C++ symbols stay native.
target_compile_definitions(sm64ds_android_game_flow PRIVATE
  _ZTV8dScene_c=port_vt_scene_rom
  SM64DS_STAGE_SLOT6_ROM=1 SM64DS_STAGE_SLOT9_ROM=1)
# Reuse the original Nintendo DS 16-voice SDAT mixer with Android AAudio,
# NOT Windows' out_win.cpp or any substitute synthesized tracks.
target_link_libraries(sm64ds_android_game_flow PRIVATE sm64ds_android_audio)

# Use the PC's real source transformer: the title InitResources writes DS
# MMIO registers, which must pass through NTR_MMIO on Android too. No copy of
# Nintendo data and no edits to the byte-matched src/ translation unit.
foreach(source IN ITEMS
    src/_ZN9dScDSMT_c13InitResourcesEv.cpp
    src/engine/fader/_ZN10FaderColor11AdvanceFadeEv.cpp
    src/Enable3dEngines.c
    src/_ZN5Stage13InitResourcesEv.cpp
    src/_ZN5Stage8BehaviorEv.cpp
    src/_ZN5Stage6RenderEv.cpp
    src/_ZN7fBase_c13OnHeapCreatedEv.cpp
    src/_ZN7fBase_c13InitResourcesEv.cpp
    src/_ZN7fBase_c16CleanupResourcesEv.cpp
    src/_ZN7fBase_c13AfterBehaviorEj.cpp
    src/_ZN7fBase_c11AfterRenderEj.cpp
    src/_ZN7fBase_c18AfterInitResourcesEj.cpp
    src/_ZN7fBase_c21AfterCleanupResourcesEj.cpp
    src/_ZN8dScene_c19BeforeInitResourcesEv.cpp
    src/_ZN8dScene_c22BeforeCleanupResourcesEv.cpp
    src/_ZN8dScene_c21AfterCleanupResourcesEj.cpp
    src/_ZN8dScene_c14BeforeBehaviorEv.cpp
    src/_ZN8dScene_c12BeforeRenderEv.cpp
    src/actors/ActorDerived.cpp)
  string(REGEX REPLACE "\\.c$" ".cpp" generated "${source}")
  set(generated "${CMAKE_CURRENT_BINARY_DIR}/host-src/${generated}")
  add_custom_command(OUTPUT "${generated}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/hostgen.py"
      --out "${CMAKE_CURRENT_BINARY_DIR}/host-src" --extern-data "${source}"
    DEPENDS ../tools/hostgen.py "../../${source}"
    VERBATIM)
  target_sources(sm64ds_android_game_flow PRIVATE "${generated}")
endforeach()

# Deliberately strict, EXPLICIT whole-game link test. A static archive accepts
# undefined symbols and unused archive members can hide missing game code.
# Build this target to expose the current closure; never package this probe
# as an APK or infer playability from resolving its symbols alone.
add_library(sm64ds_android_game_link_check SHARED EXCLUDE_FROM_ALL native_bridge.cpp)
target_include_directories(sm64ds_android_game_link_check PRIVATE
  ../ntr/include .. .)
target_link_libraries(sm64ds_android_game_link_check PRIVATE
  "-Wl,--whole-archive"
  sm64ds_android_game_entry sm64ds_android_game_flow
  sm64ds_android_audio sm64ds_android_core
  "-Wl,--no-whole-archive" android log aaudio)
target_link_options(sm64ds_android_game_link_check PRIVATE
  "LINKER:--no-undefined" "LINKER:--error-limit=0")
set_property(TARGET sm64ds_android_game_link_check APPEND PROPERTY
  LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dsstate.ld")

