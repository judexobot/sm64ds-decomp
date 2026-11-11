// Android output for the SAME DS SDAT mixer used by the PC port.
// Replaces WinMM waveOut only; the 16 DS voices, 192 Hz envelopes,
// decoded SWAVs, and game-issued SSEQ commands remain unchanged.
#include "hal/sdat/sdat.h"
#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr unsigned RING_FRAMES = 1u << 15;
constexpr unsigned RING_MASK = RING_FRAMES - 1;
constexpr int MIX_BLOCK = 1024;
std::array<sd_s16, RING_FRAMES * 2> samples{};
std::atomic<uint32_t> producer{0}, consumer{0};
std::atomic<unsigned> underruns{0}, overruns{0};
std::atomic<bool> disconnected{false};
AAudioStream *device = nullptr;        // owned by game thread, not callback
bool attempted = false;
int device_rate = 48000;
double ratio = double(SD_MIX_RATE) / 48000.0;

// The resampler's state is owned exclusively by the AAudio callback thread.
sd_s16 prev_sample[2]{}, next_sample[2]{};
double phase = 0;
bool primed = false;
std::atomic<int> volume_pct{-1};
int frame_remainder = 0;
std::FILE *wav = nullptr;
sd_u32 wav_frames = 0;

bool pop_stereo(sd_s16 out[2]) {
    const uint32_t r = consumer.load(std::memory_order_relaxed);
    if (r == producer.load(std::memory_order_acquire)) {
        out[0] = out[1] = 0;
        return false;
    }
    const unsigned i = (r & RING_MASK) * 2u;
    out[0] = samples[i];
    out[1] = samples[i + 1];
    consumer.store(r + 1, std::memory_order_release);
    return true;
}
void push_stereo(const sd_s16 *src, int frames) {
    uint32_t w = producer.load(std::memory_order_relaxed);
    const uint32_t r = consumer.load(std::memory_order_acquire);
    int i = 0;
    while (i < frames && w - r < RING_FRAMES) {
        const unsigned pos = (w & RING_MASK) * 2u;
        samples[pos] = src[2 * i];
        samples[pos + 1] = src[2 * i + 1];
        ++i; ++w;
    }
    if (i != frames) overruns.fetch_add(1, std::memory_order_relaxed);
    producer.store(w, std::memory_order_release);
}
aaudio_data_callback_result_t audio_callback(AAudioStream *, void *,
                                             void *output, int32_t frames) {
    sd_s16 *dst = static_cast<sd_s16 *>(output);
    bool starving = false;
    if (!primed) {
        if (!pop_stereo(prev_sample)) starving = true;
        if (!pop_stereo(next_sample)) starving = true;
        primed = true;
    }
    for (int32_t i = 0; i < frames; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            const double a = double(prev_sample[ch]);
            const double b = double(next_sample[ch]);
            dst[2 * i + ch] = static_cast<sd_s16>(
                std::lround(a + (b - a) * phase));
        }
        phase += ratio;
        while (phase >= 1.0) {
            prev_sample[0] = next_sample[0];
            prev_sample[1] = next_sample[1];
            if (!pop_stereo(next_sample)) starving = true;
            phase -= 1.0;
        }
    }
    if (starving) underruns.fetch_add(1, std::memory_order_relaxed);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
void audio_error(AAudioStream *, void *, aaudio_result_t) {
    // Never close/reopen from AAudio's callback thread.
    disconnected.store(true, std::memory_order_release);
}
void wav_header(std::FILE *f, sd_u32 frames) {
    const sd_u32 data_size = frames * 4u, riff_size = data_size + 36u;
    const sd_u32 fmt_bytes = 16, rate = SD_MIX_RATE, bytes_per_s = rate * 4u;
    const sd_u16 pcm = 1, channels = 2, alignment = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&riff_size, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt_bytes, 4, 1, f);
    std::fwrite(&pcm, 2, 1, f);
    std::fwrite(&channels, 2, 1, f);
    std::fwrite(&rate, 4, 1, f);
    std::fwrite(&bytes_per_s, 4, 1, f);
    std::fwrite(&alignment, 2, 1, f);
    std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f);
    std::fwrite(&data_size, 4, 1, f);
}
}

