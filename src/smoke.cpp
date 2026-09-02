#include <stdio.h>
#include <string.h>
#include "game.h"
#include "sprites.h"   // 테스트 실행 파일에서 47종 초상화 데이터를 직접 검증한다

static int Fail(const char* message) { printf("FAIL: %s\n", message); return 1; }

// 활성 로스터에서 사용하는 테스트 기본 드라이브. 손상 주입 테스트는
// preserveModifiers=1로 이 드라이브의 로스터만 빌리고 손상은 직접 지정한다.
#define TEST_DRIVE 0
#define TEST_SEED 0x5EED0001u

static int MostExpensiveFace(const GameState* game) {
    int best = -1, bestCost = -1;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        int cost = FaceCost(&game->dice[d].faces[f]);
        if (cost > bestCost) { bestCost = cost; best = d * 6 + f; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// 공용 헬퍼
// ---------------------------------------------------------------------------

static void SetAllFaces(GameState* game, int kind, int value) {
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        game->dice[d].faces[f].kind = (uint8_t)kind;
        game->dice[d].faces[f].value = (uint8_t)value;
        game->dice[d].faces[f].damaged = 0;
        game->dice[d].faces[f].quarantined = QUAR_NONE;
    }
}

// 보스전을 결정론적으로 준비한다. 손상은 재굴림해도 출력이 같도록
// 배드 섹터 + 읽기 오류를 쓰고, 모든 면을 같은 숫자로 통일한다.
static void SetupBossFight(GameState* game, int drive, int floor, uint32_t seed, int faceValue) {
    NewRun(game, seed);
    game->modifierA = MOD_BAD_SECTOR;
    game->modifierB = MOD_READ_ERROR;
    ConfigureDriveForTest(game, drive, seed, 1);
    game->floor = floor;
    game->encounter = 2;
    game->playerMaxHp = 999;
    game->playerHp = 999;
    StartCombat(game);
    SetAllFaces(game, FACE_NUMBER, faceValue);
}

static int FirstOpenSlot(const GameState* game) {
    for (int s = 0; s < SLOT_COUNT; ++s) if (!SlotLockedThisTurn(game, s)) return s;
    return -1;
}

// 주사위 0을 열린 슬롯에 놓고 턴을 넘긴다. 플레이어는 죽지 않게 유지한다.
static int PassTurn(GameState* game) {
    game->playerHp = 999;
    int slot = FirstOpenSlot(game);
    if (slot < 0) return 0;
    if (!AssignDieToSlot(game, 0, slot)) return 0;
    EndTurn(game);
    return game->phase == PHASE_COMBAT || game->phase == PHASE_REWARD || game->phase == PHASE_VICTORY;
}

// 주사위 0을 공격에 배치해 정확히 faceValue 피해를 넣는다 (증폭·특수 0 가정).
static int AttackTurn(GameState* game) {
    game->playerHp = 999;
    if (game->enemyCount > 0) game->enemies[0].block = 0;
    if (SlotLockedThisTurn(game, SLOT_ATTACK)) return 0;
    if (!AssignDieToSlot(game, 0, SLOT_ATTACK)) return 0;
    EndTurn(game);
    return 1;
}

static int TraceHealthy(const GameState* game) {
    return !game->turnTraceOverflow && game->turnTraceCount <= TURN_TRACE_CAP;
}

// ---------------------------------------------------------------------------
// 전체 런 (스폰 로직은 production 경로, 진행 흐름 검증)
// ---------------------------------------------------------------------------

static int RunCompleteGame(int drive, int modifierA, int modifierB, int preserveModifiers, unsigned int seed) {
    GameState game; NewRun(&game, seed);
    game.modifierA = modifierA; game.modifierB = modifierB;
    ConfigureDriveForTest(&game, drive, seed, preserveModifiers);
    game.floor = 0; game.encounter = 0; BeginDirectorySelection(&game);
    game.playerMaxHp = 999; game.playerHp = 999; int guard = 0;
    while (game.phase != PHASE_VICTORY && guard++ < 500) {
        if (game.phase == PHASE_DIRECTORY) {
            SelectDirectoryChoice(&game, 0);
        } else if (game.phase == PHASE_COMBAT) {
            game.playerHp = 999;
            for (int i = 0; i < game.enemyCount; ++i) if (game.enemies[i].alive) { game.enemies[i].hp = 1; game.enemies[i].block = 0; }
            AssignDieToSlot(&game, 0, SLOT_ATTACK); AssignDieToSlot(&game, 1, SLOT_AMPLIFY); AssignDieToSlot(&game, 2, SLOT_DEFEND); EndTurn(&game);
        } else if (game.phase == PHASE_REWARD) {
            if (game.rewardIsTsr) InstallTsr(&game, 0);
            else { SelectReward(&game, 0); InstallSelectedReward(&game, game.facesInstalled % 3, game.facesInstalled % 6); }
        } else if (game.phase == PHASE_PRUNE) {
            while (UsedBytes(&game) > EffectiveCapacity(&game)) {
                int index = MostExpensiveFace(&game); if (index < 0) return 5; PruneFace(&game, index / 6, index % 6);
            }
            ConfirmPrune(&game);
        } else if (game.phase == PHASE_GAMEOVER) return 1;
    }
    if (game.phase != PHASE_VICTORY) {
        printf("  stuck: phase %d floor %d encounter %d turn %d combatsWon %d\n",
            (int)game.phase, game.floor, game.encounter, game.turn, game.combatsWon);
        return 2;
    }
    if (game.combatsWon != 9) return 3;
    if (game.facesInstalled != 6) return 4;    // 일반 전투 6회 = 면 보상 6개
    if (game.tsrsInstalled != 2) return 6;     // 보스 2회 = 상주 프로그램 2개
    return 0;
}

// ---------------------------------------------------------------------------
// 데이터 무결성
// ---------------------------------------------------------------------------

static int CheckRosterIntegrity() {
    int kindCount = ENEMY_KIND_COUNT;
    if (kindCount != 47) return Fail("enemy kind count must be 47 (11 legacy + 36 active)");
    int active[ENEMY_KIND_COUNT] = {};
    int gimmickSeen[GIMMICK_COUNT] = {};
    for (int d = 0; d < DRIVE_COUNT; ++d) {
        for (int i = 0; i < DRIVE_MOB_COUNT; ++i) {
            int kind = DRIVE_MOBS[d][i];
            if (kind < 0 || kind >= ENEMY_KIND_COUNT) return Fail("drive mob kind out of range");
            if (kind < MOB_C_DLL_HIJACKER) return Fail("legacy enemies must not appear in active rosters");
            if (active[kind]) return Fail("mobs must not repeat across the rosters");
            active[kind] = 1;
            const EnemyInfo* info = &ENEMY_INFO[kind];
            if (info->role != ROLE_MOB) return Fail("drive mobs must have the mob role");
            if (info->pattern == PATTERN_LEGACY || info->pattern == PATTERN_BOSS) return Fail("active mobs must use a real pattern");
            if (info->gimmick != GIMMICK_NONE) return Fail("mobs must not carry a boss gimmick");
            // 층별 성장식: 체력·피해는 단조 증가, 방어는 감소하지 않아야 한다
            if (info->hpGrowth < 1 || info->damageGrowth < 1 || info->guardGrowth < 0) return Fail("mob growth must scale each floor");
            for (int j = 0; j < i; ++j) if (DRIVE_MOBS[d][j] == kind) return Fail("a drive's three mobs must differ");
        }
        for (int i = 0; i < DRIVE_BOSS_COUNT; ++i) {
            int kind = DRIVE_BOSSES[d][i];
            if (kind < 0 || kind >= ENEMY_KIND_COUNT) return Fail("drive boss kind out of range");
            if (kind < MOB_C_DLL_HIJACKER) return Fail("legacy bosses must not appear in active rosters");
            if (active[kind]) return Fail("bosses must not repeat across the rosters");
            active[kind] = 1;
            const EnemyInfo* info = &ENEMY_INFO[kind];
            if (info->role != ROLE_BOSS) return Fail("drive bosses must have the boss role");
            if (!IsBossKind(kind)) return Fail("IsBossKind must agree with the role metadata");
            if (info->gimmick <= GIMMICK_NONE || info->gimmick >= GIMMICK_COUNT) return Fail("active bosses need a valid gimmick");
            if (gimmickSeen[info->gimmick]) return Fail("all 18 boss gimmicks must be distinct");
            gimmickSeen[info->gimmick] = 1;
            if (BOSS_GIMMICK_INFO[info->gimmick].family == FAM_NONE) return Fail("boss gimmicks need a family");
        }
    }
    int activeCount = 0;
    for (int k = 0; k < ENEMY_KIND_COUNT; ++k) if (active[k]) ++activeCount;
    if (activeCount != 36) return Fail("active roster must reference exactly 36 kinds");
    // 레거시 검증: 로스터 미참조, 보스 3종은 GIMMICK_NONE
    for (int k = 0; k < MOB_C_DLL_HIJACKER; ++k) {
        if (active[k]) return Fail("legacy kinds must stay out of the rosters");
        if (ENEMY_INFO[k].gimmick != GIMMICK_NONE) return Fail("legacy enemies must use GIMMICK_NONE");
    }
    if (ENEMY_INFO[BOSS_DISK_ERROR].role != ROLE_BOSS || ENEMY_INFO[BOSS_FORMAT].role != ROLE_BOSS)
        return Fail("legacy bosses keep the boss role");
    if (IsBossKind(ENEMY_GLITCH) || !IsBossKind(BOSS_X_ZERO_DAY)) return Fail("role lookup mismatch");
    // 헤더의 static 데이터는 번역 단위마다 복제되므로 포인터 대신 내용으로 확인한다.
    if (wcscmp(GetEnemyInfoOrUnknown(-1)->code, L"UNKNOWN") != 0
        || wcscmp(GetEnemyInfoOrUnknown(ENEMY_KIND_COUNT)->code, L"UNKNOWN") != 0)
        return Fail("invalid kinds must resolve to the unknown info");
    return 0;
}

static int CheckOneSprite(const char* const* rows, const char* label) {
    int visible = 0;
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        const char* row = rows[y];
        if (!row) { printf("FAIL: %s row %d is null\n", label, y); return 1; }
        if ((int)strlen(row) != SPRITE_SIZE) { printf("FAIL: %s row %d must be 16 cells\n", label, y); return 1; }
        for (int x = 0; x < SPRITE_SIZE; ++x) {
            char c = row[x];
            if (!strchr(".X12345oWe", c)) { printf("FAIL: %s row %d has bad cell '%c'\n", label, y, c); return 1; }
            if (c != '.') ++visible;
        }
    }
    if (visible == 0) { printf("FAIL: %s is fully transparent\n", label); return 1; }
    return 0;
}

static int SpritesEqual(const char* const* a, const char* const* b) {
    for (int y = 0; y < SPRITE_SIZE; ++y) if (strcmp(a[y], b[y]) != 0) return 0;
    return 1;
}

