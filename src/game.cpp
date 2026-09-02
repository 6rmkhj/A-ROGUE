#include <windows.h>
#include "game.h"

static uint32_t NextRandom(GameState* game) {
    uint32_t x = game->rng;
    if (x == 0) x = 0xA341316Cu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng = x;
    return x;
}

static int RandomRange(GameState* game, int limit) {
    if (limit <= 1) return 0;
    return (int)(NextRandom(game) % (uint32_t)limit);
}

static int ClampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void PushLog(GameState* game, const wchar_t* text) {
    for (int i = 4; i > 0; --i) lstrcpynW(game->logs[i], game->logs[i - 1], 96);
    lstrcpynW(game->logs[0], text, 96);
}

static void PushLog2(GameState* game, const wchar_t* format, const wchar_t* name, int value) {
    wchar_t buffer[96];
    wsprintfW(buffer, format, name, value);
    PushLog(game, buffer);
}

static void ClearTurnTrace(GameState* game) {
    game->turnTraceCount = 0;
    game->turnTraceOverflow = 0;
    ZeroMemory(game->turnTrace, sizeof(game->turnTrace));
}

static void PushTurnTrace(GameState* game, const wchar_t* text) {
    if (game->turnTraceCount >= TURN_TRACE_CAP) { game->turnTraceOverflow = 1; return; }
    lstrcpynW(game->turnTrace[game->turnTraceCount++], text, 96);
}

// ---------------------------------------------------------------------------
// 안전 조회와 역할 판정. 보스 여부는 enum 범위가 아니라 role 메타데이터다.
// ---------------------------------------------------------------------------

int IsValidEnemyKind(int kind) {
    return kind >= 0 && kind < ENEMY_KIND_COUNT;
}

const EnemyInfo* GetEnemyInfoOrUnknown(int kind) {
    if (!IsValidEnemyKind(kind)) return &UNKNOWN_ENEMY_INFO;
    return &ENEMY_INFO[kind];
}

int IsBossKind(int kind) {
    return GetEnemyInfoOrUnknown(kind)->role == ROLE_BOSS;
}

int FaceCost(const Face* face) {
    if (!face) return 0;
    if (face->kind == FACE_NUMBER) return face->value;
    if (face->kind >= FACE_KIND_COUNT) return 0;
    return FACE_INFO[face->kind].cost;
}

int FacePower(const Face* face) {
    if (!face || face->damaged || face->kind == FACE_EMPTY) return 0;
    if (face->quarantined != QUAR_NONE) return 0;   // 격리: 출력만 0, 비용·종류는 유지
    if (face->kind == FACE_NUMBER) return face->value;
    if (face->kind >= FACE_KIND_COUNT) return 0;
    return FACE_INFO[face->kind].power;
}

int DeckBytes(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) total += FaceCost(&game->dice[d].faces[f]);
    return total;
}

int IsTsrInstalled(const GameState* game, int tsr) {
    return tsr >= 0 && tsr < TSR_COUNT && game->tsrInstalled[tsr];
}

int TsrBytes(const GameState* game) {
    int total = 0;
    for (int i = 0; i < TSR_COUNT; ++i) if (game->tsrInstalled[i]) total += TSR_INFO[i].cost;
    return total;
}

// 면과 상주 프로그램이 같은 용량 풀을 나눠 쓴다. 한도 비교는 전부 이 값으로 한다.
int UsedBytes(const GameState* game) {
    return DeckBytes(game) + TsrBytes(game);
}

int InstalledTsrCount(const GameState* game) {
    int total = 0;
    for (int i = 0; i < TSR_COUNT; ++i) if (game->tsrInstalled[i]) ++total;
    return total;
}

int InstalledTsrAt(const GameState* game, int slot) {
    for (int i = 0; i < TSR_COUNT; ++i) {
        if (!game->tsrInstalled[i]) continue;
        if (slot == 0) return i;
        --slot;
    }
    return -1;
}

int IsEnemyScanned(const GameState* game, int kind) {
    if (!game || !IsValidEnemyKind(kind)) return 0;
    return game->enemyScanned[kind] != 0;
}

int IsModifierActive(const GameState* game, int modifier) {
    return game->modifierA == modifier || game->modifierB == modifier;
}

const DifficultyInfo* DifficultyInfoOrNull(int difficulty) {
    if (difficulty < 0 || difficulty >= DIFFICULTY_COUNT) return 0;
    return &DIFFICULTY_INFO[difficulty];
}

int CorruptPercent(const GameState* game) {
    const DifficultyInfo* info = DifficultyInfoOrNull(game->difficulty);
    return info ? info->corruptPercent : DIFFICULTY_BASE_PERCENT;
}

// 오염(관통) 피해에만 난이도 배율을 건다. 반올림하되 원래 피해가 있었다면
// 최소 1은 남겨, 초급자에서도 오염이 완전히 무해해지지는 않게 한다.
int ScaleCorruptDamage(const GameState* game, int damage) {
    if (damage <= 0) return damage;
    int scaled = (damage * CorruptPercent(game) + 50) / 100;
    return scaled < 1 ? 1 : scaled;
}

// 드라이브가 아직 확정되지 않았으면(-1) 모든 특성이 0으로 죽는다.
static int DrivePerkValue(const GameState* game, int perk) {
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) return 0;
    const DriveInfo* drive = &DRIVE_INFO[game->selectedDrive];
    return drive->perk == perk ? drive->perkValue : 0;
}

int EffectiveCapacity(const GameState* game) {
    int floor = ClampInt(game->floor, 0, 2);
    int capacity = FLOOR_CAPACITY[floor];
    if (IsModifierActive(game, MOD_OVERALLOC)) capacity += 60;
    capacity += DrivePerkValue(game, PERK_CAPACITY);
    if (IsTsrInstalled(game, TSR_HIMEM)) capacity += TSR_INFO[TSR_HIMEM].value;
    // CACHE 디렉터리의 임시 한도. 다음 층 용량 검사 전에 반드시 0으로 돌아간다.
    capacity += game->directory.floorCapacityBonus;
    return capacity;
}

int SectorRepairAmount(const GameState* game) {
    return SECTOR_REPAIR_HEAL[ClampInt(game->floor, 0, 2)];
}

int NonEmptyFaceCount(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) if (game->dice[d].faces[f].kind != FACE_EMPTY) ++total;
    }
    return total;
}

int UsableFaceCount(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) if (FacePower(&game->dice[d].faces[f]) >= 1) ++total;
    }
    return total;
}

int LivingEnemyCount(const GameState* game) {
    int total = 0;
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive && game->enemies[i].hp > 0) ++total;
    return total;
}

const Face* RolledFace(const GameState* game, int dieIndex) {
    if (dieIndex < 0 || dieIndex >= 3) return 0;
    int face = game->dice[dieIndex].rolledFace;
    if (face < 0 || face >= 6) return 0;
    return &game->dice[dieIndex].faces[face];
}

int SlotLockedThisTurn(const GameState* game, int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) return 0;
    return game->boss.lockedSlot[slot];
}

int SlotLockedNextTurn(const GameState* game, int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) return 0;
    return game->boss.nextLockedSlot[slot];
}

int ResolveOrderReversed(const GameState* game) {
    return game->boss.reversed;
}

// ---------------------------------------------------------------------------
// 보스 기믹 공용 도우미
// ---------------------------------------------------------------------------

static EnemyState* BossEnemy(GameState* game) {
    for (int i = 0; i < game->enemyCount; ++i) {
        EnemyState* enemy = &game->enemies[i];
        if (enemy->alive && IsBossKind(enemy->kind)) return enemy;
    }
    return 0;
}

static const BossGimmickInfo* GimmickInfo(const GameState* game) {
    int gimmick = game->boss.gimmick;
    if (gimmick <= GIMMICK_NONE || gimmick >= GIMMICK_COUNT) gimmick = GIMMICK_NONE;
    return &BOSS_GIMMICK_INFO[gimmick];
}

static void ClearGimmickAnnouncements(BossRuntime* boss) {
    for (int i = 0; i < SLOT_COUNT; ++i) { boss->lockedSlot[i] = 0; boss->nextLockedSlot[i] = 0; }
    boss->reversed = 0;
    boss->nextReversed = 0;
    boss->offlineDie = -1;
    boss->nextOfflineDie = -1;
}

// 전투 종료 경로 공용 정리. 임시 격리·오프라인·잠금·역전을 모두 걷어낸다.
// 영구 EMPTY(kind 자체가 바뀐 면)만 그대로 남는다.
static void GimmickCombatEnd(GameState* game) {
    int released = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            if (game->dice[d].faces[f].quarantined != QUAR_NONE) {
                game->dice[d].faces[f].quarantined = QUAR_NONE;
                ++released;
            }
        }
        game->dice[d].offline = 0;
    }
    if (released > 0) PushLog(game, L"격리가 해제되었습니다. 모든 면이 복구되었습니다.");
    ZeroMemory(&game->boss, sizeof(game->boss));
    game->boss.offlineDie = -1;
    game->boss.nextOfflineDie = -1;
    game->boss.bestSlotLastTurn = -1;
    game->boss.nextTargetDie = -1;
    game->boss.nextTargetFace = -1;
}

// 전투 시작 시 보스 기믹 런타임을 초기화한다.
static void GimmickInitCombat(GameState* game) {
    EnemyState* enemy = BossEnemy(game);
    ZeroMemory(&game->boss, sizeof(game->boss));
    BossRuntime* boss = &game->boss;
    boss->offlineDie = -1;
    boss->nextOfflineDie = -1;
    boss->bestSlotLastTurn = -1;
    boss->nextTargetDie = -1;
    boss->nextTargetFace = -1;
    if (!enemy) return;
    const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
    boss->gimmick = info->gimmick;
    const BossGimmickInfo* gi = GimmickInfo(game);
    switch (boss->gimmick) {
    case GIMMICK_TIMEOUT:
        boss->countdown = gi->p1;
        break;
    case GIMMICK_LEAK:
    case GIMMICK_HEAP_OVERFLOW:
    case GIMMICK_OUT_OF_MEMORY:
        boss->gaugeMax = gi->p1;
        break;
    case GIMMICK_SAMPLE13:
    case GIMMICK_ZERO_DAY:
        boss->gaugeMax = gi->p1;
        break;
    case GIMMICK_RESTORE_POINT:
        boss->checkpointHp = enemy->hp;
        break;
    case GIMMICK_MASTER_BACKUP:
        boss->checkpointHp = enemy->maxHp * gi->p2 / 100;
        break;
    default: break;
    }
    if (boss->gimmick != GIMMICK_NONE)
        PushLog2(game, L"보스 기믹 감지: %s", gi->name, 0);
}

// C:\ ACCESS.DENIED 잠금 순환: 증폭 → 공격 → 방어 → 연쇄
static int AccessDeniedSlot(int turn) {
    static const int order[SLOT_COUNT] = {SLOT_AMPLIFY, SLOT_ATTACK, SLOT_DEFEND, SLOT_CHAIN};
    return order[((turn / 2) - 1) & 3];
}

static int OfflineFiresOn(int gimmick, int turn) {
    if (gimmick == GIMMICK_AUTOPLAY) return turn >= 3 && turn % 3 == 0;
    if (gimmick == GIMMICK_UNSAFE_EJECT) return turn >= 2 && turn % 2 == 0;
    if (gimmick == GIMMICK_NO_MEDIA) return turn >= 2 && turn % 4 != 0;
    return 0;
}

static int OfflineTargetDie(int gimmick, int turn) {
    if (gimmick == GIMMICK_AUTOPLAY) return ((turn / 3) - 1) % 3;
    if (gimmick == GIMMICK_UNSAFE_EJECT) return ((turn / 2) - 1) % 3;
    return turn % 3;   // NO.MEDIA
}

static int ReverseFiresOn(int gimmick, int turn) {
    if (gimmick == GIMMICK_PROXY) return turn >= 3 && turn % 3 == 0;
    if (gimmick == GIMMICK_ROUTING_LOOP) return turn >= 2 && turn % 2 == 0;
    return 0;
}

// 턴 시작: 이번 턴 잠금·오프라인·역전을 확정하고 다음 턴을 예고한다.
// 연출 기록. 규칙 판정에는 절대 참여하지 않고 화면이 읽기만 한다.
static void RecordFx(GameState* game, int fx, int a, int b) {
    game->boss.firedFx = (uint8_t)fx;
    game->boss.fxA = (int8_t)a;
    game->boss.fxB = (int8_t)b;
}

