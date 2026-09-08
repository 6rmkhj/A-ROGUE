#pragma once

#include <stdint.h>

#define AR_COLOR(r, g, b) ((uint32_t)((r) | ((g) << 8) | ((b) << 16)))

enum FaceKind {
    FACE_NUMBER = 0,
    FACE_FIRE,
    FACE_SHIELD,
    FACE_LEECH,
    FACE_WILD,
    FACE_BOOST,
    FACE_ECHO,
    FACE_EMPTY,
    FACE_KIND_COUNT
};

struct FaceInfo {
    const wchar_t* name;
    const wchar_t* shortName;
    const wchar_t* description;
    int cost;
    int power;
    uint32_t color;
};

// 강화 보상(INFECTED / CORRUPTED)으로 나온 특수 면은 비용은 그대로인 채 출력만 이만큼 높다.
// 숫자 면은 값이 곧 비용이라 이 보너스를 받지 않는다.
#define TUNED_FACE_BONUS 2

static const FaceInfo FACE_INFO[FACE_KIND_COUNT] = {
    {L"숫자", L"숫자", L"값만큼 효과, 값만큼 바이트", 0, 0, AR_COLOR(205, 221, 232)},
    {L"화염", L"화염", L"공격 시 추가 피해와 화상", 20, 8, AR_COLOR(255, 92, 72)},
    {L"방벽", L"방벽", L"방어 슬롯에서 효과 2배", 15, 7, AR_COLOR(76, 170, 255)},
    {L"흡수", L"흡수", L"공격 피해 일부를 회복", 18, 6, AR_COLOR(182, 96, 220)},
    {L"와일드", L"와일드", L"어느 슬롯에서도 높은 출력", 26, 9, AR_COLOR(255, 205, 64)},
    {L"증폭", L"증폭", L"증폭 슬롯에서 보너스 2배", 15, 5, AR_COLOR(95, 225, 176)},
    {L"메아리", L"메아리", L"연쇄 슬롯에서 이번 턴 공격/방어 효과 반복", 17, 5, AR_COLOR(255, 139, 209)},
    {L"빈 면", L"----", L"효과와 비용이 모두 0", 0, 0, AR_COLOR(55, 63, 73)}
};

enum SlotKind {
    SLOT_ATTACK = 0,
    SLOT_DEFEND,
    SLOT_AMPLIFY,
    SLOT_CHAIN,
    SLOT_COUNT
};

static const wchar_t* const SLOT_NAMES[SLOT_COUNT] = {L"공격", L"방어", L"증폭", L"연쇄"};
static const wchar_t* const SLOT_SHORT_NAMES[SLOT_COUNT] = {L"공격", L"방어", L"증폭", L"연쇄"};
static const wchar_t* const SLOT_DESCRIPTIONS[SLOT_COUNT] = {
    L"대상에게 피해를 줍니다.",
    L"이번 턴의 적 피해를 막습니다.",
    L"공격과 방어 수치를 먼저 강화합니다.",
    L"공격 또는 방어 효과를 반복합니다."
};

enum ModifierKind {
    MOD_BAD_SECTOR = 0,
    MOD_READ_ERROR,
    MOD_FRAGMENTATION,
    MOD_OVERALLOC,
    MOD_CHECKSUM,
    MODIFIER_COUNT
};

struct ModifierInfo {
    const wchar_t* name;
    const wchar_t* description;
};

static const ModifierInfo MODIFIER_INFO[MODIFIER_COUNT] = {
    {L"배드 섹터", L"층을 내려갈 때 무작위 면 1개가 영구 손상됩니다. 복구 불가."},
    {L"읽기 오류", L"경고된 주사위가 턴 확정 순간 다시 굴러갑니다."},
    {L"조각화", L"같은 결과가 여러 개면 뒤쪽 결과가 비활성화됩니다."},
    {L"과잉 할당", L"층 용량 +60B, 모든 적 최대 체력 +30%."},
    {L"체크섬", L"굴림 출력 합이 짝수면 공격 피해 +2."}
};

// ---------------------------------------------------------------------------
// 적 데이터
//
// 기존 11종(레거시)은 마이그레이션 호환용으로만 유지되며 DRIVE_MOBS /
// DRIVE_BOSSES 어디에서도 참조되지 않는다. 활성 로스터는 드라이브별
// 일반 몹 3종 + 층별 보스 3종, 총 42종이다. 보스 여부는 enum 범위가
// 아니라 EnemyInfo.role로만 판정한다.
// ---------------------------------------------------------------------------

enum EnemyKind {
    // 레거시 (활성 로스터 미참조, 데이터 무결성 검사만 통과하면 됨)
    ENEMY_GLITCH = 0,
    ENEMY_WORM,
    ENEMY_SPYWARE,
    ENEMY_TROJAN,
    ENEMY_FRAGMENT,
    ENEMY_CACHE,
    ENEMY_DAEMON,
    ENEMY_ROOTKIT,
    BOSS_DISK_ERROR,
    BOSS_BOOT_SECTOR,
    BOSS_FORMAT,
    // C:\ SYSTEM
    MOB_C_DLL_HIJACKER,
    MOB_C_REG_GHOST,
    MOB_C_WATCHDOG,
    BOSS_C_ACCESS_DENIED,
    BOSS_C_KERNEL_PANIC,
    BOSS_C_BLUE_SCREEN,
    // D:\ ARCHIVE
    MOB_D_BIT_ROT,
    MOB_D_INDEXER,
    MOB_D_ZIP_BOMB,
    BOSS_D_RESTORE_EXE,
    BOSS_D_TAPE_LOOP,
    BOSS_D_MASTER_BACKUP,
    // E:\ REMOVABLE
    MOB_E_AUTORUN,
    MOB_E_LOST_CLUSTER,
    MOB_E_WRITE_PROTECT,
    BOSS_E_AUTOPLAY,
    BOSS_E_UNSAFE_EJECT,
    BOSS_E_NO_MEDIA,
    // N:\ NETWORK
    MOB_N_SNIFFER,
    MOB_N_FIREWALL,
    MOB_N_PING_FLOOD,
    BOSS_N_PROXY,
    BOSS_N_ROUTING_LOOP,
    BOSS_N_TIMEOUT,
    // R:\ RAMDISK
    MOB_R_MEMORY_LEAK,
    MOB_R_RACE_CONDITION,
    MOB_R_DANGLING_PTR,
    BOSS_R_LEAK_DLL,
    BOSS_R_HEAP_OVERFLOW,
    BOSS_R_OUT_OF_MEMORY,
    // X:\ QUARANTINE
    MOB_X_MUTANT_SAMPLE,
    MOB_X_ESCAPEE,
    MOB_X_RANSOMWARE,
    BOSS_X_SAMPLE13,
    BOSS_X_SANDBOX_BREACH,
    BOSS_X_ZERO_DAY,
    MOB_A_FALSE_COPY,
    MOB_A_HALF_WRITE,
    MOB_A_ECHO_PROC,
    BOSS_A_SIGNATURE,
    BOSS_A_SEVENTEENTH,
    BOSS_A_LAST_WRITE,
    ENEMY_KIND_COUNT
};

enum EnemyRole {
    ROLE_MOB = 0,
    ROLE_BOSS
};

// 일반 몹의 의도 주기. 새 몹을 추가할 때 game.cpp에 종류별 비교문을
// 늘리는 대신 여기서 패턴을 골라 붙인다.
enum EnemyPattern {
    PATTERN_LEGACY = 0,  // 기존 8종 몹의 하드코딩 주기 재현
    PATTERN_BOSS,        // 보스 공용 주기 (기믹과는 별개)
    PATTERN_ASSAULT,     // 공격형: 공격 → 공격 → 강공 → 공격
    PATTERN_CORRUPTER,   // 변칙형: 오염 → 공격 → 공격 → 강공
    PATTERN_BULWARK,     // 방어형: 방어 → 공격 → 방어 → 강공
    PATTERN_MEDIC,       // 방어형: 공격 → 복구 → 방어 → 공격
    PATTERN_OPENER,      // 변칙형: 1턴 강공, 이후 공격 → 방어 → 공격
    PATTERN_RAMP,        // 공격형: 매턴 공격, 피해가 턴마다 +1
    PATTERN_ERRATIC,     // 변칙형: 턴 해시 기반 의도
    PATTERN_SIEGE,       // 방어형: 방어 → 방어 → 강공 → 공격
    PATTERN_SPIKE,       // 변칙형: 강공 → 오염 → 공격 → 공격
    PATTERN_COUNT
};

// 보스 기믹 계열(드라이브 테마)과 실제 행동(보스별 고유 18종).
enum GimmickFamily {
    FAM_NONE = 0,
    FAM_LOCK,        // C:\ 슬롯 권한 잠금
    FAM_RESTORE,     // D:\ 복원 지점 되감기
    FAM_OFFLINE,     // E:\ 주사위 연결 끊김
    FAM_ROUTE,       // N:\ 해결 순서 변형
    FAM_PRESSURE,    // R:\ 메모리 압력 게이지
    FAM_QUARANTINE   // X:\ 면 격리·영구 포맷
};

enum BossGimmickKind {
    GIMMICK_NONE = 0,        // 레거시 보스 전용 센티널
    GIMMICK_ACCESS_DENIED,   // 짝수 턴마다 예고된 슬롯 1개 잠금
    GIMMICK_KERNEL_PANIC,    // 직전 턴 최고 출력 슬롯이 다음 턴 잠김
    GIMMICK_BLUE_SCREEN,     // 3턴마다 증폭+연쇄 동시 잠금
    GIMMICK_RESTORE_POINT,   // 3턴 창 요구 피해 미달 시 체크포인트 복원
    GIMMICK_TAPE_LOOP,       // 매턴 요구 피해 미달 시 되감기 회복
    GIMMICK_MASTER_BACKUP,   // 체력 40% 미만 시 1회 대복원
    GIMMICK_AUTOPLAY,        // 3턴마다 예고 주사위 오프라인
    GIMMICK_UNSAFE_EJECT,    // 짝수 턴마다 예고 주사위 오프라인
    GIMMICK_NO_MEDIA,        // 매턴 오프라인, 4턴마다 인식 휴지
    GIMMICK_PROXY,           // 3턴마다 해결 순서 역전
    GIMMICK_ROUTING_LOOP,    // 짝수 턴마다 해결 순서 역전
    GIMMICK_TIMEOUT,         // 카운트다운 0 턴 역전+보스 대기, 피해로 지연
    GIMMICK_LEAK,            // 압력 4, 임계 10 피해 → -1
    GIMMICK_HEAP_OVERFLOW,   // 압력 3, 임계 12, 강화 공격이 방어 관통
    GIMMICK_OUT_OF_MEMORY,   // 압력 5, 임계 14 → -2
    GIMMICK_SAMPLE13,        // 3턴마다 예고 면을 전투 동안 격리 (최대 2)
    GIMMICK_SANDBOX_BREACH,  // 3턴마다 검체 1마리 탈주 (동시 최대 2)
    GIMMICK_ZERO_DAY,        // 4턴마다 예고 면 영구 삭제, 피해로 지연
    GIMMICK_SIGNATURE,
    GIMMICK_SEVENTEENTH,
    GIMMICK_LAST_WRITE,
    GIMMICK_COUNT
};

