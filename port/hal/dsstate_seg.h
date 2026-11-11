/* One linker section family that gathers every hosted DS global the save state
 * has to roll back, so the snapshot captures them WHOLESALE by section bounds
 * instead of a hand-maintained symbol list. The 0.2.0 crash was a symbol the
 * hand list (23 entries) never named: the ANIM_PTRS SharedFilePtr table in
 * ov002, a hosted overlay global that survived a restore and handed
 * Player::SetAnim a null BCA_File*. A list that has to be grown by hand for
 * every hosted symbol is the shape of that bug, so this replaces it.
 *
 * HOW IT WORKS
 * ------------
 * MSVC merges same-named grouped sections in order of the text after the `$`,
 * exactly like the CRT's .CRT$XCA..XCZ initializer table. So one base section
 * .dsstate with a low sentinel in .dsstate$aaa and a high sentinel in
 * .dsstate$zzz brackets everything a translation unit routes into .dsstate$mmm
 * (or any suffix that sorts between aaa and zzz). hal/dsstate_seg.cpp defines
 * the two sentinels and lk6_savestate.cpp memcpy's [dsstate_lo, dsstate_hi).
 *
 * A file that hosts DS globals puts DSSTATE_BEGIN before its definitions and
 * DSSTATE_END after them; every plain global (bss or data) in between lands in
 * the captured span with no per-symbol annotation. The overlay data generators
 * (tools/ovdata.py) emit these two markers around their arrays and a per-slot
 * $mmmNNNN suffix for the packed mounts so a mount's ROM-spaced run stays
 * contiguous inside the bracket.
 *
 * WHAT MUST NOT GO IN HERE
 * ------------------------
 * Host-only bookkeeping that must NOT roll back on a restore: the playlog path,
 * the frame counter, telemetry, the window and frame pacing in ntr_2x, out_win,
 * the launcher glue and the CRT. Those files simply do not use these markers, so
 * their globals stay in the ordinary .bss/.data the snapshot never touches. The
 * line is drawn per symbol, not per file: ntr/runtime.cpp is a host library that
 * happens to own two genuine DS globals (the DMA state and callback table), and
 * those two ARE bracketed while the rest of that file is not. The audio path is
 * captured like everything else but the live sequencer/mixer are additionally
 * reset on load (see lk6_savestate.cpp); that reset is the one deliberate
 * exception and it runs after the section copy.
 */
#ifndef PORT_HAL_DSSTATE_SEG_H
#define PORT_HAL_DSSTATE_SEG_H

/* Declare the attributes of the shared slot BEFORE anything routes into it.
 *
 * This line is load-bearing and its absence is silent. A bare bss_seg gives its
 * contribution CNT_UNINITIALIZED_DATA while the sentinels' explicitly-declared
 * section carries CNT_INITIALIZED_DATA, and the linker does NOT merge two
 * same-named sections whose characteristics differ -- it emits a SECOND output
 * section also called .dsstate, placed nowhere near the fence, and says so only
 * as `LNK4078: multiple '.dsstate' sections found with different attributes`.
 * Every hosted global in that second section is outside [dsstate_lo, dsstate_hi)
 * and is silently not captured, which is the exact bug this scheme exists to
 * kill. Declaring the section here pins one attribute set for the whole family,
 * so bss- and data-flavoured contributions land in the same output section.
 *
 * The cost is that formerly-uninitialized hosted globals are now zero-filled in
 * the image rather than demand-zeroed (~35 KB of exe). They stay writable: the
 * `write` attribute is part of what is being pinned. */
#if defined(_MSC_VER)
#pragma section(".dsstate$mmm", read, write)

/* Route every plain global between these two markers into the captured span.
 * $mmm sorts between the $aaa low sentinel and the $zzz high sentinel. Both
 * bss and data defaults are set so uninitialized and initialized hosted globals
 * both land inside the bracket. */
#define DSSTATE_BEGIN                                                          \
    __pragma(bss_seg(".dsstate$mmm"))                                          \
    __pragma(data_seg(".dsstate$mmm"))

/* A LATER `extern` RE-DECLARATION IN THE SAME TU UNDOES THIS, silently.
 *
 * MSVC fixes a symbol's segment from the declaration it saw last, not from the
 * one that defined it. So a file that defines `int data_0209fc48;` inside the
 * bracket and then, several hundred lines on in its own forward-declare block,
 * writes `extern int data_0209fc48;` while the default segment is back in
 * force, gets a symbol in ordinary .bss -- outside [dsstate_lo, dsstate_hi)
 * and not captured. Nothing warns: the file compiles, it links, and the
 * bracket around the definition still looks right.
 *
 * hal/level_boot.cpp had exactly that shape for two of the six globals the
 * w6-c Player::InitResources retirement moved into it, and dsstate_guard is
 * what found it -- inside ONE DSSTATE block, the four symbols with no later
 * re-declaration landed in the section and the two with one landed outside.
 * The fix is to delete the redundant declaration; the definition is already in
 * scope above it. */

/* Restore the default (unnamed) segments so anything after the hosted block --
 * a host-only helper's static, a string literal pool -- is not swept in. */
#define DSSTATE_END                                                            \
    __pragma(bss_seg())                                                        \
    __pragma(data_seg())
#else
/* Android/Clang does not support MSVC's section-range directives. The
 * platform-specific savestate implementation must allocate its own data
 * section and bounds before enabling save-state capture. Do not silently
 * claim Android save-state support merely because this compiles. */
#define DSSTATE_BEGIN
#define DSSTATE_END
#endif

#endif /* PORT_HAL_DSSTATE_SEG_H */
