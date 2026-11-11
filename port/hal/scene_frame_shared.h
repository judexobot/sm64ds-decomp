#pragma once
// The original PC scene loop's game-frame seam, without any Win32 types.
// The actual scene logic still comes from hal/scene_boot.cpp. The order is
// INPUT -> scene_tick -> frame present -> sound while paused -> phase-6.
//
// Android and the PC frontend can use this exact interface; no scene code or
// gameplay behaviour is reimplemented here. The first full Android link must
// include hal/scene_boot.cpp and its complete game closure.
extern "C" {
void port_rom_frame_begin(const char *loop);
int port_rom_frame_checked(int frame, const char *consumer);
void port_rom_frame_phase6(void);
void port_scene_tick(int frame, int tick_game);
void sdat_host_tick(void);
}
namespace sm64ds_port {
inline void scene_frame_begin(const char *label) {
    port_rom_frame_begin(label);
}
inline void scene_frame_simulate(int frame, bool tick_game) {
    port_scene_tick(port_rom_frame_checked(frame, "scene-tick"),
                    tick_game ? 1 : 0);
}
inline void scene_frame_complete(bool tick_game) {
    // Same PC rule: the scene tick already pumped ARM7 sound on live frames,
    // and only a paused frame needs its device buffers serviced separately.
    if (!tick_game) sdat_host_tick();
    port_rom_frame_phase6();
}
}
