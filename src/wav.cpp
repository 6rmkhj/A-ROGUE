// 개발 전용: BGM을 드라이브별 WAV로 뽑고 구조를 글로 찍는다. AROGUE.exe에는 들어가지 않는다.
//
//   wav.exe <출력 폴더>
//
// 드라이브마다 40초를 쓴다. 0~12초 일반전(intensity 1), 12~24초 보스전(2),
// 24~40초 보스 체력 절반(3). 마지막에 C:\ 위독 상태 8초를 따로 뽑는다.
// MusicRender를 게임과 똑같이 부르므로 여기서 들리는 것이 곧 게임에서 나는 것이다.
// 게임 믹서는 음악에 26%를 곱하고 다시 마스터 볼륨을 곱하므로 실제 게임에서는
// 이보다 작게 난다. 감상용으로는 크게 쓴다.
//
// WAV 뒤에 찍는 구조 로그는 귀 대신 눈으로 보는 검증이다: 마디마다 베이스 근음과
// 리드가 새로 울린 스텝, intensity별 RMS(층이 순서대로 들어오는가), 타이틀 씬 무음.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "music.h"

static const char* LETTER = "cdenrx";
static const char* NAME[12] = {"A","Bb","B","C","Db","D","Eb","E","F","Gb","G","Ab"};
static void NoteName(int n, char* out) { int o = n / 12, k = n % 12; if (k < 0) { k += 12; --o; } sprintf(out, "%s%d", NAME[k], o + 2); }

static void WriteWav(const char* path, const int16_t* pcm, int frames) {
    FILE* f = fopen(path, "wb");
    if (!f) { printf("cannot open %s\n", path); return; }
    uint32_t dataBytes = (uint32_t)frames * 2, rate = MUSIC_RATE, byteRate = rate * 2;
    uint16_t channels = 1, align = 2, bits = 16, fmt = 1;
    uint32_t fmtLen = 16, riffLen = 36 + dataBytes;
    fwrite("RIFF", 1, 4, f); fwrite(&riffLen, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtLen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&channels, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataBytes, 4, 1, f);
    fwrite(pcm, 2, frames, f);
    fclose(f);
}

// 감상용 배율. 게임 안에서는 26% x 마스터 볼륨이 곱해진다.
#define GAIN_PERCENT 45

static void Render(MusicState* m, int32_t* acc, int16_t* pcm, int frames, int* peak, int64_t* sq, int* clipped) {
    memset(acc, 0, sizeof(int32_t) * frames);
    // 게임 믹서와 같은 20ms 덩어리로 부른다. 덩어리 경계에서 이음새가 있으면 여기서 드러난다.
    for (int at = 0; at < frames; at += 441) {
        int n = frames - at < 441 ? frames - at : 441;
        MusicRender(m, acc + at, n);
    }
    for (int i = 0; i < frames; ++i) {
        int raw = acc[i];
        int a = raw < 0 ? -raw : raw;
        if (a > *peak) *peak = a;
        *sq += (int64_t)raw * raw;
        int s = raw * GAIN_PERCENT / 100;
        if (s > 32767) { s = 32767; ++*clipped; } else if (s < -32768) { s = -32768; ++*clipped; }
        pcm[i] = (int16_t)s;
    }
}

// 4마디 동안의 RMS와 피크. 페이드가 끝난 뒤 잰다.
static void MeasureBars(MusicState* m, int bars, double* rms, int* peak) {
    static int32_t buf[64];
    int stepSamples = (int)(MUSIC_RATE * 60.0 / (m->bpm * 4) + 0.5), n = stepSamples * m->stepCount * bars;
    double sq = 0; *peak = 0;
    for (int i = 0; i < n; i += 64) {
        memset(buf, 0, sizeof(buf)); MusicRender(m, buf, 64);
        for (int k = 0; k < 64; ++k) { sq += (double)buf[k] * buf[k]; int a = buf[k] < 0 ? -buf[k] : buf[k]; if (a > *peak) *peak = a; }
    }
    *rms = sqrt(sq / n);
}

static void Warm(MusicState* m, int seconds) {
    static int32_t buf[64];
    for (int i = 0; i < MUSIC_RATE * seconds; i += 64) { memset(buf, 0, sizeof(buf)); MusicRender(m, buf, 64); }
}

