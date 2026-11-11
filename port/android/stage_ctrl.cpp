// Same post-Stage::Behavior publication as PC's walk_window input boundary.
// Raw PadData still comes from scene_input; CheckInput remains the original.
#include "hal/ctrl_publish_shared.h"
#include "hal/vs_width.h"
extern "C" {
extern unsigned char data_0209f21c;
extern char data_0209f498[], data_0209f49c[], data_0209f49e[];
extern char data_0209f4a0[], data_0209f4a2[], data_0209f4a4[], data_0209f4a6[], data_0209f4ac[];
void port_frame_ctrl_publish(void) {
    int players = data_0209f21c;
    if (players < 1) players = 1;
    if (players > kPortMaxPlayers) players = kPortMaxPlayers;
    sm64ds_port::ctrl_publish_records(data_0209f498, players,
        data_0209f49c, data_0209f49e, data_0209f4a0, data_0209f4a2,
        data_0209f4a4, data_0209f4a6, data_0209f4ac);
}
}