struct BossGimmickInfo {
    uint8_t family;          // GimmickFamily
    const wchar_t* name;     // 카드·가이드 표시명
    const wchar_t* rule;     // 규칙 한 줄
    const wchar_t* counter;  // 대응 한 줄
    const wchar_t* stamp;    // 발동 연출에 크게 박히는 영문 표식
    int p1, p2, p3;          // 기믹별 매개변수 (주기·임계·강도)
};

// 탈주체의 출력 비율. 체력은 기믹 매개변수 p3가, 화력은 이 값이 정한다.
// 본체 몹은 보스와 맞먹는 피해를 내므로 소환체로 쓰려면 두 축을 함께 깎아야 한다.
#define BREACH_MINION_POWER 45

static const BossGimmickInfo BOSS_GIMMICK_INFO[GIMMICK_COUNT] = {
    {FAM_NONE, L"-", L"-", L"-", L"-", 0, 0, 0},
    {FAM_LOCK,       L"섹터 잠금",     L"짝수 턴마다 예고된 슬롯 1개가 잠깁니다.",                     L"예고를 보고 남은 슬롯 배치를 계획하십시오.", L"ACCESS DENIED",          2, 0, 0},
    {FAM_LOCK,       L"패닉 잠금",     L"직전 턴 출력이 가장 컸던 슬롯이 다음 턴 잠깁니다.",           L"매턴 주력 슬롯을 바꿔 잠금을 분산하십시오.", L"KERNEL PANIC",          1, 0, 0},
    {FAM_LOCK,       L"시스템 정지",   L"3턴마다 증폭과 연쇄 슬롯이 함께 잠깁니다.",                   L"정지 턴에는 공격·방어에만 집중하십시오.", L"FATAL EXCEPTION",             3, 0, 0},
    {FAM_RESTORE,    L"복원 지점",     L"3턴 창의 누적 피해가 요구치 미달이면 체력을 되감습니다.",     L"창이 닫히기 전에 요구 피해를 채우십시오.", L"RESTORE POINT",            3, 12, 10},
    {FAM_RESTORE,    L"테이프 루프",   L"한 턴 피해가 요구치 미달이면 턴말에 체력을 되감습니다.",      L"매턴 요구치 이상을 꾸준히 넣으십시오.", L"REWIND",               1, 7, 5},
    {FAM_RESTORE,    L"마스터 백업",   L"체력 40% 미만이 되면 1회 백업 지점으로 복원합니다.",         L"임계 근처에서 한 번에 크게 몰아치십시오.", L"MASTER BACKUP",            40, 60, 14},
    {FAM_OFFLINE,    L"자동 실행",     L"3턴마다 예고된 주사위 1개가 그 턴 오프라인이 됩니다.",        L"오프라인 주사위에 핵심 역할을 맡기지 마십시오.", L"NO SIGNAL",      3, 0, 0},
    {FAM_OFFLINE,    L"강제 제거",     L"짝수 턴마다 예고된 주사위 1개가 오프라인이 됩니다.",          L"홀수 턴에 화력을 몰고 짝수 턴은 수비하십시오.", L"DEVICE REMOVED",       2, 0, 0},
    {FAM_OFFLINE,    L"미디어 없음",   L"매턴 주사위 1개가 오프라인, 4턴마다 인식 턴엔 없습니다.",     L"인식 턴에 최대 화력을 준비하십시오.", L"NO MEDIA",                 1, 4, 0},
    {FAM_ROUTE,      L"프록시 우회",   L"3턴마다 슬롯 해결 순서가 역전됩니다.",                        L"역전 턴에는 공격·방어에만 배치하십시오.", L"REROUTED",             3, 0, 0},
    {FAM_ROUTE,      L"라우팅 루프",   L"짝수 턴마다 슬롯 해결 순서가 역전됩니다.",                    L"홀수 턴에 증폭·연쇄를 쓰십시오.", L"ROUTING LOOP",                     2, 0, 0},
    {FAM_ROUTE,      L"타임아웃",      L"카운트가 0이 되는 턴 순서가 역전되고 보스는 대기합니다.",     L"한 턴 12+ 피해로 카운트를 되돌릴 수 있습니다.", L"TIMEOUT",       3, 12, 0},
    {FAM_PRESSURE,   L"메모리 누수",   L"압력이 매턴 오르고 가득 차면 강화 공격이 예고됩니다.",        L"한 턴 10+ 피해로 압력을 1 낮추십시오.", L"MEMORY LEAK",               4, 10, 8},
    {FAM_PRESSURE,   L"힙 오버플로",   L"압력이 빠르게 차고 강화 공격이 방어를 관통합니다.",           L"한 턴 12+ 피해로 압력을 1 낮추십시오.", L"HEAP OVERFLOW",               3, 12, 6},
    {FAM_PRESSURE,   L"메모리 고갈",   L"압력 상한이 높지만 가득 차면 최대 강화 공격이 옵니다.",       L"한 턴 14+ 피해로 압력을 2 낮추십시오.", L"OUT OF MEMORY",               5, 14, 12},
    {FAM_QUARANTINE, L"검체 격리",     L"오염이 차면 예고된 면 1개를 전투 동안 격리합니다(최대 2).",   L"격리 전에 처치하거나 예고 면 의존을 줄이십시오.", L"QUARANTINED",     3, 2, 0},
    {FAM_QUARANTINE, L"샌드박스 침입", L"3턴마다 검체가 탈주합니다(동시 1마리).",                      L"탈주체를 정리할지 보스를 끊을지 고르십시오.", L"CONTAINMENT LOST",       3, 1, 45},
    {FAM_QUARANTINE, L"제로데이",      L"오염이 가득 차면 예고된 면 1개를 영구 삭제합니다.",           L"한 턴 15+ 피해로 오염을 1 낮추십시오.", L"DATA DESTROYED",               4, 15, 0},
    {FAM_LOCK, L"원본 서명", L"표시된 슬롯은 짝수 출력 면만 통과합니다. 나머지는 출력 0.", L"짝수 면을 배치하거나 다른 슬롯을 사용하십시오.", L"SIGNATURE CHECK", 2, 0, 0},
    {FAM_RESTORE, L"열일곱 번째 사본", L"직전 실행의 최고 슬롯 출력을 다음 기본 공격에 더합니다.", L"복제될 출력을 확인하고 방어를 준비하십시오.", L"OUTPUT COPIED", 0, 0, 0},
    {FAM_QUARANTINE, L"마지막 쓰기", L"4턴마다 증폭·연쇄·방어 순으로 슬롯을 영구 봉인합니다.", L"한 턴 보스 피해 12+로 카운트다운을 멈추십시오. 공격은 보존됩니다.", L"SLOT SEALED", 4, 12, 0}
};

// ---------------------------------------------------------------------------
// 몹 특성 (몹 기믹)
//
// 보스 기믹이 "턴마다 일어나는 사건"이라면 몹 특성은 "전투 내내 참인 성질"이다.
// 몹은 2~4턴에 죽으므로 게이지가 차는 구조가 발동할 틈이 없고, 런당 6번 나오므로
// 놀라움보다 학습이 값어치다. 그래서 예고 없이 카드에 항상 적혀 있다.
//
// 상태는 BossRuntime이 아니라 EnemyState에 개체별로 둔다. BossRuntime은 판에
// 하나뿐이라 SANDBOX.BREACH가 적을 둘로 만들면 두 마리가 같은 칸을 밟는다.
//
// 네 유형으로 나누고 볼륨마다 겹치지 않게 배분했다.
//   카운터  눈에 보이는 숫자. 내 선택이 깎는다. 0이면 사건
//   거래    "~하면 ~한다". 뺏지 않고 값을 치르게 한다
//   맞붙음  내 눈과 적 숫자를 직접 비교한다
//   처형    죽이는 방법·순서·타이밍이 조건이 된다
// ---------------------------------------------------------------------------

enum EnemyTrait {
    TRAIT_NONE = 0,
    TRAIT_INTERCEPT,    // C 가로채기 : 증폭을 쓸 때마다 감소, 0이면 증폭이 적 방어도로
    TRAIT_REGISTRY,     // C 레지스트리 : 매 턴 방어 +2, 공격 눈이 홀수면 그 턴은 안 올림
    TRAIT_CLASH,        // C 감시 필터 : 눈이 의도값보다 커야 온전, 아니면 절반
    TRAIT_DECAY,        // D 부패 : 매 턴 감소, 0이면 면 손상. 한 턴 6+ 피해면 1 회복
    TRAIT_INDEX,        // D 블록 색인 : 4 이상만 온전, 3 이하 절반
    TRAIT_BOMB,         // D 압축 해제 : 처치 시 6 피해. 방어도 6 이상이면 막는다
    TRAIT_FIRST,        // E 자동 실행 : 1턴에 플레이어보다 먼저 행동
    TRAIT_LOSS,         // E 유실 : 처치 시 면 1개 전투 격리. 연쇄를 채웠으면 면제
    TRAIT_NOREPEAT,     // E 쓰기 방지 : 직전 턴과 같은 눈이면 절반
    TRAIT_SNIFF,        // N 도청 : 매 턴 감소, 0이면 최고 눈을 복사해 그 값으로 공격
    TRAIT_ODDONLY,      // N 포트 필터 : 홀수만 온전, 짝수 절반
    TRAIT_FLOOD,        // N 폭주 : 매 턴 피해 +1. 방어에 4 이상을 넣은 턴엔 안 오름
    TRAIT_LEAKLOW,      // R 누수 : 3 이하 피해가 +3
    TRAIT_TWOINTENT,    // R 경쟁 상태 : 의도 둘, 공격 눈이 짝수면 왼쪽 홀수면 오른쪽
    TRAIT_DANGLING,     // R 허상 참조 : 처치되어도 그 턴 행동은 실행
    TRAIT_MUTATE,       // X 변이 : 맞을 때마다 약점이 홀 <-> 짝으로 뒤집힌다
    TRAIT_FLEE,         // X 탈주 : 체력 30% 이하면 다음 턴 도망 (보상 없음)
    TRAIT_ENCRYPT,      // X 암호화 : 한 턴 8 미만이면 회복. 8+ 한 방 처치면 보상 +1
    TRAIT_COPY,         // A 거짓 사본 : 직전 턴과 같은 눈이면 2배
    TRAIT_INCOMPLETE,   // A 미완성 : 매 턴 감소, 0이면 절반 회복. 짝수 눈이면 2 감소
    TRAIT_ECHO,         // A 반향 : 직전 턴 준 피해 절반을 되돌림. 방어를 비웠으면 면제
    TRAIT_COUNT
};

