# Android native port — ARMv7 platform work

This cross-compiles the real NTR native engine modules and a cooperative
Android ARMv7 frame scheduler. It does NOT yet build the complete game, does
NOT create a playable APK, and should not be presented as a game release.

The 32-bit ABI is necessary because the decompiled game stores pointers in
32-bit fields. The scheduler replaces Win32 fibers with native ARMv7 context
switching without rewriting the game's VBlank-blocking control flow.

Actions compile the engine library and native self-test (LZ77, GX and
cooperative VBlank). The test still needs runtime execution on an ARMv7
Android device or suitable emulator.

## Build
Use NDK r27c, CMake and Ninja:
    cmake -S port/android -B build/android-armv7 -G Ninja -DCMAKE_TOOLCHAIN_FILE="<NDK>/build/cmake/android.toolchain.cmake" -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-28
    cmake --build build/android-armv7

## Required to make the game playable
1. Integrate the actual game bootstrap, menus, stages, save/audio and full
   link closure from tangosdev/sm64ds-decomp branch port/release-0.4.0.
   The current fork's main branch only contains the early PC smoke gates.
2. Port Win32/GDI frontend and input to Android lifecycle, ANativeWindow,
   touch/gamepads and native audio, preserving the original game code.
3. Import cartridge-derived assets from the user's own game, on-device;
   never package ROM bytes in GitHub artifacts.
4. Validate 32-bit ABI availability, fixed memory windows, boot, controls,
   saving, level transitions and stability on a physical Android device.

Do not label a CI-built native archive a functional Android game.
