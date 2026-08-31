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

int FaceCost(const Face* face) {
    if (!face) return 0;
    if (face->kind == FACE_NUMBER) return face->value;
    if (face->kind >= FACE_KIND_COUNT) return 0;
    return FACE_INFO[face->kind].cost;
}

int FacePower(const Face* face) {
    if (!face || face->damaged || face->kind == FACE_EMPTY) return 0;
    if (face->kind == FACE_NUMBER) return face->value;
    if (face->kind >= FACE_KIND_COUNT) return 0;
    return FACE_INFO[face->kind].power;
}

int DeckBytes(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) total += FaceCost(&game->dice[d].faces[f]);
    return total;
}

int IsModifierActive(const GameState* game, int modifier) {
    return game->modifierA == modifier || game->modifierB == modifier;
}

int EffectiveCapacity(const GameState* game) {
    int floor = ClampInt(game->floor, 0, 2);
    int capacity = FLOOR_CAPACITY[floor];
    if (IsModifierActive(game, MOD_OVERALLOC)) capacity += 60;
    return capacity;
}

int NonEmptyFaceCount(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) if (game->dice[d].faces[f].kind != FACE_EMPTY) ++total;
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

void InitTitle(GameState* game) {
    ZeroMemory(game, sizeof(*game));
    game->phase = PHASE_TITLE;
    game->selectedDie = -1;
    game->selectedReward = -1;
}

static void SetupStartingDice(GameState* game) {
    for (int d = 0; d < 3; ++d) {
        for (int f = 0; f < 6; ++f) {
            game->dice[d].faces[f].kind = FACE_NUMBER;
            game->dice[d].faces[f].value = (uint8_t)(f + 1);
            game->dice[d].faces[f].damaged = 0;
            game->dice[d].faces[f].reserved = 0;
        }
        game->dice[d].rolledFace = 0;
        game->dice[d].assignedSlot = -1;
        game->dice[d].disabled = 0;
        game->dice[d].unstable = 0;
    }
}

static void PickModifiers(GameState* game) {
    game->modifierA = RandomRange(game, MODIFIER_COUNT);
    do game->modifierB = RandomRange(game, MODIFIER_COUNT); while (game->modifierB == game->modifierA);
}

static void RollDice(GameState* game) {
    for (int d = 0; d < 3; ++d) {
        game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
        game->dice[d].assignedSlot = -1;
        game->dice[d].disabled = 0;
        game->dice[d].unstable = 0;
    }
    game->selectedDie = -1;
    if (IsModifierActive(game, MOD_READ_ERROR)) game->dice[RandomRange(game, 3)].unstable = 1;
}

