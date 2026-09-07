// Offline regression checks for the actual mixer synthesis. Never opens an
// audio device or window; optional WAV exports can be reviewed separately.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../src/audio.h"

static const int RATE = 22050, MAX_SAMPLES = 17640;
static const short GUARD = 0x5a3d;
static int gChecks, gFailures;

static void Check(int pass, const char* message, int id, int pitch) {
    ++gChecks;
    if (!pass) { ++gFailures; printf("FAIL cue=%d pitch=%d: %s\n", id, pitch, message); }
}

struct WaveStats {
    int peak, clipped, first, last, tailPeak;
    double rms, mean;
};
static WaveStats Measure(const short* samples, int count) {
    WaveStats s = {};
    if (count <= 0) return s;
    double sum = 0, squares = 0;
    for (int i = 0; i < count; ++i) {
        int v = samples[i], magnitude = v < 0 ? -v : v;
        sum += v; squares += (double)v * v;
        if (magnitude > s.peak) s.peak = magnitude;
        if (magnitude >= 32767) ++s.clipped;
        if (i >= count - RATE / 1000 && magnitude > s.tailPeak) s.tailPeak = magnitude;
    }
    s.first = samples[0]; s.last = samples[count - 1];
    s.mean = sum / count; s.rms = sqrt(squares / count);
    return s;
}

static int Untouched(const short* samples, int first, int end) {
    for (int i = first; i < end; ++i) if (samples[i] != GUARD) return 0;
    return 1;
}
static void FillGuard(short* samples, int count) {
    for (int i = 0; i < count; ++i) samples[i] = GUARD;
}

static void CheckAllCues() {
    static short guarded[MAX_SAMPLES + 2], repeat[MAX_SAMPLES], unrelated[MAX_SAMPLES];
    int maxPeak = 0;
    double worstDcRatio = 0;
    for (int id = 0; id < SFX_COUNT; ++id) for (int pitch = 0; pitch < 8; ++pitch) {
        FillGuard(guarded, MAX_SAMPLES + 2);
        int count = RenderSfx(id, pitch, guarded + 1, MAX_SAMPLES);
        Check(count >= 8 && count <= MAX_SAMPLES, "complete cue length within capacity", id, pitch);
        if (count < 8 || count > MAX_SAMPLES) continue;
        Check(guarded[0] == GUARD && Untouched(guarded, count + 1, MAX_SAMPLES + 2), "write bounds", id, pitch);
        // Interleave another cue: local procedural noise must not inherit
        // whatever the player last heard or mutate a global random sequence.
        RenderSfx((id + 7) % SFX_COUNT, (pitch + 3) % 8, unrelated, MAX_SAMPLES);
        int again = RenderSfx(id, pitch, repeat, MAX_SAMPLES);
        Check(again == count && !memcmp(guarded + 1, repeat, count * sizeof(short)), "repeatable PCM after unrelated cue", id, pitch);
        WaveStats s = Measure(guarded + 1, count);
        Check(s.rms > 100, "audible nonempty waveform", id, pitch);
        Check(s.clipped == 0, "no hard-clipped samples", id, pitch);
        Check(fabs(s.mean) <= s.rms * .12 + 1, "DC bias below 12% of signal RMS", id, pitch);
        Check(s.first == 0 && s.last == 0, "silence joins at both endpoints", id, pitch);
        if (s.peak > maxPeak) maxPeak = s.peak;
        double dc = s.rms > 0 ? fabs(s.mean) / s.rms : 0;
        if (dc > worstDcRatio) worstDcRatio = dc;
    }
    printf("All cues: %d waveforms, peak %d/32767, worst DC/RMS %.4f\n", SFX_COUNT * 8, maxPeak, worstDcRatio);
}