static int CheckSprites() {
    char label[32];
    for (int k = 0; k < ENEMY_KIND_COUNT; ++k) {
        snprintf(label, sizeof(label), "sprite %d", k);
        if (CheckOneSprite(ENEMY_SPRITES[k], label)) return 1;
    }
    if (CheckOneSprite(UNKNOWN_SPRITE, "unknown sprite")) return 1;
    for (int a = 0; a < ENEMY_KIND_COUNT; ++a) {
        for (int b = a + 1; b < ENEMY_KIND_COUNT; ++b) {
            if (SpritesEqual(ENEMY_SPRITES[a], ENEMY_SPRITES[b])) {
                printf("FAIL: sprites %d and %d are identical\n", a, b); return 1;
            }
        }
        if (SpritesEqual(ENEMY_SPRITES[a], UNKNOWN_SPRITE)) {
            printf("FAIL: sprite %d duplicates the unknown fallback\n", a); return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 스폰과 진행
// ---------------------------------------------------------------------------

static int CheckSpawnMatrix() {
    for (int drive = 0; drive < DRIVE_COUNT; ++drive) {
        GameState game; NewRun(&game, 0x1000u + (unsigned int)drive);
        ConfigureDriveForTest(&game, drive, 0xABCD1234u + (unsigned int)drive, 0);
        int seen[ENEMY_KIND_COUNT] = {};
        for (int floor = 0; floor < 3; ++floor) {
            for (int encounter = 0; encounter < 3; ++encounter) {
                game.floor = floor; game.encounter = encounter;
                if (!StartCombat(&game)) return Fail("valid drive must start combat");
                if (game.enemyCount != 1) return Fail("each combat spawns one enemy");
                int kind = game.enemies[0].kind;
                if (encounter == 2) {
                    if (kind != DRIVE_BOSSES[drive][floor]) return Fail("boss combat must spawn the drive's floor boss");
                } else {
                    int inRoster = 0;
                    for (int i = 0; i < DRIVE_MOB_COUNT; ++i) if (DRIVE_MOBS[drive][i] == kind) inRoster = 1;
                    if (!inRoster) return Fail("normal combats must spawn only the drive's own mobs");
                    ++seen[kind];
                }
            }
        }
        for (int i = 0; i < DRIVE_MOB_COUNT; ++i) {
            if (seen[DRIVE_MOBS[drive][i]] != 2) return Fail("each mob must appear exactly twice across six normal combats");
        }
    }
    // 같은 시드는 같은 순서를 재현한다
    GameState a, b;
    NewRun(&a, 7u); ConfigureDriveForTest(&a, 2, 0xFEED0001u, 0);
    NewRun(&b, 99u); ConfigureDriveForTest(&b, 2, 0xFEED0001u, 0);
    for (int i = 0; i < 6; ++i) if (a.mobSchedule[i] != b.mobSchedule[i]) return Fail("mob schedule must be seed-reproducible");
    // 유효하지 않은 드라이브는 적을 만들지 않고 드라이브 선택으로 복귀한다
    GameState bad; NewRun(&bad, 5u);
    bad.floor = 0; bad.encounter = 0;
    if (StartCombat(&bad) != 0) return Fail("invalid drive must fail to start combat");
    if (bad.phase != PHASE_DRIVE_SELECT || bad.enemyCount != 0) return Fail("invalid drive must return to drive select with no enemies");
    // 일반 몹은 층 성장식으로 강해진다: 스폰된 체력이 base + growth × floor와 일치
    for (int floor = 0; floor < 3; ++floor) {
        GameState g; NewRun(&g, 11u); ConfigureDriveForTest(&g, 0, 0xC0DE01u, 1);
        g.modifierA = MOD_READ_ERROR; g.modifierB = MOD_CHECKSUM;
        g.floor = floor; g.encounter = 0; StartCombat(&g);
        const EnemyInfo* info = &ENEMY_INFO[g.enemies[0].kind];
        if (g.enemies[0].maxHp != info->hp + info->hpGrowth * floor) return Fail("mob hp must follow base + growth * floor");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 기믹 회귀: C:\ 슬롯 잠금
// ---------------------------------------------------------------------------

static int CheckLockGimmicks() {
    // ACCESS.DENIED: 짝수 턴 예고 잠금
    GameState g; SetupBossFight(&g, 0, 0, 0xC0C0C001u, 1);
    if (g.boss.gimmick != GIMMICK_ACCESS_DENIED) return Fail("floor1 C boss must use ACCESS.DENIED");
    if (FirstOpenSlot(&g) != 0 && SlotLockedThisTurn(&g, SLOT_ATTACK)) return Fail("turn 1 must be lock-free");
    if (!SlotLockedNextTurn(&g, SLOT_AMPLIFY)) return Fail("turn 2 amplify lock must be announced on turn 1");
    g.enemies[0].hp = 999; g.enemies[0].maxHp = 999;
    if (!PassTurn(&g)) return Fail("access denied turn 1 must pass");
    if (!SlotLockedThisTurn(&g, SLOT_AMPLIFY)) return Fail("announced amplify lock must fire on turn 2");
    if (AssignDieToSlot(&g, 1, SLOT_AMPLIFY)) return Fail("locked slot must reject placement");
    if (g.dice[1].assignedSlot != -1) return Fail("rejected placement must leave the die unassigned");
    if (!AssignDieToSlot(&g, 1, SLOT_DEFEND)) return Fail("open slots must still accept dice");
    if (!TraceHealthy(&g)) return Fail("lock turn must not overflow the trace");
    if (!PassTurn(&g)) return Fail("access denied turn 2 must pass");
    if (SlotLockedThisTurn(&g, SLOT_AMPLIFY)) return Fail("lock must release on the following turn");

    // KERNEL.PANIC: 직전 턴 최고 출력 슬롯 잠금
    GameState k; SetupBossFight(&k, 0, 1, 0xC0C0C002u, 5);
    if (k.boss.gimmick != GIMMICK_KERNEL_PANIC) return Fail("floor2 C boss must use KERNEL.PANIC");
    k.enemies[0].hp = 999; k.enemies[0].maxHp = 999;
    if (!AttackTurn(&k)) return Fail("kernel panic turn 1 must resolve");
    if (!SlotLockedThisTurn(&k, SLOT_ATTACK)) return Fail("kernel panic must lock last turn's best slot");

    // BLUE.SCREEN: 3턴마다 증폭+연쇄 동시 잠금
    GameState s; SetupBossFight(&s, 0, 2, 0xC0C0C003u, 1);
    if (s.boss.gimmick != GIMMICK_BLUE_SCREEN) return Fail("floor3 C boss must use BLUE.SCREEN");
    s.enemies[0].hp = 999; s.enemies[0].maxHp = 999;
    if (!PassTurn(&s)) return Fail("blue screen turn 1 must pass");
    if (!SlotLockedNextTurn(&s, SLOT_AMPLIFY) || !SlotLockedNextTurn(&s, SLOT_CHAIN)) return Fail("blue screen must announce the double lock");
    if (!PassTurn(&s)) return Fail("blue screen turn 2 must pass");
    if (!SlotLockedThisTurn(&s, SLOT_AMPLIFY) || !SlotLockedThisTurn(&s, SLOT_CHAIN)) return Fail("blue screen must lock amplify and chain on turn 3");
    if (SlotLockedThisTurn(&s, SLOT_ATTACK) || SlotLockedThisTurn(&s, SLOT_DEFEND)) return Fail("blue screen must leave attack and defend open");
    if (!PassTurn(&s)) return Fail("blue screen turn 3 must pass");
    if (SlotLockedThisTurn(&s, SLOT_AMPLIFY) || SlotLockedThisTurn(&s, SLOT_CHAIN)) return Fail("blue screen locks must release on turn 4");
    return 0;
}

// ---------------------------------------------------------------------------
// 기믹 회귀: D:\ 복원
// ---------------------------------------------------------------------------

static int CheckRestoreGimmicks() {
    // RESTORE.EXE: 3턴 창 미달 복원 + 상한
    GameState g; SetupBossFight(&g, 1, 0, 0xD0D0D001u, 2);
    if (g.boss.gimmick != GIMMICK_RESTORE_POINT) return Fail("floor1 D boss must use RESTORE.POINT");
    int startHp = g.enemies[0].hp;
    for (int t = 0; t < 3; ++t) if (!AttackTurn(&g)) return Fail("restore window turn must resolve");
    if (g.enemies[0].hp != startHp) return Fail("missing the damage goal must restore the checkpoint hp");
    if (g.boss.restoresUsed != 1) return Fail("restore use must be counted");
    // 요구 피해를 채우면 복원 없음
    SetAllFaces(&g, FACE_NUMBER, 5);
    int beforeWindow = g.enemies[0].hp;
    for (int t = 0; t < 3; ++t) if (!AttackTurn(&g)) return Fail("restore window 2 turn must resolve");
    if (g.enemies[0].hp != beforeWindow - 15) return Fail("meeting the damage goal must skip the restore");
    // 복원 상한 2회: 이후 창은 미달이어도 복원되지 않는다
    SetAllFaces(&g, FACE_NUMBER, 2);
    for (int t = 0; t < 3; ++t) if (!AttackTurn(&g)) return Fail("restore window 3 turn must resolve");
    if (g.boss.restoresUsed != 2) return Fail("second missed window must consume the last restore");
    int afterSecond = g.enemies[0].hp;
    for (int t = 0; t < 3; ++t) if (!AttackTurn(&g)) return Fail("restore window 4 turn must resolve");
    if (g.enemies[0].hp >= afterSecond) return Fail("exhausted restores must stop healing the boss");

    // TAPE.LOOP: 턴 미달 회복, 최대 체력·총량 상한
    GameState t2; SetupBossFight(&t2, 1, 1, 0xD0D0D002u, 2);
    if (t2.boss.gimmick != GIMMICK_TAPE_LOOP) return Fail("floor2 D boss must use TAPE.LOOP");
    int tapeMax = t2.enemies[0].maxHp;
    if (!AttackTurn(&t2)) return Fail("tape loop turn must resolve");
    if (t2.enemies[0].hp != tapeMax) return Fail("tape loop must rewind low-damage turns but never above max hp");
    SetAllFaces(&t2, FACE_NUMBER, 9);
    if (!AttackTurn(&t2)) return Fail("tape loop heavy turn must resolve");
    if (t2.enemies[0].hp != tapeMax - 9) return Fail("meeting the tape loop goal must skip the rewind");
    // 총 회복 상한: 미달 턴을 반복해도 24를 넘겨 회복하지 못한다
    SetAllFaces(&t2, FACE_NUMBER, 2);
    for (int t = 0; t < 20 && t2.phase == PHASE_COMBAT; ++t) if (!AttackTurn(&t2)) return Fail("tape loop cap turn must resolve");
    if (t2.boss.restoredTotal > 24) return Fail("tape loop total rewind must respect the cap");

    // MASTER.BACKUP: 40% 미만 1회 복원
    GameState m; SetupBossFight(&m, 1, 2, 0xD0D0D003u, 40);
    if (m.boss.gimmick != GIMMICK_MASTER_BACKUP) return Fail("floor3 D boss must use MASTER.BACKUP");
    int backupMax = m.enemies[0].maxHp;
    int checkpoint = backupMax * 60 / 100;
    if (!AttackTurn(&m)) return Fail("master backup burst turn must resolve");
    if (!m.boss.restoresUsed) return Fail("dropping under 40% must trigger the backup");
    if (m.enemies[0].hp > checkpoint) return Fail("backup must never restore above the checkpoint");
    int afterBackup = m.enemies[0].hp;
    if (afterBackup <= backupMax - 40) return Fail("backup must actually heal the boss");
    SetAllFaces(&m, FACE_NUMBER, 20);
    if (!AttackTurn(&m)) return Fail("master backup second burst must resolve");
    if (m.enemies[0].hp != afterBackup - 20) return Fail("master backup must fire only once");
    return 0;
}

// ---------------------------------------------------------------------------
// 기믹 회귀: E:\ 오프라인
// ---------------------------------------------------------------------------

static int CheckOfflineGimmicks() {
    // AUTOPLAY: 3턴마다, 예고 일치, 다음 턴 복구
    GameState g; SetupBossFight(&g, 2, 0, 0xE0E0E001u, 1);
    if (g.boss.gimmick != GIMMICK_AUTOPLAY) return Fail("floor1 E boss must use AUTOPLAY");
    g.enemies[0].hp = 999; g.enemies[0].maxHp = 999;
    if (!PassTurn(&g)) return Fail("autoplay turn 1 must pass");
    int announced = g.boss.nextOfflineDie;
    if (announced != 0) return Fail("autoplay must announce die 1 for turn 3");
    if (!PassTurn(&g)) return Fail("autoplay turn 2 must pass");
    if (g.boss.offlineDie != announced) return Fail("the announced die must match the offline die");
    if (!g.dice[announced].offline) return Fail("the offline die must carry the offline flag");
    for (int d = 0; d < 3; ++d) if (d != announced && g.dice[d].offline) return Fail("other dice must stay online");
    // 오프라인 주사위는 공격 출력 0
    int bossHp = g.enemies[0].hp;
    g.enemies[0].block = 0;
    AssignDieToSlot(&g, announced, SLOT_ATTACK);
    g.playerHp = 999;
    EndTurn(&g);
    if (g.enemies[0].hp != bossHp) return Fail("an offline die must contribute zero output");
    if (g.dice[announced].offline) return Fail("offline must recover on the next turn");

    // 오프라인 턴 조각화 억제 + DEFRAG 무관성
    GameState f; NewRun(&f, 0xE0E0E002u);
    f.modifierA = MOD_FRAGMENTATION; f.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&f, 2, 0xE0E0E002u, 1);
    f.floor = 0; f.encounter = 2; f.playerMaxHp = 999; f.playerHp = 999;
    StartCombat(&f);
    SetAllFaces(&f, FACE_NUMBER, 3);   // 모든 굴림이 중복 → 평소라면 조각화
    f.enemies[0].hp = 999; f.enemies[0].maxHp = 999;
    if (!PassTurn(&f)) return Fail("fragmentation fight turn 1 must pass");
    if (!PassTurn(&f)) return Fail("fragmentation fight turn 2 must pass");
    if (f.boss.offlineDie < 0) return Fail("autoplay must fire on turn 3");
    for (int d = 0; d < 3; ++d) if (f.dice[d].disabled) return Fail("fragmentation must be suppressed on the offline turn");
    // DEFRAG는 기존 조각화만 무효화하고 오프라인은 막지 않는다
    GameState dfr; NewRun(&dfr, 0xE0E0E003u);
    dfr.modifierA = MOD_FRAGMENTATION; dfr.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&dfr, 2, 0xE0E0E003u, 1);
    dfr.tsrInstalled[TSR_DEFRAG] = 1;
    dfr.floor = 0; dfr.encounter = 2; dfr.playerMaxHp = 999; dfr.playerHp = 999;
    StartCombat(&dfr);
    dfr.enemies[0].hp = 999; dfr.enemies[0].maxHp = 999;
    SetAllFaces(&dfr, FACE_NUMBER, 3);
    if (!PassTurn(&dfr) || !PassTurn(&dfr)) return Fail("defrag fight must reach turn 3");
    if (dfr.boss.offlineDie < 0 || !dfr.dice[dfr.boss.offlineDie].offline) return Fail("defrag must not block the offline gimmick");

    // UNSAFE.EJECT: 짝수 턴 발동
    GameState u; SetupBossFight(&u, 2, 1, 0xE0E0E004u, 1);
    if (u.boss.gimmick != GIMMICK_UNSAFE_EJECT) return Fail("floor2 E boss must use UNSAFE.EJECT");
    u.enemies[0].hp = 999; u.enemies[0].maxHp = 999;
    if (u.boss.nextOfflineDie != 0) return Fail("unsafe eject must announce die 1 for turn 2");
    if (!PassTurn(&u)) return Fail("unsafe eject turn 1 must pass");
    if (u.boss.offlineDie != 0) return Fail("unsafe eject must fire on even turns");
    if (!PassTurn(&u)) return Fail("unsafe eject turn 2 must pass");
    if (u.boss.offlineDie >= 0) return Fail("unsafe eject must rest on odd turns");
    if (!PassTurn(&u)) return Fail("unsafe eject turn 3 must pass");
    if (u.boss.offlineDie != 1) return Fail("unsafe eject must cycle to the next die");

    // NO.MEDIA: 매턴 발동, 4턴마다 인식 휴지
    GameState n; SetupBossFight(&n, 2, 2, 0xE0E0E005u, 1);
    if (n.boss.gimmick != GIMMICK_NO_MEDIA) return Fail("floor3 E boss must use NO.MEDIA");
    n.enemies[0].hp = 999; n.enemies[0].maxHp = 999;
    if (!PassTurn(&n)) return Fail("no media turn 1 must pass");
    if (n.boss.offlineDie != 2 % 3) return Fail("no media must fire on turn 2");
    if (!PassTurn(&n)) return Fail("no media turn 2 must pass");
    if (n.boss.offlineDie != 3 % 3) return Fail("no media must fire on turn 3");
    if (!PassTurn(&n)) return Fail("no media turn 3 must pass");
    if (n.boss.offlineDie >= 0) return Fail("no media must rest every 4th turn");
    return 0;
}

// ---------------------------------------------------------------------------
// 기믹 회귀: N:\ 순서 역전
// ---------------------------------------------------------------------------

static int CheckRouteGimmicks() {
    // PROXY: 3턴마다 역전, 증폭 보너스 소실
    GameState g; SetupBossFight(&g, 3, 0, 0xF0F0F001u, 4);
    if (g.boss.gimmick != GIMMICK_PROXY) return Fail("floor1 N boss must use PROXY");
    g.enemies[0].hp = 999; g.enemies[0].maxHp = 999;
    // 턴 1 (정상): 공격 4 + 증폭 2 = 6
    g.enemies[0].block = 0; g.playerHp = 999;
    AssignDieToSlot(&g, 0, SLOT_ATTACK); AssignDieToSlot(&g, 1, SLOT_AMPLIFY);
    int hpBefore = g.enemies[0].hp;
    EndTurn(&g);
    if (hpBefore - g.enemies[0].hp != 6) return Fail("normal order must apply the amplify bonus");
    if (g.lastTurnReversed) return Fail("turn 1 must not be reversed");
    if (!g.boss.nextReversed) return Fail("proxy must announce the turn 3 reversal during turn 2");
    if (!PassTurn(&g)) return Fail("proxy turn 2 must pass");
    if (!ResolveOrderReversed(&g)) return Fail("proxy must reverse turn 3");
    if (g.enemies[0].intent == INTENT_HEAVY || g.enemies[0].intent == INTENT_CORRUPT)
        return Fail("reversed turns must not stack heavy or piercing intents");
    // 턴 3 (역전): 공격 4 + 증폭 0 = 4
    g.enemies[0].block = 0; g.playerHp = 999;
    AssignDieToSlot(&g, 0, SLOT_ATTACK); AssignDieToSlot(&g, 1, SLOT_AMPLIFY);
    hpBefore = g.enemies[0].hp;
    EndTurn(&g);
    if (hpBefore - g.enemies[0].hp != 4) return Fail("reversed order must waste the amplify bonus");
    if (!g.lastTurnReversed) return Fail("the resolved-turn reversal flag must be recorded");
    if (!TraceHealthy(&g)) return Fail("reversed turn must not overflow the trace");
    if (ResolveOrderReversed(&g)) return Fail("reversal must clear on turn 4");

    // ROUTING.LOOP: 짝수 턴 역전
    GameState r; SetupBossFight(&r, 3, 1, 0xF0F0F002u, 1);
    if (r.boss.gimmick != GIMMICK_ROUTING_LOOP) return Fail("floor2 N boss must use ROUTING.LOOP");
    r.enemies[0].hp = 999; r.enemies[0].maxHp = 999;
    if (ResolveOrderReversed(&r)) return Fail("routing loop turn 1 must run forward");
    if (!r.boss.nextReversed) return Fail("routing loop must announce even-turn reversals");
    if (!PassTurn(&r)) return Fail("routing loop turn 1 must pass");
    if (!ResolveOrderReversed(&r)) return Fail("routing loop must reverse even turns");

    // TIMEOUT: 카운트다운·지연·발동 턴 보스 대기
    GameState t; SetupBossFight(&t, 3, 2, 0xF0F0F003u, 1);
    if (t.boss.gimmick != GIMMICK_TIMEOUT) return Fail("floor3 N boss must use TIMEOUT");
    t.enemies[0].hp = 999; t.enemies[0].maxHp = 999;
    if (t.boss.countdown != 3) return Fail("timeout countdown must start at 3");
    if (!PassTurn(&t)) return Fail("timeout turn 1 must pass");
    if (t.boss.countdown != 2) return Fail("timeout countdown must tick down");
    // 큰 피해로 카운트다운 지연
    SetAllFaces(&t, FACE_NUMBER, 14);
    if (!AttackTurn(&t)) return Fail("timeout delay turn must resolve");
    if (t.boss.countdown != 2) return Fail("12+ damage must delay the countdown");
    SetAllFaces(&t, FACE_NUMBER, 1);
    if (!PassTurn(&t)) return Fail("timeout turn 3 must pass");
    if (t.boss.countdown != 1) return Fail("countdown must resume after the delay");
    if (!t.boss.nextReversed) return Fail("countdown 1 must announce next-turn reversal");
    if (!PassTurn(&t)) return Fail("timeout turn 4 must pass");
    if (!ResolveOrderReversed(&t)) return Fail("countdown 0 must reverse the order");
    if (t.enemies[0].intent != INTENT_GUARD) return Fail("the timeout turn must make the boss wait");
    if (!PassTurn(&t)) return Fail("timeout fire turn must pass");
    if (t.boss.countdown != 3) return Fail("countdown must reset after firing");
    return 0;
}

// ---------------------------------------------------------------------------
// 기믹 회귀: R:\ 압력 게이지
// ---------------------------------------------------------------------------

static int CheckPressureGimmicks() {
    // LEAK.DLL: 게이지 상승 → 예고 → 강화 공격 → 초기화
    GameState g; SetupBossFight(&g, 4, 0, 0xA0A0A001u, 1);
    if (g.boss.gimmick != GIMMICK_LEAK) return Fail("floor1 R boss must use LEAK.DLL");
    g.enemies[0].hp = 999; g.enemies[0].maxHp = 999;
    for (int t = 0; t < 3; ++t) if (!PassTurn(&g)) return Fail("leak gauge turn must pass");
    if (g.boss.gauge != 3) return Fail("pressure must rise one per turn");
    if (!PassTurn(&g)) return Fail("leak turn 4 must pass");
    if (!g.boss.empowered) return Fail("full pressure must arm the empowered attack");
    int bossDamage = ENEMY_INFO[BOSS_R_LEAK_DLL].damage + BOSS_GIMMICK_INFO[GIMMICK_LEAK].p3;
    if (g.enemies[0].intent != INTENT_HEAVY || g.enemies[0].intentValue != bossDamage)
        return Fail("the empowered attack must be announced through the intent");
    int hpBefore = 999; g.playerHp = 999; g.playerBlock = 0;
    AssignDieToSlot(&g, 0, SLOT_CHAIN);   // 방어 없이 강화 공격을 받는다
    EndTurn(&g);
    if (hpBefore - g.playerHp != bossDamage) return Fail("the empowered attack must deal the announced damage");
    if (g.boss.empowered || g.boss.gauge != 0) return Fail("pressure must reset after the empowered attack");

    // 피해 임계 도달 시 게이지 감소
    GameState d; SetupBossFight(&d, 4, 0, 0xA0A0A002u, 12);
    d.enemies[0].hp = 999; d.enemies[0].maxHp = 999;
    if (!AttackTurn(&d)) return Fail("leak threshold turn must resolve");
    if (d.boss.gauge != 1) return Fail("first turn gauge should be 1 (reduce needs gauge > 0)");
    if (!AttackTurn(&d)) return Fail("leak threshold turn 2 must resolve");
    if (d.boss.gauge != 1) return Fail("10+ damage must vent one pressure before the rise");

    // 게이지가 가득 찬 턴에 보스를 죽이면 강화 공격이 취소된다
    GameState kkill; SetupBossFight(&kkill, 4, 0, 0xA0A0A003u, 5);
    kkill.enemies[0].hp = 999; kkill.enemies[0].maxHp = 999;
    SetAllFaces(&kkill, FACE_NUMBER, 1);
    for (int t = 0; t < 4; ++t) if (!PassTurn(&kkill)) return Fail("leak kill setup turn must pass");
    if (!kkill.boss.empowered) return Fail("kill test must reach the empowered turn");
    kkill.enemies[0].hp = 1; kkill.enemies[0].block = 0;
    kkill.playerHp = 999;
    AssignDieToSlot(&kkill, 0, SLOT_ATTACK);
    EndTurn(&kkill);
    if (kkill.phase != PHASE_REWARD) return Fail("killing the boss must end the fight");
    if (kkill.playerHp != 999) return Fail("a boss killed first must not fire its empowered attack");

    // HEAP.OVERFLOW: 강화 공격이 방어 관통(오염)
    GameState h; SetupBossFight(&h, 4, 1, 0xA0A0A004u, 1);
    if (h.boss.gimmick != GIMMICK_HEAP_OVERFLOW) return Fail("floor2 R boss must use HEAP.OVERFLOW");
    h.enemies[0].hp = 999; h.enemies[0].maxHp = 999;
    for (int t = 0; t < 3; ++t) if (!PassTurn(&h)) return Fail("heap overflow gauge turn must pass");
    if (!h.boss.empowered) return Fail("heap overflow must arm after three turns");
    if (h.enemies[0].intent != INTENT_CORRUPT) return Fail("heap overflow's empowered attack must pierce block");
    int pierce = ENEMY_INFO[BOSS_R_HEAP_OVERFLOW].damage + BOSS_GIMMICK_INFO[GIMMICK_HEAP_OVERFLOW].p3;
    h.playerHp = 999;
    AssignDieToSlot(&h, 0, SLOT_DEFEND);   // 방어를 쌓아도 관통된다
    EndTurn(&h);
    if (999 - h.playerHp != pierce) return Fail("the piercing empowered attack must ignore block");

    // OUT.OF.MEMORY: 임계 피해 시 게이지 -2
    GameState o; SetupBossFight(&o, 4, 2, 0xA0A0A005u, 1);
    if (o.boss.gimmick != GIMMICK_OUT_OF_MEMORY) return Fail("floor3 R boss must use OUT.OF.MEMORY");
    o.enemies[0].hp = 999; o.enemies[0].maxHp = 999;
    for (int t = 0; t < 3; ++t) if (!PassTurn(&o)) return Fail("oom gauge turn must pass");
    if (o.boss.gauge != 3) return Fail("oom gauge must reach 3");
    SetAllFaces(&o, FACE_NUMBER, 14);
    if (!AttackTurn(&o)) return Fail("oom vent turn must resolve");
    if (o.boss.gauge != 2) return Fail("14+ damage must vent two pressure");
    return 0;
}

// ---------------------------------------------------------------------------
// 기믹 회귀: X:\ 격리와 영구 삭제
// ---------------------------------------------------------------------------

static int CountQuarantined(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f)
        if (game->dice[d].faces[f].quarantined != QUAR_NONE) ++total;
    return total;
}

static int CheckQuarantineGimmicks() {
    // SAMPLE-13: 예고 → 전투 동안 격리, 용량·종류 불변, 최대 2, 종료 시 해제
    GameState g; SetupBossFight(&g, 5, 0, 0xB0B0B001u, 1);
    if (g.boss.gimmick != GIMMICK_SAMPLE13) return Fail("floor1 X boss must use SAMPLE-13");
    g.enemies[0].hp = 999; g.enemies[0].maxHp = 999;
    if (!PassTurn(&g) || !PassTurn(&g)) return Fail("sample-13 must reach turn 3");
    if (g.boss.nextTargetDie < 0) return Fail("sample-13 must announce its target one turn ahead");
    int td = g.boss.nextTargetDie, tf = g.boss.nextTargetFace;
    Face before = g.dice[td].faces[tf];
    if (!PassTurn(&g)) return Fail("sample-13 fire turn must pass");
    const Face* face = &g.dice[td].faces[tf];
    if (face->quarantined != QUAR_COMBAT) return Fail("the announced face must be quarantined for the combat");
    if (FacePower(face) != 0) return Fail("a quarantined face must output zero");
    if (FaceCost(face) != FaceCost(&before)) return Fail("quarantine must not change the face cost");
    if (face->kind != before.kind || face->value != before.value || face->damaged != before.damaged)
        return Fail("quarantine must not alter kind, value or damage");
    // 최대 2개: 세 번째 발동 주기까지 돌려도 격리는 2개까지만
    for (int t = 0; t < 7 && g.phase == PHASE_COMBAT; ++t) if (!PassTurn(&g)) return Fail("sample-13 cap turn must pass");
    if (g.boss.quarantinesDone > 2 || CountQuarantined(&g) > 2) return Fail("sample-13 must quarantine at most two faces");
    // 승리 시 격리 해제
    g.enemies[0].hp = 1; g.enemies[0].block = 0; g.playerHp = 999;
    int attacker = -1;
    for (int d2 = 0; d2 < 3; ++d2) if (FacePower(RolledFace(&g, d2)) > 0 && !g.dice[d2].offline) { attacker = d2; break; }
    if (attacker < 0) return Fail("an unquarantined attacker die must exist");
    AssignDieToSlot(&g, attacker, SLOT_ATTACK);
    EndTurn(&g);
    if (g.phase != PHASE_REWARD) return Fail("sample-13 kill must end the combat");
    if (CountQuarantined(&g) != 0) return Fail("victory must release every quarantined face");
    if (g.boss.gimmick != GIMMICK_NONE) return Fail("the boss runtime must be cleared after combat");

    // SANDBOX.BREACH: 2턴 격리 후 자동 해제
    GameState s; SetupBossFight(&s, 5, 1, 0xB0B0B002u, 1);
    if (s.boss.gimmick != GIMMICK_SANDBOX_BREACH) return Fail("floor2 X boss must use SANDBOX.BREACH");
    s.enemies[0].hp = 999; s.enemies[0].maxHp = 999;
    if (!PassTurn(&s) || !PassTurn(&s) || !PassTurn(&s)) return Fail("sandbox breach must reach its fire turn");
    if (CountQuarantined(&s) != 1) return Fail("sandbox breach must quarantine one face at the end of turn 3");
    if (!PassTurn(&s)) return Fail("sandbox breach hold turn must pass");
    if (CountQuarantined(&s) != 1) return Fail("the sandbox quarantine must persist for its duration");
    if (!PassTurn(&s)) return Fail("sandbox breach release turn must pass");
    if (CountQuarantined(&s) != 0) return Fail("the sandbox quarantine must release after two turns");

    // ZERO.DAY: 영구 삭제, 출력 가능한 면 1개 보장, 전투 후 유지
    GameState z; SetupBossFight(&z, 5, 2, 0xB0B0B003u, 1);
    if (z.boss.gimmick != GIMMICK_ZERO_DAY) return Fail("floor3 X boss must use ZERO.DAY");
    z.enemies[0].hp = 999; z.enemies[0].maxHp = 999;
    // 덱을 출력 가능한 면 2개로 축소한다
    SetAllFaces(&z, FACE_EMPTY, 0);
    z.dice[0].faces[0].kind = FACE_NUMBER; z.dice[0].faces[0].value = 5;
    z.dice[1].faces[0].kind = FACE_NUMBER; z.dice[1].faces[0].value = 5;
    if (UsableFaceCount(&z) != 2) return Fail("zero day test deck must hold two usable faces");
    for (int t = 0; t < 3; ++t) if (!PassTurn(&z)) return Fail("zero day gauge turn must pass");
    if (z.boss.nextTargetDie < 0 || !z.boss.nextTargetPermanent) return Fail("zero day must announce a permanent target");
    if (!PassTurn(&z)) return Fail("zero day fire turn must pass");
    if (UsableFaceCount(&z) != 1) return Fail("zero day must erase one face and leave one usable");
    int erased = 0;
    for (int d2 = 0; d2 < 3; ++d2) for (int f2 = 0; f2 < 6; ++f2) {
        const Face* face2 = &z.dice[d2].faces[f2];
        if (face2->kind == FACE_EMPTY && face2->quarantined != QUAR_NONE) return Fail("an erased face must not stay quarantined");
    }
    (void)erased;
    // 마지막 남은 출력 면은 영구 삭제되지 않는다 (임시 격리로 대체)
    for (int t = 0; t < 4 && z.phase == PHASE_COMBAT; ++t) if (!PassTurn(&z)) return Fail("zero day second cycle turn must pass");
    if (UsableFaceCount(&z) == 0 && CountQuarantined(&z) == 0)
        return Fail("the final usable face must never be permanently erased");
    int usableOrQuarantined = 0;
    for (int d2 = 0; d2 < 3; ++d2) for (int f2 = 0; f2 < 6; ++f2) {
        const Face* face2 = &z.dice[d2].faces[f2];
        if (face2->kind != FACE_EMPTY) ++usableOrQuarantined;
    }
    if (usableOrQuarantined < 1) return Fail("at least one non-empty face must survive zero day");
    // 전투 종료(패배 경로 포함) 후: 격리는 풀리고 영구 EMPTY는 유지된다
    for (int t = 0; t < 20 && z.phase == PHASE_COMBAT; ++t) {
        z.playerHp = 1;   // 보스의 다음 공격에 쓰러진다 (방어 턴이면 다음 턴에)
        int slot = FirstOpenSlot(&z);
        if (slot < 0) return Fail("zero day defeat turn needs an open slot");
        AssignDieToSlot(&z, 0, slot);
        EndTurn(&z);
    }
    if (z.phase != PHASE_GAMEOVER) return Fail("the zero day defeat path must reach game over");
    if (CountQuarantined(&z) != 0) return Fail("temporary quarantines must clear on game over too");
    if (NonEmptyFaceCount(&z) != 1) return Fail("the permanent erase must persist after combat");
    return 0;
}

// 전투 간 상태 누수: 보스전 이후 다음 전투에 임시 상태가 남지 않는다
static int CheckNoStateLeak() {
    GameState g; SetupBossFight(&g, 0, 0, 0xCAFE0001u, 5);
    g.enemies[0].hp = 1;
    if (!AttackTurn(&g)) return Fail("state leak setup must kill the boss");
    if (g.phase != PHASE_REWARD || !g.rewardIsTsr) return Fail("boss kill must reach the tsr reward");
    if (g.boss.gimmick != GIMMICK_NONE) return Fail("gimmick runtime must be cleared after the boss dies");
    for (int s = 0; s < SLOT_COUNT; ++s) if (SlotLockedThisTurn(&g, s) || SlotLockedNextTurn(&g, s)) return Fail("locks must not leak out of combat");
    if (ResolveOrderReversed(&g) || g.boss.nextReversed) return Fail("reversal must not leak out of combat");
    for (int d = 0; d < 3; ++d) if (g.dice[d].offline) return Fail("offline dice must not leak out of combat");
    if (CountQuarantined(&g) != 0) return Fail("quarantines must not leak out of combat");
    InstallTsr(&g, 0);
    if (g.phase != PHASE_DIRECTORY || g.floor != 1) return Fail("the run must continue to floor 2");
    SelectDirectoryChoice(&g, 0);
    if (g.phase != PHASE_COMBAT) return Fail("the new floor's directory must start the next combat");
    if (g.boss.gimmick != GIMMICK_NONE) return Fail("a normal combat must not inherit a boss gimmick");
    return 0;
}

// ---------------------------------------------------------------------------
// 디렉터리 경로 선택
// ---------------------------------------------------------------------------

static int FirstUsableFaceIndex(const GameState* game) {
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) if (FacePower(&game->dice[d].faces[f]) >= 1) return d * 6 + f;
    }
    return -1;
}

