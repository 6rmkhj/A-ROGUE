#pragma once

#include <windows.h>
#include <stdint.h>
#include "game.h"

// GDI 위에 얹은 얇은 그리기 도구. 게임 규칙을 모르고, 넘겨받은 값만 그린다.
// 모든 그리기는 아래 고정 캔버스에 이뤄지고, 마지막에 창 크기로 확대된다.

static const int BASE_WIDTH = 1120, BASE_HEIGHT = 760;

static const COLORREF C_BG = RGB(8, 12, 17), C_PANEL = RGB(16, 23, 31), C_PANEL_2 = RGB(23, 33, 43);
static const COLORREF C_LINE = RGB(50, 71, 87), C_TEXT = RGB(218, 232, 238), C_DIM = RGB(120, 145, 157);
static const COLORREF C_GREEN = RGB(82, 231, 174), C_RED = RGB(255, 92, 82), C_YELLOW = RGB(255, 204, 75), C_BLUE = RGB(83, 170, 255);
static const COLORREF C_INK = RGB(6, 10, 15);

extern HFONT gFontSmall, gFontMedium, gFontLarge, gFontHuge;
void CreateRenderFonts();
void DestroyRenderFonts();

RECT MakeRect(int l, int t, int r, int b);
int Inside(const RECT& rect, int x, int y);
void ComputeCanvasTransform(int clientW, int clientH, float* scale, int* offsetX, int* offsetY);
POINT ScreenToCanvas(HWND window, int x, int y);

void Fill(HDC dc, const RECT& rect, COLORREF color);
void Outline(HDC dc, const RECT& rect, COLORREF color, int thickness);
void Panel(HDC dc, const RECT& rect, COLORREF fillColor, COLORREF borderColor);
void Text(HDC dc, int x, int y, const wchar_t* value, COLORREF color, HFONT font);
void TextRect(HDC dc, const RECT& rect, const wchar_t* value, COLORREF color, HFONT font, UINT flags);
void Bar(HDC dc, const RECT& rect, int value, int maximum, COLORREF color);
int TextWidth(HDC dc, const wchar_t* value, HFONT font);
COLORREF MixColor(COLORREF from, COLORREF to, int amount);

// ---- 애니메이션 어휘 -------------------------------------------------------
// 정수 0~1000만 쓴다. 프레임이 시간의 순수 함수라는 규약을 지키기 위해 상태를
// 들고 다니지 않는다.
int Track(int t, int fromMs, int toMs);   // 구간 진행도. 구간 밖은 0/1000으로 물린다
int Lerp(int a, int b, int p);
int EaseOutCubic(int p);
int EaseInCubic(int p);
int ShutterFall(int p);                   // 가속 낙하 뒤 두 번 작게 튀는 셔터 곡선
int EaseOutBack(int p);                   // 목표를 지나쳤다 되돌아온다
int EaseOutBounce(int p);                 // 바닥에서 몇 번 튄다

// ---- 프레임 스냅샷 ---------------------------------------------------------
// 연출이 시작되는 순간의 캔버스를 붙잡아 두고, 연출 동안 어긋나게·옅게 겹친다.
// 잔상·트레일·데이터모싱이 전부 여기서 나온다. 게임 상태가 아니라 그리기 버퍼다.
void FxSnapshotCapture(HDC canvas);
int  FxSnapshotHeld();
void FxSnapshotBlit(HDC dc, const RECT& area, int dx, int dy, int keepPercent);
void FxSnapshotRelease();
void FxSnapshotDestroy();

// ---- 스프라이트 변형 -------------------------------------------------------
// DrawSpriteArt는 정수 균일 배율이라 늘어나지 않는다. 작은 오프스크린에 그린 뒤
// StretchBlt로 늘려 스쿼시/스트레치를 낸다. 바닥을 기준으로 변한다 (천분율).
void DrawSpriteStretched(HDC dc, const RECT& box, int kind, int alive, int flash, int sxMille, int syMille);

// ---- 셔터 -----------------------------------------------------------------
// 슬롯 하나를 덮으며 내려오는 철문. descended는 내려온 픽셀 높이다.
// 무늬는 슬롯 위쪽에 고정되므로 늘어나지 않고 아래로 드러난다.
#define SHUTTER_MESH  0   // 마름모 격자 — 안이 비친다
#define SHUTTER_CORR  1   // 골판 — 완전히 막는다
#define SHUTTER_SLAT  2   // 단열 슬랫 — 가장 두껍다
void DrawShutter(HDC dc, const RECT& slot, int style, int descended, int rattle, COLORREF tint);

void FormatFace(const Face* face, wchar_t* out);
COLORREF FaceColor(const Face* face);
void AppendStatus(wchar_t* output, const wchar_t* status);

// shiftX/bob은 상자는 그대로 두고 도트 그림만 밀어낸다 (달려드는 타격 연출).
void DrawSpriteArt(HDC dc, const RECT& box, int kind, int alive, int flash, int bob, int shiftX);
void DrawPortrait(HDC dc, const RECT& box, int kind, int alive, int selected, int flash, int bob, int shiftX);

