#pragma once
#include <stdint.h>

#define MUSIC_RATE 22050
#define MUSIC_CHANNELS 4

enum MusicScene { MUSIC_SCENE_TITLE = 0, MUSIC_SCENE_PLAY, MUSIC_SCENE_STORY, MUSIC_SCENE_GAMEOVER, MUSIC_SCENE_VICTORY };

struct MusicChannelState { uint32_t phase; int gain; int targetGain; int note; int envelope; };
struct MusicState {
    int currentDrive, targetDrive, scene, intensity, targetIntensity, critical;
    uint32_t sampleClock, stepClock, noiseRng;
    int step, bar, bpm, stepCount;
    MusicChannelState channel[MUSIC_CHANNELS];
    int enabled;
    int currentScene;
    int driveFade;
    int sceneFade;
    int intensityGain;
    int ending;
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