static void CheckShortBuffers() {
    static short guarded[MAX_SAMPLES + 34];
    const int capacities[] = {-1, 0, 7, 8, 31, 127, 441, MAX_SAMPLES + 31};
    for (int id = 0; id < SFX_COUNT; ++id) for (int k = 0; k < (int)(sizeof(capacities) / sizeof(capacities[0])); ++k) {
        FillGuard(guarded, MAX_SAMPLES + 34);
        int capacity = capacities[k];
        int count = RenderSfx(id, 0, guarded + 1, capacity);
        int limit = capacity < 8 ? 0 : capacity > MAX_SAMPLES ? MAX_SAMPLES : capacity;
        Check(count >= 0 && count <= limit, "short-buffer length", id, capacity);
        Check(guarded[0] == GUARD && Untouched(guarded, count + 1, MAX_SAMPLES + 34), "short-buffer guards", id, capacity);
        if (count > 0) Check(guarded[count] == 0, "truncated cue settles to silence", id, capacity);
    }
    Check(RenderSfx(0, 0, 0, MAX_SAMPLES) == 0, "null output rejected", -1, 0);
    FillGuard(guarded, MAX_SAMPLES + 34);
    Check(RenderSfx(-1, 0, guarded + 1, MAX_SAMPLES) == 0
        && RenderSfx(SFX_COUNT, 0, guarded + 1, MAX_SAMPLES) == 0
        && Untouched(guarded, 0, MAX_SAMPLES + 34), "invalid IDs preserve output", -1, 0);
    static short normal[MAX_SAMPLES], clamped[MAX_SAMPLES];
    for (int end = 0; end < 2; ++end) {
        int id = SFX_REPAIR, pitch = end ? 7 : 0;
        int a = RenderSfx(id, pitch, normal, MAX_SAMPLES);
        int b = RenderSfx(id, end ? 999 : -999, clamped, MAX_SAMPLES);
        Check(a == b && !memcmp(normal, clamped, a * sizeof(short)), "out-of-range pitch clamps", id, pitch);
    }
}

static int WriteWav(const char* filename, const short* samples, int count) {
    FILE* file = 0;
    if (fopen_s(&file, filename, "wb") || !file) return 0;
    uint32_t bytes = (uint32_t)count * 2, riff = bytes + 36, fmt = 16, rate = RATE, byteRate = RATE * 2;
    uint16_t pcm = 1, channels = 1, align = 2, bits = 16;
    int ok = fwrite("RIFF", 1, 4, file) == 4 && fwrite(&riff, 4, 1, file) == 1
        && fwrite("WAVEfmt ", 1, 8, file) == 8 && fwrite(&fmt, 4, 1, file) == 1
        && fwrite(&pcm, 2, 1, file) == 1 && fwrite(&channels, 2, 1, file) == 1
        && fwrite(&rate, 4, 1, file) == 1 && fwrite(&byteRate, 4, 1, file) == 1
        && fwrite(&align, 2, 1, file) == 1 && fwrite(&bits, 2, 1, file) == 1
        && fwrite("data", 1, 4, file) == 4 && fwrite(&bytes, 4, 1, file) == 1
        && fwrite(samples, sizeof(short), count, file) == (size_t)count;
    if (fclose(file)) ok = 0;
    return ok;
}

static void CheckMaterialCues(int exportWav) {
    static short sounds[4][MAX_SAMPLES];
    const int ids[] = {SFX_UI_FOCUS, SFX_BOSS_ARRIVE, SFX_LOOT_REVEAL, SFX_REPAIR};
    const int lengthsMs[] = {42, 590, 420, 460};
    const char* names[] = {"ui_focus.wav", "boss_arrive.wav", "loot_reveal.wav", "repair.wav"};
    int lengths[4]; double maxCorrelation = 0;
    for (int k = 0; k < 4; ++k) {
        int count = lengths[k] = RenderSfx(ids[k], 0, sounds[k], MAX_SAMPLES);
        Check(count == RATE * lengthsMs[k] / 1000, "authored event duration", ids[k], 0);
        WaveStats s = Measure(sounds[k], count);
        Check(s.tailPeak <= 16, "material decay ends without a residual edge", ids[k], 0);
        printf("%-18s %5d samples %3d ms | peak %5d RMS %7.1f DC %+6.1f tail %d\n",
            names[k], count, lengthsMs[k], s.peak, s.rms, s.mean, s.tailPeak);
        if (exportWav) Check(WriteWav(names[k], sounds[k], count), "write WAV preview", ids[k], 0);
    }
    // Compare overlapping actual PCM after gain normalization, so simply
    // changing volume or truncating a shared cue cannot pass as a new sound.
    for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b) {
        int n = lengths[a] < lengths[b] ? lengths[a] : lengths[b];
        double cross = 0, aa = 0, bb = 0;
        for (int i = 0; i < n; ++i) {
            cross += (double)sounds[a][i] * sounds[b][i];
            aa += (double)sounds[a][i] * sounds[a][i];
            bb += (double)sounds[b][i] * sounds[b][i];
        }
        double correlation = aa > 0 && bb > 0 ? fabs(cross / sqrt(aa * bb)) : 1;
        Check(correlation < .8, "material cues are not scaled copies", ids[a], ids[b]);
        if (correlation > maxCorrelation) maxCorrelation = correlation;
    }
    printf("Material cue maximum pair correlation: %.4f\n", maxCorrelation);
}

int main(int argc, char** argv) {
    CheckAllCues(); CheckShortBuffers();
    CheckMaterialCues(argc > 1 && !strcmp(argv[1], "--export"));
    printf("Audio QA: %d checks, %d failures; no device opened.\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