// 선택 화면에서 원하는 노드를 강제로 고른다 (생성 확률과 무관하게 효과만 검증).
static int ForceDirectoryNode(GameState* game, int kind) {
    if (game->phase != PHASE_DIRECTORY) return 0;
    game->directory.choices[0].kind = (uint8_t)kind;
    game->directory.choices[0].payload = 0;
    if (kind == DIR_NODE_CORRUPTED) {
        int face = FirstUsableFaceIndex(game);
        if (face < 0) return 0;
        game->directory.choices[0].payload = (uint8_t)face;
    }
    SelectDirectoryChoice(game, 0);
    return game->phase == PHASE_COMBAT;
}

// 전투는 즉시 이기고 보상은 건너뛰며 다음 디렉터리까지 진행한다.
static int AutoAdvanceToDirectory(GameState* game, int guardLimit) {
    int guard = 0;
    while (game->phase != PHASE_DIRECTORY && guard++ < guardLimit) {
        if (game->phase == PHASE_COMBAT) {
            game->playerMaxHp = 999; game->playerHp = 999;
            for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) { game->enemies[i].hp = 1; game->enemies[i].block = 0; }
            AssignDieToSlot(game, 0, SLOT_ATTACK); EndTurn(game);
        } else if (game->phase == PHASE_REWARD) {
            SkipReward(game);
        } else if (game->phase == PHASE_PRUNE) {
            while (UsedBytes(game) > EffectiveCapacity(game)) {
                int index = MostExpensiveFace(game); if (index < 0) return 0; PruneFace(game, index / 6, index % 6);
            }
            ConfirmPrune(game);
        } else return 0;
    }
    return game->phase == PHASE_DIRECTORY;
}

