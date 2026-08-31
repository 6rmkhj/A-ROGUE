#include <windows.h>
#include "render.h"
#include "sprites.h"

HFONT gFontSmall, gFontMedium, gFontLarge, gFontHuge;

static HFONT MakeFont(int height, int weight) {
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, HANGEUL_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
}

void CreateRenderFonts() {
    gFontSmall = MakeFont(16, FW_NORMAL);
    gFontMedium = MakeFont(21, FW_BOLD);
    gFontLarge = MakeFont(32, FW_BOLD);
    gFontHuge = MakeFont(62, FW_BOLD);
}

void DestroyRenderFonts() {
    if (gFontSmall) DeleteObject(gFontSmall);
    if (gFontMedium) DeleteObject(gFontMedium);
    if (gFontLarge) DeleteObject(gFontLarge);
    if (gFontHuge) DeleteObject(gFontHuge);
}

RECT MakeRect(int l, int t, int r, int b) { RECT value = {l, t, r, b}; return value; }
int Inside(const RECT& rect, int x, int y) { POINT p = {x, y}; return PtInRect(&rect, p); }

// 실제 창 크기(clientW x clientH) 안에 BASE_WIDTH x BASE_HEIGHT 디자인 캔버스를
// 비율을 유지한 채로 최대한 맞춰 넣었을 때의 배율과 여백(레터박스)을 계산한다.
void ComputeCanvasTransform(int clientW, int clientH, float* scale, int* offsetX, int* offsetY) {
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
POINT ScreenToCanvas(HWND window, int x, int y) {
    RECT client; GetClientRect(window, &client);
    float scale; int offsetX, offsetY;
    ComputeCanvasTransform(client.right, client.bottom, &scale, &offsetX, &offsetY);
    POINT p; p.x = (int)((x - offsetX) / scale); p.y = (int)((y - offsetY) / scale);
    return p;
}

void Fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color); FillRect(dc, &rect, brush); DeleteObject(brush);
}

void Outline(HDC dc, const RECT& rect, COLORREF color, int thickness) {
    HBRUSH brush = CreateSolidBrush(color); RECT r = rect;
    for (int i = 0; i < thickness; ++i) { FrameRect(dc, &r, brush); InflateRect(&r, -1, -1); }
    DeleteObject(brush);
}

void Panel(HDC dc, const RECT& rect, COLORREF fillColor, COLORREF borderColor) {
    Fill(dc, rect, fillColor); Outline(dc, rect, borderColor, 1);
}