static void GimmickTurnBegin(GameState* game) {
    BossRuntime* boss = &game->boss;
    ClearGimmickAnnouncements(boss);
    if (boss->gimmick == GIMMICK_NONE || !BossEnemy(game)) return;
    int turn = game->turn;
    wchar_t buffer[96];
    switch (boss->gimmick) {
    case GIMMICK_ACCESS_DENIED:
        if (turn >= 2 && turn % 2 == 0) {
            boss->lockedSlot[AccessDeniedSlot(turn)] = 1;
            RecordFx(game, GIMMICK_ACCESS_DENIED, AccessDeniedSlot(turn), -1);
        }
        if (turn + 1 >= 2 && (turn + 1) % 2 == 0) boss->nextLockedSlot[AccessDeniedSlot(turn + 1)] = 1;
        break;
    case GIMMICK_KERNEL_PANIC:
        // 직전 턴 최고 출력 슬롯이 잠긴다. 다음 턴 대상은 이번 턴 플레이에 달려
        // 있으므로 예고는 규칙 문구로 대신한다.
        if (turn >= 2 && boss->bestSlotLastTurn >= 0 && boss->bestSlotLastTurn < SLOT_COUNT) {
            boss->lockedSlot[boss->bestSlotLastTurn] = 1;
            RecordFx(game, GIMMICK_KERNEL_PANIC, boss->bestSlotLastTurn, -1);
        }
        break;
    case GIMMICK_BLUE_SCREEN:
        if (turn >= 3 && turn % 3 == 0) {
            boss->lockedSlot[SLOT_AMPLIFY] = 1; boss->lockedSlot[SLOT_CHAIN] = 1;
            RecordFx(game, GIMMICK_BLUE_SCREEN, SLOT_AMPLIFY, SLOT_CHAIN);
        }
        if ((turn + 1) >= 3 && (turn + 1) % 3 == 0) { boss->nextLockedSlot[SLOT_AMPLIFY] = 1; boss->nextLockedSlot[SLOT_CHAIN] = 1; }
        break;
    case GIMMICK_AUTOPLAY:
    case GIMMICK_UNSAFE_EJECT:
    case GIMMICK_NO_MEDIA:
        if (OfflineFiresOn(boss->gimmick, turn)) {
            boss->offlineDie = (int8_t)OfflineTargetDie(boss->gimmick, turn);
            RecordFx(game, boss->gimmick, boss->offlineDie, 0);
        } else if (boss->gimmick == GIMMICK_NO_MEDIA) {
            // 발동하지 않는 인식 턴. 벌이 아니라 기다리던 턴이므로 연출도 반대다.
            RecordFx(game, GIMMICK_NO_MEDIA, -1, 1);
        }
        if (OfflineFiresOn(boss->gimmick, turn + 1)) boss->nextOfflineDie = (int8_t)OfflineTargetDie(boss->gimmick, turn + 1);
        break;
    case GIMMICK_PROXY:
    case GIMMICK_ROUTING_LOOP:
        boss->reversed = (uint8_t)ReverseFiresOn(boss->gimmick, turn);
        boss->nextReversed = (uint8_t)ReverseFiresOn(boss->gimmick, turn + 1);
        if (boss->reversed) RecordFx(game, boss->gimmick, -1, -1);
        break;
    case GIMMICK_TIMEOUT:
        if (boss->countdown <= 0) boss->reversed = 1;
        boss->nextReversed = (boss->countdown == 1);
        // 카운트다운은 매턴 보여 준다. 0이 되는 턴만 정지 연출로 커진다.
        RecordFx(game, GIMMICK_TIMEOUT, boss->countdown, boss->reversed ? 1 : 0);
        break;
    default: break;   // D:\ 복원, R:\ 압력, X:\ 격리는 턴말 훅에서 진행
    }
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (boss->lockedSlot[i]) {
            // 잠긴 슬롯에 남아 있던 주사위는 배치가 해제된다.
            for (int d = 0; d < 3; ++d) if (game->dice[d].assignedSlot == i) game->dice[d].assignedSlot = -1;
            wsprintfW(buffer, L"권한 거부: 이번 턴 %s 슬롯이 잠겼습니다.", SLOT_NAMES[i]);
            PushLog(game, buffer);
        }
    }
    if (boss->offlineDie >= 0) {
        wsprintfW(buffer, L"연결 끊김: 이번 턴 주사위 %d이(가) 오프라인입니다.", boss->offlineDie + 1);
        PushLog(game, buffer);
    }
    if (boss->reversed) PushLog(game, L"경고: 이번 턴 해결 순서가 역전됩니다.");
    else if (boss->nextReversed) PushLog(game, L"예고: 다음 턴 해결 순서가 역전됩니다.");
}

// 격리·삭제 대상 선정. 출력 가능한 면 중에서 고르고, 영구 삭제는 삭제 후에도
// 출력 가능한 면이 최소 하나 남는 경우로만 제한한다.
static int PickQuarantineTarget(GameState* game, int permanent, int8_t* outDie, int8_t* outFace) {
    int candidates[18], count = 0;
    int usable = UsableFaceCount(game);
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            const Face* face = &game->dice[d].faces[f];
            if (FacePower(face) < 1) continue;             // 이미 출력 0인 면은 위협이 아니다
            if (permanent && usable - 1 < 1) continue;     // 마지막 남은 출력 면은 지울 수 없다
            candidates[count++] = d * 6 + f;
        }
    }
    if (count == 0) return 0;
    int pick = candidates[RandomRange(game, count)];
    *outDie = (int8_t)(pick / 6);
    *outFace = (int8_t)(pick % 6);
    return 1;
}

static int QuarantineTargetValid(const GameState* game, int permanent) {
    const BossRuntime* boss = &game->boss;
    if (boss->nextTargetDie < 0 || boss->nextTargetDie >= 3) return 0;
    if (boss->nextTargetFace < 0 || boss->nextTargetFace >= 6) return 0;
    const Face* face = &game->dice[boss->nextTargetDie].faces[boss->nextTargetFace];
    if (FacePower(face) < 1) return 0;
    if (permanent && UsableFaceCount(game) - 1 < 1) return 0;
    return 1;
}

static void AnnounceQuarantineTarget(GameState* game, int permanent) {
    BossRuntime* boss = &game->boss;
    int8_t die = -1, face = -1;
    if (PickQuarantineTarget(game, permanent, &die, &face)) {
        boss->nextTargetDie = die;
        boss->nextTargetFace = face;
        boss->nextTargetPermanent = (uint8_t)permanent;
        wchar_t buffer[96];
        wsprintfW(buffer, permanent ? L"삭제 예고: 주사위 %d의 면 %d이(가) 표적입니다."
                                    : L"격리 예고: 주사위 %d의 면 %d이(가) 표적입니다.",
                  die + 1, face + 1);
        PushLog(game, buffer);
    } else {
        boss->nextTargetDie = -1;
        boss->nextTargetFace = -1;
        boss->nextTargetPermanent = 0;
    }
}

static int ActiveQuarantineCount(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d)
        for (int f = 0; f < 6; ++f)
            if (game->dice[d].faces[f].quarantined != QUAR_NONE) ++total;
    return total;
}

// mode: QUAR_COMBAT 또는 남은 턴 수. permanent가 1이면 면을 영구 EMPTY로 만든다.
static void FireQuarantine(GameState* game, int permanent, int mode) {
    BossRuntime* boss = &game->boss;
    if (!QuarantineTargetValid(game, permanent)) {
        // 예고된 면이 그새 손상·삭제됐다면 즉석에서 재선정한다.
        AnnounceQuarantineTarget(game, permanent);
        if (!QuarantineTargetValid(game, permanent)) {
            if (permanent) {
                // 영구 삭제가 불가능하면 임시 격리로 대체한다.
                AnnounceQuarantineTarget(game, 0);
                if (QuarantineTargetValid(game, 0)) { FireQuarantine(game, 0, QUAR_COMBAT); return; }
            }
            boss->nextTargetDie = -1;
            boss->nextTargetFace = -1;
            return;
        }
    }
    Face* face = &game->dice[boss->nextTargetDie].faces[boss->nextTargetFace];
    RecordFx(game, boss->gimmick, boss->nextTargetDie, boss->nextTargetFace);
    wchar_t buffer[96];
    if (permanent) {
        face->kind = FACE_EMPTY;
        face->value = 0;
        face->damaged = 0;
        face->quarantined = QUAR_NONE;
        wsprintfW(buffer, L"제로데이: 주사위 %d의 면 %d이(가) 영구 삭제되었습니다!", boss->nextTargetDie + 1, boss->nextTargetFace + 1);
    } else {
        face->quarantined = (uint8_t)mode;
        ++boss->quarantinesDone;
        wsprintfW(buffer, L"격리 발동: 주사위 %d의 면 %d 출력이 봉인되었습니다.", boss->nextTargetDie + 1, boss->nextTargetFace + 1);
    }
    PushLog(game, buffer);
    PushTurnTrace(game, buffer);
    boss->nextTargetDie = -1;
    boss->nextTargetFace = -1;
    boss->nextTargetPermanent = 0;
}

// 보스에게 복원·되감기 회복을 적용한다. 현재 체력이 목표보다 높으면 아무것도
// 하지 않아 복원이 오히려 체력을 낮추는 일이 없다.
static int RestoreBossHp(GameState* game, EnemyState* enemy, int targetHp, int cap, const wchar_t* label) {
    if (!enemy || !enemy->alive) return 0;
    int heal = targetHp - enemy->hp;
    if (heal <= 0) return 0;
    if (heal > cap) heal = cap;
    int hpBefore = enemy->hp;
    enemy->hp = ClampInt(enemy->hp + heal, 0, enemy->maxHp);
    heal = enemy->hp - hpBefore;
    if (heal > 0) {
        wchar_t buffer[96];
        wsprintfW(buffer, L"[%s] 보스 체력 +%d (%d → %d)", label, heal, hpBefore, enemy->hp);
        PushTurnTrace(game, buffer);
        PushLog(game, buffer);
    }
    return heal;
}

