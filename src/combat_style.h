#pragma once
#include "ui.h"
#include "fx_draw.h"

// Containment light originates under the process. Motion is clock-derived;
// the silhouette and stage remain readable with effects disabled.
inline void DrawProcessStage(HDC dc, const RECT& box, int kind, int alive,
    int flash, int bob, int shift, int sx, int sy) {
    COLORREF tone = (COLORREF)GetEnemyInfoOrUnknown(kind)->color;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, box.left, box.top, box.right, box.bottom);
    Fill(dc, box, RGB(9, 15, 23));
    int cx = (box.left + box.right) / 2;
    for (int j = 0; j < 20; ++j) {
        int half = 36 + j * 5, y = box.bottom - 30 - j * 10;
        Fill(dc, MakeRect(cx - half, y - 9, cx + half, y + 1),
            MixColor(RGB(9, 15, 23), tone, alive ? 15 - j / 2 : 3));
    }
    COLORREF rail = MixColor(C_BG, tone, alive ? 48 : 12);
    for (int side = 0; side < 2; ++side) {
        int x = side ? box.right - 8 : box.left + 8;
        DrawLine(dc, x, box.top + 28, x, box.bottom - 26, MixColor(C_BG, tone, 20), 1);
        for (int j = 0; j < 9; ++j) {
            int y = box.top + 36 + j * 22;
            Fill(dc, MakeRect(x - 2, y, x + 3, y + 3), rail);
        }
    }
    DrawLine(dc, box.left + 24, box.bottom - 18, box.right - 24, box.bottom - 18, rail, 2);
    DrawLine(dc, box.left + 44, box.bottom - 13, box.right - 44, box.bottom - 13,
        MixColor(C_BG, tone, 22), 1);
    RECT art = box; InflateRect(&art, -20, -20);
    int reveal = FxDecorOn() ? EaseOutCubic(Track(SceneElapsed(), 60, 480)) : 1000;
    int clip = SaveDC(dc);
    IntersectClipRect(dc, art.left - 10, art.top - 10, art.right + 10,
        art.top - 10 + (art.bottom - art.top + 20) * reveal / 1000);
    DrawSpriteArt(dc, art, kind, alive, flash, bob, shift, sx, sy);
    RestoreDC(dc, clip);
    if (reveal > 0 && reveal < 1000) {
        int y = art.top - 10 + (art.bottom - art.top + 20) * reveal / 1000;
        DrawLine(dc, art.left, y, art.right, y, MixColor(C_BG, tone, FxScale(80)), 2);
    }
    RestoreDC(dc, saved);
}

// Shared, fixed knots keep the electric path connected. The transverse offsets
// vanish at both anchors; no frame or particle ever consumes gameplay RNG.
inline void DrawDiePips(HDC dc, int x, int y, int value, COLORREF tone) {
    if (value < 1 || value > 6) return;
    static const unsigned masks[] = {0, 16, 257, 273, 325, 341, 365};
    for (int i = 0; i < 9; ++i) if (masks[value] & (1u << i)) {
        int px = x + i % 3 * 10, py = y + i / 3 * 10;
        Fill(dc, MakeRect(px, py, px + 5, py + 5), tone);
    }
}

inline POINT CombatLancePoint(POINT from, POINT to, int p, int chain, int seed) {
    if (p <= 0) return from;
    if (p >= 1000) return to;
    int dx = to.x - from.x, dy = to.y - from.y;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int length = ax > ay ? ax : ay;
    if (length < 1) return to;
    int bow = (int)(Hash3(seed, 13, 25) % 13) - 6;
    int offset = bow * 4 * p / 1000 * (1000 - p) / 1000;
    if (chain) {
        int knot = p * 16 / 1000;
        int local = p * 16 - knot * 1000;
        int a = knot == 0 ? 0 : (int)(Hash3(seed, knot, 93) % 25) - 12;
        int b = knot == 15 ? 0 : (int)(Hash3(seed, knot + 1, 93) % 25) - 12;
        offset += Lerp(a, b, local);
    }
    offset = FxScale(offset < 0 ? -offset : offset) * (offset < 0 ? -1 : 1);
    POINT result = {Lerp(from.x, to.x, p) - dy * offset / length,
                    Lerp(from.y, to.y, p) + dx * offset / length};
    return result;
}

inline int CombatLanceProgress(int t, int life) {
    int p = Track(t, 0, life);
    return (p + 3 * p * p / 1000) / 4;
}

