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

static int Run(unsigned int seed, int* combats) {
    GameState game; NewRun(&game, seed); int steps = 0;
    while (game.phase != PHASE_VICTORY && game.phase != PHASE_GAMEOVER && steps++ < 600) {
        if (game.phase == PHASE_DRIVE_SELECT) {
            SelectDrive(&game, (int)(seed % 3u));
        } else if (game.phase == PHASE_COMBAT) {
            int target = -1, hp = 100000;
            for (int i = 0; i < game.enemyCount; ++i) if (game.enemies[i].alive && game.enemies[i].hp < hp) { target = i; hp = game.enemies[i].hp; }
            if (target >= 0) SelectEnemy(&game, target);
            int order[3] = {0, 1, 2};
            for (int i = 0; i < 3; ++i) for (int j = i + 1; j < 3; ++j) if (FacePower(RolledFace(&game, order[j])) > FacePower(RolledFace(&game, order[i]))) { int swap = order[i]; order[i] = order[j]; order[j] = swap; }
            AssignDieToSlot(&game, order[0], SLOT_ATTACK); AssignDieToSlot(&game, order[1], SLOT_DEFEND); AssignDieToSlot(&game, order[2], SLOT_AMPLIFY); EndTurn(&game);
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
    *combats = game.combatsWon; return game.phase == PHASE_VICTORY;
}

int main() {
    int wins = 0, totalCombats = 0, histogram[10] = {}; const int runs = 300;
    for (int i = 0; i < runs; ++i) {
        int combats = 0; wins += Run(0x77110000u + (unsigned int)i * 7919u, &combats); totalCombats += combats;
        if (combats >= 0 && combats <= 9) ++histogram[combats];
    }
    printf("BALANCE: %d/%d heuristic wins, %.2f average combats\n", wins, runs, (double)totalCombats / runs);
    printf("COMBATS:"); for (int i = 0; i <= 9; ++i) printf(" %d:%d", i, histogram[i]); printf("\n");
    return wins == 0 ? 1 : 0;
}

