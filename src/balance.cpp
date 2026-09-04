#include <stdio.h>
#include <stdlib.h>
#include "game.h"

static int WeakestFace(const GameState* game) {
    int best = 0, scoreBest = 100000;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        const Face* face = &game->dice[d].faces[f]; int score = FacePower(face) * 10 - FaceCost(face); if (face->damaged) score -= 100;
        if (score < scoreBest) { scoreBest = score; best = d * 6 + f; }
    }
    return best;
}

static int WorstEfficiencyFace(const GameState* game) {
    int best = -1, scoreBest = -100000;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        const Face* face = &game->dice[d].faces[f]; if (face->kind == FACE_EMPTY) continue;
        int score = FaceCost(face) * 4 - FacePower(face) * 5; if (face->damaged) score += 200;
        if (score > scoreBest) { scoreBest = score; best = d * 6 + f; }
    }
    return best;
}

static int BestReward(const GameState* game) {
    int best = 0, scoreBest = -100000;
    // 보상 후보 수는 디렉터리 노드가 줄일 수 있다 (TEMP면 2개).
    int count = game->rewardChoiceCount > 0 && game->rewardChoiceCount <= 3 ? game->rewardChoiceCount : 3;
    for (int i = 0; i < count; ++i) {
        int kind = game->rewardKinds[i], cost = kind == FACE_NUMBER ? game->rewardValues[i] : FACE_INFO[kind].cost;
        int score = game->rewardValues[i] * 10 - cost; if (kind == FACE_WILD || kind == FACE_ECHO) score += 10;
        if (score > scoreBest) { scoreBest = score; best = i; }
    }
    return best;
}

// 오프라인·조각화 주사위는 실제 출력이 0이다.
static int EffectiveDiePower(const GameState* game, int die) {
    if (game->dice[die].disabled || game->dice[die].offline) return 0;
    int power = FacePower(RolledFace(game, die));
    if (game->selectedDrive == 5 && game->driveRule.contrabandDie == die
        && game->driveRule.contrabandFace == game->dice[die].rolledFace && power > 0) power += 2;
    return power;
}

// 기믹 인지 배치: 잠긴 슬롯을 피하고, 역전 턴에는 공격·방어를 우선하며,
// 게이지·복원 압박이 있으면 최고 출력 주사위를 공격에 몰아준다.
static void AssignDice(GameState* game) {
    int order[3] = {0, 1, 2};
    for (int i = 0; i < 3; ++i) for (int j = i + 1; j < 3; ++j)
        if (EffectiveDiePower(game, order[j]) > EffectiveDiePower(game, order[i])) { int swap = order[i]; order[i] = order[j]; order[j] = swap; }
    // D: 두 번째 턴부터 가장 낮은 번호의 반복 배치만 보정되므로 die 0을 공격에 고정한다.
    if (game->selectedDrive == 1 && game->turn > 1) {
        int at = 0; while (at < 3 && order[at] != 0) ++at;
        if (at < 3) { int swap = order[0]; order[0] = order[at]; order[at] = swap; }
    }
    int reversed = ResolveOrderReversed(game);
    // 선호 슬롯: 최강 주사위 → 공격, 다음 → 방어, 남는 것 → 증폭(정상 턴)
    int prefs[3];
    prefs[0] = SLOT_ATTACK;
    int ramAggressive = game->selectedDrive == 4 && game->playerHp * 2 > game->playerMaxHp;
    prefs[1] = ramAggressive ? SLOT_AMPLIFY : SLOT_DEFEND;
    prefs[2] = game->selectedDrive == 3 || ramAggressive || reversed ? SLOT_CHAIN : SLOT_AMPLIFY;
    // 실험용: AROGUE_THIRD=chain|amplify|defend 로 세 번째 주사위의 슬롯을 전 드라이브에 강제한다.
    // 어떤 슬롯이 남는 주사위에 더 값어치 있는지 승률로 비교하기 위한 것이다. 게이트에는 쓰지 않는다.
    static int forced = -2;
    if (forced == -2) {
        const char* e = getenv("AROGUE_THIRD"); forced = -1;
        if (e && e[0] == 'c') forced = SLOT_CHAIN; else if (e && e[0] == 'a') forced = SLOT_AMPLIFY; else if (e && e[0] == 'd') forced = SLOT_DEFEND;
    }
    if (forced >= 0) { prefs[1] = forced == SLOT_DEFEND ? SLOT_AMPLIFY : SLOT_DEFEND; prefs[2] = forced; }
    int used[SLOT_COUNT] = {};
    for (int i = 0; i < 3; ++i) {
        int die = order[i];
        int placed = 0;
        for (int p = 0; p < 3 && !placed; ++p) {
            int slot = prefs[p];
            if (used[slot] || SlotLockedThisTurn(game, slot)) continue;
            if (AssignDieToSlot(game, die, slot)) { used[slot] = 1; placed = 1; }
        }
        if (!placed) {
            for (int slot = 0; slot < SLOT_COUNT && !placed; ++slot) {
                if (used[slot] || SlotLockedThisTurn(game, slot)) continue;
                if (AssignDieToSlot(game, die, slot)) { used[slot] = 1; placed = 1; }
            }
        }
    }
    // E: 낮은 눈은 남는 슬롯으로 한 번 옮겨 HOT SWAP을 실제 사용한다.
    if (game->selectedDrive == 2 && !game->driveRule.hotSwapUsed) {
        int weakest = order[2], freeSlot = -1;
        for (int s = 0; s < SLOT_COUNT; ++s) if (!used[s] && !SlotLockedThisTurn(game, s)) { freeSlot = s; break; }
        int oldSlot = game->dice[weakest].assignedSlot;
        if (freeSlot >= 0 && oldSlot >= 0 && EffectiveDiePower(game, weakest) <= 2) {
            AssignDieToSlot(game, weakest, freeSlot);
            AssignDieToSlot(game, weakest, oldSlot);
        }
    }
}

