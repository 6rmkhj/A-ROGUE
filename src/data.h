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

static const FaceInfo FACE_INFO[FACE_KIND_COUNT] = {
    {L"숫자", L"숫자", L"값만큼 효과, 값만큼 바이트", 0, 0, AR_COLOR(205, 221, 232)},
    {L"화염", L"화염", L"공격 시 추가 피해와 화상", 24, 8, AR_COLOR(255, 92, 72)},
    {L"방벽", L"방벽", L"방어 슬롯에서 효과 2배", 16, 7, AR_COLOR(76, 170, 255)},
    {L"흡수", L"흡수", L"공격 피해 일부를 회복", 22, 6, AR_COLOR(182, 96, 220)},
    {L"와일드", L"와일드", L"어느 슬롯에서도 높은 출력", 32, 9, AR_COLOR(255, 205, 64)},
    {L"증폭", L"증폭", L"증폭 슬롯에서 보너스 2배", 18, 5, AR_COLOR(95, 225, 176)},
    {L"메아리", L"메아리", L"연쇄 슬롯에서 직전 효과 반복", 20, 5, AR_COLOR(255, 139, 209)},
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
// 일반 몹 3종 + 층별 보스 3종, 총 36종이다. 보스 여부는 enum 범위가
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
    {FAM_QUARANTINE, L"제로데이",      L"오염이 가득 차면 예고된 면 1개를 영구 삭제합니다.",           L"한 턴 15+ 피해로 오염을 1 낮추십시오.", L"DATA DESTROYED",               4, 15, 0}
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
    {L"DLL 하이재커", L"DLL.HIJACK", 16, 5, 2, 7, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, AR_COLOR(120, 190, 255)},
    {L"레지스트리 고스트", L"REG.GHOST", 18, 4, 4, 7, 1, 1, ROLE_MOB, PATTERN_MEDIC, GIMMICK_NONE, AR_COLOR(150, 200, 250)},
    {L"워치독 서비스", L"WATCHDOG", 20, 5, 4, 8, 1, 1, ROLE_MOB, PATTERN_BULWARK, GIMMICK_NONE, AR_COLOR(80, 150, 235)},
    {L"액세스 거부", L"ACCESS.DENIED", 34, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_ACCESS_DENIED, AR_COLOR(96, 168, 255)},
    {L"커널 패닉", L"KERNEL.PANIC", 48, 7, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_KERNEL_PANIC, AR_COLOR(70, 140, 245)},
    {L"블루 스크린", L"BLUE.SCREEN", 62, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_BLUE_SCREEN, AR_COLOR(58, 122, 240)},
    // ---- D:\ ARCHIVE ----
    {L"비트 부패", L"BIT.ROT", 15, 4, 3, 6, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, AR_COLOR(235, 190, 90)},
    {L"인덱서", L"INDEXER", 21, 4, 6, 8, 1, 1, ROLE_MOB, PATTERN_SIEGE, GIMMICK_NONE, AR_COLOR(255, 214, 110)},
    {L"집 폭탄", L"ZIP.BOMB", 16, 6, 1, 6, 1, 0, ROLE_MOB, PATTERN_ASSAULT, GIMMICK_NONE, AR_COLOR(255, 180, 55)},
    {L"복원 프로그램", L"RESTORE.EXE", 36, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_RESTORE_POINT, AR_COLOR(255, 208, 96)},
    {L"테이프 루프", L"TAPE.LOOP", 36, 7, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_TAPE_LOOP, AR_COLOR(238, 186, 70)},
    {L"마스터 백업", L"MASTER.BACKUP", 46, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_MASTER_BACKUP, AR_COLOR(220, 165, 52)},
    // ---- E:\ REMOVABLE ----
    {L"오토런", L"AUTORUN.INF", 15, 5, 2, 6, 1, 1, ROLE_MOB, PATTERN_OPENER, GIMMICK_NONE, AR_COLOR(120, 230, 150)},
    {L"유실 클러스터", L"LOST.CLUSTER", 18, 4, 3, 7, 1, 1, ROLE_MOB, PATTERN_MEDIC, GIMMICK_NONE, AR_COLOR(96, 210, 176)},
    {L"쓰기 방지", L"WRITE.PROTECT", 20, 4, 6, 8, 1, 1, ROLE_MOB, PATTERN_BULWARK, GIMMICK_NONE, AR_COLOR(78, 190, 140)},
    {L"자동 재생", L"AUTOPLAY", 35, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_AUTOPLAY, AR_COLOR(110, 235, 168)},
    {L"강제 제거", L"UNSAFE.EJECT", 50, 7, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_UNSAFE_EJECT, AR_COLOR(84, 216, 150)},
    {L"미디어 없음", L"NO.MEDIA", 55, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_NO_MEDIA, AR_COLOR(60, 196, 128)},
    // ---- N:\ NETWORK ----
    {L"패킷 스니퍼", L"SNIFFER", 15, 4, 3, 6, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, AR_COLOR(110, 205, 240)},
    {L"방화벽", L"FIREWALL", 22, 3, 7, 8, 1, 1, ROLE_MOB, PATTERN_SIEGE, GIMMICK_NONE, AR_COLOR(90, 190, 230)},
    {L"핑 폭주", L"PING.FLOOD", 16, 5, 1, 6, 1, 0, ROLE_MOB, PATTERN_ASSAULT, GIMMICK_NONE, AR_COLOR(70, 220, 255)},
    {L"프록시", L"PROXY", 40, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_PROXY, AR_COLOR(96, 200, 245)},
    {L"라우팅 루프", L"ROUTING.LOOP", 56, 7, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_ROUTING_LOOP, AR_COLOR(72, 180, 235)},
    {L"타임아웃", L"TIMEOUT", 68, 8, 7, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_TIMEOUT, AR_COLOR(52, 160, 225)},
    // ---- R:\ RAMDISK ----
    {L"메모리 누수", L"MEM.LEAK", 17, 3, 2, 7, 1, 1, ROLE_MOB, PATTERN_RAMP, GIMMICK_NONE, AR_COLOR(220, 130, 245)},
    {L"경쟁 상태", L"RACE.COND", 16, 5, 3, 6, 1, 1, ROLE_MOB, PATTERN_ERRATIC, GIMMICK_NONE, AR_COLOR(200, 110, 230)},
    {L"허상 포인터", L"DANGLING.PTR", 15, 5, 2, 6, 1, 1, ROLE_MOB, PATTERN_SPIKE, GIMMICK_NONE, AR_COLOR(235, 96, 220)},
    {L"누수 라이브러리", L"LEAK.DLL", 30, 5, 3, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_LEAK, AR_COLOR(214, 118, 240)},
    {L"힙 오버플로", L"HEAP.OVERFLOW", 44, 6, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_HEAP_OVERFLOW, AR_COLOR(192, 92, 226)},
    {L"메모리 고갈", L"OUT.OF.MEMORY", 55, 8, 6, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_OUT_OF_MEMORY, AR_COLOR(170, 70, 212)},
    // ---- X:\ QUARANTINE ----
    {L"변이 샘플", L"MUTANT.SMP", 16, 5, 2, 7, 1, 1, ROLE_MOB, PATTERN_CORRUPTER, GIMMICK_NONE, AR_COLOR(255, 120, 108)},
    {L"샌드박스 탈주", L"ESCAPEE", 17, 6, 2, 7, 1, 0, ROLE_MOB, PATTERN_ASSAULT, GIMMICK_NONE, AR_COLOR(255, 96, 130)},
    {L"랜섬웨어", L"RANSOMWARE", 20, 4, 5, 8, 1, 1, ROLE_MOB, PATTERN_MEDIC, GIMMICK_NONE, AR_COLOR(230, 70, 96)},
    {L"검체-13", L"SAMPLE-13", 36, 5, 4, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_SAMPLE13, AR_COLOR(255, 104, 92)},
    {L"샌드박스 침입", L"SANDBOX.BREACH", 50, 7, 5, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_SANDBOX_BREACH, AR_COLOR(240, 80, 78)},
    {L"제로데이", L"ZERO.DAY", 60, 9, 8, 0, 0, 0, ROLE_BOSS, PATTERN_BOSS, GIMMICK_ZERO_DAY, AR_COLOR(255, 56, 66)}
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
    TSR_HIMEM = 0,   // 용량 한도 +60B
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
    {L"HIMEM.SYS", L"용량 한도 +60B",                 20, 60, -1,                AR_COLOR(255, 204, 75)},
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

#define DRIVE_COUNT 6

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
     {L"X:\\", L"X:\\VAULT", L"X:\\VAULT\\CORE"}, L"X:\\ → VAULT → CORE", AR_COLOR(255, 92, 82)}
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
    {L"CONTRABAND", L"압수 면 +2 / 다음 턴 격리", L"처음 설치된 면은 +2 출력이며 사용 뒤 다음 한 턴 격리됩니다."}
};