// 생성 불변식: 중복 없음, 둘 다 고위험 없음, 같은 축 두 개 없음, 층·연속 제한 준수
static int CheckDirectoryGeneration() {
    for (int drive = 0; drive < DRIVE_COUNT; ++drive) {
        for (unsigned int seed = 1; seed <= 40; ++seed) {
            GameState g; NewRun(&g, 0xD1A00000u + seed * 131u + (unsigned int)drive);
            g.driveChoices[0] = drive;
            SelectDrive(&g, 0);
            if (g.phase != PHASE_DIRECTORY) return Fail("mounting a volume must open the directory choice");
            for (int step = 0; step < 6; ++step) {
                if (g.phase != PHASE_DIRECTORY) break;
                if (DirectoryChoiceCount(&g) != DIRECTORY_CHOICE_COUNT) return Fail("the directory must always offer two cards");
                int a = g.directory.choices[0].kind, b = g.directory.choices[1].kind;
                const DirectoryNodeInfo* ia = DirectoryNodeInfoOrNull(a);
                const DirectoryNodeInfo* ib = DirectoryNodeInfoOrNull(b);
                if (!ia || !ib) return Fail("directory cards must carry a valid node kind");
                if (!ia->enabled || !ib->enabled) return Fail("disabled nodes must never be offered");
                if (a == b) return Fail("the two directory cards must differ");
                int highA = ia->risk == DIR_RISK_HIGH || ia->risk == DIR_RISK_UNKNOWN;
                int highB = ib->risk == DIR_RISK_HIGH || ib->risk == DIR_RISK_UNKNOWN;
                if (highA && highB) return Fail("two high risk directories must never be offered together");
                if (ia->category == ib->category && !highA && !highB) return Fail("two safe nodes on the same axis must not pair up");
                if (DIRECTORY_DRIVE_WEIGHT[drive][a] == 0 || DIRECTORY_DRIVE_WEIGHT[drive][b] == 0)
                    return Fail("a node with zero weight must not appear on this drive");
                if ((a == DIR_NODE_CORRUPTED || b == DIR_NODE_CORRUPTED) && g.floor < 1)
                    return Fail("corrupted must not appear on the first floor");
                if (g.directory.previousKind != DIR_NODE_NONE
                    && (a == g.directory.previousKind || b == g.directory.previousKind))
                    return Fail("the node picked last time must not be offered again");
                if (g.directory.floorCounts[a] >= ia->maxPerFloor || g.directory.floorCounts[b] >= ib->maxPerFloor)
                    return Fail("a node past its per-floor limit must not be offered");
                SelectDirectoryChoice(&g, 0);
                if (g.phase != PHASE_COMBAT) return Fail("choosing a directory card must start the combat");
                if (!AutoAdvanceToDirectory(&g, 60)) break;
            }
            for (int f = 0; f < 3; ++f) {
                int first = g.directory.history[f][0], second = g.directory.history[f][1];
                if (first != DIR_NODE_NONE && first == second) return Fail("a node must not be chosen twice in a row");
            }
        }
    }
    return 0;
}

