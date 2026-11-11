#pragma once
#include <cstddef>
#include <cstring>

namespace sm64ds_port {
// PC's Ctrl fan-out: source records and all split arrays use the DS stride.
// The ROM Stage writes the records first; publish before the next actor ticks.
inline void ctrl_publish_records(const void *records, int players,
                                 void *held, void *pressed, void *magnitude,
                                 void *x, void *y, void *angle, void *touching) {
    void *fields[] = {held, pressed, magnitude, x, y, angle, touching};
    constexpr std::size_t offsets[] = {4, 6, 8, 10, 12, 14, 20};
    for (int player = 0; player < players; ++player) {
        const std::size_t stride = static_cast<std::size_t>(player) * 0x18;
        for (std::size_t field = 0; field < 7; ++field)
            std::memcpy(static_cast<char *>(fields[field]) + stride,
                        static_cast<const char *>(records) + stride + offsets[field],
                        field == 6 ? 1 : 2);
    }
}
}
