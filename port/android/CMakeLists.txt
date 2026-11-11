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
  rom_loader.cpp touch_input.cpp)
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
