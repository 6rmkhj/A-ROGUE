#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include "audio.h"
#include "music.h"

// ---- procedural sound -----------------------------------------------------
// No sound files: every effect is synthesised into memory and mixed into the
// output stream. 22050Hz 16-bit signed, which removes the quantisation hiss the
// old 8-bit/11025Hz path had, and a buffer big enough for the longest cue
// (the old 1800-sample buffer silently truncated anything over 163ms, so the
// 180ms game-over and victory stings were being cut off mid-fade).
#define SFX_RATE 22050
#define SFX_MAX_SAMPLES 17640          // 800ms
#define SFX_NOTES 3

enum SfxWave { WAVE_PULSE = 0, WAVE_TRI, WAVE_NOISE };

struct SfxSpec {
    short hz[SFX_NOTES];      // up to three sequential notes, 0 ends the list
    short ms[SFX_NOTES];
    short bend;               // Hz added across each note (negative = falls)
    unsigned char wave, duty, attackMs, release, volume, noise, cut;  // cut 0 = unfiltered, lower = darker
    unsigned char pulses;     // >1 retriggers the envelope inside the note: clack-clack-clack
};


//                    notes(Hz)          lengths(ms)   bend  wave        duty att rel vol noise cut
static const SfxSpec SFX[SFX_COUNT] = {
    {{230,   0,   0}, { 40,  0,  0},  -25, WAVE_TRI,   50,  3, 56,  60,   6,  90,   1},  // UI_CLICK
    {{185,   0,   0}, { 44,  0,  0},  -18, WAVE_TRI,   50,  3, 52,  62,   0,  96,   1},  // DIE_PICK
    {{135,   0,   0}, { 74,  0,  0},  -16, WAVE_TRI,   50,  4, 44,  76,  18,  62,   1},  // SLOT_SET
    {{200,   0,   0}, { 56,  0,  0},  -45, WAVE_TRI,   50,  3, 50,  58,  10,  86,   1},  // TARGET
    {{140,   0,   0}, {190,  0,  0},  -20, WAVE_TRI,   50,  2, 78, 104,  46,  66,   4},  // READ_START  head seek chatter
    {{110,   0,   0}, { 92,  0,  0},  -22, WAVE_TRI,   50,  6, 44,  62,  16,  54,   1},  // DIE_LOCK    sector settles
    {{180,   0,   0}, { 90,  0,  0},  -70, WAVE_PULSE, 45,  2, 45,  60,  22,   0,   1},  // EXECUTE
    {{300,   0,   0}, {170,  0,  0}, -230, WAVE_NOISE, 50,  3, 30,  84,  74,  78,   1},  // ENEMY_DOWN
    {{560, 700,   0}, { 40, 46,  0},    0, WAVE_TRI,   50,  1, 55,  48,   0,   0,   1},  // REWARD_PICK
    {{523, 659, 784}, { 44, 44, 78},    0, WAVE_TRI,   50,  1, 45,  52,   0,   0,   1},  // REWARD_SET
    {{200,   0,   0}, { 80,  0,  0}, -110, WAVE_NOISE, 50,  2, 40,  92,  86,  64,   1},  // PRUNE       sector wiped
    {{440, 660,   0}, { 40, 70,  0},    0, WAVE_TRI,   50,  1, 45,  50,   0,   0,   1},  // CONFIRM
    {{330, 494, 659}, { 52, 52, 96},    0, WAVE_TRI,   50,  2, 40,  50,   6,   0,   1},  // BOOT
    {{523, 784,1047}, { 90, 90,240},    0, WAVE_TRI,   50,  2, 26,  56,   0,   0,   1},  // VICTORY
    {{220, 165, 110}, {140,140,300},  -30, WAVE_TRI,   50,  3, 22,  58,  30,  60,   1},  // GAMEOVER    drive dies
    {{120,   0,   0}, {130,  0,  0},  -40, WAVE_NOISE, 50,  2, 42,  88,  80,  42,   1},  // PLAYER_HIT  적의 타격이 꽂힌다
    {{200,   0,   0}, {380,  0,  0},  -60, WAVE_NOISE, 50,  3, 16,  92,  90, 150,   1},  // CRASH       화면이 노이즈로 무너진다
    {{300, 150,   0}, { 60, 90,  0},  -40, WAVE_PULSE, 30,  1, 40,  92,  10,  70,   1},  // FX_LOCK       셔터가 닫힌다
    {{160, 320,   0}, {110,140,  0},   90, WAVE_TRI,   50,  2, 46,  78,  24,  80,   3},  // FX_RESTORE    테이프가 되감긴다
    {{240,   0,   0}, {200,  0,  0}, -120, WAVE_NOISE, 50,  1, 34,  88,  88,  46,   4},  // FX_OFFLINE    접촉이 끊긴다
    {{420, 300, 420}, { 50, 50, 50},    0, WAVE_PULSE, 25,  1, 44,  74,   6,   0,   1},  // FX_ROUTE      경로가 바뀐다
    {{ 90,  70,   0}, {180,200,  0},  -30, WAVE_TRI,   50,  4, 30,  96,  34,  40,   1},  // FX_PRESSURE   압력이 한계에 닿는다
    {{700,   0,   0}, { 70,  0,  0}, -300, WAVE_NOISE, 50,  1, 30,  86,  60, 120,   2},  // FX_QUARANTINE 봉인된다
    {{330,   0,   0}, { 66,  0,  0}, -190, WAVE_NOISE, 50,  1, 46,  82,  62,  74,   1},  // HIT_IMPACT
    {{180, 360, 720}, { 55, 55, 80},   95, WAVE_TRI,   50,  3, 40,  64,   8, 100,   1},  // CHARGE: winding energy
    {{ 82,  48,   0}, { 65,120,  0},  -34, WAVE_TRI,   50,  1, 68, 100,  58,  65,   1},  // HEAVY_HIT: crack + low body
    {{260, 520, 780}, { 38, 42, 95},   20, WAVE_TRI,   50,  2, 45,  58,   5, 120,   1},  // SHIELD_RISE
    {{920, 460,   0}, { 32,110,  0},  -45, WAVE_TRI,   50,  1, 70,  72,  20, 150,   1},  // SHIELD_BLOCK: glass ping
    {{440, 660, 880}, { 25, 25, 65},   80, WAVE_PULSE, 25,  1, 60,  56,  18, 110,   1},  // CHAIN_ARC
    // These four cues use authored material layers in RenderMaterialSfx below.
    {{430,   0,   0}, { 42,  0,  0},    0, WAVE_TRI,   50,  2, 90,  14,   0, 100,   1},  // UI_FOCUS: felt detent
    {{ 92,   0,   0}, {590,  0,  0},  -36, WAVE_TRI,   50,  8, 90,  72,   0,  60,   1},  // BOSS_ARRIVE: heavy latch + pressure
    {{784,   0,   0}, {420,  0,  0},    0, WAVE_TRI,   50,  3, 90,  43,   0, 130,   1},  // LOOT_REVEAL: struck glass + small catch
    {{330,   0,   0}, {460,  0,  0},    0, WAVE_TRI,   50, 10, 90,  46,   0, 110,   1}   // REPAIR: soft seal + warm resonance
};