int out_volume_pct(void) {
    int v = volume_pct.load(std::memory_order_relaxed);
    if (v >= 0) return v;
    const char *env = std::getenv("SM64DS_VOLUME");
    if (env && *env) v = std::atoi(env);
    else v = std::getenv("SM64DS_SOUND") ? 100 : 50;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    volume_pct.store(v, std::memory_order_relaxed);
    return v;
}
extern "C" void out_set_volume_pct(int pct) {
    volume_pct.store(pct < 0 ? 0 : (pct > 100 ? 100 : pct),
                     std::memory_order_relaxed);
}
void sd_wav_open(const char *path) {
    if (wav || !path) return;
    wav = std::fopen(path, "wb");
    if (!wav) return;
    wav_frames = 0;
    wav_header(wav, 0);
}
void sd_wav_write(const sd_s16 *pcm, int frames) {
    if (!wav || !pcm || frames <= 0) return;
    const size_t written = std::fwrite(pcm, sizeof(sd_s16) * 2,
                                      static_cast<size_t>(frames), wav);
    wav_frames += static_cast<sd_u32>(written);
}
void sd_wav_close(void) {
    if (!wav) return;
    std::fseek(wav, 0, SEEK_SET);
    wav_header(wav, wav_frames);
    std::fclose(wav);
    wav = nullptr;
}
int sd_out_open(void) {
    if (attempted) return device != nullptr;
    attempted = true;
    if (std::getenv("SM64DS_NO_AUDIO")) return 0;
    AAudioStreamBuilder *builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || !builder)
        return 0;
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, 48000);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(builder, audio_callback, nullptr);
    AAudioStreamBuilder_setErrorCallback(builder, audio_error, nullptr);
    const aaudio_result_t opened = AAudioStreamBuilder_openStream(builder, &device);
    AAudioStreamBuilder_delete(builder);
    if (opened != AAUDIO_OK || !device) {
        device = nullptr;
        return 0;
    }
    device_rate = AAudioStream_getSampleRate(device);
    if (device_rate <= 0) device_rate = 48000;
    ratio = double(SD_MIX_RATE) / double(device_rate);
    phase = 0; primed = false;
    if (AAudioStream_requestStart(device) != AAUDIO_OK) {
        AAudioStream_close(device);
        device = nullptr;
        return 0;
    }
    std::fprintf(stderr, "[audio] AAudio %dHz, DS mixer %dHz, volume %d%%\n",
                 device_rate, SD_MIX_RATE, out_volume_pct());
    return 1;
}
void sd_out_close(void) {
    if (device) {
        AAudioStream_requestStop(device);
        AAudioStream_close(device);
        device = nullptr;
    }
    attempted = false;
    producer.store(0, std::memory_order_relaxed);
    consumer.store(0, std::memory_order_relaxed);
    primed = false;
    sd_wav_close();
}
void sd_out_push(void) {
    if (disconnected.exchange(false, std::memory_order_acq_rel))
        sd_out_close();
    if (!attempted) sd_out_open();
    // Preserve the PC mixer's actual 32768Hz clock, including the fractional
    // remainder; the callback only resamples audio, NEVER advances gameplay
    // or touches mixer state from AAudio's high-priority thread.
    frame_remainder += SD_MIX_RATE;
    const int frames = frame_remainder / 60;
    frame_remainder %= 60;
    sd_s16 pcm[MIX_BLOCK * 2];
    sd_mix_render(pcm, frames);
    sd_wav_write(pcm, frames);
    if (device) push_stereo(pcm, frames);
}
extern "C" void sm64ds_android_audio_pause(int paused) {
    if (!device) return;
    if (paused) AAudioStream_requestPause(device);
    else AAudioStream_requestStart(device);
}
extern "C" unsigned sm64ds_android_audio_underruns(void) {
    return underruns.load(std::memory_order_relaxed);
}
