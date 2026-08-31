#pragma once

#include <windows.h>

// 파형을 그때그때 합성해 waveOut 스트림에 섞어 넣는 효과음 모듈.
// 게임 상태나 그리기와 전혀 얽히지 않으므로 단독으로 교체할 수 있다.

#define AUDIO_TIMER_ID 5

enum SfxId {
    SFX_UI_CLICK = 0, SFX_DIE_PICK, SFX_SLOT_SET, SFX_TARGET,
    SFX_READ_START, SFX_DIE_LOCK, SFX_EXECUTE, SFX_ENEMY_DOWN,
    SFX_REWARD_PICK, SFX_REWARD_SET, SFX_PRUNE, SFX_CONFIRM,
    SFX_BOOT, SFX_VICTORY, SFX_GAMEOVER, SFX_PLAYER_HIT, SFX_CRASH, SFX_COUNT
};

void AudioOpen(HWND window);
void AudioPump();
void AudioClose();
void PlaySfx(int id);
void PlaySfxPitched(int id, int semitones);
