#pragma once

#include <stdint.h>
#include "data.h"

enum GamePhase {
    PHASE_TITLE = 0,
    PHASE_DRIVE_SELECT,
    PHASE_DIRECTORY,
    PHASE_COMBAT,
    PHASE_REWARD,
    PHASE_PRUNE,
    PHASE_GAMEOVER,
    PHASE_VICTORY
};

// X:\ 격리 상태. FaceCost와 kind·value·damaged는 그대로 두고 출력만 0이 된다.
#define QUAR_NONE 0
#define QUAR_COMBAT 255   // 전투가 끝날 때까지 격리 (SAMPLE-13)
// 1..254 = 남은 턴 수. 보스 턴 종료마다 1씩 줄고 0이 되면 해제 (SANDBOX.BREACH)

// 정리(PHASE_PRUNE)가 끝난 뒤 어디로 돌아갈지. 예전에는 boolean 하나였지만
// 디렉터리 선택이 생기면서 복귀 지점이 셋으로 늘었다.
enum PendingContinuation {
    CONTINUE_NONE = 0,
    CONTINUE_AFTER_REWARD,   // 보상 처리 흐름을 다시 탄다
    CONTINUE_DIRECTORY,      // 새 층의 디렉터리 선택으로 간다
    CONTINUE_COMBAT          // 예정된 전투를 바로 시작한다 (레거시 경로)
};

// 제시된 디렉터리 카드 하나. payload는 노드마다 뜻이 다르다.
//   CORRUPTED : 격리할 면 (die * 6 + face)
//   그 외     : 사용하지 않음
struct DirectoryChoice {
    uint8_t kind;        // DirectoryNodeKind
    uint8_t payload;
    uint8_t revealed;    // LOGS로 숨은 정보가 공개된 상태인가
    uint8_t reserved;
};

// 경로 선택 런타임. 경로 문자열은 저장하지 않고 kind만 남긴다.
struct DirectoryRuntime {
    uint32_t rng;                          // 주사위·보상과 분리된 전용 난수열
    DirectoryChoice choices[DIRECTORY_CHOICE_COUNT];
    uint8_t history[3][DIRECTORY_PER_FLOOR];  // 층별 방문 노드 (경로 문자열 조합용)
    uint8_t floorCounts[DIR_NODE_COUNT];   // 이번 층 등장 수 (층당 제한 검사)
    uint8_t choiceCount;
    uint8_t previousKind;                  // 직전에 고른 노드 (연속 선택 금지)
    uint8_t activeKind;                    // 이번 전투에 걸린 노드
    uint8_t activePayload;
    uint8_t intelThisFloor;                // LOGS로 이번 층 정보가 열렸는가
    uint8_t pad[3];
    int floorCapacityBonus;                // CACHE의 임시 한도 (층 이동 시 해제)
};

struct Face {
    uint8_t kind;
    uint8_t value;
    uint8_t damaged;
    uint8_t quarantined;   // QUAR_* — 임시 격리 상태 (영구 삭제는 kind=EMPTY)
};

struct DieState {
    Face faces[6];
    uint8_t rolledFace;
    int8_t assignedSlot;
    uint8_t disabled;      // 조각화 (ApplyFragmentation이 매턴 초기화)
    uint8_t unstable;      // 읽기 오류
    uint8_t offline;       // E:\ 보스 기믹 — 이번 턴 출력 0, 다음 턴 자동 복구
    uint8_t pad[3];
};

struct EnemyState {
    uint8_t kind;
    uint8_t intent;
    uint8_t alive;
    uint8_t burn;
    int hp;
    int maxHp;
    int block;
    int intentValue;
};