// 별도 난수열: 조회·잘못된 입력은 상태를 바꾸지 않고, 같은 seed는 같은 카드를 낸다.
static int CheckDirectoryRng() {
    GameState g; NewRun(&g, 0xD1B00001u); g.driveChoices[0] = 0;
    uint32_t combatRngBefore = g.rng;
    SelectDrive(&g, 0);
    if (g.rng == combatRngBefore) return Fail("mounting must still consume the schedule seed");
    if (g.directory.rng == 0) return Fail("the directory rng must be seeded");

    GameState snapshot = g;
    wchar_t path[96];
    // 리페인트가 부르는 조회 함수들은 상태를 한 바이트도 바꾸지 않아야 한다.
    for (int i = 0; i < 8; ++i) {
        FormatCurrentDirectory(&g, path, 96);
        DirectoryChoiceCount(&g);
        DirectoryIntelActive(&g);
        ScheduledMobKind(&g);
        FloorBossKind(&g);
        for (int k = 0; k < DIR_NODE_COUNT; ++k) DirectoryNodeAllowed(&g, k);
    }
    if (memcmp(&snapshot, &g, sizeof(GameState)) != 0) return Fail("reading the directory screen must not change the state");
    // 잘못된 index는 무시된다 (오클릭으로 선택지가 다시 뽑히면 안 된다).
    SelectDirectoryChoice(&g, -1);
    SelectDirectoryChoice(&g, DIRECTORY_CHOICE_COUNT);
    SelectDirectoryChoice(&g, 99);
    if (memcmp(&snapshot, &g, sizeof(GameState)) != 0) return Fail("an invalid directory index must not change the state");
    // 잘못된 phase에서도 마찬가지다.
    GameState wrongPhase; NewRun(&wrongPhase, 0xD1B00002u);
    GameState wrongPhaseCopy = wrongPhase;
    SelectDirectoryChoice(&wrongPhase, 0);
    if (memcmp(&wrongPhaseCopy, &wrongPhase, sizeof(GameState)) != 0) return Fail("selecting a directory outside its phase must be ignored");

    // 생성 자체는 전투 난수열을 소비하지 않는다.
    GameState quiet; NewRun(&quiet, 0xD1B00004u); quiet.driveChoices[0] = 0; SelectDrive(&quiet, 0);
    uint32_t rngBeforeGeneration = quiet.rng;
    uint32_t dirRngBefore = quiet.directory.rng;
    quiet.phase = PHASE_DIRECTORY;
    BeginDirectorySelection(&quiet);
    if (quiet.rng != rngBeforeGeneration) return Fail("generating directory cards must not consume the combat rng");
    if (quiet.directory.rng == dirRngBefore) return Fail("generating directory cards must consume the directory rng");

    // 같은 seed·드라이브면 같은 선택지가 재현된다.
    GameState a1, a2;
    NewRun(&a1, 0xD1B00003u); a1.driveChoices[0] = 3; SelectDrive(&a1, 0);
    NewRun(&a2, 0xD1B00003u); a2.driveChoices[0] = 3; SelectDrive(&a2, 0);
    for (int step = 0; step < 4; ++step) {
        if (a1.phase != PHASE_DIRECTORY || a2.phase != PHASE_DIRECTORY) break;
        if (memcmp(a1.directory.choices, a2.directory.choices, sizeof(a1.directory.choices)) != 0)
            return Fail("the same seed and drive must reproduce the same directory cards");
        SelectDirectoryChoice(&a1, 0); SelectDirectoryChoice(&a2, 0);
        if (!AutoAdvanceToDirectory(&a1, 60) || !AutoAdvanceToDirectory(&a2, 60)) break;
    }
    return 0;
}

// 진행: 드라이브 → 디렉터리 → 일반전 → 보상 → 디렉터리 → 일반전 → 보상 → 보스 → 전리품 → 다음 층
static int CheckDirectoryProgression() {
    GameState g; NewRun(&g, 0xD1C00001u); g.driveChoices[0] = 0;
    SelectDrive(&g, 0);
    if (g.phase != PHASE_DIRECTORY || g.encounter != 0) return Fail("the first directory must sit before encounter 0");
    if (!ForceDirectoryNode(&g, DIR_NODE_PROCESS)) return Fail("choosing a directory must start the scheduled combat");
    if (g.encounter != 0) return Fail("the first directory must not skip an encounter");
    g.playerMaxHp = 999; g.playerHp = 999;
    for (int i = 0; i < g.enemyCount; ++i) { g.enemies[i].hp = 1; g.enemies[i].block = 0; }
    AssignDieToSlot(&g, 0, SLOT_ATTACK); EndTurn(&g);
    if (g.phase != PHASE_REWARD || g.rewardIsTsr) return Fail("a mob kill must offer a face reward");
    SkipReward(&g);
    if (g.phase != PHASE_DIRECTORY || g.encounter != 1) return Fail("the second directory must sit before encounter 1");
    if (!ForceDirectoryNode(&g, DIR_NODE_PROCESS)) return Fail("the second directory must start encounter 1");
    for (int i = 0; i < g.enemyCount; ++i) { g.enemies[i].hp = 1; g.enemies[i].block = 0; }
    AssignDieToSlot(&g, 0, SLOT_ATTACK); EndTurn(&g);
    SkipReward(&g);
    if (g.phase != PHASE_COMBAT || g.encounter != 2) return Fail("the boss must follow the second reward with no choice screen");
    if (g.directory.activeKind != DIR_NODE_NONE) return Fail("no directory effect may carry into the boss fight");
    for (int i = 0; i < g.enemyCount; ++i) { g.enemies[i].hp = 1; g.enemies[i].block = 0; }
    AssignDieToSlot(&g, 0, SLOT_ATTACK); EndTurn(&g);
    if (g.phase != PHASE_REWARD || !g.rewardIsTsr) return Fail("the boss kill must offer resident loot");
    SkipReward(&g);
    if (g.phase != PHASE_DIRECTORY || g.floor != 1 || g.encounter != 0) return Fail("the next floor must open with a directory choice");
    if (g.directory.intelThisFloor) return Fail("intel must not carry across floors");

    // 최종 보스는 디렉터리도 보상도 거치지 않고 승리로 간다.
    GameState last; NewRun(&last, 0xD1C00002u); last.modifierA = MOD_BAD_SECTOR; last.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&last, TEST_DRIVE, TEST_SEED, 1);
    last.floor = 2; last.encounter = 2; last.playerMaxHp = 999; last.playerHp = 999;
    StartCombat(&last);
    last.enemies[0].hp = 1; last.enemies[0].block = 0;
    AssignDieToSlot(&last, 0, SLOT_ATTACK); EndTurn(&last);
    if (last.phase != PHASE_VICTORY) return Fail("the final boss must go straight to victory");
    return 0;
}

// 노드별 효과
static int CheckDirectoryNodes() {
    // TEMP: 회복 상한, 보상 후보 2개, 그리고 다음 전투에서의 복귀
    GameState temp; NewRun(&temp, 0xD1D00001u); temp.driveChoices[0] = 0; SelectDrive(&temp, 0);
    temp.playerHp = temp.playerMaxHp - 2;
    if (!ForceDirectoryNode(&temp, DIR_NODE_TEMP)) return Fail("temp must start the combat");
    if (temp.playerHp != temp.playerMaxHp) return Fail("temp must never overheal");
    temp.playerMaxHp = 999; temp.playerHp = 999;
    for (int i = 0; i < temp.enemyCount; ++i) { temp.enemies[i].hp = 1; temp.enemies[i].block = 0; }
    AssignDieToSlot(&temp, 0, SLOT_ATTACK); EndTurn(&temp);
    if (temp.phase != PHASE_REWARD) return Fail("temp combat must end in a reward");
    if (temp.rewardChoiceCount != 2) return Fail("temp must cut the face reward down to two candidates");
    if (temp.rewardTier != 0) return Fail("temp must keep the standard reward tier");
    SelectReward(&temp, 2);
    if (temp.selectedReward != -1) return Fail("the removed third candidate must not be selectable");
    SelectReward(&temp, 1);
    if (temp.selectedReward != 1) return Fail("the remaining candidates must stay selectable");
    SkipReward(&temp);
    if (temp.phase != PHASE_DIRECTORY) return Fail("temp must return to the next directory");
    if (!ForceDirectoryNode(&temp, DIR_NODE_PROCESS)) return Fail("process must follow temp");
    for (int i = 0; i < temp.enemyCount; ++i) { temp.enemies[i].hp = 1; temp.enemies[i].block = 0; }
    AssignDieToSlot(&temp, 0, SLOT_ATTACK); EndTurn(&temp);
    if (temp.rewardChoiceCount != 3) return Fail("the reward candidate count must reset after temp");

    GameState full; NewRun(&full, 0xD1D00002u); full.driveChoices[0] = 0; SelectDrive(&full, 0);
    full.playerHp = full.playerMaxHp;
    if (DirectoryNodeAllowed(&full, DIR_NODE_TEMP)) return Fail("temp must not be offered at full health");

    // CACHE: 임시 한도 +20B, 층 이동 시 해제, 그 결과 초과하면 정리 화면
    GameState cache; NewRun(&cache, 0xD1D00003u); cache.driveChoices[0] = 0; SelectDrive(&cache, 0);
    int capacityBefore = EffectiveCapacity(&cache);
    if (!ForceDirectoryNode(&cache, DIR_NODE_CACHE)) return Fail("cache must start the combat");
    if (EffectiveCapacity(&cache) != capacityBefore + DIR_CACHE_BYTES) return Fail("cache must lift the floor capacity");
    if (cache.enemies[0].block != DIR_CACHE_BLOCK) return Fail("cache must hand the enemy starting block");
    cache.playerMaxHp = 999; cache.playerHp = 999;
    for (int i = 0; i < cache.enemyCount; ++i) { cache.enemies[i].hp = 1; cache.enemies[i].block = 0; }
    AssignDieToSlot(&cache, 0, SLOT_ATTACK); EndTurn(&cache);
    SkipReward(&cache);
    if (EffectiveCapacity(&cache) != capacityBefore + DIR_CACHE_BYTES) return Fail("cache must survive until the floor ends");
    // 1층 한도(+20B) 안에는 들어가지만 2층 한도는 넘는 덱을 만든다.
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        cache.dice[d].faces[f].kind = FACE_NUMBER; cache.dice[d].faces[f].value = 14; cache.dice[d].faces[f].damaged = 0;
    }
    if (UsedBytes(&cache) > EffectiveCapacity(&cache)) return Fail("the cache test deck must fit inside the lifted limit");
    cache.encounter = 2; cache.phase = PHASE_REWARD;
    SkipReward(&cache);
    if (cache.directory.floorCapacityBonus != 0) return Fail("the cache bonus must be released before the next floor check");
    if (cache.phase != PHASE_PRUNE) return Fail("losing the cache bonus over the limit must force a prune");
    if (cache.pendingContinuation != CONTINUE_DIRECTORY) return Fail("a floor-entry prune must return to the directory");
    while (UsedBytes(&cache) > EffectiveCapacity(&cache)) {
        int index = MostExpensiveFace(&cache); if (index < 0) return Fail("unable to prune the cache test deck");
        PruneFace(&cache, index / 6, index % 6);
    }
    ConfirmPrune(&cache);
    if (cache.phase != PHASE_DIRECTORY || cache.floor != 1) return Fail("confirming that prune must open the new floor's directory");

    // LOGS: 판독 기록은 건드리지 않고 임시 정보만 연다
    GameState logs; NewRun(&logs, 0xD1D00004u); logs.driveChoices[0] = 3; SelectDrive(&logs, 0);
    uint8_t scannedBefore[ENEMY_KIND_COUNT];
    memcpy(scannedBefore, logs.enemyScanned, sizeof(scannedBefore));
    if (!ForceDirectoryNode(&logs, DIR_NODE_LOGS)) return Fail("logs must start the combat");
    if (memcmp(scannedBefore, logs.enemyScanned, sizeof(scannedBefore)) != 0) return Fail("logs must never change the scan record");
    if (!DirectoryIntelActive(&logs)) return Fail("logs must open this floor's intel");
    if (logs.enemies[0].block != DIR_LOGS_BLOCK) return Fail("logs must hand the enemy starting block");
    logs.playerMaxHp = 999; logs.playerHp = 999;
    for (int i = 0; i < logs.enemyCount; ++i) { logs.enemies[i].hp = 1; logs.enemies[i].block = 0; }
    AssignDieToSlot(&logs, 0, SLOT_ATTACK); EndTurn(&logs);
    SkipReward(&logs);
    if (logs.phase != PHASE_DIRECTORY) return Fail("logs must return to the next directory");
    if (DirectoryNodeAllowed(&logs, DIR_NODE_LOGS)) return Fail("logs must not be offered twice on one floor");

    // INFECTED: 적 최대 체력 +20%와 강화 보상
    GameState plain; NewRun(&plain, 0xD1D00005u); plain.driveChoices[0] = 0; SelectDrive(&plain, 0);
    if (!ForceDirectoryNode(&plain, DIR_NODE_PROCESS)) return Fail("the control run must start combat");
    int plainHp = plain.enemies[0].maxHp;
    GameState infected; NewRun(&infected, 0xD1D00005u); infected.driveChoices[0] = 0; SelectDrive(&infected, 0);
    if (!ForceDirectoryNode(&infected, DIR_NODE_INFECTED)) return Fail("infected must start combat");
    if (infected.enemies[0].maxHp != plainHp * DIR_INFECTED_HP_PERCENT / 100)
        return Fail("infected must raise the enemy max hp by the fixed percent");
    if (infected.enemies[0].hp != infected.enemies[0].maxHp) return Fail("infected must fill the raised hp");
    infected.playerMaxHp = 999; infected.playerHp = 999;
    for (int i = 0; i < infected.enemyCount; ++i) { infected.enemies[i].hp = 1; infected.enemies[i].block = 0; }
    AssignDieToSlot(&infected, 0, SLOT_ATTACK); EndTurn(&infected);
    if (infected.rewardTier != 1) return Fail("infected must produce a tuned reward");
    if (infected.rewardChoiceCount != 3) return Fail("a tuned reward must still offer three candidates");
    for (int i = 0; i < 3; ++i) for (int j = 0; j < i; ++j)
        if (infected.rewardKinds[i] == infected.rewardKinds[j]) return Fail("a tuned reward must not repeat a face kind");
    for (int i = 0; i < 3; ++i)
        if (infected.rewardKinds[i] == FACE_NUMBER && infected.rewardValues[i] < 8)
            return Fail("a tuned number face must roll from the raised range");

    // CORRUPTED: 대상 면은 비용을 유지하고 출력만 0, 전투가 끝나면 풀린다
    GameState corrupted; NewRun(&corrupted, 0xD1D00006u); corrupted.driveChoices[0] = 5; SelectDrive(&corrupted, 0);
    corrupted.floor = 1;
    int target = FirstUsableFaceIndex(&corrupted);
    if (target < 0) return Fail("the corrupted test needs a usable face");
    int costBefore = FaceCost(&corrupted.dice[target / 6].faces[target % 6]);
    int usableBefore = UsableFaceCount(&corrupted);
    if (!ForceDirectoryNode(&corrupted, DIR_NODE_CORRUPTED)) return Fail("corrupted must start the combat");
    const Face* hit = &corrupted.dice[target / 6].faces[target % 6];
    if (hit->quarantined == QUAR_NONE) return Fail("corrupted must quarantine its announced face");
    if (FacePower(hit) != 0) return Fail("a quarantined face must produce nothing");
    if (FaceCost(hit) != costBefore) return Fail("a quarantined face must still cost its bytes");
    if (UsableFaceCount(&corrupted) != usableBefore - 1) return Fail("corrupted must remove exactly one usable face");
    corrupted.playerMaxHp = 999; corrupted.playerHp = 999;
    for (int i = 0; i < corrupted.enemyCount; ++i) { corrupted.enemies[i].hp = 1; corrupted.enemies[i].block = 0; }
    AssignDieToSlot(&corrupted, 0, SLOT_ATTACK); EndTurn(&corrupted);
    if (corrupted.dice[target / 6].faces[target % 6].quarantined != QUAR_NONE)
        return Fail("winning must release the directory quarantine");
    if (corrupted.rewardTier != 1) return Fail("corrupted must produce a tuned reward");

    // 사망해도 격리가 남지 않는다
    GameState doomed; NewRun(&doomed, 0xD1D00007u); doomed.driveChoices[0] = 5; SelectDrive(&doomed, 0);
    doomed.floor = 1;
    if (!ForceDirectoryNode(&doomed, DIR_NODE_CORRUPTED)) return Fail("the death test must enter the corrupted combat");
    doomed.playerHp = 1;
    doomed.enemies[0].hp = 999; doomed.enemies[0].maxHp = 999;
    doomed.enemies[0].intent = INTENT_HEAVY; doomed.enemies[0].intentValue = 500;
    AssignDieToSlot(&doomed, 0, SLOT_ATTACK); EndTurn(&doomed);
    if (doomed.phase != PHASE_GAMEOVER) return Fail("the death test must reach game over");
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f)
        if (doomed.dice[d].faces[f].quarantined != QUAR_NONE) return Fail("dying must release the directory quarantine too");
    // 새 런은 경로 상태를 물려받지 않는다.
    NewRun(&doomed, 0xD1D00008u);
    if (doomed.directory.activeKind != DIR_NODE_NONE || doomed.directory.previousKind != DIR_NODE_NONE
        || doomed.directory.intelThisFloor || doomed.directory.floorCapacityBonus != 0)
        return Fail("a new run must not inherit any directory state");
    for (int f = 0; f < 3; ++f) for (int i = 0; i < DIRECTORY_PER_FLOOR; ++i)
        if (doomed.directory.history[f][i] != DIR_NODE_NONE) return Fail("a new run must clear the directory history");

    // 면을 전부 지운 채로는 디렉터리로 진행할 수 없다.
    GameState wiped; NewRun(&wiped, 0xD1D0000Au); wiped.driveChoices[0] = 0; SelectDrive(&wiped, 0);
    wiped.phase = PHASE_PRUNE; wiped.pendingContinuation = CONTINUE_DIRECTORY;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) PruneFace(&wiped, d, f);
    ConfirmPrune(&wiped);
    if (wiped.phase != PHASE_PRUNE) return Fail("an empty deck must not be allowed to continue");
    wiped.dice[0].faces[0].kind = FACE_NUMBER; wiped.dice[0].faces[0].value = 4;
    ConfirmPrune(&wiped);
    if (wiped.phase != PHASE_DIRECTORY) return Fail("one restored face must let the prune resume the directory");

    // CORRUPTED는 출력 가능한 면이 모자라면 아예 등장하지 않는다.
    GameState bare; NewRun(&bare, 0xD1D00009u); bare.driveChoices[0] = 5; SelectDrive(&bare, 0);
    bare.floor = 1;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        bare.dice[d].faces[f].kind = FACE_EMPTY; bare.dice[d].faces[f].value = 0;
    }
    bare.dice[0].faces[0].kind = FACE_NUMBER; bare.dice[0].faces[0].value = 4;
    if (DirectoryNodeAllowed(&bare, DIR_NODE_CORRUPTED)) return Fail("corrupted must not be offered with too few usable faces");
    return 0;
}

