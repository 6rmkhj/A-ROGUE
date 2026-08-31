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
    {L"배드 섹터", L"층을 내려갈 때 무작위 면 하나가 영구 손상됩니다. 보상을 설치해도 복구되지 않습니다."},
    {L"읽기 오류", L"경고된 주사위가 턴 확정 순간 다시 굴러갑니다."},
    {L"조각화", L"같은 결과가 여러 개면 뒤쪽 결과가 비활성화됩니다."},
    {L"과잉 할당", L"층 용량 +60B, 모든 적 최대 체력 +30%."},
    {L"체크섬", L"굴림 출력 합이 짝수면 공격 피해 +2."}
};

enum EnemyKind {
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
    ENEMY_KIND_COUNT
};

struct EnemyInfo {
    const wchar_t* name;
    const wchar_t* code;
    int hp;
    int damage;
    int guard;
    uint32_t color;
};

static const EnemyInfo ENEMY_INFO[ENEMY_KIND_COUNT] = {
    {L"글리치", L"글리치", 14, 4, 3, AR_COLOR(95, 225, 176)},
    {L"웜", L"웜", 18, 5, 2, AR_COLOR(170, 230, 80)},
    {L"스파이웨어", L"스파이웨어", 22, 6, 4, AR_COLOR(255, 193, 77)},
    {L"트로이 목마", L"트로이 목마", 25, 7, 3, AR_COLOR(255, 110, 95)},
    {L"파편", L"파편", 28, 7, 6, AR_COLOR(150, 150, 255)},
    {L"오염 캐시", L"오염 캐시", 24, 6, 7, AR_COLOR(90, 190, 230)},
    {L"데몬", L"데몬", 30, 8, 5, AR_COLOR(210, 105, 235)},
    {L"루트킷", L"루트킷", 34, 9, 5, AR_COLOR(235, 80, 130)},
    {L"디스크 오류", L"디스크 오류", 38, 6, 5, AR_COLOR(255, 104, 87)},
    {L"부트 섹터", L"부트 섹터", 52, 8, 7, AR_COLOR(255, 170, 70)},
    {L"포맷", L"포맷", 68, 10, 9, AR_COLOR(245, 65, 90)}
};

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
     MOD_READ_ERROR, MOD_FRAGMENTATION, PERK_HEAL_ON_WIN, 5, L"전투 승리 시 체력 5 회복",
     {L"E:\\", L"E:\\DCIM", L"E:\\DCIM\\LOST"}, L"E:\\ → DCIM → LOST", AR_COLOR(95, 225, 176)},
    {L"N:\\", L"NETWORK", L"네트워크 공유 볼륨. 원격 격리로 감염 개체가 약화되어 있습니다.",
     MOD_READ_ERROR, MOD_CHECKSUM, PERK_ENEMY_HP_DOWN, 10, L"모든 적 최대 체력 -10%",
     {L"N:\\", L"N:\\SHARE", L"N:\\SHARE\\HIDDEN"}, L"N:\\ → SHARE → HIDDEN", AR_COLOR(90, 190, 230)},
    {L"R:\\", L"RAMDISK", L"휘발성 램디스크. 접근은 빠르지만 데이터가 쉽게 증발합니다.",
     MOD_FRAGMENTATION, MOD_CHECKSUM, PERK_ATTACK_UP, 1, L"공격 피해 +1 · 최대 체력 -4",
     {L"R:\\", L"R:\\HEAP", L"R:\\HEAP\\STACK"}, L"R:\\ → HEAP → STACK", AR_COLOR(210, 105, 235)},
    {L"X:\\", L"QUARANTINE", L"격리 구역. 위험하지만 압수된 특수 데이터가 남아 있습니다.",
     MOD_OVERALLOC, MOD_READ_ERROR, PERK_BONUS_FACE, 1, L"시작 시 무작위 특수 면 1개 설치",
     {L"X:\\", L"X:\\VAULT", L"X:\\VAULT\\CORE"}, L"X:\\ → VAULT → CORE", AR_COLOR(255, 92, 82)}
};
