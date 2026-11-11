#include "hal/ctrl_publish_shared.h"
#include <array>
#include <cassert>
#include <cstdint>

int main() {
    std::array<uint8_t, 16 * 0x18> records{};
    std::array<std::array<uint8_t, 16 * 0x18>, 7> fields{};
    for (unsigned i = 0; i < records.size(); ++i)
        records[i] = static_cast<uint8_t>(i * 7 + 3);
    for (auto &field : fields) field.fill(0xa5);
    sm64ds_port::ctrl_publish_records(records.data(), 16, fields[0].data(),
        fields[1].data(), fields[2].data(), fields[3].data(), fields[4].data(),
        fields[5].data(), fields[6].data());
    const unsigned offsets[] = {4, 6, 8, 10, 12, 14, 20};
    for (unsigned player = 0; player < 16; ++player)
        for (unsigned field = 0; field < 7; ++field)
            for (unsigned byte = 0; byte < 0x18; ++byte)
                assert(fields[field][player * 0x18 + byte] ==
                    (byte < (field == 6 ? 1u : 2u)
                     ? records[player * 0x18 + offsets[field] + byte] : 0xa5));
}