// 경로 문자열은 저장되지 않고 층 경로 + 방문 노드로 조합된다.
static int CheckDirectoryPath() {
    for (int k = DIR_NODE_PROCESS; k < DIR_NODE_COUNT; ++k)
        if (wcslen(DIRECTORY_NODE_INFO[k].segment) > 11) return Fail("a directory segment must stay within 11 characters");

    GameState g; NewRun(&g, 0xD1E00001u); g.driveChoices[0] = 0; SelectDrive(&g, 0);
    wchar_t path[96];
    FormatCurrentDirectory(&g, path, 96);
    if (wcscmp(path, DRIVE_INFO[0].paths[0]) != 0) return Fail("an unvisited floor must show only its own path");
    if (!ForceDirectoryNode(&g, DIR_NODE_TEMP)) return Fail("the path test must enter a combat");
    FormatCurrentDirectory(&g, path, 96);
    if (wcscmp(path, L"C:\\TEMP") != 0) return Fail("the chosen segment must append to the floor path");
    g.playerMaxHp = 999; g.playerHp = 999;
    for (int i = 0; i < g.enemyCount; ++i) { g.enemies[i].hp = 1; g.enemies[i].block = 0; }
    AssignDieToSlot(&g, 0, SLOT_ATTACK); EndTurn(&g);
    SkipReward(&g);
    if (!ForceDirectoryNode(&g, DIR_NODE_INFECTED)) return Fail("the path test must enter the second combat");
    FormatCurrentDirectory(&g, path, 96);
    if (wcscmp(path, L"C:\\TEMP\\INFECTED") != 0) return Fail("both visited segments must appear in order");
    // 고정 버퍼 밖으로 넘치지 않는다.
    wchar_t tiny[6];
    FormatCurrentDirectory(&g, tiny, 6);
    if (wcslen(tiny) >= 6) return Fail("path formatting must respect the buffer cap");
    return 0;
}

