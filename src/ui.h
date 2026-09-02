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
#define DIR_ENTER_MS 900       // 디렉터리 진입 연출
#define NOISE_CHURN_MS 45      // 노이즈가 다시 섞이는 주기

// ---- 피격·위독·정지 연출 --------------------------------------------------
#define CRITICAL_HP 10         // 이 체력 이하부터 화면이 노이즈에 잠식된다
#define STRIKE_MS 440          // 적이 달려들었다가 제자리로 돌아오는 시간
#define STRIKE_POP_MS 720      // 피해 숫자가 떠오르다 사라지는 시간
#define PLAYER_HIT_MS 420      // 피격 테두리 섬광
#define SHAKE_MS 300           // 화면 흔들림
#define DEATH_CREEP_MS 1400    // 체력 0에서 노이즈가 화면을 다 갉아먹기까지
#define DEATH_STATIC_MS 2500   // 그 뒤 재시작 화면이 나오기까지

// ---- 연출 강도 -------------------------------------------------------------
// 줄어드는 것은 흔들림·파편·전역 글리치 같은 장식뿐이다. 슬롯 잠금과 다음 잠금
// 대상, 오프라인 주사위, 격리·삭제 대상 면, 해결 순서와 역전 예고, 압력 게이지,
// HP 잔상과 실제 피해 숫자는 어떤 모드에서도 숨기지 않는다.
enum FxLevel { FX_FULL = 0, FX_REDUCED, FX_OFF, FX_LEVEL_COUNT };
extern int gFxLevel;
int FxScale(int amount);   // 장식 강도를 현재 모드로 줄인다 (REDUCED 50%, OFF 0)
int FxDecorOn();           // 움직이는 장식을 그려도 되는가

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
extern DWORD gCombatClearStart;
extern int gTurnTraceActive;
extern DWORD gTurnTraceStart;
extern int gDescentActive, gDescentToFloor;
extern DWORD gDescentStart;
// 디렉터리 진입: 고른 경로 조각이 타이핑되는 짧은 오버레이
extern int gDirEnterActive, gDirEnterKind;
extern DWORD gDirEnterStart;
// 체력 0 이후의 정지 연출 (노이즈가 화면을 삼키고 나면 재시작 화면으로 넘어간다)
extern int gDeathActive;
extern DWORD gDeathStart;

// ---- 연출 질의 (main.cpp가 계산하고 화면이 읽는다) ------------------------
int DieNoise(int die);
int DieSettled(int die);
int DieSettleFlash(int die);
int NoiseStep(int die);
int EnemyBob(int index);
void SyncIdleAnimation();
// 가이드 2페이지에 아직 미판독 칸이 남아 있는가. 남아 있으면 가이드가 열려 있는
// 동안에도 리페인트를 계속 돌려야 노이즈가 멈추지 않는다.
int GuideNoiseActive();

// 계산 재생에서 지금까지 드러난 줄 수 (0 = 아직 없음)
int TurnTraceShown();

// ---- 기믹 발동 연출 --------------------------------------------------------
// 계산 재생이 끝나 새 턴 화면이 드러나는 순간 시작된다. 규칙은 이미 game.cpp에서
// 확정된 뒤이므로 여기서는 보여 주는 방식만 정한다.
int GimmickFxKind();        // 재생 중인 기믹 (GIMMICK_NONE = 없음)
int GimmickFxElapsed();     // 시작으로부터 경과 ms
int GimmickFxA();           // 대상 1 (슬롯·주사위·면 또는 수치)
int GimmickFxB();           // 대상 2
int GimmickFxDuration(int kind, int b);   // 그 기믹 연출의 총 길이 ms
void DrawGimmickFx(HDC dc);
// 철문이 아직 안 내려왔으면 1. 잠금 표시를 그때까지 미루는 데 쓴다.
int GimmickLockPending(int slot);

// ---- 전투 시각 이벤트 재생 -------------------------------------------------
// game.cpp가 남긴 CombatFxEvent를 계산 줄 번호에 맞춰 되짚는다.
//   eventStart = gTurnTraceStart + traceLine × TURN_TRACE_STEP_MS
// 화면은 CombatFxElapsed만 읽어 모든 위치·강도를 경과 시간의 순수 함수로 낸다.
int CombatFxPlaying();          // 지금 이벤트가 흐르고 있는가
int CombatFxElapsed(int index); // 그 이벤트 시작 이후 ms (아직 안 왔으면 -1)
// 재생 중 화면에 보일 내 체력. 아직 닿지 않은 타격의 결과를 미리 보여 주지 않는다.
int PlayerDisplayHp();
// 소리와 적의 달려들기를 그 사건의 줄에 맞춰 한 번씩 발동한다.
void SyncCombatFx();
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
RECT FxLevelRect(int index);
RECT StartButtonRect(int width, int height);
RECT DriveCardRect(int i);
RECT DirectoryChoiceRect(int i);
RECT EnemyRect(int i);
RECT SlotRect(int i);
RECT DieRect(int i);
RECT EndTurnRect();
RECT ReadButtonRect();
RECT RewardRect(int i, int width);
RECT FaceGridRect(int die, int face);
RECT ContinueRect(int width, int height);
RECT KeybButtonRect();
RECT TurnTraceTickerRect();
RECT TurnTracePanelRect();
RECT PruneTsrRect(int i);

int DieForSlotUI(int slot);
int CanRepairSector();

// ---- 화면 -----------------------------------------------------------------
void ApplyFullscreen(int enable);
void ApplyWindowedScale(int percent);
void PaintGame(HWND window);
