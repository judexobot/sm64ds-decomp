// Real PC save medium, synthetic bytes, across two independent processes.
// No ROM, Android OS or SaveData policy is simulated by this storage test.
#include "ntr/backup.h"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <sys/stat.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    const bool writing = std::strcmp(argv[1], "write") == 0;
    assert(ntr::backup::was_fresh() == writing);
    std::array<unsigned char, 64> wanted{}, actual{};
    for (unsigned i = 0; i < wanted.size(); ++i) wanted[i] = static_cast<unsigned char>(i * 11 + 7);
    if (writing) {
        assert(ntr::backup::read(33, actual.data(), actual.size()));
        for (auto byte : actual) assert(byte == 0xff);
        assert(ntr::backup::write(33, wanted.data(), wanted.size()));
    }
    assert(ntr::backup::read(33, actual.data(), actual.size()) && wanted == actual);
    assert(ntr::backup::verify(33, wanted.data(), wanted.size()));
    wanted[0] ^= 1;
    assert(!ntr::backup::verify(33, wanted.data(), wanted.size()));
    assert(!ntr::backup::write(8191, wanted.data(), 2));
    assert(!ntr::backup::read(std::numeric_limits<unsigned>::max(), actual.data(), 2));
    assert(!ntr::backup::write(0, wanted.data(), std::numeric_limits<unsigned>::max()));
    assert(ntr::backup::read(97, actual.data(), 1) && actual[0] == 0xff);
    struct stat info{};
    assert(stat(ntr::backup::path(), &info) == 0 && info.st_size == 8192);
    assert(ntr::backup::stats().flush_failures == 0);
    assert(ntr::backup::stats().writes == (writing ? 1u : 0u));
}