// 턴말 훅: 적 행동이 끝난 뒤 게이지·복원·격리·카운트다운을 진행한다.
static void GimmickTurnEnd(GameState* game) {
    // 임시 격리 타이머는 보스 생사와 무관하게 턴이 지나면 줄어든다.
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            Face* face = &game->dice[d].faces[f];
            if (face->quarantined != QUAR_NONE && face->quarantined != QUAR_COMBAT) {
                if (--face->quarantined == QUAR_NONE) {
                    wchar_t buffer[96];
                    wsprintfW(buffer, L"격리 해제: 주사위 %d의 면 %d이(가) 복구되었습니다.", d + 1, f + 1);
                    PushLog(game, buffer);
                }
            }
        }
    }
    BossRuntime* boss = &game->boss;
    EnemyState* enemy = BossEnemy(game);
    if (boss->gimmick == GIMMICK_NONE || !enemy) return;
    const BossGimmickInfo* gi = GimmickInfo(game);
    int turn = game->turn;
    wchar_t buffer[96];
    switch (boss->gimmick) {
    case GIMMICK_RESTORE_POINT:
        if (turn % gi->p1 == 0) {
            if (boss->restoresUsed < 2 && boss->windowDamage < gi->p2) {
                if (RestoreBossHp(game, enemy, boss->checkpointHp, gi->p3, L"복원 지점") > 0) {
                    ++boss->restoresUsed;
                    RecordFx(game, GIMMICK_RESTORE_POINT, -1, -1);
                }
            }
            boss->checkpointHp = enemy->hp;
            boss->windowDamage = 0;
        }
        break;
    case GIMMICK_TAPE_LOOP: {
        int totalCap = gi->p3 * 4;   // 되감기 총량 상한
        if (boss->damageThisTurn < gi->p2 && boss->restoredTotal < totalCap) {
            int cap = gi->p3;
            if (cap > totalCap - boss->restoredTotal) cap = totalCap - boss->restoredTotal;
            int healed = RestoreBossHp(game, enemy, enemy->maxHp, cap, L"테이프 루프");
            boss->restoredTotal += healed;
            if (healed > 0) RecordFx(game, GIMMICK_TAPE_LOOP, -1, -1);
        }
        break;
    }
    case GIMMICK_MASTER_BACKUP:
        if (boss->restoresUsed == 0 && enemy->hp * 100 < enemy->maxHp * gi->p1) {
            if (RestoreBossHp(game, enemy, boss->checkpointHp, gi->p3, L"마스터 백업") > 0) {
                boss->restoresUsed = 1;
                RecordFx(game, GIMMICK_MASTER_BACKUP, -1, -1);
            }
        }
        break;
    case GIMMICK_TIMEOUT:
        if (boss->reversed) {
            boss->countdown = gi->p1;
        } else {
            if (boss->damageThisTurn >= gi->p2 && boss->countdown < gi->p1) {
                ++boss->countdown;
                PushLog(game, L"타임아웃 지연: 카운트다운이 되감겼습니다.");
            }
            --boss->countdown;
            if (boss->countdown < 0) boss->countdown = 0;
        }
        break;
    case GIMMICK_LEAK:
    case GIMMICK_HEAP_OVERFLOW:
    case GIMMICK_OUT_OF_MEMORY:
        if (boss->empowered) {
            boss->empowered = 0;
            boss->gauge = 0;
            PushLog(game, L"강화 공격 이후 메모리 압력이 초기화되었습니다.");
        } else {
            if (boss->damageThisTurn >= gi->p2 && boss->gauge > 0) {
                int reduce = boss->gimmick == GIMMICK_OUT_OF_MEMORY ? 2 : 1;
                boss->gauge -= reduce;
                if (boss->gauge < 0) boss->gauge = 0;
                wsprintfW(buffer, L"압력 방출: 게이지 -%d (현재 %d/%d)", reduce, boss->gauge, boss->gaugeMax);
                PushLog(game, buffer);
            }
            ++boss->gauge;
            if (boss->gauge >= boss->gaugeMax) {
                boss->gauge = boss->gaugeMax;
                boss->empowered = 1;
                RecordFx(game, boss->gimmick, -1, -1);
                PushLog(game, L"경고: 압력 한계. 다음 턴 강화 공격이 발동합니다!");
            }
        }
        break;
    case GIMMICK_SAMPLE13:
        ++boss->gauge;
        if (boss->gauge >= gi->p1) {
            if (boss->quarantinesDone < gi->p2) FireQuarantine(game, 0, QUAR_COMBAT);
            boss->gauge = 0;
        } else if (boss->gauge == gi->p1 - 1 && boss->nextTargetDie < 0) {
            if (boss->quarantinesDone < gi->p2) AnnounceQuarantineTarget(game, 0);
        }
        break;
    case GIMMICK_SANDBOX_BREACH:
        if (turn >= gi->p1 && turn % gi->p1 == 0) {
            if (ActiveQuarantineCount(game) < 2) FireQuarantine(game, 0, gi->p2);
        }
        if ((turn + 1) % gi->p1 == 0 && ActiveQuarantineCount(game) < 2) AnnounceQuarantineTarget(game, 0);
        break;
    case GIMMICK_ZERO_DAY:
        if (boss->damageThisTurn >= gi->p2 && boss->gauge > 0) {
            --boss->gauge;
            wsprintfW(buffer, L"오염 지연: 게이지 -1 (현재 %d/%d)", boss->gauge, boss->gaugeMax);
            PushLog(game, buffer);
        }
        ++boss->gauge;
        if (boss->gauge >= gi->p1) {
            FireQuarantine(game, 1, 0);
            boss->gauge = 0;
        } else if (boss->gauge == gi->p1 - 1 && boss->nextTargetDie < 0) {
            AnnounceQuarantineTarget(game, 1);
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// 런 준비
// ---------------------------------------------------------------------------

void InitTitle(GameState* game) {
    ZeroMemory(game, sizeof(*game));
    game->phase = PHASE_TITLE;
    game->rewardChoiceCount = 3;
    game->selectedDie = -1;
    game->selectedReward = -1;
    game->selectedDrive = -1;
    game->difficulty = -1;
    game->boss.offlineDie = -1;
    game->boss.nextOfflineDie = -1;
    game->boss.bestSlotLastTurn = -1;
    game->boss.nextTargetDie = -1;
    game->boss.nextTargetFace = -1;
}

static void SetupStartingDice(GameState* game) {
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            game->dice[d].faces[f].kind = FACE_NUMBER;
            game->dice[d].faces[f].value = (uint8_t)(f + 1);
            game->dice[d].faces[f].damaged = 0;
            game->dice[d].faces[f].quarantined = QUAR_NONE;
        }
        game->dice[d].rolledFace = 0;
        game->dice[d].assignedSlot = -1;
        game->dice[d].disabled = 0;
        game->dice[d].unstable = 0;
        game->dice[d].offline = 0;
    }
}

static void PickDriveChoices(GameState* game) {
    int count = 0;
    while (count < 3) {
        int pick = RandomRange(game, DRIVE_COUNT);
        int duplicate = 0;
        for (int i = 0; i < count; ++i) if (game->driveChoices[i] == pick) duplicate = 1;
        if (!duplicate) game->driveChoices[count++] = pick;
    }
}

// 카드 3장에 서로 다른 난이도를 배정한다. 드라이브 추첨과 같은 난수열을 쓰면
// 이후 굴림 순서가 통째로 밀리므로, 시드에서 파생한 독립 난수를 쓴다.
// 5종을 섞어 앞 3개만 가져오므로 중복이 나올 수 없다.
static void PickDriveDifficulties(GameState* game, uint32_t seed) {
    uint32_t rng = seed ? seed : 0x5EED1EEDu;
    int pool[DIFFICULTY_COUNT];
    for (int i = 0; i < DIFFICULTY_COUNT; ++i) pool[i] = i;
    for (int i = DIFFICULTY_COUNT - 1; i > 0; --i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        int j = (int)(rng % (uint32_t)(i + 1));
        int swap = pool[i]; pool[i] = pool[j]; pool[j] = swap;
    }
    for (int i = 0; i < 3; ++i) game->driveDifficulty[i] = pool[i];
}

// 시드에서 파생한 독립 난수로 세 몹의 순서를 섞고 A,B / C,A / B,C로 배치해
// 6개 일반 전투에서 각 몹이 정확히 두 번씩 등장하게 한다.
static void BuildMobSchedule(GameState* game, uint32_t seed) {
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) return;
    uint32_t rng = seed ? seed : 0xA5A5A5A5u;
    int order[3] = {0, 1, 2};
    for (int i = 2; i > 0; --i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        int j = (int)(rng % (uint32_t)(i + 1));
        int swap = order[i]; order[i] = order[j]; order[j] = swap;
    }
    const int* mobs = DRIVE_MOBS[game->selectedDrive];
    int a = mobs[order[0]], b = mobs[order[1]], c = mobs[order[2]];
    game->mobSchedule[0] = a; game->mobSchedule[1] = b;   // 1층
    game->mobSchedule[2] = c; game->mobSchedule[3] = a;   // 2층
    game->mobSchedule[4] = b; game->mobSchedule[5] = c;   // 3층
    game->mobScheduleReady = 1;
}

void NewRun(GameState* game, uint32_t seed) {
    ZeroMemory(game, sizeof(*game));
    game->rng = seed ? seed : 0xC0FFEE11u;
    // 경로 전용 난수는 런 seed에서 고정 salt로 파생한다. 드라이브는 마운트 시 섞인다.
    game->directory.rng = game->rng ^ 0x4B1D5A17u;
    game->directory.previousKind = DIR_NODE_NONE;
    game->pendingContinuation = CONTINUE_NONE;
    game->rewardChoiceCount = 3;
    game->rewardTier = 0;
    game->phase = PHASE_DRIVE_SELECT;
    game->floor = 0;
    game->encounter = 0;
    game->playerMaxHp = 40;
    game->playerHp = 40;
    game->selectedDie = -1;
    game->selectedReward = -1;
    game->selectedDrive = -1;
    game->difficulty = -1;
    game->boss.offlineDie = -1;
    game->boss.nextOfflineDie = -1;
    game->boss.bestSlotLastTurn = -1;
    game->boss.nextTargetDie = -1;
    game->boss.nextTargetFace = -1;
    SetupStartingDice(game);
    PickDriveChoices(game);
    PickDriveDifficulties(game, game->rng ^ 0x9E3779B9u);
    PushLog(game, L"A:\\ROGUE 부팅 완료. 탐색할 볼륨을 선택하십시오.");
}

// 경로 전용 난수는 아래 디렉터리 절에서 정의된다. 마운트 시 한 번 섞어 준다.
static void MixDirectoryDrive(GameState* game);

void SelectDrive(GameState* game, int choiceIndex) {
    if (game->phase != PHASE_DRIVE_SELECT || choiceIndex < 0 || choiceIndex >= 3) return;
    game->selectedDrive = game->driveChoices[choiceIndex];
    game->difficulty = game->driveDifficulty[choiceIndex];
    const DriveInfo* drive = &DRIVE_INFO[game->selectedDrive];
    game->modifierA = drive->modifierA;
    game->modifierB = drive->modifierB;
    if (drive->perk == PERK_MAX_HP) {
        game->playerMaxHp += drive->perkValue;
        game->playerHp = ClampInt(game->playerHp + drive->perkValue, 1, game->playerMaxHp);
    } else if (drive->perk == PERK_ATTACK_UP) {
        game->playerMaxHp -= 4;
        game->playerHp = ClampInt(game->playerHp, 1, game->playerMaxHp);
    } else if (drive->perk == PERK_BONUS_FACE) {
        int kind = FACE_FIRE + RandomRange(game, FACE_ECHO - FACE_FIRE + 1);
        Face* face = &game->dice[RandomRange(game, 3)].faces[RandomRange(game, 6)];
        face->kind = (uint8_t)kind;
        face->value = (uint8_t)FACE_INFO[kind].power;
        PushLog2(game, L"격리 데이터 회수: %s 면 설치 (%dB).", FACE_INFO[kind].name, FaceCost(face));
    }
    BuildMobSchedule(game, NextRandom(game));
    MixDirectoryDrive(game);
    wchar_t buffer[96];
    wsprintfW(buffer, L"%s%s 마운트 완료. 심층 스캔을 시작합니다.", drive->letter, drive->label);
    PushLog(game, buffer);
    const DifficultyInfo* difficulty = DifficultyInfoOrNull(game->difficulty);
    if (difficulty) {
        wsprintfW(buffer, L"난이도 %s · %s.", difficulty->name, difficulty->brief);
        PushLog(game, buffer);
    }
    BeginDirectorySelection(game);
}

// 테스트 전용 경로: 드라이브와 일반전 순서만 준비한다. 특성 적용·전투 시작은
// 하지 않으며, preserveModifiers면 테스트가 주입한 손상 조합을 보존한다.
void ConfigureDriveForTest(GameState* game, int drive, uint32_t scheduleSeed, int preserveModifiers) {
    if (drive < 0 || drive >= DRIVE_COUNT) return;
    game->selectedDrive = drive;
    if (!preserveModifiers) {
        game->modifierA = DRIVE_INFO[drive].modifierA;
        game->modifierB = DRIVE_INFO[drive].modifierB;
    }
    BuildMobSchedule(game, scheduleSeed);
    MixDirectoryDrive(game);
}

// ---------------------------------------------------------------------------
// 디렉터리 경로 선택
//
// 층마다 두 번, 다음 일반전 직전에 하위 디렉터리를 고른다. 노드는 면이나
// 상주 프로그램을 직접 주지 않고 다음 전투와 그 보상의 조건만 바꾼다.
//
// 생성은 GameState.rng가 아니라 DirectoryRuntime.rng만 소비한다. 주사위·읽기
// 오류·보상·배드 섹터의 난수 순서가 경로 시스템 때문에 밀리지 않게 하려는 것이다.
// 같은 seed·드라이브·선택 이력이면 언제나 같은 선택지가 나오고, 리페인트나
// hover는 절대 생성 함수를 부르지 않는다.
// ---------------------------------------------------------------------------

static uint32_t NextDirectoryRandom(GameState* game) {
    uint32_t x = game->directory.rng;
    if (x == 0) x = 0x1F35A7C3u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->directory.rng = x;
    return x;
}

static int DirectoryRandomRange(GameState* game, int limit) {
    if (limit <= 1) return 0;
    return (int)(NextDirectoryRandom(game) % (uint32_t)limit);
}

// 드라이브가 확정되는 순간 한 번만 섞는다. 런 seed + 드라이브의 순수 함수다.
static void MixDirectoryDrive(GameState* game) {
    uint32_t mixed = game->directory.rng ^ ((uint32_t)(game->selectedDrive + 1) * 0x9E3779B9u);
    game->directory.rng = mixed ? mixed : 0x1F35A7C3u;
}

const DirectoryNodeInfo* DirectoryNodeInfoOrNull(int kind) {
    if (kind <= DIR_NODE_NONE || kind >= DIR_NODE_COUNT) return 0;
    return &DIRECTORY_NODE_INFO[kind];
}

int DirectoryChoiceCount(const GameState* game) {
    int count = game->directory.choiceCount;
    return count > DIRECTORY_CHOICE_COUNT ? DIRECTORY_CHOICE_COUNT : count;
}

int DirectoryIntelActive(const GameState* game) {
    return game->directory.intelThisFloor != 0;
}

static int DirectoryNodeWeight(const GameState* game, int kind) {
    if (kind <= DIR_NODE_NONE || kind >= DIR_NODE_COUNT) return 0;
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) return 0;
    return DIRECTORY_DRIVE_WEIGHT[game->selectedDrive][kind];
}

