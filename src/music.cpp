#include "music.h"

struct MusicPattern {
    int8_t bass[16], lead[16], arp[16];
    uint16_t bassGate, leadGate, arpGate, kickMask, hatMask, snareMask;
};

static const MusicPattern PATTERN[6] = {
 {{0,0,7,0,5,0,7,0,0,0,7,0,10,7,5,0},{12,0,15,0,19,0,15,0,12,0,17,0,19,0,15,0},{24,19,15,19,24,19,15,19,22,19,15,19,24,19,17,19},0x5555,0x4444,0xFFFF,0x1111,0xAAAA,0x1010},
 {{-5,0,2,0,0,0,2,0,-5,0,2,0,3,2,0,0},{7,0,10,0,14,0,10,0,7,0,12,0,14,0,10,0},{19,14,10,14,19,14,10,14,17,14,10,14,19,14,12,14},0x5151,0x4444,0x7777,0x1111,0x8888,0x1010},
 {{0,0,7,5,0,7,0,10,0,0,7,0,5,7,10,0},{12,0,15,0,19,15,0,12,0,17,0,19,0,15,12,0},{24,19,0,19,24,0,15,19,22,19,0,19,24,0,17,19},0x59A5,0x64C4,0xDBDB,0x1291,0xAAAA,0x1010},
 {{0,7,0,5,0,7,0,10,0,7,0,5,0,10,7,0},{12,0,19,0,15,0,22,0,12,0,19,0,17,0,24,0},{24,12,19,7,24,12,19,7,22,10,19,7,24,12,17,5},0x5555,0x5555,0xFFFF,0x5555,0xAAAA,0x4444},
 {{0,7,5,7,0,10,7,5,0,7,5,10,12,10,7,5},{12,15,19,15,12,17,19,15,12,15,19,22,24,19,17,15},{24,19,15,19,24,22,19,17,24,19,15,22,24,19,17,15},0xFFFF,0xAAAA,0xFFFF,0x5555,0xFFFF,0x4444},
 {{0,0,1,0,6,0,1,0,0,6,0,1,0,10,1,0},{12,0,13,0,18,0,13,0,12,18,0,13,0,22,13,0},{24,19,13,19,24,19,13,19,18,13,19,24,19,13,22,19},0x5555,0x4444,0x7FFF,0x1111,0x6AAA,0x4444}
};
static const int BPM[6] = {100,88,108,124,142,99};

static int NoteHz(int semitone) {
    static const int ratio[12] = {1024,1085,1149,1218,1290,1367,1448,1534,1625,1722,1825,1933};
    int octave = semitone / 12, note = semitone % 12;
    if (note < 0) { note += 12; --octave; }
    int hz = 110 * ratio[note] / 1024;
    while (octave > 0) { hz *= 2; --octave; }
    while (octave < 0) { hz /= 2; ++octave; }
    return hz < 20 ? 20 : hz;
}

void MusicInit(MusicState* m) {
    if (!m) return;
    *m = {};
    m->noiseRng = 0xA11CE55u; m->enabled = 1; m->stepCount = 16; m->bpm = BPM[0];
    m->driveFade = 65536; m->sceneFade = 65536; m->currentScene = MUSIC_SCENE_TITLE;
}
void MusicSetDrive(MusicState* m, int d) { if (m) m->targetDrive = d < 0 ? 0 : d > 5 ? 5 : d; }
void MusicSetScene(MusicState* m, int s) { if (m) m->scene = s; }
void MusicSetIntensity(MusicState* m, int v) { if (m) m->targetIntensity = v < 0 ? 0 : v > 3 ? 3 : v; }
void MusicSetCritical(MusicState* m, int v) { if (m) m->critical = v != 0; }
void MusicSetEnabled(MusicState* m, int v) { if (m) m->enabled = v != 0; }
void MusicSetEnding(MusicState* m, int v) { if (m) m->ending = v != 0; }

static int Triangle(uint32_t phase) { int p = (int)(phase >> 16) & 65535; return p < 32768 ? p * 2 - 32768 : 98303 - p * 2; }
static int Pulse(uint32_t phase, int duty) { return ((int)(phase >> 16) & 65535) < duty * 655 ? 22000 : -22000; }

