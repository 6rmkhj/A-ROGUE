#pragma once

#include <windows.h>
#include "game.h"
#include "render.h"
#include "fx_timing.h"

// 화면(screens.cpp)과 입력·창 관리(main.cpp)가 함께 쓰는 상태와 레이아웃.
// 좌표는 전부 render.h의 고정 캔버스(BASE_WIDTH x BASE_HEIGHT) 기준이다.

#define SETTINGS_SCALE_COUNT 5
static const int SCALE_OPTIONS[SETTINGS_SCALE_COUNT] = {75, 100, 125, 150, 200};

// 소리 크기 슬라이더. 손잡이 너비만큼 이동 구간이 줄어들므로 값↔좌표 변환을
// 한 곳에 모아 두고 그리기와 클릭 판정이 같은 식을 보게 한다.
#define VOL_HANDLE_W 16

// 카드 0~2는 설치할 면, 마지막 카드는 면 대신 체력을 얻는 섹터 복구다.
#define REWARD_CARD_COUNT 4
#define REWARD_REPAIR 3

#define COMBAT_CLEAR_MS 1500
#define DESCENT_LOCK_MS 520    // 고른 드라이브 카드가 잠기고 나머지가 밀려나는 구간
#define DESCENT_MS 2920        // 카드 잠금 + 실제 마운트/심층 스캔 전체 길이
#define DIR_SELECT_LOCK_MS 360 // 고른 디렉터리 카드가 경로로 수렴하는 구간
#define DIR_ENTER_MS 1460      // 카드 잠금 + 디렉터리 라우팅/진입 전체 길이
#define NOISE_CHURN_MS 45      // 노이즈가 다시 섞이는 주기

// 연출 타이머의 주기. 60fps를 노리고 16으로 두면 안 된다 - WM_TIMER는 시스템
// 틱(기본 15.6ms) 경계에서만 깨어나므로, 16ms짜리는 매번 다음 틱까지 밀려 두
// 틱에 한 번씩 온다. 실제로 재 보니 삽입 연출이 3.5초에 131프레임(37fps)이었고,
// 같은 연출을 8ms로 걸었더니 234프레임(66fps)이 나왔다. 틱마다 한 번 깨어나되
// 실제 속도는 그리기가 정한다 (WM_TIMER는 밀린 만큼 쌓이지 않는다).
#define FX_TIMER_MS 8

// ---- 새 게임 삽입 연출 -----------------------------------------------------
// 새 게임을 누르면 지금 화면이 먼저 갈라지고, 소용돌이에 감겨 빨려 들어가 플로피
// 한 장이 되고, 그 디스크가 공중에서 한 바퀴 뒤집힌 뒤 책상 위 컴퓨터의 3.5인치
// 드라이브에 꽂힌다. 구간 경계는 그리기와 소리·흔들림이 같은 값을 봐야 하므로
// 여기 모아 둔다.
#define BOOT_GLITCH_MS 380     // 판이 띠로 어긋나고 고리가 조여 온다
#define BOOT_SUCK_MS   820     // 화면이 세 바퀴 돌며 디스크 라벨로 빨려 들어간다
#define BOOT_FLIP_MS   480     // 만들어진 디스크가 공중에서 한 바퀴 뒤집힌다
#define BOOT_FLY_MS    320     // 디스크가 슬롯 앞으로 내려온다
#define BOOT_PUSH_MS   360     // 슬롯 안으로 밀려 들어간다 (중간에 한 번 걸린다)
#define BOOT_SEEK_MS   820     // 드라이브가 읽는다 (LED 점멸·신호 전송)
#define BOOT_ZOOM_MS   520     // 기계 전체가 덮쳐 오고 그 화면 속으로 들어간다
#define BOOT_SUCK_AT   BOOT_GLITCH_MS
#define BOOT_FLIP_AT   (BOOT_SUCK_AT + BOOT_SUCK_MS)   // 디스크 한 장이 완성되는 순간
#define BOOT_FLY_AT    (BOOT_FLIP_AT + BOOT_FLIP_MS)
#define BOOT_PUSH_AT   (BOOT_FLY_AT + BOOT_FLY_MS)
#define BOOT_CLUNK_AT  (BOOT_PUSH_AT + BOOT_PUSH_MS)   // 다 들어가 철컥 물리는 순간
#define BOOT_SEEK_END  (BOOT_CLUNK_AT + BOOT_SEEK_MS)
#define BOOT_INSERT_MS (BOOT_SEEK_END + BOOT_ZOOM_MS)

