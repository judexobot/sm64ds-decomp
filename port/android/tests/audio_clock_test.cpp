// Real PC SDAT parser/sequencer/mixer; all test samples and bytecode below are
// synthetic. This does not validate Nintendo music or an Android audio device.
#include "hal/sdat/sdat.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

int out_volume_pct() { return 100; } // test output at unity, no device backend
extern "C" void sd_host_mute(int);

static void start_wave() {
    static const sd_s16 pcm[] = {12000, -12000, 6000, -6000};
    const SdatWave wave{pcm, 4, 1, 0, SD_MIX_RATE};
    sd_mix_start(0, &wave, nullptr, 0, 0, 1.0, 64);
}
static std::vector<sd_s16> render(int chunk) {
    sd_seq_reset(); sd_mix_reset(); start_wave();
    // 48 quarter-note ticks at tempo 120, then end. It must finish on the
    // mixer-driven 192 Hz clock, including when renders arrive in tiny chunks.
    static const sd_u8 seq[] = {0x80, 48, 0xff};
    assert(sd_seq_start(0, seq, 0, nullptr));
    std::vector<sd_s16> out(SD_MIX_RATE * 2);
    for (int i = 0; i < SD_MIX_RATE; i += chunk)
        sd_mix_render(out.data() + i * 2, std::min(chunk, SD_MIX_RATE - i));
    assert(!sd_seq_active(0));
    return out;
}
int main() {
    const auto large = render(SD_MIX_RATE);
    const auto small = render(37);
    assert(large == small); // no envelope/tempo dependence on output block size
    bool audible = false;
    for (std::size_t i = 0; i < small.size(); i += 2) {
        audible |= small[i] != 0;
        assert(small[i + 1] == 0); // full-left pan
    }
    assert(audible && sd_mix_active(0));
    sd_host_mute(1);
    sd_s16 silence[2048];
    sd_mix_render(silence, 1024);
    assert(std::all_of(std::begin(silence), std::end(silence), [](sd_s16 v){return v == 0;}));
    sd_host_mute(0);
    sd_mix_release(0, "synthetic-test");
    sd_mix_render(silence, 1024);
    assert(!sd_mix_active(0));
    std::puts("real SDAT sequencer/mixer: clock, chunk continuity, pan, mute, release PASS (synthetic audio)");
}