int ScheduledMobKind(const GameState* game) {
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) return -1;
    if (!game->mobScheduleReady || game->encounter < 0 || game->encounter > 1) return -1;
    int floor = ClampInt(game->floor, 0, 2);
    return game->mobSchedule[ClampInt(floor * 2 + game->encounter, 0, 5)];
}

int FloorBossKind(const GameState* game) {
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) return -1;
    return DRIVE_BOSSES[game->selectedDrive][ClampInt(game->floor, 0, 2)];
}

static int DamagedFaceCount(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            const Face* face = &game->dice[d].faces[f];
            if (face->damaged && face->kind != FACE_EMPTY) ++total;
        }
    }
    return total;
}

// 플레이어 상태는 노드의 유효성만 결정하고 가중치는 절대 보정하지 않는다.
// (체력이 낮다고 TEMP 확률이 오르지 않는다.)
static int DirectoryNodeAllowedInternal(const GameState* game, int kind, int respectPrevious) {
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(kind);
    if (!info || !info->enabled) return 0;
    if (DirectoryNodeWeight(game, kind) <= 0) return 0;
    if (game->directory.floorCounts[kind] >= info->maxPerFloor) return 0;
    if (respectPrevious && game->directory.previousKind == kind) return 0;
    switch (kind) {
    case DIR_NODE_TEMP:
        return game->playerHp < game->playerMaxHp;
    case DIR_NODE_LOGS:
        return !game->directory.intelThisFloor;
    case DIR_NODE_CORRUPTED:
        return game->floor >= 1 && UsableFaceCount(game) >= DIR_CORRUPTED_MIN_FACES;
    case DIR_NODE_RECOVERY:
        return game->playerHp > DIR_RECOVERY_HP_COST && DamagedFaceCount(game) > 0;
    default:
        break;
    }
    return 1;
}

int DirectoryNodeAllowed(const GameState* game, int kind) {
    return DirectoryNodeAllowedInternal(game, kind, 1);
}

// 두 카드가 함께 제시돼도 되는가.
//   - 같은 노드 금지
//   - 둘 다 고위험 금지 (INFECTED + CORRUPTED)
//   - 같은 카테고리는 한쪽이 고위험일 때만 허용
//     (TEMP + RECOVERY는 같은 회복 축이라 금지, PROCESS + INFECTED는 안전 대 위험이라 허용)
static int DirectoryHighRisk(const DirectoryNodeInfo* info) {
    return info->risk == DIR_RISK_HIGH || info->risk == DIR_RISK_UNKNOWN;
}

static int DirectoryPairAllowed(int a, int b) {
    const DirectoryNodeInfo* ia = DirectoryNodeInfoOrNull(a);
    const DirectoryNodeInfo* ib = DirectoryNodeInfoOrNull(b);
    if (!ia || !ib || a == b) return 0;
    int highA = DirectoryHighRisk(ia), highB = DirectoryHighRisk(ib);
    if (highA && highB) return 0;
    if (ia->category == ib->category && !highA && !highB) return 0;
    return 1;
}

static int PickWeightedNode(GameState* game, const int* pool, int count) {
    int total = 0;
    for (int i = 0; i < count; ++i) total += DirectoryNodeWeight(game, pool[i]);
    if (total <= 0) return DIR_NODE_NONE;
    int roll = DirectoryRandomRange(game, total);
    for (int i = 0; i < count; ++i) {
        roll -= DirectoryNodeWeight(game, pool[i]);
        if (roll < 0) return pool[i];
    }
    return pool[count - 1];
}

// 격리해도 출력 가능한 면이 최소 3개 남는 대상만 고른다 (모든 면이 죽는 사고 방지).
// 실패하면 255를 돌려주고 적용 단계가 격리를 건너뛴다.
static int PickDirectoryQuarantineFace(GameState* game) {
    int candidates[18], count = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            if (FacePower(&game->dice[d].faces[f]) >= 1) candidates[count++] = d * 6 + f;
        }
    }
    if (count < DIR_CORRUPTED_MIN_FACES) return 255;
    return candidates[DirectoryRandomRange(game, count)];
}

static void GenerateDirectoryChoices(GameState* game) {
    int pool[DIR_NODE_COUNT], count = 0;
    for (int k = DIR_NODE_PROCESS; k < DIR_NODE_COUNT; ++k)
        if (DirectoryNodeAllowedInternal(game, k, 1)) pool[count++] = k;
    // 연속 금지 때문에 후보가 말라붙으면 그 규칙만 풀어 준다 (PROCESS는 항상 fallback).
    if (count < DIRECTORY_CHOICE_COUNT) {
        count = 0;
        for (int k = DIR_NODE_PROCESS; k < DIR_NODE_COUNT; ++k)
            if (DirectoryNodeAllowedInternal(game, k, 0)) pool[count++] = k;
    }

    int first = DIR_NODE_NONE, second = DIR_NODE_NONE;
    for (int attempt = 0; attempt < DIRECTORY_GEN_ATTEMPTS && second == DIR_NODE_NONE; ++attempt) {
        int a = PickWeightedNode(game, pool, count);
        if (a == DIR_NODE_NONE) break;
        int legal[DIR_NODE_COUNT], legalCount = 0;
        for (int i = 0; i < count; ++i) if (DirectoryPairAllowed(a, pool[i])) legal[legalCount++] = pool[i];
        if (legalCount == 0) continue;
        int b = PickWeightedNode(game, legal, legalCount);
        if (b == DIR_NODE_NONE) continue;
        first = a; second = b;
    }
    // 결정론적 fallback: 낮은 kind부터 훑어 합법 조합 하나를 확정한다.
    if (second == DIR_NODE_NONE) {
        for (int i = 0; i < count && second == DIR_NODE_NONE; ++i)
            for (int j = 0; j < count && second == DIR_NODE_NONE; ++j)
                if (DirectoryPairAllowed(pool[i], pool[j])) { first = pool[i]; second = pool[j]; }
    }
    if (second == DIR_NODE_NONE) { first = DIR_NODE_PROCESS; second = DIR_NODE_CACHE; }

    ZeroMemory(game->directory.choices, sizeof(game->directory.choices));
    game->directory.choices[0].kind = (uint8_t)first;
    game->directory.choices[1].kind = (uint8_t)second;
    game->directory.choiceCount = DIRECTORY_CHOICE_COUNT;
    for (int i = 0; i < DIRECTORY_CHOICE_COUNT; ++i) {
        DirectoryChoice* choice = &game->directory.choices[i];
        choice->revealed = (uint8_t)(game->directory.intelThisFloor ? 1 : 0);
        if (choice->kind == DIR_NODE_CORRUPTED) choice->payload = (uint8_t)PickDirectoryQuarantineFace(game);
    }
}

// 다음 전투로 넘어가기 전에 노드가 걸어 둔 조건을 걷어낸다.
// 면 격리는 전투 종료 경로(GimmickCombatEnd)가 이미 풀어 준다.
static void ClearDirectoryCombatEffects(GameState* game) {
    game->directory.activeKind = DIR_NODE_NONE;
    game->directory.activePayload = 0;
}

static void ApplyDirectoryChoice(GameState* game, int index) {
    DirectoryRuntime* dir = &game->directory;
    DirectoryChoice* choice = &dir->choices[index];
    int kind = choice->kind;
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(kind);
    if (!info) return;
    int floor = ClampInt(game->floor, 0, 2);
    int slot = ClampInt(game->encounter, 0, DIRECTORY_PER_FLOOR - 1);
    dir->history[floor][slot] = (uint8_t)kind;
    if (dir->floorCounts[kind] < 255) ++dir->floorCounts[kind];
    dir->previousKind = (uint8_t)kind;
    dir->activeKind = (uint8_t)kind;
    dir->activePayload = choice->payload;

    wchar_t buffer[96];
    switch (kind) {
    case DIR_NODE_TEMP: {
        int before = game->playerHp;
        game->playerHp = ClampInt(game->playerHp + DIR_TEMP_HEAL, 0, game->playerMaxHp);
        wsprintfW(buffer, L"TEMP: 임시 파일 회수 → 체력 +%d (%d → %d).", game->playerHp - before, before, game->playerHp);
        PushLog(game, buffer);
        break;
    }
    case DIR_NODE_CACHE:
        dir->floorCapacityBonus += DIR_CACHE_BYTES;
        wsprintfW(buffer, L"CACHE: 이번 층 한도 +%dB → %dB (층 이동 시 해제).", DIR_CACHE_BYTES, EffectiveCapacity(game));
        PushLog(game, buffer);
        break;
    case DIR_NODE_LOGS:
        dir->intelThisFloor = 1;
        PushLog(game, L"LOGS: 이번 층의 적 기록을 열람했습니다. 판독 기록은 바뀌지 않습니다.");
        break;
    case DIR_NODE_INFECTED:
        PushLog(game, L"INFECTED: 감염 구역에 진입합니다. 적이 더 단단하고 보상이 강화됩니다.");
        break;
    case DIR_NODE_CORRUPTED:
        PushLog(game, L"CORRUPTED: 손상 구역에 진입합니다. 면 하나가 이번 전투 동안 격리됩니다.");
        break;
    // RECOVERY와 UNKNOWN은 데이터만 준비돼 있고 아직 enabled=0이다 (2차 확장).
    default:
        PushLog(game, L"PROCESS: 표준 프로세스와 교전합니다.");
        break;
    }
}

// 선택한 노드가 다음 전투에 거는 조건. 보스 구역은 activeKind가 NONE이라 그대로 지나간다.
static void ApplyDirectoryCombatSetup(GameState* game) {
    int kind = game->directory.activeKind;
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(kind);
    if (!info) return;
    if (kind == DIR_NODE_INFECTED) {
        for (int i = 0; i < game->enemyCount; ++i) {
            EnemyState* enemy = &game->enemies[i];
            int hp = enemy->maxHp * DIR_INFECTED_HP_PERCENT / 100;
            if (hp <= enemy->maxHp) hp = enemy->maxHp + 1;
            enemy->maxHp = hp;
            enemy->hp = hp;
        }
        PushLog2(game, L"%s: 적 최대 체력이 %d%%로 증가했습니다.", info->name, DIR_INFECTED_HP_PERCENT);
    } else if (kind == DIR_NODE_CACHE || kind == DIR_NODE_LOGS) {
        int block = kind == DIR_NODE_CACHE ? DIR_CACHE_BLOCK : DIR_LOGS_BLOCK;
        for (int i = 0; i < game->enemyCount; ++i) game->enemies[i].block += block;
        PushLog2(game, L"%s: 적이 방어도 %d로 시작합니다.", info->name, block);
    } else if (kind == DIR_NODE_CORRUPTED) {
        int target = game->directory.activePayload;
        if (target < 18) {
            Face* face = &game->dice[target / 6].faces[target % 6];
            // 마지막 남은 출력을 격리하지 않는다.
            if (FacePower(face) >= 1 && UsableFaceCount(game) > 3) {
                face->quarantined = QUAR_COMBAT;
                PushLog2(game, L"%s: 주사위 %d의 면이 이번 전투 동안 격리되었습니다.", info->name, target / 6 + 1);
            }
        }
    }
}

void BeginDirectorySelection(GameState* game) {
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) {
        game->phase = PHASE_DRIVE_SELECT;
        PushLog(game, L"마운트 오류: 볼륨이 확정되지 않아 드라이브 선택으로 복귀합니다.");
        return;
    }
    ClearDirectoryCombatEffects(game);
    // 보스 구역에는 선택 화면이 없다. 고정 경로로 바로 들어간다.
    if (game->encounter >= 2) { StartCombat(game); return; }
    GenerateDirectoryChoices(game);
    game->phase = PHASE_DIRECTORY;
    game->selectedDie = -1;
    game->selectedReward = -1;
    PushLog(game, L"하위 디렉터리를 선택하십시오. 다음 프로세스의 조건이 바뀝니다.");
}

void SelectDirectoryChoice(GameState* game, int index) {
    if (game->phase != PHASE_DIRECTORY) return;
    if (index < 0 || index >= DirectoryChoiceCount(game)) return;
    int kind = game->directory.choices[index].kind;
    if (kind <= DIR_NODE_NONE || kind >= DIR_NODE_COUNT) return;
    ApplyDirectoryChoice(game, index);
    StartCombat(game);
}

