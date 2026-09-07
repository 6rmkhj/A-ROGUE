#pragma once
#include "ui.h"

// Incomplete arcs leave the subject visible through the displaced air.
inline void DrawCraftArc(HDC dc, int cx, int cy, int rx, int ry, int angle,
                         int sweep, COLORREF tone, int thickness) {
    POINT last = {cx + CosMille(angle) * rx / 1000, cy + SinMille(angle) * ry / 1000};
    for (int i = 1; i <= 7; ++i) {
        int a = angle + sweep * i / 7;
        POINT next = {cx + CosMille(a) * rx / 1000, cy + SinMille(a) * ry / 1000};
        DrawLine(dc, last.x, last.y, next.x, next.y, tone, thickness);
        last = next;
    }
}

// Local effects, clipped by the caller to the relevant card. All trajectories
// derive from time and a fixed seed; no allocations and no gameplay randomness.
inline void DrawImpactBloom(HDC dc, int cx, int cy, int t, int power, int seed, COLORREF tone) {
    if (!FxDecorOn() || t < 0 || t >= 420 || power <= 0) return;
    if (power > 14) power = 14;
    int p = EaseOutCubic(Track(t, 0, 320));
    int fade = 1000 - Track(t, 45, 300);
    int radius = 3 + (17 + power * 3) * p / 1000;
    int axis = 3070 + (int)(Hash3(seed, 11, 41) % 370);
    COLORREF air = MixColor(C_BG, tone, FxScale(50 * fade / 1000));
    if (fade > 0) {
        DrawCraftArc(dc, cx - 2, cy + 3, radius, radius * 3 / 5, axis + 580, 920, air, t < 60 ? 2 : 1);
        DrawCraftArc(dc, cx + 3, cy, radius * 4 / 5, radius / 2, axis + 2320, 610,
            MixColor(C_BG, air, 65), 1);
    }
    int count = FxScale(5) + power;
    for (int i = 0; i < count; ++i) {
        uint32_t h = Hash3(seed, i, 71);
        int age = t - (int)((h >> 8) % 35);
        int life = 220 + (int)((h >> 14) % 190);
        if (age < 0 || age >= life) continue;
        int travel = EaseOutCubic(Track(age, 0, life));
        int angle = axis - 760 + (int)(h % 1520);
        if (i % 5 == 0) angle += 1800;
        int reach = 17 + power * 3 + (int)((h >> 19) % 31);
        int dist = 3 + reach * travel / 1000;
        int gravity = age * age * (i % 3 == 0 ? 40 : 20) / (life * life);
        int x = cx + CosMille(angle) * dist / 1000;
        int y = cy + SinMille(angle) * dist / 1000 + gravity;
        int length = 1 + (5 + (int)((h >> 24) % 8)) * (1000 - travel) / 1000;
        int tailX = x - CosMille(angle) * length / 1000;
        int tailY = y - SinMille(angle) * length / 1000 - age * length / life;
        int light = FxScale((i % 3 ? 77 : 96) * (life - age) / life);
        COLORREF chip = MixColor(C_BG, age < 60 && i % 3 == 0 ? C_TEXT : tone, light);
        DrawLine(dc, tailX, tailY, x, y, chip, i % 4 == 0 && age < 140 ? 2 : 1);
        if (i % 4 == 0 && age > 80 && FxScale(2) > 1)
            DrawLine(dc, x, y, x + 2, y + 2, MixColor(C_BG, chip, 55), 1);
    }
    if (t < 90) {
        int length = (15 + power * 2) * (1000 - Track(t, 18, 90)) / 1000;
        int dx = CosMille(axis) * length / 1000, dy = SinMille(axis) * length / 1000;
        COLORREF hot = MixColor(C_BG, C_TEXT, FxScale(95 * (90 - t) / 90));
        DrawLine(dc, cx - dx / 3, cy - dy / 3, cx + dx, cy + dy, hot, t < 38 ? 3 : 1);
        if (t < 42) DrawLine(dc, cx - dy / 5, cy + dx / 5, cx + dy / 4, cy - dx / 4, hot, 2);
    }
}

inline void DrawShieldMesh(HDC dc, const RECT& area, int t, int life, COLORREF tone, int strength) {
    if (!FxDecorOn() || t < 0 || t >= life || strength <= 0) return;
    int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, area.left + 1, area.top + 1, area.right - 1, area.bottom - 1);
    int fade = 1000 - Track(t, life / 2, life);
    int seat = EaseOutCubic(Track(t, 0, 100));
    int settle = t < 90 ? 0 : SinMille((t - 90) * 15) * (1000 - Track(t, 90, 280)) / 1000;
    int inset = 5 + (1000 - seat) * 13 / 1000 + settle * 2 / 1000;
    int l = area.left + inset, r = area.right - inset;
    int top = area.top + inset, bottom = area.bottom - inset;
    int bevel = (bottom - top) / 5;
    if (bevel > 15) bevel = 15;
    if (bevel < 2 || r - l < bevel * 2) { RestoreDC(dc, saved); return; }
    POINT v[9] = {{l + bevel, top}, {r - bevel, top}, {r, top + bevel}, {r, bottom - bevel},
        {r - bevel, bottom}, {l + bevel, bottom}, {l, bottom - bevel}, {l, top + bevel}, {l + bevel, top}};
    for (int i = 0; i < 8; ++i) {
        int join = EaseOutCubic(Track(t - (i < 4 ? i : 7 - i) * 15, 0, 95));
        if (join <= 0) continue;
        int shade = i == 0 || i == 7 ? 100 : i > 2 && i < 6 ? 52 : 78;
        COLORREF edge = MixColor(C_BG, tone, strength * fade / 1000 * shade / 100);
        DrawLine(dc, v[i].x, v[i].y, Lerp(v[i].x, v[i+1].x, join), Lerp(v[i].y, v[i+1].y, join), edge, t < 100 ? 3 : 2);
    }
    // Contact stays on the lip; no lattice crosses labels or damage numbers.
    int sweep = Track(t, 45, 210), cx = (l + r) / 2;
    int span = (r - l - bevel * 2) * sweep / 2000;
    COLORREF seam = MixColor(C_BG, tone, strength * fade / 2300);
    if (span > 0) DrawLine(dc, cx - span, top + 4, cx + span, top + 4, seam, 1);
    RestoreDC(dc, saved);
}

inline void DrawOrbitCorners(HDC dc, const RECT& r, int t, COLORREF color, int strength) {
    (void)t; // Selection holds still instead of perpetually expanding.
    if (!FxDecorOn() || strength <= 0) return;
    COLORREF tone = MixColor(C_BG, color, strength);
    for (int i = 0; i < 4; ++i) {
        int x = (i & 1) ? r.right - 3 : r.left + 2, dx = (i & 1) ? -11 : 11;
        int y = (i & 2) ? r.bottom - 3 : r.top + 2, dy = (i & 2) ? -7 : 7;
        DrawLine(dc, x, y, x + dx, y, tone, 2);
        DrawLine(dc, x, y, x, y + dy, tone, 2);
    }
}
