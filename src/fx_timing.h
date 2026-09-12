#pragma once
#include "game.h"

// Presentation-only score. Launches have time to arrive; chains accelerate,
// heavy hits hold their impact. No game state or RNG is modified.
inline int FxTraceLine(const GameState& game, int line) {
    int count = game.turnTraceCount > 0 ? game.turnTraceCount : 1;
    return line < count ? line : count - 1;
}

inline int FxImpactHold(const CombatFxEvent& fx) {
    if (fx.type == CFX_ENEMY_STRIKE || fx.type == CFX_ATTACK_LAUNCH
        || fx.type == CFX_AMPLIFY || fx.type == CFX_DEFEND
        || (fx.flags & CFXF_DEFEND_CHAIN)) return 0;
    return (fx.flags & CFXF_KILL) ? 85 : (fx.flags & CFXF_BIG_HIT) ? 50 : 0;
}

inline int FxTraceSpan(const GameState& game, int line) {
    int span = 110; // bookkeeping should not hold the stage as long as a hit
    for (int i = 0; i < game.combatFxCount; ++i) {
        const CombatFxEvent& fx = game.combatFx[i];
        if (FxTraceLine(game, fx.traceLine) != line) continue;
        int beat = fx.type == CFX_AMPLIFY ? 300 : fx.type == CFX_ATTACK_LAUNCH ? 230
            : fx.type == CFX_CHAIN ? 260 : fx.type == CFX_ENEMY_STRIKE ? 410
            : fx.type == CFX_DEFEND ? 310 : 320;
        if (fx.flags & CFXF_KILL) beat = 500;
        beat += FxImpactHold(fx);
        if (beat > span) span = beat;
    }
    return span;
}

inline int FxTraceAt(const GameState& game, int line) {
    int at = 0;
    for (int i = 0; i < line; ++i) at += FxTraceSpan(game, i);
    return at;
}

inline int FxEventElapsed(const GameState& game, int index, int elapsed, int decor) {
    if (index < 0 || index >= game.combatFxCount) return -1;
    const CombatFxEvent& fx = game.combatFx[index];
    int age = elapsed - FxTraceAt(game, FxTraceLine(game, fx.traceLine));
    if (age < 0) return -1;
    int hold = decor ? FxImpactHold(fx) : 0;
    if (age < 32) return age;
    return age < 32 + hold ? 32 : age - hold;
}