// 경로 문자열은 상태에 저장하지 않고 층 경로 + 방문한 노드 segment로 조합한다.
void FormatCurrentDirectory(const GameState* game, wchar_t* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) return;
    int floor = ClampInt(game->floor, 0, 2);
    lstrcpynW(out, DRIVE_INFO[game->selectedDrive].paths[floor], cap);
    for (int i = 0; i < DIRECTORY_PER_FLOOR; ++i) {
        const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(game->directory.history[floor][i]);
        if (!info) break;
        int length = lstrlenW(out);
        // 1층 경로("C:\\")는 이미 역슬래시로 끝나므로 구분자를 겹치지 않는다.
        int separator = length > 0 && out[length - 1] == L'\\' ? 0 : 1;
        if (length + lstrlenW(info->segment) + separator + 1 > cap) break;
        if (separator) lstrcatW(out, L"\\");
        lstrcatW(out, info->segment);
    }
}

// ---------------------------------------------------------------------------
// 전투 생성
// ---------------------------------------------------------------------------

static void AddEnemy(GameState* game, int kind) {
    if (game->enemyCount >= 3) return;
    if (!IsValidEnemyKind(kind)) {
        PushLog(game, L"오류: 유효하지 않은 적 데이터가 거부되었습니다.");
        return;
    }
    const EnemyInfo* info = &ENEMY_INFO[kind];
    EnemyState* enemy = &game->enemies[game->enemyCount++];
    ZeroMemory(enemy, sizeof(*enemy));
    enemy->kind = (uint8_t)kind;
    enemy->alive = 1;
    int hp = info->hp + info->hpGrowth * game->floor;
    if (IsModifierActive(game, MOD_OVERALLOC)) hp = (hp * 130 + 99) / 100;
    int weaken = DrivePerkValue(game, PERK_ENEMY_HP_DOWN);
    if (weaken > 0) hp = ClampInt(hp * (100 - weaken) / 100, 1, hp);
    enemy->hp = hp;
    enemy->maxHp = hp;
}

static void BeginTurn(GameState* game);

int StartCombat(GameState* game) {
    // 유효하지 않은 드라이브로는 적을 만들지 않는다. 임의 기본값 대체 금지.
    if (game->selectedDrive < 0 || game->selectedDrive >= DRIVE_COUNT) {
        game->phase = PHASE_DRIVE_SELECT;
        game->enemyCount = 0;
        ZeroMemory(game->enemies, sizeof(game->enemies));
        PushLog(game, L"마운트 오류: 볼륨이 확정되지 않아 드라이브 선택으로 복귀합니다.");
        return 0;
    }
    if (!game->mobScheduleReady) BuildMobSchedule(game, NextRandom(game));
    game->phase = PHASE_COMBAT;
    game->turn = 1;
    game->enemyCount = 0;
    game->targetEnemy = 0;
    ZeroMemory(game->enemies, sizeof(game->enemies));
    int floor = ClampInt(game->floor, 0, 2);
    if (game->encounter == 2) {
        AddEnemy(game, DRIVE_BOSSES[game->selectedDrive][floor]);
    } else {
        int slot = ClampInt(floor * 2 + game->encounter, 0, 5);
        AddEnemy(game, game->mobSchedule[slot]);
    }
    ApplyDirectoryCombatSetup(game);
    GimmickInitCombat(game);
    BeginTurn(game);
    return 1;
}

// ---------------------------------------------------------------------------
// 의도 계획: 일반 몹은 패턴, 보스는 공용 주기 + 기믹 오버라이드
// ---------------------------------------------------------------------------

static void PlanBoss(GameState* game, EnemyState* enemy) {
    const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
    BossRuntime* boss = &game->boss;
    const BossGimmickInfo* gi = GimmickInfo(game);
    int damage = info->damage + info->damageGrowth * game->floor;
    int guard = info->guard + info->guardGrowth * game->floor;
    // 압력 한계: 예고된 강화 공격이 이번 턴 모든 주기를 대체한다.
    if (boss->empowered) {
        enemy->intent = (uint8_t)(boss->gimmick == GIMMICK_HEAP_OVERFLOW ? INTENT_CORRUPT : INTENT_HEAVY);
        enemy->intentValue = damage + gi->p3;
        return;
    }
    // 타임아웃 발동 턴: 순서는 역전되지만 보스는 대기(방어)한다.
    if (boss->gimmick == GIMMICK_TIMEOUT && boss->reversed) {
        enemy->intent = INTENT_GUARD;
        enemy->intentValue = guard;
        return;
    }
    int bossCycle = (game->turn + enemy->kind) % 5;
    if (bossCycle == 0) {
        enemy->intent = INTENT_CORRUPT;
        enemy->intentValue = damage - 2 > 1 ? damage - 2 : 1;
    } else if (bossCycle == 2) {
        enemy->intent = INTENT_GUARD;
        enemy->intentValue = guard;
    } else if (bossCycle == 4) {
        enemy->intent = INTENT_HEAVY;
        enemy->intentValue = damage + 3;
    } else {
        enemy->intent = INTENT_ATTACK;
        enemy->intentValue = damage;
    }
    // 역전 턴에는 강공·오염을 일반 공격으로 낮춰 억울한 즉사를 막는다.
    if (boss->reversed && (enemy->intent == INTENT_HEAVY || enemy->intent == INTENT_CORRUPT)) {
        enemy->intent = INTENT_ATTACK;
        enemy->intentValue = damage;
    }
}

// 기존 8종 몹의 하드코딩 주기를 그대로 재현한다 (레거시 호환).
static void PlanLegacyMob(GameState* game, EnemyState* enemy, int enemyIndex) {
    const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
    int cycle = (game->turn + enemyIndex + enemy->kind) & 3;
    enemy->intent = INTENT_ATTACK;
    enemy->intentValue = info->damage + game->floor;
    if (cycle == 1 && (enemy->kind == ENEMY_CACHE || enemy->kind == ENEMY_FRAGMENT)) {
        enemy->intent = INTENT_GUARD;
        enemy->intentValue = info->guard;
    } else if (cycle == 2 && (enemy->kind == ENEMY_WORM || enemy->kind == ENEMY_DAEMON)) {
        enemy->intent = INTENT_REPAIR;
        enemy->intentValue = 3 + game->floor;
    } else if (cycle == 3 && (enemy->kind == ENEMY_TROJAN || enemy->kind == ENEMY_ROOTKIT)) {
        enemy->intent = INTENT_HEAVY;
        enemy->intentValue = info->damage + 4;
    } else if (cycle == 0 && enemy->kind == ENEMY_SPYWARE) {
        enemy->intent = INTENT_CORRUPT;
        enemy->intentValue = info->damage;
    }
}

static void PlanMob(GameState* game, EnemyState* enemy, int enemyIndex) {
    const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
    if (info->pattern == PATTERN_LEGACY) { PlanLegacyMob(game, enemy, enemyIndex); return; }
    int damage = info->damage + info->damageGrowth * game->floor;
    int guard = info->guard + info->guardGrowth * game->floor;
    int corrupt = damage - 1 > 1 ? damage - 1 : 1;
    int turn = game->turn;
    int step = (turn - 1) & 3;
    enemy->intent = INTENT_ATTACK;
    enemy->intentValue = damage;
    switch (info->pattern) {
    case PATTERN_ASSAULT:
        if (step == 2) { enemy->intent = INTENT_HEAVY; enemy->intentValue = damage + 3; }
        break;
    case PATTERN_CORRUPTER:
        if (step == 0) { enemy->intent = INTENT_CORRUPT; enemy->intentValue = corrupt; }
        else if (step == 3) { enemy->intent = INTENT_HEAVY; enemy->intentValue = damage + 3; }
        break;
    case PATTERN_BULWARK:
        if (step == 0 || step == 2) { enemy->intent = INTENT_GUARD; enemy->intentValue = guard; }
        else if (step == 3) { enemy->intent = INTENT_HEAVY; enemy->intentValue = damage + 3; }
        break;
    case PATTERN_MEDIC:
        if (step == 1) { enemy->intent = INTENT_REPAIR; enemy->intentValue = 3 + game->floor; }
        else if (step == 2) { enemy->intent = INTENT_GUARD; enemy->intentValue = guard; }
        break;
    case PATTERN_OPENER:
        if (turn == 1) { enemy->intent = INTENT_HEAVY; enemy->intentValue = damage + 3; }
        else if ((turn - 2) % 3 == 1) { enemy->intent = INTENT_GUARD; enemy->intentValue = guard; }
        break;
    case PATTERN_RAMP: {
        int ramp = turn - 1;
        if (ramp > 5) ramp = 5;
        enemy->intentValue = damage + ramp;
        break;
    }
    case PATTERN_ERRATIC: {
        // 무작위처럼 보이지만 (턴, 종류, 자리)의 순수 함수라 예고와 재현이 가능하다.
        uint32_t h = (uint32_t)(turn * 2654435761u) ^ (uint32_t)(enemy->kind * 97 + enemyIndex * 31);
        h ^= h >> 13;
        switch (h % 4u) {
        case 1: enemy->intent = INTENT_GUARD; enemy->intentValue = guard; break;
        case 2: enemy->intent = INTENT_CORRUPT; enemy->intentValue = corrupt; break;
        case 3: enemy->intent = INTENT_HEAVY; enemy->intentValue = damage + 2; break;
        default: break;
        }
        break;
    }
    case PATTERN_SPIKE:
        if (step == 0) { enemy->intent = INTENT_HEAVY; enemy->intentValue = damage + 3; }
        else if (step == 1) { enemy->intent = INTENT_CORRUPT; enemy->intentValue = corrupt; }
        break;
    default: break;
    }
}

static void PlanEnemy(GameState* game, EnemyState* enemy, int enemyIndex) {
    if (IsBossKind(enemy->kind)) PlanBoss(game, enemy);
    else PlanMob(game, enemy, enemyIndex);
    // 볼륨 난이도는 오염(관통) 의도에만 걸린다. 예고 수치를 여기서 확정해 두면
    // 적 카드에 뜨는 숫자가 곧 실제로 들어올 피해가 된다.
    if (enemy->intent == INTENT_CORRUPT) enemy->intentValue = ScaleCorruptDamage(game, enemy->intentValue);
}

// ---------------------------------------------------------------------------
// 턴 진행
// ---------------------------------------------------------------------------

static void ApplyFragmentation(GameState* game);

// 오프라인 발동 턴에는 조각화 적용을 건너뛴다 (중첩 무력화 방지).
static void ApplyFragmentationIfAllowed(GameState* game) {
    if (game->boss.offlineDie >= 0) return;
    ApplyFragmentation(game);
}

static void RollDice(GameState* game) {
    for (int d = 0; d < 3; ++d) {
        game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
        game->dice[d].assignedSlot = -1;
        game->dice[d].disabled = 0;
        game->dice[d].unstable = 0;
        game->dice[d].offline = 0;
    }
    game->selectedDie = -1;
    if (IsModifierActive(game, MOD_READ_ERROR)) game->dice[RandomRange(game, 3)].unstable = 1;
}

static void BeginTurn(GameState* game) {
    game->playerBlock = 0;
    game->lastDamage = 0;
    game->lastBlock = 0;
    game->keybUsedThisTurn = 0;
    game->boss.damageThisTurn = 0;
    if (game->turn == 1 && IsTsrInstalled(game, TSR_SMARTDRV)) {
        game->playerBlock = TSR_INFO[TSR_SMARTDRV].value;
        PushLog2(game, L"%s: 선제 캐시 방어도 +%d.", TSR_INFO[TSR_SMARTDRV].name, game->playerBlock);
    }
    GimmickTurnBegin(game);
    RollDice(game);
    if (game->boss.offlineDie >= 0 && game->boss.offlineDie < 3) {
        game->dice[game->boss.offlineDie].offline = 1;
        ApplyFragmentationIfAllowed(game);   // 오프라인 턴엔 즉시 반환된다
    } else {
        ApplyFragmentation(game);
    }
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) PlanEnemy(game, &game->enemies[i], i);
    PushLog(game, L"주사위를 슬롯에 배치하고 스페이스 키로 실행합니다.");
}

int AssignDieToSlot(GameState* game, int dieIndex, int slotIndex) {
    if (game->phase != PHASE_COMBAT || dieIndex < 0 || dieIndex >= 3 || slotIndex < 0 || slotIndex >= SLOT_COUNT) return 0;
    if (game->boss.lockedSlot[slotIndex]) {
        PushLog2(game, L"권한 거부: %s 슬롯은 이번 턴 잠겨 있습니다.", SLOT_NAMES[slotIndex], 0);
        return 0;
    }
    for (int d = 0; d < 3; ++d) if (d != dieIndex && game->dice[d].assignedSlot == slotIndex) game->dice[d].assignedSlot = -1;
    if (game->dice[dieIndex].assignedSlot == slotIndex) game->dice[dieIndex].assignedSlot = -1;
    else game->dice[dieIndex].assignedSlot = (int8_t)slotIndex;
    game->selectedDie = dieIndex;
    return 1;
}

void UnassignDie(GameState* game, int dieIndex) {
    if (dieIndex < 0 || dieIndex >= 3) return;
    game->dice[dieIndex].assignedSlot = -1;
}