static void PlanEnemy(GameState* game, EnemyState* enemy, int enemyIndex) {
    const EnemyInfo* info = &ENEMY_INFO[enemy->kind];
    int cycle = (game->turn + enemyIndex + enemy->kind) & 3;
    enemy->intent = INTENT_ATTACK;
    enemy->intentValue = info->damage + game->floor;
    if (enemy->kind >= BOSS_DISK_ERROR) {
        int bossCycle = (game->turn + enemy->kind) % 5;
        if (bossCycle == 0) {
            enemy->intent = INTENT_CORRUPT;
            enemy->intentValue = info->damage - 2;
        } else if (bossCycle == 2) {
            enemy->intent = INTENT_GUARD;
            enemy->intentValue = info->guard + game->floor * 2;
        } else if (bossCycle == 4) {
            enemy->intent = INTENT_HEAVY;
            enemy->intentValue = info->damage + 3 + game->floor;
        }
        return;
    }
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

static void ApplyFragmentation(GameState* game);

static void BeginTurn(GameState* game) {
    game->playerBlock = 0;
    game->lastDamage = 0;
    game->lastBlock = 0;
    RollDice(game);
    ApplyFragmentation(game);
    for (int i = 0; i < game->enemyCount; ++i) if (game->enemies[i].alive) PlanEnemy(game, &game->enemies[i], i);
    PushLog(game, L"주사위를 슬롯에 배치하고 SPACE로 실행합니다.");
}

void NewRun(GameState* game, uint32_t seed) {
    ZeroMemory(game, sizeof(*game));
    game->rng = seed ? seed : 0xC0FFEE11u;
    game->phase = PHASE_COMBAT;
    game->floor = 0;
    game->encounter = 0;
    game->playerMaxHp = 40;
    game->playerHp = 40;
    game->selectedDie = -1;
    game->selectedReward = -1;
    SetupStartingDice(game);
    PickModifiers(game);
    PushLog(game, L"A:\\ROGUE 부팅 완료.");
    StartCombat(game);
}

static void AddEnemy(GameState* game, int kind) {
    if (game->enemyCount >= 3) return;
    EnemyState* enemy = &game->enemies[game->enemyCount++];
    ZeroMemory(enemy, sizeof(*enemy));
    enemy->kind = (uint8_t)kind;
    enemy->alive = 1;
    int hp = ENEMY_INFO[kind].hp + game->floor * 3;
    if (IsModifierActive(game, MOD_OVERALLOC)) hp = (hp * 130 + 99) / 100;
    enemy->hp = hp;
    enemy->maxHp = hp;
}

void StartCombat(GameState* game) {
    game->phase = PHASE_COMBAT;
    game->turn = 1;
    game->enemyCount = 0;
    game->targetEnemy = 0;
    ZeroMemory(game->enemies, sizeof(game->enemies));
    if (game->encounter == 2) {
        AddEnemy(game, BOSS_DISK_ERROR + game->floor);
    } else {
        int poolStart = game->floor == 0 ? ENEMY_GLITCH : game->floor == 1 ? ENEMY_SPYWARE : ENEMY_FRAGMENT;
        int poolCount = game->floor == 0 ? 3 : 4;
        AddEnemy(game, poolStart + RandomRange(game, poolCount));
    }
    BeginTurn(game);
}

void AssignDieToSlot(GameState* game, int dieIndex, int slotIndex) {
    if (game->phase != PHASE_COMBAT || dieIndex < 0 || dieIndex >= 3 || slotIndex < 0 || slotIndex >= SLOT_COUNT) return;
    for (int d = 0; d < 3; ++d) if (d != dieIndex && game->dice[d].assignedSlot == slotIndex) game->dice[d].assignedSlot = -1;
    if (game->dice[dieIndex].assignedSlot == slotIndex) game->dice[dieIndex].assignedSlot = -1;
    else game->dice[dieIndex].assignedSlot = (int8_t)slotIndex;
    game->selectedDie = dieIndex;
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
    if (enemy->hp <= 0) {
        enemy->hp = 0;
        enemy->alive = 0;
        PushLog2(game, L"%s 삭제 완료. 피해 %d.", ENEMY_INFO[enemy->kind].name, damage);
        game->targetEnemy = FirstLivingEnemy(game);
    }
    return damage;
}

static void ApplyFragmentation(GameState* game) {
    if (!IsModifierActive(game, MOD_FRAGMENTATION)) return;
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
    if (fragmented > 0) PushLog(game, L"FRAGMENTATION: 중복 주사위가 비활성화되었습니다.");
}

static int RollOutputSum(const GameState* game) {
    int total = 0;
    for (int d = 0; d < 3; ++d) total += FacePower(RolledFace(game, d));
    return total;
}

static int SlotPower(const GameState* game, int slot, int* kindOut) {
    int die = DieForSlot(game, slot);
    if (die < 0 || game->dice[die].disabled) {
        if (kindOut) *kindOut = FACE_EMPTY;
        return 0;
    }
    const Face* face = RolledFace(game, die);
    if (!face) return 0;
    if (kindOut) *kindOut = face->kind;
    return FacePower(face);
}

static void ResolvePlayer(GameState* game) {
    int ampKind = FACE_EMPTY;
    int ampPower = SlotPower(game, SLOT_AMPLIFY, &ampKind);
    int ampBonus = ampPower / 2;
    if (ampKind == FACE_BOOST) ampBonus = ampPower;
    if (ampKind == FACE_WILD) ampBonus += 2;

    int attackKind = FACE_EMPTY;
    int attackPower = SlotPower(game, SLOT_ATTACK, &attackKind);
    int checksumBonus = 0;
    if (IsModifierActive(game, MOD_CHECKSUM) && (RollOutputSum(game) & 1) == 0) {
        checksumBonus = 2;
        PushLog(game, L"CHECKSUM OK: 이번 공격 피해 +2.");
    }
    int attackDamage = attackPower > 0 ? attackPower + ampBonus + checksumBonus : 0;
    if (attackKind == FACE_FIRE) attackDamage += 4;
    if (attackKind == FACE_WILD) attackDamage += 2;
    int target = FirstLivingEnemy(game);
    int dealt = DamageEnemy(game, target, attackDamage);
    if (target >= 0 && attackKind == FACE_FIRE && game->enemies[target].alive) game->enemies[target].burn = 2;
    if (attackKind == FACE_LEECH && dealt > 0) {
        int heal = dealt / 3 + 1;
        game->playerHp = ClampInt(game->playerHp + heal, 0, game->playerMaxHp);
    }
    game->lastDamage = attackDamage;

    int defendKind = FACE_EMPTY;
    int defendPower = SlotPower(game, SLOT_DEFEND, &defendKind);
    int block = defendPower > 0 ? defendPower + ampBonus : 0;
    if (defendKind == FACE_SHIELD) block *= 2;
    if (defendKind == FACE_WILD) block += 3;
    game->playerBlock += block;
    game->lastBlock = block;

    int chainKind = FACE_EMPTY;
    int chainPower = SlotPower(game, SLOT_CHAIN, &chainKind);
    if (chainPower > 0) {
        if (game->lastDamage > 0 && LivingEnemyCount(game) > 0) {
            int repeat = (game->lastDamage * (chainPower + 4)) / 13;
            if (chainKind == FACE_ECHO) repeat = game->lastDamage;
            if (chainKind == FACE_WILD) repeat += 2;
            DamageEnemy(game, FirstLivingEnemy(game), repeat);
        } else if (game->lastBlock > 0) {
            int repeat = (game->lastBlock * (chainPower + 4)) / 13;
            if (chainKind == FACE_ECHO) repeat = game->lastBlock;
            game->playerBlock += repeat;
        }
    }
}

static void ResolveEnemies(GameState* game) {
    for (int i = 0; i < game->enemyCount; ++i) {
        EnemyState* enemy = &game->enemies[i];
        if (!enemy->alive) continue;
        if (enemy->burn > 0) {
            --enemy->burn;
            DamageEnemy(game, i, 3);
            if (!enemy->alive) continue;
        }
        if (enemy->intent == INTENT_GUARD) {
            enemy->block += enemy->intentValue;
        } else if (enemy->intent == INTENT_REPAIR) {
            enemy->hp = ClampInt(enemy->hp + enemy->intentValue, 0, enemy->maxHp);
        } else {
            int damage = enemy->intentValue;
            if (enemy->intent != INTENT_CORRUPT) {
                int absorbed = game->playerBlock < damage ? game->playerBlock : damage;
                game->playerBlock -= absorbed;
                damage -= absorbed;
            }
            game->playerHp -= damage;
            if (enemy->intent == INTENT_CORRUPT && damage > 0) PushLog(game, L"오염 공격이 BLOCK을 무시했습니다.");
        }
        if (game->playerHp <= 0) {
            game->playerHp = 0;
            game->phase = PHASE_GAMEOVER;
            PushLog(game, L"SYSTEM FAILURE. R 키로 재시작하십시오.");
            return;
        }
    }
}

static void GenerateRewards(GameState* game) {
    for (int i = 0; i < 3; ++i) {
        int kind;
        do kind = RandomRange(game, 7); while (i > 0 && kind == game->rewardKinds[i - 1]);
        game->rewardKinds[i] = kind;
        game->rewardValues[i] = kind == FACE_NUMBER ? 7 + RandomRange(game, 4) : FACE_INFO[kind].power;
    }
    game->selectedReward = -1;
}

static void CombatWon(GameState* game) {
    ++game->combatsWon;
    int heal = game->encounter == 2 ? 10 : 5;
    game->playerHp = ClampInt(game->playerHp + heal, 0, game->playerMaxHp);
    if (game->floor == 2 && game->encounter == 2) {
        game->phase = PHASE_VICTORY;
        PushLog(game, L"FORMAT 중단. 디스크가 복구되었습니다.");
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
    for (int d = 0; d < 3; ++d) {
        if (game->dice[d].unstable) {
            game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
            PushLog(game, L"READ ERROR: 경고된 주사위가 재굴림되었습니다.");
            break;
        }
    }
    ResolvePlayer(game);
    if (LivingEnemyCount(game) == 0) {
        CombatWon(game);
        return;
    }
    ResolveEnemies(game);
    if (game->phase == PHASE_GAMEOVER) return;
    if (LivingEnemyCount(game) == 0) {
        CombatWon(game);
        return;
    }
    ++game->turn;
    BeginTurn(game);
}

void SelectReward(GameState* game, int rewardIndex) {
    if (game->phase != PHASE_REWARD || rewardIndex < 0 || rewardIndex >= 3) return;
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
    PushLog(game, L"BAD SECTOR: 무작위 면 하나가 손상되었습니다.");
}

static void ContinueAfterReward(GameState* game) {
    if (DeckBytes(game) > EffectiveCapacity(game)) {
        game->phase = PHASE_PRUNE;
        game->pruneAdvancePending = 1;
        PushLog(game, L"현재 층 용량 초과: 면을 비워 한도에 맞추십시오.");
        return;
    }
    if (game->encounter < 2) {
        ++game->encounter;
        StartCombat(game);
        return;
    }
    ++game->floor;
    game->encounter = 0;
    if (game->floor >= 3) {
        game->phase = PHASE_VICTORY;
        return;
    }
    if (IsModifierActive(game, MOD_BAD_SECTOR)) DamageRandomFace(game);
    if (DeckBytes(game) > EffectiveCapacity(game)) {
        game->phase = PHASE_PRUNE;
        game->pruneAdvancePending = 0;
        PushLog(game, L"용량 초과: 면을 비워 다음 층 한도에 맞추십시오.");
    } else StartCombat(game);
}

void InstallSelectedReward(GameState* game, int dieIndex, int faceIndex) {
    if (game->phase != PHASE_REWARD || game->selectedReward < 0) return;
    if (dieIndex < 0 || dieIndex >= 3 || faceIndex < 0 || faceIndex >= 6) return;
    int reward = game->selectedReward;
    Face* face = &game->dice[dieIndex].faces[faceIndex];
    face->kind = (uint8_t)game->rewardKinds[reward];
    face->value = (uint8_t)game->rewardValues[reward];
    face->damaged = 0;
    ++game->facesInstalled;
    PushLog2(game, L"%s 면 설치. 현재 덱 %dB.", FACE_INFO[face->kind].name, DeckBytes(game));
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
    PushLog(game, L"면을 삭제해 용량을 확보했습니다.");
}

void ConfirmPrune(GameState* game) {
    if (game->phase != PHASE_PRUNE) return;
    if (DeckBytes(game) > EffectiveCapacity(game)) {
        PushLog(game, L"아직 층 용량을 초과합니다.");
        return;
    }
    if (NonEmptyFaceCount(game) == 0) {
        PushLog(game, L"최소 한 면은 남겨야 합니다.");
        return;
    }
    int advance = game->pruneAdvancePending;
    game->pruneAdvancePending = 0;
    if (advance) ContinueAfterReward(game);
    else StartCombat(game);
}
