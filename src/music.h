#pragma once
#include <stdint.h>

#define MUSIC_RATE 22050
#define MUSIC_CHANNELS 4   // 베이스 · 리드 · 아르페지오 · 코드(60Hz 고속 아르페지오)

enum MusicScene { MUSIC_SCENE_TITLE = 0, MUSIC_SCENE_PLAY, MUSIC_SCENE_STORY, MUSIC_SCENE_GAMEOVER, MUSIC_SCENE_VICTORY };

// 음 하나의 상태. env는 0~65536이고 stage 0은 어택 중, 1은 감쇠·지속이다.
// lp는 1극 저역통과 필터의 이전 출력이다.
// held는 음이 울린 샘플 수. 리드 비브라토가 이 값을 보고 늦게 들어온다.
struct MusicChannelState { uint32_t phase; int env; int lp; int note; int stage; int gate; int held; };
// 타악기. 킥은 피치가 떨어지는 사인, 스네어는 노이즈와 짧은 몸통, 하이햇은 고역 노이즈.
struct MusicDrumState { uint32_t kickPhase, bodyPhase; int kickEnv, snareEnv, hatEnv, noisePrev; };
struct MusicState {
    int currentDrive, targetDrive, scene, intensity, targetIntensity, critical;
    uint32_t sampleClock, stepClock, noiseRng;
    int step, bar, bpm, stepCount;
    MusicChannelState channel[MUSIC_CHANNELS];
    MusicDrumState drum;
    int enabled;
    int currentScene;
    int driveFade;
    int sceneFade;
    int intensityGain;   // 0 ~ 3x65536. 층이 들어오는 정도 (볼륨이 아니라 편곡)
    int playGain;        // 씬이 연주 중인가. 타이틀·스토리에서는 0으로 내려간다
    int criticalGain;    // 위독 편곡으로 넘어간 정도
    int ending;
    int pendingStep;     // 1이면 다음 샘플에서 현재 스텝의 음·타악기를 트리거한다
    int chordThird;      // 이번 마디 코드의 3도 (3 단조 / 4 장조). 코드 성부가 읽는다
};

void MusicInit(MusicState* music);
void MusicSetDrive(MusicState* music, int drive);
void MusicSetScene(MusicState* music, int scene);
void MusicSetIntensity(MusicState* music, int intensity);
void MusicSetCritical(MusicState* music, int critical);
void MusicSetEnabled(MusicState* music, int enabled);
void MusicSetEnding(MusicState* music, int ending);
// 기존 accumulator에 음악을 더한다. 게임 RNG나 Windows API를 전혀 사용하지 않는다.
void MusicRender(MusicState* music, int32_t* accumulator, int frames);