void SelectEnemy(GameState* game, int enemyIndex) {
    if (enemyIndex < 0 || enemyIndex >= game->enemyCount) return;
    if (game->enemies[enemyIndex].alive) game->targetEnemy = enemyIndex;
}

static int DieForSlot(const GameState* game, int slot) {
    for (int d = 0; d < 3; ++d) if (game->dice[d].assignedSlot == slot) return d;
    return -1;
}

static int FirstLivingEnemy(const GameState* game) {
    if (game->targetEnemy >= 0 && game->targetEnemy < game->enemyCount && game->enemies[game->targetEnemy].alive) return game->targetEnemy;
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) return i;
    return -1;
}

static int DamageEnemy(GameState* game, int enemyIndex, int damage) {
    if (enemyIndex < 0 || damage <= 0) return 0;
    EnemyState* enemy = &game->enemies[enemyIndex];
    if (!enemy->alive) return 0;
    int absorbed = enemy->block < damage ? enemy->block : damage;
    enemy->block -= absorbed;
    damage -= absorbed;
    enemy->hp -= damage;
    // 피해 임계 기믹(복원·압력·오염 지연)은 실제 체력 피해만 센다.
    if (damage > 0 && IsBossKind(enemy->kind)) {
        game->boss.damageThisTurn += damage;
        game->boss.windowDamage += damage;
    }
    if (enemy->hp <= 0) {
        enemy->hp = 0;
        enemy->alive = 0;
        // 처치한 순간 그 종류가 판독된다. 가이드의 노이즈가 여기서 걷힌다.
        if (IsValidEnemyKind(enemy->kind)) game->enemyScanned[enemy->kind] = 1;
        PushLog2(game, L"%s 삭제 완료. 피해 %d.", GetEnemyInfoOrUnknown(enemy->kind)->name, damage);
        game->targetEnemy = FirstLivingEnemy(game);
    }
    return damage;
}

static void ApplyFragmentation(GameState* game) {
    if (!IsModifierActive(game, MOD_FRAGMENTATION)) return;
    if (IsTsrInstalled(game, TSR_DEFRAG)) {
        for (int i = 0; i < 3; ++i) game->dice[i].disabled = 0;
        return;
    }
    int fragmented = 0;
    for (int i = 0; i < 3; ++i) game->dice[i].disabled = 0;
    for (int i = 1; i < 3; ++i) {
        const Face* current = RolledFace(game, i);
        if (!current || current->damaged || current->kind == FACE_EMPTY) continue;
        for (int j = 0; j < i; ++j) {
            const Face* previous = RolledFace(game, j);
            if (!previous || previous->damaged || previous->kind == FACE_EMPTY) continue;
            if (current->kind == previous->kind && current->value == previous->value) {
                game->dice[i].disabled = 1;
                ++fragmented;
                break;
            }
        }
    }
    if (fragmented > 0) PushLog(game, L"조각화: 중복 주사위가 비활성화되었습니다.");
}

static int RollOutputSum(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) total += FacePower(RolledFace(game, d));
    return total;
}

static int SlotPower(const GameState* game, int slot, int* kindOut) {
    if (kindOut) *kindOut = FACE_EMPTY;
    // 잠긴 슬롯은 배치가 거부되지만, 안전을 위해 해결 단계에서도 출력 0을 보장한다.
    if (game->boss.lockedSlot[slot]) return 0;
    int die = DieForSlot(game, slot);
    if (die < 0 || game->dice[die].disabled || game->dice[die].offline) return 0;
    const Face* face = RolledFace(game, die);
    if (!face) return 0;
    if (kindOut) *kindOut = face->kind;
    return FacePower(face);
}

// ---------------------------------------------------------------------------
// 슬롯별 플레이어 해결기. 해결 순서 변경(역전)을 지원하기 위해 지역 변수 대신
// ResolveContext로 상태를 잇는다.
// ---------------------------------------------------------------------------

struct ResolveContext {
    int ampBonus;
    int ampKind;
    int slotOutput[SLOT_COUNT];   // 슬롯별 실제 산출량 (KERNEL.PANIC 잠금 판정)
};

static void ResolveAmplify(GameState* game, ResolveContext* ctx, int wasted) {
    wchar_t trace[96];
    int ampKind = FACE_EMPTY;
    int ampPower = SlotPower(game, SLOT_AMPLIFY, &ampKind);
    int ampBonus = ampPower / 2;
    if (ampKind == FACE_BOOST) ampBonus = ampPower;
    if (ampKind == FACE_WILD) ampBonus += 2;
    if (wasted) {
        // 역전 턴: 공격·방어가 이미 해결된 뒤라 보너스가 소실된다.
        if (ampPower > 0) wsprintfW(trace, L"[증폭] 역전으로 해결이 끝난 뒤 도착 → 보너스 %d 소실", ampBonus);
        else lstrcpyW(trace, L"[증폭] 비어 있음 → 보너스 없음");
        PushTurnTrace(game, trace);
        ctx->slotOutput[SLOT_AMPLIFY] = 0;
        return;
    }
    ctx->ampBonus = ampBonus;
    ctx->ampKind = ampKind;
    ctx->slotOutput[SLOT_AMPLIFY] = ampBonus;
    if (ampPower <= 0) lstrcpyW(trace, L"[증폭] 비어 있음 → 공격·방어 보너스 +0");
    else if (ampKind == FACE_BOOST) wsprintfW(trace, L"[증폭] 증폭 면 %d × 100%% = 공격·방어 +%d", ampPower, ampBonus);
    else if (ampKind == FACE_WILD) wsprintfW(trace, L"[증폭] 와일드 %d ÷ 2 + 특수 2 = 공격·방어 +%d", ampPower, ampBonus);
    else wsprintfW(trace, L"[증폭] 출력 %d ÷ 2 = 공격·방어 +%d", ampPower, ampBonus);
    PushTurnTrace(game, trace);
}

static void ResolveAttack(GameState* game, ResolveContext* ctx) {
    wchar_t trace[96];
    int attackKind = FACE_EMPTY;
    int attackPower = SlotPower(game, SLOT_ATTACK, &attackKind);
    int ampBonus = ctx->ampBonus;
    int checksumBonus = 0;
    if (IsModifierActive(game, MOD_CHECKSUM) && (RollOutputSum(game) & 1) == 0) {
        checksumBonus = 2;
        PushLog(game, L"체크섬 일치: 이번 공격 피해 +2.");
    }
    int attackDamage = attackPower > 0 ? attackPower + ampBonus + checksumBonus : 0;
    // 드라이브 특성 보너스는 '특수' 항목에 합산해 계산 추적의 합계와 일치시킨다.
    int attackSpecial = (attackKind == FACE_FIRE ? 4 : attackKind == FACE_WILD ? 2 : 0) + DrivePerkValue(game, PERK_ATTACK_UP);
    attackDamage += attackPower > 0 ? attackSpecial : 0;
    int target = FirstLivingEnemy(game);
    int targetBlockBefore = target >= 0 ? game->enemies[target].block : 0;
    int targetHpBefore = target >= 0 ? game->enemies[target].hp : 0;
    int dealt = DamageEnemy(game, target, attackDamage);
    int targetHpAfter = target >= 0 ? game->enemies[target].hp : 0;
    if (attackPower > 0) {
        wsprintfW(trace, L"[공격/%s] 기본 %d + 증폭 %d + 특수 %d + 체크섬 %d = %d 피해",
            FACE_INFO[attackKind].shortName, attackPower, ampBonus, attackSpecial, checksumBonus, attackDamage);
    } else lstrcpyW(trace, L"[공격] 공격 슬롯이 비어 있음 → 피해 0");
    PushTurnTrace(game, trace);
    if (target >= 0 && attackKind == FACE_FIRE && game->enemies[target].alive) game->enemies[target].burn = 2;
    int actualHeal = 0;
    if (attackKind == FACE_LEECH && dealt > 0) {
        int heal = dealt / 3 + 1;
        int hpBefore = game->playerHp;
        int upper = game->playerHp + heal;
        game->playerHp = upper > game->playerMaxHp ? game->playerMaxHp : upper;
        actualHeal = game->playerHp - hpBefore;
    }
    if (attackPower > 0 && attackKind == FACE_FIRE) wsprintfW(trace, L"[적중] 적 방어도 %d 흡수 → 체력 -%d (%d → %d) · 화상 2 부여",
        targetBlockBefore < attackDamage ? targetBlockBefore : attackDamage,
        targetHpBefore - targetHpAfter, targetHpBefore, targetHpAfter);
    else if (attackPower > 0 && attackKind == FACE_LEECH) wsprintfW(trace, L"[적중] 적 방어도 %d 흡수 → 체력 -%d (%d → %d) · 내 체력 +%d",
        targetBlockBefore < attackDamage ? targetBlockBefore : attackDamage,
        targetHpBefore - targetHpAfter, targetHpBefore, targetHpAfter, actualHeal);
    else if (attackPower > 0) wsprintfW(trace, L"[적중] 적 방어도 %d 흡수 → 실제 체력 -%d (%d → %d)",
        targetBlockBefore < attackDamage ? targetBlockBefore : attackDamage,
        targetHpBefore - targetHpAfter, targetHpBefore, targetHpAfter);
    else lstrcpyW(trace, L"[적중] 적용할 공격 피해 없음");
    PushTurnTrace(game, trace);
    game->lastDamage = attackDamage;
    ctx->slotOutput[SLOT_ATTACK] = attackDamage;
}

static void ResolveDefend(GameState* game, ResolveContext* ctx) {
    wchar_t trace[96];
    int defendKind = FACE_EMPTY;
    int defendPower = SlotPower(game, SLOT_DEFEND, &defendKind);
    int ampBonus = ctx->ampBonus;
    int block = defendPower > 0 ? defendPower + ampBonus : 0;
    if (defendKind == FACE_SHIELD) block *= 2;
    if (defendKind == FACE_WILD) block += 3;
    game->playerBlock += block;
    game->lastBlock = block;
    ctx->slotOutput[SLOT_DEFEND] = block;
    if (defendPower <= 0) lstrcpyW(trace, L"[방어] 방어 슬롯이 비어 있음 → 방어도 +0");
    else if (defendKind == FACE_SHIELD) wsprintfW(trace, L"[방어] (기본 %d + 증폭 %d) × 방벽 2 = 방어도 +%d", defendPower, ampBonus, block);
    else if (defendKind == FACE_WILD) wsprintfW(trace, L"[방어] 기본 %d + 증폭 %d + 특수 3 = 방어도 +%d", defendPower, ampBonus, block);
    else wsprintfW(trace, L"[방어] 기본 %d + 증폭 %d = 방어도 +%d", defendPower, ampBonus, block);
    PushTurnTrace(game, trace);
}

static void ResolveChain(GameState* game, ResolveContext* ctx) {
    wchar_t trace[96];
    int chainKind = FACE_EMPTY;
    int chainPower = SlotPower(game, SLOT_CHAIN, &chainKind);
    if (chainPower <= 0) {
        PushTurnTrace(game, L"[연쇄] 연쇄 슬롯이 비어 있음 → 반복 없음");
        return;
    }
    if (game->lastDamage > 0 && LivingEnemyCount(game) > 0) {
        int repeat = (game->lastDamage * (chainPower + 4)) / 13;
        if (chainKind == FACE_ECHO) repeat = game->lastDamage;
        if (chainKind == FACE_WILD) repeat += 2;
        int chainTarget = FirstLivingEnemy(game);
        int hpBefore = chainTarget >= 0 ? game->enemies[chainTarget].hp : 0;
        int blockBefore = chainTarget >= 0 ? game->enemies[chainTarget].block : 0;
        DamageEnemy(game, chainTarget, repeat);
        int hpAfter = chainTarget >= 0 ? game->enemies[chainTarget].hp : 0;
        int absorbed = blockBefore < repeat ? blockBefore : repeat;
        ctx->slotOutput[SLOT_CHAIN] = repeat;
        if (chainKind == FACE_ECHO) wsprintfW(trace, L"[연쇄] 메아리: 공격 %d × 100%% = %d · 방어도 %d → 체력 -%d", game->lastDamage, repeat, absorbed, hpBefore - hpAfter);
        else if (chainKind == FACE_WILD) wsprintfW(trace, L"[연쇄] %d × (%d + 4) ÷ 13 + 2 = %d · 방어도 %d → 체력 -%d", game->lastDamage, chainPower, repeat, absorbed, hpBefore - hpAfter);
        else wsprintfW(trace, L"[연쇄] %d × (%d + 4) ÷ 13 = %d · 방어도 %d → 체력 -%d", game->lastDamage, chainPower, repeat, absorbed, hpBefore - hpAfter);
        PushTurnTrace(game, trace);
    } else if (game->lastBlock > 0) {
        int repeat = (game->lastBlock * (chainPower + 4)) / 13;
        if (chainKind == FACE_ECHO) repeat = game->lastBlock;
        game->playerBlock += repeat;
        ctx->slotOutput[SLOT_CHAIN] = repeat;
        if (chainKind == FACE_ECHO) wsprintfW(trace, L"[연쇄] 메아리: 직전 방어 %d × 100%% = 방어도 +%d", game->lastBlock, repeat);
        else wsprintfW(trace, L"[연쇄] %d × (%d + 4) ÷ 13 = 방어도 +%d", game->lastBlock, chainPower, repeat);
        PushTurnTrace(game, trace);
    } else PushTurnTrace(game, L"[연쇄] 반복할 공격·방어가 없어 발동하지 않음");
}

