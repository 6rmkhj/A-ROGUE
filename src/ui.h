#pragma once

#include <windows.h>
#include "game.h"
#include "render.h"

// 화면(screens.cpp)과 입력·창 관리(main.cpp)가 함께 쓰는 상태와 레이아웃.
// 좌표는 전부 render.h의 고정 캔버스(BASE_WIDTH x BASE_HEIGHT) 기준이다.

#define SETTINGS_SCALE_COUNT 5
static const int SCALE_OPTIONS[SETTINGS_SCALE_COUNT] = {75, 100, 125, 150, 200};

// 카드 0~2는 설치할 면, 마지막 카드는 면 대신 체력을 얻는 섹터 복구다.
#define REWARD_CARD_COUNT 4
#define REWARD_REPAIR 3

#define COMBAT_CLEAR_MS 1500
#define TURN_TRACE_STEP_MS 360
#define DESCENT_MS 2400
#define NOISE_CHURN_MS 45      // 노이즈가 다시 섞이는 주기

// ---- 공유 상태 (main.cpp가 소유한다) --------------------------------------
extern GameState gGame;
extern HWND gWindow;
extern POINT gMouse;
extern int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
extern int gGuidePage;   // 0 = 공통 규칙, 1 = 현재 드라이브·보스 기믹

// 주사위 판독 연출
extern int gReadActive, gRolled;
// 전투 종료·턴 계산·볼륨 진입 연출
extern int gCombatClearActive, gClearedFloor, gClearedEncounter;
extern int gTurnTraceActive;
extern DWORD gTurnTraceStart;
extern int gDescentActive, gDescentToFloor;
extern DWORD gDescentStart;

// ---- 연출 질의 (main.cpp가 계산하고 화면이 읽는다) ------------------------
int DieNoise(int die);
int DieSettled(int die);
int DieSettleFlash(int die);
int NoiseStep(int die);
void SyncEnemyDamage();
int EnemyHitFlash(int index);
int EnemyBob(int index);
void SyncIdleAnimation();

// ---- 레이아웃 (그리기와 클릭 판정이 같은 사각형을 봐야 한다) --------------
RECT GuideButtonRect(int width);
RECT GuideCloseRect(int width);
RECT GuidePrevRect(int width, int height);
RECT GuideNextRect(int width, int height);
RECT SettingsButtonRect(int width);
RECT SettingsCloseRect(int width);
RECT DeckButtonRect(int width);
RECT DeckCloseRect(int width);
RECT ScaleOptionRect(int index);
RECT FullscreenToggleRect();
RECT StartButtonRect(int width, int height);
RECT DriveCardRect(int i);
RECT EnemyRect(int i);
RECT SlotRect(int i);
RECT DieRect(int i);
RECT EndTurnRect();
RECT ReadButtonRect();
RECT RewardRect(int i, int width);
RECT FaceGridRect(int die, int face);
RECT ContinueRect(int width, int height);
RECT KeybButtonRect();
RECT PruneTsrRect(int i);

int DieForSlotUI(int slot);
int CanRepairSector();

// ---- 화면 -----------------------------------------------------------------
void ApplyFullscreen(int enable);
void ApplyWindowedScale(int percent);
void PaintGame(HWND window);