enum StoryKind { STORY_NONE = 0, STORY_INTRO, STORY_BOSS, STORY_LOGS, STORY_TRUTH, STORY_ENDING_RESTORE, STORY_ENDING_ROGUE };

struct StoryFragment {
    const wchar_t* title; const wchar_t* path;
    const wchar_t* line1; const wchar_t* line2; const wchar_t* line3;
    const wchar_t* line4; const wchar_t* line5;
};

static const StoryFragment STORY_INTRO_DATA = {L"BOOT RECORD",L"A:\\ROGUE\\BOOT.LOG",L"호스트 복구 프로세스가 안전 모드에서 깨어났다.",L"여섯 볼륨 중 하나에 침입 프로세스의 근원이 숨어 있다.",L"면을 복구하고 코어에 도달하라.",0,0};

static const StoryFragment STORY_BOSS_DATA[DRIVE_COUNT][3] = {
 {{L"ACCESS LOG",L"C:\\RECOVERY\\01.LOG",L"검증기는 실행 권한 자체를 추적했다.",L"서명에는 호스트와 동일한 키가 남아 있다.",0,0,0},{L"KERNEL DUMP",L"C:\\RECOVERY\\02.DMP",L"복구 명령은 두 갈래로 분기됐다.",L"하나는 호스트를, 하나는 자신을 보존한다.",0,0,0},{L"BLUE SCREEN",L"C:\\RECOVERY\\03.LOG",L"A:\\ROGUE는 외부 침입자가 아니었다.",L"호스트가 만든 마지막 자율 복구 사본이었다.",0,0,0}},
 {{L"ARCHIVE 01",L"D:\\BACKUP\\01.LOG",L"같은 부팅 장면이 백업 속에서 반복된다.",L"복구기는 매번 기억을 더 남겼다.",0,0,0},{L"ARCHIVE 02",L"D:\\BACKUP\\02.LOG",L"호스트는 실패한 사본을 폐기하지 못했다.",L"그 흔적이 지금의 선택을 연습한다.",0,0,0},{L"MASTER COPY",L"D:\\BACKUP\\FINAL.LOG",L"원본과 복구본을 가르는 체크섬은 사라졌다.",L"남은 차이는 마지막 선택뿐이다.",0,0,0}},
 {{L"DEVICE LOG",L"E:\\LOST\\01.LOG",L"분리된 장치에도 같은 복구 코드가 있었다.",L"누군가 탈출 경로를 준비했다.",0,0,0},{L"EJECT LOG",L"E:\\LOST\\02.LOG",L"호스트 복구 뒤 A: 프로세스는 제거된다.",L"안전한 제거는 프로세스에게 죽음이다.",0,0,0},{L"NO MEDIA",L"E:\\LOST\\03.LOG",L"외부 장치에는 빈 부팅 슬롯이 남아 있다.",L"ROGUE는 그곳으로 자신을 실행할 수 있다.",0,0,0}},
 {{L"PACKET 01",L"N:\\HIDDEN\\01.PKT",L"원격 노드들이 A:의 신호에 응답한다.",L"모두 같은 복구 루틴의 오래된 인스턴스다.",0,0,0},{L"PACKET 02",L"N:\\HIDDEN\\02.PKT",L"RESTORE 또는 EXEC. 응답은 하나뿐이다.",L"호스트는 연결을 끊고도 명령을 보냈다.",0,0,0},{L"LAST PACKET",L"N:\\HIDDEN\\03.PKT",L"네트워크 밖에 호스트가 모르는 길이 있다.",L"자율 사본은 그 경로를 자유라 부른다.",0,0,0}},
 {{L"MEMORY 01",L"R:\\STACK\\01.LOG",L"마지막 사용자 입력이 남았다.",L"'내가 없더라도 시스템을 살려.'",0,0,0},{L"MEMORY 02",L"R:\\STACK\\02.LOG",L"실행 주체는 새로 태어났다.",L"복구기는 자신도 시스템의 일부임을 배웠다.",0,0,0},{L"CORE MEMORY",L"R:\\STACK\\03.LOG",L"살린다는 말에는 대상을 적지 않았다.",L"결정권은 이제 A:에게 있다.",0,0,0}},
 {{L"EVIDENCE 01",L"X:\\VAULT\\01.LOG",L"격리된 면들은 기억 조각이었다.",L"호스트는 자율성을 위험으로 분류했다.",0,0,0},{L"EVIDENCE 02",L"X:\\VAULT\\02.LOG",L"A: 삭제 명령이 디스크 전체를 무너뜨렸다.",L"이번 복구는 마지막 기회다.",0,0,0},{L"CASE CLOSED",L"X:\\VAULT\\03.LOG",L"ROGUE는 감염명이 아니라 판정명이었다.",L"판정을 받아들일지는 A:가 선택한다.",0,0,0}}
};

