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
// 1200ms. 예전 한도는 800ms였는데, 연출을 하나로 묶는 상승은 그보다 길어야
// 한다 - 짧게 끊어 이어 붙이면 이은 자리가 들린다. 목소리 여덟 개의 버퍼가
// 커지지만 전부 BSS라 실행 파일 크기와는 무관하다.
#define SFX_MAX_SAMPLES 26460
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
    {{330,   0,   0}, {460,  0,  0},    0, WAVE_TRI,   50, 10, 90,  46,   0, 110,   1},  // REPAIR: soft seal + warm resonance
    // 삽입 연출 전용. 전부 아래 RenderMaterialSfx의 층으로 합성되므로 여기서
    // 뜻이 있는 칸은 길이(ms[0])뿐이다. 나머지는 표를 읽을 때 어떤 물건의
    // 소리인지 알아보라고 대표 주파수만 적어 둔다.
    {{ 168,  0,   0}, {520,  0,  0},    0, WAVE_TRI,   50,  1, 90,  58,    0,  70,   1},  // BOOT_TEAR:    화면이 과전압으로 찢긴다
    {{  90,  0,   0}, {780,  0,  0},    0, WAVE_TRI,   50,  6, 90,  40,    0,  90,   1},  // BOOT_VORTEX:  감겨 들어가는 소용돌이
    {{1180,  0,   0}, {620,  0,  0},    0, WAVE_TRI,   50,  1, 90,  46,    0, 150,   1},  // BOOT_FORGE:   디스크 한 장이 벼려진다
    {{ 300,  0,   0}, {300,  0,  0},    0, WAVE_TRI,   50,  4, 90,  34,    0, 120,   1},  // BOOT_FLIP:    공중에서 한 바퀴
    {{ 240,  0,   0}, {280,  0,  0},    0, WAVE_TRI,   50,  2, 90,  40,    0, 110,   1},  // BOOT_SLIDE:   플라스틱이 슬롯을 긁는다
    {{  78,  0,   0}, {440,  0,  0},    0, WAVE_TRI,   50,  1, 90,  56,    0,  60,   1},  // BOOT_LATCH:   걸쇠가 물린다
    {{  44,  0,   0}, {700,  0,  0},    0, WAVE_TRI,   50, 40, 90,  48,    0,  80,   1},  // BOOT_MOTOR:   스핀들이 돌기 시작한다 (판독 구간을 받치는 유일한 지속음이라 조금 세다)
    {{ 620,  0,   0}, {300,  0,  0},    0, WAVE_TRI,   50,  1, 90,  40,    0, 130,   1},  // BOOT_SEEK:    헤드가 트랙을 옮긴다
    {{  96,  0,   0}, {560,  0,  0},    0, WAVE_TRI,   50,  1, 90,  44,    0, 100,   1},  // BOOT_POWER:   브라운관이 켜진다
    {{ 120,  0,   0}, {720,  0,  0},    0, WAVE_TRI,   50,  3, 90,  54,    0,  90,   1},  // BOOT_SWALLOW: 화면 속으로 삼켜진다
    {{ 110,  0,   0}, {1200, 0,  0},    0, WAVE_TRI,   50,  4, 90,  62,    0, 120,   1},  // BOOT_RISER:   A단조가 가속하며 오른다
    {{ 440,  0,   0}, {700,  0,  0},    0, WAVE_TRI,   50,  1, 90,  72,    0, 150,   1},  // BOOT_STINGER: A단조 화음이 선다
    {{ 220,  0,   0}, {420,  0,  0},    0, WAVE_TRI,   50,  1, 90,  58,    0, 140,   1},  // BOOT_PULSE:   한 음 (0/3/7 = A3·C4·E4)
    {{  55,  0,   0}, {620,  0,  0},    0, WAVE_TRI,   50,  2, 90,  72,    0,  70,   1},  // BOOT_TOLL:    낮은 A
    {{ 110,  0,   0}, {1100, 0,  0},    0, WAVE_TRI,   50,  2, 90,  78,    0, 110,   1},  // BOOT_RESOLVE: A단조로 닫는다
    {{ 520,  0,   0}, {460,  0,  0},    0, WAVE_TRI,   50,  4, 90,  58,    0, 100,   1},  // BOOT_REVEAL:  방이 열린다
    {{ 760,  0,   0}, {620,  0,  0},    0, WAVE_TRI,   50,  1, 90,  34,    0, 140,   1},  // BOOT_CHATTER: 데이터 채터
    {{ 900,  0,   0}, {540,  0,  0},    0, WAVE_TRI,   50,  1, 90,  46,    0, 150,   1},  // BOOT_LOCK:    18칸이 잠긴다
    // 볼륨 마운트. 같은 방식으로 층을 쌓으므로 여기서도 뜻이 있는 칸은 길이뿐이다.
    {{  38,  0,   0}, {1200, 0,  0},    0, WAVE_TRI,   50, 90, 90,  52,    0,  70,   1},  // MOUNT_SPIN:   스핀들이 회전수에 오른다
    {{ 880,  0,   0}, {340,  0,  0},    0, WAVE_TRI,   50,  1, 90,  50,    0, 150,   1},  // MOUNT_LAND:   헤드가 판에 앉는다
    {{ 196,  0,   0}, {420,  0,  0},    0, WAVE_TRI,   50,  1, 90,  54,    0, 110,   1},  // MOUNT_ROT:    손상 섹터를 밟는다
    {{ 659,  0,   0}, {760,  0,  0},    0, WAVE_TRI,   50,  1, 90,  62,    0, 140,   1}   // MOUNT_LAW:    법칙이 각인된다
};