// 섹터를 판독하는 듯한 노이즈 연출. 주사위 판독과 볼륨 진입 화면이 함께 쓴다.
uint32_t Hash3(int a, int b, int c);
void DrawSectorStatic(HDC dc, const RECT& area, int die, int step, int level);
void DrawSectorHex(HDC dc, const RECT& area, int die, int step, int level);
void DrawScanlines(HDC dc, const RECT& area);
void DrawTornValue(HDC dc, const RECT& area, const wchar_t* value, COLORREF color, int die, int step, int level);

// 미판독 정보를 가리는 노이즈.
//   CorruptCode  ASCII 코드명을 길이 그대로 헥스·기호로 갈아 끼운다 (폭이 유지된다).
//   DrawHexBlock 사각형을 헥스 덤프로 채운다. 한글 설명문을 통째로 덮을 때 쓴다.
// 둘 다 tick(밀리초)을 받아 글자·칸마다 다른 주기로 흔들린다. 줄이 통째로 갈리는
// 대신 낱글자가 제각기 튀므로, 고정된 노이즈가 아니라 지금 뚫리는 중으로 보인다.
//   DrawGlitchLine 고정폭 노이즈 한 줄을 그리되 낱글자 몇 개만 밝게 띄운다.
//                  반짝이는 자리와 시점이 글자마다 달라 줄 단위로 밝아지는 것보다
//                  훨씬 살아 있어 보인다. ASCII 고정폭 문자열에만 쓸 것.
void CorruptCode(const wchar_t* source, wchar_t* out, int cap, int seed, uint32_t tick);
void DrawGlitchLine(HDC dc, int x, int y, const wchar_t* text, COLORREF dim, COLORREF lit,
                    HFONT font, int seed, uint32_t tick);
void DrawHexBlock(HDC dc, const RECT& area, COLORREF color, int seed, uint32_t tick, int rows);

// 화면 전체를 삼키는 노이즈. level은 0(깨끗)~1000(완전 백색소음)이고
// 그 값이 그대로 화면을 덮는 비율이 된다 (1000이면 빈틈이 없다).
void DrawScreenStatic(HDC dc, const RECT& area, int step, int level);
// 같은 노이즈를 가장자리 띠(thickness px) 안에만 그린다. 바깥 테두리가 가장
// 짙고 안쪽으로 갈수록 옅어져, 화면 중앙은 건드리지 않는다.
void DrawEdgeStatic(HDC dc, const RECT& area, int step, int level, int thickness);
// 잠식. 가장자리부터 노이즈가 화면을 갉아먹으며 안으로 좁혀 들어온다.
// eaten 0(멀쩡함) ~ 1000(남은 데가 없음). 경계는 매 프레임 흔들려 갉히는 것처럼 보인다.
void DrawCreepStatic(HDC dc, const RECT& area, int step, int eaten);
// 가장자리에서 안쪽으로 옅어지는 경고 테두리. 피격·위독 상태를 알린다.
void DrawEdgeGlow(HDC dc, const RECT& area, COLORREF color, int level, int thickness);

// ---- 전투 연출 프리미티브 --------------------------------------------------
// 전부 게임 상태를 모르고 넘겨받은 숫자만 그린다. 위치·강도는 호출자가 경과
// 시간의 순수 함수로 계산해 넘긴다 (프레임마다 누적하는 상태가 없어야 한다).

// 직각 회로 신호. from에서 midY까지 세로로, 거기서 가로로, 다시 to까지 세로로
// 꺾어 가는 경로 위를 packets개의 짧은 사각형이 흘러간다. progress 0~1000,
// trail은 꼬리 간격(px), branch > 0이면 두 갈래로 갈라져 나란히 간다.
void DrawSignalPath(HDC dc, POINT from, POINT to, int midY, int progress, int packets,
                    COLORREF color, int trail, int branch);
// 중심에서 흩어지는 작은 사각형 파편. 씨앗이 같으면 궤적도 같다.
void DrawPixelBurst(HDC dc, int cx, int cy, int t, int life, int count, int seed, COLORREF color);
// 사각형 안쪽만 가로 띠로 잘라 좌우로 어긋나게 복사한다 (초상화 밴드 글리치).
void DrawBandGlitch(HDC dc, const RECT& area, int t, int amp, int seed, int bands);
// 테두리가 바깥으로 겹겹이 퍼져 나가는 펄스. layers겹이 expand px 간격으로 선다.
void DrawPulseFrame(HDC dc, const RECT& area, int expand, int layers, COLORREF color);
// 잔상이 남는 게이지. ghost가 value보다 크면 그 차이가 예전 값의 잔상으로 남는다.
void DrawGhostBar(HDC dc, const RECT& area, int value, int ghost, int maximum,
                  COLORREF color, COLORREF ghostColor);
// [■][■][ ][ ] 형태의 칸 게이지. 압력·오염처럼 눈금이 중요한 값에 쓴다.
void DrawPacketGrid(HDC dc, const RECT& area, int filled, int total, COLORREF color, COLORREF dim);
