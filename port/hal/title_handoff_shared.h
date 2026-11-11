#pragma once
// The real PC title-to-level finalizer, shared with native Android.
//
// These are the ORIGINAL game's port seams, not an emulation of StartFile.
// port_title_entry_commit validates the ROM's scene-3 request, calls the
// ONE port_level_entry_latch (which reaps title models and carries intro state)
// and leaves the target in port_level_host_boot_target.
//
// The order is load-bearing: report -> finish scene census -> commit title.
// Never call twice. The caller must take the level boot path only if committed.
extern "C" {
void port_rom_frame_report(void);
int port_scene_finish(int frames_run);
int port_title_entry_commit(void);
}
namespace sm64ds_port {
struct TitleFinish {
    int scene_result;
    bool entered_level;
};
inline TitleFinish title_finish_and_commit(int frames_run) {
    port_rom_frame_report();
    const int result = port_scene_finish(frames_run);
    const bool committed = port_title_entry_commit() != 0;
    return {result, committed};
}
}
