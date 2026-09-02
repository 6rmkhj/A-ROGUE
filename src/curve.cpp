// 개발 전용 측정 도구: 난이도 곡선이 실제로 어디에 있는지 잰다.
// balance.cpp와 같은 탐욕 정책으로 런을 돌리면서 구역마다 체력·피해·방어를
// 기록한다. "쉽다"는 감각이 어느 구간에서 나오는지 숫자로 보기 위한 것이다.
#include <stdio.h>
#include "game.h"

#define ENCOUNTERS 9

struct Stats {
    long long runs;
    long long reached[ENCOUNTERS];      // 그 구역에 도달한 런 수
    long long hpSum[ENCOUNTERS];        // 구역 시작 시 체력 합
    long long maxHpSum[ENCOUNTERS];
    long long damageSum[ENCOUNTERS];    // 그 구역에서 잃은 체력 합
    long long turnSum[ENCOUNTERS];
    long long deaths[ENCOUNTERS];       // 그 구역에서 죽은 런 수
    long long turns;
    long long zeroDamageTurns;          // 받은 피해가 0인 턴 (방어가 전부 막았거나 적이 안 때림)
    long long blockSum;                 // 턴마다 얻은 방어도 합
    long long intentSum;                // 턴마다 적이 예고한 피해 합
    long long intentTurns;
    long long wins;
};

static int EffectiveDiePower(const GameState* game, int die) {
    if (game->dice[die].disabled || game->dice[die].offline) return 0;
    return FacePower(RolledFace(game, die));
}

// balance.cpp와 동일한 기믹 인지 탐욕 배치
static void AssignDice(GameState* game) {
    int order[3] = {0, 1, 2};
    for (int i = 0; i < 3; ++i) for (int j = i + 1; j < 3; ++j)
        if (EffectiveDiePower(game, order[j]) > EffectiveDiePower(game, order[i])) { int s = order[i]; order[i] = order[j]; order[j] = s; }
    int reversed = ResolveOrderReversed(game);
    int prefs[3] = {SLOT_ATTACK, SLOT_DEFEND, reversed ? SLOT_CHAIN : SLOT_AMPLIFY};
    int used[SLOT_COUNT] = {};
    for (int i = 0; i < 3; ++i) {
        int die = order[i], placed = 0;
        for (int p = 0; p < 3 && !placed; ++p) {
            int slot = prefs[p];
            if (used[slot] || SlotLockedThisTurn(game, slot)) continue;
            if (AssignDieToSlot(game, die, slot)) { used[slot] = 1; placed = 1; }
        }
        if (!placed) for (int slot = 0; slot < SLOT_COUNT && !placed; ++slot) {
            if (used[slot] || SlotLockedThisTurn(game, slot)) continue;
            if (AssignDieToSlot(game, die, slot)) { used[slot] = 1; placed = 1; }
        }
    }
}

static int WeakestFace(const GameState* game) {
    int best = 0, scoreBest = 100000;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        const Face* face = &game->dice[d].faces[f];
        int score = FacePower(face) * 10 - FaceCost(face);
        if (face->damaged) score -= 100;
        if (score < scoreBest) { scoreBest = score; best = d * 6 + f; }
    }
    return best;
}

static int WorstEfficiencyFace(const GameState* game) {
    int best = -1, scoreBest = -100000;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        const Face* face = &game->dice[d].faces[f];
        if (face->kind == FACE_EMPTY) continue;
        int score = FaceCost(face) * 4 - FacePower(face) * 5;
        if (face->damaged) score += 200;
        if (score > scoreBest) { scoreBest = score; best = d * 6 + f; }
    }
    return best;
}

static int BestReward(const GameState* game) {
    int best = 0, scoreBest = -100000;
    for (int i = 0; i < 3; ++i) {
        int kind = game->rewardKinds[i], cost = kind == FACE_NUMBER ? game->rewardValues[i] : FACE_INFO[kind].cost;
        int score = game->rewardValues[i] * 10 - cost;
        if (kind == FACE_WILD || kind == FACE_ECHO) score += 10;
        if (score > scoreBest) { scoreBest = score; best = i; }
    }
    return best;
}