// ---------------------------------------------------------------------------
// 디렉터리 선택 휴리스틱과 통계
//
// 상태를 보고 고르는 플레이어를 흉내 낸다. 체력이 넉넉하면 위험 노드로 성장을
// 사고, 체력이 낮으면 회복을, 용량이 조이면 임시 한도를 산다. 사람과 같지는
// 않지만 "한 선택지가 언제나 정답"인 상태를 잡아내기에는 충분하다.
// ---------------------------------------------------------------------------

static int gOffered[DIR_NODE_COUNT];
static int gChosen[DIR_NODE_COUNT];
static int gChosenWins[DIR_NODE_COUNT];
static int gPairOffered[DIR_NODE_COUNT][DIR_NODE_COUNT];
static int gPairFirstChosen[DIR_NODE_COUNT][DIR_NODE_COUNT];
static long gFloorHp[3];
static int gFloorHpSamples[3];
static long gFloorBytes[3];
static int gFloorUsable[3];
static int gPruneCount;
static unsigned long gLawActivations[DRIVE_COUNT], gHotSwaps[DRIVE_COUNT], gPacketChains[DRIVE_COUNT], gContrabandUses[DRIVE_COUNT];
static unsigned long gTurns[DRIVE_COUNT], gDamage[DRIVE_COUNT], gBlock[DRIVE_COUNT], gSlotChosen[DRIVE_COUNT][SLOT_COUNT];

// 덱의 평균 출력. 위험 노드를 감당할 화력이 있는지 재는 대용치다.
static int AverageFacePower(const GameState* game) {
    int total = 0, count = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        const Face* face = &game->dice[d].faces[f];
        if (face->kind == FACE_EMPTY) continue;
        total += FacePower(face); ++count;
    }
    return count > 0 ? total / count : 0;
}

