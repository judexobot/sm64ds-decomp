cmake_minimum_required(VERSION 3.22)
project(sm64ds_android_native LANGUAGES C CXX ASM)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "armeabi-v7a")
  message(FATAL_ERROR "The original game ABI requires Android armeabi-v7a")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
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
  ../hal/rom_frame.cpp ../hal/title_entry.cpp)
target_include_directories(sm64ds_android_game_flow PUBLIC
  ../ntr/include .. .)
target_link_libraries(sm64ds_android_game_flow
  PRIVATE sm64ds_android_core)
