#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include "game.h"
#include "sprites.h"

static GameState gGame;
static HWND gWindow;
static HFONT gFontSmall, gFontMedium, gFontLarge, gFontHuge;
static POINT gMouse;
static int gGuideOpen;

static const COLORREF C_BG = RGB(8, 12, 17), C_PANEL = RGB(16, 23, 31), C_PANEL_2 = RGB(23, 33, 43);
static const COLORREF C_LINE = RGB(50, 71, 87), C_TEXT = RGB(218, 232, 238), C_DIM = RGB(120, 145, 157);
static const COLORREF C_GREEN = RGB(82, 231, 174), C_RED = RGB(255, 92, 82), C_YELLOW = RGB(255, 204, 75), C_BLUE = RGB(83, 170, 255);
static const COLORREF C_INK = RGB(6, 10, 15);

static RECT MakeRect(int l, int t, int r, int b) { RECT value = {l, t, r, b}; return value; }
static int Inside(const RECT& rect, int x, int y) { POINT p = {x, y}; return PtInRect(&rect, p); }

static void Fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color); FillRect(dc, &rect, brush); DeleteObject(brush);
}

static void Outline(HDC dc, const RECT& rect, COLORREF color, int thickness) {
    HBRUSH brush = CreateSolidBrush(color); RECT r = rect;
    for (int i = 0; i < thickness; ++i) { FrameRect(dc, &r, brush); InflateRect(&r, -1, -1); }
    DeleteObject(brush);
}

static void Panel(HDC dc, const RECT& rect, COLORREF fillColor, COLORREF borderColor) {
    Fill(dc, rect, fillColor); Outline(dc, rect, borderColor, 1);
}