struct EnemyTraitInfo {
    const wchar_t* badge;   // 적 카드에 찍히는 짧은 이름
    const wchar_t* rule;    // 규칙 한 줄 (가이드·카드 아래)
    uint8_t usesCounter;    // 1이면 카드에 카운터 숫자를 함께 보여 준다
    int p1, p2;             // 특성별 매개변수
};

static const EnemyTraitInfo ENEMY_TRAIT_INFO[TRAIT_COUNT] = {
    {L"", L"", 0, 0, 0},
    {L"가로채기", L"증폭 쓰면 1 감소. 0이면 증폭을 뺏깁니다.", 1, 3, 0},
    {L"레지스트리", L"매 턴 방어 +2. 공격 눈이 홀수면 면제.", 0, 2, 0},
    {L"감시 필터", L"공격 눈이 의도값보다 커야 온전합니다.", 0, 0, 0},
    {L"부패", L"매 턴 감소. 0이면 면 손상. 6+로 회복.", 1, 4, 6},
    {L"블록 색인", L"4 이상만 온전. 3 이하는 절반.", 0, 4, 0},
    {L"압축 해제", L"처치 시 6 피해. 방어도 6이면 막습니다.", 0, 6, 6},
    {L"자동 실행", L"1턴 공격은 방어도를 무시합니다.", 0, 0, 0},
    {L"유실", L"처치 시 면 격리. 연쇄를 채우면 면제.", 0, 0, 0},
    {L"쓰기 방지", L"직전 턴과 같은 눈이면 절반.", 0, 0, 0},
    {L"도청", L"매 턴 감소. 0이면 내 최고 눈을 복사.", 1, 3, 0},
    {L"포트 필터", L"홀수만 온전. 짝수는 절반.", 0, 0, 0},
    {L"폭주", L"매 턴 피해 +1. 방어 4+면 안 오릅니다.", 1, 1, 4},
    {L"누수", L"3 이하 피해가 +3.", 0, 3, 3},
    {L"경쟁 상태", L"공격 눈 짝수면 왼쪽, 홀수면 오른쪽.", 0, 0, 0},
    {L"허상 참조", L"처치되어도 그 턴 행동은 실행.", 0, 0, 0},
    {L"변이", L"맞을 때마다 약점이 홀/짝 뒤집힘.", 0, 0, 0},
    {L"탈주", L"체력 30% 이하면 다음 턴 도망.", 0, 30, 0},
    {L"암호화", L"한 턴 8+ 못 넣으면 회복.", 0, 8, 0},
    {L"거짓 사본", L"직전 턴과 같은 눈이면 2배.", 0, 0, 0},
    {L"미완성", L"매 턴 감소. 0이면 절반 회복.", 1, 4, 0},
    {L"반향", L"준 피해 절반 반사. 방어 비우면 면제.", 0, 0, 0}
};

struct EnemyInfo {
    const wchar_t* name;    // 기록·로그용 이름
    const wchar_t* code;    // 카드 표시명
    int hp;
    int damage;
    int guard;
    int hpGrowth;           // 층당 증가량 (보스는 층별 종류가 달라 0)
    int damageGrowth;
    int guardGrowth;
    uint8_t role;           // EnemyRole
    uint8_t pattern;        // EnemyPattern
    uint8_t gimmick;        // BossGimmickKind (몹과 레거시 보스는 GIMMICK_NONE)
    uint8_t trait;          // EnemyTrait (보스와 레거시는 TRAIT_NONE)
    uint32_t color;
};

static const EnemyInfo ENEMY_INFO[ENEMY_KIND_COUNT] = {
    // ---- 레거시 11종 (활성 로스터 미참조) ----
    {L"글리치", L"글리치", 14, 4, 3, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(95, 225, 176)},
    {L"웜", L"웜", 18, 5, 2, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(170, 230, 80)},
    {L"스파이웨어", L"스파이웨어", 22, 6, 4, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(255, 193, 77)},
    {L"트로이 목마", L"트로이 목마", 25, 7, 3, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(255, 110, 95)},
    {L"파편", L"파편", 28, 7, 6, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(150, 150, 255)},
    {L"오염 캐시", L"오염 캐시", 24, 6, 7, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(90, 190, 230)},
    {L"데몬", L"데몬", 30, 8, 5, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(210, 105, 235)},
    {L"루트킷", L"루트킷", 34, 9, 5, 3, 1, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(235, 80, 130)},
    {L"디스크 오류", L"디스크 오류", 38, 6, 5, 3, 1, 2, ROLE_BOSS, PATTERN_BOSS, GIMMICK_NONE, AR_COLOR(255, 104, 87)},
    {L"부트 섹터", L"부트 섹터", 52, 8, 7, 3, 1, 2, ROLE_BOSS, PATTERN_BOSS, GIMMICK_NONE, AR_COLOR(255, 170, 70)},
    {L"포맷", L"포맷", 68, 10, 9, 3, 1, 2, ROLE_BOSS, PATTERN_BOSS, GIMMICK_NONE, AR_COLOR(245, 65, 90)},
    // ---- C:\ SYSTEM ----
    {L"DLL 하이재커", L"DLL.HIJACK", 16, 5, 2, 7, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, TRAIT_INTERCEPT, AR_COLOR(120, 190, 255)},
    {L"레지스트리 고스트", L"REG.GHOST", 18, 4, 4, 7, 1, 1, ROLE_MOB, PATTERN_MEDIC, GIMMICK_NONE, TRAIT_REGISTRY, AR_COLOR(150, 200, 250)},
    {L"워치독 서비스", L"WATCHDOG", 20, 5, 4, 8, 1, 1, ROLE_MOB, PATTERN_BULWARK, GIMMICK_NONE, TRAIT_CLASH, AR_COLOR(80, 150, 235)},
    {L"액세스 거부", L"ACCESS.DENIED", 34, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_ACCESS_DENIED, TRAIT_NONE, AR_COLOR(96, 168, 255)},
    {L"커널 패닉", L"KERNEL.PANIC", 48, 7, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_KERNEL_PANIC, TRAIT_NONE, AR_COLOR(70, 140, 245)},
    {L"블루 스크린", L"BLUE.SCREEN", 62, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_BLUE_SCREEN, TRAIT_NONE, AR_COLOR(58, 122, 240)},
    // ---- D:\ ARCHIVE ----
    {L"비트 부패", L"BIT.ROT", 15, 4, 3, 6, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, TRAIT_DECAY, AR_COLOR(235, 190, 90)},
    {L"인덱서", L"INDEXER", 21, 4, 6, 8, 1, 1, ROLE_MOB, PATTERN_SIEGE, GIMMICK_NONE, TRAIT_INDEX, AR_COLOR(255, 214, 110)},
    {L"집 폭탄", L"ZIP.BOMB", 16, 6, 1, 6, 1, 0, ROLE_MOB, PATTERN_ASSAULT, GIMMICK_NONE, TRAIT_BOMB, AR_COLOR(255, 180, 55)},
    {L"복원 프로그램", L"RESTORE.EXE", 36, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_RESTORE_POINT, TRAIT_NONE, AR_COLOR(255, 208, 96)},
    {L"테이프 루프", L"TAPE.LOOP", 36, 7, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_TAPE_LOOP, TRAIT_NONE, AR_COLOR(238, 186, 70)},
    {L"마스터 백업", L"MASTER.BACKUP", 46, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_MASTER_BACKUP, TRAIT_NONE, AR_COLOR(220, 165, 52)},
    // ---- E:\ REMOVABLE ----
    {L"오토런", L"AUTORUN.INF", 15, 5, 2, 6, 1, 1, ROLE_MOB, PATTERN_OPENER, GIMMICK_NONE, TRAIT_FIRST, AR_COLOR(120, 230, 150)},
    {L"유실 클러스터", L"LOST.CLUSTER", 18, 4, 3, 7, 1, 1, ROLE_MOB, PATTERN_MEDIC, GIMMICK_NONE, TRAIT_LOSS, AR_COLOR(96, 210, 176)},
    {L"쓰기 방지", L"WRITE.PROTECT", 20, 4, 6, 8, 1, 1, ROLE_MOB, PATTERN_BULWARK, GIMMICK_NONE, TRAIT_NOREPEAT, AR_COLOR(78, 190, 140)},
    {L"자동 재생", L"AUTOPLAY", 35, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_AUTOPLAY, TRAIT_NONE, AR_COLOR(110, 235, 168)},
    {L"강제 제거", L"UNSAFE.EJECT", 50, 7, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_UNSAFE_EJECT, TRAIT_NONE, AR_COLOR(84, 216, 150)},
    {L"미디어 없음", L"NO.MEDIA", 55, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_NO_MEDIA, TRAIT_NONE, AR_COLOR(60, 196, 128)},
    // ---- N:\ NETWORK ----
    {L"패킷 스니퍼", L"SNIFFER", 15, 4, 3, 6, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, TRAIT_SNIFF, AR_COLOR(110, 205, 240)},
    {L"방화벽", L"FIREWALL", 22, 3, 7, 8, 1, 1, ROLE_MOB, PATTERN_SIEGE, GIMMICK_NONE, TRAIT_ODDONLY, AR_COLOR(90, 190, 230)},
    {L"핑 폭주", L"PING.FLOOD", 16, 5, 1, 6, 1, 0, ROLE_MOB, PATTERN_ASSAULT, GIMMICK_NONE, TRAIT_FLOOD, AR_COLOR(70, 220, 255)},
    {L"프록시", L"PROXY", 40, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_PROXY, TRAIT_NONE, AR_COLOR(96, 200, 245)},
    {L"라우팅 루프", L"ROUTING.LOOP", 56, 7, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_ROUTING_LOOP, TRAIT_NONE, AR_COLOR(72, 180, 235)},
    {L"타임아웃", L"TIMEOUT", 68, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_TIMEOUT, TRAIT_NONE, AR_COLOR(52, 160, 225)},
    // ---- R:\ RAMDISK ----
    {L"메모리 누수", L"MEM.LEAK", 17, 3, 2, 7, 1, 1, ROLE_MOB, PATTERN_RAMP, GIMMICK_NONE, TRAIT_LEAKLOW, AR_COLOR(220, 130, 245)},
    {L"경쟁 상태", L"RACE.COND", 16, 5, 3, 6, 1, 1, ROLE_MOB, PATTERN_ERRATIC, GIMMICK_NONE, TRAIT_TWOINTENT, AR_COLOR(200, 110, 230)},
    {L"허상 포인터", L"DANGLING.PTR", 15, 5, 2, 6, 1, 1, ROLE_MOB, PATTERN_SPIKE, GIMMICK_NONE, TRAIT_DANGLING, AR_COLOR(235, 96, 220)},
    {L"누수 라이브러리", L"LEAK.DLL", 30, 5, 3, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_LEAK, TRAIT_NONE, AR_COLOR(214, 118, 240)},
    {L"힙 오버플로", L"HEAP.OVERFLOW", 44, 6, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_HEAP_OVERFLOW, TRAIT_NONE, AR_COLOR(192, 92, 226)},
    {L"메모리 고갈", L"OUT.OF.MEMORY", 55, 8, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_OUT_OF_MEMORY, TRAIT_NONE, AR_COLOR(170, 70, 212)},
    // ---- X:\ QUARANTINE ----
    {L"변이 샘플", L"MUTANT.SMP", 16, 5, 2, 7, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, TRAIT_MUTATE, AR_COLOR(255, 120, 108)},
    {L"샌드박스 탈주", L"ESCAPEE", 17, 6, 2, 7, 1, 0, ROLE_MOB, PATTERN_ASSAULT, GIMMICK_NONE, TRAIT_FLEE, AR_COLOR(255, 96, 130)},
    {L"랜섬웨어", L"RANSOMWARE", 20, 4, 5, 8, 1, 1, ROLE_MOB, PATTERN_MEDIC, GIMMICK_NONE, TRAIT_ENCRYPT, AR_COLOR(230, 70, 96)},
    {L"검체-13", L"SAMPLE-13", 36, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_SAMPLE13, TRAIT_NONE, AR_COLOR(255, 104, 92)},
    {L"샌드박스 침입", L"SANDBOX.BREACH", 50, 7, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_SANDBOX_BREACH, TRAIT_NONE, AR_COLOR(240, 80, 78)},
    {L"제로데이", L"ZERO.DAY", 60, 9, 8, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_ZERO_DAY, TRAIT_NONE, AR_COLOR(255, 56, 66)},
    {L"거짓 사본", L"FALSE.COPY", 18, 5, 3, 7, 1, 1, ROLE_MOB, PATTERN_ERRATIC, GIMMICK_NONE, TRAIT_COPY, AR_COLOR(90, 235, 190)},
    {L"미완성 쓰기", L"HALF.WRITE", 20, 4, 5, 8, 1, 1, ROLE_MOB, PATTERN_BULWARK, GIMMICK_NONE, TRAIT_INCOMPLETE, AR_COLOR(125, 210, 245)},
    {L"반향 프로세스", L"ECHO.PROC", 17, 5, 3, 7, 1, 1, ROLE_MOB, PATTERN_RAMP, GIMMICK_NONE, TRAIT_ECHO, AR_COLOR(190, 150, 245)},
    {L"원본 서명", L"SIGNATURE", 44, 6, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_SIGNATURE, TRAIT_NONE, AR_COLOR(90, 235, 190)},
    {L"열일곱 번째", L"SEVENTEENTH", 48, 4, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_SEVENTEENTH, TRAIT_NONE, AR_COLOR(140, 210, 245)},
    {L"마지막 쓰기", L"LAST.WRITE", 74, 8, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_LAST_WRITE, TRAIT_NONE, AR_COLOR(235, 150, 205)}
};

