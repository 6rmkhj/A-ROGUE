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

// ---- 피격·위독·정지 연출 --------------------------------------------------
#define CRITICAL_HP 10         // 이 체력 이하부터 화면이 노이즈에 잠식된다
#define STRIKE_MS 440          // 적이 달려들었다가 제자리로 돌아오는 시간
#define STRIKE_POP_MS 720      // 피해 숫자가 떠오르다 사라지는 시간
#define PLAYER_HIT_MS 420      // 피격 테두리 섬광
#define SHAKE_MS 300           // 화면 흔들림
#define DEATH_CREEP_MS 1400    // 체력 0에서 노이즈가 화면을 다 갉아먹기까지
#define DEATH_STATIC_MS 2500   // 그 뒤 재시작 화면이 나오기까지

// ---- 공유 상태 (main.cpp가 소유한다) --------------------------------------
extern GameState gGame;
extern HWND gWindow;
extern POINT gMouse;
extern int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
extern int gGuidePage;   // 0 = 공통 규칙, 1 = 현재 드라이브·보스 기믹
extern int gRestartArmed; // 설정 화면의 "다시 시작" 버튼: 0=대기, 1=한 번 더 누르면 확정

// 주사위 판독 연출
extern int gReadActive, gRolled;
// 전투 종료·턴 계산·볼륨 진입 연출
extern int gCombatClearActive, gClearedFloor, gClearedEncounter;
extern int gTurnTraceActive;
extern DWORD gTurnTraceStart;
extern int gDescentActive, gDescentToFloor;
extern DWORD gDescentStart;
// 체력 0 이후의 정지 연출 (노이즈가 화면을 삼키고 나면 재시작 화면으로 넘어간다)
extern int gDeathActive;
extern DWORD gDeathStart;

// ---- 연출 질의 (main.cpp가 계산하고 화면이 읽는다) ------------------------
int DieNoise(int die);
int DieSettled(int die);
int DieSettleFlash(int die);
int NoiseStep(int die);
void SyncEnemyDamage();
int EnemyHitFlash(int index);
int EnemyBob(int index);
void SyncIdleAnimation();
// 가이드 2페이지에 아직 미판독 칸이 남아 있는가. 남아 있으면 가이드가 열려 있는
// 동안에도 리페인트를 계속 돌려야 노이즈가 멈추지 않는다.
int GuideNoiseActive();

// 계산 재생에서 지금까지 드러난 줄 수 (0 = 아직 없음)
int TurnTraceShown();

// 적의 타격: 계산 재생이 그 적의 [적 행동] 줄에 닿는 순간 발동한다.
void SyncEnemyStrikes();
int EnemyStrikeDrop(int index);     // 플레이어 쪽(아래)으로 파고드는 픽셀
int EnemyStrikeShift(int index);    // 달려들 때의 좌우 흔들림
int EnemyStrikePop(int index);      // 피해 숫자 표시 강도 1000 → 0
int EnemyStrikeDamage(int index);   // 그때 들어온 피해 (0 = 방어도가 전부 막음)

// 피격 반응: 화면 흔들림과 테두리 섬광
int PlayerHitFlash();
int PlayerHitBlocked();
int ScreenShakeX();
int ScreenShakeY();

// 화면 노이즈. 살아 있는 동안에는 항상 가장자리에만 머문다.
//   체력 2~CRITICAL_HP : 체력이 줄수록 띠가 두꺼워지고 짙어진다
//   체력 1             : 버티는 시간만큼 띠가 더 두꺼워지고 짙어진다 (중앙은 그대로)
//   체력 0             : 그 노이즈가 화면 전체를 갉아먹으며 안으로 좁혀 들어온다
int AmbientNoiseLevel();   // 테두리 띠 밀도 (0 = 위독 연출 없음)
int AmbientNoiseBand();    // 테두리 띠 두께(px)
int DeathCreepAmount();    // 잠식 정도 0~1000 (0 = 정지 중이 아님)
void SyncLastGasp();       // 체력 1이 된 시각을 잡아 둔다 (띠가 자라는 기준)
int NoiseFrameStep();

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
RECT RestartButtonRect();
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
