#include "readable_guard.h"
#ifndef _WIN32
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <limits>

bool port_host_bad_read_ptr(const void *p, size_t len) {
    if (len == 0) return false; // Mirrors Windows' empty byte range.
    if (!p) return true;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(p);
    if (begin > std::numeric_limits<uintptr_t>::max() - len)
        return true; // wraparound would cause a false-positive check
    const uintptr_t end = begin + len;
    // /proc/self/maps is process-local. No caller ever needs /proc/PID/mem,
    // ptrace or unsafe speculative reads. If unavailable, FAIL CLOSED.
    std::FILE *maps = std::fopen("/proc/self/maps", "r");
    if (!maps) return true;
    char line[4096];
    uintptr_t cursor = begin;
    while (cursor < end && std::fgets(line, sizeof line, maps)) {
        unsigned long long map_start = 0, map_end = 0;
        char perm[5] = {};
        if (std::sscanf(line, "%llx-%llx %4s", &map_start,
                        &map_end, perm) != 3)
            continue;
        // These mappings are sorted. A gap is an invalid address even
        // when the next VMA is readable. PROT_NONE has no 'r'.
        if (map_end <= cursor) continue;
        if (map_start > cursor || perm[0] != 'r')
            break;
        if (map_end > cursor)
            cursor = static_cast<uintptr_t>(map_end < end ? map_end : end);
    }
    std::fclose(maps);
    return cursor != end;
}
#endif