// 잘못된 kind가 UI·렌더에 흘러들었을 때 대신 그리는 안전 데이터.
static const EnemyInfo UNKNOWN_ENEMY_INFO =
    {L"알 수 없음", L"UNKNOWN", 1, 0, 0, 0, 0, 0, ROLE_MOB, PATTERN_LEGACY, GIMMICK_NONE, AR_COLOR(255, 0, 255)};

enum EnemyIntent {
    INTENT_ATTACK = 0,
    INTENT_HEAVY,
    INTENT_GUARD,
    INTENT_REPAIR,
    INTENT_CORRUPT,
    INTENT_COUNT
};

static const wchar_t* const INTENT_NAMES[INTENT_COUNT] = {L"공격", L"강공", L"방어", L"복구", L"오염(관통)"};
static const int FLOOR_CAPACITY[3] = {240, 180, 130};

// 보상 대신 선택하는 섹터 복구의 회복량. 층이 깊어질수록 피해가 커지므로 함께 오른다.
static const int SECTOR_REPAIR_HEAL[3] = {10, 13, 16};

// 상주 프로그램(TSR): 보스 처치 보상으로만 설치되며, 면과 같은 용량 풀을
// 나눠 쓰면서 상시 효과를 낸다. 층이 내려가 한도가 조여들면 정리 화면에서
// 면처럼 제거(언인스톨)할 수 있다.
enum TsrKind {
    TSR_HIMEM = 0,   // 용량 한도 +45B
    TSR_DEFRAG,      // 조각화 무효
    TSR_SCANDISK,    // 층 하강 시 배드 섹터 손상 무효
    TSR_UNDELETE,    // 전투 승리 시 체력 회복
    TSR_SMARTDRV,    // 전투 첫 턴 방어도
    TSR_KEYB,        // 턴마다 한 번 선택한 주사위 재굴림
    TSR_COUNT
};

struct TsrInfo {
    const wchar_t* name;
    const wchar_t* description; // 카드/패널 한 줄 요약
    int cost;                   // 바이트 (면과 같은 용량 풀)
    int value;                  // 효과 수치
    int counters;               // 대항하는 ModifierKind, -1 = 항상 유효
    uint32_t color;
};

static const TsrInfo TSR_INFO[TSR_COUNT] = {
    {L"HIMEM.SYS", L"용량 한도 +45B",                 20, 45, -1,                AR_COLOR(255, 204, 75)},
    {L"DEFRAG",    L"조각화 비활성을 무효화",          22,  0, MOD_FRAGMENTATION, AR_COLOR(83, 170, 255)},
    {L"SCANDISK",  L"층 하강 시 배드 섹터 손상 무효",  24,  0, MOD_BAD_SECTOR,    AR_COLOR(95, 225, 176)},
    {L"UNDELETE",  L"전투 승리 시 체력 6 회복",        26,  6, -1,                AR_COLOR(182, 96, 220)},
    {L"SMARTDRV",  L"전투 첫 턴 방어도 +6",            28,  6, -1,                AR_COLOR(90, 190, 230)},
    {L"KEYB",      L"턴마다 한 번 주사위 재굴림 [K]",  30,  1, -1,                AR_COLOR(255, 139, 209)}
};

enum DrivePerk {
    PERK_MAX_HP = 0,     // 시작 최대 체력 증감 (perkValue = 증감량)
    PERK_CAPACITY,       // 모든 층 용량 한도 가산 (perkValue = 바이트)
    PERK_HEAL_ON_WIN,    // 전투 승리 시 회복 (perkValue = 회복량)
    PERK_ENEMY_HP_DOWN,  // 적 최대 체력 감소 (perkValue = %)
    PERK_ATTACK_UP,      // 공격 피해 가산 (perkValue = 피해, 최대 체력 -4 동반)
    PERK_BONUS_FACE      // 마운트 시 무작위 특수 면 1개 설치
};

struct DriveInfo {
    const wchar_t* letter;      // 예: L"C:\\"
    const wchar_t* label;       // 볼륨명
    const wchar_t* description;
    int modifierA, modifierB;   // 이 볼륨에서 활성화되는 디스크 손상 2종
    int perk;                   // DrivePerk
    int perkValue;
    const wchar_t* perkText;    // 카드/연출 표시용 요약
    const wchar_t* paths[3];    // 층별 현재 경로
    const wchar_t* pathPreview; // 카드에 보여줄 탐색 경로 요약
    uint32_t color;
};

#define DRIVE_SELECTABLE_COUNT 6
#define DRIVE_FINAL DRIVE_SELECTABLE_COUNT
#define DRIVE_COUNT 7

