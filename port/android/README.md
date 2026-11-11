# SM64DS Android port — integration branch

This branch starts from tangosdev/sm64ds-decomp:port/release-0.4.0,
which has the real game PC boot, title/menu/file select, stages, audio and save code.
It is a genuine native-port effort, but the complete game is NOT yet linked as
an Android binary and NO playable APK is available.

Android changes:
- ARMv7 cooperative VBlank stack switch replacing the Windows-only fiber model.
- NDK build of the actual port/ntr GX, PPU, textures, BMD, and NitroFS components.
- Android ROM loader validates European ASMP and FNT/FAT ranges, then mounts
  the original .nds through the existing game host filesystem.
- CI performs ARMv7 cross-compilation and a synthetic copyright-free NitroFS test.

ROM: bring your own European retail NTR-ASMP-EUR .nds, never commit it or
extracted files. The import function takes an app-private copy path; SAF document
picker integration is still needed in the eventual playable frontend.

The remaining hard blockers are extracting the game frame loop out of
port/tests/walk_window.cpp (Win32/GDI), Android window/touch/gamepad/audio and
save services, link closure with the full game, and validation on Android hardware.
Do not distribute a static library or tests as a game APK.
