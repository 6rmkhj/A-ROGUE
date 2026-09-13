#pragma once

#include <windows.h>

// 파형을 그때그때 합성해 waveOut 스트림에 섞어 넣는 효과음 모듈.
// 게임 상태나 그리기와 전혀 얽히지 않으므로 단독으로 교체할 수 있다.

#define AUDIO_TIMER_ID 5

enum SfxId {
    SFX_UI_CLICK = 0, SFX_DIE_PICK, SFX_SLOT_SET, SFX_TARGET,
    SFX_READ_START, SFX_DIE_LOCK, SFX_EXECUTE, SFX_ENEMY_DOWN,
    SFX_REWARD_PICK, SFX_REWARD_SET, SFX_PRUNE, SFX_CONFIRM,
    SFX_BOOT, SFX_VICTORY, SFX_GAMEOVER, SFX_PLAYER_HIT, SFX_CRASH,
    // 기믹 발동. 계열마다 하나씩 두고 층별 구분은 PlaySfxPitched로 낸다.
    SFX_FX_LOCK, SFX_FX_RESTORE, SFX_FX_OFFLINE, SFX_FX_ROUTE, SFX_FX_PRESSURE, SFX_FX_QUARANTINE,
    // 플레이어 공격이 적에게 닿는 순간. 계열 SFX와 달리 매 턴 울리므로 짧고 건조하다.
    SFX_HIT_IMPACT,
    SFX_CHARGE, SFX_HEAVY_HIT, SFX_SHIELD_RISE, SFX_SHIELD_BLOCK, SFX_CHAIN_ARC,
    SFX_UI_FOCUS, SFX_BOSS_ARRIVE, SFX_LOOT_REVEAL, SFX_REPAIR,
    // 새 게임 삽입 연출 전용. 이 연출은 4초 동안 혼자 화면을 쓰는데 예전에는
    // 주사위·보상 효과음을 빌려 썼다. 빌린 소리는 "무엇이 일어났는가"를 말하지
    // 못한다 - 화면이 찢기고, 감겨 들어가고, 플라스틱이 슬롯을 긁고, 걸쇠가
    // 물리고, 모터가 돌고, 브라운관이 켜지는 것은 전부 다른 물건의 소리다.
    SFX_BOOT_TEAR, SFX_BOOT_VORTEX, SFX_BOOT_FORGE, SFX_BOOT_FLIP, SFX_BOOT_SLIDE,
    SFX_BOOT_LATCH, SFX_BOOT_MOTOR, SFX_BOOT_SEEK, SFX_BOOT_POWER, SFX_BOOT_SWALLOW,
    // 위의 열 개는 기계 소리다 - 무슨 일이 일어나는지는 말하지만 그것만으로는
    // 끝까지 소음이다. 아래 다섯은 음이다. 타이틀 곡이 A단조(tonic 0 = A2,
    // music.cpp의 SONG[0])이므로 이 연출도 같은 조에서 오르고 같은 조로 닫는다.
    // 기계음이 마디를 치고, 이 음들이 그 마디를 하나의 악절로 묶는다.
    SFX_BOOT_RISER,    // 1200ms 상승. A단조 아르페지오가 가속하며 올라간다
    SFX_BOOT_STINGER,  // 벼림. A단조 화음이 한 번에 선다
    SFX_BOOT_PULSE,    // 한 음. 피치 0/3/7로 A3·C4·E4가 되어 하강을 따라 내려온다
    SFX_BOOT_TOLL,     // 낮은 A. 걸쇠가 물리는 자리의 근음
    SFX_BOOT_RESOLVE,  // 마지막. A단조가 옥타브로 펼쳐지며 닫힌다
    // 카메라가 붙고 나서 생긴 세 자리. 그림에는 사건이 있는데 소리가 없던 곳이다.
    SFX_BOOT_REVEAL,   // 카메라가 물러나며 방이 열린다 (하강 구간이 통째로 조용했다)
    SFX_BOOT_CHATTER,  // 판독 중의 데이터 채터 (연출에서 가장 평평한 구간이었다)
    SFX_BOOT_LOCK,     // 18칸이 차례로 잠긴다. 그림의 잠금과 같은 가속
    SFX_COUNT
};

void AudioOpen(HWND window);
void AudioClose();
// 장치가 버퍼를 다 비웠는데 채워 주지 못한 횟수. 끊김이 실제로 났는지 여기서 본다.
int AudioUnderruns();
void PlaySfx(int id);
void PlaySfxPitched(int id, int semitones);
// The same deterministic PCM synthesis used by the live mixer. No device,
// window, game RNG, or playback is required for offline waveform verification.
// Returns samples written: mono signed 16-bit PCM at 22050 Hz, at most 17640.
int RenderSfx(int id, int semitones, short* out, int capacity);

// 모든 효과음에 함께 걸리는 마스터 볼륨 (0~100). 합성 단계가 아니라 믹서에서
// 걸리므로 이미 울리고 있는 소리에도 곧바로 적용되고, 파형을 다시 만들 필요가 없다.
void SetAudioVolume(int percent);
int AudioVolume();
void AudioSetMusicVolume(int percent);
int AudioMusicVolume();
void AudioSetSfxVolume(int percent);
int AudioSfxVolume();

void AudioSetScene(int scene);
void AudioSetDrive(int drive);
void AudioSetIntensity(int intensity);
void AudioSetCritical(int critical);
void AudioSetMusicEnabled(int enabled);
int AudioMusicEnabled();
void AudioSetEnding(int ending);