// 보스 기믹 런타임. 한 번에 보스는 한 마리라 GameState에 공용으로 둔다.
// 전투 시작 시 초기화되고 전투 종료 경로에서 임시 효과가 전부 정리된다.
struct BossRuntime {
    uint8_t gimmick;                     // GIMMICK_NONE이면 이번 전투에 기믹 없음
    uint8_t reversed;                    // 이번 턴 해결 순서 역전
    uint8_t nextReversed;                // 다음 턴 역전 예고
    uint8_t empowered;                   // 이번 턴 강화 공격 발동 (R:\ 압력)
    uint8_t lockedSlot[SLOT_COUNT];      // 이번 턴 잠긴 슬롯
    uint8_t nextLockedSlot[SLOT_COUNT];  // 다음 턴 잠금 예고
    int8_t offlineDie;                   // 이번 턴 오프라인 주사위 (-1 없음)
    int8_t nextOfflineDie;               // 다음 턴 오프라인 예고 (-1 없음)
    int8_t bestSlotLastTurn;             // 직전 턴 최고 출력 슬롯 (KERNEL.PANIC)
    int8_t nextTargetDie;                // 격리·삭제 예고 대상 (-1 없음)
    int8_t nextTargetFace;
    uint8_t nextTargetPermanent;         // 1 = ZERO.DAY 영구 삭제 예고
    // 발동 연출 기록. 규칙에는 전혀 관여하지 않고 화면이 읽기만 하므로
    // 스모크·밸런스의 결정론은 그대로다 (타격 연출 기록과 같은 규약).
    uint8_t firedFx;                     // 이번 실행에서 발동한 기믹 (GIMMICK_NONE = 없음)
    int8_t fxA;                          // 대상 1: 슬롯·주사위·면 인덱스 또는 수치 (-1 없음)
    int8_t fxB;                          // 대상 2: 면 인덱스나 보조 플래그 (-1 없음)
    int8_t fxEnemy;                      // 대상 적 (-1 없음). 복원 연출이 카드를 찾는 데 쓴다
    int16_t fxHpBefore;                  // 복원 전 체력 (되감기 잔상의 출발점)
    int16_t fxHpAfter;                   // 복원 후 체력
    int gauge;                           // 압력·오염 게이지
    int gaugeMax;
    int countdown;                       // N:\ TIMEOUT 카운트다운
    int damageThisTurn;                  // 이번 턴 보스가 받은 피해
    int windowDamage;                    // D:\ 복원 창 누적 피해
    int checkpointHp;                    // 복원 지점
    int restoresUsed;
    int restoredTotal;                   // TAPE.LOOP 총 회복량 (상한 검사용)
    int quarantinesDone;                 // SAMPLE-13 격리 횟수 (최대 p2)
};

// 턴 계산 추적: 내부는 12줄까지 기록하고 화면은 최근 8줄을 보여준다.
#define TURN_TRACE_CAP 12
#define TURN_TRACE_SHOWN 8

// ---------------------------------------------------------------------------
// 전투 시각 이벤트. 계산 문자열을 되짚는 대신, 규칙이 이미 확정한 사건을 그대로
// 남긴다. 화면은 이 기록을 읽기만 하고 규칙·난수에는 전혀 관여하지 않으므로
// 스모크·밸런스의 결정론은 변하지 않는다 (기믹 발동 기록과 같은 규약).
// ---------------------------------------------------------------------------
enum CombatFxType {
    CFX_NONE = 0,
    CFX_AMPLIFY,        // 증폭 슬롯이 보너스를 만들었다 (소실이면 CFXF_WASTED)
    CFX_ATTACK_LAUNCH,  // 공격 슬롯이 신호를 쏘았다
    CFX_ENEMY_HIT,      // 그 신호가 적에게 닿았다
    CFX_DEFEND,         // 방어도를 얻었다
    CFX_CHAIN,          // 연쇄가 공격이나 방어를 반복했다
    CFX_BURN,           // 화상 피해
    CFX_ENEMY_STRIKE    // 적이 나를 때렸다
};

enum CombatFxFlags {
    CFXF_NONE         = 0,
    CFXF_KILL         = 1 << 0,   // 이 사건으로 대상이 삭제됐다
    CFXF_BIG_HIT      = 1 << 1,   // 큰 타격 (처치 / 10 이상 / 최대 체력 20% 이상)
    CFXF_BLOCKED      = 1 << 2,   // 방어도가 전부 받아내 체력 피해가 0이었다
    CFXF_CORRUPT      = 1 << 3,   // 관통 공격 (방어도를 절반만 인정)
    CFXF_EMPOWERED    = 1 << 4,   // 보스 강화 공격
    CFXF_DEFEND_CHAIN = 1 << 5,   // 연쇄가 공격이 아니라 방어를 반복했다
    CFXF_WASTED       = 1 << 6    // 역전으로 증폭 보너스가 도착하지 못했다
};

// value/beforeValue/afterValue의 뜻은 type마다 다르다.
//   CFX_AMPLIFY        value = 실제 보너스 (소실이면 0, beforeValue = 잃은 양)
//   CFX_ATTACK_LAUNCH  value = 발사한 공격 피해
//   CFX_ENEMY_HIT      value = 실제 체력 피해, before/after = 피격 전후 HP
//   CFX_DEFEND         value = 얻은 방어도, before/after = 방어도 전후
//   CFX_CHAIN          공격 반복이면 CFX_ENEMY_HIT과 같고, 방어 반복이면 CFX_DEFEND과 같다
//   CFX_BURN           value = 실제 체력 피해, before/after = 피격 전후 HP
//   CFX_ENEMY_STRIKE   value = 내가 잃은 체력, before/after = 내 체력 전후
struct CombatFxEvent {
    uint8_t type;         // CombatFxType
    uint8_t traceLine;    // 이 사건이 적힌 계산 줄 번호 (>= turnTraceCount면 재생 끝에 몰아 발동)
    int8_t sourceSlot;    // 원인 슬롯 (-1 없음)
    int8_t sourceDie;     // 그때 그 슬롯에 있던 주사위 (-1 없음). 다음 턴이면 배치가 초기화된다
    int8_t targetEnemy;   // 대상 적 (-1 없음). CFX_ENEMY_STRIKE는 때린 적
    uint8_t flags;        // CombatFxFlags
    int16_t value;
    int16_t beforeValue;
    int16_t afterValue;
};

