#pragma once
#include "ui.h"
#include "fx_draw.h"

// Mechanical movements finish and settle. Recurring signals belong to a read
// head or live connection; copy and hit rectangles always stay put.
inline void DrawCardMotion(HDC dc, const RECT& r, COLORREF color, int index, int active) {
    if (!FxDecorOn()) return;
    int age = SceneElapsed() - index * 65;
    if (age >= 0 && age < 460) {
        int p = EaseOutCubic(Track(age, 0, 330)), fade = 1000 - Track(age, 180, 460);
        int x = Lerp(r.left + 2, r.right - 3, p);
        COLORREF tone = MixColor(C_PANEL, color, FxScale(65 * fade / 1000));
        Fill(dc, MakeRect(r.left + 2, r.top + 1, x, r.top + 3), tone);
        DrawLine(dc, x, r.top + 2, x, r.top + 6, tone, 1);
    }
    if (!active) return;
    int focus = Inside(r, gMouse.x, gMouse.y) ? UiFocusElapsed() : -1;
    int p = focus < 0 ? 1000 : EaseOutCubic(Track(focus, 35, 200));
    int length = 6 + p * 7 / 1000;
    COLORREF tone = MixColor(C_PANEL, color, FxScale(38 + p * 35 / 1000));
    DrawLine(dc, r.left + 2, r.top + 2, r.left + 2 + length, r.top + 2, tone, 2);
    DrawLine(dc, r.left + 2, r.top + 2, r.left + 2, r.top + 2 + length, tone, 2);
    DrawLine(dc, r.right - 3, r.bottom - 3, r.right - 3 - length, r.bottom - 3, tone, 2);
    DrawLine(dc, r.right - 3, r.bottom - 3, r.right - 3, r.bottom - 3 - length, tone, 2);
}

inline void DrawDriveRack(HDC dc, const RECT& r, int seed, COLORREF tone) {
    COLORREF metal = MixColor(C_BG, tone, FxScale(20));
    for (int side = 0; side < 2; ++side) {
        int x = side ? r.right + 6 : r.left - 7;
        DrawLine(dc, x, r.top + 10, x, r.bottom - 10, metal, 1);
        for (int y = r.top + 20; y < r.bottom - 10; y += 38)
            Fill(dc, MakeRect(x - 1, y, x + 2, y + 5), metal);
    }
    int tick = (int)((GetTickCount() + (DWORD)seed * 719u) % 4200u);
    int from = r.top + 18 + seed * 31 % 120, to = r.bottom - 24 - seed * 23 % 150;
    int head = tick < 2100 ? Lerp(from, to, EaseOutCubic(Track(tick, 0, 260)))
        : Lerp(to, from, EaseOutCubic(Track(tick, 2100, 2410)));
    int light = tick < 320 || (tick >= 2100 && tick < 2470) ? 60 : 22;
    Fill(dc, MakeRect(r.left - 10, head - 2, r.left - 3, head + 3), MixColor(C_BG, tone, FxScale(light)));
}

