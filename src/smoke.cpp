#include <stdio.h>
#include "game.h"

static int Fail(const char* message) { printf("FAIL: %s\n", message); return 1; }

static int MostExpensiveFace(const GameState* game) {
    int best = -1, bestCost = -1;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        int cost = FaceCost(&game->dice[d].faces[f]);
        if (cost > bestCost) { bestCost = cost; best = d * 6 + f; }
    }
    return best;
}

static int RunCompleteGame(int modifierA, int modifierB, unsigned int seed) {
    GameState game; NewRun(&game, seed); game.modifierA = modifierA; game.modifierB = modifierB; game.floor = 0; game.encounter = 0; StartCombat(&game);
    game.playerMaxHp = 999; game.playerHp = 999; int guard = 0;
    while (game.phase != PHASE_VICTORY && guard++ < 500) {
        if (game.phase == PHASE_COMBAT) {
            game.playerHp = 999;
            for (int i = 0; i < game.enemyCount; ++i) if (game.enemies[i].alive) game.enemies[i].hp = 1;
            AssignDieToSlot(&game, 0, SLOT_ATTACK); AssignDieToSlot(&game, 1, SLOT_AMPLIFY); AssignDieToSlot(&game, 2, SLOT_DEFEND); EndTurn(&game);
        } else if (game.phase == PHASE_REWARD) {
            SelectReward(&game, 0); InstallSelectedReward(&game, game.facesInstalled % 3, game.facesInstalled % 6);
        } else if (game.phase == PHASE_PRUNE) {
            while (DeckBytes(&game) > EffectiveCapacity(&game)) {
                int index = MostExpensiveFace(&game); if (index < 0) return 5; PruneFace(&game, index / 6, index % 6);
            }
            ConfirmPrune(&game);
        } else if (game.phase == PHASE_GAMEOVER) return 1;
    }
    if (game.phase != PHASE_VICTORY) return 2;
    if (game.combatsWon != 9) return 3;
    if (game.facesInstalled != 8) return 4;
    return 0;
}

int main() {
    GameState base; NewRun(&base, 0x12345678u);
    if (DeckBytes(&base) != 63) return Fail("starting deck must be 63 bytes");
    if (base.modifierA == base.modifierB) return Fail("modifiers must be unique");
    Face damaged = {FACE_FIRE, 8, 1, 0};
    if (FaceCost(&damaged) != 24 || FacePower(&damaged) != 0) return Fail("damaged faces retain cost and lose power");
    base.floor = 2; base.modifierA = MOD_OVERALLOC; base.modifierB = MOD_CHECKSUM;
    if (EffectiveCapacity(&base) != 190) return Fail("overallocation must add 60 bytes");

    GameState prune; NewRun(&prune, 0xCAFEBABEu); prune.floor = 2; prune.modifierA = MOD_CHECKSUM; prune.modifierB = MOD_FRAGMENTATION; prune.phase = PHASE_PRUNE;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        prune.dice[d].faces[f].kind = FACE_WILD; prune.dice[d].faces[f].value = 9; prune.dice[d].faces[f].damaged = 0;
    }
    if (DeckBytes(&prune) != 576) return Fail("all-wild test deck must be 576 bytes");
    while (DeckBytes(&prune) > EffectiveCapacity(&prune)) {
        int index = MostExpensiveFace(&prune); if (index < 0) return Fail("unable to find prune candidate"); PruneFace(&prune, index / 6, index % 6);
    }
    ConfirmPrune(&prune); if (prune.phase != PHASE_COMBAT) return Fail("valid pruned deck must continue");

    GameState burn; NewRun(&burn, 0xB0010001u); burn.modifierA = MOD_BAD_SECTOR; burn.modifierB = MOD_CHECKSUM;
    burn.enemies[0].hp = 3; burn.enemies[0].burn = 1; burn.enemies[0].intent = INTENT_GUARD; burn.enemies[0].intentValue = 0;
    AssignDieToSlot(&burn, 0, SLOT_DEFEND); EndTurn(&burn);
    if (burn.phase != PHASE_REWARD || burn.combatsWon != 1) return Fail("burn killing the last enemy must end combat immediately");

    GameState corrupt; NewRun(&corrupt, 0xC0110001u); corrupt.modifierA = MOD_BAD_SECTOR; corrupt.modifierB = MOD_CHECKSUM;
    corrupt.enemies[0].hp = corrupt.enemies[0].maxHp; corrupt.enemies[0].intent = INTENT_CORRUPT; corrupt.enemies[0].intentValue = 5;
    int hpBeforeCorrupt = corrupt.playerHp; AssignDieToSlot(&corrupt, 0, SLOT_DEFEND); EndTurn(&corrupt);
    if (corrupt.playerHp != hpBeforeCorrupt - 5) return Fail("corrupt intent must ignore player block");

    int fragmentationShown = 0;
    for (unsigned int seed = 1; seed <= 256 && !fragmentationShown; ++seed) {
        GameState fragmented; NewRun(&fragmented, seed); fragmented.modifierA = MOD_FRAGMENTATION; fragmented.modifierB = MOD_CHECKSUM; StartCombat(&fragmented);
        for (int d = 0; d < 3; ++d) if (fragmented.dice[d].disabled) fragmentationShown = 1;
    }
    if (!fragmentationShown) return Fail("fragmentation must be marked before the player executes the turn");

    GameState firstFloorCap; NewRun(&firstFloorCap, 0xCA900001u); firstFloorCap.modifierA = MOD_BAD_SECTOR; firstFloorCap.modifierB = MOD_CHECKSUM;
    firstFloorCap.phase = PHASE_REWARD; firstFloorCap.floor = 0; firstFloorCap.encounter = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        firstFloorCap.dice[d].faces[f].kind = FACE_WILD; firstFloorCap.dice[d].faces[f].value = 9; firstFloorCap.dice[d].faces[f].damaged = 0;
    }
    SkipReward(&firstFloorCap);
    if (firstFloorCap.phase != PHASE_PRUNE || firstFloorCap.floor != 0 || firstFloorCap.encounter != 0) return Fail("floor 1 capacity must be enforced before advancing");
    while (DeckBytes(&firstFloorCap) > EffectiveCapacity(&firstFloorCap)) {
        int index = MostExpensiveFace(&firstFloorCap); if (index < 0) return Fail("unable to prune floor 1 test deck"); PruneFace(&firstFloorCap, index / 6, index % 6);
    }
    ConfirmPrune(&firstFloorCap);
    if (firstFloorCap.phase != PHASE_COMBAT || firstFloorCap.floor != 0 || firstFloorCap.encounter != 1) return Fail("floor 1 prune must resume at the next encounter");

    int runs = 0;
    for (int a = 0; a < MODIFIER_COUNT; ++a) for (int b = a + 1; b < MODIFIER_COUNT; ++b) for (int seed = 0; seed < 3; ++seed) {
        int result = RunCompleteGame(a, b, 0x10203040u + (unsigned int)(a * 101 + b * 17 + seed));
        if (result != 0) { printf("FAIL: modifier pair %d/%d seed %d result %d\n", a, b, seed, result); return 1; }
        ++runs;
    }
    printf("PASS: %d complete runs across all modifier pairs\n", runs); return 0;
}