// 2^(n/12) in 1/256ths, for pitching a cue up by whole semitones
static const int SEMITONE[8] = {256, 271, 287, 304, 323, 342, 362, 384};

static int SfxOsc(int wave, int phase, int period, int duty, uint32_t* noiseSeed) {
    if (wave == WAVE_NOISE) {
        *noiseSeed = *noiseSeed * 1664525u + 1013904223u;
        return (int)((*noiseSeed >> 16) & 0xFFFFu) - 32768;
    }
    if (period < 2) period = 2;
    int pos = phase % period;
    if (wave == WAVE_TRI) {
        int half = period / 2;
        int up = pos < half ? pos * 65536 / half : (period - pos) * 65536 / (period - half);
        return up - 32768;
    }
    // asymmetric duty carries a DC bias, so pick high/low levels whose mean is
    // zero and rescale if that pushes the peak past the headroom
    int hi = 26000 * (100 - duty) / 50, lo = -26000 * duty / 50;
    int cap = hi > -lo ? hi : -lo;
    if (cap > 26000) { hi = hi * 26000 / cap; lo = lo * 26000 / cap; }
    return pos * 100 < period * duty ? hi : lo;
}

// A phase-continuous, rounded triangle: bends do not jump between integer
// periods. These event cues are single physical gestures with overlapping
// resonances, so each has its own weight and decay instead of a note ladder.
static int MaterialTone(uint32_t* phase, int hz) {
    *phase += (uint32_t)hz * 65536u / SFX_RATE;
    int p = (int)(*phase & 65535u);
    int tri = p < 32768 ? p * 2 - 32768 : 98304 - p * 2;
    int a = tri < 0 ? -tri : tri;
    return tri * (65536 - a) / 32768;
}