void Text(HDC dc, int x, int y, const wchar_t* value, COLORREF color, HFONT font) {
    HFONT old = (HFONT)SelectObject(dc, font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    TextOutW(dc, x, y, value, lstrlenW(value)); SelectObject(dc, old);
}

void TextRect(HDC dc, const RECT& rect, const wchar_t* value, COLORREF color, HFONT font, UINT flags) {
    HFONT old = (HFONT)SelectObject(dc, font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color); RECT r = rect;
    DrawTextW(dc, value, -1, &r, flags); SelectObject(dc, old);
}

void Bar(HDC dc, const RECT& rect, int value, int maximum, COLORREF color) {
    Fill(dc, rect, RGB(39, 48, 57));
    if (maximum > 0 && value > 0) { RECT part = rect; part.right = part.left + (part.right - part.left) * value / maximum; Fill(dc, part, color); }
    Outline(dc, rect, C_LINE, 1);
}

void FormatFace(const Face* face, wchar_t* out) {
    if (!face) lstrcpyW(out, L"--");
    else if (face->damaged) lstrcpyW(out, L"손상");
    else if (face->quarantined != QUAR_NONE) lstrcpyW(out, L"격리");
    else if (face->kind == FACE_NUMBER) wsprintfW(out, L"%d", (int)face->value);
    else lstrcpyW(out, FACE_INFO[face->kind].shortName);
}

COLORREF FaceColor(const Face* face) {
    if (!face) return C_DIM;
    if (face->damaged) return C_RED;
    if (face->quarantined != QUAR_NONE) return C_YELLOW;
    return (COLORREF)FACE_INFO[face->kind].color;
}

void AppendStatus(wchar_t* output, const wchar_t* status) {
    if (output[0]) lstrcatW(output, L" · ");
    lstrcatW(output, status);
}

uint32_t Hash3(int a, int b, int c) {
    uint32_t h = (uint32_t)a * 0x9E3779B1u ^ (uint32_t)b * 0x85EBCA6Bu ^ (uint32_t)c * 0xC2B2AE35u;
    h ^= h >> 15; h *= 0x2545F491u; h ^= h >> 13;
    return h;
}

COLORREF MixColor(COLORREF from, COLORREF to, int amount) {
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

// 잘못된 kind가 흘러들어도 배열을 선행 접근하지 않도록 폴백으로 대체한다.
static const char* const* GetEnemySpriteOrUnknown(int kind) {
    if (kind < 0 || kind >= ENEMY_KIND_COUNT) return UNKNOWN_SPRITE;
    return ENEMY_SPRITES[kind];
}

// Runs of identical cells collapse into one FillRect, so a portrait costs ~60 GDI calls.
void DrawSpriteArt(HDC dc, const RECT& box, int kind, int alive, int flash, int bob) {
    const char* const* rows = GetEnemySpriteOrUnknown(kind);
    COLORREF base = (COLORREF)GetEnemyInfoOrUnknown(kind)->color;
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

void DrawPortrait(HDC dc, const RECT& box, int kind, int alive, int selected, int flash, int bob) {
    Panel(dc, box, alive ? RGB(11, 17, 24) : RGB(13, 13, 15), selected ? (COLORREF)GetEnemyInfoOrUnknown(kind)->color : C_LINE);
    for (int y = box.top + 2; y < box.bottom - 1; y += 4) Fill(dc, MakeRect(box.left + 1, y, box.right - 1, y + 1), RGB(8, 13, 19));
    if (alive) {
        int centerX = (box.left + box.right) / 2, shadow = box.bottom - 9;
        Fill(dc, MakeRect(centerX - 36, shadow, centerX + 36, shadow + 4), RGB(7, 11, 16));
        Fill(dc, MakeRect(centerX - 26, shadow + 4, centerX + 26, shadow + 6), RGB(9, 14, 20));
    }
    DrawSpriteArt(dc, box, kind, alive, flash, bob);
}

static const wchar_t HEX_DIGIT[17] = L"0123456789ABCDEF";

// static blocks. Brushes are made once per call and reused across every block,
// so a full cell of snow costs three GDI objects, not one per block.
void DrawSectorStatic(HDC dc, const RECT& area, int die, int step, int level) {
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
void DrawSectorHex(HDC dc, const RECT& area, int die, int step, int level) {
    if (level < 260) return;
    wchar_t line[15];
    for (int row = 0; row < 2; ++row) {
        for (int i = 0; i < 14; ++i) line[i] = (i % 3 == 2) ? L' ' : HEX_DIGIT[Hash3(die * 7 + row, step, i) & 15u];
        line[14] = 0;
        RECT rowRect = MakeRect(area.left, area.top + 2 + row * 19, area.right, area.top + 20 + row * 19);
        TextRect(dc, rowRect, line, RGB(58, 116, 96), gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
}

void DrawScanlines(HDC dc, const RECT& area) {
    HBRUSH line = CreateSolidBrush(RGB(10, 15, 20));
    for (int y = area.top; y < area.bottom; y += 3) { RECT s = MakeRect(area.left, y, area.right, y + 1); FillRect(dc, &s, line); }
    DeleteObject(line);
}

// the face value, torn into horizontal bands that slide back into alignment
void DrawTornValue(HDC dc, const RECT& area, const wchar_t* value, COLORREF color, int die, int step, int level) {
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
