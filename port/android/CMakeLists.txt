cmake_minimum_required(VERSION 3.22)
project(sm64ds_android_native LANGUAGES C CXX ASM)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "armeabi-v7a")
    message(FATAL_ERROR "SM64DS port currently requires armeabi-v7a")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Actual NTR engine components, without Nintendo assets or fake game output.
add_library(sm64ds_android_core STATIC
    ../ntr/io.cpp ../ntr/gx.cpp ../ntr/texture.cpp ../ntr/ppu.cpp
    ../ntr/lz77.cpp ../ntr/fs.cpp ../ntr/bmd.cpp
    ../ntr/rt_android.cpp ../ntr/arm_switch.S)
target_include_directories(sm64ds_android_core PUBLIC "../ntr/include")
target_compile_definitions(sm64ds_android_core PUBLIC SM64DS_PLATFORM_ANDROID=1)

add_executable(android_runtime_test tests/runtime_test.cpp)
target_link_libraries(android_runtime_test PRIVATE sm64ds_android_core)
