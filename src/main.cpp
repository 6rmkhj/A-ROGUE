#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include "game.h"
#include "sprites.h"

static GameState gGame;
static HWND gWindow;
static HFONT gFontSmall, gFontMedium, gFontLarge, gFontHuge;
static POINT gMouse;
static int gHoverId = -1;
static int gGuideOpen;
static int gSettingsOpen;
static int gFullscreen;
static int gWindowedScale = 100;
static RECT gWindowedRect;

static const int BASE_WIDTH = 1120, BASE_HEIGHT = 760;
#define SETTINGS_SCALE_COUNT 5
static const int SCALE_OPTIONS[SETTINGS_SCALE_COUNT] = {75, 100, 125, 150, 200};

static const COLORREF C_BG = RGB(8, 12, 17), C_PANEL = RGB(16, 23, 31), C_PANEL_2 = RGB(23, 33, 43);
static const COLORREF C_LINE = RGB(50, 71, 87), C_TEXT = RGB(218, 232, 238), C_DIM = RGB(120, 145, 157);
static const COLORREF C_GREEN = RGB(82, 231, 174), C_RED = RGB(255, 92, 82), C_YELLOW = RGB(255, 204, 75), C_BLUE = RGB(83, 170, 255);
static const COLORREF C_INK = RGB(6, 10, 15);

static RECT MakeRect(int l, int t, int r, int b) { RECT value = {l, t, r, b}; return value; }
static int Inside(const RECT& rect, int x, int y) { POINT p = {x, y}; return PtInRect(&rect, p); }

// 실제 창 크기(clientW x clientH) 안에 BASE_WIDTH x BASE_HEIGHT 디자인 캔버스를
// 비율을 유지한 채로 최대한 맞춰 넣었을 때의 배율과 여백(레터박스)을 계산한다.
static void ComputeCanvasTransform(int clientW, int clientH, float* scale, int* offsetX, int* offsetY) {
    float scaleX = clientW > 0 ? (float)clientW / BASE_WIDTH : 1.0f;
    float scaleY = clientH > 0 ? (float)clientH / BASE_HEIGHT : 1.0f;
    float s = scaleX < scaleY ? scaleX : scaleY;
    if (s < 0.05f) s = 0.05f;
    *scale = s;
    *offsetX = (int)((clientW - BASE_WIDTH * s) / 2.0f);
    *offsetY = (int)((clientH - BASE_HEIGHT * s) / 2.0f);
}

// 실제 창(스크린) 좌표를 디자인 캔버스 좌표로 역변환한다. 모든 Rect()/Inside() 판정은
// 여전히 BASE_WIDTH x BASE_HEIGHT 기준으로 짜여 있으므로, 입력 좌표만 여기서 맞춰준다.
static POINT ScreenToCanvas(HWND window, int x, int y) {
    RECT client; GetClientRect(window, &client);
    float scale; int offsetX, offsetY;
    ComputeCanvasTransform(client.right, client.bottom, &scale, &offsetX, &offsetY);
    POINT p; p.x = (int)((x - offsetX) / scale); p.y = (int)((y - offsetY) / scale);
    return p;
}

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
    else if (face->damaged) lstrcpyW(out, L"손상");
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

static RECT GuideButtonRect(int width) { return MakeRect(width - 148, 6, width - 18, 33); }
static RECT GuideCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
static RECT SettingsButtonRect(int width) { return MakeRect(width - 148, 36, width - 18, 63); }
static RECT SettingsCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
static RECT ScaleOptionRect(int index) { int left = 84 + index * 130; return MakeRect(left, 260, left + 112, 302); }
static RECT FullscreenToggleRect() { return MakeRect(84, 380, 364, 422); }