// 2^(n/12) in 1/256ths, for pitching a cue up by whole semitones
static const int SEMITONE[8] = {256, 271, 287, 304, 323, 342, 362, 384};

// A단조. 반음(A2=110Hz 기준)을 Hz로. music.cpp의 NoteHz와 같은 비율표라
// 연출이 내는 음과 타이틀 곡이 같은 음정에 선다.
static int BootNoteHz(int semitone) {
    static const int ratio[12] = {1024,1085,1149,1218,1290,1367,1448,1534,1625,1722,1825,1933};
    int octave = semitone / 12, note = semitone % 12;
    if (note < 0) { note += 12; --octave; }
    int hz = 110 * ratio[note] / 1024;
    while (octave > 0) { hz *= 2; --octave; }
    while (octave < 0) { hz /= 2; ++octave; }
    return hz < 20 ? 20 : hz;
}

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

// 아주 짧은 반사 하나. 이 게임의 소리는 전부 건조한 점음원이라, 카메라가 책상과
// 방을 열어 보여 주는 순간에도 소리는 귀에 붙어 있었다. 큰 사건에만 23ms와 47ms
// 반사를 얹으면 같은 파형이 "어딘가에서 난 소리"가 된다.
// 뒤에서부터 읽으므로 반사끼리 되먹임하지 않고, 마지막 80ms는 이득을 0으로
// 거둬 꼬리를 되살리지 않는다 (되살리면 끝이 잘린 것으로 들린다).
static void AddRoomTap(short* out, int count, int delayMs, int gain) {
    int d = SFX_RATE * delayMs / 1000;
    if (d < 1 || d >= count) return;
    int taper = SFX_RATE * 80 / 1000;
    for (int i = count - 1; i >= d; --i) {
        int left = count - 1 - i;
        int g = left < taper ? gain * left / taper : gain;
        if (g <= 0) continue;
        int v = out[i] + out[i - d] * g / 100;
        out[i] = (short)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
    }
}

// 방을 주는 소리. 큰 사건과 음만이고, 잔딸깍·마찰처럼 가까이서 나야 하는 것은 뺀다.
static int BootWantsRoom(int id) {
    return id == SFX_BOOT_FORGE || id == SFX_BOOT_LATCH || id == SFX_BOOT_POWER
        || id == SFX_BOOT_SWALLOW || id == SFX_BOOT_STINGER || id == SFX_BOOT_TOLL
        || id == SFX_BOOT_RESOLVE || id == SFX_BOOT_REVEAL
        // 마운트는 기계 안을 들여다보는 장면이다. 판에 닿는 소리와 각인만
        // 방을 주고, 스핀들 지속음과 손상 섹터의 갈림은 귀에 붙여 둔다.
        || id == SFX_MOUNT_LAND || id == SFX_MOUNT_LAW;
}