static int MaterialEnvelope(int sample, int startMs, int lengthMs, int attackMs) {
    int at = sample - SFX_RATE * startMs / 1000;
    int length = SFX_RATE * lengthMs / 1000;
    if (at < 0 || at >= length) return 0;
    int attack = SFX_RATE * attackMs / 1000;
    if (attack < 1) attack = 1;
    if (at < attack) return at * 256 / attack;
    int tail = (length - at) * 256 / (length - attack);
    return tail * tail / 256;
}

static int RenderMaterialSfx(int id, int shift, short* out, int capacity) {
    if (id < SFX_UI_FOCUS || id > SFX_REPAIR) return 0;
    int count = SFX_RATE * SFX[id].ms[0] / 1000;
    if (count > capacity) count = capacity;
    uint32_t phase[3] = {}, noise = 0x5f31a129u + (uint32_t)id * 7919u;
    int filtered = 0;
    for (int i = 0; i < count; ++i) {
        noise = noise * 1664525u + 1013904223u;
        int hiss = (int)((noise >> 16) & 65535u) - 32768;
        filtered += (hiss - filtered) * 52 / 256;
        int ms = i * 1000 / SFX_RATE, value;
        if (id == SFX_UI_FOCUS) {
            int body = MaterialTone(&phase[0], 430 * shift / 256);
            value = body * MaterialEnvelope(i, 0, 42, 2) / 256 * 14 / 100
                + filtered * MaterialEnvelope(i, 0, 11, 1) / 256 * 9 / 100;
        } else if (id == SFX_BOSS_ARRIVE) {
            int fall = ms < 80 ? ms : 80;
            int body = MaterialTone(&phase[0], (104 - fall * 48 / 80) * shift / 256);
            int metal = MaterialTone(&phase[1], 173 * shift / 256);
            value = body * MaterialEnvelope(i, 18, 565, 5) / 256 * 58 / 100
                + metal * MaterialEnvelope(i, 20, 210, 2) / 256 * 13 / 100
                + filtered * MaterialEnvelope(i, 0, 116, 14) / 256 * 36 / 100
                + filtered * MaterialEnvelope(i, 146, 46, 1) / 256 * 19 / 100;
        } else if (id == SFX_LOOT_REVEAL) {
            int glass = MaterialTone(&phase[0], 784 * shift / 256);
            int overtone = MaterialTone(&phase[1], 1309 * shift / 256);
            value = glass * MaterialEnvelope(i, 24, 396, 2) / 256 * 28 / 100
                + overtone * MaterialEnvelope(i, 26, 233, 3) / 256 * 11 / 100
                + filtered * MaterialEnvelope(i, 0, 28, 1) / 256 * 18 / 100;
        } else {
            int settle = ms < 200 ? ms : 200;
            int warm = MaterialTone(&phase[0], (286 + settle * 44 / 200) * shift / 256);
            int paired = MaterialTone(&phase[1], 334 * shift / 256);
            value = warm * MaterialEnvelope(i, 0, 460, 48) / 256 * 29 / 100
                + paired * MaterialEnvelope(i, 94, 366, 34) / 256 * 13 / 100
                + filtered * MaterialEnvelope(i, 0, 185, 22) / 256 * 26 / 100
                + filtered * MaterialEnvelope(i, 208, 31, 2) / 256 * 12 / 100;
        }
        if (value > 32767) value = 32767; else if (value < -32767) value = -32767;
        out[i] = (short)value;
    }
    return count;
}

static void FadeSfxTail(short* samples, int count) {
    for (int i = 0; i < 32 && i < count; ++i)
        samples[count - 1 - i] = (short)((int)samples[count - 1 - i] * i / 32);
}