// 창 모드로 되돌아갈 때 복원할 위치/크기를 저장해 두고, 모니터 전체를 덮는 테두리 없는 창으로 전환한다.
static void ApplyFullscreen(int enable) {
    if (enable == gFullscreen) return;
    if (enable) {
        GetWindowRect(gWindow, &gWindowedRect);
        MONITORINFO info; info.cbSize = sizeof(info);
        GetMonitorInfoW(MonitorFromWindow(gWindow, MONITOR_DEFAULTTOPRIMARY), &info);
        SetWindowLongPtrW(gWindow, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(gWindow, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
            info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top, SWP_FRAMECHANGED);
        gFullscreen = 1;
    } else {
        SetWindowLongPtrW(gWindow, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(gWindow, HWND_TOP, gWindowedRect.left, gWindowedRect.top,
            gWindowedRect.right - gWindowedRect.left, gWindowedRect.bottom - gWindowedRect.top, SWP_FRAMECHANGED);
        gFullscreen = 0;
    }
}

// BASE_WIDTH x BASE_HEIGHT 캔버스를 percent%로 표시할 창 크기를 계산해 적용한다 (창 모드에서만 의미가 있다).
static void ApplyWindowedScale(int percent) {
    gWindowedScale = percent;
    if (gFullscreen) ApplyFullscreen(0);
    RECT desired = {0, 0, BASE_WIDTH * percent / 100, BASE_HEIGHT * percent / 100};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int width = desired.right - desired.left, height = desired.bottom - desired.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    SetWindowPos(gWindow, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED);
}

static void DrawSettings(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"설정", C_GREEN, gFontLarge);
    RECT close = SettingsCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    Text(dc, 84, 228, L"화면 배율", C_YELLOW, gFontMedium);
    for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) {
        RECT r = ScaleOptionRect(i); int active = !gFullscreen && gWindowedScale == SCALE_OPTIONS[i]; int hover = Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, active ? RGB(28, 70, 57) : hover ? RGB(28, 39, 48) : C_PANEL_2, active ? C_GREEN : hover ? C_BLUE : C_LINE);
        wchar_t label[16]; wsprintfW(label, L"%d%%", SCALE_OPTIONS[i]);
        TextRect(dc, r, label, active ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(84, 312, panel.right - 30, 336), L"전체화면에서는 적용되지 않습니다.", C_DIM, gFontSmall, DT_SINGLELINE);

    Text(dc, 84, 348, L"전체화면", C_YELLOW, gFontMedium);
    RECT fs = FullscreenToggleRect(); int hoverFs = Inside(fs, gMouse.x, gMouse.y);
    Panel(dc, fs, gFullscreen ? RGB(28, 70, 57) : hoverFs ? RGB(28, 39, 48) : C_PANEL_2, gFullscreen ? C_GREEN : hoverFs ? C_BLUE : C_LINE);
    TextRect(dc, fs, gFullscreen ? L"전체화면 끄기" : L"전체화면 켜기", gFullscreen ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    TextRect(dc, MakeRect(84, panel.bottom - 50, panel.right - 30, panel.bottom - 20), L"취소 키로 닫을 수 있습니다.", C_DIM, gFontSmall, DT_SINGLELINE);
}

// ---- corrupted-sector dice reveal (display only) --------------------------
// The turn's real result is already fixed in gGame.dice[].rolledFace by the
// seeded RNG in game.cpp; everything here only decides how it is revealed, so
// smoke/balance determinism is untouched. Every value below is a pure function
// of elapsed time -- WM_MOUSEMOVE repaints too, and state advanced per frame
// would make the effect race whenever the mouse moves.
#define NOISE_STAGGER_MS 80
#define NOISE_SCAN_MS 200      // opaque static, the face is not readable yet
#define NOISE_LOCK_MS 180      // the face tears its way through the static
#define NOISE_SETTLE_MS 110    // border flash once a cell locks on
#define NOISE_TOTAL_MS (NOISE_SCAN_MS + NOISE_LOCK_MS)
#define NOISE_CHURN_MS 45      // how often the static reshuffles

static DWORD gReadStart;
static int gReadActive, gReadLanded, gRolled;
static int gRollFloor = -1, gRollEncounter = -1, gRollTurn = -1;

#define COMBAT_CLEAR_MS 1500
#define TURN_TRACE_STEP_MS 360
static int gCombatClearActive;
static DWORD gCombatClearStart;
static int gClearedFloor, gClearedEncounter;
static int gTurnTraceActive, gTurnTracePendingClear;
static DWORD gTurnTraceStart;
static int gTraceFloor, gTraceEncounter;

static void FinishCombatClear() {
    if (!gCombatClearActive) return;
    gCombatClearActive = 0;
    KillTimer(gWindow, 3);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginCombatClear(int floor, int encounter) {
    gClearedFloor = floor;
    gClearedEncounter = encounter;
    gCombatClearStart = GetTickCount();
    gCombatClearActive = 1;
    SetTimer(gWindow, 3, 16, 0);
}

static int TurnTraceRevealDuration() {
    int count = gGame.turnTraceCount > 0 ? gGame.turnTraceCount : 1;
    return count * TURN_TRACE_STEP_MS;
}

static void FinishTurnTrace() {
    if (!gTurnTraceActive) return;
    gTurnTraceActive = 0;
    KillTimer(gWindow, 4);
    if (gTurnTracePendingClear) BeginCombatClear(gTraceFloor, gTraceEncounter);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginTurnTrace(int floor, int encounter, int pendingClear) {
    gTraceFloor = floor;
    gTraceEncounter = encounter;
    gTurnTracePendingClear = pendingClear;
    gTurnTraceStart = GetTickCount();
    gTurnTraceActive = 1;
    SetTimer(gWindow, 4, 16, 0);
}

static int ReadElapsed() { return (int)(GetTickCount() - gReadStart); }
static int DieReadEnd(int die) { return die * NOISE_STAGGER_MS + NOISE_TOTAL_MS; }
static int DieSettled(int die) { return !gReadActive || ReadElapsed() >= DieReadEnd(die); }
static int RollBlocking() { return gReadActive && !DieSettled(2); }
static int DieLocalTime(int die) { return ReadElapsed() - die * NOISE_STAGGER_MS; }
static int NoiseStep(int die) { int t = DieLocalTime(die); return (t < 0 ? 0 : t) / NOISE_CHURN_MS; }

static uint32_t Hash3(int a, int b, int c) {
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA6Bu ^ (uint32_t)c * 0xC2B2AE35u;
    h ^= h >> 15; h *= 0x2545F491u; h ^= h >> 13;
    return h;
}

// 1000 = unreadable static, 0 = clean. Cells still queued read as full static,
// so the whole row goes to snow at once and then locks on one at a time.
static int DieNoise(int die) {
    if (!gReadActive) return 0;
    int t = DieLocalTime(die);
    if (t < NOISE_SCAN_MS) return 1000;
    if (t < NOISE_TOTAL_MS) return 1000 - (t - NOISE_SCAN_MS) * 1000 / NOISE_LOCK_MS;
    return 0;
}

static int DieSettleFlash(int die) {
    if (!gReadActive) return 0;
    int since = ReadElapsed() - DieReadEnd(die);
    if (since < 0 || since >= NOISE_SETTLE_MS) return 0;
    return 1000 - since * 1000 / NOISE_SETTLE_MS;
}

static void StopRead() {
    if (!gReadActive) return;
    gReadActive = 0; gRolled = 1; KillTimer(gWindow, 1);
}

static void BeginRead() {
    if (gGame.phase != PHASE_COMBAT || gRolled || gReadActive) return;
    gReadStart = GetTickCount(); gReadActive = 1; gReadLanded = 0;
    PlayTone(120, 90);
    SetTimer(gWindow, 1, 16, 0);
}

// Dice are rolled inside game.cpp at turn start; watch the turn identity so a
// new turn drops back to an unread sector and waits for the READ button.
static void SyncRollAnimation() {
    if (gGame.phase != PHASE_COMBAT) {
        if (gReadActive) { gReadActive = 0; KillTimer(gWindow, 1); }
        gRolled = 0; gRollTurn = -1; return;
    }
    if (gGame.floor == gRollFloor && gGame.encounter == gRollEncounter && gGame.turn == gRollTurn) return;
    gRollFloor = gGame.floor; gRollEncounter = gGame.encounter; gRollTurn = gGame.turn;
    if (gReadActive) { gReadActive = 0; KillTimer(gWindow, 1); }
    gRolled = 0;
}

static void TickRollAnimation() {
    int elapsed = ReadElapsed();
    for (int d = 0; d < 3; ++d) {
        if (!(gReadLanded & (1 << d)) && elapsed >= DieReadEnd(d)) { gReadLanded |= 1 << d; PlayTone(440 + d * 110, 28); }
    }
    if (elapsed >= DieReadEnd(2) + NOISE_SETTLE_MS) StopRead();
    InvalidateRect(gWindow, 0, FALSE);
}

static void DrawHeader(HDC dc, int width) {
    Fill(dc, MakeRect(0, 0, width, 68), RGB(10, 16, 22)); Fill(dc, MakeRect(0, 67, width, 68), C_GREEN);
    Text(dc, 24, 14, L"A:\\ROGUE", C_GREEN, gFontLarge);
    if (gGame.phase != PHASE_TITLE) {
        wchar_t b[128]; wsprintfW(b, L"%d층/3  ·  %d구역/3  ·  %d턴", gGame.floor + 1, gGame.encounter + 1, gGame.turn);
        Text(dc, 230, 14, b, C_TEXT, gFontMedium); wsprintfW(b, L"체력 %d/%d", gGame.playerHp, gGame.playerMaxHp);
        Text(dc, width - 440, 14, b, gGame.playerHp <= 10 ? C_RED : C_TEXT, gFontMedium);
        wsprintfW(b, L"덱 %dB / %dB", DeckBytes(&gGame), EffectiveCapacity(&gGame));
        Text(dc, width - 305, 14, b, DeckBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontSmall);
    }
    RECT guide = GuideButtonRect(width); int hover = Inside(guide, gMouse.x, gMouse.y);
    Panel(dc, guide, gGuideOpen ? RGB(32, 82, 67) : hover ? RGB(27, 48, 52) : C_PANEL_2, gGuideOpen || hover ? C_GREEN : C_LINE);
    TextRect(dc, guide, L"가이드 [F1]", gGuideOpen ? C_GREEN : C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT settings = SettingsButtonRect(width); int hoverSettings = Inside(settings, gMouse.x, gMouse.y);
    Panel(dc, settings, gSettingsOpen ? RGB(32, 82, 67) : hoverSettings ? RGB(27, 48, 52) : C_PANEL_2, gSettingsOpen || hoverSettings ? C_GREEN : C_LINE);
    TextRect(dc, settings, L"설정 [F2]", gSettingsOpen ? C_GREEN : C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
    TextRect(dc, start, L"[ 새 게임 ]", C_GREEN, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(150, height - 105, width - 150, height - 25),
        L"마우스 또는 1·2·3으로 주사위 선택  /  슬롯 클릭으로 배치  /  스페이스 키로 실행  /  취소 키로 보상 건너뛰기",
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
static RECT EndTurnRect() { return MakeRect(696, 616, 896, 679); }
static RECT ReadButtonRect() { return MakeRect(696, 544, 896, 600); }
static int DieForSlotUI(int slot) { for (int d = 0; d < 3; ++d) if (gGame.dice[d].assignedSlot == slot) return d; return -1; }

static void DrawEnemy(HDC dc, int index) {
    const EnemyState* enemy = &gGame.enemies[index]; const EnemyInfo* info = &ENEMY_INFO[enemy->kind]; RECT r = EnemyRect(index);
    int selected = index == gGame.targetEnemy && enemy->alive;
    Panel(dc, r, enemy->alive ? C_PANEL : RGB(18, 18, 20), selected ? C_YELLOW : C_LINE);
    DrawPortrait(dc, PortraitRect(r), enemy->kind, enemy->alive, selected, EnemyHitFlash(index), enemy->alive ? EnemyBob(index) : 0);
    Text(dc, r.left + 12, r.top + 140, info->code, enemy->alive ? (COLORREF)info->color : C_DIM, gFontMedium);
    Text(dc, r.left + 12, r.top + 165, selected ? L"▶ 공격 대상" : enemy->kind >= BOSS_DISK_ERROR ? L"보스 프로세스" : L"적 프로세스", selected ? C_YELLOW : C_DIM, gFontSmall);
    wchar_t b[80]; wsprintfW(b, L"체력 %d / %d", enemy->hp, enemy->maxHp); Text(dc, r.left + 12, r.top + 187, b, C_TEXT, gFontSmall);
    Bar(dc, MakeRect(r.left + 12, r.top + 208, r.right - 12, r.top + 220), enemy->hp, enemy->maxHp, (COLORREF)info->color);
    if (enemy->alive) {
        wsprintfW(b, L"의도: %s %d", INTENT_NAMES[enemy->intent], enemy->intentValue);
        Text(dc, r.left + 12, r.top + 227, b, enemy->intent == INTENT_HEAVY || enemy->intent == INTENT_CORRUPT ? C_RED : C_YELLOW, gFontSmall);
        if (enemy->block > 0 || enemy->burn > 0) { wsprintfW(b, L"방어도 %d   화상 %d", enemy->block, enemy->burn); Text(dc, r.left + 12, r.top + 247, b, C_DIM, gFontSmall); }
    } else Text(dc, r.left + 12, r.top + 227, L"[ 삭제됨 ]", C_DIM, gFontSmall);
}

static void DrawSlot(HDC dc, int slot) {
    RECT r = SlotRect(slot); int die = DieForSlotUI(slot); int hover = Inside(r, gMouse.x, gMouse.y);
    Panel(dc, r, hover ? RGB(23, 39, 48) : C_PANEL, hover ? C_GREEN : C_LINE);
    Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], slot == SLOT_ATTACK ? C_RED : slot == SLOT_DEFEND ? C_BLUE : C_GREEN, gFontMedium);
    if (die >= 0) {
        const Face* face = RolledFace(&gGame, die); wchar_t value[24]; FormatFace(face, value);
        TextRect(dc, MakeRect(r.left + 5, r.top + 42, r.right - 5, r.top + 83), value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        wchar_t b[48]; wsprintfW(b, L"주사위 %d · %dB", die + 1, FaceCost(face));
        TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6), b, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    } else TextRect(dc, MakeRect(r.left + 5, r.top + 48, r.right - 5, r.bottom - 10), L"비어 있음", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
}

static const wchar_t HEX_DIGIT[17] = L"0123456789ABCDEF";

// static blocks. Brushes are made once per call and reused across every block,
// so a full cell of snow costs three GDI objects, not one per block.
static void DrawSectorStatic(HDC dc, const RECT& area, int die, int step, int level) {
    if (level <= 0) return;
    int w = area.right - area.left, h = area.bottom - area.top;
    if (w <= 2 || h <= 2) return;
    HBRUSH shade[3] = {CreateSolidBrush(RGB(30, 40, 48)), CreateSolidBrush(RGB(58, 82, 92)), CreateSolidBrush(RGB(64, 140, 112))};
    int count = 6 + 30 * level / 1000;
    for (int i = 0; i < count; ++i) {
        uint32_t hx = Hash3(die, step, i);
        int bx = area.left + (int)(hx % (uint32_t)w);
        int by = area.top + (int)((hx >> 9) % (uint32_t)h);
        int bw = 5 + (int)((hx >> 17) % 30u), bh = 2 + (int)((hx >> 23) % 5u);
        if (bx + bw > area.right) bw = area.right - bx;
        if (by + bh > area.bottom) bh = area.bottom - by;
        if (bw <= 0 || bh <= 0) continue;
        RECT nr = MakeRect(bx, by, bx + bw, by + bh);
        FillRect(dc, &nr, shade[(hx >> 29) % 3u]);
    }
    for (int i = 0; i < 3; ++i) DeleteObject(shade[i]);
}

// two rows of hex garbage, like a dump of the sector being read
static void DrawSectorHex(HDC dc, const RECT& area, int die, int step, int level) {
    if (level < 260) return;
    wchar_t line[15];
    for (int row = 0; row < 2; ++row) {
        for (int i = 0; i < 14; ++i) line[i] = (i % 3 == 2) ? L' ' : HEX_DIGIT[Hash3(die * 7 + row, step, i) & 15u];
        line[14] = 0;
        RECT rowRect = MakeRect(area.left, area.top + 2 + row * 19, area.right, area.top + 20 + row * 19);
        TextRect(dc, rowRect, line, RGB(58, 116, 96), gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
}

static void DrawScanlines(HDC dc, const RECT& area) {
    HBRUSH line = CreateSolidBrush(RGB(10, 15, 20));
    for (int y = area.top; y < area.bottom; y += 3) { RECT s = MakeRect(area.left, y, area.right, y + 1); FillRect(dc, &s, line); }
    DeleteObject(line);
}

// the face value, torn into horizontal bands that slide back into alignment
static void DrawTornValue(HDC dc, const RECT& area, const wchar_t* value, COLORREF color, int die, int step, int level) {
    const int bands = 5;
    int h = area.bottom - area.top, amp = 26 * level / 1000;
    for (int i = 0; i < bands; ++i) {
        int top = area.top + h * i / bands, bottom = area.top + h * (i + 1) / bands;
        int dx = amp > 0 ? (int)(Hash3(die, step, 64 + i) % (uint32_t)(amp * 2 + 1)) - amp : 0;
        int saved = SaveDC(dc);
        IntersectClipRect(dc, area.left - 60, top, area.right + 60, bottom);
        TextRect(dc, MakeRect(area.left + dx, area.top, area.right + dx, area.bottom), value, color, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RestoreDC(dc, saved);
    }
}

static void DrawDie(HDC dc, int index) {
    RECT r = DieRect(index); const DieState* die = &gGame.dice[index];
    const Face* face = &gGame.dice[index].faces[gGame.dice[index].rolledFace];
    int selected = gGame.selectedDie == index, hover = Inside(r, gMouse.x, gMouse.y);
    int noise = DieNoise(index), flash = DieSettleFlash(index), step = NoiseStep(index);
    RECT cell = MakeRect(r.left + 6, r.top + 25, r.right - 6, r.top + 72);
    RECT statusRect = MakeRect(r.left + 7, r.top + 74, r.right - 7, r.bottom - 5);

    COLORREF border = selected ? C_GREEN : hover ? C_BLUE : C_LINE;
    if (noise > 0) border = (step & 1) ? C_RED : RGB(96, 58, 58);
    else if (flash > 0) border = C_GREEN;
    Panel(dc, r, selected ? RGB(26, 48, 49) : C_PANEL, border);
    wchar_t b[64]; wsprintfW(b, L"주사위 %d", index + 1);
    Text(dc, r.left + 10, r.top + 8, b, selected ? C_GREEN : C_TEXT, gFontSmall);

    if (!gRolled && !gReadActive) {   // sector never read this turn: faint drift
        DrawSectorStatic(dc, cell, index, (int)(GetTickCount() / 260u), 70);
        DrawScanlines(dc, cell);
        TextRect(dc, statusRect, L"판독 전", C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    if (!DieSettled(index)) {         // being read: snow, hex dump, torn face
        wchar_t value[24]; FormatFace(face, value);
        DrawSectorStatic(dc, cell, index, step, noise);
        DrawSectorHex(dc, cell, index, step, noise);
        if (noise < 1000) DrawTornValue(dc, cell, value, FaceColor(face), index, step, noise);
        DrawScanlines(dc, cell);
        TextRect(dc, statusRect, L"판독 중", C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    wchar_t value[24]; FormatFace(face, value);
    TextRect(dc, cell, value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (flash > 0) DrawScanlines(dc, cell);
    wchar_t statuses[64] = L""; int statusCount = 0;
    if (face && face->damaged) { AppendStatus(statuses, L"손상"); ++statusCount; }
    if (die->unstable) { AppendStatus(statuses, L"읽기 오류"); ++statusCount; }
    if (die->disabled) { AppendStatus(statuses, L"조각화"); ++statusCount; }
    if (statusCount == 1) TextRect(dc, statusRect, statuses, C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    else if (statusCount > 1) TextRect(dc, statusRect, statuses, C_RED, gFontSmall, DT_CENTER | DT_WORDBREAK);
    else {
        wsprintfW(b, L"%s · %dB", FACE_INFO[face->kind].name, FaceCost(face));
        TextRect(dc, statusRect, b, C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}
static void DrawSidebar(HDC dc, int width, int height) {
    RECT side = MakeRect(width - 212, 94, width - 22, height - 22); Panel(dc, side, C_PANEL, C_LINE);
    Text(dc, side.left + 12, side.top + 12, L"디스크 손상", C_RED, gFontSmall);
    Text(dc, side.left + 12, side.top + 42, MODIFIER_INFO[gGame.modifierA].name, C_YELLOW, gFontSmall);
    TextRect(dc, MakeRect(side.left + 12, side.top + 65, side.right - 10, side.top + 124), MODIFIER_INFO[gGame.modifierA].description, C_DIM, gFontSmall, DT_WORDBREAK);
    Text(dc, side.left + 12, side.top + 136, MODIFIER_INFO[gGame.modifierB].name, C_YELLOW, gFontSmall);
    TextRect(dc, MakeRect(side.left + 12, side.top + 159, side.right - 10, side.top + 222), MODIFIER_INFO[gGame.modifierB].description, C_DIM, gFontSmall, DT_WORDBREAK);
    Text(dc, side.left + 12, side.top + 242, L"실행 순서", C_GREEN, gFontSmall);
    TextRect(dc, MakeRect(side.left + 12, side.top + 268, side.right - 10, side.top + 320), L"증폭 > 공격 > 방어 > 연쇄", C_TEXT, gFontSmall, DT_WORDBREAK);
    const Face* selectedFace = gGame.selectedDie >= 0 ? RolledFace(&gGame, gGame.selectedDie) : 0;
    if (selectedFace) {
        Text(dc, side.left + 12, side.top + 340, FACE_INFO[selectedFace->kind].name, FaceColor(selectedFace), gFontMedium);
        TextRect(dc, MakeRect(side.left + 12, side.top + 372, side.right - 10, side.top + 430), FACE_INFO[selectedFace->kind].description, C_DIM, gFontSmall, DT_WORDBREAK);
    }
    Text(dc, side.left + 12, side.bottom - 112, L"시스템 기록", C_GREEN, gFontSmall);
    for (int i = 0; i < 3; ++i) TextRect(dc, MakeRect(side.left + 12, side.bottom - 88 + i * 25, side.right - 8, side.bottom - 66 + i * 25), gGame.logs[i], i == 0 ? C_TEXT : C_DIM, gFontSmall, DT_END_ELLIPSIS | DT_SINGLELINE);
}

static void DrawCombat(HDC dc, int width, int height) {
    SyncEnemyDamage();
    for (int i = 0; i < gGame.enemyCount; ++i) DrawEnemy(dc, i);
    if (gGame.hasTurnResult) {
        wchar_t result[128];
        wsprintfW(result, L"최근 실행  적 체력 -%d  ·  내 체력 -%d  ·  획득 방어도 %d",
            gGame.lastTurnDamageDealt, gGame.lastTurnDamageTaken, gGame.lastTurnBlockGained);
        Text(dc, 28, 382, result, C_YELLOW, gFontSmall);
    } else Text(dc, 28, 382, L"① 배치  →  ② 스페이스: 증폭 > 공격 > 방어 > 연쇄  →  ③ 적 행동", C_DIM, gFontSmall);
    for (int i = 0; i < SLOT_COUNT; ++i) DrawSlot(dc, i);
    for (int i = 0; i < 3; ++i) DrawDie(dc, i);
    RECT read = ReadButtonRect(); int readHover = Inside(read, gMouse.x, gMouse.y), canRead = !gRolled && !gReadActive;
    Panel(dc, read, canRead ? (readHover ? RGB(34, 86, 70) : RGB(24, 58, 49)) : C_PANEL, canRead ? C_GREEN : C_LINE);
    TextRect(dc, read, L"판독 [R]", canRead ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT end = EndTurnRect(); int hover = Inside(end, gMouse.x, gMouse.y) && gRolled;
    Panel(dc, end, hover ? RGB(71, 42, 42) : C_PANEL_2, hover ? C_RED : C_LINE);
    TextRect(dc, end, L"실행 [스페이스]", gRolled ? C_RED : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawSidebar(dc, width, height);
}

static void DrawCombatClear(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(190, 184, width - 190, height - 176); Panel(dc, panel, C_PANEL, C_GREEN);
    wchar_t cleared[96]; wsprintfW(cleared, L"%d층 · %d구역  —  적 삭제 완료", gClearedFloor + 1, gClearedEncounter + 1);
    TextRect(dc, MakeRect(panel.left + 20, panel.top + 34, panel.right - 20, panel.top + 86),
        cleared, C_GREEN, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    wchar_t result[160];
    wsprintfW(result, L"적 체력이 0이 되어 전투가 종료되었습니다.\n이번 실행: 적 체력 -%d · 내 체력 -%d",
        gGame.lastTurnDamageDealt, gGame.lastTurnDamageTaken);
    TextRect(dc, MakeRect(panel.left + 40, panel.top + 104, panel.right - 40, panel.top + 174),
        result, C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    const wchar_t* next = gClearedFloor == 2 && gClearedEncounter == 2
        ? L"다음 과정  최종 전투 결과 확인"
        : gClearedEncounter == 2
            ? L"다음 과정  보상 선택 → 면 교체/건너뛰기 → 다음 층\n(용량 초과 시 면 정리 화면을 거칩니다)"
            : L"다음 과정  보상 선택 → 면 교체/건너뛰기 → 다음 구역";
    TextRect(dc, MakeRect(panel.left + 40, panel.top + 198, panel.right - 40, panel.top + 270),
        next, C_YELLOW, gFontMedium, DT_CENTER | DT_WORDBREAK);
    TextRect(dc, MakeRect(panel.left + 40, panel.bottom - 46, panel.right - 40, panel.bottom - 18),
        L"잠시 후 보상 화면으로 이동합니다 · 클릭이나 키로 바로 넘기기", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

static void DrawTurnCalculation(HDC dc) {
    RECT panel = MakeRect(28, 396, 884, 730); Panel(dc, panel, RGB(10, 17, 24), C_BLUE);
    Text(dc, panel.left + 18, panel.top + 14, L"턴 계산 과정", C_BLUE, gFontMedium);
    Text(dc, panel.left + 224, panel.top + 18, L"증폭 → 공격 → 적중 → 방어 → 연쇄 → 적 행동", C_DIM, gFontSmall);

    int count = gGame.turnTraceCount;
    int shown = (int)(GetTickCount() - gTurnTraceStart) / TURN_TRACE_STEP_MS + 1;
    if (shown > count) shown = count;
    for (int i = 0; i < shown; ++i) {
        int y = panel.top + 55 + i * 31;
        COLORREF color = i == shown - 1 && shown < count ? C_YELLOW : C_TEXT;
        Fill(dc, MakeRect(panel.left + 18, y + 3, panel.left + 22, y + 23), color);
        TextRect(dc, MakeRect(panel.left + 34, y, panel.right - 18, y + 27),
            gGame.turnTrace[i], color, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    wchar_t progress[48]; wsprintfW(progress, L"계산 %d / %d", shown, count);
    TextRect(dc, MakeRect(panel.right - 126, panel.top + 17, panel.right - 18, panel.top + 40),
        progress, C_GREEN, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    if (shown == count) TextRect(dc, MakeRect(panel.left + 18, panel.bottom - 31, panel.right - 18, panel.bottom - 9),
        L"계산 완료 · 계속하려면 화면을 클릭하세요", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

static RECT RewardRect(int i, int width) {
    int cardWidth = 220, gap = 28, total = cardWidth * 3 + gap * 2; int left = (width - total) / 2 + i * (cardWidth + gap);
    return MakeRect(left, 130, left + cardWidth, 278);
}
static RECT FaceGridRect(int die, int face) { int left = 150 + face * 112, top = 350 + die * 90; return MakeRect(left, top, left + 98, top + 68); }
static RECT ContinueRect(int width, int height) { return MakeRect(width - 276, height - 94, width - 42, height - 38); }

static void DrawFaceGrid(HDC dc, int mode) {
    for (int d = 0; d < 3; ++d) {
        wchar_t label[24]; wsprintfW(label, L"주사위 %d", d + 1); Text(dc, 56, 371 + d * 90, label, C_GREEN, gFontMedium);
        for (int f = 0; f < 6; ++f) {
            RECT r = FaceGridRect(d, f); const Face* face = &gGame.dice[d].faces[f]; int hover = Inside(r, gMouse.x, gMouse.y);
            COLORREF border = hover && mode ? (mode == 2 ? C_RED : C_GREEN) : C_LINE; Panel(dc, r, hover ? RGB(28, 39, 48) : C_PANEL, border);
            wchar_t value[24]; FormatFace(face, value); TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 37), value, FaceColor(face), gFontMedium, DT_CENTER | DT_SINGLELINE);
            wchar_t bytes[24]; wsprintfW(bytes, L"%dB", FaceCost(face)); TextRect(dc, MakeRect(r.left + 4, r.bottom - 23, r.right - 4, r.bottom - 4), bytes, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
    }
}

static void DrawReward(HDC dc, int width, int height) {
    TextRect(dc, MakeRect(0, 76, width, 102), L"전투 완료  →  [현재: 보상 선택]  →  면 교체  →  다음 전투", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 96, width, 122), L"보상 하나를 고른 뒤 교체할 주사위 면을 클릭하십시오", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    for (int i = 0; i < 3; ++i) {
        RECT r = RewardRect(i, width); int selected = gGame.selectedReward == i, hover = Inside(r, gMouse.x, gMouse.y), kind = gGame.rewardKinds[i];
        Panel(dc, r, selected ? RGB(31, 55, 48) : C_PANEL, selected ? C_GREEN : hover ? C_BLUE : C_LINE);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), FACE_INFO[kind].name, (COLORREF)FACE_INFO[kind].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[48]; int cost = kind == FACE_NUMBER ? gGame.rewardValues[i] : FACE_INFO[kind].cost;
        wsprintfW(b, L"출력 %d  ·  %dB", gGame.rewardValues[i], cost); TextRect(dc, MakeRect(r.left + 8, r.top + 58, r.right - 8, r.top + 82), b, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 16, r.top + 92, r.right - 16, r.bottom - 12), FACE_INFO[kind].description, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
    }
    Text(dc, 56, 304, gGame.selectedReward >= 0 ? L"2/2  교체할 기존 면을 클릭하세요" : L"1/2  위에서 보상 면을 먼저 선택하세요", gGame.selectedReward >= 0 ? C_YELLOW : C_GREEN, gFontSmall); DrawFaceGrid(dc, gGame.selectedReward >= 0 ? 1 : 0);
    RECT skip = ContinueRect(width, height); Panel(dc, skip, C_PANEL_2, C_LINE); TextRect(dc, skip, L"건너뛰기 [취소]", C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawPrune(HDC dc, int width, int height) {
    wchar_t b[160]; wsprintfW(b, L"%d층 진입 한도: %dB  ·  현재: %dB", gGame.floor + 1, EffectiveCapacity(&gGame), DeckBytes(&gGame));
    TextRect(dc, MakeRect(0, 92, width, 132), b, DeckBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(80, 145, width - 80, 218), L"면을 클릭하면 빈 면(0B)으로 삭제됩니다. 손상 면도 원래 비용을 차지합니다.\n한도 이하가 되면 다음 층으로 진행할 수 있습니다.", C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    DrawFaceGrid(dc, 2); RECT confirm = ContinueRect(width, height); int ready = DeckBytes(&gGame) <= EffectiveCapacity(&gGame) && NonEmptyFaceCount(&gGame) > 0;
    Panel(dc, confirm, ready ? RGB(28, 70, 57) : C_PANEL_2, ready ? C_GREEN : C_LINE); TextRect(dc, confirm, L"계속 [엔터]", ready ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawEndScreen(HDC dc, int width, int height, int victory) {
    TextRect(dc, MakeRect(0, height / 2 - 150, width, height / 2 - 70), victory ? L"디스크 복구 완료" : L"시스템 정지", victory ? C_GREEN : C_RED, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    wchar_t b[192]; wsprintfW(b, L"전투 %d회 완료  ·  면 %d개 설치  ·  최종 덱 %dB\n\nR 또는 엔터 키로 새 게임", gGame.combatsWon, gGame.facesInstalled, DeckBytes(&gGame));
    TextRect(dc, MakeRect(120, height / 2 - 40, width - 120, height / 2 + 110), b, C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
}

static void DrawGuide(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"시스템 가이드", C_GREEN, gFontLarge);
    Text(dc, panel.left + 255, panel.top + 28, L"전투 중에도 F1로 열고 닫을 수 있습니다.", C_DIM, gFontSmall);
    RECT close = GuideCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int left = panel.left + 30, middle = width / 2 + 12, top = panel.top + 76;
    Text(dc, left, top, L"빠른 시작", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 32, middle - 28, top + 126),
        L"1. 주사위를 클릭하거나 1·2·3으로 선택\n2. 서로 다른 슬롯을 클릭해 배치\n3. 적을 클릭해 공격 대상 선택\n4. 스페이스 키로 턴 실행", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, left, top + 140, L"슬롯 실행 순서", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 172, middle - 28, top + 286),
        L"증폭  공격·방어 출력을 먼저 강화\n공격  선택한 적에게 피해\n방어  이번 턴 적 공격을 흡수\n연쇄  직전 공격 또는 방어를 반복", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, left, top + 300, L"상태와 적 의도", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 332, middle - 28, panel.bottom - 24),
        L"화상: 적 행동 직전에 3 피해\n읽기 오류: 실행 순간 해당 주사위를 다시 굴림\n조각화: 중복 결과, 이번 턴 출력 0\n복합 상태는 주사위에 모두 함께 표시\n오염(관통): 표시 수치만큼 체력에 직접 피해. 방어도를 소모하거나 적용받지 않음", C_TEXT, gFontSmall, DT_WORDBREAK);

    Text(dc, middle, top, L"디스크 손상", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 32, panel.right - 28, top + 190),
        L"배드 섹터  층 이동 시 무작위 면 영구 손상 (설치해도 복구 안 됨)\n읽기 오류  경고 주사위가 실행 순간 재굴림\n조각화  같은 결과 중 뒤쪽 주사위 비활성화\n과잉 할당  용량 +60B, 적 체력 +30%\n체크섬  굴림 합이 짝수면 공격 +2", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 204, L"덱·보상·용량", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 236, panel.right - 28, top + 350),
        L"18개 면의 비용 합이 덱 용량입니다. 전투 뒤 보상 면을 골라 기존 면과 교체합니다. 현재 층 및 다음 층 한도를 넘으면 빈 면(0B)이 되도록 면을 삭제해야 합니다.", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 364, L"조작", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 396, panel.right - 28, panel.bottom - 24),
        L"클릭 / 1·2·3  선택\n스페이스  턴 실행\n취소  배치 해제·보상 건너뛰기·가이드 닫기\n엔터  용량 정리 확정\nF1  가이드 열기·닫기\nF2  설정 열기·닫기", C_TEXT, gFontSmall, DT_WORDBREAK);
}

static void PaintGame(HWND window) {
    SyncIdleAnimation();
    PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint); RECT client; GetClientRect(window, &client);
    int clientWidth = client.right, clientHeight = client.bottom;
    if (clientWidth <= 0 || clientHeight <= 0) { EndPaint(window, &paint); return; }

    // 1단계: 항상 고정된 BASE_WIDTH x BASE_HEIGHT 캔버스에 그린다 - 기존 좌표 계산은 전부 그대로 둔다.
    HDC canvas = CreateCompatibleDC(dc); HBITMAP canvasBitmap = CreateCompatibleBitmap(dc, BASE_WIDTH, BASE_HEIGHT); HBITMAP oldCanvas = (HBITMAP)SelectObject(canvas, canvasBitmap);
    RECT canvasRect = MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT);
    Fill(canvas, canvasRect, C_BG); DrawHeader(canvas, BASE_WIDTH);
    if (gTurnTraceActive) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_TITLE) DrawTitle(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_COMBAT) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_REWARD) DrawReward(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_PRUNE) DrawPrune(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_GAMEOVER) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 0); else if (gGame.phase == PHASE_VICTORY) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 1);
    if (gTurnTraceActive) DrawTurnCalculation(canvas);
    else if (gCombatClearActive) DrawCombatClear(canvas, BASE_WIDTH, BASE_HEIGHT);
    if (gSettingsOpen) DrawSettings(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGuideOpen) DrawGuide(canvas, BASE_WIDTH, BASE_HEIGHT);

    // 2단계: 실제 창 크기의 오프스크린 버퍼 위에서 배경 채우기 + 비율 유지 확대까지 전부 끝낸다.
    // (화면 DC에 직접 그리면 배경 채우기와 StretchBlt 사이가 노출돼 깜빡임이 생긴다.)
    HDC composite = CreateCompatibleDC(dc); HBITMAP compositeBitmap = CreateCompatibleBitmap(dc, clientWidth, clientHeight); HBITMAP oldComposite = (HBITMAP)SelectObject(composite, compositeBitmap);
    float scale; int offsetX, offsetY; ComputeCanvasTransform(clientWidth, clientHeight, &scale, &offsetX, &offsetY);
    int scaledWidth = (int)(BASE_WIDTH * scale), scaledHeight = (int)(BASE_HEIGHT * scale);
    Fill(composite, client, C_BG);
    SetStretchBltMode(composite, HALFTONE); SetBrushOrgEx(composite, 0, 0, 0);
    StretchBlt(composite, offsetX, offsetY, scaledWidth, scaledHeight, canvas, 0, 0, BASE_WIDTH, BASE_HEIGHT, SRCCOPY);

    // 3단계: 완성된 프레임을 화면에 단 한 번에 복사한다.
    BitBlt(dc, 0, 0, clientWidth, clientHeight, composite, 0, 0, SRCCOPY);

    SelectObject(composite, oldComposite); DeleteObject(compositeBitmap); DeleteDC(composite);
    SelectObject(canvas, oldCanvas); DeleteObject(canvasBitmap); DeleteDC(canvas);
    EndPaint(window, &paint);
}

static void BeginNewRun() { NewRun(&gGame, GetTickCount() ^ (uint32_t)(ULONG_PTR)gWindow); PlayTone(520, 90); InvalidateRect(gWindow, 0, FALSE); }

static void ExecuteCombatTurn() {
    int floor = gGame.floor, encounter = gGame.encounter;
    int turn = gGame.turn;
    GamePhase before = gGame.phase;
    EndTurn(&gGame);
    int resolved = before == PHASE_COMBAT && (gGame.phase != before || gGame.turn != turn);
    int cleared = gGame.phase == PHASE_REWARD || gGame.phase == PHASE_VICTORY;
    if (resolved) BeginTurnTrace(floor, encounter, cleared);
    if (gGame.phase == PHASE_GAMEOVER) PlayTone(130, 180);
    else if (gGame.phase == PHASE_VICTORY) PlayTone(880, 180);
    else if (before != gGame.phase) PlayTone(760, 90);
    else PlayTone(260, 55);
}

static void ClickCombat(int x, int y) {
    if (Inside(ReadButtonRect(), x, y)) { BeginRead(); return; }
    if (!gRolled) return;
    for (int i = 0; i < gGame.enemyCount; ++i) if (Inside(EnemyRect(i), x, y)) { SelectEnemy(&gGame, i); PlayTone(330, 45); return; }
    for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) { gGame.selectedDie = i; PlayTone(480 + i * 60, 35); return; }
    for (int i = 0; i < SLOT_COUNT; ++i) if (Inside(SlotRect(i), x, y)) {
        if (gGame.selectedDie >= 0) { AssignDieToSlot(&gGame, gGame.selectedDie, i); PlayTone(640 + i * 45, 45); }
        else { int die = DieForSlotUI(i); if (die >= 0) gGame.selectedDie = die; } return;
    }
    if (Inside(EndTurnRect(), x, y)) {
        ExecuteCombatTurn();
    }
}

static void ClickReward(int x, int y) {
    for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) { SelectReward(&gGame, i); PlayTone(600 + i * 80, 50); return; }
    if (gGame.selectedReward >= 0) for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) { InstallSelectedReward(&gGame, d, f); PlayTone(760, 85); return; }
    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { SkipReward(&gGame); PlayTone(350, 50); }
}

static void ClickPrune(int x, int y) {
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) { PruneFace(&gGame, d, f); PlayTone(180, 65); return; }
    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { ConfirmPrune(&gGame); PlayTone(560, 60); }
}

// 현재 페이즈에서 (x, y)가 어떤 상호작용 가능한 사각형 위에 있는지 식별하는 id를 반환한다.
// -1은 "호버 없음". 마우스가 움직여도 이 id가 바뀌지 않으면 화면을 다시 그릴 필요가 없다.
static int HoverId(int x, int y) {
    if (Inside(SettingsButtonRect(BASE_WIDTH), x, y)) return 900;
    if (gSettingsOpen) {
        if (Inside(SettingsCloseRect(BASE_WIDTH), x, y)) return 901;
        for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) if (Inside(ScaleOptionRect(i), x, y)) return 910 + i;
        if (Inside(FullscreenToggleRect(), x, y)) return 920;
        return -1;
    }
    if (Inside(GuideButtonRect(BASE_WIDTH), x, y)) return 800;
    if (gGuideOpen) return Inside(GuideCloseRect(BASE_WIDTH), x, y) ? 801 : -1;
    if (gGame.phase == PHASE_TITLE) {
        if (Inside(StartButtonRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 0;
        return -1;
    }
    if (gGame.phase == PHASE_COMBAT) {
        for (int i = 0; i < gGame.enemyCount; ++i) if (Inside(EnemyRect(i), x, y)) return 100 + i;
        for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) return 200 + i;
        for (int i = 0; i < SLOT_COUNT; ++i) if (Inside(SlotRect(i), x, y)) return 300 + i;
        if (Inside(EndTurnRect(), x, y)) return 400;
        return -1;
    }
    if (gGame.phase == PHASE_REWARD) {
        for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) return 500 + i;
        for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) return 600 + d * 6 + f;
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 700;
        return -1;
    }
    if (gGame.phase == PHASE_PRUNE) {
        for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) return 600 + d * 6 + f;
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 700;
        return -1;
    }
    return -1;
}

static void HandleClick(int x, int y) {
    if (gTurnTraceActive) { FinishTurnTrace(); return; }
    if (gCombatClearActive) { FinishCombatClear(); return; }
    if (gSettingsOpen) {
        if (Inside(SettingsCloseRect(BASE_WIDTH), x, y) || Inside(SettingsButtonRect(BASE_WIDTH), x, y)) { gSettingsOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
        for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) if (Inside(ScaleOptionRect(i), x, y)) { ApplyWindowedScale(SCALE_OPTIONS[i]); InvalidateRect(gWindow, 0, FALSE); return; }
        if (Inside(FullscreenToggleRect(), x, y)) { ApplyFullscreen(!gFullscreen); InvalidateRect(gWindow, 0, FALSE); return; }
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(SettingsButtonRect(BASE_WIDTH), x, y)) { gSettingsOpen = 1; gGuideOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) {
        if (Inside(GuideCloseRect(BASE_WIDTH), x, y) || Inside(GuideButtonRect(BASE_WIDTH), x, y)) gGuideOpen = 0;
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(GuideButtonRect(BASE_WIDTH), x, y)) { gGuideOpen = 1; gSettingsOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGame.phase == PHASE_TITLE) { if (Inside(StartButtonRect(BASE_WIDTH, BASE_HEIGHT), x, y)) BeginNewRun(); }
    else if (gGame.phase == PHASE_COMBAT) ClickCombat(x, y); else if (gGame.phase == PHASE_REWARD) ClickReward(x, y);
    else if (gGame.phase == PHASE_PRUNE) ClickPrune(x, y); else BeginNewRun();
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static void HandleKey(WPARAM key) {
    if (gTurnTraceActive) return;
    if (gCombatClearActive) { FinishCombatClear(); return; }
    if (key == VK_F2) { gSettingsOpen = !gSettingsOpen; gGuideOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gSettingsOpen) { if (key == VK_ESCAPE) gSettingsOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (key == VK_F1) { gGuideOpen = !gGuideOpen; gSettingsOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) { if (key == VK_ESCAPE) gGuideOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGame.phase == PHASE_TITLE) { if (key == VK_RETURN || key == VK_SPACE) BeginNewRun(); }
    else if (gGame.phase == PHASE_COMBAT) {
        if (key == 'R') BeginRead();
        else if (!gRolled) { /* sector not read yet */ }
        else if (key >= '1' && key <= '3') { gGame.selectedDie = (int)(key - '1'); PlayTone(480 + gGame.selectedDie * 60, 35); }
        else if (key == VK_SPACE) ExecuteCombatTurn();
        else if (key == VK_ESCAPE && gGame.selectedDie >= 0) UnassignDie(&gGame, gGame.selectedDie);
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
    case WM_GETMINMAXINFO: { MINMAXINFO* info = (MINMAXINFO*)lParam; info->ptMinTrackSize.x = 480; info->ptMinTrackSize.y = 320; return 0; }
    case WM_MOUSEMOVE: {
        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        int hover = HoverId(gMouse.x, gMouse.y);
        if (hover != gHoverId) { gHoverId = hover; InvalidateRect(window, 0, FALSE); }
        return 0;
    }
    case WM_LBUTTONDOWN: { POINT p = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); HandleClick(p.x, p.y); return 0; }
    case WM_KEYDOWN: if ((lParam & (1u << 30)) == 0) HandleKey(wParam); return 0;
    case WM_TIMER:
        if (wParam == 1u) TickRollAnimation();
        else if (wParam == 2u) InvalidateRect(window, 0, FALSE);
        else if (wParam == 3u) {
            if ((int)(GetTickCount() - gCombatClearStart) >= COMBAT_CLEAR_MS) FinishCombatClear();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 4u) {
            if ((int)(GetTickCount() - gTurnTraceStart) >= TurnTraceRevealDuration()) KillTimer(window, 4);
            InvalidateRect(window, 0, FALSE);
        }
        return 0;
    case WM_PAINT: PaintGame(window); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        KillTimer(window, 1); KillTimer(window, 2); KillTimer(window, 3); KillTimer(window, 4);
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