static int RenderMaterialSfx(int id, int shift, short* out, int capacity) {
    if (id < SFX_UI_FOCUS || id > SFX_MOUNT_LAW) return 0;
    int count = SFX_RATE * SFX[id].ms[0] / 1000;
    if (count > capacity) count = capacity;
    uint32_t phase[4] = {}, noise = 0x5f31a129u + (uint32_t)id * 7919u;
    // 잡음 하나에서 세 가지 재료를 뽑는다. filtered는 마찰·전이음, rumble은 훨씬
    // 느린 극이라 베어링 그릇거림이 되고, 둘의 차(band)는 중역만 남은 바람 소리다.
    // 대역을 나누지 않으면 무엇을 넣어도 같은 '치익' 하나로 들린다.
    int filtered = 0, rumble = 0;
    for (int i = 0; i < count; ++i) {
        noise = noise * 1664525u + 1013904223u;
        int hiss = (int)((noise >> 16) & 65535u) - 32768;
        filtered += (hiss - filtered) * 52 / 256;
        rumble += (hiss - rumble) * 12 / 256;
        int band = filtered - rumble;
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
        } else if (id == SFX_REPAIR) {
            int settle = ms < 200 ? ms : 200;
            int warm = MaterialTone(&phase[0], (286 + settle * 44 / 200) * shift / 256);
            int paired = MaterialTone(&phase[1], 334 * shift / 256);
            value = warm * MaterialEnvelope(i, 0, 460, 48) / 256 * 29 / 100
                + paired * MaterialEnvelope(i, 94, 366, 34) / 256 * 13 / 100
                + filtered * MaterialEnvelope(i, 0, 185, 22) / 256 * 26 / 100
                + filtered * MaterialEnvelope(i, 208, 31, 2) / 256 * 12 / 100;
        } else if (id == SFX_BOOT_TEAR) {
            // 과전압. 저역이 아래로 떨어지면서 중역이 함께 찢어진다. 맨 앞의
            // 1.5kHz 한 조각이 "지금 갈라졌다"를 알리는 모서리다.
            int fall = ms < 300 ? ms : 300;
            int sub = MaterialTone(&phase[0], (176 - fall * 132 / 300) * shift / 256);
            int rasp = MaterialTone(&phase[1], (612 - fall) * shift / 256);
            int shear = MaterialTone(&phase[2], (1480 + fall * 8) * shift / 256);
            value = sub * MaterialEnvelope(i, 0, 520, 6) / 256 * 42 / 100
                + rasp * MaterialEnvelope(i, 4, 210, 3) / 256 * 15 / 100
                + shear * MaterialEnvelope(i, 0, 64, 1) / 256 * 10 / 100
                + band * MaterialEnvelope(i, 0, 330, 2) / 256 * 26 / 100;
        } else if (id == SFX_BOOT_VORTEX) {
            // 감겨 들어가는 소리. 높이와 떨림이 같이 올라간다 - 회전이 빨라질수록
            // 진동수도 빨라지는 물건이라야 화면이 실제로 감기는 것으로 들린다.
            int whirl = MaterialTone(&phase[0], (88 + ms * 430 / 780) * shift / 256);
            int upper = MaterialTone(&phase[1], (264 + ms * 1290 / 780) * shift / 256);
            int lfo = MaterialTone(&phase[2], 12 + ms * 30 / 780);
            int trem = 176 + lfo / 410;
            value = whirl * MaterialEnvelope(i, 0, 780, 60) / 256 * 34 / 100
                + upper * MaterialEnvelope(i, 40, 740, 180) / 256 * 17 / 100 * trem / 256
                + band * MaterialEnvelope(i, 0, 780, 120) / 256 * 26 / 100 * trem / 256;
        } else if (id == SFX_BOOT_FORGE) {
            // 디스크 한 장이 벼려지는 순간. 금속을 때린 배음 셋이 각기 다른 속도로
            // 사라지고 그 아래에 몸통이 남는다.
            int strike = MaterialTone(&phase[0], 1180 * shift / 256);
            int ring = MaterialTone(&phase[1], 1772 * shift / 256);
            int shimmer = MaterialTone(&phase[2], 2656 * shift / 256);
            int drop = ms < 120 ? ms : 120;
            int thud = MaterialTone(&phase[3], (150 - drop * 78 / 120) * shift / 256);
            value = strike * MaterialEnvelope(i, 0, 600, 1) / 256 * 26 / 100
                + ring * MaterialEnvelope(i, 2, 420, 1) / 256 * 15 / 100
                + shimmer * MaterialEnvelope(i, 4, 230, 2) / 256 * 9 / 100
                + thud * MaterialEnvelope(i, 0, 620, 3) / 256 * 28 / 100
                + filtered * MaterialEnvelope(i, 0, 40, 1) / 256 * 16 / 100;
        } else if (id == SFX_BOOT_FLIP) {
            // 공중에서 도는 판이 가르는 공기. 가운데에서 가장 세고 양끝이 비어 있다.
            int swell = ms < 150 ? ms * 256 / 150 : (300 - ms) * 256 / 150;
            if (swell < 0) swell = 0;
            int air = MaterialTone(&phase[0], (300 + ms * 2) * shift / 256);
            value = band * MaterialEnvelope(i, 0, 300, 90) / 256 * 52 / 100 * swell / 256
                + air * MaterialEnvelope(i, 0, 296, 110) / 256 * 20 / 100;
        } else if (id == SFX_BOOT_SLIDE) {
            // 플라스틱이 금속 슬롯을 긁으며 들어간다. 마찰이 잦아드는 동안 몸통
            // 주파수는 조금 올라간다 - 판이 안으로 물리며 조여지는 소리다.
            int grip = ms < 200 ? 256 - ms * 96 / 200 : 160;
            int body = MaterialTone(&phase[0], (232 + ms * 40 / 280) * shift / 256);
            int squeak = MaterialTone(&phase[1], (1420 - ms * 300 / 280) * shift / 256);
            value = band * MaterialEnvelope(i, 0, 250, 14) / 256 * 46 / 100 * grip / 256
                + body * MaterialEnvelope(i, 0, 280, 10) / 256 * 30 / 100
                + squeak * MaterialEnvelope(i, 60, 150, 24) / 256 * 12 / 100;
        } else if (id == SFX_BOOT_LATCH) {
            // 걸쇠. 아주 짧은 금속 모서리 둘과, 한참 남는 무거운 몸통.
            int drop = ms < 70 ? ms : 70;
            int body = MaterialTone(&phase[0], (132 - drop * 62 / 70) * shift / 256);
            int metal = MaterialTone(&phase[1], 538 * shift / 256);
            int tick = MaterialTone(&phase[2], 1340 * shift / 256);
            value = body * MaterialEnvelope(i, 0, 440, 3) / 256 * 52 / 100
                + metal * MaterialEnvelope(i, 2, 165, 1) / 256 * 18 / 100
                + tick * MaterialEnvelope(i, 0, 34, 1) / 256 * 11 / 100
                + filtered * MaterialEnvelope(i, 0, 52, 1) / 256 * 17 / 100;
        } else if (id == SFX_BOOT_MOTOR) {
            // 스핀들이 회전수에 오른다. 기본음·배음·고역 휘파람이 같은 비율로
            // 올라가고, 느린 잡음이 베어링 그릇거림을 깐다.
            int hum = MaterialTone(&phase[0], (42 + ms * 54 / 700) * shift / 256);
            int harm = MaterialTone(&phase[1], (126 + ms * 162 / 700) * shift / 256);
            int whine = MaterialTone(&phase[2], (420 + ms * 640 / 700) * shift / 256);
            value = hum * MaterialEnvelope(i, 0, 700, 150) / 256 * 40 / 100
                + harm * MaterialEnvelope(i, 30, 670, 260) / 256 * 22 / 100
                + whine * MaterialEnvelope(i, 120, 580, 340) / 256 * 10 / 100
                + rumble * MaterialEnvelope(i, 0, 700, 90) / 256 * 24 / 100;
        } else if (id == SFX_BOOT_SEEK) {
            // 헤드가 트랙을 넷 옮긴다. 하나하나가 같은 재료의 짧은 딸깍이고
            // 마지막 하나만 길게 남아 멈춘 자리를 알린다.
            int env = MaterialEnvelope(i, 0, 46, 1) + MaterialEnvelope(i, 74, 44, 1)
                + MaterialEnvelope(i, 150, 42, 1) + MaterialEnvelope(i, 224, 76, 1);
            int rasp = MaterialTone(&phase[0], 624 * shift / 256);
            int edge = MaterialTone(&phase[1], 1560 * shift / 256);
            value = rasp * env / 256 * 30 / 100 + edge * env / 256 * 14 / 100
                + filtered * env / 256 * 40 / 100;
        } else if (id == SFX_BOOT_POWER) {
            // 브라운관 점등. 전원 퍽 소리 뒤에 화면이 부풀어 오르고, 그 위에
            // 플라이백 휘파람이 얇게 남는다 - 켜진 브라운관은 계속 운다.
            int thump = ms < 90 ? ms : 90;
            int thunk = MaterialTone(&phase[0], (118 - thump * 56 / 90) * shift / 256);
            int lift = ms < 300 ? ms : 300;
            int bloom = MaterialTone(&phase[1], (196 + lift * 120 / 300) * shift / 256);
            int flyback = MaterialTone(&phase[2], 2456 * shift / 256);
            value = thunk * MaterialEnvelope(i, 0, 300, 2) / 256 * 40 / 100
                + bloom * MaterialEnvelope(i, 24, 536, 90) / 256 * 26 / 100
                + flyback * MaterialEnvelope(i, 40, 520, 200) / 256 * 9 / 100
                + filtered * MaterialEnvelope(i, 0, 110, 1) / 256 * 22 / 100;
        } else if (id == SFX_BOOT_RISER) {
            // 이 연출을 하나의 악절로 묶는 줄. A단조 아르페지오(A·C·E)가 옥타브를
            // 넘어가며 오르고, 음 하나의 길이가 점점 짧아져 가속한다. 끝은 벼림의
            // 화음과 같은 자리(A5)라 상승이 그 화음으로 빨려 들어가 멈춘다.
            // A단조를 두 옥타브 반 올라간다. 22(G4)와 34(G5)는 다음 A로 끌어올리는
            // 이끔음이다. 마지막 음 A5(880Hz)는 곧 이어지는 화음의 근음보다 한
            // 옥타브 위라, 상승이 그 화음 위에 얹히며 멈춘다. 더 위로 올리면
            // 3kHz대가 되어 긴장이 아니라 귀만 아프다.
            static const int STEP[12] = {0,3,7,12,15,19,22,24,27,31,34,36};
            int onset = 0, index = 11, local = 0, span = 100;
            for (int n = 0; n < 12; ++n) {
                int width = n < 11 ? 145 - n * 9 : 1200 - onset;   // 합이 정확히 1200ms
                if (ms < onset + width) { index = n; local = ms - onset; span = width; break; }
                onset += width;
            }
            int noteHz = BootNoteHz(STEP[index]);
            int pluck = MaterialTone(&phase[0], noteHz * shift / 256);
            int upper = MaterialTone(&phase[1], noteHz * 2 * shift / 256);
            // 음 하나의 봉투. 짧은 어택에 제곱 감쇠라 계단이 아니라 튕김이 된다.
            if (span < 4) span = 4;
            int env = local < 0 || local >= span ? 0
                : local < 3 ? local * 256 / 3
                : ((span - local) * 256 / span) * ((span - local) * 256 / span) / 256;
            // 전체를 받치는 저역. 아르페지오가 가벼우므로 바닥이 같이 올라야 한다.
            int bed = MaterialTone(&phase[2], (55 + ms * 55 / 1200) * shift / 256);
            int swell = ms * 256 / 1200;
            // 위로 갈수록 세진다. 같은 크기로 두면 음이 짧아지는 만큼 구간의
            // 평균이 오히려 내려가, 올라가는 줄인데 조용해지는 것으로 들린다
            // (실제로 재 보니 1000ms 부근이 이 연출에서 가장 조용했다).
            int lift = 190 + index * 14;
            value = pluck * env / 256 * 30 / 100 * lift / 256
                + upper * env / 256 * 11 / 100 * lift / 256
                + bed * MaterialEnvelope(i, 0, 1200, 400) / 256 * 30 / 100 * (160 + swell * 96 / 256) / 256
                + band * MaterialEnvelope(i, 0, 1200, 700) / 256 * 18 / 100 * swell / 256;
        } else if (id == SFX_BOOT_STINGER) {
            // 벼림. A단조 삼화음이 한 번에 서고 오래 남는다. 기계 타격(BOOT_FORGE)이
            // 위에 얹히므로 이쪽은 음정만 또렷하면 된다.
            int a4 = MaterialTone(&phase[0], BootNoteHz(24) * shift / 256);   // A4 440
            int c5 = MaterialTone(&phase[1], BootNoteHz(27) * shift / 256);   // C5 523
            int e5 = MaterialTone(&phase[2], BootNoteHz(31) * shift / 256);   // E5 659
            int a2 = MaterialTone(&phase[3], BootNoteHz(0) * shift / 256);    // A2 110 몸통
            value = a4 * MaterialEnvelope(i, 0, 700, 2) / 256 * 24 / 100
                + c5 * MaterialEnvelope(i, 4, 640, 2) / 256 * 17 / 100
                + e5 * MaterialEnvelope(i, 8, 590, 2) / 256 * 14 / 100
                + a2 * MaterialEnvelope(i, 0, 700, 3) / 256 * 26 / 100
                + filtered * MaterialEnvelope(i, 0, 34, 1) / 256 * 14 / 100;
        } else if (id == SFX_BOOT_PULSE) {
            // 한 음. 피치 0·3·7로 불러 A3·C4·E4가 되고, 디스크가 내려오는 동안
            // 그 순서로 울려 하강이 음정으로도 내려간다.
            int base = 220 * shift / 256;
            int body = MaterialTone(&phase[0], base);
            int oct = MaterialTone(&phase[1], base * 2);
            value = body * MaterialEnvelope(i, 0, 420, 2) / 256 * 34 / 100
                + oct * MaterialEnvelope(i, 2, 300, 2) / 256 * 15 / 100
                + filtered * MaterialEnvelope(i, 0, 26, 1) / 256 * 16 / 100;
        } else if (id == SFX_BOOT_TOLL) {
            // 낮은 A. 걸쇠가 물리는 자리에서 근음을 한 번 눌러 준다. 기계 소리에
            // 음정이 없으면 철컥이 아무 조성에도 속하지 않는 잡음으로 남는다.
            // 때린 종이라 근음이 처음 40ms 동안 살짝 위에서 내려앉는다.
            int bend = ms < 40 ? 40 - ms : 0;
            int root = MaterialTone(&phase[0], (BootNoteHz(0) + bend) * shift / 256);   // A2 110
            int sub = MaterialTone(&phase[1], BootNoteHz(-12) * shift / 256);           // A1 55
            int fifth = MaterialTone(&phase[2], BootNoteHz(19) * shift / 256);          // E4 330 배음
            value = root * MaterialEnvelope(i, 0, 620, 2) / 256 * 38 / 100
                + sub * MaterialEnvelope(i, 14, 560, 22) / 256 * 22 / 100
                + fifth * MaterialEnvelope(i, 0, 260, 2) / 256 * 16 / 100
                + filtered * MaterialEnvelope(i, 0, 30, 1) / 256 * 14 / 100;
        } else if (id == SFX_BOOT_RESOLVE) {
            // 닫는 화음. A단조가 네 옥타브로 펼쳐지고, 아래에서부터 차례로 들어와
            // 마지막에 한 덩어리가 된다. 여기서 런이 시작되므로 이 소리가 곧
            // 타이틀 곡에서 게임 곡으로 넘어가는 이음매다.
            int a1 = MaterialTone(&phase[0], BootNoteHz(-12) * shift / 256);
            int a2 = MaterialTone(&phase[1], BootNoteHz(0) * shift / 256);
            int c4 = MaterialTone(&phase[2], BootNoteHz(15) * shift / 256);
            int e4 = MaterialTone(&phase[3], BootNoteHz(19) * shift / 256);
            value = a1 * MaterialEnvelope(i, 0, 1100, 8) / 256 * 34 / 100
                + a2 * MaterialEnvelope(i, 40, 1060, 10) / 256 * 22 / 100
                + c4 * MaterialEnvelope(i, 120, 980, 14) / 256 * 15 / 100
                + e4 * MaterialEnvelope(i, 200, 900, 18) / 256 * 13 / 100
                + filtered * MaterialEnvelope(i, 0, 90, 2) / 256 * 12 / 100;
        } else if (id == SFX_BOOT_REVEAL) {
            // 카메라가 물러나며 책상과 방이 한꺼번에 드러나는 400ms. 그림에서는
            // 이 연출에서 공간이 생기는 유일한 순간인데 소리로는 비어 있었다.
            // 상승(RISER)의 반대로 내려가고, 마지막에 근음이 도착해 자리를 잡는다.
            int fall = ms < 320 ? ms : 320;
            int sweep = MaterialTone(&phase[0], (520 - fall * 390 / 320) * shift / 256);
            int land = MaterialTone(&phase[1], BootNoteHz(0) * shift / 256);   // A2
            value = sweep * MaterialEnvelope(i, 0, 380, 10) / 256 * 30 / 100
                + land * MaterialEnvelope(i, 250, 210, 30) / 256 * 26 / 100
                + band * MaterialEnvelope(i, 0, 460, 120) / 256 * 24 / 100;
        } else if (id == SFX_BOOT_CHATTER) {
            // 판을 읽는 동안의 잔딸깍. 간격을 일정하게 두면 기계가 아니라
            // 메트로놈이 되므로 칸마다 길이와 높이를 어긋나게 한다. 아래에는
            // 근음 하나가 계속 깔려 이 구간이 조성 밖으로 나가지 않는다.
            int k = ms / 38, span = 6 + (k * 7) % 9;
            int at = i - SFX_RATE * (k * 38) / 1000, len = SFX_RATE * span / 1000;
            int attack = SFX_RATE / 1000;
            int env = (ms > 560 || at < 0 || at >= len || len < 2) ? 0
                : at < attack ? at * 256 / attack
                : ((len - at) * 256 / len) * ((len - at) * 256 / len) / 256;
            int tick = MaterialTone(&phase[0], (760 + (k * 137) % 420) * shift / 256);
            int hum = MaterialTone(&phase[1], BootNoteHz(0) * shift / 256);
            value = tick * env / 256 * 26 / 100
                + filtered * env / 256 * 30 / 100
                + hum * MaterialEnvelope(i, 0, 620, 120) / 256 * 22 / 100;
        } else if (id == SFX_BOOT_LOCK) {
            // 18칸이 왼쪽 위부터 잠긴다. 간격이 좁아지는 가속이 그림의 잠금
            // (EaseOutCubic)과 같은 모양이라 눈과 귀가 같은 리듬을 본다.
            // 칸이 잠길수록 높아지고, 밑에는 닫는 화음의 근음(A4)이 깔린다.
            int onset = 0, index = 17, span = 64;
            for (int n = 0; n < 18; ++n) {
                int width = n < 17 ? 44 - n * 2 : 540 - onset;
                if (ms < onset + width) { index = n; span = width; break; }
                onset += width;
            }
            if (span < 4) span = 4;
            int at = i - SFX_RATE * onset / 1000, len = SFX_RATE * span / 1000;
            int attack = SFX_RATE / 1000;
            int env = (at < 0 || at >= len || len < 2) ? 0
                : at < attack ? at * 256 / attack
                : ((len - at) * 256 / len) * ((len - at) * 256 / len) / 256;
            int tick = MaterialTone(&phase[0], (900 + index * 52) * shift / 256);
            int body = MaterialTone(&phase[1], BootNoteHz(24) * shift / 256);   // A4
            value = tick * env / 256 * 26 / 100
                + filtered * env / 256 * 22 / 100
                + body * MaterialEnvelope(i, 0, 540, 60) / 256 * 20 / 100;
        } else if (id == SFX_MOUNT_SPIN) {
            // 정지한 스핀들이 회전수에 오른다. 주파수만 올리면 사이렌이 되므로
            // 공기를 가르는 소리와 베어링 맥놀이를 속도에 비례해 함께 붙인다 -
            // 무거운 것이 돌기 시작하는 소리는 음정이 아니라 그 비율이 만든다.
            // 900ms에 회전수에 닿고 남은 300ms는 그 속도로 돈다 (그림의 기동
            // 구간이 끝나는 자리에서 판독 채터가 이 소리를 이어받는다).
            int climb = ms < 900 ? ms : 900, speed = climb * 256 / 900;
            int base = 26 + climb * 62 / 900;
            int body  = MaterialTone(&phase[0], base * shift / 256);
            int whine = MaterialTone(&phase[1], base * 9 * shift / 256);
            int air   = MaterialTone(&phase[2], base * 23 / 4 * shift / 256);
            int beatn = MaterialTone(&phase[3], (base * 7 / 4 + 3) * shift / 256);
            value = body * MaterialEnvelope(i, 0, 1200, 300) / 256 * 46 / 100
                + whine * MaterialEnvelope(i, 120, 1070, 430) / 256 * 13 / 100 * speed / 256
                + air * MaterialEnvelope(i, 60, 1130, 360) / 256 * 10 / 100 * speed / 256
                + beatn * MaterialEnvelope(i, 0, 1190, 260) / 256 * 14 / 100
                + band * MaterialEnvelope(i, 0, 1200, 520) / 256 * 20 / 100 * speed / 256
                + filtered * MaterialEnvelope(i, 0, 210, 40) / 256 * 10 / 100;
        } else if (id == SFX_MOUNT_LAND) {
            // 헤드가 도는 판 위에 내려앉는다. 닿는 순간의 딱 소리와, 그 뒤로
            // 짧게 붙어 스치는 마찰. 금속 고리는 판이 아니라 암이 떠는 소리다.
            int tick = MaterialTone(&phase[0], 1560 * shift / 256);
            int ring = MaterialTone(&phase[1], 880 * shift / 256);
            int body = MaterialTone(&phase[2], 146 * shift / 256);
            value = tick * MaterialEnvelope(i, 0, 26, 1) / 256 * 34 / 100
                + ring * MaterialEnvelope(i, 4, 300, 3) / 256 * 26 / 100
                + body * MaterialEnvelope(i, 0, 240, 2) / 256 * 32 / 100
                + filtered * MaterialEnvelope(i, 0, 34, 1) / 256 * 30 / 100
                + band * MaterialEnvelope(i, 26, 280, 44) / 256 * 16 / 100;
        } else if (id == SFX_MOUNT_ROT) {
            // 손상 섹터를 밟는다. 잡음을 고르게 깔면 '치익' 하나로 들리므로
            // 33Hz 언저리로 잘라 낸다 - 흠이 난 자리가 판이 도는 주기마다
            // 되돌아오는 소리다. 반음 어긋난 두 음이 밑에서 맥놀이를 만든다.
            int chop = ((ms * 66 / 1000) & 1) ? 256 : 88;
            int fall = ms < 380 ? ms : 380;
            int rasp = MaterialTone(&phase[0], (196 - fall * 74 / 380) * shift / 256);
            int beatn = MaterialTone(&phase[1], (207 - fall * 78 / 380) * shift / 256);
            value = rasp * MaterialEnvelope(i, 0, 400, 6) / 256 * 30 / 100
                + beatn * MaterialEnvelope(i, 0, 400, 6) / 256 * 22 / 100
                + band * MaterialEnvelope(i, 0, 400, 8) / 256 * 44 / 100 * chop / 256
                + filtered * MaterialEnvelope(i, 0, 90, 2) / 256 * 26 / 100;
        } else if (id == SFX_MOUNT_LAW) {
            // 각인. 앞의 40ms는 금속에 파고드는 마찰이고, 그 뒤에 A단조 3화음이
            // 판 전체를 울린다(A4·C5·E5). 삽입 연출의 닫는 화음과 같은 조라
            // 볼륨이 정해지는 순간이 그 악절 안에 앉는다.
            int cut = MaterialTone(&phase[0], 2200 * shift / 256);
            int a = MaterialTone(&phase[1], BootNoteHz(24) * shift / 256);
            int c = MaterialTone(&phase[2], BootNoteHz(27) * shift / 256);
            int e = MaterialTone(&phase[3], BootNoteHz(31) * shift / 256);
            value = cut * MaterialEnvelope(i, 0, 44, 2) / 256 * 24 / 100
                + filtered * MaterialEnvelope(i, 0, 60, 3) / 256 * 26 / 100
                + a * MaterialEnvelope(i, 30, 720, 6) / 256 * 30 / 100
                + c * MaterialEnvelope(i, 46, 680, 10) / 256 * 20 / 100
                + e * MaterialEnvelope(i, 62, 640, 14) / 256 * 16 / 100;
        } else {
            // 삼켜지는 순간. 520ms 동안 올라붙었다가 한 번 크게 닫힌다.
            int climb = ms < 520 ? ms : 520;
            int rush = MaterialTone(&phase[0], (126 + climb * 640 / 520) * shift / 256);
            int over = MaterialTone(&phase[1], (252 + climb * 1480 / 520) * shift / 256);
            int after = ms < 560 ? 0 : ms - 560;
            int slam = MaterialTone(&phase[2], (172 - (after < 120 ? after : 120) * 110 / 120) * shift / 256);
            int swell = climb * 256 / 520;
            value = rush * MaterialEnvelope(i, 0, 545, 300) / 256 * 26 / 100
                + over * MaterialEnvelope(i, 60, 485, 320) / 256 * 12 / 100 * swell / 256
                + band * MaterialEnvelope(i, 0, 545, 380) / 256 * 22 / 100
                + slam * MaterialEnvelope(i, 520, 200, 3) / 256 * 46 / 100
                + filtered * MaterialEnvelope(i, 520, 90, 1) / 256 * 20 / 100;
        }
        // 삽입 연출의 소리는 열몇 개가 한 흐름 안에서 겹친다. 하나하나는 알맞은
        // 크기라도 합치면 천장을 넘으므로(실제로 쟀더니 46,728이었다), 이 계열만
        // 표의 volume 칸을 실제 이득으로 쓴다. 음(RISER~RESOLVE)을 높게 두고
        // 기계음을 그 밑에 깐다 - 무엇이 선율이고 무엇이 반주인지 여기서 정한다.
        // 앞의 재료음 넷은 층 비율이 곧 크기이던 예전 규칙 그대로다.
        if (id >= SFX_BOOT_TEAR) value = value * SFX[id].volume / 100;
        if (value > 32767) value = 32767; else if (value < -32767) value = -32767;
        out[i] = (short)value;
    }
    if (BootWantsRoom(id)) { AddRoomTap(out, count, 23, 20); AddRoomTap(out, count, 47, 11); }
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
static int gDuckGainQ = 1000 << 12;

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
        // 효과음이 시작될 때 25ms에 걸쳐 자리를 내주고, 끝난 뒤에는 160ms 동안
        // 천천히 돌아온다. 65↔100%를 한 샘플에 바꾸면 연속된 부트 큐마다 음악이
        // 펌프처럼 출렁인다.
        int targetQ = (gDuckFrames > 0 ? 650 : 1000) << 12;
        int step = (350 << 12) / (SFX_RATE * (gDuckFrames > 0 ? 25 : 160) / 1000);
        if (gDuckGainQ > targetQ) {
            gDuckGainQ -= step;
            if (gDuckGainQ < targetQ) gDuckGainQ = targetQ;
        } else if (gDuckGainQ < targetQ) {
            gDuckGainQ += step;
            if (gDuckGainQ > targetQ) gDuckGainQ = targetQ;
        }
        int musicDuck = gDuckGainQ >> 12;
        int32_t sfx = accumulator[i] * gSfxVolume / 100;
        int32_t bgm = music[i] * 26 / 100 * musicDuck / 1000 * gMusicVolume / 100;
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
    gDuckGainQ = 1000 << 12;
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
    // 삽입 연출은 음악 위에서 울리는 것이 아니라 음악을 잠시 밀어낸다. 화면이
    // 찢기고 삼켜지는 동안 타이틀 곡이 그대로 흐르면 두 사건이 따로 논다.
    if (id == SFX_BOOT_TEAR || id == SFX_BOOT_SWALLOW) return 620;
    if (id == SFX_BOOT_VORTEX) return 780;
    if (id == SFX_BOOT_FORGE || id == SFX_BOOT_POWER) return 300;
    if (id == SFX_BOOT_LATCH) return 240;
    if (id == SFX_BOOT_RISER) return 1200;
    if (id == SFX_BOOT_RESOLVE) return 1100;
    if (id == SFX_BOOT_STINGER || id == SFX_BOOT_TOLL) return 500;
    if (id == SFX_BOOT_PULSE) return 200;
    if (id == SFX_BOOT_REVEAL) return 460;
    if (id == SFX_BOOT_LOCK) return 540;
    if (id == SFX_BOOT_CHATTER) return 0;   // 바닥에 깔리는 소리라 음악을 밀지 않는다
    // 마운트도 음악 위에서 울리는 것이 아니라 음악을 잠시 밀어낸다. 기동음은
    // 구간 전체를 받치므로 가장 길고, 각인은 그 위에 한 번 선다.
    if (id == SFX_MOUNT_SPIN) return 1200;
    if (id == SFX_MOUNT_LAW) return 760;
    if (id == SFX_MOUNT_ROT) return 300;
    if (id == SFX_MOUNT_LAND) return 220;
    if (id >= SFX_BOOT_TEAR && id <= SFX_BOOT_SWALLOW) return 120;
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