// ---- output mixer ---------------------------------------------------------
// PlaySound only ever plays one thing per process: firing a second cue cuts the
// first one off mid-note, which is what makes rapid clicking sound chewed up.
// Instead keep a waveOut stream running and mix the active cues into it, so
// overlapping sounds actually overlap.
#define MIX_VOICES 8
#define MIX_BUFFERS 4
#define MIX_FRAMES 441            // 20ms at 22050Hz, so a fresh cue is audible fast

struct MixVoice { short data[SFX_MAX_SAMPLES]; int length, position; };
static MixVoice gVoice[MIX_VOICES];
static int gVoiceAge[MIX_VOICES], gVoiceClock;
static HWAVEOUT gWaveOut;
static HWND gAudioWindow;     // 펌프 타이머를 다는 창. 오디오가 창 전역에 의존하지 않게 여기 보관한다.
static WAVEHDR gWaveHdr[MIX_BUFFERS];
static short gMixBuf[MIX_BUFFERS][MIX_FRAMES];
static int gAudioClosing;
// 믹싱은 전용 스레드에서 돈다. 예전에는 UI 스레드의 10ms 타이머가 펌프를 돌렸는데,
// WM_TIMER는 큐가 빌 때만 오고 페인트 한 번이 수십 ms를 먹으면 80ms짜리 큐가
// 말라 소리가 끊겼다. 지금은 장치가 버퍼를 비울 때마다 이벤트로 깨어나 채운다.
// UI 스레드는 효과음을 넣거나 음악 설정을 바꿀 때만 잠깐 잠근다.
static CRITICAL_SECTION gAudioLock;
static HANDLE gAudioEvent, gAudioThread;
static volatile LONG gAudioQuit;
static volatile LONG gUnderruns;

struct AudioGuard {
    AudioGuard() { EnterCriticalSection(&gAudioLock); }
    ~AudioGuard() { LeaveCriticalSection(&gAudioLock); }
};
static MusicState gMusic;
static int gMusicEnabled = 1;
static int gDuckFrames;

// 기본값을 최대치로 두지 않는다. 효과음이 스무 종 넘게 겹쳐 울리는 게임이라
// 100%는 실제로 시끄럽다. 필요하면 설정에서 올린다.
static int gAudioVolume = 50;
static int gMusicVolume = 100;
static int gSfxVolume = 100;

void SetAudioVolume(int percent) {
    if (percent < 0) percent = 0; else if (percent > 100) percent = 100;
    gAudioVolume = percent;   // int 한 칸이라 잠그지 않아도 찢어지지 않는다
}

int AudioVolume() { return gAudioVolume; }

void AudioSetMusicVolume(int percent) {
    if (percent < 0) percent = 0; else if (percent > 100) percent = 100;
    gMusicVolume = percent;
}
int AudioMusicVolume() { return gMusicVolume; }

void AudioSetSfxVolume(int percent) {
    if (percent < 0) percent = 0; else if (percent > 100) percent = 100;
    gSfxVolume = percent;
}
int AudioSfxVolume() { return gSfxVolume; }

void AudioSetScene(int scene) { AudioGuard g; MusicSetScene(&gMusic, scene); }
void AudioSetDrive(int drive) { AudioGuard g; MusicSetDrive(&gMusic, drive); }
void AudioSetIntensity(int intensity) { AudioGuard g; MusicSetIntensity(&gMusic, intensity); }
void AudioSetCritical(int critical) { AudioGuard g; MusicSetCritical(&gMusic, critical); }
void AudioSetMusicEnabled(int enabled) { AudioGuard g; gMusicEnabled = enabled != 0; MusicSetEnabled(&gMusic, gMusicEnabled); }
int AudioMusicEnabled() { return gMusicEnabled; }
void AudioSetEnding(int ending) { AudioGuard g; MusicSetEnding(&gMusic, ending); }
int AudioUnderruns() { return (int)gUnderruns; }

static void MixFrames(short* out, int frames) {
    int32_t accumulator[MIX_FRAMES] = {};
    int32_t music[MIX_FRAMES] = {};
    MusicRender(&gMusic, music, frames);
    for (int v = 0; v < MIX_VOICES; ++v) {
        MixVoice* mv = &gVoice[v];
        if (mv->length <= 0) continue;
        int n = mv->length - mv->position;
        if (n > frames) n = frames;
        for (int i = 0; i < n; ++i) {
            accumulator[i] += mv->data[mv->position + i];
        }
        mv->position += n;
        if (mv->position >= mv->length) mv->length = 0;
    }
    for (int i = 0; i < frames; ++i) {
        int musicDuck = gDuckFrames > 0 ? 65 : 100;
        int32_t sfx = accumulator[i] * gSfxVolume / 100;
        int32_t bgm = music[i] * 26 / 100 * musicDuck / 100 * gMusicVolume / 100;
        int s = (sfx + bgm) * gAudioVolume / 100;
        if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
        out[i] = (short)s;
        if (gDuckFrames > 0) --gDuckFrames;
    }
}