// 8마디의 화성과 리드 트리거를 한 줄씩 찍는다. 시퀀서 상태만 읽으므로 DSP와 무관하다.
static void PrintScore(int drive) {
    static int32_t buf[64];
    MusicState m; MusicInit(&m); m.currentDrive = m.targetDrive = drive;
    MusicSetScene(&m, MUSIC_SCENE_PLAY); MusicSetIntensity(&m, 3);
    char line[512] = {0}, name[8];
    int lastStep = -1, lastBar = -1, bars = 0;
    // 첫 마디는 페이드 중이라 버린다. 그 다음 8마디를 본다.
    while (bars < 10) {
        memset(buf, 0, sizeof(buf)); MusicRender(&m, buf, 64);
        if (m.step == lastStep) continue;
        lastStep = m.step;
        if (m.bar != lastBar) {
            if (line[0] && bars >= 2) printf("%s\n", line);
            lastBar = m.bar; ++bars; line[0] = 0;
            NoteName(m.channel[0].note, name);
            sprintf(line, "  bar %2d [%c] bass %-3s | lead:", m.bar, ((m.bar >> 2) & 1) ? 'B' : 'A', name);
        }
        const MusicChannelState* L = &m.channel[1];
        char cell[16];
        if (L->gate && L->stage == 0) { NoteName(L->note, name); sprintf(cell, " %-3s", name); }
        else sprintf(cell, " %-3s", L->gate ? "-" : ".");
        strcat(line, cell);
    }
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    const int seconds = 40, frames = MUSIC_RATE * seconds;
    static int32_t acc[MUSIC_RATE * 40];
    static int16_t pcm[MUSIC_RATE * 40];
    char path[512];
    for (int d = 0; d < 6; ++d) {
        MusicState m; MusicInit(&m);
        m.currentDrive = m.targetDrive = d;   // 드라이브 전환 페이드를 건너뛴다
        MusicSetScene(&m, MUSIC_SCENE_PLAY);
        int peak = 0, clipped = 0; int64_t sq = 0;
        // 세 구간을 이어 붙인다. intensity는 시퀀서를 재시작하지 않으므로 곡이 이어진다.
        int cut1 = MUSIC_RATE * 12, cut2 = MUSIC_RATE * 24;
        MusicSetIntensity(&m, 1); Render(&m, acc, pcm, cut1, &peak, &sq, &clipped);
        MusicSetIntensity(&m, 2); Render(&m, acc + cut1, pcm + cut1, cut2 - cut1, &peak, &sq, &clipped);
        MusicSetIntensity(&m, 3); Render(&m, acc + cut2, pcm + cut2, frames - cut2, &peak, &sq, &clipped);
        snprintf(path, sizeof(path), "%s/drive_%c.wav", dir, LETTER[d]);
        WriteWav(path, pcm, frames);
        int rms = 0; { int64_t mean = sq / frames; while ((int64_t)(rms + 1) * (rms + 1) <= mean) ++rms; }
        printf("drive_%c.wav  peak %5d  rms %5d  clipped %d  bpm %d  steps %d\n", LETTER[d], peak, rms, clipped, m.bpm, m.stepCount);
        PrintScore(d);
        for (int in = 0; in <= 3; ++in) {
            MusicState t; MusicInit(&t); t.currentDrive = t.targetDrive = d;
            MusicSetScene(&t, MUSIC_SCENE_PLAY); MusicSetIntensity(&t, in);
            Warm(&t, 2);
            double r; int p; MeasureBars(&t, 4, &r, &p);
            printf("  intensity %d: rms %5.0f  peak %5d\n", in, r, p);
        }
        {
            MusicState t; MusicInit(&t); t.currentDrive = t.targetDrive = d; MusicSetScene(&t, MUSIC_SCENE_TITLE);
            Warm(&t, 3);
            double r; int p; MeasureBars(&t, 1, &r, &p);
            printf("  title scene after 3s: peak %d\n", p);
        }
    }
    {
        MusicState m; MusicInit(&m);
        m.currentDrive = m.targetDrive = 0;
        MusicSetScene(&m, MUSIC_SCENE_PLAY); MusicSetIntensity(&m, 2); MusicSetCritical(&m, 1);
        int peak = 0, clipped = 0; int64_t sq = 0;
        int n = MUSIC_RATE * 8;
        Render(&m, acc, pcm, n, &peak, &sq, &clipped);
        snprintf(path, sizeof(path), "%s/critical_c.wav", dir);
        WriteWav(path, pcm, n);
        printf("critical_c.wav  peak %5d  clipped %d\n", peak, clipped);
    }
    return 0;
}
