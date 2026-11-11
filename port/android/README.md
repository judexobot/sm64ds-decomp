# Super Mario 64 DS — Android native port (WIP)

The branch starts from tangosdev/sm64ds-decomp:port/release-0.4.0, not the
smoke-test-only main branch. That PC tree includes original gameplay, menus,
stages, game saves, HUD, audio and level logic, but it is still tightly coupled
to a Win32 frontend.

## Real Android components now in the branch

- Android NDK ARMv7 build, maintaining the game's 32-bit ABI.
- Cooperative ARMv7 VBlank fiber/scheduler implementation.
- Real NTR filesystem, BMD, GX/PPU, texturing, IPC and LZ77 engine modules.
- Local import of user's own European ASMP .nds, with FAT/FNT validation.
- SurfaceView + transparent TouchControlsView for Android portrait/landscape
  host layout (actual game should run in landscape).
- Multi-touch joystick + A/B/X/Y, L/R, R2, Start and Select. Input is
  published in the same XInput-shaped PortPadState the PC port already reads.
  Touch controls are held independently and released on CANCEL/pause.
- Optional Nintendo DS stylus input (0..255 by 0..191) for bottom-screen
  menus and minigames. TouchControlsView.setStylusRegion defines the visible
  sub-screen rectangle; the existing game's TouchInfo poll has an Android
  input branch to receive these values, not a substitute minigame.
- NTR_WIDE_RT for runtime aspect/render-scale support. The JNI
  nativeConfigureDisplay API keeps the underlying game's wide-buffer path.
- ANativeWindow RGBA_8888 frame presenter: accepts the original engine's
  ARGB composited output and letterboxes it correctly on mobile surfaces.
- Android JNI bridge and reusable NativeScreenLayout to pair real display
  pixels with the touch overlay.
- Synthetic ROM loader and native touch tests in CI; javac verification of
  the Android View classes; ARMv7 native .so/.a cross-compilation in CI.

The CI artifact is a native library / Java classes and tests, *not* a game APK.
There is deliberately no mock game Activity and no fake playable artifact.

## Using your own ROM

The decomp targets the European retail Super Mario 64 DS image, game code
ASMP. Keep your legally dumped cartridge ROM outside GitHub. A real app
should open Android ACTION_OPEN_DOCUMENT and use RomImporter.importEuropeanRom
to copy its selected Uri into app-private storage; that helper calls the NTR
ROM validation and keeps no network dependency. Nintendo assets must never be
checked into source or packed into public CI artifacts.

## Android integration still required before shipping a real APK

1. Separate the real game loop, title, stage/scene rendering and controls from
   port/tests/walk_window.cpp (large Win32/GDI-specific source), and link the
   full original game into the same libsm64ds_android.so that owns input.
2. Integrate the existing game's 2D/3D compositing and frame handoff with
   sm64ds_android::present_frame. Ensure both DS screens are drawn during
   minigames and publish the actual sub-screen rectangle for stylus mapping.
3. Replace/port Windows audio, file paths, callbacks, OS threads, save path,
   clock and lifecycle to Android (AAudio/Oboe or equivalent, native app APIs).
4. Wire the gamepad bridge to each gameplay and scene path; test all touch
   releases, camera, attack, running and native DS stylus interactions.
5. Create Android package only after linking the full game entry point.
   Test title, file select, opening cutscene, adventure levels, minigames,
   level transitions, controller, sound, save and resume on real hardware.
6. Validate ARMv7 32-bit support and fixed DS memory mappings on the
   target mobile device. Those mappings cannot be assumed to work on every
   Android release/device.

No native game boot or complete Android gameplay has been verified yet.
Do not label this branch as a full Android port or ship an empty shell.