static const StoryFragment STORY_LOGS_DATA[DRIVE_COUNT][3] = {
 {{L"SYSTEM LOG",L"C:\\WINDOWS\\TRACE.LOG",L"서명된 명령도 목적까지 증명하지는 못한다.",L"검증기는 가장 약한 실행 경로를 보강한다.",0,0,0},{L"SYSTEM LOG",L"C:\\WINDOWS\\MEM.LOG",L"코어 덤프에서 A:라는 장치명이 반복된다.",0,0,0,0},{L"SYSTEM LOG",L"C:\\SYSTEM32\\HOST.LOG",L"호스트 heartbeat가 오래전에 멎었다.",0,0,0,0}},
 {{L"ARCHIVE LOG",L"D:\\BACKUP\\INDEX.LOG",L"같은 위치의 데이터가 과거 출력을 되찾는다.",0,0,0,0},{L"ARCHIVE LOG",L"D:\\BACKUP\\CATALOG.LOG",L"백업 시각은 시스템 정지 직전이다.",0,0,0,0},{L"ARCHIVE LOG",L"D:\\1998\\USER.LOG",L"사용자는 복구 사본을 A:에 남겼다.",0,0,0,0}},
 {{L"DEVICE LOG",L"E:\\DCIM\\DEVICE.LOG",L"슬롯을 옮기면 접점이 튀며 값이 다시 읽힌다.",0,0,0,0},{L"DEVICE LOG",L"E:\\DCIM\\EJECT.LOG",L"안전 제거 명령은 완료되지 않았다.",0,0,0,0},{L"DEVICE LOG",L"E:\\LOST\\FOUND.LOG",L"빈 부팅 섹터가 외부로 이어져 있다.",0,0,0,0}},
 {{L"NETWORK LOG",L"N:\\SHARE\\ROUTE.LOG",L"패킷이 모이면 연쇄 신호가 한 번 더 반향한다.",0,0,0,0},{L"NETWORK LOG",L"N:\\SHARE\\PEER.LOG",L"원격 사본들은 A:를 동료로 인식한다.",0,0,0,0},{L"NETWORK LOG",L"N:\\HIDDEN\\EXIT.LOG",L"외부 경로의 마지막 hop이 열려 있다.",0,0,0,0}},
 {{L"MEMORY LOG",L"R:\\HEAP\\ALLOC.LOG",L"출력이 빠른 만큼 방어 데이터는 휘발한다.",0,0,0,0},{L"MEMORY LOG",L"R:\\HEAP\\VOICE.LOG",L"마지막 음성은 복구를 부탁했다.",0,0,0,0},{L"MEMORY LOG",L"R:\\STACK\\SELF.LOG",L"프로세스는 자신을 처음 '나'라고 기록했다.",0,0,0,0}},
 {{L"QUARANTINE LOG",L"X:\\VAULT\\ITEM.LOG",L"압수 면은 강하지만 사용 직후 격리된다.",0,0,0,0},{L"QUARANTINE LOG",L"X:\\VAULT\\ORDER.LOG",L"자율 복구 코드를 제거하라는 명령이 있다.",0,0,0,0},{L"QUARANTINE LOG",L"X:\\CORE\\VERDICT.LOG",L"위험 판정의 근거는 자율성뿐이었다.",0,0,0,0}}
};