inline void DrawSceneField(HDC dc, int phase, COLORREF color, int width, int height) {
    if (!FxDecorOn()) return;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, 0, 70, width, height);
    COLORREF dim = MixColor(C_BG, color, FxScale(20));
    int age = SceneElapsed();
    if (phase == PHASE_TITLE) {
        DrawLine(dc, 36, 454, 260, 454, dim, 1);
        DrawLine(dc, width - 260, 454, width - 36, 454, dim, 1);
        for (int side = 0; side < 2; ++side) for (int i = 0; i < 18; ++i) {
            int x = side ? width - 42 : 42, y = 214 + i * 12;
            int length = 7 + (int)(Hash3(side, i, 90) % 13);
            DrawLine(dc, x, y, x + (side ? -length : length), y, MixColor(C_BG, color, FxScale(10)), 1);
        }
    } else if (phase == PHASE_DRIVE_SELECT) {
        for (int i = 0; i < gGame.driveChoiceCount; ++i) {
            int drive = gGame.driveChoices[i];
            DrawDriveRack(dc, DriveCardRect(i), drive, (COLORREF)DRIVE_INFO[drive].color);
        }
    } else if (phase == PHASE_DIRECTORY) {
        int y = 584, cx = width / 2;
        DrawLine(dc, cx, y, cx, y + 45, dim, 2);
        for (int i = 0; i < 2; ++i) {
            RECT r = DirectoryChoiceRect(i);
            POINT to = {(r.left + r.right) / 2, r.bottom + 2};
            DrawLine(dc, to.x, to.y, to.x, y, dim, 1);
            DrawLine(dc, to.x, y, cx, y, dim, 1);
            int lead = age - i * 110;
            if (lead >= 0 && lead < 760) {
                POINT from = {cx, y + 45};
                DrawSignalPath(dc, from, to, y, EaseOutCubic(Track(lead, 0, 650)), 2,
                    MixColor(C_BG, color, FxScale(55)), 7, 0);
            }
        }
    } else if (phase == PHASE_REWARD) {
        DrawLine(dc, 66, 292, width - 66, 292, dim, 1);
        for (int i = 0; i < REWARD_CARD_COUNT; ++i) {
            RECT r = RewardRect(i, width); int cx = (r.left + r.right) / 2;
            DrawLine(dc, cx, r.bottom + 1, cx, 292, dim, 1);
            if (age >= i * 75 && age < 850) {
                int p = EaseOutCubic(Track(age - i * 75, 0, 480));
                int x = Lerp(66, cx, p);
                Fill(dc, MakeRect(x - 3, 291, x + 4, 293), MixColor(C_BG, color, FxScale(60 * (850 - age) / 850)));
            }
        }
    } else if (phase == PHASE_PRUNE) {
        int used = UsedBytes(&gGame), capacity = EffectiveCapacity(&gGame);
        int filled = capacity > 0 ? (used * 64 + capacity - 1) / capacity : 64;
        if (filled > 64) filled = 64;
        for (int i = 0; i < 64; ++i) {
            int x = LEGACY_X + 152 + i * 10;
            Fill(dc, MakeRect(x, 226, x + 6, 237), MixColor(C_BG, color, FxScale(i < filled ? 45 : 10)));
        }
    } else if (phase == PHASE_STORY) {
        for (int side = 0; side < 2; ++side) {
            int x = side ? width - 102 : 96;
            DrawLine(dc, x + 3, 128, x + 3, height - 108, MixColor(C_BG, color, FxScale(10)), 1);
            for (int i = 0; i < 18; ++i) {
                int y = 133 + i * 28;
                Outline(dc, MakeRect(x, y, x + 7, y + 9), dim, 1);
            }
            if (age < 1100) {
                int y = Lerp(133, height - 120, EaseOutCubic(Track(age, 0, 1000)));
                Fill(dc, MakeRect(x - 2, y, x + 9, y + 3), MixColor(C_BG, color, FxScale(62)));
            }
        }
    } else if (phase == PHASE_ENDING_CHOICE) {
        for (int i = 0; i < ENDING_COUNT; ++i) {
            RECT r = EndingChoiceRect(i); int x = (r.left + r.right) / 2;
            COLORREF tone = i == 1 ? C_BLUE : i == 2 ? C_YELLOW : C_GREEN;
            DrawLine(dc, x, 256, x, r.top - 1, MixColor(C_BG, tone, FxScale(48)), 2);
            if (age < 850) {
                int span = 120 * EaseOutCubic(Track(age - i * 85, 0, 480)) / 1000;
                DrawLine(dc, x - span, 260, x + span, 260, MixColor(C_BG, tone, FxScale(45)), 1);
            }
        }
    } else if (phase == PHASE_CHAPTER_CLEAR || phase == PHASE_VICTORY) {
        int ending = phase == PHASE_CHAPTER_CLEAR ? 0 : gGame.story.selectedEnding;
        for (int side = 0; side < 2; ++side) for (int i = 0; i < 6; ++i) {
            int delay = i * 65 + side * 90;
            int settle = EaseOutCubic(Track(age, delay, delay + 800));
            int x = side ? width - 88 : 62, y = 262 + i * 30;
            int drift = (1000 - settle) * (12 + i * 5) / 1000;
            if (ending == 1) drift = settle * (i + 2) * 5 / 1000;
            x += side ? drift : -drift;
            COLORREF tone = MixColor(C_BG, color, FxScale(ending == 1 ? 48 - settle * 28 / 1000 : 26 + settle * 20 / 1000));
            Outline(dc, MakeRect(x, y, x + 26, y + 16), tone, 1);
            DrawLine(dc, x + 5, y + 5, x + 19, y + 5, tone, 1);
        }
    } else if (phase == PHASE_GAMEOVER) {
        int fade = 1000 - Track(age, 150, 1700);
        for (int i = 0; i < 18; ++i) {
            int x = LEGACY_X + 180 + i * 42, y = 526;
            int lift = SinMille(i * 410 + age % 60000 * 2) * fade / 70000;
            DrawLine(dc, x, y + lift, x + 34, y + lift,
                MixColor(C_BG, C_RED, FxScale(14 + fade * 28 / 1000)), 1);
        }
    }
    RestoreDC(dc, saved);
}