inline void DrawEnergyLance(HDC dc, POINT from, POINT to, int t, int life, COLORREF color, int chain, int seed) {
    if (!FxDecorOn() || life <= 0 || t < 0 || t >= life) return;
    int headP = CombatLanceProgress(t, life);
    int tailP = CombatLanceProgress(t - (chain ? 145 : 100), life);
    int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, 22, 86, 698, 536);
    for (int i = 0; i < 16; ++i) {
        int a = i * 1000 / 16, b = (i + 1) * 1000 / 16;
        if (a < tailP) a = tailP;
        if (b > headP) b = headP;
        if (a >= b) continue;
        POINT start = CombatLancePoint(from, to, a, chain, seed);
        POINT end = CombatLancePoint(from, to, b, chain, seed);
        int light = 30 + (b - tailP) * 65 / (headP - tailP + 1);
        DrawLine(dc, start.x, start.y, end.x, end.y, MixColor(C_BG, color, FxScale(light / 3)), chain ? 3 : 5);
        DrawLine(dc, start.x, start.y, end.x, end.y, MixColor(C_BG, color, FxScale(light)), chain ? 1 : 2);
        if (chain && i % 4 == 2 && b < headP && FxScale(2) > 1) {
            int branch = 4 + (int)(Hash3(seed, i, 69) % 8);
            int dx = end.x - start.x, dy = end.y - start.y;
            int divisor = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + 1;
            DrawLine(dc, end.x, end.y, end.x - dy * branch / divisor, end.y + dx * branch / divisor,
                MixColor(C_BG, color, FxScale(light / 2)), 1);
        }
    }
    POINT head = CombatLancePoint(from, to, headP, chain, seed);
    POINT behind = CombatLancePoint(from, to, headP - 28, chain, seed);
    DrawLine(dc, behind.x, behind.y, head.x, head.y,
        MixColor(C_BG, C_TEXT, FxScale(93)), chain ? 2 : 3);
    if (t < 80) {
        int recoil = 4 + 9 * t / 80;
        DrawCraftArc(dc, from.x, from.y, recoil, recoil / 2, 2100, 1200,
            MixColor(C_BG, color, FxScale(55 * (80 - t) / 80)), 1);
    }
    RestoreDC(dc, saved);
}

inline void DrawImpactCut(HDC dc, const RECT& portrait, int t, COLORREF tone, int kill) {
    if (!FxDecorOn() || t < 0 || t >= 380) return;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, portrait.left, portrait.top, portrait.right, portrait.bottom);
    int cx = (portrait.left + portrait.right) / 2, cy = (portrait.top + portrait.bottom) / 2;
    int reach = (portrait.right - portrait.left) * EaseOutCubic(Track(t, 0, 100)) / 2;
    int fade = 1000 - Track(t, 70, 380);
    int light = FxScale(90 * fade / 1000);
    DrawLine(dc, cx - reach, cy + reach / 2, cx + reach, cy - reach / 2,
        MixColor(C_BG, tone, light / 3), kill ? 13 : 9);
    DrawLine(dc, cx - reach, cy + reach / 2, cx + reach, cy - reach / 2,
        MixColor(C_BG, tone, light), kill ? 5 : 3);
    DrawLine(dc, cx - reach, cy + reach / 2, cx + reach, cy - reach / 2,
        MixColor(C_BG, C_TEXT, light), 1);
    if (kill) DrawLine(dc, cx - reach, cy - reach / 3, cx + reach, cy + reach / 3,
        MixColor(C_BG, C_YELLOW, light), 2);
    RestoreDC(dc, saved);
}

