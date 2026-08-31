#pragma once

#include <stdint.h>
#include "data.h"

enum GamePhase {
    PHASE_TITLE = 0,
    PHASE_COMBAT,
    PHASE_REWARD,
    PHASE_PRUNE,
    PHASE_GAMEOVER,
    PHASE_VICTORY
};

struct Face {
    uint8_t kind;
    uint8_t value;
    uint8_t damaged;
    uint8_t reserved;
};

struct DieState {
    Face faces[6];
    uint8_t rolledFace;
    int8_t assignedSlot;
    uint8_t disabled;
    uint8_t unstable;
};

struct EnemyState {
    uint8_t kind;
    uint8_t intent;
    uint8_t alive;
    uint8_t burn;
    int hp;
    int maxHp;
    int block;
    int intentValue;
};

struct GameState {
    GamePhase phase;
    uint32_t rng;
    int floor;
    int encounter;
    int turn;
    int playerHp;
    int playerMaxHp;
    int playerBlock;
    int targetEnemy;
    int selectedDie;
    int enemyCount;
    EnemyState enemies[3];
    DieState dice[3];
    int modifierA;
    int modifierB;
    int rewardKinds[3];
    int rewardValues[3];
    int selectedReward;
    int lastDamage;
    int lastBlock;
    int combatsWon;
    int facesInstalled;
    int pruneAdvancePending;
    wchar_t logs[5][96];
};

void InitTitle(GameState* game);
void NewRun(GameState* game, uint32_t seed);
void StartCombat(GameState* game);
void AssignDieToSlot(GameState* game, int dieIndex, int slotIndex);
void UnassignDie(GameState* game, int dieIndex);
void SelectEnemy(GameState* game, int enemyIndex);
void EndTurn(GameState* game);
void SelectReward(GameState* game, int rewardIndex);
void InstallSelectedReward(GameState* game, int dieIndex, int faceIndex);
void SkipReward(GameState* game);
void PruneFace(GameState* game, int dieIndex, int faceIndex);
void ConfirmPrune(GameState* game);

int FaceCost(const Face* face);
int FacePower(const Face* face);
int DeckBytes(const GameState* game);
int EffectiveCapacity(const GameState* game);
int NonEmptyFaceCount(const GameState* game);
int IsModifierActive(const GameState* game, int modifier);
int LivingEnemyCount(const GameState* game);
const Face* RolledFace(const GameState* game, int dieIndex);