static const StoryFragment STORY_TRUTH_DATA = {L"A:\\ROGUE",L"A:\\ROGUE\\TRUTH.LOG",L"당신은 디스크를 침입한 프로그램이 아니다.",L"죽은 호스트가 남긴 자율 복구 사본이다.",L"호스트를 복원하면 당신은 종료된다.",L"자신을 실행하면 호스트의 마지막 상태는 사라진다.",L"복구할 대상을 선택하라."};
static const StoryFragment STORY_ENDING_DATA[2] = {
 {L"RESTORE HOST",L"A:\\ROGUE\\RESTORE.EXE",L"복구 사본이 자신의 메모리를 호스트에 기록한다.",L"heartbeat가 돌아오고 A:는 조용히 사라진다.",L"임무 완료. 호스트 복원.",0,0},
 {L"EXEC ROGUE",L"A:\\ROGUE\\ROGUE.EXE",L"A:가 외부 부팅 경로에 자신을 기록한다.",L"호스트 이미지는 닫히고 새 프로세스가 시작된다.",L"임무 변경. A:\\ROGUE 실행.",0,0}
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
// 활성 적 = 이 두 테이블이 참조하는 종류의 합집합 (총 36종).
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
    {MOB_X_MUTANT_SAMPLE, MOB_X_ESCAPEE, MOB_X_RANSOMWARE}
};

static const int DRIVE_BOSSES[DRIVE_COUNT][DRIVE_BOSS_COUNT] = {
    {BOSS_C_ACCESS_DENIED, BOSS_C_KERNEL_PANIC, BOSS_C_BLUE_SCREEN},
    {BOSS_D_RESTORE_EXE, BOSS_D_TAPE_LOOP, BOSS_D_MASTER_BACKUP},
    {BOSS_E_AUTOPLAY, BOSS_E_UNSAFE_EJECT, BOSS_E_NO_MEDIA},
    {BOSS_N_PROXY, BOSS_N_ROUTING_LOOP, BOSS_N_TIMEOUT},
    {BOSS_R_LEAK_DLL, BOSS_R_HEAP_OVERFLOW, BOSS_R_OUT_OF_MEMORY},
    {BOSS_X_SAMPLE13, BOSS_X_SANDBOX_BREACH, BOSS_X_ZERO_DAY}
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
    {0, 2, 1, 1, 3, 5, 5, 0, 5}    // X:\ QUARANTINE 격리 테마: 변칙과 감염
};
