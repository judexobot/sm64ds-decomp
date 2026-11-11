#include "blz.h"
#include <cstring>
#include <vector>
namespace sm64ds_android {
namespace {
constexpr size_t MAX_DECOMPRESSED = 64u * 1024u * 1024u;
uint32_t rd32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
}
// Faithful to the PC portable kit's Expand-Blz: compressed backwards from
// a 4-byte [header length:8 | compressed length:24] footer + 4-byte expansion;
// a footer expansion of zero denotes a stored/plain module.
bool expand_blz(std::vector<uint8_t> *data, size_t expected_size) {
    if (!data) return false;
    const size_t old_size = data->size();
    if (old_size < 8 || rd32(data->data() + old_size - 4) == 0)
        return !expected_size || old_size <= expected_size;
    const uint32_t footer = rd32(data->data() + old_size - 8);
    const size_t header_size = footer >> 24;
    const size_t compressed_size = footer & 0xFFFFFFu;
    const size_t expansion = rd32(data->data() + old_size - 4);
    if (header_size < 8 || compressed_size < header_size ||
        compressed_size > old_size || expansion > MAX_DECOMPRESSED ||
        old_size > MAX_DECOMPRESSED - expansion)
        return false;
    const size_t full_size = old_size + expansion;
    if (expected_size && expected_size != full_size) return false;
    const size_t prefix = old_size - compressed_size;
    size_t src = old_size - header_size, dst = full_size;
    std::vector<uint8_t> result(full_size);
    std::memcpy(result.data(), data->data(), old_size);
    // Back references index already-decoded bytes to the right. The game
    // extractor's rare case of displacement > decoded length uses distance 2.
    while (dst > prefix) {
        if (src <= prefix) return false;
        const uint8_t flags = result[--src];
        for (int bit = 7; bit >= 0 && dst > prefix; --bit) {
            if (flags & (1u << bit)) {
                if (src < prefix + 2) return false;
                const uint8_t b1 = result[--src], b2 = result[--src];
                const size_t length = (b1 >> 4) + 3;
                size_t dist = (((b1 & 15u) << 8) | b2) + 3;
                const size_t done = full_size - dst;
                if (dist > done) {
                    if (done < 2) return false;
                    dist = 2;
                }
                for (size_t i = 0; i < length; ++i) {
                    if (dst <= prefix || dst - 1 + dist >= full_size)
                        return false;
                    --dst;
                    result[dst] = result[dst + dist];
                }
            } else {
                if (src <= prefix || dst <= prefix) return false;
                result[--dst] = result[--src];
            }
        }
    }
    *data = std::move(result);
    return true;
}
}