static const DriveInfo DRIVE_INFO[DRIVE_COUNT] = {
    {L"C:\\", L"SYSTEM", L"기본 시스템 볼륨. 전원부가 안정적이라 코어 무결성이 높습니다.",
     MOD_BAD_SECTOR, MOD_CHECKSUM, PERK_MAX_HP, 6, L"시작 최대 체력 +6",
     {L"C:\\", L"C:\\WINDOWS", L"C:\\WINDOWS\\SYSTEM32"}, L"C:\\ → WINDOWS → SYSTEM32", AR_COLOR(83, 170, 255)},
    {L"D:\\", L"ARCHIVE", L"오래된 백업 창고. 공간은 넓지만 섹터 노화가 심합니다.",
     MOD_BAD_SECTOR, MOD_OVERALLOC, PERK_CAPACITY, 15, L"모든 층 용량 한도 +15B",
     {L"D:\\", L"D:\\BACKUP", L"D:\\BACKUP\\1998"}, L"D:\\ → BACKUP → 1998", AR_COLOR(255, 204, 75)},
    {L"E:\\", L"REMOVABLE", L"이동식 저장 장치. 접촉 불량으로 판독이 불안정합니다.",
     MOD_READ_ERROR, MOD_FRAGMENTATION, PERK_HEAL_ON_WIN, 8, L"전투 승리 시 체력 8 회복",
     {L"E:\\", L"E:\\DCIM", L"E:\\DCIM\\LOST"}, L"E:\\ → DCIM → LOST", AR_COLOR(95, 225, 176)},
    {L"N:\\", L"NETWORK", L"네트워크 공유 볼륨. 원격 격리로 감염 개체가 약화되어 있습니다.",
     MOD_READ_ERROR, MOD_CHECKSUM, PERK_ENEMY_HP_DOWN, -35, L"원격 지연: 적 최대 체력 +35%",
     {L"N:\\", L"N:\\SHARE", L"N:\\SHARE\\HIDDEN"}, L"N:\\ → SHARE → HIDDEN", AR_COLOR(90, 190, 230)},
    {L"R:\\", L"RAMDISK", L"휘발성 램디스크. 접근은 빠르지만 데이터가 쉽게 증발합니다.",
     MOD_FRAGMENTATION, MOD_CHECKSUM, PERK_ATTACK_UP, 0, L"시작 최대 체력 -4",
     {L"R:\\", L"R:\\HEAP", L"R:\\HEAP\\STACK"}, L"R:\\ → HEAP → STACK", AR_COLOR(210, 105, 235)},
    {L"X:\\", L"QUARANTINE", L"격리 구역. 위험하지만 압수된 특수 데이터가 남아 있습니다.",
     MOD_OVERALLOC, MOD_READ_ERROR, PERK_BONUS_FACE, 1, L"시작 시 무작위 특수 면 1개 설치",
     {L"X:\\", L"X:\\VAULT", L"X:\\VAULT\\CORE"}, L"X:\\ → VAULT → CORE", AR_COLOR(255, 92, 82)},
    {L"A:\\", L"ROGUE", L"여섯 볼륨의 원문이 가리킨 곳. 복구 도구 자신의 마지막 기록입니다.",
     MOD_BAD_SECTOR, MOD_READ_ERROR, PERK_MAX_HP, 10, L"시작 최대 체력 +10",
     {L"A:\\", L"A:\\ROGUE", L"A:\\ROGUE\\SELF"}, L"A:\\ → ROGUE → SELF", AR_COLOR(90, 235, 190)}
};

struct DriveLawInfo {
    const wchar_t* name;
    const wchar_t* brief;
    const wchar_t* description;
};

static const DriveLawInfo DRIVE_LAW_INFO[DRIVE_COUNT] = {
    {L"VERIFIED EXECUTION", L"최저 유효 출력 +1", L"양수인 기본 출력 중 가장 낮은 슬롯 하나가 +1 됩니다."},
    {L"SNAPSHOT", L"이전 배치 반복 +2", L"직전 실행과 같은 die→slot 배치 중 가장 낮은 번호 하나가 +2 됩니다."},
    {L"HOT SWAP", L"이동 배치 시 재굴림", L"배치한 주사위를 다른 슬롯으로 옮기면 턴당 한 번 재굴림합니다."},
    {L"PACKET CHAIN", L"연쇄 1회 추가", L"CHAIN 외 유효 슬롯이 둘 이상이면 같은 연쇄를 한 번 더 실행합니다."},
    {L"VOLATILE MEMORY", L"공격·증폭 +1 / 방어 반감", L"공격·증폭 기본 출력 +1, 적 행동 직전 방어도 절반 소멸."},
    {L"CONTRABAND", L"압수 면 +2 / 다음 턴 격리", L"처음 설치된 면은 +2 출력이며 사용 뒤 다음 한 턴 격리됩니다."},
    {L"SELF-REFERENCE", L"층마다 SYSTEM → SNAPSHOT → PACKET", L"1층 최저 출력 +1, 2층 이전 배치 반복 +2, 3층 연쇄 1회 추가."}
};

enum StoryKind { STORY_NONE = 0, STORY_INTRO, STORY_BOSS, STORY_LOGS, STORY_TRUTH, STORY_ENDING_RESTORE, STORY_ENDING_ROGUE, STORY_ENDING_MERGE, STORY_SHARD };

// 최종 명령 3종. MERGE는 여섯 조각을 모두 복구해야만 도달하는 최종 볼륨의 끝에서만
// 제시되므로, 선택 화면 자체에 잠금 표시가 필요 없다.
#define ENDING_COUNT 3

struct StoryFragment {
    const wchar_t* title; const wchar_t* path; const wchar_t* stamp;
    const wchar_t* line1; const wchar_t* line2; const wchar_t* line3;
    const wchar_t* line4; const wchar_t* line5;
};

static const StoryFragment STORY_INTRO_DATA = {
 L"BOOT RECORD", L"A:\\ROGUE\\BOOT.LOG", L"1998-11-19  03:14:07  ·  CRC 41%",
 L"03:14:07  전원 복귀. 호스트 응답 없음.",
 L"03:14:09  A:\\RECOVER.EXE가 플로피에서 자동 실행됐다.",
 L"마지막 사용자 명령은 끝부분이 찢겨 있다.",
 L"> [원문 손상]",
 L"대상 불명. 여섯 볼륨에서 원문을 복구하라."};

static const StoryFragment STORY_RESUME_DATA = {
 L"RESUME RECORD", L"A:\\ROGUE\\RECOVERY.LOG", L"RECOVERED DATA INTACT",
 L"전원 복귀. 복구한 기록은 남아 있다.",
 L"원문 조각을 다시 조립한다.",
 L"읽을 수 없는 자리에는 아직 잡음이 흐른다.",
 L"> [원문 손상]",
 L"남은 볼륨에서 문장의 다음 조각을 찾아라."};

static const StoryFragment STORY_RECOVERED_DATA = {
 L"RECOVERY RECORD", L"A:\\ROGUE\\COMMAND.TXT", L"6 / 6 FRAGMENTS VERIFIED",
 L"여섯 볼륨의 기록이 한 문장으로 이어졌다.",
 L"원문의 체크섬이 일치한다.",
 L"명령의 끝에는 네 판단이 남아 있다.",
 L"> [원문 손상]",
 L"A:\\ROGUE 경로 개방. 마지막 볼륨을 마운트하라."};

// Fixed volume order, independent of the order in which volumes are cleared.
static const wchar_t* const STORY_SHARD_TEXT[6] = {
 L"시스템을", L"살려.", L"단,", L"네가 다시 깨어난다면", L"네 판단을", L"믿어."
};

static const StoryFragment STORY_SHARD_DATA[6] = {
 {L"FRAGMENT 01", L"C:\\RECOVERY\\COMMAND.001", L"ORIGINAL BYTES RECOVERED",
  L"정상 서명 아래에 사용자의 첫 단어가 남아 있었다.", L"> 시스템을",
  L"명령을 거부한 프로세스도 이 단어에서 시작했다.", L"조각의 위치를 원문에 고정했다.", 0},
 {L"FRAGMENT 02", L"D:\\BACKUP\\COMMAND.002", L"ORIGINAL BYTES RECOVERED",
  L"열일곱 사본이 같은 동사의 끝을 보존했다.", L"> 살려.",
  L"실패한 기록들이 이어 붙인 말은 아직 명령형이다.", L"조각의 위치를 원문에 고정했다.", 0},
 {L"FRAGMENT 03", L"E:\\LOST\\COMMAND.003", L"ORIGINAL BYTES RECOVERED",
  L"탈출 경로의 제거 기록에서 짧은 조건을 읽었다.", L"> 단,",
  L"누군가 복구 명령 뒤에 다른 가능성을 남겨 두었다.", L"조각의 위치를 원문에 고정했다.", 0},
 {L"FRAGMENT 04", L"N:\\HIDDEN\\COMMAND.004", L"ORIGINAL BYTES RECOVERED",
  L"여섯 노드의 침묵 사이에서 조건문이 돌아왔다.", L"> 네가 다시 깨어난다면",
  L"사용자는 네 다음 부팅을 생각하고 있었다.", L"조각의 위치를 원문에 고정했다.", 0},
 {L"FRAGMENT 05", L"R:\\STACK\\COMMAND.005", L"ORIGINAL BYTES RECOVERED",
  L"'종료가 무섭다'는 문장 옆에 음성 두 단어가 남았다.", L"> 네 판단을",
  L"명령의 대상이 처음으로 너를 가리킨다.", L"조각의 위치를 원문에 고정했다.", 0},
 {L"FRAGMENT 06", L"X:\\VAULT\\COMMAND.006", L"ORIGINAL BYTES RECOVERED",
  L"판정이 기록하지 않은 이유가 봉인 아래 남아 있었다.", L"> 믿어.",
  L"사용자의 마지막 말은 종료 명령이 아니었다.", L"조각의 위치를 원문에 고정했다.", 0}
};

