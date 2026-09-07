#pragma once
#include "render.h"

// Local effects, clipped by the caller to the relevant card. All trajectories
// derive from time and a fixed seed; no allocations and no gameplay randomness.
inline void DrawImpactBloom(HDC dc, int cx, int cy, int t, int power, int seed, COLORREF tone) {
    if (t < 0 || t >= 420 || power <= 0) return;
    int p = EaseOutCubic(Track(t, 0, 420));
    COLORREF fade = MixColor(C_BG, tone, 85 * (1000 - Track(t, 60, 420)) / 1000);
    int radius = 5 + (22 + power * 3) * p / 1000;
    DrawGlowRing(dc, cx, cy, radius, radius * 2 / 3, fade, t < 80 ? 3 : 1);
    if (t > 45) DrawGlowRing(dc, cx, cy, radius * 3 / 4, radius / 2, MixColor(C_BG, fade, 40), 1);
    for (int i = 0; i < 8 + power; ++i) {
        uint32_t h = Hash3(seed, i, 71);
        int age = t - (i % 3) * 18;
        if (age < 0 || age >= 310) continue;
        int travel = EaseOutCubic(Track(age, 0, 310));
        int angle = (int)(h % 3600);
        int nearR = 4 + travel * (14 + power * 3) / 1000;
        int farR = nearR + (8 + power * 2) * (1000 - travel) / 1000;
        DrawLine(dc, cx + CosMille(angle) * nearR / 1000, cy + SinMille(angle) * nearR / 1000,
            cx + CosMille(angle) * farR / 1000, cy + SinMille(angle) * farR / 1000,
            MixColor(C_BG, i % 3 ? tone : C_TEXT, 90 * (310 - age) / 310), i % 3 ? 1 : 2);
    }
    if (t < 100) {
        int slash = (18 + power * 3) * (1000 - Track(t, 35, 100)) / 1000;
        DrawLine(dc, cx - slash, cy + slash / 2, cx + slash, cy - slash / 2, C_TEXT, t < 50 ? 3 : 1);
    }
}

inline void DrawShieldMesh(HDC dc, const RECT& area, int t, int life, COLORREF tone, int strength) {
    if (t < 0 || t >= life || strength <= 0) return;
    int fade = 1000 - Track(t, life / 3, life);
    int spread = EaseOutCubic(Track(t, 0, 180));
    int cx = (area.left + area.right) / 2, cy = (area.top + area.bottom) / 2;
    int rx = (area.right - area.left - 8) * spread / 2000;
    int ry = (area.bottom - area.top - 8) * spread / 2000;
    POINT v[7];
    for (int i = 0; i <= 6; ++i) {
        v[i].x = cx + CosMille(300 + i * 600) * rx / 1000;
        v[i].y = cy + SinMille(300 + i * 600) * ry / 1000;
        if (i) DrawLine(dc, v[i-1].x, v[i-1].y, v[i].x, v[i].y,
            MixColor(C_BG, tone, strength * fade / 1000), t < 100 ? 3 : 2);
    }
    for (int i = 0; i < 3; ++i)
        DrawLine(dc, v[i].x, v[i].y, v[i+3].x, v[i+3].y,
            MixColor(C_BG, tone, strength * fade / 5000), 1);
}

inline void DrawOrbitCorners(HDC dc, const RECT& r, int t, COLORREF color, int strength) {
    int breathe = (SinMille(t % 2400 * 3600 / 2400) + 1000) / 2;
    int length = 9 + breathe * 7 / 1000;
    COLORREF tone = MixColor(C_BG, color, strength);
    for (int i = 0; i < 4; ++i) {
        int x = (i & 1) ? r.right - 3 : r.left + 2, dx = (i & 1) ? -length : length;
        int y = (i & 2) ? r.bottom - 3 : r.top + 2, dy = (i & 2) ? -length : length;
        DrawLine(dc, x, y, x + dx, y, tone, 2);
        DrawLine(dc, x, y, x, y + dy, tone, 2);
    }
}