inline void DrawTitleDisk(HDC dc, int x, int y, int back, COLORREF tone) {
    if (!FxDecorOn()) return;
    int saved = SaveDC(dc);
    COLORREF edge = MixColor(C_BG, tone, FxScale(35));
    Panel(dc, MakeRect(x, y, x + 140, y + 158), MixColor(C_BG, tone, FxScale(10)), edge);
    Fill(dc, MakeRect(x + 126, y, x + 140, y + 8), C_BG);
    DrawLine(dc, x + 126, y, x + 140, y + 14, edge, 1);
    Panel(dc, MakeRect(x + 25, y + 1, x + 106, y + 48), MixColor(C_BG, C_TEXT, FxScale(12)), edge);
    Fill(dc, MakeRect(x + 73, y + 9, x + 92, y + 39), C_BG);
    Panel(dc, MakeRect(x + 14, y + 72, x + 126, y + 143), C_BG, edge);
    if (back) {
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 6; ++col) {
            int age = SceneElapsed() - (row * 6 + col) * 30;
            int lit = age >= 0 && age < 900;
            int left = x + 24 + col * 16, top = y + 83 + row * 17;
            Fill(dc, MakeRect(left, top, left + 10, top + 10), MixColor(C_BG, lit ? C_TEXT : tone, FxScale(lit ? 45 : 22)));
        }
    } else {
        Text(dc, x + 28, y + 82, L"A:", MixColor(C_BG, tone, FxScale(72)), gFontLarge);
        DrawLine(dc, x + 26, y + 120, x + 114, y + 120, edge, 1);
        DrawLine(dc, x + 26, y + 129, x + 88, y + 129, edge, 1);
    }
    Outline(dc, MakeRect(x + 8, y + 149, x + 17, y + 155), edge, 1);
    Fill(dc, MakeRect(x + 119, y + 150, x + 130, y + 154), C_BG);
    RestoreDC(dc, saved);
}

// 장면 도착. 화면 하나가 다음 화면으로 바뀌는 자리를 여기 한 곳에서 잇는다.
// PaintGame이 모든 화면 위에 같은 값으로 얹으므로 각 화면은 이 연출을 모른다 -
// 삽입 연출처럼 앞 연출이 밝게 끝나는 자리도 여기가 받아 내려놓는다.
//
// major는 화면 자체가 바뀐 도착이고(타이틀 → 기록 → 볼륨 → 전투), minor는 같은
// 화면 안에서 쪽만 넘긴 도착이다. 기록 한 쪽을 넘길 때마다 브라운관이 다시
// 열리면 읽는 흐름이 끊기므로, 여는 동작은 major에서만 한다.
#define SCENE_ARRIVE_OPEN_MS 200
#define SCENE_ARRIVE_MS      520