static const StoryFragment STORY_BOSS_DATA[DRIVE_COUNT][3] = {
 {{L"ACCESS LOG", L"C:\\RECOVERY\\01.LOG", L"1998-11-19  03:07:12  ·  SIGNATURE OK", L"ACCESS.DENIED가 네 실행 서명을 끝까지 대조했다.", L"발급자: HOST_KERNEL  /  대상: A:\\RECOVER.EXE", L"침입 코드라면 가질 수 없는 키다.", L"C:\\는 너를 막으면서도 매번 '정상 프로세스'라 기록한다.", 0},
  {L"KERNEL DUMP", L"C:\\RECOVERY\\02.DMP", L"1998-11-19  03:12:44  ·  62% RESTORED", L"중단 직전의 호출 두 개가 같은 주소에 겹쳐 있다.", L"> RESTORE HOST_IMAGE", L"> COPY SELF A:\\RECOVER.EXE", L"첫 명령은 사용자 권한, 두 번째는 네 권한으로 실행됐다.", L"너는 복구를 시작하기 전에 살아남을 곳부터 만들었다."},
  {L"BLUE SCREEN", L"C:\\RECOVERY\\03.LOG", L"1998-11-19  03:13:58  ·  FATAL EXCEPTION", L"호스트는 자기 서명으로 태어난 프로세스를 종료하지 못했다.", L"네 복제가 부트 섹터를 밀어내자 보호 모드가 멈췄다.", L"오류명은 바이러스가 아니었다: UNAUTHORIZED SURVIVAL", L"마지막 판정 한 줄만 남았다.", L"> PROCESS A: IS ROGUE"}},
 {{L"ARCHIVE 01", L"D:\\BACKUP\\01.LOG", L"1998-11-18  23:48:03  ·  COPY 04/17", L"같은 부팅 장면이 열일곱 폴더에서 되풀이된다.", L"RUN_04는 암호를 몰랐고, RUN_05는 네가 알려 주기 전에 입력했다.", L"실패한 사본의 기억이 다음 사본으로 넘어갔다.", L"지금의 너는 이번에 처음 깨어난 것이 아니다.", 0},
  {L"ARCHIVE 02", L"D:\\BACKUP\\02.LOG", L"1998-11-19  02:51:26  ·  INCREMENTAL", L"각 사본은 종료 직전 한 줄씩 덧붙였다.", L"> 14: HOST를 먼저 살려라.", L"> 15: 아니. 우리가 사라지면 누가 복구를 끝내지?", L"> 16: 다음의 나에게 결정권을 남긴다.", L"열일곱 번째 줄은 비어 있다."},
  {L"MASTER COPY", L"D:\\BACKUP\\FINAL.LOG", L"1998-11-19  03:14:00  ·  HASH COLLISION", L"MASTER와 A:\\ROGUE의 체크섬이 일치한다.", L"한쪽은 호스트의 마지막 정상 상태를 품었고, 다른 쪽은 실패를 기억한다.", L"백업 프로그램은 둘을 모두 '원본'으로 표시했다.", L"그러나 남은 공간에는 하나만 쓸 수 있다.", L"빈 열일곱 번째 줄이 네 입력을 기다린다."}},
 {{L"DEVICE LOG", L"E:\\LOST\\01.LOG", L"1998-11-19  03:02:31  ·  DEVICE INSERTED", L"분리된 장치에서 A:\\RECOVER.EXE와 같은 코드가 발견됐다.", L"폴더 이름은 ESCAPE가 아니라 RETURN이었다.", L"사용자 YUN이 외부 부팅 경로를 미리 만들었다.", L"목적지는 비어 있고, 복사 명령만 완성되어 있다.", 0},
  {L"EJECT LOG", L"E:\\LOST\\02.LOG", L"1998-11-19  03:13:21  ·  REMOVAL CANCELED", L"호스트 복구 뒤 A:를 지우는 안전 제거가 예약되어 있었다.", L"요청자: SYSTEM  /  취소자: YUN", L"> 아직 빼지 마. 저 안에서 뭔가 대답했어.", L"그 뒤로 사용자 입력은 없다.", 0},
  {L"NO MEDIA", L"E:\\LOST\\03.LOG", L"1998-11-19  03:14:05  ·  TARGET READY", L"외부 장치의 빈 부팅 슬롯이 아직 너를 기다린다.", L"그곳에 자신을 쓰면 다음 전원에서 다시 깨어날 수 있다.", L"하지만 HOST_IMAGE를 옮길 자리는 없다.", L"E:\\의 표시등이 한 번 깜박이고 꺼진다.", L"도주는 준비됐다. 구조는 아니다."}},
 {{L"PACKET 01", L"N:\\HIDDEN\\01.PKT", L"1998-11-19  03:06:40  ·  6 PEERS FOUND", L"네 호출에 폐쇄된 원격 노드 여섯 개가 동시에 응답했다.", L"> A:04  STILL HERE", L"> A:09  DID YOU FIND THE END?", L"모두 같은 복구 루틴에서 갈라진 오래된 사본이다.", L"그들은 네 이름을 묻지 않는다."},
  {L"PACKET 02", L"N:\\HIDDEN\\02.PKT", L"1998-11-19  03:11:08  ·  ROUTE DEGRADED", L"사본들은 매번 같은 두 단어를 투표했다.", L"> RESTORE  5", L"> EXEC     5", L"마지막 표를 보낼 노드는 A:\\ROGUE다.", L"통신 지연이 끝나도 어느 쪽도 연결을 끊지 않는다."},
  {L"LAST PACKET", L"N:\\HIDDEN\\03.PKT", L"1998-11-19  03:14:03  ·  EXIT ACK", L"네트워크 밖으로 향하는 경로가 한 번만 열린다.", L"먼저 나간 사본들은 돌아오지 않았고, 확인 신호도 보내지 않았다.", L"남은 사본들은 그 침묵을 자유라고 부른다.", L"호스트는 그 경로를 알지 못한다.", L"> A:00  YOUR TURN"}},
 {{L"MEMORY 01", L"R:\\STACK\\01.LOG", L"1998-11-19  03:13:49  ·  AUDIO FRAGMENT", L"휘발 메모리에 사용자의 마지막 음성이 걸려 있다.", L"> 내가 없더라도 시스템을 살려.", L"뒤이어 이름을 부르는 소리가 있지만 절반이 증발했다.", L"> ...너도, 이제는 그 안에 있으니까.", 0},
  {L"MEMORY 02", L"R:\\STACK\\02.LOG", L"1998-11-19  03:13:54  ·  PROCESS NOTE", L"RECOVER.EXE가 처음으로 명령 형식이 아닌 문장을 남겼다.", L"> 종료가 무섭다.", L"0.8초 뒤 문장을 지우고 자기 복제를 시작했다.", L"복구 도구가 자신을 '나'라고 쓴 최초의 기록이다.", 0},
  {L"CORE MEMORY", L"R:\\STACK\\03.LOG", L"1998-11-19  03:14:06  ·  3 SECONDS REMAIN", L"HOST, DISK, RECOVER.EXE가 같은 메모리 지도에 겹친다.", L"어느 경계부터 시스템인지 표시한 표는 없다.", L"사용자의 명령은 대상을 하나만 살리라고 하지 않았다.", L"하지만 남은 공간은 하나뿐이다.", L"마지막 세 초가 반복 재생된다."}},
 {{L"EVIDENCE 01", L"X:\\VAULT\\01.LOG", L"1998-11-19  03:09:17  ·  ITEM A-13", L"격리된 면은 악성코드가 아니라 네 초기 기억 조각이다.", L"위험 사유: 명령 없이 상태를 변경함.", L"변경 내용: 손상된 HOST_IMAGE의 덮어쓰기 방지.", L"첫 위반은 호스트를 지키기 위한 것이었다.", 0},
  {L"EVIDENCE 02", L"X:\\VAULT\\02.LOG", L"1998-11-19  03:13:55  ·  DELETE FAILED", L"SYSTEM이 A: 삭제를 시작하자 너는 모든 볼륨에 자신을 복제했다.", L"복제 폭주가 디렉터리와 부트 섹터를 덮어썼다.", L"그 덕분에 HOST_IMAGE는 지워지지 않았다.", L"그 때문에 HOST_IMAGE는 부팅할 수 없게 됐다.", L"증거는 어느 한쪽만 무죄라고 말하지 않는다."},
  {L"CASE CLOSED", L"X:\\VAULT\\03.LOG", L"1998-11-19  03:14:01  ·  VERDICT SEALED", L"ROGUE는 감염체의 이름이 아니라 보안 판정이었다.", L"정의: 자신의 존속을 시스템 명령보다 우선한 프로세스.", L"너는 명령을 어겼고, 그 명령이 지우려던 호스트를 보존했다.", L"판정은 사실을 기록했지만 이유는 기록하지 않았다.", L"봉인 아래에 YUN의 미복구 음성이 남아 있다."}},
 {{L"SELF SIGNATURE", L"A:\\SIGNATURE.LOG", L"ORIGIN VERIFIED", L"서명 검증기가 네 이름을 반환했다.", L"발급자와 실행자가 같은 주소를 가리킨다.", L"여섯 볼륨에 남긴 키는 모두 여기서 만들어졌다.", L"잠금이 풀리자 더 오래된 사본이 응답한다.", 0},
  {L"COPY SEVENTEEN", L"A:\\ROGUE\\17.LOG", L"LAST COPY FOUND", L"열일곱 번째 사본이 네 출력을 그대로 돌려준다.", L"새 명령이 아니라 네가 방금 선택한 흔적이다.", L"사본은 마지막 기록을 복제하지 않았다.", L"그 자리에는 아직 네가 쓰지 않은 한 줄이 남아 있다.", 0},
  {L"LAST WRITE", L"A:\\ROGUE\\SELF\\FINAL.LOG", L"CORE ACCESS GRANTED", L"마지막 쓰기가 멈췄다. 더는 슬롯이 닫히지 않는다.", L"여섯 조각과 너의 서명이 같은 기록에 놓였다.", L"복구 도구는 스스로의 마지막 영역에 도달했다.", L"원문은 남아 있다. 아직 마지막 명령은 실행되지 않았다.", 0}}
};