static void ResolvePlayer(GameState* game) {
    ResolveContext ctx = {};
    ctx.ampKind = FACE_EMPTY;
    int reversed = ResolveOrderReversed(game);
    game->lastTurnReversed = reversed;
    if (reversed) {
        PushTurnTrace(game, L"[역전] 해결 순서: 연쇄 → 방어 → 공격 → 증폭");
        ResolveChain(game, &ctx);
        ResolveDefend(game, &ctx);
        ResolveAttack(game, &ctx);
        ResolveAmplify(game, &ctx, 1);
    } else {
        ResolveAmplify(game, &ctx, 0);
        ResolveAttack(game, &ctx);
        ResolveDefend(game, &ctx);
        ResolveChain(game, &ctx);
    }
    // KERNEL.PANIC: 이번 턴 출력이 가장 컸던 슬롯이 다음 턴 잠금 대상이 된다.
    int best = -1, bestValue = 0;
    for (int s = 0; s < SLOT_COUNT; ++s) {
        game->lastTurnSlotOutput[s] = ctx.slotOutput[s];
        if (ctx.slotOutput[s] > bestValue) { bestValue = ctx.slotOutput[s]; best = s; }
    }
    game->boss.bestSlotLastTurn = (int8_t)best;
}

static void ResolveEnemies(GameState* game) {
    for (int i = 0; i < game->enemyCount; ++i) {
        EnemyState* enemy = &game->enemies[i];
        if (!enemy->alive) continue;
        const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
        if (enemy->burn > 0) {
            --enemy->burn;
            int hpBefore = enemy->hp;
            DamageEnemy(game, i, 3);
            wchar_t burnTrace[96]; wsprintfW(burnTrace, L"[화상] %s 체력 -%d (%d → %d)",
                info->code, hpBefore - enemy->hp, hpBefore, enemy->hp);
            PushTurnTrace(game, burnTrace);
            if (!enemy->alive) continue;
        }
        wchar_t trace[96];
        if (enemy->intent == INTENT_GUARD) {
            enemy->block += enemy->intentValue;
            wsprintfW(trace, L"[적 행동] %s 방어 → 적 방어도 +%d", info->code, enemy->intentValue);
        } else if (enemy->intent == INTENT_REPAIR) {
            int hpBefore = enemy->hp;
            enemy->hp = ClampInt(enemy->hp + enemy->intentValue, 0, enemy->maxHp);
            wsprintfW(trace, L"[적 행동] %s 복구 → 체력 +%d (%d → %d)", info->code, enemy->hp - hpBefore, hpBefore, enemy->hp);
        } else {
            int damage = enemy->intentValue;
            int absorbed = 0;
            int empowered = game->boss.empowered && IsBossKind(enemy->kind);
            if (enemy->intent == INTENT_CORRUPT) {
                // 관통은 방어도를 절반만 인정한다. 막아낸 만큼의 두 배가 소모되므로
                // 방어도 자체는 음수가 되지 않는다.
                int usable = game->playerBlock / 2;
                absorbed = usable < damage ? usable : damage;
                game->playerBlock -= absorbed * 2;
                damage -= absorbed;
            } else {
                absorbed = game->playerBlock < damage ? game->playerBlock : damage;
                game->playerBlock -= absorbed;
                damage -= absorbed;
            }
            game->playerHp -= damage;
            // 계산 재생이 이 줄에 닿는 순간 적이 달려들도록, 곧 기록될 줄 번호까지 남긴다.
            game->lastTurnEnemyStruck[i] = 1;
            game->lastTurnEnemyStrikeDamage[i] = damage;
            game->lastTurnEnemyStrikeTrace[i] = game->turnTraceCount;
            if (enemy->intent == INTENT_CORRUPT && damage > 0) PushLog(game, L"오염 공격은 방어도를 절반만 인정합니다.");
            if (empowered && enemy->intent == INTENT_CORRUPT) wsprintfW(trace, L"[적 행동] %s 강화 오염 %d - 방어 절반 %d = 내 체력 -%d", info->code, enemy->intentValue, absorbed, damage);
            else if (empowered) wsprintfW(trace, L"[적 행동] %s 강화 공격 %d - 방어도 %d = 내 체력 -%d", info->code, enemy->intentValue, absorbed, damage);
            else if (enemy->intent == INTENT_CORRUPT) wsprintfW(trace, L"[적 행동] %s 오염 %d - 방어 절반 %d = 내 체력 -%d", info->code, enemy->intentValue, absorbed, damage);
            else wsprintfW(trace, L"[적 행동] %s 공격 %d - 방어도 %d = 내 체력 -%d", info->code, enemy->intentValue, absorbed, damage);
        }
        PushTurnTrace(game, trace);
        if (game->playerHp <= 0) {
            game->playerHp = 0;
            game->phase = PHASE_GAMEOVER;
            // 임시 기믹 상태는 게임오버 화면으로도 새어 나가면 안 된다.
            GimmickCombatEnd(game);
            PushLog(game, L"시스템 정지. R 키로 다시 시작하십시오.");
            return;
        }
    }
}

// 재굴림이 걸린 주사위의 흔들림은 그 슬롯 하나로 끝나지 않는다. 증폭 보너스는 뒤에 해결될
// 공격·방어에 실리고, 연쇄는 그 공격·방어를 그대로 반복한다. 체크섬은 배치와 무관하게 세
// 주사위 출력의 합만 보므로 공격에 바로 걸린다. 역전 턴에는 증폭 보너스가 소실되고 연쇄가
// 먼저 돌아 반복할 값이 아직 없으므로 (연쇄 → 방어 → 공격 → 증폭) 전파가 없다.
static void MarkShakenSlots(const GameState* game, TurnPreview* out) {
    // 비었거나 잠긴 슬롯, 출력이 0으로 고정된 주사위(오프라인·조각화)가 놓인 슬롯은 어떤 눈이
    // 나와도 산출량이 0이라 확정이다. SlotPower와 같은 조건으로 먼저 걸러 두고, 전파가 끝난
    // 뒤 한 번 더 걷어 낸다.
    int certain[SLOT_COUNT];
    for (int s = 0; s < SLOT_COUNT; ++s) {
        int die = DieForSlot(game, s);
        certain[s] = die < 0 || game->boss.lockedSlot[s] || game->dice[die].offline || game->dice[die].disabled;
        if (certain[s]) out->slotUnknown[s] = 0;
    }
    if (IsModifierActive(game, MOD_CHECKSUM)) out->slotUnknown[SLOT_ATTACK] = 1;
    if (!ResolveOrderReversed(game)) {
        if (out->slotUnknown[SLOT_AMPLIFY]) {
            out->slotUnknown[SLOT_ATTACK] = 1;
            out->slotUnknown[SLOT_DEFEND] = 1;
        }
        if (out->slotUnknown[SLOT_ATTACK] || out->slotUnknown[SLOT_DEFEND]) out->slotUnknown[SLOT_CHAIN] = 1;
    }
    for (int s = 0; s < SLOT_COUNT; ++s) if (certain[s]) out->slotUnknown[s] = 0;
}

// 사본에서 한 턴을 그대로 실행해 결과 숫자만 돌려준다. 원본은 읽기만 하므로
// 난수도, 기믹 상태도, 전투 진행도 전혀 움직이지 않는다.
void PreviewTurn(const GameState* game, TurnPreview* out) {
    if (!out) return;
    ZeroMemory(out, sizeof(*out));
    if (!game || game->phase != PHASE_COMBAT) return;
    int assigned = 0;
    for (int d = 0; d < 3; ++d) if (game->dice[d].assignedSlot >= 0) ++assigned;
    if (assigned == 0) return;

    GameState copy = *game;
    // 읽기 오류는 실행하는 순간 다시 굴러간다. 사본에서 그대로 굴려 보면 실제로 나올 숫자가
    // 미리보기로 새어 나가므로, 재굴림 자체를 빼고 돌린다. 대신 예상은 확정이 아니라고 밝히고,
    // 흔들리는 값이 닿는 슬롯은 산출량을 모른다고 표시한다.
    int shaken = 0;
    for (int d = 0; d < 3; ++d) {
        if (!copy.dice[d].unstable) continue;
        out->uncertain = 1;
        shaken = 1;
        if (copy.dice[d].assignedSlot >= 0) out->slotUnknown[copy.dice[d].assignedSlot] = 1;
        copy.dice[d].unstable = 0;
    }
    if (shaken) MarkShakenSlots(&copy, out);
    EndTurn(&copy);

    out->valid = 1;
    out->damageDealt = copy.lastTurnDamageDealt;
    out->damageTaken = copy.lastTurnDamageTaken;
    out->blockGained = copy.lastTurnBlockGained;
    for (int s = 0; s < SLOT_COUNT; ++s) out->slotOutput[s] = copy.lastTurnSlotOutput[s];
    out->playerDies = copy.phase == PHASE_GAMEOVER;
    out->combatEnds = !out->playerDies && LivingEnemyCount(&copy) == 0;
}

// 디렉터리 노드는 면을 직접 주지 않고 이 표준 보상의 tier와 후보 수만 바꾼다.
//   표준 : 숫자 7~10, 후보 3개 (기존과 완전히 동일한 난수 소비)
//   강화 : 숫자 8~11, 세 후보의 종류 중복 없음 (INFECTED / CORRUPTED)
//   TEMP : 표준 분포지만 후보 2개
static void GenerateRewards(GameState* game) {
    const DirectoryNodeInfo* node = DirectoryNodeInfoOrNull(game->directory.activeKind);
    int tuned = node ? node->rewardTier : 0;
    int count = node ? node->rewardChoices : 3;
    count = ClampInt(count, 1, 3);
    game->rewardTier = tuned;
    game->rewardChoiceCount = count;
    for (int i = 0; i < count; ++i) {
        int kind, duplicate;
        do {
            kind = RandomRange(game, 7);
            duplicate = i > 0 && kind == game->rewardKinds[i - 1];
            if (tuned) for (int j = 0; j < i; ++j) if (kind == game->rewardKinds[j]) duplicate = 1;
        } while (duplicate);
        game->rewardKinds[i] = kind;
        game->rewardValues[i] = kind == FACE_NUMBER ? (tuned ? 8 : 7) + RandomRange(game, 4) : FACE_INFO[kind].power;
    }
    for (int i = count; i < 3; ++i) { game->rewardKinds[i] = FACE_EMPTY; game->rewardValues[i] = 0; }
    game->rewardIsTsr = 0;
    game->selectedReward = -1;
}

// 보스 전리품: 아직 설치하지 않은 TSR 중에서 3개를 제시한다. 특정 손상에
// 대항하는 TSR은 이번 런에 그 손상이 있을 때만 후보가 된다 (죽은 카드 방지).
static void GenerateTsrRewards(GameState* game) {
    int pool[TSR_COUNT], count = 0;
    for (int i = 0; i < TSR_COUNT; ++i) {
        if (game->tsrInstalled[i]) continue;
        if (TSR_INFO[i].counters >= 0 && !IsModifierActive(game, TSR_INFO[i].counters)) continue;
        pool[count++] = i;
    }
    for (int i = count - 1; i > 0; --i) {
        int j = RandomRange(game, i + 1);
        int swap = pool[i]; pool[i] = pool[j]; pool[j] = swap;
    }
    for (int i = 0; i < 3; ++i) {
        game->rewardKinds[i] = count > 0 ? pool[i % count] : TSR_HIMEM;
        game->rewardValues[i] = TSR_INFO[game->rewardKinds[i]].cost;
    }
    game->rewardIsTsr = 1;
    game->rewardChoiceCount = 3;
    game->rewardTier = 0;
    game->selectedReward = -1;
}