void MusicRender(MusicState* m, int32_t* out, int frames) {
    if (!m || !out || frames <= 0) return;
    for (int i = 0; i < frames; ++i) {
        // drive/scene은 sample clock을 건드리지 않고 0.4초 fade-out, 0.7초 fade-in한다.
        if (m->currentDrive != m->targetDrive) {
            m->driveFade -= 7;
            if (m->driveFade <= 0) { m->driveFade = 0; m->currentDrive = m->targetDrive; }
        } else if (m->driveFade < 65536) { m->driveFade += 4; if (m->driveFade > 65536) m->driveFade = 65536; }
        if (m->currentScene != m->scene) {
            m->sceneFade -= 7;
            if (m->sceneFade <= 0) { m->sceneFade = 0; m->currentScene = m->scene; }
        } else if (m->sceneFade < 65536) { m->sceneFade += 4; if (m->sceneFade > 65536) m->sceneFade = 65536; }
        m->bpm = BPM[m->currentDrive]; m->stepCount = m->currentDrive == 5 ? 15 : 16;
        const MusicPattern* p = &PATTERN[m->currentDrive];
        m->stepClock += (uint32_t)(m->bpm * 4);
        if (m->stepClock >= MUSIC_RATE * 60u) {
            m->stepClock -= MUSIC_RATE * 60u;
            if (++m->step >= m->stepCount) { m->step = 0; ++m->bar; }
        }
        ++m->sampleClock;
        if (!m->enabled || m->currentScene == MUSIC_SCENE_GAMEOVER) continue;
        int targetGain = m->targetIntensity * 65536;
        if (m->currentScene == MUSIC_SCENE_TITLE || m->currentScene == MUSIC_SCENE_STORY) targetGain = 0;
        if (m->intensityGain < targetGain) { m->intensityGain += 6; if (m->intensityGain > targetGain) m->intensityGain = targetGain; }
        else if (m->intensityGain > targetGain) { m->intensityGain -= 6; if (m->intensityGain < targetGain) m->intensityGain = targetGain; }
        m->intensity = (m->intensityGain + 32768) / 65536;
        int mix = m->intensityGain / 256;
        int step = m->step;
        int sample = 0;
        int bassOn = (p->bassGate >> step) & 1;
        int leadOn = (p->leadGate >> step) & 1;
        int arpOn = (p->arpGate >> step) & 1;
        // E: 고정 dropout. 같은 위치가 항상 빠져 결정론을 보존한다.
        if (m->currentDrive == 2 && (step == 3 || step == 10)) leadOn = arpOn = 0;
        int bassNote = p->bass[step] - (m->critical ? 12 : 0);
        int hz = NoteHz(bassNote);
        m->channel[0].phase += (uint32_t)((uint64_t)hz * 0x100000000ull / MUSIC_RATE);
        if (bassOn) sample += Triangle(m->channel[0].phase) * (mix + 512) / 2304;
        int leadNote = p->lead[step];
        if (m->currentScene == MUSIC_SCENE_VICTORY) leadNote += m->ending ? 1 : 7;
        hz = NoteHz(leadNote); m->channel[1].phase += (uint32_t)((uint64_t)hz * 0x100000000ull / MUSIC_RATE);
        if (leadOn && !m->critical) sample += Pulse(m->channel[1].phase, m->currentDrive == 3 ? 25 : 38) * (mix + 256) / 2816;
        hz = NoteHz(p->arp[step]); m->channel[2].phase += (uint32_t)((uint64_t)hz * 0x100000000ull / MUSIC_RATE);
        if (arpOn && mix >= 128) sample += Pulse(m->channel[2].phase, 25) * mix / 3840;
        int percussion = 0;
        if ((p->kickMask >> step) & 1) percussion += 9000;
        if ((p->snareMask >> step) & 1) percussion += 5000;
        if ((p->hatMask >> step) & 1) percussion += 1800;
        if (percussion && (mix >= 128 || m->currentScene == MUSIC_SCENE_VICTORY)) {
            m->noiseRng = m->noiseRng * 1664525u + 1013904223u;
            sample += (((int)(m->noiseRng >> 16) - 32768) * percussion) / 32768;
        }
        if (m->critical) sample += Pulse(m->channel[0].phase * 2u, 18) / 10;
        sample = (int)((int64_t)sample * m->driveFade / 65536 * m->sceneFade / 65536);
        out[i] += sample;
    }
}