static int DirectoryScore(const GameState* game, int kind) {
    int hpPercent = game->playerMaxHp > 0 ? game->playerHp * 100 / game->playerMaxHp : 100;
    int capacity = EffectiveCapacity(game);
    int pressure = capacity > 0 ? UsedBytes(game) * 100 / capacity : 0;
    int power = AverageFacePower(game);
    int usable = UsableFaceCount(game);
    switch (kind) {
    case DIR_NODE_TEMP:      return hpPercent < 70 ? 58 + (70 - hpPercent) / 2 : 18;
    case DIR_NODE_CACHE:     return pressure > 85 ? 52 + (pressure - 85) : 24;
    // 보스가 무거워질수록 정보의 값이 오른다.
    case DIR_NODE_LOGS:      return 32 + game->floor * 4;
    // 위험 노드는 체력만이 아니라 화력이 받쳐 줄 때만 산다.
    case DIR_NODE_INFECTED:  return hpPercent >= 70 ? 20 + power * 4 : 10;
    case DIR_NODE_CORRUPTED: return hpPercent >= 70 && usable >= 12 ? 16 + power * 4 : 8;
    default:                 return 35;   // PROCESS: 언제나 이해 가능한 기준선
    }
}

// 선택과 통계 수집. runChosen은 이 런에서 고른 노드 표시(승률 집계용)다.
static void ChooseDirectory(GameState* game, int* runChosen) {
    int count = DirectoryChoiceCount(game);
    if (count <= 0) { SelectDirectoryChoice(game, 0); return; }
    int a = game->directory.choices[0].kind;
    int b = count > 1 ? game->directory.choices[1].kind : a;
    int scoreA = DirectoryScore(game, a), scoreB = DirectoryScore(game, b);
    int pick = scoreB > scoreA ? 1 : 0;
    int kind = pick == 0 ? a : b;

    int floor = game->floor < 0 ? 0 : (game->floor > 2 ? 2 : game->floor);
    gFloorHp[floor] += game->playerHp * 100 / (game->playerMaxHp > 0 ? game->playerMaxHp : 1);
    gFloorBytes[floor] += UsedBytes(game);
    gFloorUsable[floor] += UsableFaceCount(game);
    ++gFloorHpSamples[floor];

    ++gOffered[a];
    if (count > 1) ++gOffered[b];
    int low = a < b ? a : b, high = a < b ? b : a;
    ++gPairOffered[low][high];
    if (kind == low) ++gPairFirstChosen[low][high];
    ++gChosen[kind];
    runChosen[kind] = 1;

    SelectDirectoryChoice(game, pick);
}

static int Run(int drive, unsigned int seed, int* combats, int* difficulty) {
    GameState game; NewRun(&game, seed);
    game.driveChoices[0] = drive;   // 검사 대상 드라이브를 강제로 첫 카드에 놓는다
    *difficulty = game.driveDifficulty[0];
    int runChosen[DIR_NODE_COUNT] = {};
    int steps = 0;
    while (game.phase != PHASE_VICTORY && game.phase != PHASE_GAMEOVER && steps++ < 600) {
        if (game.phase == PHASE_DRIVE_SELECT) {
            SelectDrive(&game, 0);
        } else if (game.phase == PHASE_DIRECTORY) {
            ChooseDirectory(&game, runChosen);
        } else if (game.phase == PHASE_COMBAT) {
            int target = -1, hp = 100000;
            for (int i = 0; i < game.enemyCount; ++i) if (game.enemies[i].alive && game.enemies[i].hp < hp) { target = i; hp = game.enemies[i].hp; }
            if (target >= 0) SelectEnemy(&game, target);
            AssignDice(&game);
            for (int d = 0; d < 3; ++d) if (game.dice[d].assignedSlot >= 0) ++gSlotChosen[drive][game.dice[d].assignedSlot];
            EndTurn(&game);
            ++gTurns[drive]; gDamage[drive] += (unsigned long)game.lastTurnDamageDealt; gBlock[drive] += (unsigned long)game.lastTurnBlockGained;
        } else if (game.phase == PHASE_REWARD) {
            // 체력이 절반 아래로 떨어지면 덱 강화를 한 번 포기하고 회복한다.
            if (game.playerHp * 100 < game.playerMaxHp * 55) RepairSector(&game);
            else if (game.rewardIsTsr) InstallTsr(&game, 0);
            else { int reward = BestReward(&game), face = WeakestFace(&game); SelectReward(&game, reward); InstallSelectedReward(&game, face / 6, face % 6); }
        } else if (game.phase == PHASE_PRUNE) {
            ++gPruneCount;
            while (UsedBytes(&game) > EffectiveCapacity(&game)) { int face = WorstEfficiencyFace(&game); if (face < 0) break; PruneFace(&game, face / 6, face % 6); }
            ConfirmPrune(&game);
        } else if (game.phase == PHASE_STORY) {
            AdvanceStory(&game);
        } else if (game.phase == PHASE_ENDING_CHOICE) {
            SelectEnding(&game, seed & 1u);
        }
    }
    gLawActivations[drive] += game.driveRule.activations;
    gHotSwaps[drive] += game.driveRule.hotSwapCount;
    gPacketChains[drive] += game.driveRule.packetChainCount;
    gContrabandUses[drive] += game.driveRule.contrabandUses;
    *combats = game.combatsWon;
    int won = game.phase == PHASE_VICTORY;
    if (won) for (int k = 0; k < DIR_NODE_COUNT; ++k) if (runChosen[k]) ++gChosenWins[k];
    return won;
}

