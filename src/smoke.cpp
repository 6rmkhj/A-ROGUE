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
            if (game.rewardIsTsr) InstallTsr(&game, 0);
            else { SelectReward(&game, 0); InstallSelectedReward(&game, game.facesInstalled % 3, game.facesInstalled % 6); }
        } else if (game.phase == PHASE_PRUNE) {
            while (UsedBytes(&game) > EffectiveCapacity(&game)) {
                int index = MostExpensiveFace(&game); if (index < 0) return 5; PruneFace(&game, index / 6, index % 6);
            }
            ConfirmPrune(&game);
        } else if (game.phase == PHASE_GAMEOVER) return 1;
    }
    if (game.phase != PHASE_VICTORY) return 2;
    if (game.combatsWon != 9) return 3;
    if (game.facesInstalled != 6) return 4;    // 일반 전투 6회 = 면 보상 6개
    if (game.tsrsInstalled != 2) return 6;     // 보스 2회 = 상주 프로그램 2개
    return 0;
}

int main() {
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
    if (base.phase != PHASE_COMBAT) return Fail("selecting a drive must start combat");
    Face damaged = {FACE_FIRE, 8, 1, 0};
    if (FaceCost(&damaged) != 24 || FacePower(&damaged) != 0) return Fail("damaged faces retain cost and lose power");
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
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        prune.dice[d].faces[f].kind = FACE_WILD; prune.dice[d].faces[f].value = 9; prune.dice[d].faces[f].damaged = 0;
    }
    if (DeckBytes(&prune) != 576) return Fail("all-wild test deck must be 576 bytes");
    while (DeckBytes(&prune) > EffectiveCapacity(&prune)) {
        int index = MostExpensiveFace(&prune); if (index < 0) return Fail("unable to find prune candidate"); PruneFace(&prune, index / 6, index % 6);
    }
    ConfirmPrune(&prune); if (prune.phase != PHASE_COMBAT) return Fail("valid pruned deck must continue");

    GameState burn; NewRun(&burn, 0xB0010001u); burn.modifierA = MOD_BAD_SECTOR; burn.modifierB = MOD_CHECKSUM; StartCombat(&burn);
    burn.enemies[0].hp = 3; burn.enemies[0].burn = 1; burn.enemies[0].intent = INTENT_GUARD; burn.enemies[0].intentValue = 0;
    AssignDieToSlot(&burn, 0, SLOT_DEFEND); EndTurn(&burn);
    if (burn.phase != PHASE_REWARD || burn.combatsWon != 1) return Fail("burn killing the last enemy must end combat immediately");

    GameState corrupt; NewRun(&corrupt, 0xC0110001u); corrupt.modifierA = MOD_BAD_SECTOR; corrupt.modifierB = MOD_CHECKSUM; StartCombat(&corrupt);
    corrupt.enemies[0].hp = corrupt.enemies[0].maxHp; corrupt.enemies[0].intent = INTENT_CORRUPT; corrupt.enemies[0].intentValue = 5;
    int hpBeforeCorrupt = corrupt.playerHp; AssignDieToSlot(&corrupt, 0, SLOT_DEFEND); EndTurn(&corrupt);
    if (corrupt.playerHp != hpBeforeCorrupt - 5) return Fail("corrupt intent must ignore player block");

    GameState badSector; NewRun(&badSector, 0xBADD5EC7u); badSector.phase = PHASE_REWARD;
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
    if (repair.phase != PHASE_COMBAT || repair.encounter != 1) return Fail("sector repair must advance the run");

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

    GameState tsr; NewRun(&tsr, 0x75720001u); tsr.modifierA = MOD_BAD_SECTOR; tsr.modifierB = MOD_CHECKSUM;
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
    if (tsr.floor != 1 || tsr.phase != PHASE_COMBAT) return Fail("boss loot must advance to the next floor");

    GameState himem; NewRun(&himem, 0x75720002u); himem.modifierA = MOD_BAD_SECTOR; himem.modifierB = MOD_CHECKSUM;
    himem.tsrInstalled[TSR_HIMEM] = 1;
    if (EffectiveCapacity(&himem) != 240 + TSR_INFO[TSR_HIMEM].value) return Fail("himem must extend the capacity");
    if (UsedBytes(&himem) != 63 + TSR_INFO[TSR_HIMEM].cost) return Fail("used bytes must include resident programs");

    GameState smart; NewRun(&smart, 0x75720003u); smart.modifierA = MOD_BAD_SECTOR; smart.modifierB = MOD_CHECKSUM;
    smart.tsrInstalled[TSR_SMARTDRV] = 1; StartCombat(&smart);
    if (smart.playerBlock != TSR_INFO[TSR_SMARTDRV].value) return Fail("smartdrv must grant first-turn block");
    smart.enemies[0].hp = 999; smart.enemies[0].maxHp = 999; smart.enemies[0].intent = INTENT_GUARD; smart.enemies[0].intentValue = 0;
    AssignDieToSlot(&smart, 0, SLOT_ATTACK); EndTurn(&smart);
    if (smart.turn != 2 || smart.playerBlock != 0) return Fail("smartdrv block must last only the first turn");

    for (unsigned int seed = 1; seed <= 64; ++seed) {
        GameState defrag; NewRun(&defrag, seed); defrag.modifierA = MOD_FRAGMENTATION; defrag.modifierB = MOD_CHECKSUM;
        defrag.tsrInstalled[TSR_DEFRAG] = 1; StartCombat(&defrag);
        for (int d = 0; d < 3; ++d) if (defrag.dice[d].disabled) return Fail("defrag must suppress fragmentation");
    }

    GameState scan; NewRun(&scan, 0x75720004u); scan.modifierA = MOD_BAD_SECTOR; scan.modifierB = MOD_CHECKSUM;
    scan.tsrInstalled[TSR_SCANDISK] = 1; scan.phase = PHASE_REWARD; scan.encounter = 2;
    SkipReward(&scan);
    int scanDamaged = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (scan.dice[d].faces[f].damaged) ++scanDamaged;
    if (scan.floor != 1 || scanDamaged != 0) return Fail("scandisk must block descent damage");
    GameState noScan; NewRun(&noScan, 0x75720004u); noScan.modifierA = MOD_BAD_SECTOR; noScan.modifierB = MOD_CHECKSUM;
    noScan.phase = PHASE_REWARD; noScan.encounter = 2;
    SkipReward(&noScan);
    int rawDamaged = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (noScan.dice[d].faces[f].damaged) ++rawDamaged;
    if (rawDamaged != 1) return Fail("bad sector descent must damage one face without scandisk");

    GameState undel; NewRun(&undel, 0x75720005u); undel.modifierA = MOD_BAD_SECTOR; undel.modifierB = MOD_CHECKSUM;
    undel.tsrInstalled[TSR_UNDELETE] = 1; StartCombat(&undel);
    undel.playerHp = 20; undel.enemies[0].hp = 1;
    AssignDieToSlot(&undel, 0, SLOT_ATTACK); EndTurn(&undel);
    if (undel.playerHp != 20 + TSR_INFO[TSR_UNDELETE].value) return Fail("undelete must heal after a win");

    GameState keyb; NewRun(&keyb, 0x75720006u); keyb.modifierA = MOD_BAD_SECTOR; keyb.modifierB = MOD_CHECKSUM;
    keyb.tsrInstalled[TSR_KEYB] = 1; StartCombat(&keyb);
    KeybReroll(&keyb, 0);
    if (!keyb.keybUsedThisTurn) return Fail("keyb reroll must consume the turn charge");
    keyb.dice[1].rolledFace = 2;
    KeybReroll(&keyb, 1);
    if (keyb.dice[1].rolledFace != 2) return Fail("keyb must reroll only once per turn");
    keyb.enemies[0].hp = 999; keyb.enemies[0].maxHp = 999; keyb.enemies[0].intent = INTENT_GUARD; keyb.enemies[0].intentValue = 0;
    AssignDieToSlot(&keyb, 0, SLOT_ATTACK); EndTurn(&keyb);
    if (keyb.phase != PHASE_COMBAT || keyb.keybUsedThisTurn) return Fail("keyb charge must reset each turn");
    GameState noKeyb; NewRun(&noKeyb, 0x75720007u); noKeyb.modifierA = MOD_BAD_SECTOR; noKeyb.modifierB = MOD_CHECKSUM; StartCombat(&noKeyb);
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
    for (int a = 0; a < MODIFIER_COUNT; ++a) for (int b = a + 1; b < MODIFIER_COUNT; ++b) for (int seed = 0; seed < 3; ++seed) {
        int result = RunCompleteGame(a, b, 0x10203040u + (unsigned int)(a * 101 + b * 17 + seed));
        if (result != 0) { printf("FAIL: modifier pair %d/%d seed %d result %d\n", a, b, seed, result); return 1; }
        ++runs;
    }
    printf("PASS: %d complete runs across all modifier pairs\n", runs); return 0;
}