inline void DrawSceneArrival(HDC dc, COLORREF tone, int major = 1) {
    int t = SceneElapsed();
    if (!FxDecorOn() || t < 0 || t >= SCENE_ARRIVE_MS) return;
    int stageTop = 68, cy = (stageTop + BASE_HEIGHT) / 2, half = (BASE_HEIGHT - stageTop) / 2;
    // 브라운관이 켜진다. 가운데 한 줄에서 위아래로 벌어지고 바깥은 잉크로 덮여
    // 있다. 뒤의 판은 그동안에도 계속 그려져 있으므로 다 열린 순간 이어 붙는
    // 자리가 없다 - 열리는 것은 덮개뿐이다.
    int open = major ? EaseOutCubic(Track(t, 0, SCENE_ARRIVE_OPEN_MS)) : 1000;
    if (open < 1000) {
        int gap = half * open / 1000;
        // 덮개는 처음 한순간 아직 달아올라 있다. 삽입 연출이 캔버스를 하얗게
        // 삼키며 끝나는데 다음 판이 곧장 잉크로 시작하면 그 사이가 흰색에서
        // 검은색으로 한 프레임에 튀어, 가장 큰 사건 바로 뒤에 깜빡임이 남는다.
        COLORREF veil = MixColor(C_INK, C_TEXT, FxScale(26 * (1000 - Track(t, 0, 140)) / 1000));
        Fill(dc, MakeRect(0, stageTop, BASE_WIDTH, cy - gap), veil);
        Fill(dc, MakeRect(0, cy + gap, BASE_WIDTH, BASE_HEIGHT), veil);
        COLORREF lip = MixColor(C_INK, tone, FxScale(42 + 58 * (1000 - open) / 1000));
        Fill(dc, MakeRect(0, cy - gap - 2, BASE_WIDTH, cy - gap), lip);
        Fill(dc, MakeRect(0, cy + gap, BASE_WIDTH, cy + gap + 2), lip);
    }
    // 켜지는 순간의 번짐. 주사선 사이로만 밝히므로 판이 계속 보인다 - 통째로
    // 덮으면 이 채우기에는 알파가 없어 아무리 옅게 섞어도 판이 사라진다.
    int bloom = 1000 - Track(t, 0, major ? 300 : 150);
    if (bloom > 0) {
        COLORREF surge = MixColor(C_BG, C_TEXT, FxScale((major ? 44 : 18) * bloom / 1000));
        for (int y = stageTop + ((t / 40) & 1); y < BASE_HEIGHT; y += 3)
            Fill(dc, MakeRect(0, y, BASE_WIDTH, y + 1), surge);
    }
    // 머리띠를 훑고 지나가는 판독 헤드.
    int p = EaseOutCubic(Track(t, 0, 420)), fade = 1000 - Track(t, 160, SCENE_ARRIVE_MS);
    int head = Lerp(24, BASE_WIDTH - 24, p);
    DrawLine(dc, head - 18, 71, head, 71, MixColor(C_BG, tone, FxScale(50 * fade / 1000)), 1);
}

// Procedural reward emblems read as objects before the description is read.
// kind -1 is a resident chip; -2 is sector repair. No asset files or RNG.
inline void DrawRewardEmblem(HDC dc, const RECT& card, int kind, COLORREF tone) {
    int saved = SaveDC(dc);
    int x = card.left + 36, y = card.top + 70;
    RECT socket = MakeRect(x - 22, y - 22, x + 23, y + 23);
    Fill(dc, socket, MixColor(C_INK, tone, 12));
    Outline(dc, socket, MixColor(C_INK, tone, 34), 1);
    Fill(dc, MakeRect(card.left + 1, card.top + 1, card.right - 1, card.top + 4), MixColor(C_PANEL, tone, 65));
    if (kind == -2) {
        Fill(dc, MakeRect(x - 4, y - 14, x + 5, y + 15), tone);
        Fill(dc, MakeRect(x - 14, y - 4, x + 15, y + 5), tone);
    } else if (kind == -1) {
        Outline(dc, MakeRect(x - 10, y - 10, x + 11, y + 11), tone, 2);
        Fill(dc, MakeRect(x - 4, y - 4, x + 5, y + 5), tone);
        for (int i = -1; i <= 1; ++i) {
            DrawLine(dc, x - 16, y + i * 7, x - 11, y + i * 7, tone, 2);
            DrawLine(dc, x + 11, y + i * 7, x + 16, y + i * 7, tone, 2);
            DrawLine(dc, x + i * 7, y - 16, x + i * 7, y - 11, tone, 2);
            DrawLine(dc, x + i * 7, y + 11, x + i * 7, y + 16, tone, 2);
        }
    } else if (kind == FACE_SHIELD) {
        POINT p[] = {{x-14,y-12},{x+14,y-12},{x+11,y+6},{x,y+16},{x-11,y+6},{x-14,y-12}};
        for (int i = 0; i < 5; ++i) DrawLine(dc,p[i].x,p[i].y,p[i+1].x,p[i+1].y,tone,2);
        DrawLine(dc,x,y-8,x,y+10,tone,2);
    } else if (kind == FACE_FIRE) {
        POINT p[] = {{x+4,y-18},{x-11,y+1},{x-6,y+1},{x-9,y+16},{x+13,y-5},{x+4,y-5},{x+4,y-18}};
        for (int i = 0; i < 6; ++i) DrawLine(dc,p[i].x,p[i].y,p[i+1].x,p[i+1].y,tone,2);
    } else if (kind == FACE_BOOST) {
        for (int i = 0; i < 3; ++i) {
            int yy = y - 12 + i * 10;
            DrawLine(dc,x-12,yy+7,x,yy,tone,2); DrawLine(dc,x,yy,x+12,yy+7,tone,2);
        }
    } else if (kind == FACE_ECHO) {
        Outline(dc,MakeRect(x-15,y-13,x+7,y+9),MixColor(C_BG,tone,48),2);
        Outline(dc,MakeRect(x-6,y-4,x+16,y+18),tone,2);
    } else if (kind == FACE_LEECH) {
        DrawLine(dc,x,y-17,x-12,y+4,tone,2); DrawLine(dc,x,y-17,x+12,y+4,tone,2);
        DrawLine(dc,x-12,y+4,x-7,y+14,tone,2); DrawLine(dc,x+12,y+4,x+7,y+14,tone,2);
        DrawLine(dc,x-7,y+14,x+7,y+14,tone,2); DrawLine(dc,x,y-1,x,y+9,tone,2);
    } else if (kind == FACE_WILD) {
        DrawLine(dc,x,y-17,x+17,y,tone,2); DrawLine(dc,x+17,y,x,y+17,tone,2);
        DrawLine(dc,x,y+17,x-17,y,tone,2); DrawLine(dc,x-17,y,x,y-17,tone,2);
        Fill(dc,MakeRect(x-4,y-4,x+5,y+5),tone);
    } else {
        Outline(dc,MakeRect(x-14,y-14,x+15,y+15),tone,2);
        for (int i = -1; i <= 1; ++i) Fill(dc,MakeRect(x+i*8-2,y+i*8-2,x+i*8+3,y+i*8+3),tone);
    }
    RestoreDC(dc,saved);
}