static void Run(int drive, unsigned int seed, Stats* st) {
    GameState game; NewRun(&game, seed);
    game.driveChoices[0] = drive;
    ++st->runs;
    int steps = 0, index = -1, hpAtStart = 0, turnsHere = 0;
    while (game.phase != PHASE_VICTORY && game.phase != PHASE_GAMEOVER && steps++ < 600) {
        if (game.phase == PHASE_DRIVE_SELECT) { SelectDrive(&game, 0); }
        else if (game.phase == PHASE_COMBAT) {
            int floorIndex = game.floor < 0 ? 0 : game.floor > 2 ? 2 : game.floor;
            int here = floorIndex * 3 + game.encounter;
            if (here != index) {   // 새 구역에 들어섰다
                if (index >= 0 && index < ENCOUNTERS) { st->damageSum[index] += hpAtStart - game.playerHp; st->turnSum[index] += turnsHere; }
                index = here;
                turnsHere = 0;
                if (index >= 0 && index < ENCOUNTERS) {
                    ++st->reached[index];
                    st->hpSum[index] += game.playerHp;
                    st->maxHpSum[index] += game.playerMaxHp;
                    hpAtStart = game.playerHp;
                }
            }
            for (int i = 0; i < game.enemyCount; ++i)
                if (game.enemies[i].alive && game.enemies[i].intent != INTENT_GUARD && game.enemies[i].intent != INTENT_REPAIR) {
                    st->intentSum += game.enemies[i].intentValue; ++st->intentTurns;
                }
            int target = -1, hp = 100000;
            for (int i = 0; i < game.enemyCount; ++i) if (game.enemies[i].alive && game.enemies[i].hp < hp) { target = i; hp = game.enemies[i].hp; }
            if (target >= 0) SelectEnemy(&game, target);
            AssignDice(&game);
            EndTurn(&game);
            ++st->turns; ++turnsHere;
            st->blockSum += game.lastTurnBlockGained;
            if (game.lastTurnDamageTaken == 0) ++st->zeroDamageTurns;
        }
        else if (game.phase == PHASE_REWARD) {
            if (game.playerHp * 100 < game.playerMaxHp * 55) RepairSector(&game);
            else if (game.rewardIsTsr) InstallTsr(&game, 0);
            else { int r = BestReward(&game), f = WeakestFace(&game); SelectReward(&game, r); InstallSelectedReward(&game, f / 6, f % 6); }
        }
        else if (game.phase == PHASE_PRUNE) {
            while (UsedBytes(&game) > EffectiveCapacity(&game)) { int f = WorstEfficiencyFace(&game); if (f < 0) break; PruneFace(&game, f / 6, f % 6); }
            ConfirmPrune(&game);
        }
    }
    if (index >= 0 && index < ENCOUNTERS) { st->damageSum[index] += hpAtStart - game.playerHp; st->turnSum[index] += turnsHere; }
    if (game.phase == PHASE_GAMEOVER && index >= 0 && index < ENCOUNTERS) ++st->deaths[index];
    if (game.phase == PHASE_VICTORY) ++st->wins;
}

int main() {
    Stats st = {};
    const int runsPerDrive = 300;
    for (int drive = 0; drive < DRIVE_COUNT; ++drive)
        for (int i = 0; i < runsPerDrive; ++i)
            Run(drive, 0x77110000u + (unsigned int)(drive * 7717 + i * 7919), &st);

    printf("CURVE over %lld runs (%d drives x %d seeds), %lld wins (%.1f%%)\n",
        st.runs, DRIVE_COUNT, runsPerDrive, st.wins, st.runs ? 100.0 * st.wins / st.runs : 0.0);
    printf("  %-14s %8s %8s %8s %8s %8s\n", "encounter", "reached", "hp% in", "dmg", "turns", "deaths");
    for (int i = 0; i < ENCOUNTERS; ++i) {
        const char* label = (i % 3 == 2) ? "boss" : "mob";
        char name[32]; sprintf(name, "floor%d %s%d", i / 3 + 1, label, i % 3 + 1);
        double hpPct = st.maxHpSum[i] ? 100.0 * st.hpSum[i] / st.maxHpSum[i] : 0.0;
        printf("  %-14s %8lld %7.1f%% %8.2f %8.2f %8lld\n", name, st.reached[i], hpPct,
            st.reached[i] ? (double)st.damageSum[i] / st.reached[i] : 0.0,
            st.reached[i] ? (double)st.turnSum[i] / st.reached[i] : 0.0,
            st.deaths[i]);
    }
    printf("  turns total %lld · damage-free turns %lld (%.1f%%)\n",
        st.turns, st.zeroDamageTurns, st.turns ? 100.0 * st.zeroDamageTurns / st.turns : 0.0);
    printf("  average block gained per turn %.2f vs average telegraphed hit %.2f\n",
        st.turns ? (double)st.blockSum / st.turns : 0.0,
        st.intentTurns ? (double)st.intentSum / st.intentTurns : 0.0);
    return 0;
}