// ---- 보스 조우 연출 --------------------------------------------------------
// 일반전 앞에는 디렉터리 2택과 진입 연출이 있지만 보스 구역에는 둘 다 없다.
// 두 번째 일반전의 보상을 고르면 판이 곧장 보스전으로 갈렸다 - 이 게임에서
// 가장 큰 사건이 카드 한 장 바뀌는 것으로 끝났다는 뜻이다. 디렉터리 화면이
// 내내 "LOCKED DESTINATION ...\<BOSS>"로 가리켜 온 그 자리를 여기서 연다.
//   경보   잠긴 목적지의 마지막 조각이 실제 코드로 풀린다
//   게이트 그 경로를 막고 있던 철문이 좌우로 갈라진다
//   강림   열린 틈에서 보스가 걸어 나와 바닥을 딛는다 (충격파·흔들림)
//   명패   코드·수치·기믹이 박히고 명패가 걷히며 전투판이 열린다
// 구간 경계는 그리기와 소리·흔들림이 같은 값을 봐야 하므로 여기 모아 둔다.
#define BOSS_ALERT_MS  680     // 경보가 올라오고 목적지가 판독된다
#define BOSS_GATE_MS   760     // 잠금이 풀리고 문짝이 갈라진다
#define BOSS_RISE_MS   860     // 보스가 앞으로 나와 바닥을 딛는다
#define BOSS_NAME_MS   800     // 명패가 박히고 기믹 도장이 찍힌다
#define BOSS_HAND_MS   420     // 명패가 좌우로 걷히며 전투판을 내보낸다
#define BOSS_GATE_AT   BOSS_ALERT_MS
#define BOSS_RISE_AT   (BOSS_GATE_AT + BOSS_GATE_MS)
#define BOSS_LAND_AT   (BOSS_RISE_AT + 520)            // 발이 바닥에 닿는 순간
#define BOSS_NAME_AT   (BOSS_RISE_AT + BOSS_RISE_MS)
#define BOSS_HAND_AT   (BOSS_NAME_AT + BOSS_NAME_MS)
#define BOSS_INTRO_MS  (BOSS_HAND_AT + BOSS_HAND_MS)

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

// ---- 직접 조작 피드백 ------------------------------------------------------
// 클릭 결과는 게임 상태에 즉시 반영하고, 그 직전/직후 위치만 짧게 기록해 화면이
// 경과 시간의 순수 함수로 재생한다. 보상은 처리 직후 화면이 바뀌므로 마지막 보상
// 화면 스냅샷을 잠깐 유지하고, 주사위 배치·정리는 현재 화면 위에서 바로 재생한다.
enum UiFxKind {
    UIFX_NONE = 0,
    UIFX_DIE_PLACE,
    UIFX_DIE_MOVE,
    UIFX_DIE_REMOVE,
    UIFX_REWARD_FACE,
    UIFX_REWARD_TSR,
    UIFX_REWARD_REPAIR,
    UIFX_PRUNE_DELETE,
    UIFX_PRUNE_RESTORE
};

struct UiFxState {
    int kind;
    DWORD start;
    int die, face;
    int displacedDie;
    int fromSlot, toSlot;
    int rewardIndex;
    int valueBefore, valueAfter;
    Face shownFace;
};

extern UiFxState gUiFx;
int UiFxElapsed();
int UiFxSnapshotActive();
void CaptureUiFxSnapshot();
void DrawUiInteractionFx(HDC dc);

// ---- 공유 상태 (main.cpp가 소유한다) --------------------------------------
extern GameState gGame;
extern HWND gWindow;
extern POINT gMouse;
extern int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
extern uint8_t gPruneTsrPending[TSR_COUNT];
extern int gGuidePage;   // 0 = 공통 규칙, 1 = 현재 드라이브·보스 기믹
extern int gRestartArmed; // 설정 화면의 "다시 시작" 버튼: 0=대기, 1=한 번 더 누르면 확정
extern int gCampaignResetArmed; // "진행도 초기화" 버튼. 런이 아니라 세이브를 지우므로 확정을 따로 받는다
// 보상 포기 버튼. 되돌릴 수 없는 손실이라 "다시 시작"과 같은 두 번 누르기를 쓴다.
// 보상 화면을 벗어나거나 카드를 새로 고르면 0으로 풀린다.
extern int gRewardSkipArmed;
// 마지막 캠페인 세이브가 실패했으면 1. 한 번 서면 그 실행 동안 유지되고,
// 다음 저장이 성공하면 다시 0으로 내려간다.
extern int gSaveFailed;
extern int gSettingsSaveFailed;
extern int gCampaignCorrupt;

