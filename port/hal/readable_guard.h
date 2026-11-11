#pragma once
#include <cstddef>
// The PC port checks whether an actor still occupies readable mapped memory
// before inspecting it during scene teardown or rejected level transitions.
// Windows preserves the existing IsBadReadPtr behaviour, Android/Linux uses
// a conservative, read-only /proc/self/maps inspection. Neither is a lifetime
// ownership guarantee; all calls are best-effort checks, not pointer leases.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
inline bool port_host_bad_read_ptr(const void *p, size_t len) {
    return IsBadReadPtr(p, len) != 0;
}
#else
bool port_host_bad_read_ptr(const void *p, size_t len);
#endif