int main() {
    if (CheckRosterIntegrity()) return 1;
    if (CheckSprites()) return 1;
    if (CheckSpawnMatrix()) return 1;
    if (CheckLockGimmicks()) return 1;
    if (CheckRestoreGimmicks()) return 1;
    if (CheckOfflineGimmicks()) return 1;
    if (CheckRouteGimmicks()) return 1;
    if (CheckPressureGimmicks()) return 1;
    if (CheckQuarantineGimmicks()) return 1;
    if (CheckNoStateLeak()) return 1;
    if (CheckDirectoryGeneration()) return 1;
    if (CheckDirectoryRng()) return 1;
    if (CheckDirectoryProgression()) return 1;
    if (CheckDirectoryNodes()) return 1;
    if (CheckDirectoryPath()) return 1;

    GameState base; NewRun(&base, 0x12345678u);
    if (DeckBytes(&base) != 63) return Fail("starting deck must be 63 bytes");
    if (base.phase != PHASE_DRIVE_SELECT) return Fail("new run must offer drive choices");
    if (base.driveChoices[0] == base.driveChoices[1] || base.driveChoices[0] == base.driveChoices[2]
        || base.driveChoices[1] == base.driveChoices[2]) return Fail("drive choices must be unique");
    SelectDrive(&base, 1);
    if (base.selectedDrive != base.driveChoices[1]) return Fail("selecting a drive must store the chosen drive");
    if (base.modifierA != DRIVE_INFO[base.selectedDrive].modifierA
        || base.modifierB != DRIVE_INFO[base.selectedDrive].modifierB) return Fail("selecting a drive must apply its modifiers");
    if (base.modifierA == base.modifierB) return Fail("modifiers must be unique");
    if (base.phase != PHASE_DIRECTORY) return Fail("selecting a drive must open the directory choice");
    if (!base.mobScheduleReady) return Fail("selecting a drive must build the mob schedule");
    Face damaged = {FACE_FIRE, 8, 1, 0};
    if (FaceCost(&damaged) != 24 || FacePower(&damaged) != 0) return Fail("damaged faces retain cost and lose power");
    Face quarantined = {FACE_FIRE, 8, 0, QUAR_COMBAT};
    if (FaceCost(&quarantined) != 24 || FacePower(&quarantined) != 0) return Fail("quarantined faces retain cost and lose power");
    base.floor = 2; base.modifierA = MOD_OVERALLOC; base.modifierB = MOD_CHECKSUM; base.selectedDrive = -1;
    if (EffectiveCapacity(&base) != 190) return Fail("overallocation must add 60 bytes");

    GameState capPerk; NewRun(&capPerk, 0xD01D01u);
    capPerk.driveChoices[0] = 1; // D:\ ARCHIVE - 용량 +15B, 손상에 과잉 할당 포함
    SelectDrive(&capPerk, 0);
    if (EffectiveCapacity(&capPerk) != 240 + 60 + DRIVE_INFO[1].perkValue) return Fail("capacity perk must add its bonus");
    GameState hpPerk; NewRun(&hpPerk, 0xD02D02u);
    hpPerk.driveChoices[0] = 0; // C:\ SYSTEM - 시작 최대 체력 +6
    SelectDrive(&hpPerk, 0);
    if (hpPerk.playerMaxHp != 40 + DRIVE_INFO[0].perkValue || hpPerk.playerHp != hpPerk.playerMaxHp) return Fail("hp perk must raise starting hp");

    GameState prune; NewRun(&prune, 0xCAFEBABEu); prune.floor = 2; prune.modifierA = MOD_CHECKSUM; prune.modifierB = MOD_FRAGMENTATION; prune.phase = PHASE_PRUNE;
    ConfigureDriveForTest(&prune, TEST_DRIVE, TEST_SEED, 1);
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        prune.dice[d].faces[f].kind = FACE_WILD; prune.dice[d].faces[f].value = 9; prune.dice[d].faces[f].damaged = 0;
    }
    if (DeckBytes(&prune) != 576) return Fail("all-wild test deck must be 576 bytes");
    while (DeckBytes(&prune) > EffectiveCapacity(&prune)) {
        int index = MostExpensiveFace(&prune); if (index < 0) return Fail("unable to find prune candidate"); PruneFace(&prune, index / 6, index % 6);
    }
    ConfirmPrune(&prune); if (prune.phase != PHASE_COMBAT) return Fail("valid pruned deck must continue");

    GameState burn; NewRun(&burn, 0xB0010001u); burn.modifierA = MOD_BAD_SECTOR; burn.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&burn, TEST_DRIVE, TEST_SEED, 1); StartCombat(&burn);
    burn.enemies[0].hp = 3; burn.enemies[0].burn = 1; burn.enemies[0].intent = INTENT_GUARD; burn.enemies[0].intentValue = 0;
    AssignDieToSlot(&burn, 0, SLOT_DEFEND); EndTurn(&burn);
    if (burn.phase != PHASE_REWARD || burn.combatsWon != 1) return Fail("burn killing the last enemy must end combat immediately");

    GameState corrupt; NewRun(&corrupt, 0xC0110001u); corrupt.modifierA = MOD_BAD_SECTOR; corrupt.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&corrupt, TEST_DRIVE, TEST_SEED, 1); StartCombat(&corrupt);
    SetAllFaces(&corrupt, FACE_NUMBER, 6);   // 어떤 면이 나와도 방어도는 6
    corrupt.enemies[0].hp = corrupt.enemies[0].maxHp; corrupt.enemies[0].intent = INTENT_CORRUPT; corrupt.enemies[0].intentValue = 12;
    // PlanEnemy가 다음 턴 의도를 덮어쓰기 전에 이번 턴 효과만 본다
    int hpBeforeCorrupt = corrupt.playerHp; AssignDieToSlot(&corrupt, 0, SLOT_DEFEND); EndTurn(&corrupt);
    // 방어도 6은 관통을 3만 막고, 막은 몫의 두 배가 소모된다.
    if (hpBeforeCorrupt - corrupt.playerHp != 9) return Fail("corrupt intent must only be halved by player block");
    if (corrupt.playerBlock != 0) return Fail("blocking corrupt damage must spend twice what it absorbs");

    GameState grades; NewRun(&grades, 0xD1FF0001u);
    if (grades.difficulty != -1) return Fail("difficulty must stay unset until a volume is mounted");
    for (int i = 0; i < 3; ++i) {
        if (grades.driveDifficulty[i] < 0 || grades.driveDifficulty[i] >= DIFFICULTY_COUNT)
            return Fail("each drive card must carry a valid difficulty");
    }
    if (grades.driveDifficulty[0] == grades.driveDifficulty[1] || grades.driveDifficulty[0] == grades.driveDifficulty[2]
        || grades.driveDifficulty[1] == grades.driveDifficulty[2]) return Fail("drive difficulties must be unique");
    SelectDrive(&grades, 2);
    if (grades.difficulty != grades.driveDifficulty[2]) return Fail("mounting must store the chosen card's difficulty");

    GameState scale; NewRun(&scale, 0xD1FF0002u);
    scale.difficulty = DIFF_BEGINNER;
    if (CorruptPercent(&scale) != 25) return Fail("beginner must take a quarter of the corrupt damage");
    if (ScaleCorruptDamage(&scale, 12) != 3) return Fail("corrupt damage must scale by the difficulty percent");
    if (ScaleCorruptDamage(&scale, 1) != 1) return Fail("scaled corrupt damage must never round down to zero");
    if (ScaleCorruptDamage(&scale, 0) != 0) return Fail("no corrupt damage must stay zero");
    scale.difficulty = DIFF_MADNESS;
    if (ScaleCorruptDamage(&scale, 12) != 24) return Fail("madness must double the corrupt damage");
    scale.difficulty = -1;
    if (CorruptPercent(&scale) != DIFFICULTY_BASE_PERCENT) return Fail("an unset difficulty must fall back to the base percent");

    // 같은 시드·같은 보스에서 등급만 바꿔, 예고된 오염 수치 자체가 배율되는지 본다.
    // 난이도는 난수를 건드리지 않으므로 두 런은 턴마다 같은 상황을 지난다.
    GameState baseline, easier;
    SetupBossFight(&baseline, 4, 1, 0xD1FF0003u, 3);   // R:\ HEAP.OVERFLOW - 오염을 예고하는 보스
    SetupBossFight(&easier, 4, 1, 0xD1FF0003u, 3);
    baseline.difficulty = DIFF_NIGHTMARE;   // 기준값 100%
    easier.difficulty = DIFF_BEGINNER;
    int baseCorrupt = 0, easyCorrupt = 0;
    for (int t = 0; t < 12 && !baseCorrupt; ++t) {
        AssignDieToSlot(&baseline, 0, SLOT_DEFEND); EndTurn(&baseline);
        AssignDieToSlot(&easier, 0, SLOT_DEFEND); EndTurn(&easier);
        if (baseline.phase != PHASE_COMBAT || easier.phase != PHASE_COMBAT) break;
        if (baseline.enemies[0].intent != INTENT_CORRUPT || easier.enemies[0].intent != INTENT_CORRUPT) continue;
        baseCorrupt = baseline.enemies[0].intentValue;
        easyCorrupt = easier.enemies[0].intentValue;
    }
    if (baseCorrupt <= 0 || easyCorrupt <= 0) return Fail("the pressure boss must telegraph a corrupt intent");
    if (easyCorrupt != ScaleCorruptDamage(&easier, baseCorrupt))
        return Fail("the telegraphed corrupt value must already carry the difficulty multiplier");

    // 판독: 처치한 종류만 열리고, 미리보기나 새 런으로는 열리지 않는다.
    GameState codex; NewRun(&codex, 0x5CA40001u); codex.modifierA = MOD_BAD_SECTOR; codex.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&codex, TEST_DRIVE, TEST_SEED, 1); StartCombat(&codex);
    int scanKind = codex.enemies[0].kind;
    if (IsEnemyScanned(&codex, scanKind)) return Fail("a fresh run must start with nothing scanned");
    for (int k = 0; k < ENEMY_KIND_COUNT; ++k) if (IsEnemyScanned(&codex, k)) return Fail("a fresh run must scan no enemy kind");
    if (IsEnemyScanned(&codex, -1) || IsEnemyScanned(&codex, ENEMY_KIND_COUNT)) return Fail("an out of range kind must never read as scanned");
    SetAllFaces(&codex, FACE_NUMBER, 6);
    codex.enemies[0].hp = 1; codex.enemies[0].block = 0;
    // 미리보기로 죽여 보는 것만으로는 판독되면 안 된다.
    AssignDieToSlot(&codex, 0, SLOT_ATTACK);
    TurnPreview peek; PreviewTurn(&codex, &peek);
    if (!peek.combatEnds) return Fail("the scan setup must preview a kill");
    if (IsEnemyScanned(&codex, scanKind)) return Fail("previewing a kill must not scan the enemy");
    EndTurn(&codex);
    if (!IsEnemyScanned(&codex, scanKind)) return Fail("killing an enemy must scan its kind");
    for (int k = 0; k < ENEMY_KIND_COUNT; ++k)
        if (k != scanKind && IsEnemyScanned(&codex, k)) return Fail("killing one enemy must not scan any other kind");
    NewRun(&codex, 0x5CA40001u);
    if (IsEnemyScanned(&codex, scanKind)) return Fail("a new run must clear every scan");

    // 미리보기는 원본을 한 바이트도 건드리지 않고, 실제 실행과 같은 숫자를 내야 한다.
    GameState preview; NewRun(&preview, 0x9E1E0001u); preview.modifierA = MOD_BAD_SECTOR; preview.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&preview, TEST_DRIVE, TEST_SEED, 1); StartCombat(&preview);
    SetAllFaces(&preview, FACE_NUMBER, 4);
    preview.enemies[0].hp = 99; preview.enemies[0].maxHp = 99;
    TurnPreview empty; PreviewTurn(&preview, &empty);
    if (empty.valid) return Fail("an empty placement must not produce a preview");
    AssignDieToSlot(&preview, 0, SLOT_ATTACK);
    AssignDieToSlot(&preview, 1, SLOT_DEFEND);
    GameState untouched = preview;
    TurnPreview ahead; PreviewTurn(&preview, &ahead);
    if (!ahead.valid) return Fail("placing a die must produce a preview");
    if (memcmp(&untouched, &preview, sizeof(GameState)) != 0) return Fail("preview must not mutate the game state");
    if (ahead.uncertain) return Fail("a turn with no read error must preview a certain result");
    if (ahead.slotOutput[SLOT_ATTACK] <= 0 || ahead.slotOutput[SLOT_DEFEND] <= 0)
        return Fail("preview must report the output of every filled slot");
    if (ahead.slotOutput[SLOT_CHAIN] != 0) return Fail("an empty slot must preview zero output");
    EndTurn(&preview);
    if (ahead.damageDealt != preview.lastTurnDamageDealt) return Fail("preview damage dealt must match the real turn");
    if (ahead.damageTaken != preview.lastTurnDamageTaken) return Fail("preview damage taken must match the real turn");
    if (ahead.blockGained != preview.lastTurnBlockGained) return Fail("preview block must match the real turn");
    for (int s = 0; s < SLOT_COUNT; ++s)
        if (ahead.slotOutput[s] != preview.lastTurnSlotOutput[s]) return Fail("preview slot output must match the real turn");

    // 치명타·사망 예고도 미리 보여야 한다.
    GameState lethal; NewRun(&lethal, 0x9E1E0002u); lethal.modifierA = MOD_BAD_SECTOR; lethal.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&lethal, TEST_DRIVE, TEST_SEED, 1); StartCombat(&lethal);
    SetAllFaces(&lethal, FACE_NUMBER, 6);
    lethal.enemies[0].hp = 1; lethal.enemies[0].block = 0;
    AssignDieToSlot(&lethal, 0, SLOT_ATTACK);
    TurnPreview kill; PreviewTurn(&lethal, &kill);
    if (!kill.combatEnds || kill.playerDies) return Fail("a lethal placement must preview the kill");
    if (lethal.phase != PHASE_COMBAT) return Fail("previewing a kill must not end the real combat");

    GameState doomed; NewRun(&doomed, 0x9E1E0003u); doomed.modifierA = MOD_BAD_SECTOR; doomed.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&doomed, TEST_DRIVE, TEST_SEED, 1); StartCombat(&doomed);
    SetAllFaces(&doomed, FACE_NUMBER, 1);
    doomed.enemies[0].hp = 999; doomed.enemies[0].maxHp = 999;
    doomed.enemies[0].intent = INTENT_HEAVY; doomed.enemies[0].intentValue = 500;
    doomed.playerHp = 3;
    AssignDieToSlot(&doomed, 0, SLOT_ATTACK);
    TurnPreview fatal; PreviewTurn(&doomed, &fatal);
    if (!fatal.playerDies) return Fail("a fatal placement must preview the loss");
    if (doomed.phase != PHASE_COMBAT || doomed.playerHp != 3) return Fail("previewing a loss must not kill the real run");

    // 읽기 오류가 걸린 주사위가 있으면 미리보기는 확정이 아니라고 밝혀야 한다.
    GameState shaky; NewRun(&shaky, 0x9E1E0004u); shaky.modifierA = MOD_READ_ERROR; shaky.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&shaky, TEST_DRIVE, TEST_SEED, 1); StartCombat(&shaky);
    SetAllFaces(&shaky, FACE_NUMBER, 4);
    shaky.enemies[0].hp = 99; shaky.enemies[0].maxHp = 99;
    AssignDieToSlot(&shaky, 0, SLOT_ATTACK);
    TurnPreview unsure; PreviewTurn(&shaky, &unsure);
    if (!unsure.valid) return Fail("a read-error turn must still produce a preview");
    if (!unsure.uncertain) return Fail("a read error must mark the preview as uncertain");

    // 경고된 주사위를 사본에서 다시 굴려 버리면 실제로 나올 숫자가 미리보기로 새어 나간다.
    // 재굴림은 빠지고, 흔들리는 값이 닿는 슬롯만 "모름"으로 표시되어야 한다.
    int shakyDie = -1;
    for (int d = 0; d < 3; ++d) if (shaky.dice[d].unstable) shakyDie = d;
    if (shakyDie < 0) return Fail("a read-error drive must mark one die unstable");
    for (int d = 0; d < 3; ++d) UnassignDie(&shaky, d);
    AssignDieToSlot(&shaky, shakyDie, SLOT_DEFEND);
    AssignDieToSlot(&shaky, (shakyDie + 1) % 3, SLOT_ATTACK);
    GameState shakyBefore = shaky;
    TurnPreview hidden; PreviewTurn(&shaky, &hidden);
    if (memcmp(&shakyBefore, &shaky, sizeof(GameState)) != 0) return Fail("previewing a read-error turn must not mutate the game state");
    if (shaky.dice[shakyDie].rolledFace != shakyBefore.dice[shakyDie].rolledFace) return Fail("preview must not reroll the unstable die");
    if (!hidden.uncertain) return Fail("a read error must mark the preview as uncertain");
    if (!hidden.slotUnknown[SLOT_DEFEND]) return Fail("the slot holding an unstable die must preview as unknown");
    // 체크섬은 세 주사위 출력의 합만 보므로 흔들리는 주사위가 어디 놓이든 공격이 함께 흔들린다.
    if (!hidden.slotUnknown[SLOT_ATTACK]) return Fail("a checksum run must preview the attack as unknown");
    if (hidden.slotUnknown[SLOT_AMPLIFY] || hidden.slotUnknown[SLOT_CHAIN]) return Fail("an empty slot must never preview as unknown");

    // 증폭은 뒤에 해결될 공격·방어에 보너스를 얹고, 연쇄는 그 공격을 반복한다.
    // 증폭에 놓인 불안정 주사위 하나가 세 슬롯을 전부 모르게 만들어야 한다.
    GameState spread; NewRun(&spread, 0x9E1E0005u); spread.modifierA = MOD_READ_ERROR; spread.modifierB = MOD_BAD_SECTOR;
    ConfigureDriveForTest(&spread, TEST_DRIVE, TEST_SEED, 1); StartCombat(&spread);
    SetAllFaces(&spread, FACE_NUMBER, 4);
    spread.enemies[0].hp = 99; spread.enemies[0].maxHp = 99;
    int ampDie = -1;
    for (int d = 0; d < 3; ++d) if (spread.dice[d].unstable) ampDie = d;
    if (ampDie < 0) return Fail("a read-error drive must mark one die unstable");
    for (int d = 0; d < 3; ++d) UnassignDie(&spread, d);
    AssignDieToSlot(&spread, ampDie, SLOT_AMPLIFY);
    AssignDieToSlot(&spread, (ampDie + 1) % 3, SLOT_ATTACK);
    AssignDieToSlot(&spread, (ampDie + 2) % 3, SLOT_CHAIN);
    TurnPreview chained; PreviewTurn(&spread, &chained);
    if (!chained.slotUnknown[SLOT_AMPLIFY]) return Fail("the amplify slot holding an unstable die must preview as unknown");
    if (!chained.slotUnknown[SLOT_ATTACK]) return Fail("an unstable amplify die must make the attack it boosts unknown");
    if (!chained.slotUnknown[SLOT_CHAIN]) return Fail("an unstable amplify die must make the chain repeat unknown");
    if (chained.slotUnknown[SLOT_DEFEND]) return Fail("an empty slot must never preview as unknown");

    // 오프라인 주사위는 어떤 눈이 나와도 출력 0이라, 재굴림이 걸려 있어도 슬롯 숫자는 확정이다.
    GameState pinned = spread; pinned.dice[ampDie].offline = 1;
    TurnPreview certain; PreviewTurn(&pinned, &certain);
    if (!certain.uncertain) return Fail("a read error must still mark the preview as uncertain");
    for (int s = 0; s < SLOT_COUNT; ++s)
        if (certain.slotUnknown[s]) return Fail("an offline unstable die must leave every slot certain");

    GameState badSector; NewRun(&badSector, 0xBADD5EC7u); badSector.phase = PHASE_REWARD;
    ConfigureDriveForTest(&badSector, TEST_DRIVE, TEST_SEED, 1);
    badSector.dice[0].faces[0].kind = FACE_FIRE; badSector.dice[0].faces[0].value = 8; badSector.dice[0].faces[0].damaged = 1;
    badSector.rewardKinds[0] = FACE_SHIELD; badSector.rewardValues[0] = FACE_INFO[FACE_SHIELD].power;
    SelectReward(&badSector, 0); InstallSelectedReward(&badSector, 0, 0);
    if (!badSector.dice[0].faces[0].damaged) return Fail("installing a reward onto a bad sector must not repair it");
    if (badSector.dice[0].faces[0].kind != FACE_SHIELD) return Fail("installing a reward onto a bad sector must still swap the face kind");

    GameState repair; NewRun(&repair, 0x5EC70001u); repair.driveChoices[0] = 0; SelectDrive(&repair, 0);
    repair.phase = PHASE_REWARD; repair.floor = 0; repair.encounter = 0; repair.playerHp = 12;
    int repairHeal = SectorRepairAmount(&repair);
    if (repairHeal != SECTOR_REPAIR_HEAL[0]) return Fail("repair amount must follow the floor table");
    RepairSector(&repair);
    if (repair.playerHp != 12 + repairHeal) return Fail("sector repair must restore hp");
    if (repair.sectorsRepaired != 1) return Fail("sector repair must be counted");
    if (repair.facesInstalled != 0) return Fail("sector repair must not install a face");
    if (repair.phase != PHASE_DIRECTORY || repair.encounter != 1) return Fail("sector repair must advance the run");

    GameState repairCap; NewRun(&repairCap, 0x5EC70002u); repairCap.driveChoices[0] = 0; SelectDrive(&repairCap, 0);
    repairCap.phase = PHASE_REWARD; repairCap.floor = 2; repairCap.playerHp = repairCap.playerMaxHp - 2;
    RepairSector(&repairCap);
    if (repairCap.playerHp != repairCap.playerMaxHp) return Fail("sector repair must not overheal");

    GameState repairFull; NewRun(&repairFull, 0x5EC70003u); repairFull.driveChoices[0] = 0; SelectDrive(&repairFull, 0);
    repairFull.phase = PHASE_REWARD; repairFull.playerHp = repairFull.playerMaxHp;
    RepairSector(&repairFull);
    if (repairFull.phase != PHASE_REWARD || repairFull.sectorsRepaired != 0) return Fail("sector repair at full hp must be rejected");

    int fragmentationShown = 0;
    for (unsigned int seed = 1; seed <= 256 && !fragmentationShown; ++seed) {
        GameState fragmented; NewRun(&fragmented, seed); fragmented.modifierA = MOD_FRAGMENTATION; fragmented.modifierB = MOD_CHECKSUM;
        ConfigureDriveForTest(&fragmented, TEST_DRIVE, seed, 1); StartCombat(&fragmented);
        for (int d = 0; d < 3; ++d) if (fragmented.dice[d].disabled) fragmentationShown = 1;
    }
    if (!fragmentationShown) return Fail("fragmentation must be marked before the player executes the turn");

    GameState firstFloorCap; NewRun(&firstFloorCap, 0xCA900001u); firstFloorCap.modifierA = MOD_BAD_SECTOR; firstFloorCap.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&firstFloorCap, TEST_DRIVE, TEST_SEED, 1);
    firstFloorCap.phase = PHASE_REWARD; firstFloorCap.floor = 0; firstFloorCap.encounter = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        firstFloorCap.dice[d].faces[f].kind = FACE_WILD; firstFloorCap.dice[d].faces[f].value = 9; firstFloorCap.dice[d].faces[f].damaged = 0;
    }
    SkipReward(&firstFloorCap);
    if (firstFloorCap.phase != PHASE_PRUNE || firstFloorCap.floor != 0 || firstFloorCap.encounter != 0) return Fail("floor 1 capacity must be enforced before advancing");
    while (DeckBytes(&firstFloorCap) > EffectiveCapacity(&firstFloorCap)) {
        int index = MostExpensiveFace(&firstFloorCap); if (index < 0) return Fail("unable to prune floor 1 test deck"); PruneFace(&firstFloorCap, index / 6, index % 6);
    }
    if (firstFloorCap.pendingContinuation != CONTINUE_AFTER_REWARD) return Fail("a reward-time prune must resume the reward flow");
    ConfirmPrune(&firstFloorCap);
    if (firstFloorCap.phase != PHASE_DIRECTORY || firstFloorCap.floor != 0 || firstFloorCap.encounter != 1) return Fail("floor 1 prune must resume at the next encounter");

    GameState tsr; NewRun(&tsr, 0x75720001u); tsr.modifierA = MOD_BAD_SECTOR; tsr.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&tsr, TEST_DRIVE, TEST_SEED, 1);
    tsr.encounter = 2; StartCombat(&tsr);
    tsr.playerHp = 999; tsr.playerMaxHp = 999; tsr.enemies[0].hp = 1;
    AssignDieToSlot(&tsr, 0, SLOT_ATTACK); EndTurn(&tsr);
    if (tsr.phase != PHASE_REWARD || !tsr.rewardIsTsr) return Fail("boss kill must offer resident program loot");
    for (int i = 0; i < 3; ++i) {
        int kind = tsr.rewardKinds[i];
        if (kind < 0 || kind >= TSR_COUNT) return Fail("tsr loot must be a valid program");
        if (TSR_INFO[kind].counters >= 0 && !IsModifierActive(&tsr, TSR_INFO[kind].counters)) return Fail("counter tsr must not appear without its modifier");
        for (int j = 0; j < i; ++j) if (tsr.rewardKinds[j] == kind) return Fail("tsr loot must be distinct");
    }
    int lootKind = tsr.rewardKinds[1], usedBeforeLoot = UsedBytes(&tsr);
    InstallTsr(&tsr, 1);
    if (!IsTsrInstalled(&tsr, lootKind) || tsr.tsrsInstalled != 1) return Fail("installing loot must register the program");
    if (UsedBytes(&tsr) != usedBeforeLoot + TSR_INFO[lootKind].cost) return Fail("resident programs must consume capacity");
    if (tsr.floor != 1 || tsr.phase != PHASE_DIRECTORY) return Fail("boss loot must advance to the next floor");

    GameState himem; NewRun(&himem, 0x75720002u); himem.modifierA = MOD_BAD_SECTOR; himem.modifierB = MOD_CHECKSUM;
    himem.tsrInstalled[TSR_HIMEM] = 1;
    if (EffectiveCapacity(&himem) != 240 + TSR_INFO[TSR_HIMEM].value) return Fail("himem must extend the capacity");
    if (UsedBytes(&himem) != 63 + TSR_INFO[TSR_HIMEM].cost) return Fail("used bytes must include resident programs");

    GameState smart; NewRun(&smart, 0x75720003u); smart.modifierA = MOD_BAD_SECTOR; smart.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&smart, TEST_DRIVE, TEST_SEED, 1);
    smart.tsrInstalled[TSR_SMARTDRV] = 1; StartCombat(&smart);
    if (smart.playerBlock != TSR_INFO[TSR_SMARTDRV].value) return Fail("smartdrv must grant first-turn block");
    smart.enemies[0].hp = 999; smart.enemies[0].maxHp = 999; smart.enemies[0].intent = INTENT_GUARD; smart.enemies[0].intentValue = 0;
    AssignDieToSlot(&smart, 0, SLOT_ATTACK); EndTurn(&smart);
    if (smart.turn != 2 || smart.playerBlock != 0) return Fail("smartdrv block must last only the first turn");

    for (unsigned int seed = 1; seed <= 64; ++seed) {
        GameState defrag; NewRun(&defrag, seed); defrag.modifierA = MOD_FRAGMENTATION; defrag.modifierB = MOD_CHECKSUM;
        ConfigureDriveForTest(&defrag, TEST_DRIVE, seed, 1);
        defrag.tsrInstalled[TSR_DEFRAG] = 1; StartCombat(&defrag);
        for (int d = 0; d < 3; ++d) if (defrag.dice[d].disabled) return Fail("defrag must suppress fragmentation");
    }

    GameState scan; NewRun(&scan, 0x75720004u); scan.modifierA = MOD_BAD_SECTOR; scan.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&scan, TEST_DRIVE, TEST_SEED, 1);
    scan.tsrInstalled[TSR_SCANDISK] = 1; scan.phase = PHASE_REWARD; scan.encounter = 2;
    SkipReward(&scan);
    int scanDamaged = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (scan.dice[d].faces[f].damaged) ++scanDamaged;
    if (scan.floor != 1 || scanDamaged != 0) return Fail("scandisk must block descent damage");
    GameState noScan; NewRun(&noScan, 0x75720004u); noScan.modifierA = MOD_BAD_SECTOR; noScan.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&noScan, TEST_DRIVE, TEST_SEED, 1);
    noScan.phase = PHASE_REWARD; noScan.encounter = 2;
    SkipReward(&noScan);
    int rawDamaged = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (noScan.dice[d].faces[f].damaged) ++rawDamaged;
    if (rawDamaged != 1) return Fail("bad sector descent must damage one face without scandisk");

    GameState undel; NewRun(&undel, 0x75720005u); undel.modifierA = MOD_BAD_SECTOR; undel.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&undel, TEST_DRIVE, TEST_SEED, 1);
    undel.tsrInstalled[TSR_UNDELETE] = 1; StartCombat(&undel);
    undel.playerHp = 20; undel.enemies[0].hp = 1;
    AssignDieToSlot(&undel, 0, SLOT_ATTACK); EndTurn(&undel);
    if (undel.playerHp != 20 + TSR_INFO[TSR_UNDELETE].value) return Fail("undelete must heal after a win");

    GameState keyb; NewRun(&keyb, 0x75720006u); keyb.modifierA = MOD_BAD_SECTOR; keyb.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&keyb, TEST_DRIVE, TEST_SEED, 1);
    keyb.tsrInstalled[TSR_KEYB] = 1; StartCombat(&keyb);
    KeybReroll(&keyb, 0);
    if (!keyb.keybUsedThisTurn) return Fail("keyb reroll must consume the turn charge");
    keyb.dice[1].rolledFace = 2;
    KeybReroll(&keyb, 1);
    if (keyb.dice[1].rolledFace != 2) return Fail("keyb must reroll only once per turn");
    keyb.enemies[0].hp = 999; keyb.enemies[0].maxHp = 999; keyb.enemies[0].intent = INTENT_GUARD; keyb.enemies[0].intentValue = 0;
    AssignDieToSlot(&keyb, 0, SLOT_ATTACK); EndTurn(&keyb);
    if (keyb.phase != PHASE_COMBAT || keyb.keybUsedThisTurn) return Fail("keyb charge must reset each turn");
    GameState noKeyb; NewRun(&noKeyb, 0x75720007u); noKeyb.modifierA = MOD_BAD_SECTOR; noKeyb.modifierB = MOD_CHECKSUM;
    ConfigureDriveForTest(&noKeyb, TEST_DRIVE, TEST_SEED, 1); StartCombat(&noKeyb);
    KeybReroll(&noKeyb, 0);
    if (noKeyb.keybUsedThisTurn) return Fail("keyb reroll requires the resident program");

    GameState unin; NewRun(&unin, 0x75720008u); unin.modifierA = MOD_BAD_SECTOR; unin.modifierB = MOD_CHECKSUM;
    unin.tsrInstalled[TSR_SMARTDRV] = 1; unin.phase = PHASE_PRUNE;
    int usedBeforeUninstall = UsedBytes(&unin);
    UninstallTsr(&unin, TSR_SMARTDRV);
    if (IsTsrInstalled(&unin, TSR_SMARTDRV) || UsedBytes(&unin) != usedBeforeUninstall - TSR_INFO[TSR_SMARTDRV].cost)
        return Fail("uninstall must free resident bytes");
    unin.phase = PHASE_COMBAT; unin.tsrInstalled[TSR_KEYB] = 1;
    UninstallTsr(&unin, TSR_KEYB);
    if (!IsTsrInstalled(&unin, TSR_KEYB)) return Fail("uninstall must only work on the prune screen");

    int runs = 0;
    // 모든 손상 조합 × 시드 (드라이브 0 로스터 사용, 손상 직접 주입)
    for (int a = 0; a < MODIFIER_COUNT; ++a) for (int b = a + 1; b < MODIFIER_COUNT; ++b) for (int seed = 0; seed < 3; ++seed) {
        int result = RunCompleteGame(TEST_DRIVE, a, b, 1, 0x10203040u + (unsigned int)(a * 101 + b * 17 + seed));
        if (result != 0) { printf("FAIL: modifier pair %d/%d seed %d result %d\n", a, b, seed, result); return 1; }
        ++runs;
    }
    // 6개 드라이브 각각의 고유 손상·로스터로 전체 런
    for (int drive = 0; drive < DRIVE_COUNT; ++drive) for (int seed = 0; seed < 3; ++seed) {
        int result = RunCompleteGame(drive, 0, 0, 0, 0x50607080u + (unsigned int)(drive * 131 + seed));
        if (result != 0) { printf("FAIL: drive %d seed %d result %d\n", drive, seed, result); return 1; }
        ++runs;
    }
    printf("PASS: roster, sprites, spawn matrix, 18 gimmick scenarios, directory routing, %d complete runs\n", runs); return 0;
}