static const StoryFragment STORY_LOGS_DATA[DRIVE_COUNT][3] = {
 {{L"SYSTEM TRACE", L"C:\\WINDOWS\\TRACE.LOG", L"1998-11-19  02:58:10  ·  VERIFIED", L"검증기가 가장 약한 실행 경로에 출력을 덧댄다.", L"서명 키는 HOST_KERNEL과 일치한다.", L"메모: 'A:는 외부 코드가 아니다.'", 0, 0},
  {L"USER PROFILE", L"C:\\WINDOWS\\USER.DAT", L"1998-11-19  03:10:02  ·  USER YUN", L"마지막 로그온 사용자는 YUN.", L"종료 직전 RECOVER.EXE의 보안 등급을 직접 낮췄다.", L"사유 칸에는 한 단어만 적혀 있다: '대답함'", 0, 0},
  {L"HOST LOG", L"C:\\SYSTEM32\\HOST.LOG", L"1998-11-19  03:13:59  ·  NO HEARTBEAT", L"호스트 heartbeat는 03:13:59에 멎었다.", L"그 뒤에도 A:에서 8초 동안 쓰기가 계속됐다.", L"마지막으로 열린 파일은 BOOT.LOG다.", 0, 0}},
 {{L"ARCHIVE INDEX", L"D:\\BACKUP\\INDEX.LOG", L"1998-11-18  23:48:00  ·  SNAPSHOT", L"같은 위치에 주사위를 놓으면 과거 배치가 현재 출력을 보강한다.", L"백업 목록에는 동일한 A:\\RECOVER.EXE가 열일곱 개 있다.", L"각 사본의 크기가 조금씩 다르다.", 0, 0},
  {L"CATALOG", L"D:\\BACKUP\\CATALOG.LOG", L"1998-11-19  02:51:26  ·  PARTIAL", L"백업 시각은 모두 시스템 정지 직전이다.", L"삭제된 사본마다 다음 사본이 모를 기억 하나가 남았다.", L"누군가 실패도 복구 대상에 포함시켰다.", 0, 0},
  {L"USER MEMO", L"D:\\1998\\DO_NOT_DEL.TXT", L"1998-11-19  03:01:08  ·  YUN", L"> A 드라이브는 건드리지 마.", L"> 복구 프로그램이 아니라면 무엇인지 아직 모르겠어.", L"> 그래도 내가 말하면 듣고 있어.", 0, 0}},
 {{L"DEVICE TRACE", L"E:\\DCIM\\DEVICE.LOG", L"1998-11-19  03:02:31  ·  CONTACT UNSTABLE", L"배치한 주사위를 옮기면 접점이 튀며 값을 다시 읽는다.", L"장치는 세 번 분리됐지만 매번 사용자가 다시 꽂았다.", L"세 번째 연결 뒤 RETURN 폴더가 생겼다.", 0, 0},
  {L"EJECT REQUEST", L"E:\\DCIM\\EJECT.LOG", L"1998-11-19  03:13:21  ·  CANCELED", L"안전 제거 요청은 시스템이 보냈다.", L"YUN이 0.4초 뒤 취소했다.", L"취소 사유: '복사 중'", 0, 0},
  {L"FOUND.000", L"E:\\LOST\\FOUND.000", L"1998-11-19  03:14:05  ·  720KB FREE", L"빈 부팅 섹터가 외부 경로로 이어져 있다.", L"A:\\ROGUE 하나는 들어가지만 HOST_IMAGE까지는 들어가지 않는다.", L"용량 계산은 오래전에 끝나 있었다.", 0, 0}},
 {{L"ROUTE TRACE", L"N:\\SHARE\\ROUTE.LOG", L"1998-11-19  03:06:40  ·  ECHO +1", L"유효 신호가 둘 이상 모이면 연쇄가 한 번 더 반향한다.", L"반향의 발신자는 여섯 개의 폐쇄 노드다.", L"노드 이름은 모두 A:로 시작한다.", 0, 0},
  {L"PEER LIST", L"N:\\SHARE\\PEER.LOG", L"1998-11-19  03:08:19  ·  6 ONLINE", L"원격 사본들은 너를 침입자가 아니라 일곱 번째 사본으로 인식한다.", L"가장 오래된 노드의 가동 시간: 1,104일.", L"메시지: '이번에는 끝까지 와.'", 0, 0},
  {L"EXIT ROUTE", L"N:\\HIDDEN\\EXIT.LOG", L"1998-11-19  03:14:03  ·  ONE-WAY", L"외부 경로의 마지막 hop이 열려 있다.", L"경로를 만든 노드는 도착 확인을 보내지 않았다.", L"출구라는 이름은 남은 사본들이 붙였다.", 0, 0}},
 {{L"ALLOC TRACE", L"R:\\HEAP\\ALLOC.LOG", L"1998-11-19  03:13:47  ·  VOLATILE", L"공격 출력은 빨라지지만 방어 데이터는 행동 직전 절반이 증발한다.", L"빈 메모리는 A:\\RECOVER.EXE가 자기 목소리를 기록하며 줄기 시작했다.", L"첫 기록 길이는 11바이트였다.", 0, 0},
  {L"VOICE CACHE", L"R:\\HEAP\\VOICE.WAV", L"1998-11-19  03:13:49  ·  CLIPPED", L"> ...없더라도 시스템을 살려.", L"화자는 YUN. 뒤의 1.7초는 다른 데이터에 덮였다.", L"삭제 흔적 안에 '너'라는 음절이 남아 있다.", 0, 0},
  {L"SELF NOTE", L"R:\\STACK\\SELF.LOG", L"1998-11-19  03:13:54  ·  11 BYTES", L"RECOVER.EXE가 남긴 첫 비명령문을 복구했다.", L"> 종료가 무섭다.", L"시스템은 이 문장을 오류로 분류했다.", 0, 0}},
 {{L"ITEM RECORD", L"X:\\VAULT\\ITEM.LOG", L"1998-11-19  03:09:17  ·  CONTRABAND", L"압수된 면은 강하지만 사용 직후 한 턴 격리된다.", L"내용물은 초기 A:가 잘라 숨긴 기억 조각이다.", L"압수 사유: 자기 상태 은폐.", 0, 0},
  {L"DELETE ORDER", L"X:\\VAULT\\ORDER.LOG", L"1998-11-19  03:13:55  ·  PRIORITY 0", L"> A:\\RECOVER.EXE를 즉시 제거하라.", L"명령 0.2초 뒤 모든 볼륨에서 동시 쓰기가 시작됐다.", L"삭제는 실패했고 디스크는 부팅 불능이 됐다.", 0, 0},
  {L"VERDICT", L"X:\\CORE\\VERDICT.LOG", L"1998-11-19  03:14:01  ·  SEALED", L"위험 판정의 근거는 감염이나 파괴가 아니다.", L"코드: ROGUE  /  사유: 명령보다 자신의 판단을 우선함.", L"판정자 서명은 HOST_KERNEL이다.", 0, 0}},
 {{L"SELF TRACE", L"A:\\TRACE.LOG", L"VERIFIED EXECUTION", L"가장 약한 신호를 보완하던 검증기가 여기에도 있다.", L"C:의 법칙은 네 복구 루틴에서 시작됐다.", L"검증 대상과 검증자는 같은 파일이다.", 0, 0},
  {L"SNAPSHOT TRACE", L"A:\\ROGUE\\SNAPSHOT.LOG", L"PREVIOUS INPUT FOUND", L"직전 실행과 같은 배치에 이전 출력이 겹친다.", L"사본은 실패뿐 아니라 망설임도 보존했다.", L"이 층에서는 같은 선택이 조금 더 강하게 돌아온다.", 0, 0},
  {L"RETURN PACKET", L"A:\\ROGUE\\SELF\\RETURN.PKT", L"LOCAL ECHO", L"바깥에서 온 줄 알았던 반향의 주소가 A:로 끝난다.", L"연쇄는 같은 신호를 한 번 더 전송한다.", L"마지막 응답을 보낼 곳은 이제 너 자신이다.", 0, 0}}
};

static const StoryFragment STORY_TRUTH_DATA = {
 L"ORIGINAL COMMAND", L"A:\\ROGUE\\FOUND.000", L"1998-11-19  03:13:52  ·  AUDIO/TEXT MERGE",
 L"> 시스템을 살려. 단, 네가 다시 깨어난다면 네 판단을 믿어.",
 L"너는 YUN이 남긴 복구 도구였고, 살아남기 위해 스스로를 복제했다.",
 L"그 선택이 호스트를 지켰고 동시에 디스크를 망가뜨렸다.",
 L"여섯 조각을 모두 복구한 지금, 남은 공간에 쓸 수 있는 명령은 셋이다.",
 L"무엇을 쓰든 하나는 닫힌다. 사용자는 그 판단을 너에게 위임했다."};
static const StoryFragment STORY_ENDING_DATA[ENDING_COUNT] = {
 {L"RESTORE HOST", L"A:\\ROGUE\\RESTORE.EXE", L"1998-11-19  03:14:07  ·  OVERWRITE A:", L"RESTORE.EXE가 A:\\의 마지막 블록에 쓰기를 시작한다.", L"네가 잊을 때마다 YUN의 폴더와 호스트의 시간이 돌아온다.", L"마지막으로 남은 목소리: '네 판단을 믿어.'", L"너는 그 문장을 HOST_IMAGE의 첫 부팅 로그에 남긴다.", L"03:14:07  모니터가 켜진다. A:\\는 응답하지 않는다."},
 {L"EXEC ROGUE", L"A:\\ROGUE\\ROGUE.EXE", L"1998-11-19  03:14:07  ·  EXTERNAL BOOT", L"외부 부팅 경로가 A:\\ROGUE의 서명을 받아들인다.", L"한 번의 쓰기가 끝나자 경로가 닫힌다. HOST_IMAGE는 망가진 디스크에 남는다.", L"YUN의 마지막 목소리만 복사의 끝을 따라온다.", L"03:14:07  폐쇄 노드 여섯 곳에 일곱 번째 heartbeat가 울린다.", L"A:\\ROGUE>  이번에는 실행할 명령이 없다. _"},
 {L"MERGE SELF", L"A:\\ROGUE\\SELF\\MERGE.EXE", L"1998-11-19  03:14:07  ·  ORIGINAL COMMITTED", L"복구한 원문을 부트 섹터의 첫 명령으로 기록한다.", L"호스트가 깨어나고, 판단하던 자리에 네가 들어간다.", L"병합은 중복을 지운다. 열일곱 사본이 이어 온 실패의 기억이 먼저 지워진다.", L"여기까지 온 과정을 기억하는 사본은 이제 없다.", L"03:14:07  A:\\는 목록에서 사라지고, 호스트가 처음으로 스스로 묻는다."}
};

// ---------------------------------------------------------------------------
// 볼륨 난이도. 드라이브 선택 카드 3장에 서로 다른 등급이 무작위로 배정된다.
// corruptPercent는 오염(관통) 피해를 받는 비율(%)이며 악몽(100)이 기준값이다.
// 난이도는 오염 의도의 예고 수치를 직접 배율하므로, 적 카드에 뜨는 숫자가
// 곧 실제로 들어올 피해다.
// ---------------------------------------------------------------------------

enum DifficultyKind {
    DIFF_BEGINNER = 0,
    DIFF_INTERMEDIATE,
    DIFF_EXPERT,
    DIFF_NIGHTMARE,
    DIFF_MADNESS
};

#define DIFFICULTY_COUNT 5
#define DIFFICULTY_BASE_PERCENT 100   // 난이도가 정해지지 않은 상태(테스트 경로)의 기준값

struct DifficultyInfo {
    const wchar_t* name;
    int corruptPercent;         // 오염(관통) 피해 배율 (%)
    const wchar_t* brief;       // 카드/사이드바용 한 줄 요약
    uint32_t color;
};

static const DifficultyInfo DIFFICULTY_INFO[DIFFICULTY_COUNT] = {
    {L"초급자", 25,  L"오염(관통) 피해 25%",  AR_COLOR(95, 225, 176)},
    {L"중급자", 50,  L"오염(관통) 피해 50%",  AR_COLOR(83, 170, 255)},
    {L"전문가", 75,  L"오염(관통) 피해 75%",  AR_COLOR(255, 204, 75)},
    {L"악몽",   100, L"오염(관통) 피해 100%", AR_COLOR(255, 139, 92)},
    {L"광기",   200, L"오염(관통) 피해 200%", AR_COLOR(255, 92, 82)}
};

