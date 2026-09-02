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

int TextWidth(HDC dc, const wchar_t* value, HFONT font) {
    if (!value || !value[0]) return 0;
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE size;
    int ok = GetTextExtentPoint32W(dc, value, lstrlenW(value), &size);
    SelectObject(dc, old);
    return ok ? size.cx : 0;
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
void DrawSpriteArt(HDC dc, const RECT& box, int kind, int alive, int flash, int bob, int shiftX) {
    const char* const* rows = GetEnemySpriteOrUnknown(kind);
    COLORREF base = (COLORREF)GetEnemyInfoOrUnknown(kind)->color;
    int boxWidth = box.right - box.left, boxHeight = box.bottom - box.top;
    int scale = (boxWidth < boxHeight ? boxWidth : boxHeight) / SPRITE_SIZE; if (scale < 1) scale = 1;
    int originX = box.left + (boxWidth - scale * SPRITE_SIZE) / 2 + shiftX;
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

void DrawPortrait(HDC dc, const RECT& box, int kind, int alive, int selected, int flash, int bob, int shiftX) {
    Panel(dc, box, alive ? RGB(11, 17, 24) : RGB(13, 13, 15), selected ? (COLORREF)GetEnemyInfoOrUnknown(kind)->color : C_LINE);
    for (int y = box.top + 2; y < box.bottom - 1; y += 4) Fill(dc, MakeRect(box.left + 1, y, box.right - 1, y + 1), RGB(8, 13, 19));
    if (alive) {
        int centerX = (box.left + box.right) / 2, shadow = box.bottom - 9;
        Fill(dc, MakeRect(centerX - 36, shadow, centerX + 36, shadow + 4), RGB(7, 11, 16));
        Fill(dc, MakeRect(centerX - 26, shadow + 4, centerX + 26, shadow + 6), RGB(9, 14, 20));
    }
    // 달려드는 동안에도 그림은 초상 상자 안에서만 움직인다. 상자를 넘어가는
    // 부분은 잘려 나가면서 화면 밖으로 몸을 던지는 것처럼 보인다.
    int saved = SaveDC(dc);
    IntersectClipRect(dc, box.left + 1, box.top + 1, box.right - 1, box.bottom - 1);
    DrawSpriteArt(dc, box, kind, alive, flash, bob, shiftX);
    RestoreDC(dc, saved);
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

// 글자마다 주기가 다르다. 빠른 글자는 쉴 새 없이 튀고 느린 글자는 잠깐 멈춘 것처럼
// 보여서, 한 줄이 통째로 갈리는 것보다 훨씬 "뚫리는 중"으로 읽힌다. 구분 기호(. -)와
// 공백은 남겨 두어 원래 형태가 어렴풋이 읽히고, 판독되는 순간의 대비가 살아난다.
void CorruptCode(const wchar_t* source, wchar_t* out, int cap, int seed, uint32_t tick) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!source) return;
    int n = 0;
    for (; source[n] && n < cap - 1; ++n) {
        wchar_t c = source[n];
        if (c == L' ' || c == L'.' || c == L'-') { out[n] = c; continue; }
        uint32_t period = 45u + (Hash3(seed, n, 0x51CE) % 210u);   // 45~254ms
        uint32_t h = Hash3(seed * 13 + n, (int)(tick / period), n);
        out[n] = (h & 7u) == 0 ? L'?' : (h & 7u) == 1 ? L'#' : HEX_DIGIT[(h >> 4) & 15u];
    }
    out[n] = 0;
}

// 고정폭 노이즈 한 줄. 낱글자 몇 개만 골라 밝게 띄운다. 어두운 줄에서는 그 자리를
// 비워 두고 따로 찍으므로 글자가 겹쳐 뭉개지지 않는다. 글자마다 반짝이는 주기가
// 달라, 줄 단위로 밝아지는 것보다 훨씬 "지금 뚫리는 중"으로 읽힌다.
void DrawGlitchLine(HDC dc, int x, int y, const wchar_t* text, COLORREF dim, COLORREF lit,
                    HFONT font, int seed, uint32_t tick) {
    if (!text || !text[0]) return;
    wchar_t base[80];
    int flag[80];
    int n = 0;
    for (; text[n] && n < 79; ++n) {
        base[n] = text[n];
        uint32_t period = 130u + (Hash3(seed, n, 0x1177) % 300u);   // 130~429ms
        flag[n] = text[n] != L' ' && (Hash3(seed * 7 + n, (int)(tick / period), n) % 11u) == 0;
        if (flag[n]) base[n] = L' ';
    }
    base[n] = 0;
    Text(dc, x, y, base, dim, font);
    int cell = TextWidth(dc, L"0", font);
    if (cell <= 0) return;
    for (int i = 0; i < n; ++i) {
        if (!flag[i]) continue;
        wchar_t one[2] = {text[i], 0};
        Text(dc, x + i * cell, y, one, lit, font);
    }
}

// 사각형을 헥스 덤프로 채운다. 판독 전 설명문 자리를 통째로 덮는 용도다.
// 한글은 폭이 두 배라 글자 단위로 갈아 끼우면 폭이 무너지므로, 설명문은
// 바꾸지 않고 이렇게 덮는다. 칸마다 주기가 다르고, 줄마다 좌우로 몇 px씩
// 어긋나며, 흩어진 낱글자가 초록으로 반짝인다.
void DrawHexBlock(HDC dc, const RECT& area, COLORREF color, int seed, uint32_t tick, int rows) {
    int width = area.right - area.left;
    if (width <= 0 || rows <= 0) return;
    int columns = width / 9;              // Consolas 16px의 ASCII 한 칸이 대략 9px
    if (columns > 62) columns = 62;
    if (columns < 4) columns = 4;
    COLORREF lit = MixColor(color, C_GREEN, 82);
    wchar_t line[64];
    for (int row = 0; row < rows; ++row) {
        int top = area.top + row * 19;
        if (top + 14 > area.bottom) break;
        for (int i = 0; i < columns; ++i) {
            if (i % 3 == 2) { line[i] = L' '; continue; }
            uint32_t period = 40u + (Hash3(seed + row, i, 0x7A5D) % 190u);   // 40~229ms
            line[i] = HEX_DIGIT[Hash3(seed * 31 + row, (int)(tick / period), i) & 15u];
        }
        line[columns] = 0;
        int jitter = (int)(Hash3(seed, row, (int)(tick / 90u)) % 4u);
        DrawGlitchLine(dc, area.left + jitter, top, line, color, lit, gFontSmall, seed * 31 + row, tick);
    }
}

// 한 줄을 왼쪽에서 오른쪽으로 훑으며 level 확률로 조각을 채운다. 채우는 비율이
// 곧 level이므로 1000에서는 화면에 원래 그림이 한 점도 남지 않는다. 붓은 한 번만
// 만들어 돌려 쓰고, 건너뛰는 조각은 GDI 호출 자체를 하지 않는다.
void DrawScreenStatic(HDC dc, const RECT& area, int step, int level) {
    if (level <= 0) return;
    if (level > 1000) level = 1000;
    int width = area.right - area.left, height = area.bottom - area.top;
    if (width <= 2 || height <= 2) return;
    HBRUSH shade[4] = {CreateSolidBrush(RGB(13, 19, 25)), CreateSolidBrush(RGB(33, 47, 55)),
                       CreateSolidBrush(RGB(72, 104, 96)), CreateSolidBrush(RGB(150, 196, 178))};
    for (int y = area.top; y < area.bottom; y += 6) {
        int bottom = y + 6 > area.bottom ? area.bottom : y + 6;
        int x = area.left;
        for (int i = 0; x < area.right; ++i) {
            uint32_t h = Hash3(step, y, i);
            int w = 10 + (int)(h % 70u);
            if (x + w > area.right) w = area.right - x;
            if ((int)((h >> 11) % 1000u) < level) {
                uint32_t pick = (h >> 26) % 100u;
                RECT cell = MakeRect(x, y, x + w, bottom);
                FillRect(dc, &cell, shade[pick < 44u ? 0 : pick < 76u ? 1 : pick < 95u ? 2 : 3]);
            }
            x += w;
        }
    }
    // 신호가 무너질수록 밝은 띠 하나가 화면을 타고 흘러내린다.
    if (level > 380) {
        int band = area.top + (int)((uint32_t)(step * 17) % (uint32_t)height);
        int thick = 2 + level / 400;
        if (band + thick > area.bottom) thick = area.bottom - band;
        if (thick > 0) Fill(dc, MakeRect(area.left, band, area.right, band + thick), MixColor(RGB(60, 90, 88), RGB(196, 232, 216), level / 10));
    }
    for (int i = 0; i < 4; ++i) DeleteObject(shade[i]);
}

// 띠를 세 겹으로 나눠 안쪽 겹일수록 옅게 덮는다. 각 겹은 중앙을 잘라낸
// 클립 안에서만 그려지므로 화면 한가운데는 원본 그대로 남는다.
void DrawEdgeStatic(HDC dc, const RECT& area, int step, int level, int thickness) {
    if (level <= 0 || thickness <= 0) return;
    const int rings = 3;
    for (int i = 0; i < rings; ++i) {
        RECT outer = area, inner = area;
        InflateRect(&outer, -thickness * i / rings, -thickness * i / rings);
        InflateRect(&inner, -thickness * (i + 1) / rings, -thickness * (i + 1) / rings);
        if (inner.right - inner.left < 2 || inner.bottom - inner.top < 2) break;
        int saved = SaveDC(dc);
        IntersectClipRect(dc, outer.left, outer.top, outer.right, outer.bottom);
        ExcludeClipRect(dc, inner.left, inner.top, inner.right, inner.bottom);
        DrawScreenStatic(dc, outer, step + i * 31, level * (rings - i) / rings);
        RestoreDC(dc, saved);
    }
}

// 아직 멀쩡한 사각형을 시간에 따라 좁혀 가며, 그 바깥은 빈틈없이 덮고 경계
// 안쪽은 옅게 갉아 놓는다. 네 변의 위치가 매번 조금씩 흔들려 경계가 물어뜯긴
// 것처럼 보인다.
void DrawCreepStatic(HDC dc, const RECT& area, int step, int eaten) {
    if (eaten <= 0) return;
    if (eaten > 1000) eaten = 1000;
    int w = area.right - area.left, h = area.bottom - area.top;
    if (w <= 4 || h <= 4) return;
    int insetX = w * eaten / 2000, insetY = h * eaten / 2000;   // 1000이면 정확히 닫힌다
    int wobble = 6 + eaten * 24 / 1000;
    RECT clean = area;
    clean.left += insetX + (int)(Hash3(step / 3, 1, 0) % (uint32_t)(wobble * 2 + 1)) - wobble;
    clean.right -= insetX + (int)(Hash3(step / 3, 2, 0) % (uint32_t)(wobble * 2 + 1)) - wobble;
    clean.top += insetY + (int)(Hash3(step / 3, 3, 0) % (uint32_t)(wobble * 2 + 1)) - wobble;
    clean.bottom -= insetY + (int)(Hash3(step / 3, 4, 0) % (uint32_t)(wobble * 2 + 1)) - wobble;
    if (clean.left < area.left) clean.left = area.left;
    if (clean.top < area.top) clean.top = area.top;
    if (clean.right > area.right) clean.right = area.right;
    if (clean.bottom > area.bottom) clean.bottom = area.bottom;
    if (clean.right - clean.left < 24 || clean.bottom - clean.top < 24) {
        DrawScreenStatic(dc, area, step, 1000);   // 다 먹혔다
        return;
    }
    int saved = SaveDC(dc);
    ExcludeClipRect(dc, clean.left, clean.top, clean.right, clean.bottom);
    DrawScreenStatic(dc, area, step, 1000);
    RestoreDC(dc, saved);
    // 아직 남은 안쪽도 경계부터 갉히기 시작한다.
    DrawEdgeStatic(dc, clean, step + 13, 700, 26 + eaten * 70 / 1000);
}

// 바깥 테두리가 가장 진하고 안쪽으로 갈수록 배경색에 녹아든다.
void DrawEdgeGlow(HDC dc, const RECT& area, COLORREF color, int level, int thickness) {
    if (level <= 0 || thickness <= 0) return;
    if (level > 1000) level = 1000;
    RECT r = area;
    for (int i = 0; i < thickness; ++i) {
        if (r.right - r.left < 2 || r.bottom - r.top < 2) break;
        int fade = level * (thickness - i) / thickness;
        HBRUSH brush = CreateSolidBrush(MixColor(C_BG, color, fade * 100 / 1000));
        FrameRect(dc, &r, brush);
        DeleteObject(brush);
        InflateRect(&r, -1, -1);
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

// ---------------------------------------------------------------------------
// 전투 연출 프리미티브. 게임 규칙을 모르고 넘겨받은 숫자만 그린다.
// ---------------------------------------------------------------------------

// 3구간 직각 경로(세로 → 가로 → 세로) 위에서 거리 d에 해당하는 점.
static POINT PathPointAt(POINT from, POINT to, int midY, int d) {
    POINT p = from;
    int a = midY - from.y, b = to.x - from.x, c = to.y - midY;
    int la = a < 0 ? -a : a, lb = b < 0 ? -b : b, lc = c < 0 ? -c : c;
    if (d <= la) { p.x = from.x; p.y = from.y + (a < 0 ? -d : d); return p; }
    d -= la;
    if (d <= lb) { p.x = from.x + (b < 0 ? -d : d); p.y = midY; return p; }
    d -= lb;
    if (d > lc) d = lc;
    p.x = to.x; p.y = midY + (c < 0 ? -d : d);
    return p;
}

void DrawSignalPath(HDC dc, POINT from, POINT to, int midY, int progress, int packets,
                    COLORREF color, int trail, int branch) {
    int a = midY - from.y, b = to.x - from.x, c = to.y - midY;
    int total = (a < 0 ? -a : a) + (b < 0 ? -b : b) + (c < 0 ? -c : c);
    if (total <= 0 || packets <= 0) return;
    if (progress < 0) progress = 0;
    if (progress > 1000) progress = 1000;
    COLORREF faint = MixColor(C_BG, color, 26);
    // 지나온 자리만 어둡게 남는다. 아직 지나지 않은 구간은 그리지 않아
    // 신호가 "이미 도착해 있다"로 잘못 읽히지 않는다.
    int head = total * progress / 1000;
    for (int d = 0; d < head; d += 4) {
        POINT p = PathPointAt(from, to, midY, d);
        Fill(dc, MakeRect(p.x - 1, p.y - 1, p.x + 1, p.y + 1), faint);
        if (branch) {
            Fill(dc, MakeRect(p.x - 1 - branch, p.y - 1, p.x + 1 - branch, p.y + 1), faint);
            Fill(dc, MakeRect(p.x - 1 + branch, p.y - 1, p.x + 1 + branch, p.y + 1), faint);
        }
    }
    COLORREF hot = MixColor(color, RGB(255, 255, 255), 45);
    for (int i = 0; i < packets; ++i) {
        int d = head - i * (trail > 0 ? trail : 10);
        if (d < 0) continue;
        POINT p = PathPointAt(from, to, midY, d);
        COLORREF tone = i == 0 ? hot : MixColor(C_BG, color, 90 - i * 22);
        if (branch) {
            Fill(dc, MakeRect(p.x - 3 - branch, p.y - 2, p.x + 3 - branch, p.y + 2), tone);
            Fill(dc, MakeRect(p.x - 3 + branch, p.y - 2, p.x + 3 + branch, p.y + 2), tone);
        } else {
            Fill(dc, MakeRect(p.x - 4, p.y - 3, p.x + 4, p.y + 3), tone);
        }
    }
}

void DrawPixelBurst(HDC dc, int cx, int cy, int t, int life, int count, int seed, COLORREF color) {
    if (t < 0 || t >= life || life <= 0 || count <= 0) return;
    int p = t * 1000 / life;
    int fade = 100 - p / 10;
    if (fade <= 2) return;
    COLORREF c = MixColor(C_BG, color, fade);
    COLORREF hot = MixColor(c, RGB(255, 255, 255), 45);
    for (int i = 0; i < count; ++i) {
        uint32_t h = Hash3(seed, i, 23);
        int dx = (int)(h % 201u) - 100, dy = (int)((h >> 9) % 161u) - 110;
        int speed = 40 + (int)((h >> 19) % 60u);
        int x = cx + dx * p * speed / 260000;
        int y = cy + dy * p * speed / 260000 + p * p / 14000;   // 중력에 처진다
        if (x < 0 || x >= BASE_WIDTH || y < 68 || y >= BASE_HEIGHT) continue;
        int w = 2 + (int)((h >> 5) % 3u);
        Fill(dc, MakeRect(x, y, x + w, y + w), (i & 3) == 0 ? hot : c);
    }
}

void DrawBandGlitch(HDC dc, const RECT& area, int t, int amp, int seed, int bands) {
    if (amp <= 0 || bands <= 0) return;
    int h = (area.bottom - area.top) / bands;
    if (h <= 0) return;
    int w = area.right - area.left;
    for (int i = 0; i < bands; ++i) {
        int y = area.top + i * h;
        int dx = (int)(Hash3(seed, i, t / 25) % (uint32_t)(amp * 2 + 1)) - amp;
        if (dx != 0) BitBlt(dc, area.left + dx, y, w, h, dc, area.left, y, SRCCOPY);
    }
}

void DrawPulseFrame(HDC dc, const RECT& area, int expand, int layers, COLORREF color) {
    if (layers <= 0) return;
    for (int i = 0; i < layers; ++i) {
        RECT r = area;
        InflateRect(&r, expand * (i + 1), expand * (i + 1));
        int level = 90 - i * 90 / (layers + 1);
        if (level <= 3) continue;
        Outline(dc, r, MixColor(C_BG, color, level), 1);
    }
}

void DrawGhostBar(HDC dc, const RECT& area, int value, int ghost, int maximum,
                  COLORREF color, COLORREF ghostColor) {
    Fill(dc, area, RGB(39, 48, 57));
    int width = area.right - area.left;
    if (maximum > 0) {
        if (ghost > value) {
            RECT part = area;
            part.right = area.left + width * (ghost > maximum ? maximum : ghost) / maximum;
            Fill(dc, part, ghostColor);
        }
        if (value > 0) {
            RECT part = area;
            part.right = area.left + width * (value > maximum ? maximum : value) / maximum;
            Fill(dc, part, color);
            // 줄어드는 경계에 밝은 머리를 세워 어디까지 깎였는지 눈에 박히게 한다.
            if (ghost > value) Fill(dc, MakeRect(part.right - 1, part.top, part.right + 1, part.bottom),
                MixColor(color, RGB(255, 255, 255), 60));
        }
    }
    Outline(dc, area, C_LINE, 1);
}

void DrawPacketGrid(HDC dc, const RECT& area, int filled, int total, COLORREF color, COLORREF dim) {
    if (total <= 0) return;
    if (total > 12) total = 12;
    int width = area.right - area.left;
    int cell = width / total;
    if (cell < 3) return;
    for (int i = 0; i < total; ++i) {
        RECT r = MakeRect(area.left + i * cell + 1, area.top, area.left + (i + 1) * cell - 2, area.bottom);
        if (i < filled) Fill(dc, r, color);
        else Outline(dc, r, dim, 1);
    }
}