inline void DrawFracture(HDC dc, const RECT& portrait, int t, int seed, COLORREF tone, int kill) {
    if (!FxDecorOn() || t < 0 || t >= 620) return;
    int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, portrait.left + 1, portrait.top + 1, portrait.right - 1, portrait.bottom - 1);
    int cx = (portrait.left + portrait.right) / 2 + (int)(Hash3(seed, 12, 9) % 11) - 5;
    int cy = (portrait.top + portrait.bottom) / 2 + 6;
    int fade = 1000 - Track(t, 130, 620);
    int branches = kill ? 7 : 4;
    int faultAngle = 2500 + (int)(Hash3(seed, 17, 49) % 720);
    for (int i = 0; i < branches; ++i) {
        uint32_t h = Hash3(seed, i, 84);
        int angle = faultAngle + (i % 2) * 1800;
        int reach = 34 + (int)((h >> 9) % (kill ? 59 : 31));
        int originX = cx, originY = cy, delay = (int)((h >> 18) % 20);
        if (i >= 2) {
            // Secondary cracks emerge from actual knots on the two main faults,
            // rather than every crack radiating from one perfect centre.
            int source = (i - 2) % 2, knot = 1 + (i - 2) / 2;
            uint32_t trunk = Hash3(seed, source, 84);
            int trunkReach = 34 + (int)((trunk >> 9) % (kill ? 59 : 31));
            int trunkAngle = faultAngle + source * 1800;
            int bend = (int)(Hash3(seed + source * 13, knot, 51) % 19) - 9;
            originX += CosMille(trunkAngle) * trunkReach * knot / 4000 - SinMille(trunkAngle) * bend / 1000;
            originY += SinMille(trunkAngle) * trunkReach * knot / 4000 + CosMille(trunkAngle) * bend / 1000;
            angle += (knot % 2 ? 1 : -1) * (390 + (int)(h % 290));
            reach = 18 + (int)((h >> 9) % (kill ? 34 : 20));
            delay += 35 + knot * 22;
        }
        int grow = EaseOutCubic(Track(t - delay, 0, kill ? 180 : 145));
        POINT last = {originX, originY};
        for (int joint = 1; joint <= 4; ++joint) {
            int step = joint * 250;
            int extent = grow < step ? grow : step;
            int previous = (joint - 1) * 250;
            if (extent <= previous) break;
            int bend = (int)(Hash3(seed + i * 13, joint, 51) % 19) - 9;
            POINT next = {originX + CosMille(angle) * reach * joint / 4000 - SinMille(angle) * bend / 1000,
                          originY + SinMille(angle) * reach * joint / 4000 + CosMille(angle) * bend / 1000};
            POINT end = {Lerp(last.x, next.x, (extent - previous) * 4), Lerp(last.y, next.y, (extent - previous) * 4)};
            int light = FxScale((75 - joint * 8) * fade / 1000);
            DrawLine(dc, last.x + 1, last.y + 1, end.x + 1, end.y + 1, MixColor(C_PANEL, C_INK, FxScale(fade * 70 / 1000)), 2);
            DrawLine(dc, last.x, last.y, end.x, end.y, MixColor(C_PANEL, tone, light), 1);
            // A fork starts at an existing joint, after the main crack arrives.
            if (joint == 3 && grow > 780) {
                int fork = (grow - 780) * (10 + (int)((h >> 23) % 12)) / 220;
                int forkAngle = angle + (i % 2 ? 490 : -560);
                DrawLine(dc, last.x, last.y, last.x + CosMille(forkAngle) * fork / 1000,
                    last.y + SinMille(forkAngle) * fork / 1000, MixColor(C_PANEL, tone, light * 2 / 3), 1);
            }
            last = next;
        }
        if (kill && t > delay + 140 && (i % 2 == 0 || FxScale(2) > 1)) {
            int age = t - delay - 140, travel = EaseOutCubic(Track(age, 0, 480));
            int x = originX + CosMille(angle) * (reach + travel * 22 / 1000) / 1000;
            int y = originY + SinMille(angle) * (reach + travel * 22 / 1000) / 1000 + age * age * 20 / (480 * 480);
            int spin = angle + age * (i % 2 ? 2 : -3);
            int dx = CosMille(spin) * 4 / 1000, dy = SinMille(spin) * 4 / 1000;
            COLORREF chip = MixColor(C_PANEL, tone, FxScale(fade * 65 / 1000));
            DrawLine(dc, x - dx, y - dy, x + dx, y + dy, chip, 2);
            DrawLine(dc, x + dx, y + dy, x - dy, y + dx, chip, 1);
        }
    }
    RestoreDC(dc, saved);
}

inline void DrawBossHalo(HDC dc, const RECT& r, COLORREF color, int index, int danger) {
    if (!FxDecorOn()) return;
    int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1);
    // Armour stays still at rest. Dangerous intent briefly tightens the grip,
    // followed by a long still interval, rather than a rotating or flashing halo.
    int tick = (int)(GetTickCount() % 60000);
    int cycle = (tick + index * 170) % 3000;
    int load = danger ? EaseInCubic(Track(cycle, 0, 300)) * (1000 - EaseOutCubic(Track(cycle, 300, 600))) / 1000 : 0;
    int grip = FxScale(load * 4 / 1000);
    COLORREF tone = MixColor(C_PANEL, color, FxScale((danger ? 43 : 23) + load / 35));
    for (int side = 0; side < 2; ++side) {
        int sign = side ? -1 : 1;
        int x = side ? r.right - 7 - grip : r.left + 6 + grip;
        int y = r.top + 13;
        DrawLine(dc, x, y + 19, x, y + 5, tone, danger ? 2 : 1);
        DrawLine(dc, x, y + 5, x + sign * 5, y, tone, danger ? 2 : 1);
        DrawLine(dc, x + sign * 5, y, x + sign * 17, y, tone, 1);
        int bottom = r.bottom - 12;
        DrawLine(dc, x, bottom - 12, x, bottom - 4, MixColor(C_PANEL, tone, 70), 1);
        DrawLine(dc, x, bottom - 4, x + sign * 4, bottom, MixColor(C_PANEL, tone, 70), 1);
        if (danger) {
            COLORREF warning = MixColor(C_PANEL, C_RED, FxScale(53 + load / 30));
            DrawLine(dc, x + sign * 3, y + 31, x + sign * 7, y + 37, warning, 2);
            DrawLine(dc, x + sign * 7, y + 37, x + sign * 3, y + 43, warning, 2);
        }
    }
    RestoreDC(dc, saved);
}