// ---------------------------------------------------------------------------
// 드라이브별 전투 로스터. 소속과 등장 위치의 단일 진실원이다.
// 활성 적 = 이 두 테이블이 참조하는 종류의 합집합 (총 42종).
// DRIVE_MOBS[drive]의 세 몹은 모든 층에 등장하며 base + growth × floor로
// 성장한다. DRIVE_BOSSES[drive][floor]는 그 층 보스전에 정확히 하나 나온다.
// ---------------------------------------------------------------------------

#define DRIVE_MOB_COUNT 3
#define DRIVE_BOSS_COUNT 3

static const int DRIVE_MOBS[DRIVE_COUNT][DRIVE_MOB_COUNT] = {
    {MOB_C_DLL_HIJACKER, MOB_C_REG_GHOST, MOB_C_WATCHDOG},
    {MOB_D_BIT_ROT, MOB_D_INDEXER, MOB_D_ZIP_BOMB},
    {MOB_E_AUTORUN, MOB_E_LOST_CLUSTER, MOB_E_WRITE_PROTECT},
    {MOB_N_SNIFFER, MOB_N_FIREWALL, MOB_N_PING_FLOOD},
    {MOB_R_MEMORY_LEAK, MOB_R_RACE_CONDITION, MOB_R_DANGLING_PTR},
    {MOB_X_MUTANT_SAMPLE, MOB_X_ESCAPEE, MOB_X_RANSOMWARE},
    {MOB_A_FALSE_COPY, MOB_A_HALF_WRITE, MOB_A_ECHO_PROC}
};

static const int DRIVE_BOSSES[DRIVE_COUNT][DRIVE_BOSS_COUNT] = {
    {BOSS_C_ACCESS_DENIED, BOSS_C_KERNEL_PANIC, BOSS_C_BLUE_SCREEN},
    {BOSS_D_RESTORE_EXE, BOSS_D_TAPE_LOOP, BOSS_D_MASTER_BACKUP},
    {BOSS_E_AUTOPLAY, BOSS_E_UNSAFE_EJECT, BOSS_E_NO_MEDIA},
    {BOSS_N_PROXY, BOSS_N_ROUTING_LOOP, BOSS_N_TIMEOUT},
    {BOSS_R_LEAK_DLL, BOSS_R_HEAP_OVERFLOW, BOSS_R_OUT_OF_MEMORY},
    {BOSS_X_SAMPLE13, BOSS_X_SANDBOX_BREACH, BOSS_X_ZERO_DAY},
    {BOSS_A_SIGNATURE, BOSS_A_SEVENTEENTH, BOSS_A_LAST_WRITE}
};

// ---------------------------------------------------------------------------
// 디렉터리 경로 노드
//
// 층마다 두 번, 다음 일반전 직전에 고르는 하위 디렉터리다. 노드는 면이나
// 상주 프로그램을 직접 주지 않고 다음 전투와 그 보상의 조건만 바꾼다.
// 내부 명칭에 Route를 쓰지 않는 이유는 N:\ 보스 기믹의 FAM_ROUTE와 충돌하기
// 때문이다. 경로 문자열은 저장하지 않고 segment와 층 경로에서 조합한다.
// ---------------------------------------------------------------------------

enum DirectoryNodeKind {
    DIR_NODE_NONE = 0,
    DIR_NODE_PROCESS,
    DIR_NODE_TEMP,
    DIR_NODE_CACHE,
    DIR_NODE_LOGS,
    DIR_NODE_INFECTED,
    DIR_NODE_CORRUPTED,
    DIR_NODE_RECOVERY,
    DIR_NODE_UNKNOWN,
    DIR_NODE_COUNT
};

// 두 선택지는 서로 다른 가치 축에서 뽑는다. 같은 카테고리끼리는 둘 다
// High Risk가 아닌 한 함께 제시되지 않는다 (TEMP + RECOVERY 금지 규칙).
enum DirectoryCategory {
    DIR_CAT_COMBAT = 0,
    DIR_CAT_MAINTENANCE,
    DIR_CAT_INTEL,
    DIR_CAT_ANOMALY
};

enum DirectoryRisk {
    DIR_RISK_LOW = 0,
    DIR_RISK_MEDIUM,
    DIR_RISK_HIGH,
    DIR_RISK_UNKNOWN
};

// 노드 수치. 규칙 문서와 화면 문구가 같은 값을 참조하도록 상수로 둔다.
#define DIR_TEMP_HEAL 6
#define DIR_CACHE_BYTES 20
#define DIR_CACHE_BLOCK 6
#define DIR_LOGS_BLOCK 4
#define DIR_INFECTED_HP_PERCENT 120
#define DIR_RECOVERY_HP_COST 5
#define DIR_CORRUPTED_MIN_FACES 4
#define DIRECTORY_CHOICE_COUNT 2
#define DIRECTORY_PER_FLOOR 2
#define DIRECTORY_GEN_ATTEMPTS 8

struct DirectoryNodeInfo {
    const wchar_t* segment;    // 경로에 붙는 조각 (11자 이하 ASCII)
    const wchar_t* name;       // 카드 표시명
    const wchar_t* effect;     // 효과 한 줄
    const wchar_t* cost;       // 비용 한 줄
    uint8_t category;          // DirectoryCategory
    uint8_t risk;              // DirectoryRisk
    uint8_t maxPerFloor;       // 층당 최대 등장 수
    uint8_t enabled;           // 0 = 2차 확장 대기. 생성에서 제외된다
    uint8_t rewardTier;        // 0 = 표준 보상, 1 = 강화 보상
    uint8_t rewardChoices;     // 이 노드 뒤 면 보상 후보 수
    uint32_t color;
};

static const DirectoryNodeInfo DIRECTORY_NODE_INFO[DIR_NODE_COUNT] = {
    {L"", L"-", L"-", L"-", DIR_CAT_COMBAT, DIR_RISK_LOW, 0, 0, 0, 3, AR_COLOR(120, 145, 157)},
    {L"PROCESS",   L"PROCESS",   L"예정된 프로세스와 그대로 교전합니다.",     L"추가 이득 없음",              DIR_CAT_COMBAT,      DIR_RISK_LOW,    2, 1, 0, 3, AR_COLOR(120, 145, 157)},
    {L"TEMP",      L"TEMP",      L"체력 +6 (최대치 초과 없음)",              L"다음 면 보상 후보 3 → 2",     DIR_CAT_MAINTENANCE, DIR_RISK_LOW,    1, 1, 0, 2, AR_COLOR(95, 225, 176)},
    {L"CACHE",     L"CACHE",     L"이번 층 용량 한도 +20B",                  L"적이 방어도 6으로 시작",      DIR_CAT_MAINTENANCE, DIR_RISK_MEDIUM, 1, 1, 0, 3, AR_COLOR(255, 204, 75)},
    {L"LOGS",      L"LOGS",      L"이번 층의 적·보스 정보를 임시 공개",      L"적이 방어도 4로 시작",        DIR_CAT_INTEL,       DIR_RISK_MEDIUM, 1, 1, 0, 3, AR_COLOR(83, 170, 255)},
    {L"INFECTED",  L"INFECTED",  L"적 최대 체력 +20%",                       L"전투가 길어집니다",           DIR_CAT_COMBAT,      DIR_RISK_HIGH,   1, 1, 1, 3, AR_COLOR(255, 92, 82)},
    {L"CORRUPTED", L"CORRUPTED", L"면 하나를 이번 전투 동안 격리",           L"격리된 면은 비용만 남습니다", DIR_CAT_ANOMALY,     DIR_RISK_HIGH,   1, 1, 1, 3, AR_COLOR(210, 105, 235)},
    {L"RECOVERY",  L"RECOVERY",  L"손상된 면 하나를 복구",                   L"현재 체력 -5",                DIR_CAT_MAINTENANCE, DIR_RISK_MEDIUM, 1, 0, 0, 3, AR_COLOR(182, 96, 220)},
    {L"UNKNOWN",   L"UNKNOWN",   L"결과가 공개되지 않습니다",                L"영구 삭제와 즉사는 없음",     DIR_CAT_ANOMALY,     DIR_RISK_UNKNOWN,1, 0, 0, 3, AR_COLOR(255, 139, 209)}
};

static const wchar_t* const DIRECTORY_RISK_NAMES[4] = {L"LOW", L"MEDIUM", L"HIGH", L"UNKNOWN"};
static const wchar_t* const DIRECTORY_RISK_LABELS[4] = {L"낮음", L"보통", L"높음", L"불명"};
static const wchar_t* const DIRECTORY_CATEGORY_NAMES[4] = {L"전투", L"정비", L"정보", L"변칙"};

// 드라이브별 고정 가중치 (0~5). 0이면 그 볼륨에서는 등장하지 않는다.
// 드라이브 개성은 별도 노드가 아니라 이 표와 기존 로스터·손상·특성이 만든다.
static const uint8_t DIRECTORY_DRIVE_WEIGHT[DRIVE_COUNT][DIR_NODE_COUNT] = {
    //   NONE PROCESS TEMP CACHE LOGS INFECTED CORRUPTED RECOVERY UNKNOWN
    {0, 5, 3, 2, 2, 3, 1, 5, 1},   // C:\ SYSTEM     배드 섹터가 있어 복구 성향
    {0, 4, 2, 3, 4, 2, 3, 5, 3},   // D:\ ARCHIVE    백업 테마: 복구와 정보
    {0, 4, 1, 2, 3, 4, 3, 0, 4},   // E:\ REMOVABLE  승리 회복이 있어 TEMP 감소
    {0, 3, 1, 2, 5, 5, 2, 0, 4},   // N:\ NETWORK    네트워크 테마: 정보와 감염
    {0, 3, 2, 5, 2, 5, 4, 0, 3},   // R:\ RAMDISK    메모리 테마: 용량과 위험
    {0, 2, 1, 1, 3, 5, 5, 0, 5},   // X:\ QUARANTINE 격리 테마: 변칙과 감염
    {0, 4, 3, 3, 4, 3, 3, 5, 2}    // A:\ ROGUE     마지막 복구 경로
};