// 장치가 비운 버퍼를 전부 다시 채워 넣는다. 오디오 스레드에서만 부른다.
static void Pump() {
    int done = 0;
    for (int i = 0; i < MIX_BUFFERS; ++i) if (gWaveHdr[i].dwFlags & WHDR_DONE) ++done;
    if (done == MIX_BUFFERS) InterlockedIncrement(&gUnderruns);   // 큐가 완전히 말랐었다
    for (int i = 0; i < MIX_BUFFERS; ++i) {
        if (!(gWaveHdr[i].dwFlags & WHDR_DONE)) continue;
        { AudioGuard g; MixFrames(gMixBuf[i], MIX_FRAMES); }
        gWaveHdr[i].dwFlags &= ~WHDR_DONE;
        waveOutWrite(gWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
    }
}

static DWORD WINAPI AudioThread(void*) {
    // 20ms 버퍼 넷이 전부다. 페인트가 밀려도 이 스레드는 밀리지 않아야 한다.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (!gAudioQuit) {
        WaitForSingleObject(gAudioEvent, 20);   // 버퍼 완료 이벤트. 놓쳐도 20ms 뒤 깨어난다
        if (gAudioQuit) break;
        Pump();
    }
    return 0;
}

void AudioOpen(HWND window) {
    InitializeCriticalSection(&gAudioLock);
    MusicInit(&gMusic);
    WAVEFORMATEX format;
    ZeroMemory(&format, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 1;
    format.nSamplesPerSec = SFX_RATE; format.wBitsPerSample = 16;
    format.nBlockAlign = 2; format.nAvgBytesPerSec = SFX_RATE * 2;
    gAudioEvent = CreateEventW(0, FALSE, FALSE, 0);
    if (!gAudioEvent || waveOutOpen(&gWaveOut, WAVE_MAPPER, &format, (DWORD_PTR)gAudioEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        gWaveOut = 0; return;               // no audio device: stay silent, keep playing
    }
    for (int i = 0; i < MIX_BUFFERS; ++i) {
        ZeroMemory(&gWaveHdr[i], sizeof(WAVEHDR));
        gWaveHdr[i].lpData = (LPSTR)gMixBuf[i];
        gWaveHdr[i].dwBufferLength = MIX_FRAMES * 2;
        waveOutPrepareHeader(gWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
        MixFrames(gMixBuf[i], MIX_FRAMES);
        waveOutWrite(gWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
    }
    gAudioWindow = window;
    gAudioThread = CreateThread(0, 0, AudioThread, 0, 0, 0);
}

void AudioClose() {
    if (!gWaveOut) return;
    gAudioClosing = 1;
    InterlockedExchange(&gAudioQuit, 1);
    if (gAudioThread) { SetEvent(gAudioEvent); WaitForSingleObject(gAudioThread, 500); CloseHandle(gAudioThread); gAudioThread = 0; }
    waveOutReset(gWaveOut);
    for (int i = 0; i < MIX_BUFFERS; ++i) waveOutUnprepareHeader(gWaveOut, &gWaveHdr[i], sizeof(WAVEHDR));
    waveOutClose(gWaveOut);
    gWaveOut = 0;
}

int RenderSfx(int id, int semitones, short* out, int capacity) {
    if (id < 0 || id >= SFX_COUNT || !out || capacity < 8) return 0;
    if (capacity > SFX_MAX_SAMPLES) capacity = SFX_MAX_SAMPLES;
    const SfxSpec* s = &SFX[id];
    if (semitones < 0) semitones = 0; else if (semitones > 7) semitones = 7;
    int shift = SEMITONE[semitones];
    int material = RenderMaterialSfx(id, shift, out, capacity);
    if (material) { FadeSfxTail(out, material); return material; }
    uint32_t noiseSeed = 0x13579BDFu + (uint32_t)id * 7919u;

    int total = 0;
    for (int n = 0; n < SFX_NOTES && s->hz[n] > 0; ++n) {
        int hz = (int)s->hz[n] * shift / 256;
        int count = SFX_RATE * s->ms[n] / 1000;
        if (total + count > capacity) count = capacity - total;
        if (count <= 0) break;
        int pulses = s->pulses < 1 ? 1 : s->pulses, span = count;
        if (pulses > 1) { span = count / pulses; if (span < 8) { span = count; pulses = 1; } }
        int attack = SFX_RATE * s->attackMs / 1000;
        if (attack > span / 2) attack = span / 2;
        if (attack < 1) attack = 1;
        int phase = 0, lp = 0;
        for (int i = 0; i < count; ++i) {
            int nowHz = hz + (int)s->bend * i / count; if (nowHz < 20) nowHz = 20;
            // Integrate frequency instead of taking a growing sample index
            // modulo a changing period. The latter makes a downward bend
            // stall or reverse, leaving a large DC offset in heavy impacts.
            int value = SfxOsc(s->wave, phase, 16384, s->duty, &noiseSeed);
            if (s->noise > 0 && s->wave != WAVE_NOISE) {
                int hiss = SfxOsc(WAVE_NOISE, 0, 0, 0, &noiseSeed);
                value = (value * (100 - s->noise) + hiss * s->noise) / 100;
            }
            // one-pole low-pass: white noise and narrow pulses are all high
            // harmonics, which is what makes a cue read as piercing
            if (s->cut > 0) { lp += (value - lp) * s->cut / 256; value = lp; }
            int k = pulses > 1 ? i % span : i, env;
            if (k < attack) env = k * 256 / attack;
            else {
                int t = (k - attack) * 256 / (span - attack + 1);
                env = 256 - t * s->release / 100;
                if (env < 0) env = 0;
                env = env * (256 - t / 4) / 256;
            }
            // a pulsed cue still has to fade away overall, not just per clack
            if (pulses > 1) env = env * (256 - i * 256 / count) / 256;
            int o = value * env / 256 * s->volume / 100;
            if (o > 32767) o = 32767; else if (o < -32767) o = -32767;
            out[total + i] = (short)o;
            phase = (phase + nowHz * 16384 / SFX_RATE) & 16383;
        }
        total += count;
    }
    if (total < 8) return 0;
    FadeSfxTail(out, total);
    return total;
}

static int SfxDuckDuration(int id) {
    if (id == SFX_BOSS_ARRIVE) return 260;
    if (id == SFX_LOOT_REVEAL || id == SFX_REPAIR) return 90;
    if (id == SFX_EXECUTE || id == SFX_ENEMY_DOWN || id == SFX_VICTORY || id == SFX_GAMEOVER
        || id == SFX_PLAYER_HIT || id == SFX_CRASH || (id >= SFX_FX_LOCK && id <= SFX_CHAIN_ARC)) return 160;
    return 0; // A focus detent never pumps the music underneath the cursor.
}

void PlaySfxPitched(int id, int semitones) {
    if (id < 0 || id >= SFX_COUNT || !gWaveOut) return;
    AudioGuard guard;
    int duck = SFX_RATE * SfxDuckDuration(id) / 1000;
    if (duck > gDuckFrames) gDuckFrames = duck;
    int slot = -1;
    for (int v = 0; v < MIX_VOICES; ++v) if (gVoice[v].length <= 0) { slot = v; break; }
    if (slot < 0) {
        // Hover feedback is expendable when the mix is full. A cursor must
        // never cut off a strike, boss arrival, or another deliberate action.
        if (id == SFX_UI_FOCUS) return;
        slot = 0;
        for (int v = 1; v < MIX_VOICES; ++v) if (gVoiceAge[v] < gVoiceAge[slot]) slot = v;
    }
    MixVoice* voice = &gVoice[slot];
    voice->length = 0;
    gVoiceAge[slot] = ++gVoiceClock;
    int total = RenderSfx(id, semitones, voice->data, SFX_MAX_SAMPLES);
    voice->position = 0;
    voice->length = total; // Publish only after the complete cue is available.
}

void PlaySfx(int id) { PlaySfxPitched(id, 0); }
