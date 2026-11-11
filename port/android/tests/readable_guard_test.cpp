#include "hal/readable_guard.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    int live = 42;
    assert(!port_host_bad_read_ptr(&live, sizeof live));
    assert(port_host_bad_read_ptr(nullptr, sizeof live));
    assert(!port_host_bad_read_ptr(nullptr, 0));
    assert(port_host_bad_read_ptr(reinterpret_cast<void *>(~size_t(3)), 8));

    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void *region = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(region != MAP_FAILED);
    std::memset(region, 0x5A, page);
    assert(!port_host_bad_read_ptr(region, page));
    assert(mprotect(static_cast<char *>(region) + page, page, PROT_NONE) == 0);
    assert(port_host_bad_read_ptr(static_cast<char *>(region) + page, 1));
    assert(port_host_bad_read_ptr(static_cast<char *>(region) + page - 1, 2));
    assert(!port_host_bad_read_ptr(static_cast<char *>(region) + page - 1, 1));
    assert(munmap(region, page * 2) == 0);
    std::puts("original PC level guards: readable, PROT_NONE, crossed range, overflow passed");
}
