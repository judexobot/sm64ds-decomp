#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sm64ds_android {
// Original Nintendo DS backward LZ used by ARM9 and overlay code. The decoded
// bytes come solely from the user's private cartridge, never from the APK.
// expected_size=0 allows ARM9 (where the target length is in the BLZ footer).
bool expand_blz(std::vector<uint8_t> *bytes,
                size_t expected_size = 0);
}