static void Text(HDC dc, int x, int y, const wchar_t* value, COLORREF color, HFONT font) {
    HFONT old = (HFONT)SelectObject(dc, font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    TextOutW(dc, x, y, value, lstrlenW(value)); SelectObject(dc, old);
}

static void TextRect(HDC dc, const RECT& rect, const wchar_t* value, COLORREF color, HFONT font, UINT flags) {
    HFONT old = (HFONT)SelectObject(dc, font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color); RECT r = rect;
    DrawTextW(dc, value, -1, &r, flags); SelectObject(dc, old);
}

static void Bar(HDC dc, const RECT& rect, int value, int maximum, COLORREF color) {
    Fill(dc, rect, RGB(39, 48, 57));
    if (maximum > 0 && value > 0) { RECT part = rect; part.right = part.left + (part.right - part.left) * value / maximum; Fill(dc, part, color); }
    Outline(dc, rect, C_LINE, 1);
}

static void FormatFace(const Face* face, wchar_t* out) {
    if (!face) lstrcpyW(out, L"--");
    else if (face->damaged) lstrcpyW(out, L"BAD");
    else if (face->kind == FACE_NUMBER) wsprintfW(out, L"%d", (int)face->value);
    else lstrcpyW(out, FACE_INFO[face->kind].shortName);
}

static COLORREF FaceColor(const Face* face) {
    if (!face) return C_DIM;
    if (face->damaged) return C_RED;
    return (COLORREF)FACE_INFO[face->kind].color;
}

static void AppendStatus(wchar_t* output, const wchar_t* status) {
    if (output[0]) lstrcatW(output, L" · ");
    lstrcatW(output, status);
}

#pragma pack(push, 1)
struct WaveMemory {
    char riff[4]; DWORD riffSize; char wave[4]; char fmt[4]; DWORD fmtSize; WORD format; WORD channels;
    DWORD sampleRate; DWORD byteRate; WORD blockAlign; WORD bits; char data[4]; DWORD dataSize; unsigned char samples[1800];
};
#pragma pack(pop)
static WaveMemory gWave;

static void CopyTag(char* destination, const char* source) { for (int i = 0; i < 4; ++i) destination[i] = source[i]; }
static void PlayTone(int frequency, int milliseconds) {
    const int rate = 11025; int count = rate * milliseconds / 1000; if (count > 1800) count = 1800; if (count < 16) count = 16;
    CopyTag(gWave.riff, "RIFF"); CopyTag(gWave.wave, "WAVE"); CopyTag(gWave.fmt, "fmt "); CopyTag(gWave.data, "data");
    gWave.riffSize = 36 + count; gWave.fmtSize = 16; gWave.format = 1; gWave.channels = 1; gWave.sampleRate = rate;
    gWave.byteRate = rate; gWave.blockAlign = 1; gWave.bits = 8; gWave.dataSize = count;
    int period = rate / frequency; if (period < 2) period = 2;
    for (int i = 0; i < count; ++i) {
        int amplitude = (i % period) < period / 2 ? 38 : -38; int fade = (count - i) * 255 / count;
        gWave.samples[i] = (unsigned char)(128 + amplitude * fade / 255);
    }
    PlaySoundA((LPCSTR)&gWave, 0, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static RECT GuideButtonRect(int width) { return MakeRect(width - 116, 12, width - 18, 54); }
static RECT GuideCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }

#define ROLL_BASE_MS 300
#define ROLL_STAGGER_MS 80
#define ROLL_FLASH_MS 130
#define ROLL_FLIPS 16

static DWORD gRollStart;
static int gRollActive, gRollLanded;
static int gRollFloor = -1, gRollEncounter = -1, gRollTurn = -1;

static int DieRollDuration(int die) { return ROLL_BASE_MS + die * ROLL_STAGGER_MS; }
static int RollElapsed() { return (int)(GetTickCount() - gRollStart); }

// Display-only noise. The real result already sits in gGame.dice[].rolledFace; this
// only picks which face is shown mid-tumble, so the seeded game RNG stays untouched.
static int RollNoiseFace(int die, int step) {
    uint32_t h = (uint32_t)die * 0x9E3779B1u + (uint32_t)step * 0x85EBCA6Bu;
    h ^= h >> 15; h *= 0x2545F491u; h ^= h >> 13;
    return (int)(h % 6u);
}

// 0..1000, eased out so face flips are dense at first and thin out as the die settles.
static int RollProgress(int die) {
    int duration = DieRollDuration(die), elapsed = RollElapsed();
    if (elapsed >= duration) return 1000;
    int p = elapsed * 1000 / duration;
    return p * (2000 - p) / 1000;
}

static int RollSettled(int die) { return !gRollActive || RollElapsed() >= DieRollDuration(die); }

static int RollBlocking() { return gRollActive && !RollSettled(2); }

static int RollFaceIndex(int die) {
    if (RollSettled(die)) return gGame.dice[die].rolledFace;
    return RollNoiseFace(die, RollProgress(die) * ROLL_FLIPS / 1000);
}

// 0..1000, non-zero only during the short pop right after a die lands.
static int RollFlash(int die) {
    if (!gRollActive) return 0;
    int since = RollElapsed() - DieRollDuration(die);
    if (since < 0 || since >= ROLL_FLASH_MS) return 0;
    return 1000 - since * 1000 / ROLL_FLASH_MS;
}

static int RollOffsetY(int die) {
    if (!gRollActive) return 0;
    if (RollSettled(die)) return -(RollFlash(die) * 5 / 1000);
    int amplitude = 9 * (1000 - RollElapsed() * 1000 / DieRollDuration(die)) / 1000;
    return ((RollProgress(die) * ROLL_FLIPS / 1000) & 1) ? -amplitude : amplitude;
}

static void StopRollAnimation() {
    if (!gRollActive) return;
    gRollActive = 0; KillTimer(gWindow, 1);
}

// Dice are rolled inside game.cpp, so detect a fresh roll by watching the turn identity.
static void SyncRollAnimation() {
    if (gGame.phase != PHASE_COMBAT) { StopRollAnimation(); gRollTurn = -1; return; }
    if (gGame.floor == gRollFloor && gGame.encounter == gRollEncounter && gGame.turn == gRollTurn) return;
    gRollFloor = gGame.floor; gRollEncounter = gGame.encounter; gRollTurn = gGame.turn;
    gRollStart = GetTickCount(); gRollActive = 1; gRollLanded = 0; SetTimer(gWindow, 1, 16, 0);
}

static void TickRollAnimation() {
    int elapsed = RollElapsed();
    for (int d = 0; d < 3; ++d) {
        if (!(gRollLanded & (1 << d)) && elapsed >= DieRollDuration(d)) { gRollLanded |= 1 << d; PlayTone(300 + d * 90, 30); }
    }
    if (elapsed >= DieRollDuration(2) + ROLL_FLASH_MS) StopRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static void DrawHeader(HDC dc, int width) {
    Fill(dc, MakeRect(0, 0, width, 68), RGB(10, 16, 22)); Fill(dc, MakeRect(0, 67, width, 68), C_GREEN);
    Text(dc, 24, 14, L"A:\\ROGUE", C_GREEN, gFontLarge);
    if (gGame.phase != PHASE_TITLE) {
        wchar_t b[128]; wsprintfW(b, L"FLOOR %d/3  ·  NODE %d/3  ·  TURN %d", gGame.floor + 1, gGame.encounter + 1, gGame.turn);
        Text(dc, 230, 14, b, C_TEXT, gFontMedium); wsprintfW(b, L"HP %d/%d", gGame.playerHp, gGame.playerMaxHp);
        Text(dc, width - 440, 14, b, gGame.playerHp <= 10 ? C_RED : C_TEXT, gFontMedium);
        wsprintfW(b, L"DECK %dB / %dB", DeckBytes(&gGame), EffectiveCapacity(&gGame));
        Text(dc, width - 285, 14, b, DeckBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontSmall);
    }
    RECT guide = GuideButtonRect(width); int hover = Inside(guide, gMouse.x, gMouse.y);
    Panel(dc, guide, gGuideOpen ? RGB(32, 82, 67) : hover ? RGB(27, 48, 52) : C_PANEL_2, gGuideOpen || hover ? C_GREEN : C_LINE);
    TextRect(dc, guide, L"GUIDE [F1]", gGuideOpen ? C_GREEN : C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static RECT StartButtonRect(int width, int height) { return MakeRect(width / 2 - 150, height / 2 + 92, width / 2 + 150, height / 2 + 154); }
static void DrawTitle(HDC dc, int width, int height) {
    for (int y = 70; y < height; y += 4) Fill(dc, MakeRect(0, y, width, y + 1), RGB(11, 17, 23));
    TextRect(dc, MakeRect(0, height / 2 - 170, width, height / 2 - 80), L"A:\\ROGUE", C_GREEN, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(120, height / 2 - 72, width - 120, height / 2 + 70),
        L"18개의 주사위 면이 당신의 덱이자 디스크입니다.\n강한 면은 더 많은 바이트를 차지합니다.\n층이 내려갈수록 줄어드는 용량 안에서 시스템을 복구하십시오.",
        C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    RECT start = StartButtonRect(width, height); int hover = Inside(start, gMouse.x, gMouse.y);
    Panel(dc, start, hover ? RGB(32, 82, 67) : C_PANEL_2, hover ? C_GREEN : C_LINE);
    TextRect(dc, start, L"[ NEW RUN ]", C_GREEN, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(150, height - 105, width - 150, height - 25),
        L"마우스 또는 1·2·3으로 주사위 선택  /  슬롯 클릭으로 배치  /  SPACE 실행  /  ESC 보상 건너뛰기",
        C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
}

static COLORREF MixColor(COLORREF from, COLORREF to, int amount) {
    int r = GetRValue(from) + (GetRValue(to) - GetRValue(from)) * amount / 100;
    int g = GetGValue(from) + (GetGValue(to) - GetGValue(from)) * amount / 100;
    int b = GetBValue(from) + (GetBValue(to) - GetBValue(from)) * amount / 100;
    return RGB(r, g, b);
}

// Sprite cells carry shading only; the enemy color from data.h supplies the hue.
static int SpriteCellColor(char cell, COLORREF base, COLORREF* out) {
    switch (cell) {
    case 'X': *out = MixColor(base, C_INK, 82); return 1;
    case '1': *out = MixColor(base, C_INK, 66); return 1;
    case '2': *out = MixColor(base, C_INK, 46); return 1;
    case '3': *out = MixColor(base, C_INK, 24); return 1;
    case '4': *out = base; return 1;
    case '5': *out = MixColor(base, RGB(255, 255, 255), 45); return 1;
    case 'o': *out = RGB(9, 13, 18); return 1;
    case 'W': *out = RGB(232, 242, 247); return 1;
    case 'e': *out = MixColor(base, RGB(255, 255, 232), 74); return 1;
    }
    return 0;
}

// Runs of identical cells collapse into one FillRect, so a portrait costs ~60 GDI calls.
static void DrawSpriteArt(HDC dc, const RECT& box, int kind, int alive, int flash, int bob) {
    const char* const* rows = ENEMY_SPRITES[kind];
    COLORREF base = (COLORREF)ENEMY_INFO[kind].color;
    int boxWidth = box.right - box.left, boxHeight = box.bottom - box.top;
    int scale = (boxWidth < boxHeight ? boxWidth : boxHeight) / SPRITE_SIZE; if (scale < 1) scale = 1;
    int originX = box.left + (boxWidth - scale * SPRITE_SIZE) / 2;
    int originY = box.top + (boxHeight - scale * SPRITE_SIZE) / 2 + bob;
    for (int y = 0; y < SPRITE_SIZE; ++y) {
        const char* row = rows[y];
        for (int x = 0; x < SPRITE_SIZE; ) {
            COLORREF color;
            if (!SpriteCellColor(row[x], base, &color)) { ++x; continue; }
            int end = x + 1; while (end < SPRITE_SIZE && row[end] == row[x]) ++end;
            if (!alive) color = MixColor(color, RGB(34, 40, 48), 74);
            else if (flash > 0) color = MixColor(color, RGB(255, 255, 255), flash * 78 / 1000);
            int top = originY + y * scale, bottom = top + scale;
            // A deleted enemy keeps its silhouette but loses every other pixel.
            if (alive) Fill(dc, MakeRect(originX + x * scale, top, originX + end * scale, bottom), color);
            else for (int px = x; px < end; ++px) if (((px + y) & 1) == 0) Fill(dc, MakeRect(originX + px * scale, top, originX + (px + 1) * scale, bottom), color);
            x = end;
        }
    }
}

static void DrawPortrait(HDC dc, const RECT& box, int kind, int alive, int selected, int flash, int bob) {
    Panel(dc, box, alive ? RGB(11, 17, 24) : RGB(13, 13, 15), selected ? (COLORREF)ENEMY_INFO[kind].color : C_LINE);
    for (int y = box.top + 2; y < box.bottom - 1; y += 4) Fill(dc, MakeRect(box.left + 1, y, box.right - 1, y + 1), RGB(8, 13, 19));
    if (alive) {
        int centerX = (box.left + box.right) / 2, shadow = box.bottom - 9;
        Fill(dc, MakeRect(centerX - 36, shadow, centerX + 36, shadow + 4), RGB(7, 11, 16));
        Fill(dc, MakeRect(centerX - 26, shadow + 4, centerX + 26, shadow + 6), RGB(9, 14, 20));
    }
    DrawSpriteArt(dc, box, kind, alive, flash, bob);
}

#define HIT_FLASH_MS 240

static int gEnemyShownHp[3];
static DWORD gEnemyHitAt[3];

// Combat resolves in one call inside game.cpp, so damage is detected by watching HP.
static void SyncEnemyDamage() {
    for (int i = 0; i < 3; ++i) {
        int hp = i < gGame.enemyCount ? gGame.enemies[i].hp : 0;
        if (hp < gEnemyShownHp[i]) gEnemyHitAt[i] = GetTickCount();
        gEnemyShownHp[i] = hp;
    }
}

static int EnemyHitFlash(int index) {
    if (!gEnemyHitAt[index]) return 0;
    int since = (int)(GetTickCount() - gEnemyHitAt[index]);
    if (since < 0 || since >= HIT_FLASH_MS) return 0;
    return 1000 - since * 1000 / HIT_FLASH_MS;
}

// Idle float, phase-shifted per slot so a group never breathes in sync.
static int EnemyBob(int index) {
    int phase = (int)((GetTickCount() / 110 + (DWORD)index * 5) % 12u);
    if (phase > 6) phase = 12 - phase;
    return 3 - phase;
}

static int gIdleActive;
static void SyncIdleAnimation() {
    int wanted = gGame.phase == PHASE_COMBAT && !gGuideOpen;
    if (wanted == gIdleActive) return;
    gIdleActive = wanted;
    if (wanted) SetTimer(gWindow, 2, 55, 0); else KillTimer(gWindow, 2);
}

static RECT EnemyRect(int i) { int left = 28 + i * 218; return MakeRect(left, 94, left + 198, 366); }
static RECT PortraitRect(const RECT& panel) { return MakeRect(panel.left + 31, panel.top + 8, panel.left + 167, panel.top + 132); }
static RECT SlotRect(int i) { int left = 28 + i * 172; return MakeRect(left, 408, left + 154, 532); }
static RECT DieRect(int i) { int left = 48 + i * 220; return MakeRect(left, 574, left + 184, 695); }
static RECT EndTurnRect() { return MakeRect(712, 616, 884, 679); }
static int DieForSlotUI(int slot) { for (int d = 0; d < 3; ++d) if (gGame.dice[d].assignedSlot == slot) return d; return -1; }

static void DrawEnemy(HDC dc, int index) {
    const EnemyState* enemy = &gGame.enemies[index]; const EnemyInfo* info = &ENEMY_INFO[enemy->kind]; RECT r = EnemyRect(index);
    int selected = index == gGame.targetEnemy && enemy->alive;
    Panel(dc, r, enemy->alive ? C_PANEL : RGB(18, 18, 20), selected ? C_YELLOW : C_LINE);
    DrawPortrait(dc, PortraitRect(r), enemy->kind, enemy->alive, selected, EnemyHitFlash(index), enemy->alive ? EnemyBob(index) : 0);
    Text(dc, r.left + 12, r.top + 140, info->code, enemy->alive ? (COLORREF)info->color : C_DIM, gFontMedium);
    Text(dc, r.left + 12, r.top + 165, info->name, C_DIM, gFontSmall);
    wchar_t b[80]; wsprintfW(b, L"HP %d / %d", enemy->hp, enemy->maxHp); Text(dc, r.left + 12, r.top + 187, b, C_TEXT, gFontSmall);
    Bar(dc, MakeRect(r.left + 12, r.top + 208, r.right - 12, r.top + 220), enemy->hp, enemy->maxHp, (COLORREF)info->color);
    if (enemy->alive) {
        wsprintfW(b, L"의도: %s %d", INTENT_NAMES[enemy->intent], enemy->intentValue);
        Text(dc, r.left + 12, r.top + 227, b, enemy->intent == INTENT_HEAVY || enemy->intent == INTENT_CORRUPT ? C_RED : C_YELLOW, gFontSmall);
        if (enemy->block > 0 || enemy->burn > 0) { wsprintfW(b, L"BLOCK %d   BURN %d", enemy->block, enemy->burn); Text(dc, r.left + 12, r.top + 247, b, C_DIM, gFontSmall); }
    } else Text(dc, r.left + 12, r.top + 227, L"[ DELETED ]", C_DIM, gFontSmall);
}

static void DrawSlot(HDC dc, int slot) {
    RECT r = SlotRect(slot); int die = DieForSlotUI(slot); int hover = Inside(r, gMouse.x, gMouse.y);
    Panel(dc, r, hover ? RGB(23, 39, 48) : C_PANEL, hover ? C_GREEN : C_LINE);
    Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], slot == SLOT_ATTACK ? C_RED : slot == SLOT_DEFEND ? C_BLUE : C_GREEN, gFontMedium);
    if (die >= 0) {
        const Face* face = RolledFace(&gGame, die); wchar_t value[24]; FormatFace(face, value);
        TextRect(dc, MakeRect(r.left + 5, r.top + 42, r.right - 5, r.top + 83), value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        wchar_t b[48]; wsprintfW(b, L"DIE %d · %dB", die + 1, FaceCost(face));
        TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6), b, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    } else TextRect(dc, MakeRect(r.left + 5, r.top + 48, r.right - 5, r.bottom - 10), L"EMPTY", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
}

static void DrawDie(HDC dc, int index) {
    RECT r = DieRect(index); const DieState* die = &gGame.dice[index];
    const Face* face = &gGame.dice[index].faces[RollFaceIndex(index)];
    int rolling = !RollSettled(index), flash = RollFlash(index), offset = RollOffsetY(index);
    int selected = gGame.selectedDie == index, hover = Inside(r, gMouse.x, gMouse.y);
    Panel(dc, r, selected ? RGB(26, 48, 49) : C_PANEL, selected ? C_GREEN : hover ? C_BLUE : C_LINE);
    if (flash > 0) Outline(dc, r, FaceColor(face), 2);
    wchar_t b[64]; wsprintfW(b, L"DIE %d", index + 1); Text(dc, r.left + 10, r.top + 8, b, selected ? C_GREEN : C_TEXT, gFontSmall);
    wchar_t value[24]; FormatFace(face, value);
    TextRect(dc, MakeRect(r.left + 10, r.top + 27 + offset, r.right - 10, r.top + 70 + offset), value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT statusRect = MakeRect(r.left + 7, r.top + 74, r.right - 7, r.bottom - 5);
    if (rolling) { TextRect(dc, statusRect, L"ROLLING", C_BLUE, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE); return; }
    wchar_t statuses[64] = L""; int statusCount = 0;
    if (face && face->damaged) { AppendStatus(statuses, L"BAD"); ++statusCount; }
    if (die->unstable) { AppendStatus(statuses, L"READ ERROR"); ++statusCount; }
    if (die->disabled) { AppendStatus(statuses, L"FRAGMENTED"); ++statusCount; }
    if (statusCount == 1) TextRect(dc, statusRect, statuses, C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    else if (statusCount > 1) TextRect(dc, statusRect, statuses, C_RED, gFontSmall, DT_CENTER | DT_WORDBREAK);
    else {
        wsprintfW(b, L"%s · %dB", FACE_INFO[face->kind].name, FaceCost(face));
        TextRect(dc, statusRect, b, C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void DrawSidebar(HDC dc, int width, int height) {
    RECT side = MakeRect(width - 212, 94, width - 22, height - 22); Panel(dc, side, C_PANEL, C_LINE);
    Text(dc, side.left + 12, side.top + 12, L"DISK DAMAGE", C_RED, gFontSmall);
    Text(dc, side.left + 12, side.top + 42, MODIFIER_INFO[gGame.modifierA].name, C_YELLOW, gFontSmall);
    TextRect(dc, MakeRect(side.left + 12, side.top + 65, side.right - 10, side.top + 124), MODIFIER_INFO[gGame.modifierA].description, C_DIM, gFontSmall, DT_WORDBREAK);
    Text(dc, side.left + 12, side.top + 136, MODIFIER_INFO[gGame.modifierB].name, C_YELLOW, gFontSmall);
    TextRect(dc, MakeRect(side.left + 12, side.top + 159, side.right - 10, side.top + 222), MODIFIER_INFO[gGame.modifierB].description, C_DIM, gFontSmall, DT_WORDBREAK);
    Text(dc, side.left + 12, side.top + 242, L"EXEC ORDER", C_GREEN, gFontSmall);
    TextRect(dc, MakeRect(side.left + 12, side.top + 268, side.right - 10, side.top + 320), L"AMP > ATK > DEF > CHAIN", C_TEXT, gFontSmall, DT_WORDBREAK);
    const Face* selectedFace = gGame.selectedDie >= 0 ? RolledFace(&gGame, gGame.selectedDie) : 0;
    if (selectedFace) {
        Text(dc, side.left + 12, side.top + 340, FACE_INFO[selectedFace->kind].name, FaceColor(selectedFace), gFontMedium);
        TextRect(dc, MakeRect(side.left + 12, side.top + 372, side.right - 10, side.top + 430), FACE_INFO[selectedFace->kind].description, C_DIM, gFontSmall, DT_WORDBREAK);
    }
    Text(dc, side.left + 12, side.bottom - 112, L"SYSTEM LOG", C_GREEN, gFontSmall);
    for (int i = 0; i < 3; ++i) TextRect(dc, MakeRect(side.left + 12, side.bottom - 88 + i * 25, side.right - 8, side.bottom - 66 + i * 25), gGame.logs[i], i == 0 ? C_TEXT : C_DIM, gFontSmall, DT_END_ELLIPSIS | DT_SINGLELINE);
}

static void DrawCombat(HDC dc, int width, int height) {
    SyncEnemyDamage();
    for (int i = 0; i < gGame.enemyCount; ++i) DrawEnemy(dc, i);
    Text(dc, 28, 382, L"주사위 하나당 슬롯 하나 · 한 슬롯은 비워집니다", C_DIM, gFontSmall);
    for (int i = 0; i < SLOT_COUNT; ++i) DrawSlot(dc, i); for (int i = 0; i < 3; ++i) DrawDie(dc, i);
    RECT end = EndTurnRect(); int hover = Inside(end, gMouse.x, gMouse.y); Panel(dc, end, hover ? RGB(71, 42, 42) : C_PANEL_2, hover ? C_RED : C_LINE);
    TextRect(dc, end, L"EXECUTE [SPACE]", C_RED, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE); DrawSidebar(dc, width, height);
}

static RECT RewardRect(int i, int width) {
    int cardWidth = 220, gap = 28, total = cardWidth * 3 + gap * 2; int left = (width - total) / 2 + i * (cardWidth + gap);
    return MakeRect(left, 112, left + cardWidth, 260);
}
static RECT FaceGridRect(int die, int face) { int left = 150 + face * 112, top = 350 + die * 90; return MakeRect(left, top, left + 98, top + 68); }
static RECT ContinueRect(int width, int height) { return MakeRect(width - 276, height - 94, width - 42, height - 38); }

static void DrawFaceGrid(HDC dc, int mode) {
    for (int d = 0; d < 3; ++d) {
        wchar_t label[24]; wsprintfW(label, L"DIE %d", d + 1); Text(dc, 56, 371 + d * 90, label, C_GREEN, gFontMedium);
        for (int f = 0; f < 6; ++f) {
            RECT r = FaceGridRect(d, f); const Face* face = &gGame.dice[d].faces[f]; int hover = Inside(r, gMouse.x, gMouse.y);
            COLORREF border = hover && mode ? (mode == 2 ? C_RED : C_GREEN) : C_LINE; Panel(dc, r, hover ? RGB(28, 39, 48) : C_PANEL, border);
            wchar_t value[24]; FormatFace(face, value); TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 37), value, FaceColor(face), gFontMedium, DT_CENTER | DT_SINGLELINE);
            wchar_t bytes[24]; wsprintfW(bytes, L"%dB", FaceCost(face)); TextRect(dc, MakeRect(r.left + 4, r.bottom - 23, r.right - 4, r.bottom - 4), bytes, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
    }
}

static void DrawReward(HDC dc, int width, int height) {
    TextRect(dc, MakeRect(0, 78, width, 106), L"보상 하나를 고른 뒤 교체할 주사위 면을 클릭하십시오", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    for (int i = 0; i < 3; ++i) {
        RECT r = RewardRect(i, width); int selected = gGame.selectedReward == i, hover = Inside(r, gMouse.x, gMouse.y), kind = gGame.rewardKinds[i];
        Panel(dc, r, selected ? RGB(31, 55, 48) : C_PANEL, selected ? C_GREEN : hover ? C_BLUE : C_LINE);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), FACE_INFO[kind].name, (COLORREF)FACE_INFO[kind].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[48]; int cost = kind == FACE_NUMBER ? gGame.rewardValues[i] : FACE_INFO[kind].cost;
        wsprintfW(b, L"POWER %d  ·  %dB", gGame.rewardValues[i], cost); TextRect(dc, MakeRect(r.left + 8, r.top + 58, r.right - 8, r.top + 82), b, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 16, r.top + 92, r.right - 16, r.bottom - 12), FACE_INFO[kind].description, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
    }
    Text(dc, 56, 304, L"CURRENT DISK FACES", C_GREEN, gFontSmall); DrawFaceGrid(dc, gGame.selectedReward >= 0 ? 1 : 0);
    RECT skip = ContinueRect(width, height); Panel(dc, skip, C_PANEL_2, C_LINE); TextRect(dc, skip, L"SKIP [ESC]", C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawPrune(HDC dc, int width, int height) {
    wchar_t b[160]; wsprintfW(b, L"FLOOR %d 진입 한도: %dB  ·  현재: %dB", gGame.floor + 1, EffectiveCapacity(&gGame), DeckBytes(&gGame));
    TextRect(dc, MakeRect(0, 92, width, 132), b, DeckBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(80, 145, width - 80, 218), L"면을 클릭하면 EMPTY(0B)로 삭제됩니다. 손상 면도 원래 비용을 차지합니다.\n한도 이하가 되면 다음 층으로 진행할 수 있습니다.", C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    DrawFaceGrid(dc, 2); RECT confirm = ContinueRect(width, height); int ready = DeckBytes(&gGame) <= EffectiveCapacity(&gGame) && NonEmptyFaceCount(&gGame) > 0;
    Panel(dc, confirm, ready ? RGB(28, 70, 57) : C_PANEL_2, ready ? C_GREEN : C_LINE); TextRect(dc, confirm, L"CONTINUE [ENTER]", ready ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawEndScreen(HDC dc, int width, int height, int victory) {
    TextRect(dc, MakeRect(0, height / 2 - 150, width, height / 2 - 70), victory ? L"DISK RECOVERED" : L"SYSTEM FAILURE", victory ? C_GREEN : C_RED, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    wchar_t b[192]; wsprintfW(b, L"전투 %d회 완료  ·  면 %d개 설치  ·  최종 덱 %dB\n\nR 또는 ENTER로 새 런", gGame.combatsWon, gGame.facesInstalled, DeckBytes(&gGame));
    TextRect(dc, MakeRect(120, height / 2 - 40, width - 120, height / 2 + 110), b, C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
}

static void DrawGuide(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"SYSTEM GUIDE", C_GREEN, gFontLarge);
    Text(dc, panel.left + 255, panel.top + 28, L"전투 중에도 F1로 열고 닫을 수 있습니다.", C_DIM, gFontSmall);
    RECT close = GuideCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int left = panel.left + 30, middle = width / 2 + 12, top = panel.top + 76;
    Text(dc, left, top, L"빠른 시작", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 32, middle - 28, top + 126),
        L"1. 주사위를 클릭하거나 1·2·3으로 선택\n2. 서로 다른 슬롯을 클릭해 배치\n3. 적을 클릭해 공격 대상 선택\n4. SPACE로 턴 실행", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, left, top + 140, L"슬롯 실행 순서", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 172, middle - 28, top + 286),
        L"AMP  공격·방어 출력을 먼저 강화\nATK  선택한 적에게 피해\nDEF  이번 턴 적 공격을 흡수\nCHAIN  직전 공격 또는 방어를 반복", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, left, top + 300, L"상태와 적 의도", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 332, middle - 28, panel.bottom - 24),
        L"BURN: 적 행동 직전에 3 피해\nREAD ERROR: SPACE 순간 해당 주사위 재굴림\nFRAGMENTED: 중복 결과, 이번 턴 출력 0\n복합 상태는 주사위에 모두 함께 표시\n오염(관통): 표시 수치만큼 HP 직접 피해. BLOCK을 소모하거나 적용받지 않음", C_TEXT, gFontSmall, DT_WORDBREAK);

    Text(dc, middle, top, L"디스크 손상", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 32, panel.right - 28, top + 190),
        L"배드 섹터  층 이동 시 무작위 면 손상\n읽기 오류  경고 주사위가 실행 순간 재굴림\n조각화  같은 결과 중 뒤쪽 주사위 비활성화\n과잉 할당  용량 +60B, 적 HP +30%\n체크섬  굴림 합이 짝수면 공격 +2", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 204, L"덱·보상·용량", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 236, panel.right - 28, top + 350),
        L"18개 면의 비용 합이 DECK 용량입니다. 전투 뒤 보상 면을 골라 기존 면과 교체합니다. 현재 층 및 다음 층 한도를 넘으면 EMPTY(0B)가 되도록 면을 삭제해야 합니다.", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 364, L"조작", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 396, panel.right - 28, panel.bottom - 24),
        L"클릭 / 1·2·3  선택\nSPACE  턴 실행\nESC  배치 해제·보상 건너뛰기·가이드 닫기\nENTER  용량 정리 확정\nF1  가이드 열기·닫기", C_TEXT, gFontSmall, DT_WORDBREAK);
}

static void PaintGame(HWND window) {
    PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint); RECT client; GetClientRect(window, &client); int width = client.right, height = client.bottom;
    HDC memory = CreateCompatibleDC(dc); HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height); HBITMAP old = (HBITMAP)SelectObject(memory, bitmap);
    SyncIdleAnimation();
    Fill(memory, client, C_BG); DrawHeader(memory, width);
    if (gGame.phase == PHASE_TITLE) DrawTitle(memory, width, height); else if (gGame.phase == PHASE_COMBAT) DrawCombat(memory, width, height);
    else if (gGame.phase == PHASE_REWARD) DrawReward(memory, width, height); else if (gGame.phase == PHASE_PRUNE) DrawPrune(memory, width, height);
    else if (gGame.phase == PHASE_GAMEOVER) DrawEndScreen(memory, width, height, 0); else if (gGame.phase == PHASE_VICTORY) DrawEndScreen(memory, width, height, 1);
    if (gGuideOpen) DrawGuide(memory, width, height);
    BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY); SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); EndPaint(window, &paint);
}

static void BeginNewRun() { NewRun(&gGame, GetTickCount() ^ (uint32_t)(ULONG_PTR)gWindow); PlayTone(520, 90); InvalidateRect(gWindow, 0, FALSE); }

static void ClickCombat(int x, int y) {
    for (int i = 0; i < gGame.enemyCount; ++i) if (Inside(EnemyRect(i), x, y)) { SelectEnemy(&gGame, i); PlayTone(330, 45); return; }
    for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) { gGame.selectedDie = i; PlayTone(480 + i * 60, 35); return; }
    for (int i = 0; i < SLOT_COUNT; ++i) if (Inside(SlotRect(i), x, y)) {
        if (gGame.selectedDie >= 0) { AssignDieToSlot(&gGame, gGame.selectedDie, i); PlayTone(640 + i * 45, 45); }
        else { int die = DieForSlotUI(i); if (die >= 0) gGame.selectedDie = die; } return;
    }
    if (Inside(EndTurnRect(), x, y)) {
        GamePhase before = gGame.phase; EndTurn(&gGame);
        if (gGame.phase == PHASE_GAMEOVER) PlayTone(130, 180); else if (gGame.phase == PHASE_VICTORY) PlayTone(880, 180);
        else if (before != gGame.phase) PlayTone(760, 90); else PlayTone(260, 55);
    }
}

static void ClickReward(int x, int y) {
    RECT client; GetClientRect(gWindow, &client);
    for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, client.right), x, y)) { SelectReward(&gGame, i); PlayTone(600 + i * 80, 50); return; }
    if (gGame.selectedReward >= 0) for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) { InstallSelectedReward(&gGame, d, f); PlayTone(760, 85); return; }
    if (Inside(ContinueRect(client.right, client.bottom), x, y)) { SkipReward(&gGame); PlayTone(350, 50); }
}

static void ClickPrune(int x, int y) {
    RECT client; GetClientRect(gWindow, &client);
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) { PruneFace(&gGame, d, f); PlayTone(180, 65); return; }
    if (Inside(ContinueRect(client.right, client.bottom), x, y)) { ConfirmPrune(&gGame); PlayTone(560, 60); }
}

static void HandleClick(int x, int y) {
    RECT client; GetClientRect(gWindow, &client);
    if (gGuideOpen) {
        if (Inside(GuideCloseRect(client.right), x, y) || Inside(GuideButtonRect(client.right), x, y)) gGuideOpen = 0;
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(GuideButtonRect(client.right), x, y)) { gGuideOpen = 1; InvalidateRect(gWindow, 0, FALSE); return; }
    if (RollBlocking()) { StopRollAnimation(); InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGame.phase == PHASE_TITLE) { if (Inside(StartButtonRect(client.right, client.bottom), x, y)) BeginNewRun(); }
    else if (gGame.phase == PHASE_COMBAT) ClickCombat(x, y); else if (gGame.phase == PHASE_REWARD) ClickReward(x, y);
    else if (gGame.phase == PHASE_PRUNE) ClickPrune(x, y); else BeginNewRun();
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static void HandleKey(WPARAM key) {
    if (key == VK_F1) { gGuideOpen = !gGuideOpen; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) { if (key == VK_ESCAPE) gGuideOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (RollBlocking()) { StopRollAnimation(); InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGame.phase == PHASE_TITLE) { if (key == VK_RETURN || key == VK_SPACE) BeginNewRun(); }
    else if (gGame.phase == PHASE_COMBAT) {
        if (key >= '1' && key <= '3') { gGame.selectedDie = (int)(key - '1'); PlayTone(480 + gGame.selectedDie * 60, 35); }
        else if (key == VK_SPACE) {
            GamePhase before = gGame.phase; EndTurn(&gGame);
            if (gGame.phase == PHASE_GAMEOVER) PlayTone(130, 180); else if (gGame.phase == PHASE_VICTORY) PlayTone(880, 180);
            else if (before != gGame.phase) PlayTone(760, 90); else PlayTone(260, 55);
        } else if (key == VK_ESCAPE && gGame.selectedDie >= 0) UnassignDie(&gGame, gGame.selectedDie);
    } else if (gGame.phase == PHASE_REWARD) {
        if (key >= '1' && key <= '3') SelectReward(&gGame, (int)(key - '1')); else if (key == VK_ESCAPE) SkipReward(&gGame);
    } else if (gGame.phase == PHASE_PRUNE) { if (key == VK_RETURN) ConfirmPrune(&gGame); }
    else if (key == 'R' || key == VK_RETURN) BeginNewRun();
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        gFontSmall = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        gFontMedium = CreateFontW(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        gFontLarge = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        gFontHuge = CreateFontW(62, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas"); return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO* info = (MINMAXINFO*)lParam; info->ptMinTrackSize.x = 1136; info->ptMinTrackSize.y = 799; return 0; }
    case WM_MOUSEMOVE: gMouse.x = GET_X_LPARAM(lParam); gMouse.y = GET_Y_LPARAM(lParam); InvalidateRect(window, 0, FALSE); return 0;
    case WM_LBUTTONDOWN: HandleClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
    case WM_KEYDOWN: if ((lParam & (1u << 30)) == 0) HandleKey(wParam); return 0;
    case WM_TIMER: if (wParam == 1u) TickRollAnimation(); else if (wParam == 2u) InvalidateRect(window, 0, FALSE); return 0;
    case WM_PAINT: PaintGame(window); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        KillTimer(window, 1); KillTimer(window, 2);
        if (gFontSmall) DeleteObject(gFontSmall); if (gFontMedium) DeleteObject(gFontMedium); if (gFontLarge) DeleteObject(gFontLarge); if (gFontHuge) DeleteObject(gFontHuge);
        PlaySoundW(0, 0, 0); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware(); InitTitle(&gGame); WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure; wc.hInstance = instance; wc.hCursor = LoadCursorW(0, IDC_ARROW); wc.hIcon = LoadIconW(0, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszClassName = L"ARogueWindowClass"; if (!RegisterClassExW(&wc)) return 1;
    RECT desired = {0, 0, 1120, 760}; AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0); int width = desired.right - desired.left, height = desired.bottom - desired.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    gWindow = CreateWindowExW(0, wc.lpszClassName, L"A:\\ROGUE · 1.44MB", WS_OVERLAPPEDWINDOW, x, y, width, height, 0, 0, instance, 0);
    if (!gWindow) return 2; ShowWindow(gWindow, showCommand); UpdateWindow(gWindow);
    MSG message; while (GetMessageW(&message, 0, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return (int)message.wParam;
}