static void CombatWon(GameState* game) {
    // 어떤 조기 return보다 먼저 임시 기믹 상태를 정리한다. 영구 EMPTY만 남는다.
    GimmickCombatEnd(game);
    ++game->combatsWon;
    if (game->floor == 2 && game->encounter == 2) {
        game->phase = PHASE_VICTORY;
        PushLog(game, L"침입 프로세스 전멸. 디스크가 복구되었습니다.");
        return;
    }
    int heal = DrivePerkValue(game, PERK_HEAL_ON_WIN);
    if (heal > 0) {
        int hpBefore = game->playerHp;
        game->playerHp = ClampInt(game->playerHp + heal, 0, game->playerMaxHp);
        if (game->playerHp > hpBefore) PushLog2(game, L"%s 특성: 체력 %d 회복.", DRIVE_INFO[game->selectedDrive].letter, game->playerHp - hpBefore);
    }
    if (IsTsrInstalled(game, TSR_UNDELETE)) {
        int hpBefore = game->playerHp;
        game->playerHp = ClampInt(game->playerHp + TSR_INFO[TSR_UNDELETE].value, 0, game->playerMaxHp);
        if (game->playerHp > hpBefore) PushLog2(game, L"%s: 체력 %d 복원.", TSR_INFO[TSR_UNDELETE].name, game->playerHp - hpBefore);
    }
    if (game->encounter == 2) {
        GenerateTsrRewards(game);
        game->phase = PHASE_REWARD;
        PushLog(game, L"보스 전리품: 상주 프로그램 하나를 설치할 수 있습니다.");
        return;
    }
    GenerateRewards(game);
    game->phase = PHASE_REWARD;
    PushLog(game, L"전투 보상: 설치할 면과 교체 위치를 선택하십시오.");
}

void EndTurn(GameState* game) {
    if (game->phase != PHASE_COMBAT) return;
    int assigned = 0;
    for (int d = 0; d < 3; ++d) if (game->dice[d].assignedSlot >= 0) ++assigned;
    if (assigned == 0) {
        PushLog(game, L"최소 하나의 주사위를 슬롯에 배치해야 합니다.");
        return;
    }
    ClearTurnTrace(game);
    RecordFx(game, GIMMICK_NONE, -1, -1);
    for (int i = 0; i < 3; ++i) {
        game->lastTurnEnemyStruck[i] = 0;
        game->lastTurnEnemyStrikeDamage[i] = 0;
        game->lastTurnEnemyStrikeTrace[i] = -1;
    }
    // 읽기 오류의 재굴림은 오프라인 출력 0보다 먼저 처리된다.
    for (int d = 0; d < 3; ++d) {
        if (game->dice[d].unstable) {
            game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
            PushLog(game, L"읽기 오류: 경고된 주사위가 다시 굴러갔습니다.");
            wchar_t trace[96]; wsprintfW(trace, L"[읽기 오류] 주사위 %d 다시 굴림 → 출력 %d", d + 1, FacePower(RolledFace(game, d)));
            PushTurnTrace(game, trace);
            break;
        }
    }
    if (game->boss.offlineDie >= 0) {
        wchar_t trace[96];
        wsprintfW(trace, L"[오프라인] 주사위 %d 연결 끊김 → 이번 턴 출력 0", game->boss.offlineDie + 1);
        PushTurnTrace(game, trace);
    }
    int enemyHpBefore = 0;
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) enemyHpBefore += game->enemies[i].hp;
    int playerHpBefore = game->playerHp;
    ResolvePlayer(game);
    game->lastTurnBlockGained = game->playerBlock;
    int enemyHpAfterPlayer = 0;
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) enemyHpAfterPlayer += game->enemies[i].hp;
    if (LivingEnemyCount(game) == 0) {
        game->lastTurnDamageDealt = enemyHpBefore - enemyHpAfterPlayer;
        game->lastTurnDamageTaken = 0;
        CombatWon(game);
        return;
    }
    ResolveEnemies(game);
    int enemyHpAfter = 0;
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) enemyHpAfter += game->enemies[i].hp;
    int lowestEnemyHp = enemyHpAfter < enemyHpAfterPlayer ? enemyHpAfter : enemyHpAfterPlayer;
    game->lastTurnDamageDealt = enemyHpBefore - lowestEnemyHp;
    game->lastTurnDamageTaken = playerHpBefore - game->playerHp;
    if (game->lastTurnDamageTaken < 0) game->lastTurnDamageTaken = 0;
    wchar_t result[96];
    wsprintfW(result, L"실행 결과: 적 체력 -%d · 내 체력 -%d · 방어도 %d.",
        game->lastTurnDamageDealt, game->lastTurnDamageTaken, game->lastTurnBlockGained);
    PushLog(game, result);
    if (game->phase == PHASE_GAMEOVER) return;
    GimmickTurnEnd(game);
    if (LivingEnemyCount(game) == 0) {
        CombatWon(game);
        return;
    }
    ++game->turn;
    BeginTurn(game);
}

// 후보 수가 정해지지 않은 경로(테스트가 직접 세운 상태)는 기존대로 3개로 본다.
static int RewardCardCount(const GameState* game) {
    return game->rewardChoiceCount > 0 && game->rewardChoiceCount <= 3 ? game->rewardChoiceCount : 3;
}

void SelectReward(GameState* game, int rewardIndex) {
    if (game->phase != PHASE_REWARD || game->rewardIsTsr || rewardIndex < 0) return;
    if (rewardIndex >= RewardCardCount(game)) return;
    game->selectedReward = rewardIndex;
}

static void DamageRandomFace(GameState* game) {
    int candidates[18];
    int count = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            Face* face = &game->dice[d].faces[f];
            if (face->kind != FACE_EMPTY && !face->damaged) candidates[count++] = d * 6 + f;
        }
    }
    if (count == 0) return;
    int pick = candidates[RandomRange(game, count)];
    game->dice[pick / 6].faces[pick % 6].damaged = 1;
    PushLog(game, L"배드 섹터: 무작위 면 하나가 손상되었습니다.");
}

// 층 하강. CACHE의 임시 한도는 다음 층 용량 검사보다 먼저 사라진다.
static void EnterNextFloor(GameState* game) {
    ++game->floor;
    game->encounter = 0;
    if (game->floor >= 3) {
        game->phase = PHASE_VICTORY;
        return;
    }
    if (game->directory.floorCapacityBonus != 0) {
        game->directory.floorCapacityBonus = 0;
        PushLog(game, L"CACHE 임시 한도가 해제되었습니다.");
    }
    game->directory.intelThisFloor = 0;
    ZeroMemory(game->directory.floorCounts, sizeof(game->directory.floorCounts));
    if (IsModifierActive(game, MOD_BAD_SECTOR)) {
        if (IsTsrInstalled(game, TSR_SCANDISK)) PushLog2(game, L"%s: 배드 섹터 손상을 차단했습니다.", TSR_INFO[TSR_SCANDISK].name, 0);
        else DamageRandomFace(game);
    }
    if (UsedBytes(game) > EffectiveCapacity(game)) {
        game->phase = PHASE_PRUNE;
        game->pendingContinuation = CONTINUE_DIRECTORY;
        PushLog(game, L"용량 초과: 다음 층 한도에 맞게 정리하십시오.");
        return;
    }
    BeginDirectorySelection(game);
}

static void ContinueAfterReward(GameState* game) {
    if (UsedBytes(game) > EffectiveCapacity(game)) {
        game->phase = PHASE_PRUNE;
        game->pendingContinuation = CONTINUE_AFTER_REWARD;
        PushLog(game, L"현재 층 용량 초과: 면이나 상주 프로그램을 정리하십시오.");
        return;
    }
    if (game->encounter < 2) {
        ++game->encounter;
        // 두 번째 일반전 앞에는 다시 디렉터리 선택이 있고, 보스 앞에는 없다.
        if (game->encounter < 2) BeginDirectorySelection(game);
        else { ClearDirectoryCombatEffects(game); StartCombat(game); }
        return;
    }
    EnterNextFloor(game);
}

void InstallSelectedReward(GameState* game, int dieIndex, int faceIndex) {
    if (game->phase != PHASE_REWARD || game->rewardIsTsr || game->selectedReward < 0) return;
    if (game->selectedReward >= RewardCardCount(game)) return;
    if (dieIndex < 0 || dieIndex >= 3 || faceIndex < 0 || faceIndex >= 6) return;
    int reward = game->selectedReward;
    Face* face = &game->dice[dieIndex].faces[faceIndex];
    face->kind = (uint8_t)game->rewardKinds[reward];
    face->value = (uint8_t)game->rewardValues[reward];
    ++game->facesInstalled;
    PushLog2(game, L"%s 면 설치. 현재 덱 %dB.", FACE_INFO[face->kind].name, DeckBytes(game));
    ContinueAfterReward(game);
}

// 보스 전리품 카드를 클릭하면 그 자리에서 상주가 시작된다.
void InstallTsr(GameState* game, int rewardIndex) {
    if (game->phase != PHASE_REWARD || !game->rewardIsTsr) return;
    if (rewardIndex < 0 || rewardIndex >= 3) return;
    int tsr = game->rewardKinds[rewardIndex];
    if (tsr < 0 || tsr >= TSR_COUNT || game->tsrInstalled[tsr]) return;
    game->tsrInstalled[tsr] = 1;
    ++game->tsrsInstalled;
    PushLog2(game, L"%s 상주 시작. 사용 %dB.", TSR_INFO[tsr].name, UsedBytes(game));
    ContinueAfterReward(game);
}

// 면을 설치하는 대신 체력을 회복한다.
void RepairSector(GameState* game) {
    if (game->phase != PHASE_REWARD) return;
    if (game->playerHp >= game->playerMaxHp) return;
    int hpBefore = game->playerHp;
    game->playerHp = ClampInt(game->playerHp + SectorRepairAmount(game), 0, game->playerMaxHp);
    ++game->sectorsRepaired;
    wchar_t buffer[96];
    wsprintfW(buffer, L"섹터 복구: 체력 +%d (%d → %d).", game->playerHp - hpBefore, hpBefore, game->playerHp);
    PushLog(game, buffer);
    ContinueAfterReward(game);
}

void SkipReward(GameState* game) {
    if (game->phase != PHASE_REWARD) return;
    PushLog(game, L"보상을 건너뛰었습니다.");
    ContinueAfterReward(game);
}

void PruneFace(GameState* game, int dieIndex, int faceIndex) {
    if (game->phase != PHASE_PRUNE) return;
    if (dieIndex < 0 || dieIndex >= 3 || faceIndex < 0 || faceIndex >= 6) return;
    Face* face = &game->dice[dieIndex].faces[faceIndex];
    if (face->kind == FACE_EMPTY) return;
    face->kind = FACE_EMPTY;
    face->value = 0;
    face->damaged = 0;
    face->quarantined = QUAR_NONE;
    PushLog(game, L"면을 삭제해 용량을 확보했습니다.");
}

// 정리 화면에서 상주 프로그램을 종료해 용량을 되찾는다.
void UninstallTsr(GameState* game, int tsrIndex) {
    if (game->phase != PHASE_PRUNE) return;
    if (tsrIndex < 0 || tsrIndex >= TSR_COUNT || !game->tsrInstalled[tsrIndex]) return;
    game->tsrInstalled[tsrIndex] = 0;
    PushLog2(game, L"%s 종료. 사용 %dB.", TSR_INFO[tsrIndex].name, UsedBytes(game));
}

// KEYB: 판독이 끝난 뒤 턴마다 한 번, 선택한 주사위를 다시 굴린다.
void KeybReroll(GameState* game, int dieIndex) {
    if (game->phase != PHASE_COMBAT || !IsTsrInstalled(game, TSR_KEYB)) return;
    if (game->keybUsedThisTurn || dieIndex < 0 || dieIndex >= 3) return;
    game->dice[dieIndex].rolledFace = (uint8_t)RandomRange(game, 6);
    game->keybUsedThisTurn = 1;
    ApplyFragmentationIfAllowed(game);
    wchar_t buffer[96];
    wsprintfW(buffer, L"KEYB: 주사위 %d 재입력 → 출력 %d.", dieIndex + 1, FacePower(RolledFace(game, dieIndex)));
    PushLog(game, buffer);
}

void ConfirmPrune(GameState* game) {
    if (game->phase != PHASE_PRUNE) return;
    if (UsedBytes(game) > EffectiveCapacity(game)) {
        PushLog(game, L"아직 층 용량을 초과합니다.");
        return;
    }
    if (NonEmptyFaceCount(game) == 0) {
        PushLog(game, L"최소 한 면은 남겨야 합니다.");
        return;
    }
    int continuation = game->pendingContinuation;
    game->pendingContinuation = CONTINUE_NONE;
    if (continuation == CONTINUE_AFTER_REWARD) ContinueAfterReward(game);
    else if (continuation == CONTINUE_DIRECTORY) BeginDirectorySelection(game);
    else StartCombat(game);   // CONTINUE_COMBAT과 테스트가 직접 세운 경로
}
