#include <stdio.h>
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
    for (int i = 0; i < 3; ++i) {
        int kind = game->rewardKinds[i], cost = kind == FACE_NUMBER ? game->rewardValues[i] : FACE_INFO[kind].cost;
        int score = game->rewardValues[i] * 10 - cost; if (kind == FACE_WILD || kind == FACE_ECHO) score += 10;
        if (score > scoreBest) { scoreBest = score; best = i; }
    }
    return best;
}

// 오프라인·조각화 주사위는 실제 출력이 0이다.
static int EffectiveDiePower(const GameState* game, int die) {
    if (game->dice[die].disabled || game->dice[die].offline) return 0;
    return FacePower(RolledFace(game, die));
}

// 기믹 인지 배치: 잠긴 슬롯을 피하고, 역전 턴에는 공격·방어를 우선하며,
// 게이지·복원 압박이 있으면 최고 출력 주사위를 공격에 몰아준다.
static void AssignDice(GameState* game) {
    int order[3] = {0, 1, 2};
    for (int i = 0; i < 3; ++i) for (int j = i + 1; j < 3; ++j)
        if (EffectiveDiePower(game, order[j]) > EffectiveDiePower(game, order[i])) { int swap = order[i]; order[i] = order[j]; order[j] = swap; }
    int reversed = ResolveOrderReversed(game);
    // 선호 슬롯: 최강 주사위 → 공격, 다음 → 방어, 남는 것 → 증폭(정상 턴)
    int prefs[3];
    prefs[0] = SLOT_ATTACK; prefs[1] = SLOT_DEFEND; prefs[2] = reversed ? SLOT_CHAIN : SLOT_AMPLIFY;
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
}

static int Run(int drive, unsigned int seed, int* combats) {
    GameState game; NewRun(&game, seed);
    game.driveChoices[0] = drive;   // 검사 대상 드라이브를 강제로 첫 카드에 놓는다
    int steps = 0;
    while (game.phase != PHASE_VICTORY && game.phase != PHASE_GAMEOVER && steps++ < 600) {
        if (game.phase == PHASE_DRIVE_SELECT) {
            SelectDrive(&game, 0);
        } else if (game.phase == PHASE_COMBAT) {
            int target = -1, hp = 100000;
            for (int i = 0; i < game.enemyCount; ++i) if (game.enemies[i].alive && game.enemies[i].hp < hp) { target = i; hp = game.enemies[i].hp; }
            if (target >= 0) SelectEnemy(&game, target);
            AssignDice(&game);
            EndTurn(&game);
        } else if (game.phase == PHASE_REWARD) {
            // 체력이 절반 아래로 떨어지면 덱 강화를 한 번 포기하고 회복한다.
            if (game.playerHp * 100 < game.playerMaxHp * 55) RepairSector(&game);
            else if (game.rewardIsTsr) InstallTsr(&game, 0);
            else { int reward = BestReward(&game), face = WeakestFace(&game); SelectReward(&game, reward); InstallSelectedReward(&game, face / 6, face % 6); }
        } else if (game.phase == PHASE_PRUNE) {
            while (UsedBytes(&game) > EffectiveCapacity(&game)) { int face = WorstEfficiencyFace(&game); if (face < 0) break; PruneFace(&game, face / 6, face % 6); }
            ConfirmPrune(&game);
        }
    }
    *combats = game.combatsWon;
    return game.phase == PHASE_VICTORY;
}

int main() {
    const int runsPerDrive = 300;
    int totalWins = 0, failed = 0;
    double totalAvg = 0.0;
    printf("BALANCE per-drive (%d seeds each)\n", runsPerDrive);
    for (int drive = 0; drive < DRIVE_COUNT; ++drive) {
        int wins = 0, totalCombats = 0;
        int bossReached[3] = {}, bossKilled[3] = {};
        for (int i = 0; i < runsPerDrive; ++i) {
            int combats = 0;
            wins += Run(drive, 0x77110000u + (unsigned int)(drive * 7717 + i * 7919), &combats);
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
        // 게이트: 어떤 드라이브도 0승이면 안 되고, 1층 보스 도달률이 바닥이면 실패
        if (wins == 0) { printf("  GATE FAIL: drive %d has zero wins\n", drive); failed = 1; }
        if (bossReached[0] < runsPerDrive / 10) { printf("  GATE FAIL: drive %d rarely reaches boss 1\n", drive); failed = 1; }
    }
    printf("BALANCE: %d/%d total heuristic wins, %.2f average combats\n",
        totalWins, runsPerDrive * DRIVE_COUNT, totalAvg / DRIVE_COUNT);
    return failed;
}