#define COMBAT_FX_CAP 16

struct GameState {
    GamePhase phase;
    uint32_t rng;
    int floor;
    int encounter;
    int turn;
    int playerHp;
    int playerMaxHp;
    int playerBlock;
    int targetEnemy;
    int selectedDie;
    int enemyCount;
    EnemyState enemies[3];
    DieState dice[3];
    int modifierA;
    int modifierB;
    int driveChoices[3];
    int driveDifficulty[3];       // 카드별 난이도 (세 장 모두 다름)
    int selectedDrive;
    int difficulty;               // 마운트한 볼륨의 난이도 (마운트 전 -1)
    int mobSchedule[6];           // 일반전 6회의 등장 순서 (드라이브 확정 시 셔플)
    int mobScheduleReady;
    BossRuntime boss;
    int rewardKinds[3];
    int rewardValues[3];
    int selectedReward;
    int rewardIsTsr;              // 1 = 보스 보상: 카드 0~2가 면이 아니라 TSR
    uint8_t tsrInstalled[TSR_COUNT];
    uint8_t keybUsedThisTurn;
    uint8_t tsrReserved[2];
    // 판독한 적. 처치한 종류만 1이 되고 가이드의 노이즈가 걷힌다.
    // 런 단위로만 유지된다 (NewRun의 ZeroMemory가 초기화).
    uint8_t enemyScanned[ENEMY_KIND_COUNT];
    int lastDamage;
    int lastBlock;
    int lastTurnDamageDealt;
    int lastTurnDamageTaken;
    int lastTurnBlockGained;
    int lastTurnSlotOutput[SLOT_COUNT];   // 슬롯별 산출량 (미리보기·UI 표기)
    int lastTurnReversed;         // 직전 해결이 역전 순서였는지 (UI 표기)
    // 타격 연출용 기록. 규칙에는 전혀 관여하지 않고 화면이 읽기만 한다.
    CombatFxEvent combatFx[COMBAT_FX_CAP];
    uint8_t combatFxCount;
    uint8_t combatFxOverflow;            // 16개를 넘겨 기록이 버려졌으면 1 (회귀 검사용)
    uint8_t combatFxPad[2];
    int turnTraceCount;
    int turnTraceOverflow;        // 12줄을 넘겨 기록이 버려졌으면 1 (회귀 검사용)
    wchar_t turnTrace[TURN_TRACE_CAP][96];
    int combatsWon;
    int facesInstalled;
    int sectorsRepaired;
    int tsrsInstalled;
    int pendingContinuation;      // PendingContinuation — 정리 후 복귀 지점
    DirectoryRuntime directory;
    int rewardChoiceCount;        // 이번 면 보상의 후보 수 (TEMP면 2)
    int rewardTier;               // 0 = 표준, 1 = 강화
    wchar_t logs[5][96];
};

// 실행 전 미리보기. 상태 사본에서 EndTurn을 돌려 숫자만 읽으므로 원본은 변하지 않고
// 규칙에도 관여하지 않는다. 읽기 오류가 걸린 주사위는 실행 순간 다시 굴러가므로
// 그때는 uncertain이 서고, 미리보기 값은 확정이 아니라 현재 굴림 기준의 예상이다.
struct TurnPreview {
    int valid;                    // 배치된 주사위가 없으면 0
    int damageDealt;              // 적이 잃을 체력
    int damageTaken;              // 내가 잃을 체력
    int blockGained;              // 이번 턴 얻는 방어도
    int slotOutput[SLOT_COUNT];   // 슬롯별 산출량
    int slotUnknown[SLOT_COUNT];  // 읽기 오류로 확정할 수 없는 슬롯 (미리보기는 ? 로 표시)
    int combatEnds;               // 이 배치로 적이 전멸하는가
    int playerDies;               // 이 배치로 내가 쓰러지는가
    int uncertain;                // 읽기 오류로 확정할 수 없음
};
void PreviewTurn(const GameState* game, TurnPreview* out);