int main() {
    const int runsPerDrive = 300;
    int totalWins = 0, failed = 0;
    double totalAvg = 0.0;
    // 난이도는 카드마다 무작위로 붙으므로 등급별 표본이 저절로 쌓인다.
    int gradeRuns[DIFFICULTY_COUNT] = {}, gradeWins[DIFFICULTY_COUNT] = {};
    int driveWins[DRIVE_COUNT] = {};
    printf("BALANCE per-drive (%d seeds each)\n", runsPerDrive);
    for (int drive = 0; drive < DRIVE_COUNT; ++drive) {
        int wins = 0, totalCombats = 0;
        int bossReached[3] = {}, bossKilled[3] = {};
        for (int i = 0; i < runsPerDrive; ++i) {
            int combats = 0, difficulty = -1;
            int won = Run(drive, 0x77110000u + (unsigned int)(drive * 7717 + i * 7919), &combats, &difficulty);
            wins += won;
            if (difficulty >= 0 && difficulty < DIFFICULTY_COUNT) { ++gradeRuns[difficulty]; gradeWins[difficulty] += won; }
            totalCombats += combats;
            // 보스 도달·처치는 승리한 전투 수에서 직접 유도된다.
            for (int f = 0; f < 3; ++f) {
                if (combats >= f * 3 + 2) ++bossReached[f];
                if (combats >= f * 3 + 3) ++bossKilled[f];
            }
        }
        double avg = (double)totalCombats / runsPerDrive;
        totalAvg += avg;
        printf("  drive %d: wins %d/%d, avg combats %.2f, boss reach %d/%d/%d, boss kill %d/%d/%d\n",
            drive, wins, runsPerDrive, avg,
            bossReached[0], bossReached[1], bossReached[2],
            bossKilled[0], bossKilled[1], bossKilled[2]);
        totalWins += wins;
        driveWins[drive] = wins;
        // 게이트: 어떤 드라이브도 0승이면 안 되고, 1층 보스 도달률이 바닥이면 실패
        if (wins < runsPerDrive * 20 / 100 || wins > runsPerDrive * 80 / 100) { printf("  GATE FAIL: drive %d must stay within 20-80%% wins\n", drive); failed = 1; }
        if (bossReached[0] < runsPerDrive / 10) { printf("  GATE FAIL: drive %d rarely reaches boss 1\n", drive); failed = 1; }
        printf("    turns %lu, avg damage %.2f, avg block %.2f, law %lu, hot swap %lu, packet %lu, contraband %lu\n",
            gTurns[drive], gTurns[drive] ? (double)gDamage[drive] / gTurns[drive] : 0.0,
            gTurns[drive] ? (double)gBlock[drive] / gTurns[drive] : 0.0,
            gLawActivations[drive], gHotSwaps[drive], gPacketChains[drive], gContrabandUses[drive]);
        printf("    slots attack %lu defend %lu amplify %lu chain %lu\n", gSlotChosen[drive][0], gSlotChosen[drive][1], gSlotChosen[drive][2], gSlotChosen[drive][3]);
        if (gLawActivations[drive] == 0) { printf("  GATE FAIL: drive %d law never activates\n", drive); failed = 1; }
    }
    int minWins = driveWins[0], maxWins = driveWins[0];
    for (int d = 1; d < DRIVE_COUNT; ++d) { if (driveWins[d] < minWins) minWins = driveWins[d]; if (driveWins[d] > maxWins) maxWins = driveWins[d]; }
    if (maxWins - minWins > runsPerDrive * 30 / 100) { printf("  GATE FAIL: drive win spread is %d points (limit %d)\n", maxWins - minWins, runsPerDrive * 30 / 100); failed = 1; }
    for (int g = 0; g < DIFFICULTY_COUNT; ++g) {
        printf("  difficulty %d (corrupt %d%%): wins %d/%d\n",
            g, DIFFICULTY_INFO[g].corruptPercent, gradeWins[g], gradeRuns[g]);
        if (gradeRuns[g] > 0 && gradeWins[g] == 0) { printf("  GATE FAIL: difficulty %d has zero wins\n", g); failed = 1; }
    }

    printf("DIRECTORY node usage (offered / chosen / pick rate / win rate when chosen)\n");
    int riskyOffered = 0, riskyChosen = 0;
    for (int k = DIR_NODE_PROCESS; k < DIR_NODE_COUNT; ++k) {
        if (!DIRECTORY_NODE_INFO[k].enabled) continue;
        int offered = gOffered[k], chosen = gChosen[k];
        printf("  %-10ls offered %6d  chosen %6d  pick %3d%%  win %3d%%\n",
            DIRECTORY_NODE_INFO[k].name, offered, chosen,
            offered > 0 ? chosen * 100 / offered : 0,
            chosen > 0 ? gChosenWins[k] * 100 / chosen : 0);
        if (offered == 0) { printf("  GATE FAIL: node %ls is never offered\n", DIRECTORY_NODE_INFO[k].name); failed = 1; }
        if (k == DIR_NODE_INFECTED || k == DIR_NODE_CORRUPTED) { riskyOffered += offered; riskyChosen += chosen; }
        // 성공 기준: 제시될 때 70% 이상 고정 선택되면 사실상 선택지가 아니다.
        if (offered >= 100 && chosen * 100 / offered >= 70)
            printf("  WARN: %ls is taken %d%% of the time it appears\n", DIRECTORY_NODE_INFO[k].name, chosen * 100 / offered);
    }
    if (riskyOffered > 0) {
        int riskyRate = riskyChosen * 100 / riskyOffered;
        printf("  risky (INFECTED+CORRUPTED) pick rate: %d%%\n", riskyRate);
        // 성공 기준: 위험 노드가 제시될 때 합산 20% 이상은 선택돼야 한다.
        if (riskyRate < 20) printf("  WARN: risky directories are taken only %d%% of the time\n", riskyRate);
    }

    printf("DIRECTORY pairs (offered / first-card share)\n");
    for (int a = DIR_NODE_PROCESS; a < DIR_NODE_COUNT; ++a) {
        for (int b = a + 1; b < DIR_NODE_COUNT; ++b) {
            if (gPairOffered[a][b] == 0) continue;
            printf("  %-10ls vs %-10ls  %5d offers  %3d%% took the first\n",
                DIRECTORY_NODE_INFO[a].name, DIRECTORY_NODE_INFO[b].name,
                gPairOffered[a][b], gPairFirstChosen[a][b] * 100 / gPairOffered[a][b]);
        }
    }

    printf("DIRECTORY floor state at each choice (average)\n");
    for (int f = 0; f < 3; ++f) {
        if (gFloorHpSamples[f] == 0) continue;
        printf("  floor %d: samples %5d  hp %3ld%%  used %4ldB  usable faces %2d\n",
            f + 1, gFloorHpSamples[f], gFloorHp[f] / gFloorHpSamples[f],
            gFloorBytes[f] / gFloorHpSamples[f], gFloorUsable[f] / gFloorHpSamples[f]);
    }
    printf("  prune screens entered: %d\n", gPruneCount);

    printf("BALANCE: %d/%d total heuristic wins, %.2f average combats\n",
        totalWins, runsPerDrive * DRIVE_COUNT, totalAvg / DRIVE_COUNT);
    return failed;
}
