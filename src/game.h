#pragma once

#include <stdint.h>
#include "data.h"

enum GamePhase {
    PHASE_TITLE = 0,
    PHASE_DRIVE_SELECT,
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
    int lastDamage;
    int lastBlock;
    int lastTurnDamageDealt;
    int lastTurnDamageTaken;
    int lastTurnBlockGained;
    int lastTurnSlotOutput[SLOT_COUNT];   // 슬롯별 산출량 (미리보기·UI 표기)
    int lastTurnReversed;         // 직전 해결이 역전 순서였는지 (UI 표기)
    // 타격 연출용 기록. 규칙에는 전혀 관여하지 않고 화면이 읽기만 하므로
    // 스모크·밸런스의 결정론은 그대로다.
    uint8_t lastTurnEnemyStruck[3];      // 이번 실행에서 나를 때린 적 (막혀서 피해 0이어도 1)
    int lastTurnEnemyStrikeDamage[3];    // 방어도를 뚫고 들어온 피해
    int lastTurnEnemyStrikeTrace[3];     // 그 행동이 적힌 계산 줄 번호 (-1 = 없음)
    int hasTurnResult;
    int turnTraceCount;
    int turnTraceOverflow;        // 12줄을 넘겨 기록이 버려졌으면 1 (회귀 검사용)
    wchar_t turnTrace[TURN_TRACE_CAP][96];
    int combatsWon;
    int facesInstalled;
    int sectorsRepaired;
    int tsrsInstalled;
    int pruneAdvancePending;
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
    int combatEnds;               // 이 배치로 적이 전멸하는가
    int playerDies;               // 이 배치로 내가 쓰러지는가
    int uncertain;                // 읽기 오류로 확정할 수 없음
};
void PreviewTurn(const GameState* game, TurnPreview* out);

void InitTitle(GameState* game);
void NewRun(GameState* game, uint32_t seed);
void SelectDrive(GameState* game, int choiceIndex);
// 실패(유효하지 않은 드라이브) 시 적을 만들지 않고 드라이브 선택으로 복귀하며 0을 반환.
int StartCombat(GameState* game);
// 잠긴 슬롯 등으로 배치가 거부되면 0을 반환한다 (효과음 분기용).
int AssignDieToSlot(GameState* game, int dieIndex, int slotIndex);
void UnassignDie(GameState* game, int dieIndex);
void SelectEnemy(GameState* game, int enemyIndex);
void EndTurn(GameState* game);
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

// ---- 보스 기믹 상태 질의 (UI·휴리스틱 공용) --------------------------------
int SlotLockedThisTurn(const GameState* game, int slot);
int SlotLockedNextTurn(const GameState* game, int slot);
int ResolveOrderReversed(const GameState* game);
