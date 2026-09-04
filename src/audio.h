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
    SFX_COUNT
};

void AudioOpen(HWND window);
void AudioPump();
void AudioClose();
void PlaySfx(int id);
void PlaySfxPitched(int id, int semitones);

// 모든 효과음에 함께 걸리는 마스터 볼륨 (0~100). 합성 단계가 아니라 믹서에서
// 걸리므로 이미 울리고 있는 소리에도 곧바로 적용되고, 파형을 다시 만들 필요가 없다.
void SetAudioVolume(int percent);
int AudioVolume();