// 되돌릴 수 없는 선택을 고르는 단계와 확정하는 단계로 나눈다. 각 값은 아직
// 확정하지 않은 후보의 번호이고, -1은 "고른 것 없음"이다. 첫 입력은 후보를
// 세우고 카드에 확정 문구를 띄우기만 하며, 같은 후보에 한 번 더 와야 실제로
// 적용된다. 다른 후보를 누르면 그쪽으로 옮겨 가고 취소 키로 풀린다.
extern int gDirectoryArmed;   // 디렉터리 카드
extern int gTsrArmed;         // 보스 전리품 카드
extern int gFaceSwapArmed;    // 보상 면을 덮어쓸 기존 면 (die * 6 + face)
extern int gEndingArmed;      // 최종 명령 카드

// 주사위 판독 연출
extern int gReadActive, gRolled;
// 전투 종료·턴 계산·볼륨 진입 연출
extern int gCombatClearActive, gClearedFloor, gClearedEncounter;
extern DWORD gCombatClearStart;
extern int gTurnTraceActive;
extern DWORD gTurnTraceStart;
extern int gDescentActive, gDescentToFloor;
extern DWORD gDescentStart;
extern int gDescentChoiceIndex; // 최초 마운트 때 고른 카드 (층 하강이면 -1)
// 디렉터리 진입: 고른 경로 조각이 타이핑되는 짧은 오버레이
extern int gDirEnterActive, gDirEnterKind, gDirEnterChoiceIndex;
extern DWORD gDirEnterStart;
// 보스 조우: 잠긴 목적지가 열리고 보스가 걸어 나오는 동안. 판은 이미 보스전
// 상태다 (StartCombat이 먼저 끝나 있다) - 그래서 언제 건너뛰어도 결과가 같다.
extern int gBossIntroActive;
extern DWORD gBossIntroStart;
// 새 게임: 화면이 디스크로 빨려 들어가 드라이브에 꽂힐 때까지. 이 연출이 도는
// 동안 판은 아직 누르기 직전 그대로다 (런은 연출이 끝날 때 만들어진다).
extern int gBootActive;
extern DWORD gBootStart;
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
int SceneElapsed(); // visible phase/page entrance, presentation state only
int UiFocusElapsed(); // ms on the current actionable hover target, -1 when absent
// 가이드 2페이지에 아직 미판독 칸이 남아 있는가. 남아 있으면 가이드가 열려 있는
// 동안에도 리페인트를 계속 돌려야 노이즈가 멈추지 않는다.
int GuideNoiseActive();

// 계산 재생에서 지금까지 드러난 줄 수 (0 = 아직 없음)
int TurnTraceShown();
const DieState* DisplayDie(int index); // 재생 중에는 실행 직전의 주사위·배치를 보존

// ---- 기믹 발동 연출 --------------------------------------------------------
// 계산 재생이 끝나 새 턴 화면이 드러나는 순간 시작된다. 규칙은 이미 game.cpp에서
// 확정된 뒤이므로 여기서는 보여 주는 방식만 정한다.
int GimmickFxKind();        // 재생 중인 기믹 (GIMMICK_NONE = 없음)
int GimmickFxElapsed();     // 시작으로부터 경과 ms
int GimmickFxA();           // 대상 1 (슬롯·주사위·면 또는 수치)
int GimmickFxB();           // 대상 2
int GimmickFxDuration(int kind, int b);   // 그 기믹 연출의 총 길이 ms
int GimmickFxImpactAt(int kind, int b);   // 히트스톱이 걸리는 시점 ms (0 = 없음)
void DrawGimmickFx(HDC dc);
// 철문이 아직 안 내려왔으면 1. 잠금 표시를 그때까지 미루는 데 쓴다.
int GimmickLockPending(int slot);
// 소환된 카드가 아직 격리막 안에 있으면 1. 계산 재생 중과 연출의 임팩트 전까지는
// 카드를 그리지도, 클릭하지도, 세지도 않는다. 규칙은 이미 소환을 끝냈지만 화면에서는
// 막이 깨지는 순간에 나타나야 연출이 사건이 된다.
int GimmickSummonPending(int enemyIndex);

