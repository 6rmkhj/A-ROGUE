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
    {L"숫자", L"NUM", L"값만큼 효과, 값만큼 바이트", 0, 0, AR_COLOR(205, 221, 232)},
    {L"화염", L"FIRE", L"공격 시 추가 피해와 화상", 24, 8, AR_COLOR(255, 92, 72)},
    {L"방벽", L"SHLD", L"방어 슬롯에서 효과 2배", 16, 7, AR_COLOR(76, 170, 255)},
    {L"흡수", L"LEECH", L"공격 피해 일부를 회복", 22, 6, AR_COLOR(182, 96, 220)},
    {L"와일드", L"WILD", L"어느 슬롯에서도 높은 출력", 32, 9, AR_COLOR(255, 205, 64)},
    {L"증폭", L"BOOST", L"증폭 슬롯에서 보너스 2배", 18, 5, AR_COLOR(95, 225, 176)},
    {L"메아리", L"ECHO", L"연쇄 슬롯에서 직전 효과 반복", 20, 5, AR_COLOR(255, 139, 209)},
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
static const wchar_t* const SLOT_SHORT_NAMES[SLOT_COUNT] = {L"ATK", L"DEF", L"AMP", L"CHAIN"};
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
    {L"과잉 할당", L"층 용량 +60B, 모든 적 최대 HP +30%."},
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
    {L"글리치", L"GLITCH", 14, 4, 3, AR_COLOR(95, 225, 176)},
    {L"웜", L"WORM", 18, 5, 2, AR_COLOR(170, 230, 80)},
    {L"스파이웨어", L"SPY", 22, 6, 4, AR_COLOR(255, 193, 77)},
    {L"트로이 목마", L"TROJAN", 25, 7, 3, AR_COLOR(255, 110, 95)},
    {L"파편", L"FRAG", 28, 7, 6, AR_COLOR(150, 150, 255)},
    {L"오염 캐시", L"CACHE", 24, 6, 7, AR_COLOR(90, 190, 230)},
    {L"데몬", L"DAEMON", 30, 8, 5, AR_COLOR(210, 105, 235)},
    {L"루트킷", L"ROOTKIT", 34, 9, 5, AR_COLOR(235, 80, 130)},
    {L"디스크 오류", L"DISK ERROR", 38, 6, 5, AR_COLOR(255, 104, 87)},
    {L"부트 섹터", L"BOOT SECTOR", 52, 8, 7, AR_COLOR(255, 170, 70)},
    {L"F O R M A T", L"FORMAT", 68, 10, 9, AR_COLOR(245, 65, 90)}
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
static const wchar_t* const FLOOR_NAMES[3] = {L"DISK", L"BOOT", L"FORMAT"};