inline void DrawRewardSocket(HDC dc, const RECT& r, COLORREF tone, int index, int tuned) {
    if (!FxDecorOn()) return;
    int age = SceneElapsed() - index * 75, cx = (r.left + r.right) / 2;
    int settle = EaseOutCubic(Track(age, 140, 470));
    for (int i = 0; i < 5; ++i) {
        int x = cx - 20 + i * 10, height = 3 + (1000 - settle) * (i % 2 ? 8 : 5) / 1000;
        Fill(dc, MakeRect(x, r.bottom + 1, x + 3, r.bottom + height), MixColor(C_BG, tone, FxScale(18 + settle * 20 / 1000)));
    }
    if (age >= 160 && age < 640) {
        int p = EaseOutCubic(Track(age, 160, 640)), reach = (tuned ? 80 : 50) * p / 1000;
        COLORREF flash = MixColor(C_BG, tone, FxScale(75 * (1000 - p) / 1000));
        DrawLine(dc, cx - reach, r.bottom + 8, cx - reach - 9, r.bottom + 8, flash, 1);
        DrawLine(dc, cx + reach, r.bottom + 8, cx + reach + 9, r.bottom + 8, flash, 1);
    }
}

inline void DrawInstallFilament(HDC dc, POINT from, POINT to, int t, int life, COLORREF tone, int seed) {
    if (!FxDecorOn() || t < 0 || t >= life || life < 140) return;
    for (int i = 0; i < FxScale(7); ++i) {
        int age = t - i * 13;
        if (age < 0) continue;
        int p = Track(age, 0, life - 90);
        p = p * p / 1000 * (3000 - 2 * p) / 1000;
        int arch = p * (1000 - p) / 1000;
        int bend = (int)(Hash3(seed, i, 611) % 61) - 30;
        int x = Lerp(from.x, to.x, p) + bend * arch / 250;
        int y = Lerp(from.y, to.y, p) - (24 + i * 3) * arch / 250;
        int fade = 1000 - Track(age, life - 130, life - 40);
        if (fade <= 0) continue;
        int size = 1 + (1000 - p) * 2 / 1000;
        COLORREF color = MixColor(C_BG, i ? tone : C_TEXT, FxScale((80 - i * 7) * fade / 1000));
        Fill(dc, MakeRect(x - size, y - 1, x + size + 1, y + 2), color);
    }
}