void InitTitle(GameState* game);
void NewRun(GameState* game, uint32_t seed);
void SelectDrive(GameState* game, int choiceIndex);
// 다음 일반전 앞의 디렉터리 2택을 생성하고 PHASE_DIRECTORY로 들어간다.
void BeginDirectorySelection(GameState* game);
// 유효하지 않은 index/phase면 상태를 전혀 바꾸지 않는다 (repaint·Esc 재생성 금지).
void SelectDirectoryChoice(GameState* game, int index);
int DirectoryChoiceCount(const GameState* game);
const DirectoryNodeInfo* DirectoryNodeInfoOrNull(int kind);
// 노드가 지금 등장할 수 있는가. 플레이어 상태는 유효성만 결정하고 확률은 보정하지 않는다.
int DirectoryNodeAllowed(const GameState* game, int kind);
// 이번 층에서 지금까지 지나온 경로. "C:\WINDOWS\TEMP\INFECTED" 형태로 조합한다.
void FormatCurrentDirectory(const GameState* game, wchar_t* out, int cap);
// 다음 일반전에 나올 몹 (보스 구역이면 -1).
int ScheduledMobKind(const GameState* game);
// 이번 층 보스 종류.
int FloorBossKind(const GameState* game);
// 카드의 적 코드를 공개해도 되는가 (판독 완료 또는 LOGS 활성).
int DirectoryIntelActive(const GameState* game);
// 실패(유효하지 않은 드라이브) 시 적을 만들지 않고 드라이브 선택으로 복귀하며 0을 반환.
int StartCombat(GameState* game);
// 잠긴 슬롯 등으로 배치가 거부되면 0을 반환한다 (효과음 분기용).
int AssignDieToSlot(GameState* game, int dieIndex, int slotIndex);
void UnassignDie(GameState* game, int dieIndex);
void SelectEnemy(GameState* game, int enemyIndex);
void EndTurn(GameState* game);

// ---- 디버그 ---------------------------------------------------------------
// 관리자 터미널(`)의 win 전용. 규칙 경로를 우회하지 않고 살아 있는 적을 전부
// 눕힌 뒤 정상 승리 처리를 그대로 탄다. 스모크·밸런스는 이 함수를 부르지 않으므로
// 자동 검증의 결정론에는 아무 영향이 없다.
void DebugWinCombat(GameState* game);
void SelectReward(GameState* game, int rewardIndex);
void InstallSelectedReward(GameState* game, int dieIndex, int faceIndex);
void InstallTsr(GameState* game, int rewardIndex);
void RepairSector(GameState* game);
void SkipReward(GameState* game);
void PruneFace(GameState* game, int dieIndex, int faceIndex);
void UninstallTsr(GameState* game, int tsrIndex);
void ConfirmPrune(GameState* game);
void KeybReroll(GameState* game, int dieIndex);

// 테스트 전용: 드라이브와 일반전 순서만 준비한다. 전투를 시작하지 않고,
// preserveModifiers가 1이면 테스트가 주입한 modifierA/B를 그대로 둔다.
void ConfigureDriveForTest(GameState* game, int drive, uint32_t scheduleSeed, int preserveModifiers);

int FaceCost(const Face* face);
int FacePower(const Face* face);
int DeckBytes(const GameState* game);
int TsrBytes(const GameState* game);
int UsedBytes(const GameState* game);
int IsTsrInstalled(const GameState* game, int tsr);
int InstalledTsrCount(const GameState* game);
int InstalledTsrAt(const GameState* game, int slot);
int EffectiveCapacity(const GameState* game);
int SectorRepairAmount(const GameState* game);
int NonEmptyFaceCount(const GameState* game);
int UsableFaceCount(const GameState* game);   // FacePower ≥ 1로 실제 출력 가능한 면 수
int IsModifierActive(const GameState* game, int modifier);
// 난이도가 정해지지 않았으면(테스트 경로) DIFFICULTY_BASE_PERCENT를 돌려준다.
int CorruptPercent(const GameState* game);
int ScaleCorruptDamage(const GameState* game, int damage);
const DifficultyInfo* DifficultyInfoOrNull(int difficulty);
int LivingEnemyCount(const GameState* game);
const Face* RolledFace(const GameState* game, int dieIndex);

// ---- 안전 조회와 역할 판정 (enum 범위 비교 금지) ---------------------------
const EnemyInfo* GetEnemyInfoOrUnknown(int kind);
int IsBossKind(int kind);
int IsValidEnemyKind(int kind);
// 이번 런에서 처치해 본 적인가. 가이드가 정보를 드러낼지 판정한다.
int IsEnemyScanned(const GameState* game, int kind);

// ---- 보스 기믹 상태 질의 (UI·휴리스틱 공용) --------------------------------
int SlotLockedThisTurn(const GameState* game, int slot);
int SlotLockedNextTurn(const GameState* game, int slot);
int ResolveOrderReversed(const GameState* game);