// ---- 전투 시각 이벤트 재생 -------------------------------------------------
// game.cpp가 남긴 CombatFxEvent를 계산 줄 번호에 맞춰 되짚는다.
//   eventStart = gTurnTraceStart + FxTraceAt(gGame, traceLine)
// 화면은 CombatFxElapsed만 읽어 모든 위치·강도를 경과 시간의 순수 함수로 낸다.
int CombatFxPlaying();          // 지금 이벤트가 흐르고 있는가
int CombatFxElapsed(int index); // 그 이벤트 시작 이후 ms (아직 안 왔으면 -1)
int CombatFxLeadElapsed(int index, int leadMs); // 결과 전 신호 이동만 미리 재생
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

// ---- 관리자 터미널 (디버그) ------------------------------------------------
// `(백틱)으로 열고 닫는다. 보스까지 가는 데 걸리는 시간을 줄이려고 넣은 개발용
// 창이라 규칙에는 관여하지 않는다. 명령이 부르는 것은 전부 정규 규칙 함수다.
// 커서를 깜빡이지 않으므로 리페인트를 따로 돌릴 필요가 없다.
//
// 배포 빌드에서는 열리지 않는다. AROGUE_DEV로 빌드했거나 실행 인자에 -dev가
// 있을 때만 gDevMode가 서고, 그때만 백틱이 먹는다. 일반 플레이어가 실수로
// 승리 명령을 눌러 캠페인 기록을 망치는 길을 아예 없앤다.
extern int gDevMode;
#define TERM_LOG_LINES 10
#define TERM_LOG_CAP   72
#define TERM_INPUT_MAX 40
extern int gTermOpen;
extern wchar_t gTermLog[TERM_LOG_LINES][TERM_LOG_CAP];
extern int gTermLogCount;
extern wchar_t gTermInput[TERM_INPUT_MAX + 1];
extern int gTermInputLen;
void DrawTerminal(HDC dc, int width, int height);

// 새 게임 삽입 연출. 붙잡아 둔 판을 돌려 얹으므로 캔버스의 실제 픽셀 크기가 필요하다.
void DrawBootInsert(HDC dc, int width, int height, int deviceW, int deviceH);
void DrawBossIntro(HDC dc, int width, int height);

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
RECT LanguageOptionRect(int index);
enum AudioVolumeChannel { AUDIO_VOLUME_MASTER = 0, AUDIO_VOLUME_BGM, AUDIO_VOLUME_SFX, AUDIO_VOLUME_COUNT };
RECT VolumeSliderRect(int channel);
RECT VolumeHandleRect(int channel, int volume);
int VolumeFromX(int channel, int x);          // 슬라이더 위 x좌표를 0~100으로
RECT FullscreenToggleRect();
RECT BgmToggleRect();
RECT RestartButtonRect();
RECT CampaignResetRect();
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
// 스토리 화면의 [다음]. 패널 아무 곳이나 눌러 넘어가지 않도록 진행 입력을
// 이 버튼 하나로 좁힌다 (엔터·스페이스는 그대로 받는다).
RECT StoryNextRect(int width, int height);
RECT EndingChoiceRect(int index);
// 최종 명령의 확정 버튼. 카드 선택과 실행을 갈라 놓는다.
RECT EndingConfirmRect();
RECT EndingRestartRect();
RECT KeybButtonRect();
RECT TurnTraceTickerRect();
RECT TurnTracePanelRect();
RECT PruneTsrRect(int i);

int DieForSlotUI(int slot);
int CanRepairSector();
int VictoryElapsed();

// ---- 화면 -----------------------------------------------------------------
void ApplyFullscreen(int enable);
void ApplyWindowedScale(int percent);
// 지금 적용된 창 배율(%). 설정 저장이 화면 상태가 아니라 값을 읽어야 한다.
int WindowedScale();
void PaintGame(HWND window);
// 페인트 계측. 터미널 perf 명령이 읽는다.
int PaintLastMs();
int PaintMaxMs();
int PaintCount();
