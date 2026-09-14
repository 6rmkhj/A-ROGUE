#include <windows.h>
#include "ui.h"
#include "render.h"
#include "audio.h"
#include "fx_draw.h"
#include "presentation.h"
#include "localization.h"

// 창 모드 복원 정보는 설정 화면만 쓰므로 여기 둔다.
static int gWindowedScale = 100;
static RECT gWindowedRect;

RECT GuideButtonRect(int width) { return MakeRect(width - 148, 4, width - 18, 23); }
RECT GuideCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT GuidePrevRect(int width, int height) { (void)width; return MakeRect(84, height - 74, 234, height - 40); }
RECT GuideNextRect(int width, int height) { return MakeRect(width - 234, height - 74, width - 84, height - 40); }
RECT GuideTabRect(int page) {
    int w = (BASE_WIDTH - 168 - 8 * (GUIDE_PAGE_COUNT - 1)) / GUIDE_PAGE_COUNT, left = 84 + page * (w + 8);
    return MakeRect(left, 146, left + w, 180);
}
RECT SettingsButtonRect(int width) { return MakeRect(width - 148, 25, width - 18, 44); }
RECT SettingsCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT DeckButtonRect(int width) { return MakeRect(width - 148, 46, width - 18, 65); }
RECT DeckCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT ScaleOptionRect(int index) { int left = 84 + index * 130; return MakeRect(left, 260, left + 112, 302); }
RECT LanguageOptionRect(int index) { int left = 84 + index * 150; return MakeRect(left, 164, left + 132, 206); }
// 오른쪽 열. 설정 화면은 왼쪽 364px만 쓰고 나머지가 비어 있었다. 화면 배율 행은
// x=716까지 뻗으므로 겹치지 않게 한 행 아래(전체화면과 같은 높이)에 둔다.
// 판정용 사각형은 홈보다 두껍다. 얇은 선을 정확히 집어야 하면 쓰기 나쁘다.
// 캔버스가 넓어진 만큼 오른쪽 열도 밀어 두 열이 패널 양쪽을 고르게 쓰게 한다.
static const int SETTINGS_COL2 = 560 + (BASE_WIDTH - LEGACY_WIDTH);
RECT VolumeSliderRect(int channel) {
    int top = 370 + channel * 50;
    return MakeRect(SETTINGS_COL2 + 130, top, SETTINGS_COL2 + 400, top + 32);
}

RECT VolumeHandleRect(int channel, int volume) {
    RECT r = VolumeSliderRect(channel);
    int travel = (r.right - r.left) - VOL_HANDLE_W;
    int left = r.left + travel * volume / 100;
    return MakeRect(left, r.top, left + VOL_HANDLE_W, r.bottom);
}

int VolumeFromX(int channel, int x) {
    RECT r = VolumeSliderRect(channel);
    int travel = (r.right - r.left) - VOL_HANDLE_W;
    if (travel < 1) return 0;
    int v = (x - r.left - VOL_HANDLE_W / 2) * 100 / travel;
    return v < 0 ? 0 : v > 100 ? 100 : v;
}
RECT FullscreenToggleRect() { return MakeRect(84, 380, 364, 422); }
// 소리 슬라이더 안내문(y434~458) 아래로 내려온다. 468이면 라벨이 안내문을 덮는다.
RECT BgmToggleRect() { return MakeRect(SETTINGS_COL2 + 40, 420, SETTINGS_COL2 + 115, 452); }
RECT RestartButtonRect() { return MakeRect(84, 460, 364, 502); }
RECT CampaignResetRect() { return MakeRect(SETTINGS_COL2, 658, SETTINGS_COL2 + 280, 700); }
RECT ReplayPrevRect() { return MakeRect(LEGACY_X + 370, 650, LEGACY_X + 545, 688); }
RECT ReplayNextRect() { return MakeRect(LEGACY_X + 575, 650, LEGACY_X + 750, 688); }
RECT FxLevelRect(int index) { int left = 84 + index * 150; return MakeRect(left, 592, left + 132, 634); }

// 창 모드로 되돌아갈 때 복원할 위치/크기를 저장해 두고, 모니터 전체를 덮는 테두리 없는 창으로 전환한다.
void ApplyFullscreen(int enable) {
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

int WindowedScale() { return gWindowedScale; }

// BASE_WIDTH x BASE_HEIGHT 캔버스를 percent%로 표시할 창 크기를 계산해 적용한다 (창 모드에서만 의미가 있다).
void ApplyWindowedScale(int percent) {
    gWindowedScale = percent;
    if (gFullscreen) ApplyFullscreen(0);
    RECT desired = {0, 0, BASE_WIDTH * percent / 100, BASE_HEIGHT * percent / 100};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int width = desired.right - desired.left, height = desired.bottom - desired.top;

    MONITORINFO info = {}; info.cbSize = sizeof(info);
    HMONITOR monitor = MonitorFromWindow(gWindow, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(monitor, &info)) {
        info.rcWork = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }
    int workW = info.rcWork.right - info.rcWork.left;
    int workH = info.rcWork.bottom - info.rcWork.top;
    if (width > workW) width = workW;
    if (height > workH) height = workH;
    int x = info.rcWork.left + (workW - width) / 2;
    int y = info.rcWork.top + (workH - height) / 2;
    SetWindowPos(gWindow, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED);
}

// 위에서 내려온 콘솔 한 장. 게임 화면 위에 마지막으로 얹히므로 어떤 연출이
// 진행 중이어도 가려지지 않는다.
void DrawTerminal(HDC dc, int width, int height) {
    (void)height;
    const int lineH = 20;
    int bodyH = 32 + TERM_LOG_LINES * lineH + 34;
    RECT panel = MakeRect(0, 0, width, bodyH);
    Fill(dc, panel, RGB(4, 9, 7));
    DrawScanlines(dc, panel);
    Fill(dc, MakeRect(0, bodyH - 2, width, bodyH), C_GREEN);
    Text(dc, 18, 7, L"A:\\ROGUE  ADMIN CONSOLE   [`] 닫기   [help] 명령 목록", C_DIM, gFontSmall);

    int y = 30;
    for (int i = 0; i < gTermLogCount; ++i) {
        // 되울린 입력은 흐리게, 결과는 초록으로 둔다.
        Text(dc, 18, y, gTermLog[i], gTermLog[i][0] == L'>' ? C_DIM : C_GREEN, gFontSmall);
        y += lineH;
    }

    // 입력 줄은 로그 길이와 무관하게 맨 아래 고정이다.
    int inputY = bodyH - 28;
    Text(dc, 18, inputY, L"A:\\>", C_YELLOW, gFontSmall);
    int caret = 18 + TextWidth(dc, L"A:\\> ", gFontSmall);
    if (gTermInputLen > 0) {
        Text(dc, caret, inputY, gTermInput, C_TEXT, gFontSmall);
        caret += TextWidth(dc, gTermInput, gFontSmall);
    }
    Fill(dc, MakeRect(caret + 1, inputY + 3, caret + 10, inputY + 17), C_GREEN);
}

// ---------------------------------------------------------------------------
// 캠페인 진행도 표시
//
// 여섯 볼륨의 복구 상태를 한 줄로 보여 준다. 타이틀·볼륨 선택·설정이 같은 그림을
// 보므로 세 화면의 표기가 어긋날 수 없다. offeredMask는 지금 카드에 올라온 볼륨을
// 테두리로 구분하려는 것이고, 없으면 0을 넘긴다.
// ---------------------------------------------------------------------------
static int ShardChipWidth(int compact) { return compact ? 44 : 56; }

static int ShardStripWidth(int compact) {
    int gap = compact ? 6 : 8;
    return 6 * ShardChipWidth(compact) + 5 * gap;
}

static void DrawShardStrip(HDC dc, int left, int top, uint8_t clearedMask, uint8_t offeredMask, int compact) {
    int cell = ShardChipWidth(compact), gap = compact ? 6 : 8, h = compact ? 24 : 28;
    for (int i = 0; i < 6; ++i) {
        RECT chip = MakeRect(left + i * (cell + gap), top, left + i * (cell + gap) + cell, top + h);
        int recovered = clearedMask & (1u << i);
        int offered = offeredMask & (1u << i);
        COLORREF tone = (COLORREF)DRIVE_INFO[i].color;
        Panel(dc, chip, recovered ? MixColor(C_PANEL, tone, 26) : C_PANEL_2,
              recovered ? tone : offered ? MixColor(C_LINE, tone, 60) : C_LINE);
        TextRect(dc, chip, DRIVE_INFO[i].letter, recovered ? tone : offered ? C_TEXT : C_DIM,
                 gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// 진행도 한 줄: "복구된 조각 3 / 6". 조각 수는 세 화면이 같은 함수에서 읽는다.
static void FormatShardProgress(uint8_t clearedMask, wchar_t* out) {
    wsprintfW(out, L"복구된 조각 %d / 6", RecoveredShardCount(clearedMask));
}

static void DrawSettings(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"설정", C_GREEN, gFontLarge);
    RECT close = SettingsCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, MakeRect(panel.left + 140, panel.top + 26, close.left - 20, panel.top + 50), L"취소 키로 닫을 수 있습니다.", C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    Text(dc, 84, 132, L"언어", C_YELLOW, gFontMedium);
    static const wchar_t* const LANGUAGE_NAMES[LANGUAGE_COUNT] = {L"한국어", L"English"};
    // translations.tsv가 없으면 English를 골라도 한국어가 그대로 나온다. 고를 수
    // 있게 두면 설정이 고장난 것처럼 보이므로 잠그고 이유를 적는다.
    int englishReady = TranslationsLoaded();
    for (int i = 0; i < LANGUAGE_COUNT; ++i) {
        RECT r = LanguageOptionRect(i); int active = UiLanguage() == i;
        int usable = i != LANGUAGE_ENGLISH || englishReady;
        int hover = usable && Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, active ? RGB(28, 70, 57) : hover ? RGB(28, 39, 48) : C_PANEL_2,
            active ? C_GREEN : hover ? C_BLUE : C_LINE);
        TextRect(dc, r, LANGUAGE_NAMES[i], active ? C_GREEN : usable ? C_TEXT : C_DIM, gFontMedium,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (!englishReady)
        TextRect(dc, MakeRect(84, 208, panel.right - 30, 226),
            L"translations.tsv를 찾지 못해 English를 쓸 수 없습니다. 실행 파일과 같은 폴더에 두십시오.",
            C_RED, gFontSmall, DT_SINGLELINE);

    Text(dc, 84, 228, L"화면 배율", C_YELLOW, gFontMedium);
    for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) {
        RECT r = ScaleOptionRect(i); int active = !gFullscreen && gWindowedScale == SCALE_OPTIONS[i]; int hover = Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, active ? RGB(28, 70, 57) : hover ? RGB(28, 39, 48) : C_PANEL_2, active ? C_GREEN : hover ? C_BLUE : C_LINE);
        wchar_t label[16]; wsprintfW(label, L"%d%%", SCALE_OPTIONS[i]);
        TextRect(dc, r, label, active ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(84, 312, panel.right - 30, 336), L"전체화면에서는 적용되지 않습니다.", C_DIM, gFontSmall, DT_SINGLELINE);

    // Master / BGM / SFX를 같은 영역에서 독립적으로 조절한다.
    Text(dc, SETTINGS_COL2, 342, L"오디오", C_YELLOW, gFontMedium);
    static const wchar_t* const volumeNames[AUDIO_VOLUME_COUNT] = {L"Master", L"BGM", L"SFX"};
    int volumes[AUDIO_VOLUME_COUNT] = {AudioVolume(), AudioMusicVolume(), AudioSfxVolume()};
    for (int channel = 0; channel < AUDIO_VOLUME_COUNT; ++channel) {
        int vol = volumes[channel];
        RECT slider = VolumeSliderRect(channel);
        RECT handle = VolumeHandleRect(channel, vol);
        int hover = Inside(slider, gMouse.x, gMouse.y);
        int mid = (slider.top + slider.bottom) / 2;
        Text(dc, SETTINGS_COL2, slider.top + 5, volumeNames[channel], channel == AUDIO_VOLUME_MASTER ? C_GREEN : C_TEXT, gFontSmall);
        RECT groove = MakeRect(slider.left, mid - 3, slider.right, mid + 3);
        Panel(dc, groove, C_PANEL_2, C_LINE);
        int filled = (handle.left + handle.right) / 2;
        if (filled > groove.left) Fill(dc, MakeRect(groove.left + 1, groove.top + 1, filled, groove.bottom - 1), vol == 0 ? C_LINE : C_GREEN);
        for (int t = 0; t <= 4; ++t) {
            int tx = slider.left + VOL_HANDLE_W / 2 + ((slider.right - slider.left - VOL_HANDLE_W) * t / 4);
            Fill(dc, MakeRect(tx, groove.bottom + 2, tx + 1, groove.bottom + 6), C_LINE);
        }
        Panel(dc, handle, hover ? RGB(28, 70, 57) : C_PANEL_2, hover ? C_GREEN : C_LINE);
        Fill(dc, MakeRect(handle.left + 6, handle.top + 7, handle.left + 10, handle.bottom - 7), vol == 0 ? C_DIM : C_GREEN);
        wchar_t label[24];
        if (vol == 0) lstrcpyW(label, L"음소거"); else wsprintfW(label, L"%d%%", vol);
        TextRect(dc, MakeRect(slider.right + 8, slider.top, SETTINGS_COL2 + 500, slider.bottom), label, vol == 0 ? C_DIM : C_GREEN, gFontSmall, DT_VCENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(SETTINGS_COL2, 510, panel.right - 30, 532), L"끌거나 좌우 방향키로 조절합니다.", C_DIM, gFontSmall, DT_SINGLELINE);

    // 캠페인 진행도. 지우는 것이 런 하나가 아니라 세이브 전체라 런 초기화와
    // 멀리 떨어뜨려 두고, 확정도 따로 받는다.
    Text(dc, SETTINGS_COL2, 552, L"캠페인 진행도", C_YELLOW, gFontMedium);
    DrawShardStrip(dc, SETTINGS_COL2, 580, gGame.clearedMask, 0, 1);
    wchar_t progress[96], seen[64];
    FormatShardProgress(gGame.clearedMask, progress);
    Text(dc, SETTINGS_COL2, 610, progress, C_TEXT, gFontSmall);
    int endings = 0;
    for (int i = 0; i < ENDING_COUNT; ++i) if (gGame.seenEndingMask & (1u << i)) ++endings;
    wsprintfW(seen, L"기록한 최종 명령 %d / %d", endings, ENDING_COUNT);
    Text(dc, SETTINGS_COL2, 630, gGame.clearedMask == 0x3F ? seen : L"최종 볼륨 잠김", C_DIM, gFontSmall);
    RECT reset = CampaignResetRect(); int hoverReset = Inside(reset, gMouse.x, gMouse.y);
    Panel(dc, reset, gCampaignResetArmed ? RGB(80, 30, 30) : hoverReset ? RGB(48, 28, 28) : C_PANEL_2,
        gCampaignResetArmed || hoverReset ? C_RED : C_LINE);
    TextRect(dc, reset, gCampaignResetArmed ? L"정말 진행도 삭제?" : L"진행도 초기화",
        gCampaignResetArmed ? C_RED : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(SETTINGS_COL2, 704, panel.right - 30, 728),
        gCampaignResetArmed ? L"한 번 더 클릭하면 확정되고 타이틀로 돌아갑니다."
                            : L"복구한 조각과 엔딩 기록을 지우고 처음부터 시작합니다.",
        gCampaignResetArmed ? C_RED : C_DIM, gFontSmall, DT_WORDBREAK);
    RECT bgm = BgmToggleRect(); int hoverBgm = Inside(bgm, gMouse.x, gMouse.y);
    Panel(dc, bgm, AudioMusicEnabled() ? RGB(28, 70, 57) : hoverBgm ? RGB(28, 39, 48) : C_PANEL_2,
        AudioMusicEnabled() ? C_GREEN : hoverBgm ? C_BLUE : C_LINE);
    TextRect(dc, bgm, AudioMusicEnabled() ? L"BGM ON" : L"BGM OFF", AudioMusicEnabled() ? C_GREEN : C_DIM,
        gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    Text(dc, 84, 348, L"전체화면", C_YELLOW, gFontMedium);
    RECT fs = FullscreenToggleRect(); int hoverFs = Inside(fs, gMouse.x, gMouse.y);
    Panel(dc, fs, gFullscreen ? RGB(28, 70, 57) : hoverFs ? RGB(28, 39, 48) : C_PANEL_2, gFullscreen ? C_GREEN : hoverFs ? C_BLUE : C_LINE);
    TextRect(dc, fs, gFullscreen ? L"전체화면 끄기" : L"전체화면 켜기", gFullscreen ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    Text(dc, 84, 428, L"런 초기화", C_YELLOW, gFontMedium);
    RECT rs = RestartButtonRect(); int hoverRs = Inside(rs, gMouse.x, gMouse.y);
    Panel(dc, rs, gRestartArmed ? RGB(80, 30, 30) : hoverRs ? RGB(48, 28, 28) : C_PANEL_2, gRestartArmed ? C_RED : hoverRs ? C_RED : C_LINE);
    TextRect(dc, rs, gRestartArmed ? L"정말 다시 시작?" : L"다시 시작", gRestartArmed ? C_RED : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (gRestartArmed) TextRect(dc, MakeRect(84, 506, 364, 526), L"한 번 더 클릭하면 확정됩니다.", C_DIM, gFontSmall, DT_SINGLELINE);

    // 연출 강도. 줄어드는 것은 장식뿐이고, 판을 읽는 데 필요한 정보는
    // 어떤 모드에서도 그대로 남는다.
    Text(dc, 84, 560, L"연출 강도", C_YELLOW, gFontMedium);
    static const wchar_t* const FX_LEVEL_NAMES[FX_LEVEL_COUNT] = {L"FULL", L"REDUCED", L"OFF"};
    for (int i = 0; i < FX_LEVEL_COUNT; ++i) {
        RECT r = FxLevelRect(i); int active = gFxLevel == i; int hover = Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, active ? RGB(28, 70, 57) : hover ? RGB(28, 39, 48) : C_PANEL_2, active ? C_GREEN : hover ? C_BLUE : C_LINE);
        TextRect(dc, r, FX_LEVEL_NAMES[i], active ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(84, 644, 524, 726),
        gFxLevel == FX_OFF ? L"움직이는 장식을 끕니다. 슬롯 잠금·오프라인 주사위·격리 대상 면·해결 순서·압력 게이지·체력 잔상과 피해 숫자는 그대로 보입니다."
        : gFxLevel == FX_REDUCED ? L"흔들림과 파편을 절반으로 줄이고 전역 글리치를 최소화합니다. 필수 정보는 그대로 보입니다."
        : L"모든 장식 효과를 사용합니다. 슬롯 잠금·격리 대상 면 같은 필수 정보는 어떤 모드에서도 숨기지 않습니다.",
        C_DIM, gFontSmall, DT_WORDBREAK);

}

// 헤더의 체력·용량 묶음. 보상 연출(섹터 복구·상주 설치)이 날아가 닿는 자리도
// 이 사각형이라 그리기와 연출이 같은 좌표를 본다. 넓어진 캔버스에서 영어 용량
// 문구가 가이드 버튼에 닿지 않도록 예전보다 조금 왼쪽에 두고, 세 자리 체력
// ("체력 999/999")이 용량 문구를 덮지 않도록 두 묶음 사이를 벌린다.
static RECT HeaderHpRect(int width) { return MakeRect(width - 495, 7, width - 350, 51); }
static RECT HeaderCapacityRect(int width) { return MakeRect(width - 335, 7, width - 172, 51); }

static void DrawHeader(HDC dc, int width) {
    Fill(dc, MakeRect(0, 0, width, 68), RGB(10, 16, 22)); Fill(dc, MakeRect(0, 67, width, 68), C_GREEN);
    Text(dc, 24, 14, L"A:\\ROGUE", C_GREEN, gFontLarge);
    if (gGame.phase != PHASE_TITLE && gGame.phase != PHASE_DRIVE_SELECT) {
        wchar_t b[128];
        if (gGame.selectedDrive >= 0) {
            wchar_t here[80];
            FormatCurrentDirectory(&gGame, here, 80);
            wsprintfW(b, L"%s  ·  %d층/3  ·  %d구역/3  ·  %d턴", here, gGame.floor + 1, gGame.encounter + 1, DisplayTurn());
            Text(dc, 230, 18, b, C_TEXT, gFontSmall);
            const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
            if (difficulty) {
                wsprintfW(b, L"난이도 %s  ·  %s", difficulty->name, difficulty->brief);
                Text(dc, 230, 38, b, (COLORREF)difficulty->color, gFontSmall);
            }
        } else {
            wsprintfW(b, L"%d층/3  ·  %d구역/3  ·  %d턴", gGame.floor + 1, gGame.encounter + 1, DisplayTurn());
            Text(dc, 230, 14, b, C_TEXT, gFontMedium);
        }
        RECT hpBlock = HeaderHpRect(width), capBlock = HeaderCapacityRect(width);
        int shownHp = PlayerDisplayHp();
        wsprintfW(b, L"체력 %d/%d", shownHp, gGame.playerMaxHp);
        Text(dc, hpBlock.left, 14, b, shownHp <= 10 ? C_RED : C_TEXT, gFontMedium);
        int ghostHp = shownHp;
        for (int i = gGame.combatFxCount - 1; i >= 0 && CombatFxPlaying(); --i) {
            const CombatFxEvent* fx = &gGame.combatFx[i];
            int age = CombatFxElapsed(i);
            if (fx->type != CFX_ENEMY_STRIKE || age < 0) continue;
            if (fx->beforeValue > shownHp && age < 680)
                ghostHp = Lerp(fx->beforeValue, shownHp, EaseOutCubic(Track(age, 130, 680)));
            break;
        }
        DrawGhostBar(dc, MakeRect(hpBlock.left, 44, hpBlock.left + 118, 51), shownHp, ghostHp,
            gGame.playerMaxHp, shownHp <= 10 ? C_RED : C_GREEN, MixColor(C_BG, C_RED, 70));
        wsprintfW(b, L"용량 %dB / %dB", UsedBytes(&gGame), EffectiveCapacity(&gGame));
        Text(dc, capBlock.left, 14, b, UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontSmall);
        DrawGhostBar(dc, MakeRect(capBlock.left, 44, capBlock.left + 140, 51), UsedBytes(&gGame), UsedBytes(&gGame),
            EffectiveCapacity(&gGame), UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_BLUE, C_LINE);
    }
    RECT guide = GuideButtonRect(width); int hover = Inside(guide, gMouse.x, gMouse.y);
    Panel(dc, guide, gGuideOpen ? RGB(32, 82, 67) : hover ? RGB(27, 48, 52) : C_PANEL_2, gGuideOpen || hover ? C_GREEN : C_LINE);
    TextRect(dc, guide, L"가이드 [F1]", gGuideOpen ? C_GREEN : C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT settings = SettingsButtonRect(width); int hoverSettings = Inside(settings, gMouse.x, gMouse.y);
    Panel(dc, settings, gSettingsOpen ? RGB(32, 82, 67) : hoverSettings ? RGB(27, 48, 52) : C_PANEL_2, gSettingsOpen || hoverSettings ? C_GREEN : C_LINE);
    TextRect(dc, settings, L"설정 [F2]", gSettingsOpen ? C_GREEN : C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (gGame.phase != PHASE_TITLE) {
        RECT deck = DeckButtonRect(width); int hoverDeck = Inside(deck, gMouse.x, gMouse.y);
        Panel(dc, deck, gDeckOpen ? RGB(32, 82, 67) : hoverDeck ? RGB(27, 48, 52) : C_PANEL_2, gDeckOpen || hoverDeck ? C_GREEN : C_LINE);
        TextRect(dc, deck, L"덱 [F3]", gDeckOpen ? C_GREEN : C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

RECT StartButtonRect(int width, int height) { return MakeRect(width / 2 - 150, height / 2 + 92, width / 2 + 150, height / 2 + 154); }

// ---------------------------------------------------------------------------
// 타이틀 — 차가운 부팅
//
// 사람이 이 게임에서 가장 먼저 보는 판이다. 예전에는 모든 글자가 첫 프레임에
// 제 색으로 서 있었고 0.7초 뒤에는 완전히 멈춘 그림이 됐다. 이제는 전원이
// 들어와 18개 섹터가 차례로 타 들어가고, 그 불이 지나간 자리에서 서명 → 제목
// → 안내 → 버튼 → 진행도가 차례로 드러난다. 다 선 뒤에도 헤드는 판을 계속 읽는다.
//
// 드러나는 방식은 전부 "덮개를 걷는 것"이다. 글자는 처음부터 제자리에 제 색으로
// 그려져 있고 그 위의 잉크만 물러난다. 글자 색을 시간에 따라 옅게 섞는 방법도
// 있지만, 그러면 진입 코드가 그리기 코드 전체에 스며들고 FX_OFF에서 갈라진다.
// 덮개는 FxDecorOn()이 아니면 아예 그리지 않으므로 OFF가 곧 완성된 화면이 된다 —
// 이 화면에서 진입을 아는 코드는 맨 아래 마스크 호출 몇 줄뿐이다.
// ---------------------------------------------------------------------------
// 덮개 한 장. 가운데에서 위아래로 걷힌다. at 이전에는 완전히 덮고, at+span
// 이후에는 아무것도 그리지 않는다 (다 걷힌 뒤에는 비용도 0이다).
static void DrawRevealMask(HDC dc, const RECT& r, int at, int span, COLORREF tone) {
    if (!FxDecorOn()) return;
    int t = SceneElapsed();
    if (t >= at + span) return;
    int open = t <= at ? 0 : EaseOutCubic(Track(t, at, at + span));
    int half = (r.bottom - r.top + 1) / 2, cover = half - half * open / 1000;
    if (cover <= 0) return;
    Fill(dc, MakeRect(r.left, r.top, r.right, r.top + cover), C_BG);
    Fill(dc, MakeRect(r.left, r.bottom - cover, r.right, r.bottom), C_BG);
    // 문틀은 실제로 걷히기 시작한 뒤에만 그린다. 다 닫혀 있는 동안에는 두 문틀이
    // 한가운데서 겹쳐 배경 위에 가로줄 하나로 남는데, 덮을 몸통이 아직 안 보이는
    // 자리에서는 그것이 셔터가 아니라 떠 있는 선으로 읽힌다.
    if (open <= 0) return;
    COLORREF lip = MixColor(C_BG, tone, FxScale(55));
    Fill(dc, MakeRect(r.left, r.top + cover - 2, r.right, r.top + cover), lip);
    Fill(dc, MakeRect(r.left, r.bottom - cover, r.right, r.bottom - cover + 2), lip);
}

// 글자 한 줄이 찍히는 동작. 남은 덮개가 왼쪽에서 오른쪽으로 물러나고 그 경계에
// 헤드가 선다. 빈 배경이 넓은 줄에는 위아래 셔터가 맞지 않는다 — 셔터는 가릴
// 몸통이 있어야 셔터로 보이고, 없으면 가로줄 두 개가 떠 있는 것으로 읽힌다.
static void DrawWipeMask(HDC dc, const RECT& r, int at, int span, COLORREF tone) {
    if (!FxDecorOn()) return;
    int t = SceneElapsed();
    if (t >= at + span) return;
    int open = t <= at ? 0 : EaseOutCubic(Track(t, at, at + span));
    int head = Lerp(r.left, r.right, open);
    Fill(dc, MakeRect(head, r.top, r.right, r.bottom), C_BG);
    if (open > 0)
        Fill(dc, MakeRect(head, r.top + 2, head + 3, r.bottom - 2), MixColor(C_BG, tone, FxScale(72)));
}

static void DrawTitle(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_TITLE, C_GREEN, width, height);
    int cx = width / 2, scene = SceneElapsed();
    // The title is a boot signature burning into an eighteen-sector disk.
    // 윗줄은 왼쪽부터, 아랫줄은 오른쪽부터 켜진다 — 판의 양면을 함께 훑는다.
    for (int i = 0; i < 18; ++i) {
        int x = cx - 315 + i * 35;
        int age = scene - i * 28;
        int power = !FxDecorOn() ? 40 : age < 0 ? 8 : age < 180 ? 88 : 40;
        COLORREF band = i < 6 ? C_GREEN : i < 12 ? C_BLUE : C_YELLOW;
        Fill(dc, MakeRect(x, 164, x + 27, 168), MixColor(C_BG, band, power));
        int backAge = scene - (17 - i) * 28;
        int back = !FxDecorOn() ? 18 : backAge < 0 ? 5 : backAge < 200 ? 52 : 18;
        Fill(dc, MakeRect(x, 426, x + 27, 428), MixColor(C_BG, C_GREEN, back));
        // 불이 막 닿은 칸에서만 위로 한 번 튄다.
        if (FxDecorOn() && age >= 0 && age < 130)
            Fill(dc, MakeRect(x + 12, 164 - 9 * (130 - age) / 130, x + 16, 168),
                MixColor(C_BG, band, FxScale(72 * (130 - age) / 130)));
    }
    // 다 선 뒤에는 헤드가 아래 열을 천천히 오간다. 타이틀이 멈춘 그림이 되지 않는다.
    if (FxDecorOn() && scene >= TITLE_SETTLE_AT) {
        int tick = (int)(GetTickCount() % 6200u);
        int sweep = tick < 3100 ? EaseOutCubic(Track(tick, 0, 2600))
                                : 1000 - EaseOutCubic(Track(tick, 3100, 5700));
        int hx = Lerp(cx - 315, cx + 312, sweep);
        Fill(dc, MakeRect(hx - 2, 422, hx + 3, 432), MixColor(C_BG, C_GREEN, FxScale(62)));
        Fill(dc, MakeRect(hx - 1, 432, hx + 2, 440), MixColor(C_BG, C_GREEN, FxScale(20)));
    }

    const wchar_t* sign = L"RECOVERY SYSTEM  /  BUILD 17";
    TextRect(dc, MakeRect(0, 187, width, 212), sign, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    // 커서는 서명 오른쪽에 따로 찍는다. 문자열에 붙이면 가운데 정렬이라 깜빡일
    // 때마다 줄 전체가 좌우로 흔들린다.
    if (FxDecorOn() && scene >= TITLE_SIGN_AT + 220 && ((GetTickCount() / 520) & 1)) {
        int signW = TextWidth(dc, sign, gFontSmall);
        Fill(dc, MakeRect(cx + signW / 2 + 8, 192, cx + signW / 2 + 16, 206),
            MixColor(C_BG, C_GREEN, FxScale(58)));
    }

    RECT logo = MakeRect(0, 215, width, 327);
    // 제목은 떨어지지 않는다. 판에서 밀려 올라와 제자리를 한 번 지나쳤다 앉는다.
    int land = FxDecorOn() ? EaseOutBack(Track(scene, TITLE_LOGO_AT, TITLE_LOGO_AT + 470)) : 1000;
    OffsetRect(&logo, 0, (1000 - land) * 30 / 1000);
    RECT extrusion = logo; OffsetRect(&extrusion, 3, 5);
    TextRect(dc, extrusion, L"A:\\ROGUE", RGB(17, 73, 65), gFontTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, logo, L"A:\\ROGUE", C_GREEN, gFontTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    int logoW = TextWidth(dc, L"A:\\ROGUE", gFontTitle);
    RECT glyphs = MakeRect(cx - logoW / 2 - 10, logo.top, cx + logoW / 2 + 10, logo.bottom);
    // 올라오는 동안에는 판독이 아직 끝나지 않아 가로 띠로 어긋난다.
    int tear = scene < TITLE_LOGO_AT ? 0 : 1000 - Track(scene, TITLE_LOGO_AT, TITLE_LOGO_AT + 260);
    if (FxDecorOn() && tear > 0) {
        int amp = FxScale(1 + 22 * tear / 1000);
        for (int i = 0; i < 5; ++i) {
            int top = logo.top + (logo.bottom - logo.top) * i / 5;
            int bottom = logo.top + (logo.bottom - logo.top) * (i + 1) / 5;
            int dx = (int)(Hash3(i, scene / 45, 17) % (uint32_t)(amp * 2 + 1)) - amp;
            int saved = SaveDC(dc);
            IntersectClipRect(dc, glyphs.left - 80, top, glyphs.right + 80, bottom);
            Fill(dc, MakeRect(glyphs.left - 80, top, glyphs.right + 80, bottom), C_BG);
            RECT slid = logo; OffsetRect(&slid, dx, 0);
            TextRect(dc, slid, L"A:\\ROGUE", MixColor(C_BG, C_GREEN, 100 - 28 * tear / 1000),
                gFontTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RestoreDC(dc, saved);
        }
    }
    // 앉는 순간의 충격. 고리가 한 번 퍼지고 테두리가 짧게 겹친다.
    int seat = scene - (TITLE_LOGO_AT + 330);
    if (FxDecorOn() && seat >= 0 && seat < 300) {
        int p = seat * 1000 / 300;
        DrawPulseFrame(dc, glyphs, FxScale(3 + seat / 16), 3,
            MixColor(C_BG, C_GREEN, FxScale(52 * (1000 - p) / 1000)));
        DrawPixelBurst(dc, glyphs.left + 6, (logo.top + logo.bottom) / 2, seat, 300, FxScale(9), 41, C_GREEN);
        DrawPixelBurst(dc, glyphs.right - 6, (logo.top + logo.bottom) / 2, seat, 300, FxScale(9), 77, C_GREEN);
    }
    // 다 선 뒤에도 5.4초마다 제목 위를 빛이 한 번 지나간다.
    if (FxDecorOn() && scene >= TITLE_SETTLE_AT) {
        int tick = (int)(GetTickCount() % 5400u);
        if (tick < 820) {
            int x = Lerp(glyphs.left - 60, glyphs.right + 60, EaseOutCubic(tick * 1000 / 820));
            int saved = SaveDC(dc);
            IntersectClipRect(dc, x - 26, glyphs.top, x + 26, glyphs.bottom);
            TextRect(dc, logo, L"A:\\ROGUE", MixColor(C_GREEN, RGB(228, 255, 245), FxScale(78)),
                gFontTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RestoreDC(dc, saved);
        }
    }

    TextRect(dc, MakeRect(120, 342, width - 120, 416),
        L"18개의 주사위 면이 당신의 덱이자 디스크입니다.\n강한 면은 더 많은 바이트를 차지합니다.\n층이 내려갈수록 줄어드는 용량 안에서 시스템을 복구하십시오.",
        C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    RECT start = StartButtonRect(width, height); int hover = Inside(start, gMouse.x, gMouse.y);
    Panel(dc, start, hover ? RGB(32, 82, 67) : RGB(18, 48, 42), C_GREEN);
    DrawCardMotion(dc, start, C_GREEN, 1, hover);
    // 진행도가 있으면 "새 게임"이 아니라 남은 볼륨을 이어서 고르는 것이다.
    int resuming = gGame.clearedMask != 0;
    TextRect(dc, start, resuming ? L"[ 이어하기 ]" : L"[ 새 게임 ]", C_GREEN, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    // 버튼이 다 열린 뒤에는 테두리가 천천히 숨을 쉰다. 가만히 있는 화면에서도
    // 누를 곳이 어디인지가 계속 눈에 들어온다.
    if (FxDecorOn() && scene >= TITLE_START_AT + 260 && !hover) {
        int pulse = (int)(GetTickCount() % 2600u);
        int glow = pulse < 1300 ? pulse * 1000 / 1300 : (2600 - pulse) * 1000 / 1300;
        Outline(dc, MakeRect(start.left - 4, start.top - 4, start.right + 4, start.bottom + 4),
            MixColor(C_BG, C_GREEN, FxScale(10 + glow * 26 / 1000)), 1);
    }

    int stripW = ShardStripWidth(0);
    wchar_t progress[96];
    FormatShardProgress(gGame.clearedMask, progress);
    TextRect(dc, MakeRect(0, height / 2 + 168, width, height / 2 + 194),
        gGame.clearedMask == 0x3F ? L"여섯 조각이 모두 연결됐습니다 · A:\\ROGUE 개방" : progress,
        gGame.clearedMask == 0x3F ? C_GREEN : gGame.clearedMask ? C_TEXT : C_DIM,
        gFontSmall, DT_CENTER | DT_SINGLELINE);
    DrawShardStrip(dc, (width - stripW) / 2, height / 2 + 200, gGame.clearedMask, 0, 0);
    if (gGame.clearedMask == 0x3F) {
        int endings = 0;
        for (int i = 0; i < ENDING_COUNT; ++i) if (gGame.seenEndingMask & (1u << i)) ++endings;
        wchar_t seen[64]; wsprintfW(seen, L"기록한 최종 명령 %d / %d", endings, ENDING_COUNT);
        TextRect(dc, MakeRect(0, height / 2 + 234, width, height / 2 + 258), seen, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(150, height - 105, width - 150, height - 25),
        L"1·2·3 주사위 선택 / 슬롯 클릭 배치 / Space 실행 / 보상 Esc: 선택 취소 · 선택 없음에서 두 번 눌러 포기",
        C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);

    // 여기까지가 완성된 화면이다. 아래 덮개가 그 위를 걷어 내며 순서를 만든다.
    // 덮개는 화면 가장자리의 섹터 눈금(x=42, width-42)에 닿지 않는 폭으로 둔다.
    DrawWipeMask(dc, MakeRect(cx - 380, 184, cx + 380, 213), TITLE_SIGN_AT, 260, C_GREEN);
    DrawRevealMask(dc, MakeRect(cx - 430, 210, cx + 430, 362), TITLE_LOGO_AT, 300, C_GREEN);
    for (int i = 0; i < 3; ++i)
        DrawWipeMask(dc, MakeRect(140, 341 + i * 25, width - 140, 366 + i * 25),
            TITLE_BLURB_AT + i * 95, 300, C_BLUE);
    DrawRevealMask(dc, MakeRect(start.left - 5, start.top - 5, start.right + 5, start.bottom + 5),
        TITLE_START_AT, 280, C_GREEN);
    DrawWipeMask(dc, MakeRect(cx - 300, height / 2 + 166, cx + 300, height / 2 + 196),
        TITLE_SHARD_AT, 240, C_GREEN);
    // 조각 칸은 DrawShardStrip과 같은 식으로 자리를 낸다. 띠 전체 폭을 6으로
    // 나누면 칸 사이 간격이 칸마다 조금씩 밀려, 여섯 번째에서 덮개가 칸을 벗어난다.
    int chip = ShardChipWidth(0), chipGap = 8, chipLeft = (width - stripW) / 2;
    for (int i = 0; i < 6; ++i) {
        int left = chipLeft + i * (chip + chipGap);
        DrawRevealMask(dc, MakeRect(left - 2, height / 2 + 198, left + chip + 2, height / 2 + 230),
            TITLE_SHARD_AT + 90 + i * 55, 200, (COLORREF)DRIVE_INFO[i].color);
    }
    DrawWipeMask(dc, MakeRect(150, height - 107, width - 150, height - 23), TITLE_HINT_AT, 300, C_DIM);
}

// 기믹 상태줄은 두 줄까지 접힌다("발동: 강화 공격! (피해 N+로 예방했어야)"). 게이지는 체력 바 밑의
// 얇은 띠로 붙이고, 그 아래 250~384에 두 줄(40px)을 준다.
static int SoloProcessStage() {
    return gGame.enemyCount == 1 || (gGame.enemyCount == 2 && GimmickSummonPending(1));
}
RECT EnemyRect(int i) {
    // A lone process owns the stage; summons keep the three-target layout.
    if (SoloProcessStage() && i == 0) return MakeRect(28, 94, 916, 384);
    int left = 28 + i * 218; return MakeRect(left, 94, left + 198, 384);
}
static RECT PortraitRect(const RECT& panel) {
    if (panel.right - panel.left > 600)
        return MakeRect(panel.left + 20, panel.top + 14, panel.left + 292, panel.bottom - 14);
    return MakeRect(panel.left + 31, panel.top + 8, panel.left + 167, panel.top + 132);
}
static RECT EnemyInfoBody(RECT card) {
    if (card.right - card.left > 600) {
        card.left += 326; card.right -= 20; card.top -= 82; card.bottom = 324;
    }
    return card;
}
RECT SlotRect(int i) { int left = 28 + i * 172; return MakeRect(left, 408, left + 154, 532); }
RECT DieRect(int i) { int left = 48 + i * 220; return MakeRect(left, 574, left + 184, 708); }

// 여섯 면 상태 띠의 한 칸. 그리기와 격리 연출이 같은 사각형을 봐야 한다.
static RECT FaceStripCell(int die, int face) {
    RECT r = DieRect(die);
    int cell = (r.right - r.left - 24) / 6;
    int left = r.left + 12 + face * cell;
    return MakeRect(left, r.top + 100, left + cell - 3, r.top + 112);
}

// 슬롯 아래를 잇는 해결 순서 배선 (N:\ 계열 보스가 있을 때만).
static void DrawRoutingState(HDC dc);
// 조작 버튼 열. 전투판 오른쪽 끝(COMBAT_MAIN_RIGHT) 안에 두고, 그 너머는 사이드바가 쓴다.
RECT EndTurnRect() { return MakeRect(714, 616, 916, 679); }
RECT ReadButtonRect() { return MakeRect(714, 544, 916, 600); }
RECT KeybButtonRect() { return MakeRect(714, 685, 916, 732); }
int DieForSlotUI(int slot) { for (int d = 0; d < 3; ++d) if (DisplayDie(d)->assignedSlot == slot) return d; return -1; }

// 설치된 상주 프로그램은 비어 있는 적 슬롯에 세로로 나열한다.
// 현재 전투는 적이 하나라 슬롯 1이 항상 비지만, 다중 적에도 안전하게
// enemyCount 다음 슬롯을 쓴다. 적이 가득 차면 표시만 생략된다.
static void DrawTsrPanel(HDC dc) {
    int count = InstalledTsrCount(&gGame);
    if (count <= 0 || SoloProcessStage() || gGame.enemyCount >= 3) return;
    RECT r = EnemyRect(gGame.enemyCount);
    Panel(dc, r, RGB(12, 19, 26), C_LINE);
    Text(dc, r.left + 12, r.top + 10, L"상주 프로그램", C_GREEN, gFontSmall);
    wchar_t b[64]; wsprintfW(b, L"%dB", TsrBytes(&gGame));
    TextRect(dc, MakeRect(r.right - 70, r.top + 10, r.right - 12, r.top + 30), b, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    for (int i = 0; i < count && i < 3; ++i) {
        int tsr = InstalledTsrAt(&gGame, i);
        if (tsr < 0) break;
        int top = r.top + 40 + i * 68;
        Fill(dc, MakeRect(r.left + 12, top - 8, r.right - 12, top - 7), RGB(28, 40, 50));
        Text(dc, r.left + 12, top, TSR_INFO[tsr].name, (COLORREF)TSR_INFO[tsr].color, gFontSmall);
        wsprintfW(b, L"%dB", TSR_INFO[tsr].cost);
        TextRect(dc, MakeRect(r.right - 60, top, r.right - 12, top + 20), b, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        if (tsr == TSR_KEYB) {
            TextRect(dc, MakeRect(r.left + 12, top + 21, r.right - 12, top + 58),
                gGame.keybUsedThisTurn ? L"이번 턴 사용됨" : L"재굴림 대기 중",
                gGame.keybUsedThisTurn ? C_DIM : C_YELLOW, gFontSmall, DT_WORDBREAK);
        } else {
            TextRect(dc, MakeRect(r.left + 12, top + 21, r.right - 12, top + 58), TSR_INFO[tsr].description, C_DIM, gFontSmall, DT_WORDBREAK);
        }
    }
}

// 보스 기믹의 현재·다음 상태를 카드에 직접 표시할 한 줄을 만든다.
static void FormatGimmickStatus(wchar_t* out, int size) {
    const BossRuntime* boss = &gGame.boss;
    const BossGimmickInfo* gi = &BOSS_GIMMICK_INFO[boss->gimmick];
    out[0] = 0;
    if (boss->gimmick == GIMMICK_SIGNATURE) { wsprintfW(out, L"서명: %s 슬롯은 짝수 면만 통과", SLOT_NAMES[boss->signatureSlot]); return; }
    if (boss->gimmick == GIMMICK_SEVENTEENTH) { wsprintfW(out, L"복제 출력 +%d · 다음 공격에 합산", boss->copiedPower); return; }
    if (boss->gimmick == GIMMICK_LAST_WRITE) {
        if (boss->nextSealSlot < 0) lstrcpynW(out, L"봉인 완료 · 공격 슬롯 유지", size);
        else wsprintfW(out, L"%s 봉인까지 %d턴 · 피해 %d+로 지연", SLOT_NAMES[boss->nextSealSlot], boss->countdown, gi->p2);
        return;
    }
    switch (gi->family) {
    case FAM_LOCK: {
        int locked = -1, next = -1;
        for (int s = 0; s < SLOT_COUNT; ++s) {
            if (boss->lockedSlot[s] && locked < 0) locked = s;
            if (boss->nextLockedSlot[s] && next < 0) next = s;
        }
        if (boss->gimmick == GIMMICK_BLUE_SCREEN) {
            // 파쇄는 잠금과 달리 턴을 넘겨 유지되므로 남은 턴을 그대로 보여 준다.
            int gone = -1, left = 0;
            for (int s = 0; s < SLOT_COUNT; ++s) if (boss->shredLeft[s] > left) { gone = s; left = boss->shredLeft[s]; }
            if (gone >= 0) wsprintfW(out, L"%s 칸 파쇄됨 · %d턴 뒤 복구", SLOT_NAMES[gone], left);
            else if (boss->shredNext >= 0 && SlotShredPending(&gGame, boss->shredNext))
                wsprintfW(out, L"파쇄 임박: 실행하면 %s 칸이 부서집니다", SLOT_NAMES[boss->shredNext]);
            else if (boss->shredNext >= 0)
                wsprintfW(out, L"예고: 다음 턴 %s 칸 파쇄 · 피해 %d+로 빗나감", SLOT_NAMES[boss->shredNext], gi->p2);
            else lstrcpynW(out, L"다음 파쇄 대기 중", size);
            break;
        }
        if (locked >= 0) wsprintfW(out, L"발동: %s 슬롯 잠김", SLOT_NAMES[locked]);
        else if (next >= 0) wsprintfW(out, L"예고: 다음 턴 %s 잠금", SLOT_NAMES[next]);
        else if (boss->gimmick == GIMMICK_KERNEL_PANIC) lstrcpynW(out, L"이번 최고 출력 슬롯이 다음 턴 잠김", size);
        else lstrcpynW(out, L"다음 잠금 대기 중", size);
        break;
    }
    case FAM_RESTORE:
        if (boss->gimmick == GIMMICK_RESTORE_POINT) wsprintfW(out, L"창 피해 %d/%d · 복원 %d/2회", boss->windowDamage, gi->p2, boss->restoresUsed);
        else if (boss->gimmick == GIMMICK_TAPE_LOOP) wsprintfW(out, L"이번 턴 피해 %d/%d 미달 시 +%d", boss->damageThisTurn, gi->p2, gi->p3);
        else wsprintfW(out, boss->restoresUsed ? L"백업 소진됨" : L"체력 %d%% 미만 시 1회 복원", gi->p1);
        break;
    case FAM_OFFLINE:
        if (boss->offlineDie >= 0) wsprintfW(out, L"발동: 주사위 %d 오프라인", boss->offlineDie + 1);
        else if (boss->nextOfflineDie >= 0) wsprintfW(out, L"예고: 다음 턴 주사위 %d 오프라인", boss->nextOfflineDie + 1);
        else lstrcpynW(out, L"연결 안정 · 다음 발동 대기", size);
        break;
    case FAM_ROUTE:
        if (boss->gimmick == GIMMICK_TIMEOUT) {
            if (boss->reversed) lstrcpynW(out, L"타임아웃! 역전 · 보스 대기", size);
            else wsprintfW(out, L"카운트다운 %d · 피해 %d+로 지연", boss->countdown, gi->p2);
        } else if (boss->reversed) lstrcpynW(out, L"발동: 이번 턴 순서 역전", size);
        else if (boss->nextReversed) lstrcpynW(out, L"예고: 다음 턴 순서 역전", size);
        else lstrcpynW(out, L"라우팅 정상 · 역전 대기", size);
        break;
    case FAM_PRESSURE:
        if (boss->empowered) wsprintfW(out, L"발동: 강화 공격! (피해 %d+로 예방했어야)", gi->p2);
        else wsprintfW(out, L"압력 %d/%d · 피해 %d+ 시 감소", boss->gauge, boss->gaugeMax, gi->p2);
        break;
    case FAM_QUARANTINE:
        if (boss->nextTargetDie >= 0) wsprintfW(out, boss->nextTargetPermanent
            ? L"삭제 예고: 주사위 %d 면 %d" : L"격리 예고: 주사위 %d 면 %d",
            boss->nextTargetDie + 1, boss->nextTargetFace + 1);
        else if (boss->gimmick == GIMMICK_SANDBOX_BREACH) {
            // 예고는 저장된 상태가 아니라 턴 번호에서 나온다. 규칙과 같은 식이다.
            int loose = LivingMinionCount(&gGame), cap = gi->p2;
            if (loose > 0 && GimmickSummonPending(gGame.boss.fxA)) --loose;   // 아직 안 나온 탈주체는 세지 않는다
            if (loose >= cap) wsprintfW(out, L"탈주체 %d · 한계", loose);
            else if (gGame.turn % gi->p1 == 0) lstrcpynW(out, L"이번 턴 끝 탈주", size);
            else if ((gGame.turn + 1) % gi->p1 == 0) lstrcpynW(out, L"예고: 다음 턴 탈주", size);
            else wsprintfW(out, L"%d턴마다 탈주 %d/%d", gi->p1, loose, cap);
        }
        else wsprintfW(out, L"오염 %d/%d", boss->gauge, boss->gaugeMax > 0 ? boss->gaugeMax : gi->p1);
        break;
    default:
        lstrcpynW(out, L"", size);
        break;
    }
}

// ---------------------------------------------------------------------------
// 전투 시각 이벤트 그리기.
// game.cpp가 남긴 CombatFxEvent만 읽고, 모든 위치·강도는 CombatFxElapsed()의
// 순수 함수로 낸다. 프레임마다 쌓는 상태가 없으므로 마우스가 움직여 다시
// 그려져도 연출이 어긋나지 않고, 재생을 건너뛰면 그 자리에서 전부 사라진다.
// ---------------------------------------------------------------------------
#define CFX_AMP_EXPAND_MS  60     // 증폭 슬롯 테두리가 부풀어 오른다
#define CFX_AMP_TRAVEL_MS  150    // 그 보너스가 공격 슬롯으로 건너간다
#define CFX_AMP_LAND_MS    260    // 도착한 공격 슬롯이 초록 → 노랑 → 빨강으로 튄다
#define CFX_LAUNCH_MS      230    // 공격 신호 도착 = 다음 적중 줄
#define CFX_LAUNCH_HOLD_MS 340    // 경로 잔상이 걷히기까지
#define CFX_HIT_FLASH_MS   75
#define CFX_KNOCK_MS       280
#define CFX_DEBRIS_MS      420
#define CFX_GHOST_MS       540
#define CFX_NUMBER_MS      640
#define CFX_BIGHIT_MS      90     // 큰 피해·처치에만 붙는 초상화 밴드 분할
#define CFX_DEFEND_SCAN_MS 80
#define CFX_DEFEND_WAVE_MS 160
#define CFX_DEFEND_TAG_MS  300
#define CFX_KILL_FRAG_MS   260
#define CFX_KILL_CLEAR_MS  480

// 적 카드 아래와 슬롯 위 사이의 빈 통로. 공격·연쇄 신호는 여기서 가로로
// 건너간다. 카드 안으로 파고들면 코드·체력·의도 위를 굵은 선이 밟게 되므로
// 신호는 카드 아래 모서리까지만 가고, 충격은 그 안에서 따로 터진다.
#define CFX_ROUTE_Y 386
// 슬롯끼리 주고받는 신호는 슬롯 아래 빈 자리로 돌아간다 (슬롯 판을 가리지 않는다).
#define CFX_SLOT_ROUTE_Y 552

// 피해량을 그대로 파티클 수로 쓰지 않는다. 1~10으로 눌러 큰 피해가 화면을
// 뒤덮지 않게 하고, 작은 피해도 눈에 보이는 최소치를 갖게 한다.
static int CfxIntensity(int damage) {
    int intensity = damage / 3;
    if (intensity < 1) intensity = 1;
    if (intensity > 10) intensity = 10;
    return intensity;
}

static int CfxIsEnemyDamage(const CombatFxEvent* fx) {
    if (fx->type == CFX_ENEMY_HIT || fx->type == CFX_BURN) return 1;
    return fx->type == CFX_CHAIN && !(fx->flags & CFXF_DEFEND_CHAIN);
}

// 그 적을 때린 사건 중 window ms 안에서 재생 중인 마지막 것. 없으면 -1.
static int EnemyDamageFx(int enemy, int window, int* elapsedOut) {
    if (!CombatFxPlaying() || enemy < 0) return -1;
    int found = -1;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        if (fx->targetEnemy != enemy || !CfxIsEnemyDamage(fx)) continue;
        int t = CombatFxElapsed(i);
        if (t < 0 || t >= window) continue;
        found = i;
        if (elapsedOut) *elapsedOut = t;
    }
    return found;
}

// 처치 사건. 재생이 시작된 뒤로는 계속 유효해 붕괴 → 빈 껍데기로 이어진다.
static int EnemyKillFx(int enemy, int* elapsedOut) {
    if (!CombatFxPlaying() || enemy < 0) return -1;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        if (fx->targetEnemy != enemy || !(fx->flags & CFXF_KILL)) continue;
        if (fx->type == CFX_ENEMY_STRIKE) continue;
        int t = CombatFxElapsed(i);
        if (t < 0) continue;
        if (elapsedOut) *elapsedOut = t;
        return i;
    }
    return -1;
}

// 초기 40ms는 거의 흰색, 그 뒤 급격히 꺼진다.
static int EnemyFxFlash(int enemy) {
    int t = 0;
    if (EnemyDamageFx(enemy, CFX_HIT_FLASH_MS, &t) < 0) return 0;
    if (t < 40) return 1000;
    return 1000 - (t - 40) * 1000 / (CFX_HIT_FLASH_MS - 40);
}

// 위로 빠르게 밀렸다가 느리게 돌아온다 (음수 = 위쪽).
static int EnemyFxKnock(int enemy) {
    int t = 0;
    int index = EnemyDamageFx(enemy, CFX_KNOCK_MS, &t);
    if (index < 0) return 0;
    int peak = FxScale(4 + CfxIntensity(gGame.combatFx[index].value));
    int lunge = CFX_KNOCK_MS * 30 / 100;
    int advance = t < lunge ? t * 1000 / lunge
                            : 1000 - (t - lunge) * 1000 / (CFX_KNOCK_MS - lunge);
    return -(peak * advance / 1000);
}

// 잔상 체력. 피격 전 값에서 실제 값까지 따라 내려온다.
static int EnemyFxGhostHp(int enemy, int currentHp) {
    int t = 0;
    int index = EnemyDamageFx(enemy, CFX_GHOST_MS, &t);
    if (index < 0) return currentHp;
    const CombatFxEvent* fx = &gGame.combatFx[index];
    int before = fx->beforeValue, after = fx->afterValue;
    if (before <= after) return currentHp;
    int ghost = Lerp(before, after, EaseOutCubic(Track(t, 110, CFX_GHOST_MS)));
    return ghost > currentHp ? ghost : currentHp;
}

static POINT CfxPoint(int x, int y) { POINT p; p.x = x; p.y = y; return p; }

static POINT SlotTopAnchor(int slot) {
    RECT r = SlotRect(slot);
    return CfxPoint((r.left + r.right) / 2, r.top - 2);
}

static POINT SlotBottomAnchor(int slot) {
    RECT r = SlotRect(slot);
    return CfxPoint((r.left + r.right) / 2, r.bottom + 2);
}

static POINT EnemyHitAnchor(int enemy) {
    RECT card = EnemyRect(enemy);
    RECT portrait = PortraitRect(card);
    return CfxPoint((portrait.left + portrait.right) / 2, card.bottom + 2);
}

// 신호가 카드로 들어가는 순간 아래 모서리가 짧게 밝아진다. 경로가 카드 밖에서
// 끊기더라도 "이 카드로 들어갔다"가 남는다.
static void DrawCardEntry(HDC dc, int enemy, int t, int life, COLORREF color) {
    if (t < 0 || t >= life) return;
    RECT card = EnemyRect(enemy);
    if (card.right - card.left > 600) {
        RECT portrait = PortraitRect(card); card.left = portrait.left; card.right = portrait.right;
    }
    int fade = 1000 - t * 1000 / life;
    int half = (card.right - card.left) / 2 * (1000 - fade) / 1000 + 12;
    int cx = (card.left + card.right) / 2;
    Fill(dc, MakeRect(cx - half, card.bottom - 3, cx + half, card.bottom),
        MixColor(C_BG, color, 30 + fade * 70 / 1000));
}

// 재생 중에는 아직 닿지 않은 사건의 결과를 미리 보여 주지 않는다. 아직 오지
// 않은 첫 사건의 피격 전 값에서 멈춰 있다가, 계산이 그 줄에 닿는 순간
// 실제 값으로 내려온다. 이래야 증폭 → 공격 → 적중을 눈으로 따라갈 수 있다.
static int EnemyDisplayHp(int index) {
    if (!CombatFxPlaying() || index < 0 || index >= gGame.enemyCount) return gGame.enemies[index].hp;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        if (fx->targetEnemy != index || !CfxIsEnemyDamage(fx)) continue;
        if (CombatFxElapsed(i) >= 0) continue;
        return fx->beforeValue;
    }
    return gGame.enemies[index].hp;
}

int PlayerDisplayHp() {
    if (!CombatFxPlaying()) return gGame.playerHp;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        if (fx->type != CFX_ENEMY_STRIKE) continue;
        if (CombatFxElapsed(i) >= 0) continue;
        return fx->beforeValue;
    }
    return gGame.playerHp;
}

// ---- FX Back : 슬롯·주사위 아래를 지나가는 신호 ---------------------------
static void DrawCombatFxBack(HDC dc) {
    if (!CombatFxPlaying()) return;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        int t = fx->type == CFX_CHAIN ? CombatFxLeadElapsed(i, CFX_LAUNCH_MS) : CombatFxElapsed(i);
        if (t < 0) continue;
        switch (fx->type) {

        // 증폭: 슬롯이 부풀고, 그 보너스가 공격 슬롯으로 건너간다.
        case CFX_AMPLIFY: {
            if (fx->sourceSlot < 0 || fx->sourceSlot >= SLOT_COUNT) break;
            if (t < CFX_AMP_EXPAND_MS)
                DrawPulseFrame(dc, SlotRect(fx->sourceSlot), 2 + t * 4 / CFX_AMP_EXPAND_MS, 3, C_GREEN);
            if (t < CFX_AMP_EXPAND_MS || t >= CFX_AMP_LAND_MS) break;
            int span = CFX_AMP_TRAVEL_MS - CFX_AMP_EXPAND_MS;
            int p = (t - CFX_AMP_EXPAND_MS) * 1000 / span;
            if (p > 1000) p = 1000;
            // 역전으로 소실된 보너스는 공격 슬롯에 닿기 전에 끊긴다.
            if ((fx->flags & CFXF_WASTED) && p > 620) p = 620;
            DrawSignalPath(dc, SlotBottomAnchor(fx->sourceSlot), SlotBottomAnchor(SLOT_ATTACK),
                CFX_SLOT_ROUTE_Y, p, 2, (fx->flags & CFXF_WASTED) ? C_DIM : C_GREEN, 9, 0);
            break;
        }

        // 공격: 공격 슬롯 위에서 떠나 계산 줄을 통과해 대상 초상화로 올라간다.
        case CFX_ATTACK_LAUNCH: {
            if (fx->targetEnemy < 0 || fx->targetEnemy >= gGame.enemyCount) break;
            if (t >= CFX_LAUNCH_HOLD_MS) break;
            int p = t < CFX_LAUNCH_MS ? t * 1000 / CFX_LAUNCH_MS : 1000;
            DrawSignalPath(dc, SlotTopAnchor(SLOT_ATTACK), EnemyHitAnchor(fx->targetEnemy),
                CFX_ROUTE_Y, p, 3, C_RED, 11, 0);
            DrawCardEntry(dc, fx->targetEnemy, t - CFX_LAUNCH_MS, 140, C_RED);
            break;
        }

        // 연쇄: 공격과 다른 실루엣 — 두 갈래로 갈라졌다 대상 앞에서 다시 모인다.
        case CFX_CHAIN: {
            if (t >= CFX_LAUNCH_HOLD_MS) break;
            int p = t < CFX_LAUNCH_MS ? t * 1000 / CFX_LAUNCH_MS : 1000;
            int branch = 10 - 10 * p / 1000;   // 도착하면서 다시 하나로 합쳐진다
            if (fx->flags & CFXF_DEFEND_CHAIN) {
                DrawSignalPath(dc, SlotBottomAnchor(SLOT_CHAIN), SlotBottomAnchor(SLOT_DEFEND),
                    CFX_SLOT_ROUTE_Y, p, 2, C_BLUE, 11, branch);
            } else if (fx->targetEnemy >= 0 && fx->targetEnemy < gGame.enemyCount) {
                DrawSignalPath(dc, SlotTopAnchor(SLOT_CHAIN), EnemyHitAnchor(fx->targetEnemy),
                    CFX_ROUTE_Y, p, 2, C_YELLOW, 11, branch);
                DrawCardEntry(dc, fx->targetEnemy, t - CFX_LAUNCH_MS, 140, C_YELLOW);
            }
            break;
        }
        default: break;
        }
    }
}

// ---- FX Front : 이미 그려진 판 위에 얹는 결과 -----------------------------
static void DrawCombatFxFront(HDC dc) {
    if (!CombatFxPlaying()) return;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        if (fx->targetEnemy >= 0 && fx->targetEnemy < gGame.enemyCount) {
            if (fx->type == CFX_ATTACK_LAUNCH)
                DrawEnergyLance(dc, SlotTopAnchor(SLOT_ATTACK), EnemyHitAnchor(fx->targetEnemy),
                    CombatFxElapsed(i), CFX_LAUNCH_MS, C_RED, 0, i);
            if (fx->type == CFX_CHAIN && !(fx->flags & CFXF_DEFEND_CHAIN))
                DrawEnergyLance(dc, SlotTopAnchor(SLOT_CHAIN), EnemyHitAnchor(fx->targetEnemy),
                    CombatFxLeadElapsed(i, CFX_LAUNCH_MS), CFX_LAUNCH_MS, C_YELLOW, 1, i);
        }
        int t = CombatFxElapsed(i);
        if (t < 0) continue;

        if (fx->type == CFX_AMPLIFY && !(fx->flags & CFXF_WASTED) && t < 300 && FxDecorOn()) {
            RECT slot = SlotRect(SLOT_AMPLIFY);
            int cx = (slot.left + slot.right) / 2, cy = slot.top + 60;
            int radius = Lerp(52, 10, EaseInCubic(Track(t, 0, 160)));
            for (int k = 0; k < FxScale(8); ++k) {
                int angle = k * 450 + t * 7;
                int x = cx + CosMille(angle) * radius / 1000;
                int y = cy + SinMille(angle) * radius / 1600;
                Fill(dc, MakeRect(x - 2, y - 2, x + 3, y + 3), MixColor(C_GREEN, C_TEXT, k * 7));
            }
        }
        if ((fx->type == CFX_DEFEND || (fx->type == CFX_CHAIN && (fx->flags & CFXF_DEFEND_CHAIN))) && FxDecorOn()) {
            RECT shield = SlotRect(SLOT_DEFEND);
            DrawShieldMesh(dc, shield, t, 440, C_BLUE, FxScale(85));
        }
        if (fx->type == CFX_ENEMY_STRIKE && (fx->flags & CFXF_BLOCKED) && FxDecorOn()) {
            RECT shield = MakeRect(28, 414, 698, 538);
            DrawShieldMesh(dc, shield, t, 400, C_BLUE, FxScale(80));
            DrawPixelBurst(dc, 360, 420, t, 340, FxScale(18), i + 61, C_BLUE);
        }

        // 증폭이 도착한 공격 슬롯이 초록 → 노랑 → 빨강으로 짧게 넘어간다.
        if (fx->type == CFX_AMPLIFY && !(fx->flags & CFXF_WASTED)
            && t >= CFX_AMP_TRAVEL_MS && t < CFX_AMP_LAND_MS) {
            int land = (t - CFX_AMP_TRAVEL_MS) * 1000 / (CFX_AMP_LAND_MS - CFX_AMP_TRAVEL_MS);
            COLORREF tint = land < 500 ? MixColor(C_GREEN, C_YELLOW, land * 100 / 500)
                                       : MixColor(C_YELLOW, C_RED, (land - 500) * 100 / 500);
            Outline(dc, SlotRect(SLOT_ATTACK), tint, 2);
        }

        // 방어: 슬롯 안을 파란 스캔이 지나가고, 밖으로 사각 파동이 퍼진 뒤 BLOCK +N.
        if (fx->type == CFX_DEFEND && t < CFX_DEFEND_TAG_MS) {
            RECT slot = SlotRect(SLOT_DEFEND);
            if (t < CFX_DEFEND_SCAN_MS) {
                int y = slot.top + (slot.bottom - slot.top) * t / CFX_DEFEND_SCAN_MS;
                Fill(dc, MakeRect(slot.left + 2, y, slot.right - 2, y + 2), C_BLUE);
                Fill(dc, MakeRect(slot.left + 2, y + 2, slot.right - 2, y + 6), MixColor(C_BG, C_BLUE, 40));
            } else if (t < CFX_DEFEND_WAVE_MS) {
                int wave = (t - CFX_DEFEND_SCAN_MS) * 12 / (CFX_DEFEND_WAVE_MS - CFX_DEFEND_SCAN_MS);
                DrawPulseFrame(dc, slot, wave + 1, 3, C_BLUE);
            } else {
                wchar_t tag[24]; wsprintfW(tag, L"BLOCK +%d", fx->value);
                int rise = (t - CFX_DEFEND_WAVE_MS) * 14 / (CFX_DEFEND_TAG_MS - CFX_DEFEND_WAVE_MS);
                RECT box = MakeRect(slot.left, slot.top + 34 - rise, slot.right, slot.top + 58 - rise);
                Fill(dc, box, RGB(7, 12, 20));
                TextRect(dc, box, tag, C_BLUE, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        if (!CfxIsEnemyDamage(fx)) continue;
        int enemy = fx->targetEnemy;
        if (enemy < 0 || enemy >= gGame.enemyCount) continue;
        RECT card = EnemyRect(enemy);
        RECT portrait = PortraitRect(card);
        int cx = (portrait.left + portrait.right) / 2, cy = (portrait.top + portrait.bottom) / 2;
        if ((fx->flags & (CFXF_BIG_HIT | CFXF_KILL)) && !(fx->flags & CFXF_BLOCKED))
            DrawImpactCut(dc, portrait, t, fx->type == CFX_CHAIN ? C_YELLOW : C_RED, (fx->flags & CFXF_KILL) != 0);
        if ((fx->flags & (CFXF_BIG_HIT | CFXF_KILL)) && !(fx->flags & CFXF_BLOCKED))
            DrawFracture(dc, portrait, t, i + enemy * 31, (fx->flags & CFXF_KILL) ? C_YELLOW : C_RED,
                (fx->flags & CFXF_KILL) != 0);

        if (FxDecorOn()) {
            int saved = SaveDC(dc);
            IntersectClipRect(dc, portrait.left + 1, portrait.top + 1, portrait.right - 1, portrait.bottom - 1);
            COLORREF tone = (fx->flags & CFXF_BLOCKED) ? C_BLUE : fx->type == CFX_CHAIN ? C_YELLOW
                : fx->type == CFX_BURN ? RGB(255, 144, 48) : C_RED;
            int power = FxScale(2 + CfxIntensity(fx->value));
            if (fx->type == CFX_BURN) {
                for (int k = 0; k < FxScale(12); ++k) {
                    int age = t - k * 17;
                    if (age < 0 || age > 380) continue;
                    int x = cx - 48 + k * 9;
                    int y = portrait.bottom - 12 - EaseOutCubic(Track(age, 0, 380)) * 94 / 1000;
                    Fill(dc, MakeRect(x, y, x + 3, y + 12), MixColor(C_BG, tone, 90 * (380 - age) / 380));
                }
            } else DrawImpactBloom(dc, cx, cy + 8, t, power, i * 31 + enemy, tone);
            if ((fx->flags & CFXF_KILL) && t >= 80)
                DrawImpactBloom(dc, cx, cy + 8, t - 80, FxScale(12), i + 193, C_YELLOW);
            RestoreDC(dc, saved);
        }

        // 큰 타격만 초상화를 가로 띠로 쪼갠다. 작은 피해에는 붙지 않는다.
        if ((fx->flags & CFXF_BIG_HIT) && t < CFX_BIGHIT_MS && FxDecorOn())
            DrawBandGlitch(dc, portrait, t, FxScale(7 - 7 * t / CFX_BIGHIT_MS), i * 13 + enemy, 7);

        if (t < CFX_DEBRIS_MS && FxDecorOn()) {
            int debris = FxScale(4 + CfxIntensity(fx->value));
            COLORREF tone = fx->type == CFX_BURN ? C_YELLOW : (fx->flags & CFXF_BIG_HIT) ? C_RED : C_TEXT;
            DrawPixelBurst(dc, cx, cy + 12, t, CFX_DEBRIS_MS, debris, i * 31 + enemy * 7, tone);
        }

        // 실제 체력 피해량. 방어도가 전부 받아냈으면 그렇게 적는다.
        if (t < CFX_NUMBER_MS && EnemyDamageFx(enemy, CFX_NUMBER_MS, 0) == i) {
            wchar_t number[32];
            COLORREF tone;
            if (fx->flags & CFXF_BLOCKED) { lstrcpyW(number, L"방어도가 막음"); tone = C_BLUE; }
            else {
                const wchar_t* form = fx->type == CFX_BURN ? L"화상 -%d"
                                    : fx->type == CFX_CHAIN ? L"연쇄 -%d" : L"-%d";
                wsprintfW(number, form, fx->value);
                tone = (fx->flags & CFXF_BIG_HIT) ? C_RED : C_YELLOW;
            }
            int rise = EaseOutCubic(Track(t, 0, CFX_NUMBER_MS)) * 24 / 1000;
            int fade = 1000 - Track(t, 380, CFX_NUMBER_MS);
            RECT box = MakeRect(portrait.left - 8, portrait.top + 26 - rise, portrait.right + 8, portrait.top + 96 - rise);
            HFONT numberFont = (fx->flags & CFXF_BIG_HIT) ? gFontHuge : gFontLarge;
            if (fx->flags & CFXF_BLOCKED || fx->type == CFX_BURN || fx->type == CFX_CHAIN) numberFont = gFontMedium;
            RECT shadow = box; OffsetRect(&shadow, 2, 2);
            TextRect(dc, shadow, number, C_INK, numberFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            TextRect(dc, box, number, MixColor(C_BG, tone, 25 + fade * 75 / 1000), numberFont,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void DrawEnemy(HDC dc, int index) {
    const EnemyState* enemy = &gGame.enemies[index]; const EnemyState* action = DisplayEnemyAction(index); const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind); RECT r = EnemyRect(index);
    int selected = index == gGame.targetEnemy && (enemy->alive || EnemyDisplayHp(index) > 0);
    int isBoss = IsBossKind(enemy->kind);
    int hasGimmick = isBoss && gGame.boss.gimmick != GIMMICK_NONE;
    int drop = enemy->alive ? EnemyStrikeDrop(index) : 0, shift = enemy->alive ? EnemyStrikeShift(index) : 0;
    int knock = EnemyFxKnock(index);
    int killT = 0, killFx = EnemyKillFx(index, &killT);
    int collapsing = killFx >= 0 && killT < CFX_KILL_CLEAR_MS;
    // 처치 줄에 아직 닿지 않았으면 살아 있는 카드로 남는다.
    int shownAlive = enemy->alive || collapsing || EnemyDisplayHp(index) > 0;
    int hitT = 0, damageFx = EnemyDamageFx(index, CFX_KNOCK_MS, &hitT);
    int squash = damageFx >= 0 ? FxScale((1000 - EaseOutCubic(Track(hitT, 32, CFX_KNOCK_MS))) * 210 / 1000) : 0;
    RECT portrait = PortraitRect(r);
    int wide = r.right - r.left > 600;
    Panel(dc, r, shownAlive ? C_PANEL : RGB(18, 18, 20),
        drop > 0 || collapsing ? C_RED : selected ? C_YELLOW : C_LINE);
    // 붕괴 중에는 살아 있던 그림을 그대로 쪼갠다. 죽은 형태로 먼저 바뀌면
    // "부서지는 장면"이 아니라 "이미 끝난 장면"으로 읽힌다.
    if (wide) {
        Fill(dc, MakeRect(r.left + 1, r.top + 1, r.right - 1, r.top + 4),
            MixColor(C_PANEL, (COLORREF)info->color, 75));
        DrawProcessStage(dc, portrait, enemy->kind, shownAlive, EnemyFxFlash(index),
            (shownAlive ? EnemyBob(index) : 0) + drop + knock, shift, 1000 + squash, 1000 - squash);
        DrawLine(dc, r.left + 312, r.top + 22, r.left + 312, r.bottom - 22, C_LINE, 1);
        Text(dc, r.left + 338, r.top + 14, isBoss ? L"BOSS / ACCESS RESTRICTED" : L"HOSTILE PROCESS / CONNECTED",
            isBoss ? C_RED : C_DIM, gFontSmall);
    } else DrawPortrait(dc, portrait, enemy->kind, shownAlive, selected, EnemyFxFlash(index),
        (shownAlive ? EnemyBob(index) : 0) + drop + knock, shift, 1000 + squash, 1000 - squash);
    if (isBoss && shownAlive && !collapsing)
        DrawBossHalo(dc, portrait, (COLORREF)info->color, index,
            action->intent == INTENT_HEAVY || action->intent == INTENT_CORRUPT);
    DrawCardMotion(dc, r, isBoss ? (COLORREF)info->color : C_RED, index, 0);
    if (selected && shownAlive && FxDecorOn())
        DrawOrbitCorners(dc, portrait, (int)(GetTickCount() % 2400), C_YELLOW, FxScale(75));
    if (collapsing) {
        if (killT < CFX_KILL_FRAG_MS && FxDecorOn()) {
            DrawBandGlitch(dc, portrait, killT, FxScale(3 + 13 * killT / CFX_KILL_FRAG_MS), index * 7 + 3, 9);
        } else if (killT >= CFX_KILL_FRAG_MS) {
            // 조각이 위에서부터 지워지고 그 경계에서 픽셀이 떨어져 나간다.
            int span = CFX_KILL_CLEAR_MS - CFX_KILL_FRAG_MS;
            int wipe = (killT - CFX_KILL_FRAG_MS) * (portrait.bottom - portrait.top) / span;
            Fill(dc, MakeRect(portrait.left, portrait.top, portrait.right, portrait.top + wipe), RGB(10, 10, 12));
            if (FxDecorOn()) DrawPixelBurst(dc, (portrait.left + portrait.right) / 2, portrait.top + wipe,
                killT - CFX_KILL_FRAG_MS, span, FxScale(14), index * 19 + 5, C_RED);
        }
    }
    // 맞은 양은 때린 적 위로 떠오른다. 방어도가 다 받아냈으면 파랗게 튕겨낸 표시.
    int pop = enemy->alive ? EnemyStrikePop(index) : 0;
    if (pop > 0) {
        wchar_t hit[32];
        int taken = EnemyStrikeDamage(index);
        if (taken > 0) wsprintfW(hit, L"내 체력 -%d", taken); else lstrcpyW(hit, L"방어도가 막음");
        int rise = (1000 - pop) * 26 / 1000, center = (portrait.left + portrait.right) / 2;
        int tone = 30 + pop * 70 / 1000;
        RECT tag = MakeRect(center - 74, r.top + 108 - rise, center + 74, r.top + 132 - rise);
        COLORREF accent = MixColor(C_PANEL, taken > 0 ? C_RED : C_BLUE, tone);
        Fill(dc, tag, RGB(9, 7, 11));   // 도트 그림 위에서도 읽히도록 바탕을 깐다
        Outline(dc, tag, accent, 1);
        TextRect(dc, tag, hit, accent, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (wide) {
        r = EnemyInfoBody(r);
        Text(dc, r.left + 12, r.top + 120, info->code, shownAlive ? (COLORREF)info->color : C_DIM, gFontLarge);
    } else Text(dc, r.left + 12, r.top + 140, info->code, shownAlive ? (COLORREF)info->color : C_DIM, gFontMedium);
    wchar_t b[96];
    if (selected && !hasGimmick && !isBoss && enemy->trait != TRAIT_NONE)
        wsprintfW(b, L"▶ 대상 · %s", ENEMY_TRAIT_INFO[enemy->trait].badge);
    else if (selected && !hasGimmick) lstrcpyW(b, L"▶ 공격 대상");
    else if (hasGimmick) wsprintfW(b, selected ? L"▶ 보스 · %s" : L"보스 기믹: %s", BOSS_GIMMICK_INFO[gGame.boss.gimmick].name);
    else if (!isBoss && enemy->trait != TRAIT_NONE)
        wsprintfW(b, L"특성: %s", ENEMY_TRAIT_INFO[enemy->trait].badge);
    else lstrcpyW(b, isBoss ? L"보스 프로세스" : L"적 프로세스");
    Text(dc, r.left + 12, r.top + 165, b,
        selected ? C_YELLOW : hasGimmick ? (COLORREF)info->color
        : (!isBoss && enemy->trait != TRAIT_NONE) ? (COLORREF)info->color : C_DIM, gFontSmall);
    int shownHp = EnemyDisplayHp(index);
    if (enemy->block > 0 || enemy->burn > 0) wsprintfW(b, L"체력 %d/%d · 방%d 화%d", shownHp, enemy->maxHp, enemy->block, enemy->burn);
    else wsprintfW(b, L"체력 %d / %d", shownHp, enemy->maxHp);
    Text(dc, r.left + 12, r.top + 187, b, C_TEXT, gFontSmall);
    // 잔상 체력이 실제 체력까지 따라 내려온다. 숫자를 읽지 않아도 얼마나
    // 깎였는지가 남는다 (연출 강도와 무관하게 항상 보여 준다).
    DrawGhostBar(dc, MakeRect(r.left + 12, r.top + 208, r.right - 12, r.top + 220),
        shownHp, EnemyFxGhostHp(index, shownHp), enemy->maxHp,
        (COLORREF)info->color, MixColor(C_BG, C_RED, 62));
    if (shownAlive) {
        wsprintfW(b, L"의도: %s %d", INTENT_NAMES[action->intent], action->intentValue);
        Text(dc, r.left + 12, r.top + 231, b, action->intent == INTENT_HEAVY || action->intent == INTENT_CORRUPT ? C_RED : C_YELLOW, gFontSmall);
        if (hasGimmick) {
            wchar_t status[96]; FormatGimmickStatus(status, 96);
            int active = gGame.boss.empowered || gGame.boss.reversed || gGame.boss.offlineDie >= 0
                || gGame.boss.lockedSlot[0] || gGame.boss.lockedSlot[1] || gGame.boss.lockedSlot[2] || gGame.boss.lockedSlot[3];
            // 압력·오염은 글로 적힌 수치와 함께 칸 게이지로도 세운다. 몇 칸
            // 남았는지가 한눈에 잡혀야 피해로 눌러야 할 턴을 놓치지 않는다.
            int family = BOSS_GIMMICK_INFO[gGame.boss.gimmick].family;
            // 게이지는 체력 바 바로 밑의 얇은 띠다. 행을 따로 쓰지 않으므로 상태줄이 두 줄을 온전히 갖는다.
            if ((family == FAM_PRESSURE || family == FAM_QUARANTINE) && gGame.boss.gaugeMax > 0) {
                DrawPacketGrid(dc, MakeRect(r.left + 12, r.top + 222, r.right - 12, r.top + 229),
                    gGame.boss.gauge, gGame.boss.gaugeMax,
                    gGame.boss.empowered ? C_RED : (COLORREF)info->color, C_LINE);
            }
            TextRect(dc, MakeRect(r.left + 12, r.top + 250, r.right - 10, r.bottom), status, active ? C_RED : C_YELLOW, gFontSmall, DT_WORDBREAK);
        } else {
            // 몹 특성. 카운터 계열은 남은 숫자를 함께 보여 준다 - 위협이 숫자로 보여야
            // 플레이어가 자기 선택으로 그것을 관리할 수 있다.
            const EnemyTraitInfo* et = &ENEMY_TRAIT_INFO[enemy->trait];
            int line = r.top + 250;
            // 카운터는 의도 줄 오른쪽 끝에 붙인다. 줄을 따로 쓰면 아래 규칙문이
            // 카드 밑변에서 잘린다 (250~384 = 두 줄뿐이다).
            if (enemy->trait != TRAIT_NONE && et->usesCounter) {
                wsprintfW(b, L"%d", action->counter);
                TextRect(dc, MakeRect(r.left + 12, r.top + 231, r.right - 12, r.top + 249), b,
                    C_YELLOW, gFontSmall, DT_RIGHT | DT_SINGLELINE);
            }
            if (enemy->trait == TRAIT_TWOINTENT) {
                // 두 번째 의도까지 보여 준다. 둘 다 보이므로 예고가 지켜진다.
                uint8_t second = (uint8_t)((action->flags >> 4) & 7);
                wsprintfW(b, L"또는 %s %d (홀수 눈)", INTENT_NAMES[second], action->memo);
                Text(dc, r.left + 12, line, b, C_RED, gFontSmall);
                line += 20;
            }
            if ((enemy->block > 0 || enemy->burn > 0) && enemy->trait == TRAIT_NONE) {
                // 체력 줄이 이미 "방N 화N"을 보여 준다. 특성이 있으면 이 줄을 규칙문에 내준다.
                wsprintfW(b, L"방어도 %d   화상 %d", enemy->block, enemy->burn);
                Text(dc, r.left + 12, line, b, C_DIM, gFontSmall);
                line += 20;
            }
            if (enemy->trait != TRAIT_NONE && enemy->trait != TRAIT_TWOINTENT && line < r.bottom - 18)
                TextRect(dc, MakeRect(r.left + 12, line, r.right - 10, r.bottom), et->rule, C_DIM, gFontSmall, DT_WORDBREAK);
        }
    } else {
        Text(dc, r.left + 12, r.top + 231, L"[ 삭제됨 ]", C_DIM, gFontSmall);
        // 붕괴가 끝나면 빈 껍데기에 종료 도장만 남는다.
        if (!collapsing) {
            RECT stamp = MakeRect(portrait.left + 4, portrait.top + 54, portrait.right - 4, portrait.top + 88);
            Fill(dc, stamp, RGB(10, 10, 12));
            Outline(dc, stamp, RGB(72, 32, 34), 1);
            TextRect(dc, stamp, L"PROCESS TERMINATED", MixColor(C_BG, C_RED, 72),
                gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// 실행 전 미리보기. DrawCombat이 프레임마다 한 번 계산하고 아래에서 읽기만 한다.
static TurnPreview gPreview;

static COLORREF SlotAccent(int slot) {
    return slot == SLOT_ATTACK ? C_RED : slot == SLOT_DEFEND ? C_BLUE : slot == SLOT_CHAIN ? C_YELLOW : C_GREEN;
}

// 주사위 번호 배지. 세 주사위가 같은 값을 내면 값만으로는 구분할 수 없으므로,
// 슬롯과 주사위 카드 양쪽에 같은 기호를 찍어 짝을 드러낸다.
static const wchar_t* const DIE_BADGE[3] = {L"①", L"②", L"③"};

// 빈 구멍 안에서 깜빡이는 오류 기호. 파쇄가 남긴 자리와 복구를 기다리는 자리가
// 같은 잡음을 쓴다.
static void DrawShredNoise(HDC dc, const RECT& r, int slot, int step, COLORREF tone) {
    if (!FxDecorOn()) return;
    static const wchar_t ERRG[] = L"#%&?@$*+=<>0123456789ABCDEF";
    for (int i = 0; i < 10; ++i) {
        uint32_t h = Hash3(slot + 40, i, step);
        if (h % 3u) continue;
        wchar_t g[2] = {ERRG[(h >> 9) % (uint32_t)(sizeof(ERRG) / sizeof(ERRG[0]) - 1)], 0};
        Text(dc, r.left + 8 + (int)((h >> 3) % (uint32_t)(r.right - r.left - 18)),
                 r.top + 6 + (int)((h >> 17) % (uint32_t)(r.bottom - r.top - 22)), g, tone, gFontSmall);
    }
}

// 부서진 칸은 빈 구멍으로 남는다. 남은 칸을 당겨 채우면 클릭 자리가 움직여
// 잘못 누르게 되므로 자리는 그대로 두고, 가장자리만 불씨처럼 타다 식는다.
static void DrawShredHole(HDC dc, const RECT& r, int slot, int turnsLeft) {
    Fill(dc, r, RGB(5, 7, 10));
    int step = (int)(GetTickCount() / 110);
    for (int i = 0; i < 24; ++i) {
        uint32_t h = Hash3(slot, i, step / 3);
        int along = (int)(h % 1000u), side = i & 3, x, y;
        if (side == 0)      { x = r.left + (r.right - r.left) * along / 1000; y = r.top; }
        else if (side == 1) { x = r.right - 2; y = r.top + (r.bottom - r.top) * along / 1000; }
        else if (side == 2) { x = r.left + (r.right - r.left) * along / 1000; y = r.bottom - 2; }
        else                { x = r.left; y = r.top + (r.bottom - r.top) * along / 1000; }
        int lit = (int)((h >> 11) % 100u);
        Fill(dc, MakeRect(x, y, x + 2, y + 2),
            MixColor(RGB(5, 7, 10), lit > 55 ? RGB(255, 120, 60) : C_RED, 45 + lit / 2));
    }
    DrawShredNoise(dc, r, slot, step, RGB(90, 42, 36));   // 안쪽에서 오류 기호가 깜빡인다
    Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], MixColor(C_BG, C_RED, 60), gFontMedium);
    wchar_t left[8]; wsprintfW(left, L"%d", turnsLeft);
    TextRect(dc, MakeRect(r.left, r.top + 34, r.right, r.top + 84), left, C_RED, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(r.left, r.top + 86, r.right, r.top + 106), L"턴 뒤 복구", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6), L"SLOT SHREDDED",
        MixColor(C_BG, C_RED, 75), gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 실행을 누르면 부서질 칸. 아직 배치는 받으므로 잠그지 않고 조준선만 붙인다.
static void DrawShredAim(HDC dc, const RECT& r) {
    int phase = (int)(GetTickCount() % 1200u);
    COLORREF tone = MixColor(C_BG, C_RED, 45 + 45 * (phase < 600 ? phase : 1200 - phase) / 600);
    for (int i = 0; i < 4; ++i) {
        int x = (i & 1) ? r.right + 3 : r.left - 4, dx = (i & 1) ? -14 : 14;
        int y = (i & 2) ? r.bottom + 3 : r.top - 4, dy = (i & 2) ? -10 : 10;
        DrawLine(dc, x, y, x + dx, y, tone, 2);
        DrawLine(dc, x, y, x, y + dy, tone, 2);
    }
    RECT tag = MakeRect(r.left + 3, r.bottom - 29, r.right - 3, r.bottom - 5);
    Fill(dc, tag, RGB(38, 16, 18));
    TextRect(dc, tag, L"실행 시 파쇄", C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawSlot(HDC dc, int slot) {
    RECT r = SlotRect(slot); int die = DieForSlotUI(slot); int hover = Inside(r, gMouse.x, gMouse.y);
    int locked = SlotLockedThisTurn(&gGame, slot) && !GimmickLockPending(slot);
    int lockedNext = SlotLockedNextTurn(&gGame, slot);
    if (locked && SlotShredTurnsLeft(&gGame, slot) > 0) { DrawShredHole(dc, r, slot, SlotShredTurnsLeft(&gGame, slot)); return; }
    if (locked) {
        // 잠긴 슬롯: 배치를 받지 않으며 어둡고 붉게 오버레이한다.
        Panel(dc, r, RGB(38, 16, 18), C_RED);
        Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], C_RED, gFontMedium);
        TextRect(dc, MakeRect(r.left + 5, r.top + 42, r.right - 5, r.top + 83), L"잠김", C_RED, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6), L"ACCESS DENIED", C_RED, gFontSmall, DT_CENTER | DT_SINGLELINE);
        return;
    }
    Panel(dc, r, hover ? RGB(23, 39, 48) : C_PANEL, hover ? C_GREEN : lockedNext ? C_YELLOW : C_LINE);
    Fill(dc, MakeRect(r.left + 1, r.top + 1, r.right - 1, r.top + 4), MixColor(C_PANEL, SlotAccent(slot), die >= 0 ? 70 : 24));
    DrawLine(dc, r.left + 5, r.bottom - 6, r.right - 5, r.bottom - 6, C_INK, 2);
    DrawCardMotion(dc, r, SlotAccent(slot), slot, hover && gGame.selectedDie >= 0);
    if (gGame.boss.gimmick == GIMMICK_SIGNATURE && gGame.boss.signatureSlot == slot) {
        Outline(dc, r, C_YELLOW, 2);
        TextRect(dc, MakeRect(r.left + 4, r.top + 88, r.right - 4, r.top + 108), L"짝수 서명 필요", C_YELLOW, gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
    if (die >= 0 || (hover && gGame.selectedDie >= 0)) {
        COLORREF accent = SlotAccent(slot);
        Fill(dc, MakeRect(r.left + 1, r.bottom - 4, r.right - 1, r.bottom - 1), MixColor(C_PANEL, accent, 65));
        if (FxDecorOn()) DrawOrbitCorners(dc, r, (int)(GetTickCount() % 2400) + slot * 180, accent, FxScale(70));
    }
    Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], SlotAccent(slot), gFontMedium);
    // 예상 산출량. 0이면 이 슬롯이 이번 턴 아무 일도 하지 않는다는 뜻이라 흐리게 둔다.
    // 읽기 오류로 다시 굴러갈 주사위가 놓인 슬롯은 숫자를 만들어 보이지 않고 ? 로 남긴다.
    if (gPreview.valid && die >= 0) {
        int unknown = gPreview.slotUnknown[slot];
        wchar_t out[24];
        if (unknown) lstrcpyW(out, L"→ ?");
        else wsprintfW(out, L"→ %d", gPreview.slotOutput[slot]);
        COLORREF tint = unknown ? C_YELLOW : gPreview.slotOutput[slot] > 0 ? SlotAccent(slot) : C_DIM;
        TextRect(dc, MakeRect(r.left + 58, r.top + 10, r.right - 8, r.top + 32), out, tint, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    if (die >= 0) {
        const DieState* shownDie = DisplayDie(die);
        const Face* face = &shownDie->faces[shownDie->rolledFace]; wchar_t value[24]; FormatFace(face, value);
        int offline = shownDie->offline;
        if (offline) TextRect(dc, MakeRect(r.left + 5, r.top + 42, r.right - 5, r.top + 83), L"오프라인", C_RED, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        else TextRect(dc, MakeRect(r.left + 5, r.top + 42, r.right - 5, r.top + 83), value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        wchar_t b[48]; wsprintfW(b, L"%s 주사위 %d · %dB", DIE_BADGE[die], die + 1, FaceCost(face));
        TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6), b,
            die == gGame.selectedDie ? C_YELLOW : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    } else if (gGame.selectedDie >= 0) {
        // 고른 주사위가 있으면 빈 슬롯이 클릭 결과를 미리 말해 준다.
        wchar_t hint[32]; wsprintfW(hint, L"주사위 %d 배치", gGame.selectedDie + 1);
        TextRect(dc, MakeRect(r.left + 5, r.top + 48, r.right - 5, r.top + 89), hint, C_YELLOW, gFontSmall, DT_CENTER | DT_SINGLELINE);
    } else TextRect(dc, MakeRect(r.left + 5, r.top + 48, r.right - 5, r.top + 89), L"비어 있음", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
    if (lockedNext) TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6),
        die >= 0 ? L"" : L"다음 턴 잠김", C_YELLOW, gFontSmall, DT_CENTER | DT_SINGLELINE);
    if (lockedNext && die >= 0) TextRect(dc, MakeRect(r.left + 4, r.top + 88, r.right - 4, r.top + 108), L"다음 턴 잠김", C_YELLOW, gFontSmall, DT_CENTER | DT_SINGLELINE);
    if (SlotShredPending(&gGame, slot)) DrawShredAim(dc, r);
}

// 여섯 면의 상태 띠. 큰 굴림값과 별개로 어느 면이 격리·삭제 예고 대상인지,
// 어느 면이 이미 잠겼는지를 정지 화면만 보고도 확인할 수 있다.
// 미판독 상태에서는 이번 굴림이 어디인지 표시하지 않는다 (정보 누출 금지).
static void DrawFaceStrip(HDC dc, int index) {
    RECT r = DieRect(index);
    const DieState* die = DisplayDie(index);
    const BossRuntime* boss = &gGame.boss;
    for (int f = 0; f < 6; ++f) {
        RECT box = FaceStripCell(index, f);
        const Face* face = &die->faces[f];
        int warned = boss->nextTargetDie == index && boss->nextTargetFace == f;
        if (face->kind == FACE_EMPTY) {                     // 영구 삭제: 빈 칸
            Outline(dc, box, RGB(58, 24, 26), 1);
        } else if (face->quarantined != QUAR_NONE) {        // 격리: 붉은 빗금
            Outline(dc, box, C_RED, 1);
            for (int x = box.left + 2; x < box.right - 1; x += 3)
                Fill(dc, MakeRect(x, box.top + 1, x + 1, box.bottom - 1), MixColor(C_BG, C_RED, 62));
        } else if (warned) {                                // 예고: 노랑
            Fill(dc, box, MixColor(C_BG, C_YELLOW, boss->nextTargetPermanent ? 72 : 46));
            Outline(dc, box, C_YELLOW, 1);
        } else {
            Fill(dc, box, MixColor(C_BG, FaceColor(face), face->damaged ? 20 : 32));
        }
        if (f == die->rolledFace && (gRolled || gReadActive || gTurnTraceActive) && DieSettled(index))
            Fill(dc, MakeRect(box.left, box.bottom + 2, box.right, box.bottom + 4), C_TEXT);
    }
}

static void DrawDie(HDC dc, int index) {
    RECT r = DieRect(index); const DieState* die = DisplayDie(index);
    const Face* face = &die->faces[die->rolledFace];
    int selected = gGame.selectedDie == index, hover = Inside(r, gMouse.x, gMouse.y);
    int noise = DieNoise(index), flash = DieSettleFlash(index), step = NoiseStep(index);
    RECT cell = MakeRect(r.left + 6, r.top + 25, r.right - 6, r.top + 72);
    RECT statusRect = MakeRect(r.left + 7, r.top + 74, r.right - 7, r.top + 96);

    COLORREF border = selected ? C_YELLOW : hover ? C_BLUE : C_LINE;
    if (noise > 0) border = (step & 1) ? C_RED : RGB(96, 58, 58);
    else if (flash > 0) border = C_GREEN;
    Panel(dc, r, selected ? RGB(42, 36, 18) : C_PANEL, border);
    Fill(dc, MakeRect(r.left + 5, r.top + 29, r.right - 5, r.top + 71), selected ? RGB(28, 26, 17) : RGB(9, 16, 24));
    DrawLine(dc, r.left + 6, r.top + 72, r.right - 6, r.top + 72, MixColor(C_PANEL, border, 45), 1);
    DrawCardMotion(dc, r, selected ? C_YELLOW : C_BLUE, index, selected || hover);
    // 판독 연출의 붉은·초록 테두리가 선택 표시를 덮어 버리므로, 선택은 그 위에
    // 두께 2로 덧그려 어느 상태에서도 사라지지 않게 한다.
    if (selected) Outline(dc, r, C_YELLOW, 2);
    if (flash > 0 && FxDecorOn()) {
        int t = (1000 - flash) * 260 / 1000;
        DrawPulseFrame(dc, r, 1 + EaseOutCubic(Track(t, 0, 260)) * 5 / 1000, 2,
            MixColor(C_BG, FaceColor(face), flash / 10));
        DrawPixelBurst(dc, (r.left + r.right) / 2, r.top + 44, t, 260, FxScale(10), index + 91, FaceColor(face));
    }
    wchar_t b[64]; wsprintfW(b, L"%s 주사위 %d", DIE_BADGE[index], index + 1);
    Text(dc, r.left + 10, r.top + 8, b, selected ? C_YELLOW : C_TEXT, gFontSmall);
    // 배치돼 있으면 어느 슬롯인지를 그 슬롯 색으로 적는다. 선택 표시는 테두리가
    // 이미 하고 있으므로, 자리를 두고 다투게 두지 않는다.
    int placedIn = die->assignedSlot;
    if (placedIn >= 0 && placedIn < SLOT_COUNT) {
        wchar_t where[32]; wsprintfW(where, L"→ %s", SLOT_SHORT_NAMES[placedIn]);
        TextRect(dc, MakeRect(r.left + 80, r.top + 6, r.right - 10, r.top + 26), where,
            SlotAccent(placedIn), gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else if (selected) TextRect(dc, MakeRect(r.left + 80, r.top + 6, r.right - 10, r.top + 26),
        L"▶ 선택", C_YELLOW, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    if (!gRolled && !gReadActive && !gTurnTraceActive) {   // sector never read this turn: faint drift
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
    RECT valueCell = cell;
    if (face->kind == FACE_NUMBER && !face->damaged && face->quarantined == QUAR_NONE
        && face->value >= 1 && face->value <= 6) {
        valueCell.right -= 34;
        DrawDiePips(dc, r.right - 48, r.top + 37, face->value, MixColor(C_PANEL, FaceColor(face), 80));
    }
    if (flash > 0) OffsetRect(&valueCell, 0, -FxScale(flash * 6 / 1000));
    TextRect(dc, valueCell, value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (flash > 0) DrawScanlines(dc, cell);
    wchar_t statuses[64] = L""; int statusCount = 0;
    if (face && face->damaged) { AppendStatus(statuses, L"손상"); ++statusCount; }
    if (face && face->quarantined != QUAR_NONE) { AppendStatus(statuses, L"격리"); ++statusCount; }
    if (die->unstable && gGame.boss.nextOfflineDie == index) { AppendStatus(statuses, L"읽기 오류 → 다음 오프라인"); ++statusCount; }
    else if (die->unstable) { AppendStatus(statuses, L"읽기 오류"); ++statusCount; }
    if (die->disabled) { AppendStatus(statuses, L"조각화"); ++statusCount; }
    if (die->offline) { AppendStatus(statuses, L"오프라인"); ++statusCount; }
    if (!die->unstable && gGame.boss.nextOfflineDie == index) { AppendStatus(statuses, L"다음 턴 오프라인"); ++statusCount; }
    if (statusCount == 1) TextRect(dc, statusRect, statuses, C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    else if (statusCount > 1) TextRect(dc, statusRect, statuses, C_RED, gFontSmall, DT_CENTER | DT_WORDBREAK);
    else {
        wsprintfW(b, L"%s · %dB", FACE_INFO[face->kind].name, FaceCost(face));
        TextRect(dc, statusRect, b, C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}
// 지금 판에 걸린 디스크 손상. 마운트 전(-1)처럼 손상이 없는 판도 그려질 수 있으므로
// 표 밖을 읽지 않고 null을 돌려준다 (그 칸은 비워 둔다).
static const ModifierInfo* ActiveModifierInfo(int modifier) {
    return modifier >= 0 && modifier < MODIFIER_COUNT ? &MODIFIER_INFO[modifier] : 0;
}

// 카드에는 사이드바용 긴 설명 대신 한 줄 요약을 쓴다 (카드 폭 제약).
// 전투 사이드바의 디스크 손상 줄도 같은 요약을 쓴다.
static const wchar_t* const MODIFIER_BRIEF[MODIFIER_COUNT] = {
    L"층 하강 시 무작위 면 1개 영구 손상",
    L"경고된 주사위가 실행 순간 재굴림",
    L"중복 굴림 결과는 뒤쪽이 비활성화",
    L"용량 +60B · 적 체력 +30%",
    L"굴림 합이 짝수면 공격 +2"
};

// ---------------------------------------------------------------------------
// 전투 정보 사이드바
//
// 넓어진 오른쪽 폭에는 이번 턴 판단에 필요한 요약만 모은다. 긴 설명은 적 카드와
// 가이드에 두고, 여기서는 "무엇을 하면 어떻게 되는가"를 화면 왕복 없이 읽게 한다.
//   TARGET   지금 대상의 체력·의도·특성 또는 기믹 예고
//   FORECAST 실행 전 미리보기. PreviewTurn이 이미 낸 값을 나눠 크게 적을 뿐이다
//   SYSTEM   해결 순서·잠금·오프라인·격리·게이지와 볼륨 법칙·디스크 손상·상주
//   HISTORY  직전 실행 요약과 시스템 기록
// 계산 재생 중에는 FORECAST~HISTORY 자리를 계산 패널이 쓴다 (DrawTurnCalculation).
// 장식이 없으므로 연출 강도(FULL/REDUCED/OFF)와 무관하게 늘 같은 내용이 보인다.
// 칸 제목과 줄 태그는 ASCII로 두어 번역 없이 두 언어에서 같은 폭을 쓴다.
// ---------------------------------------------------------------------------
RECT CombatSidebarRect() { return MakeRect(SIDEBAR_LEFT, SIDEBAR_TOP, SIDEBAR_RIGHT, SIDEBAR_BOTTOM); }
// SYSTEM은 예전 사이드바의 디스크 손상 설명·볼륨 법칙·난이도를 전부 담아야 해서
// 가장 크게 잡는다. HISTORY는 예전과 같이 시스템 기록 세 줄이다.
RECT TargetInfoRect()    { return MakeRect(SIDEBAR_LEFT, SIDEBAR_TOP, SIDEBAR_RIGHT, 246); }
RECT ForecastRect()      { return MakeRect(SIDEBAR_LEFT, 256, SIDEBAR_RIGHT, 380); }
RECT SystemInfoRect()    { return MakeRect(SIDEBAR_LEFT, 390, SIDEBAR_RIGHT, 632); }
RECT CombatHistoryRect() { return MakeRect(SIDEBAR_LEFT, 642, SIDEBAR_RIGHT, SIDEBAR_BOTTOM); }

#define SIDEBAR_ROW_H 21   // SYSTEM·HISTORY 한 줄 높이
#define SIDEBAR_TAG_W 84   // 줄 앞 ASCII 태그 칸 ("INFECTION"까지 들어간다)
#define SIDEBAR_LINE_H 16  // gFontSmall 한 줄. 여러 줄 설명은 이 배수로 잘라 반쯤 잘린 줄을 남기지 않는다

static void DrawSidebarFrame(HDC dc, const RECT& r, const wchar_t* title, COLORREF accent) {
    Panel(dc, r, C_PANEL, C_LINE);
    Text(dc, r.left + 12, r.top + 7, title, accent, gFontSmall);
    Fill(dc, MakeRect(r.left + 10, r.top + 28, r.right - 10, r.top + 29), RGB(28, 40, 50));
}

static void DrawSidebarWrapped(HDC dc, int left, int top, int right, int bottom, const wchar_t* text, COLORREF color) {
    int lines = (bottom - top) / SIDEBAR_LINE_H;
    if (lines <= 0) return;
    TextRect(dc, MakeRect(left, top, right, top + lines * SIDEBAR_LINE_H), text, color, gFontSmall, DT_WORDBREAK);
}

static void AppendBounded(wchar_t* out, int cap, const wchar_t* part) {
    int used = lstrlenW(out);
    if (used < cap - 1) lstrcpynW(out + used, part, cap - used);
}

// 이번 턴 판단에 쓰는 적. 고른 대상이 쓰러졌거나 아직 격리막 안이면 판에 남은 첫 적을 본다.
static int SidebarTargetIndex() {
    int t = gGame.targetEnemy;
    int valid = t >= 0 && t < gGame.enemyCount && !GimmickSummonPending(t);
    if (valid && (gGame.enemies[t].alive || EnemyDisplayHp(t) > 0)) return t;
    for (int i = 0; i < gGame.enemyCount; ++i)
        if (!GimmickSummonPending(i) && (gGame.enemies[i].alive || EnemyDisplayHp(i) > 0)) return i;
    return valid ? t : -1;
}

// 적 카드의 전체 내용을 옮기지 않는다. 대상 → 체력 → 다음 행동 → 판단에 걸리는
// 특성 → 기믹 예고 순으로 요약만 둔다. 재생 중에도 남아 체력이 계산 줄에 맞춰
// 내려가는 것을 보여 준다 (아직 닿지 않은 피해는 미리 빼지 않는다).
static void DrawTargetPanel(HDC dc) {
    RECT r = TargetInfoRect();
    DrawSidebarFrame(dc, r, L"TARGET / INTENT", C_YELLOW);
    int index = SidebarTargetIndex();
    if (index < 0) {
        TextRect(dc, MakeRect(r.left + 12, r.top + 64, r.right - 12, r.top + 88), L"대상 없음", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
        return;
    }
    const EnemyState* enemy = &gGame.enemies[index]; const EnemyState* action = DisplayEnemyAction(index);
    const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
    int left = r.left + 12, right = r.right - 12;
    int shownHp = EnemyDisplayHp(index);
    int shownAlive = enemy->alive || shownHp > 0;
    int isBoss = IsBossKind(enemy->kind);
    int hasGimmick = isBoss && gGame.boss.gimmick != GIMMICK_NONE;
    wchar_t b[128];
    // 적이 여럿이면 몇 번째를 보고 있는지 적는다. 대상은 적 카드를 눌러 바꾼다.
    if (gGame.enemyCount > 1) {
        wsprintfW(b, L"#%d / %d", index + 1, gGame.enemyCount);
        TextRect(dc, MakeRect(r.right - 90, r.top + 7, right, r.top + 25), b, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(left, r.top + 34, left + 180, r.top + 58), info->code,
        shownAlive ? (COLORREF)info->color : C_DIM, gFontMedium, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    wsprintfW(b, L"체력 %d / %d", shownHp, enemy->maxHp);
    TextRect(dc, MakeRect(left + 180, r.top + 34, right, r.top + 58), b, shownAlive ? C_TEXT : C_DIM,
        gFontMedium, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    DrawGhostBar(dc, MakeRect(left, r.top + 62, right, r.top + 68), shownHp, EnemyFxGhostHp(index, shownHp),
        enemy->maxHp, (COLORREF)info->color, MixColor(C_BG, C_RED, 62));
    if (!shownAlive) {
        Text(dc, left, r.top + 74, L"[ 삭제됨 ]", C_DIM, gFontSmall);
        return;
    }
    int y = r.top + 74;
    wsprintfW(b, L"의도: %s %d", INTENT_NAMES[action->intent], action->intentValue);
    TextRect(dc, MakeRect(left, y, right, y + 20), b,
        action->intent == INTENT_HEAVY || action->intent == INTENT_CORRUPT ? C_RED : C_YELLOW,
        gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 20;
    if (enemy->block > 0 || enemy->burn > 0) {
        wsprintfW(b, L"방어도 %d   화상 %d", enemy->block, enemy->burn);
        Text(dc, left, y, b, C_DIM, gFontSmall);
        y += 20;
    }
    if (hasGimmick) {
        wsprintfW(b, L"보스 기믹: %s", BOSS_GIMMICK_INFO[gGame.boss.gimmick].name);
        TextRect(dc, MakeRect(left, y, right, y + 20), b, (COLORREF)info->color, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
        y += 20;
        wchar_t status[96]; FormatGimmickStatus(status, 96);
        int active = gGame.boss.empowered || gGame.boss.reversed || gGame.boss.offlineDie >= 0
            || gGame.boss.lockedSlot[0] || gGame.boss.lockedSlot[1] || gGame.boss.lockedSlot[2] || gGame.boss.lockedSlot[3];
        DrawSidebarWrapped(dc, left, y, right, r.bottom - 4, status, active ? C_RED : C_YELLOW);
    } else if (!isBoss && enemy->trait != TRAIT_NONE) {
        // 카운터 계열은 남은 숫자를 오른쪽 끝에 붙인다 (적 카드와 같은 약속).
        const EnemyTraitInfo* et = &ENEMY_TRAIT_INFO[enemy->trait];
        wsprintfW(b, L"특성: %s", et->badge);
        TextRect(dc, MakeRect(left, y, right - 40, y + 20), b, (COLORREF)info->color, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
        if (et->usesCounter) {
            wsprintfW(b, L"%d", action->counter);
            TextRect(dc, MakeRect(right - 40, y, right, y + 20), b, C_YELLOW, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        }
        y += 20;
        if (enemy->trait == TRAIT_TWOINTENT) {
            uint8_t second = (uint8_t)((action->flags >> 4) & 7);
            wsprintfW(b, L"또는 %s %d (홀수 눈)", INTENT_NAMES[second], action->memo);
            Text(dc, left, y, b, C_RED, gFontSmall);
        } else DrawSidebarWrapped(dc, left, y, right, r.bottom - 4, et->rule, C_DIM);
    } else Text(dc, left, y, isBoss ? L"보스 프로세스" : L"적 프로세스", C_DIM, gFontSmall);
}

// 실행 전 미리보기. 숫자는 PreviewTurn이 이미 계산해 둔 값을 칸별로 나눠 적을 뿐이다.
// 읽기 오류로 확정할 수 없으면 기존 규칙대로 영향을 받는 값을 전부 ? 로 가리고,
// 가려진 결과를 따로 계산해 보여 주지 않는다.
static void DrawForecastPanel(HDC dc) {
    RECT r = ForecastRect();
    DrawSidebarFrame(dc, r, L"FORECAST", C_GREEN);
    static const wchar_t* const LABELS[3] = {L"적 체력", L"내 체력", L"방어도"};
    int valid = gPreview.valid, unknown = valid && gPreview.uncertain;
    int colW = (r.right - r.left - 20) / 3;
    for (int i = 0; i < 3; ++i) {
        int cl = r.left + 10 + i * colW, cr = cl + colW;
        if (i) Fill(dc, MakeRect(cl, r.top + 36, cl + 1, r.top + 84), RGB(28, 40, 50));
        TextRect(dc, MakeRect(cl, r.top + 32, cr, r.top + 50), LABELS[i], C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        wchar_t value[16];
        COLORREF tone = C_DIM;
        if (!valid) lstrcpyW(value, L"--");
        else if (unknown) { lstrcpyW(value, i == 2 ? L"?" : L"-?"); tone = C_YELLOW; }
        else {
            int amount = i == 0 ? gPreview.damageDealt : i == 1 ? gPreview.damageTaken : gPreview.blockGained;
            if (amount <= 0) lstrcpyW(value, L"0");
            else wsprintfW(value, i == 2 ? L"+%d" : L"-%d", amount);
            // 내 체력 0은 안전하다는 확정이라 초록으로 둔다. 나머지 0은 이번 턴 아무 일도 없다는 뜻이다.
            if (i == 0) tone = amount <= 0 ? C_DIM : gPreview.combatEnds ? C_GREEN : C_TEXT;
            else if (i == 1) tone = amount <= 0 ? C_GREEN : C_RED;
            else tone = amount <= 0 ? C_DIM : C_BLUE;
        }
        TextRect(dc, MakeRect(cl, r.top + 48, cr, r.top + 84), value, tone, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    RECT banner = MakeRect(r.left + 10, r.top + 90, r.right - 10, r.bottom - 8);
    if (!valid) {
        TextRect(dc, banner, !gRolled || gReadActive ? L"판독 후 예상 결과가 표시됩니다" : L"주사위를 슬롯에 배치하면 결과를 미리 봅니다",
            C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }
    const wchar_t* verdict;
    COLORREF tone;
    if (unknown) {
        // 지금 굴림 기준의 결말은 가능성으로만 덧붙인다 (예전 한 줄 미리보기와 같은 규칙).
        verdict = gPreview.playerDies ? L"! 읽기 오류 · 시스템 정지 가능"
                : gPreview.combatEnds ? L"! 읽기 오류 · 적 삭제 가능" : L"! 읽기 오류 · 결과 확정 아님";
        tone = C_YELLOW;
    } else if (gPreview.playerDies) { verdict = L"시스템 정지 확정"; tone = C_RED; }
    else if (gPreview.combatEnds) { verdict = L"적 삭제 확정"; tone = C_GREEN; }
    else {
        int reversed = ResolveOrderReversed(&gGame);
        TextRect(dc, banner, reversed ? L"역전 턴 · 연쇄부터 해결됩니다" : L"실행 전 예상 · 스페이스로 확정",
            reversed ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }
    Panel(dc, banner, MixColor(C_PANEL, tone, 16), tone);
    TextRect(dc, banner, verdict, tone, unknown ? gFontSmall : gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// SYSTEM 칸의 한 줄. 조립한 값은 조각을 미리 번역해 두어 영어에서도 온전하다.
struct SidebarRow {
    const wchar_t* tag;       // ASCII 태그 (번역하지 않는다)
    wchar_t text[112];
    wchar_t detail[112];      // 값 뒤에 흐리게 붙는 설명 (없으면 빈 문자열)
    COLORREF color;
    wchar_t desc[192];        // 값 아래 칸 폭 전체로 접어 쓰는 설명 (이미 번역된 글)
    COLORREF descColor;
    int gauge, gaugeMax;      // gaugeMax > 0이면 글 대신 칸 게이지로 그린다
};

static int WrappedTextHeight(HDC dc, const wchar_t* value, HFONT font, int width);

static SidebarRow* AddSidebarRow(SidebarRow* rows, int* count, int cap, const wchar_t* tag, const wchar_t* text, COLORREF color) {
    if (*count >= cap) return 0;
    SidebarRow* row = &rows[(*count)++];
    ZeroMemory(row, sizeof(*row));
    row->tag = tag; row->color = color;
    if (text) lstrcpynW(row->text, text, 112);
    return row;
}

// 이번 턴(next=0) 또는 다음 턴(next=1)에 잠긴 슬롯 이름을 " · "로 잇는다. 셔터가
// 아직 안 내려온 슬롯은 슬롯 그림과 같이 이번 턴 잠금에서 뺀다. 이름이 없으면 0.
static int JoinLockedSlots(wchar_t* out, int cap, int next) {
    out[0] = 0;
    for (int s = 0; s < SLOT_COUNT; ++s) {
        int now = SlotLockedThisTurn(&gGame, s) && !GimmickLockPending(s);
        if (next ? (!SlotLockedNextTurn(&gGame, s) || now) : !now) continue;
        if (out[0]) AppendBounded(out, cap, L" · ");
        AppendBounded(out, cap, LocalizeText(SLOT_SHORT_NAMES[s]));
    }
    return out[0] != 0;
}

// 해결 순서 줄의 값 칸. 경로 기믹이 발동하면 이 칸이 노이즈로 갈렸다 새 순서로 재조립된다.
static RECT SystemOrderRect() {
    RECT r = SystemInfoRect();
    return MakeRect(r.left + 12 + SIDEBAR_TAG_W, r.top + 34, r.right - 10, r.top + 34 + SIDEBAR_ROW_H);
}

// 해결 순서 한 줄. 번역문이 값 칸보다 길면(영어) "(역전!)" 꼬리를 떼고 " > "를 ">"로
// 좁힌다 — 역전은 붉은 글자가 이미 말하고, 순서 자체는 끝까지 읽혀야 한다.
static void FormatOrderLine(HDC dc, int reversed, int maxWidth, wchar_t* out, int cap) {
    lstrcpynW(out, LocalizeText(reversed ? L"연쇄 > 방어 > 공격 > 증폭 (역전!)" : L"증폭 > 공격 > 방어 > 연쇄"), cap);
    if (TextWidth(dc, out, gFontSmall) <= maxWidth) return;
    const wchar_t* bare = LocalizeText(reversed ? L"연쇄 > 방어 > 공격 > 증폭" : L"증폭 > 공격 > 방어 > 연쇄");
    int n = 0;
    for (const wchar_t* p = bare; *p && n < cap - 1; ++p) {
        if (*p == L' ' && (p[1] == L'>' || (p > bare && p[-1] == L'>'))) continue;
        out[n++] = *p;
    }
    out[n] = 0;
}

static void DrawSystemPanel(HDC dc) {
    RECT r = SystemInfoRect();
    DrawSidebarFrame(dc, r, L"SYSTEM / ROUTING", C_BLUE);
    const int cap = 16;
    SidebarRow rows[cap];
    int count = 0;
    wchar_t b[112], names[64];

    // ---- 판단에 필요한 상태: 칸이 모자라도 빼지 않는다 --------------------
    int reversed = ResolveOrderReversed(&gGame);
    RECT orderRect = SystemOrderRect();
    wchar_t order[112];
    FormatOrderLine(dc, reversed, orderRect.right - orderRect.left, order, 112);
    AddSidebarRow(rows, &count, cap, L"ORDER", order, reversed ? C_RED : C_TEXT);
    if (!reversed && gGame.boss.nextReversed) AddSidebarRow(rows, &count, cap, L"NEXT", L"예고: 다음 턴 순서 역전", C_YELLOW);
    if (JoinLockedSlots(names, 64, 0)) { wsprintfW(b, L"잠김 · %s", names); AddSidebarRow(rows, &count, cap, L"LOCK", b, C_RED); }
    if (JoinLockedSlots(names, 64, 1)) { wsprintfW(b, L"다음 턴 잠김 · %s", names); AddSidebarRow(rows, &count, cap, L"LOCK", b, C_YELLOW); }
    for (int d = 0; d < 3; ++d) if (DisplayDie(d)->offline) {
        wsprintfW(b, L"오프라인 · 주사위 %d", d + 1);
        AddSidebarRow(rows, &count, cap, L"OFFLINE", b, C_RED);
    }
    if (gGame.boss.nextOfflineDie >= 0 && gGame.boss.nextOfflineDie < 3) {
        wsprintfW(b, L"다음 턴 오프라인 · 주사위 %d", gGame.boss.nextOfflineDie + 1);
        AddSidebarRow(rows, &count, cap, L"OFFLINE", b, C_YELLOW);
    }
    int quarantined = 0;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        const Face* face = &DisplayDie(d)->faces[f];
        if (face->kind != FACE_EMPTY && face->quarantined != QUAR_NONE) ++quarantined;
    }
    if (quarantined) { wsprintfW(b, L"격리 중 · 면 %d개", quarantined); AddSidebarRow(rows, &count, cap, L"QUAR", b, C_RED); }
    if (gGame.boss.nextTargetDie >= 0 && gGame.boss.nextTargetDie < 3 && gGame.boss.nextTargetFace >= 0 && gGame.boss.nextTargetFace < 6) {
        wsprintfW(b, gGame.boss.nextTargetPermanent ? L"삭제 예고 · 주사위 %d %d면" : L"격리 예고 · 주사위 %d %d면",
            gGame.boss.nextTargetDie + 1, gGame.boss.nextTargetFace + 1);
        AddSidebarRow(rows, &count, cap, L"QUAR", b, gGame.boss.nextTargetPermanent ? C_RED : C_YELLOW);
    }
    if (gGame.boss.gimmick != GIMMICK_NONE && gGame.boss.gaugeMax > 0) {
        int family = BOSS_GIMMICK_INFO[gGame.boss.gimmick].family;
        if (family == FAM_PRESSURE || family == FAM_QUARANTINE) {
            COLORREF tone = C_YELLOW;
            for (int i = 0; i < gGame.enemyCount; ++i)
                if (IsBossKind(gGame.enemies[i].kind)) { tone = (COLORREF)GetEnemyInfoOrUnknown(gGame.enemies[i].kind)->color; break; }
            SidebarRow* row = AddSidebarRow(rows, &count, cap, family == FAM_PRESSURE ? L"PRESSURE" : L"INFECTION", L"",
                gGame.boss.empowered ? C_RED : tone);
            if (row) { row->gauge = gGame.boss.gauge; row->gaugeMax = gGame.boss.gaugeMax; }
        }
    }

    // ---- 볼륨 규칙: 예전 사이드바의 법칙·디스크 손상·난이도를 전문 그대로 옮긴다 ----
    int lawDrive = EffectiveLawDrive(&gGame);
    const DriveLawInfo* law = &DRIVE_LAW_INFO[lawDrive >= 0 && lawDrive < DRIVE_COUNT ? lawDrive : 0];
    SidebarRow* lawRow = AddSidebarRow(rows, &count, cap, L"LAW", law->name, C_YELLOW);
    if (lawRow) {
        if (gGame.selectedDrive == 2) lstrcpyW(lawRow->detail, gGame.driveRule.hotSwapUsed ? L"· USED" : L"· READY");
        lstrcpynW(lawRow->desc, LocalizeText(law->brief), 192); lawRow->descColor = C_TEXT;
    }
    int diskTagged = 0;
    for (int m = 0; m < 2; ++m) {
        const ModifierInfo* mod = ActiveModifierInfo(m ? gGame.modifierB : gGame.modifierA);
        if (!mod) continue;
        SidebarRow* row = AddSidebarRow(rows, &count, cap, diskTagged ? L"" : L"DISK", mod->name, C_YELLOW);
        diskTagged = 1;
        if (row) { lstrcpynW(row->desc, LocalizeText(mod->description), 192); row->descColor = C_DIM; }
    }
    const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
    if (difficulty) {
        // 예전 문구("이름 · 오염 N%\n관통은 방어 절반만")를 번역한 뒤 첫 줄은 값, 둘째 줄은 설명으로 나눈다.
        wchar_t d[112], first[112];
        wsprintfW(d, L"%s · 오염 %d%%\n관통은 방어 절반만", difficulty->name, difficulty->corruptPercent);
        lstrcpynW(first, LocalizeText(d), 112);
        const wchar_t* rest = L"";
        for (int i = 0; first[i]; ++i) if (first[i] == L'\n') { first[i] = 0; rest = first + i + 1; break; }
        SidebarRow* row = AddSidebarRow(rows, &count, cap, L"DIFF", first, (COLORREF)difficulty->color);
        if (row) { lstrcpynW(row->desc, rest, 192); row->descColor = C_DIM; }
    }

    // 설명은 칸 폭 전체로 접는다. 칸이 모자라면 뒤쪽 설명부터 한 줄로 줄이고, 그래도
    // 모자라면 설명 줄을 뺀다. 상태 줄은 앞에 있고 설명이 없으므로 줄지 않는다.
    int descLeft = r.left + 12, descRight = r.right - 10;
    int lines[cap], total = 0;
    for (int i = 0; i < count; ++i) {
        lines[i] = rows[i].desc[0]
            ? (WrappedTextHeight(dc, rows[i].desc, gFontSmall, descRight - descLeft) + SIDEBAR_LINE_H - 1) / SIDEBAR_LINE_H : 0;
        total += SIDEBAR_ROW_H + lines[i] * SIDEBAR_LINE_H;
    }
    int avail = r.bottom - r.top - 34 - 4;
    for (int i = count - 1; i >= 0 && total > avail; --i)
        if (lines[i] > 1) { total -= (lines[i] - 1) * SIDEBAR_LINE_H; lines[i] = 1; }
    for (int i = count - 1; i >= 0 && total > avail; --i)
        if (lines[i] == 1) { total -= SIDEBAR_LINE_H; lines[i] = 0; }

    int y = r.top + 34;
    for (int i = 0; i < count && y + SIDEBAR_ROW_H <= r.bottom - 4; ++i) {
        const SidebarRow* row = &rows[i];
        if (row->tag && row->tag[0])
            TextRect(dc, MakeRect(r.left + 12, y, r.left + 12 + SIDEBAR_TAG_W, y + SIDEBAR_ROW_H), row->tag, C_DIM, gFontSmall, DT_SINGLELINE | DT_VCENTER);
        RECT value = MakeRect(r.left + 12 + SIDEBAR_TAG_W, y, r.right - 10, y + SIDEBAR_ROW_H);
        if (row->gaugeMax > 0) {
            DrawPacketGrid(dc, MakeRect(value.left, y + 6, value.left + 132, y + 15), row->gauge, row->gaugeMax, row->color, C_LINE);
            wsprintfW(b, L"%d / %d", row->gauge, row->gaugeMax);
            TextRect(dc, MakeRect(value.left + 142, y, value.right, y + SIDEBAR_ROW_H), b, row->color, gFontSmall, DT_SINGLELINE | DT_VCENTER);
        } else {
            TextRect(dc, value, row->text, row->color, gFontSmall, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (row->detail[0]) {
                int used = TextWidth(dc, row->text, gFontSmall) + 8;
                if (value.left + used < value.right - 24)
                    TextRect(dc, MakeRect(value.left + used, y, value.right, y + SIDEBAR_ROW_H), row->detail, C_DIM, gFontSmall,
                        DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }
        }
        y += SIDEBAR_ROW_H;
        if (lines[i] == 1)
            TextRect(dc, MakeRect(descLeft, y, descRight, y + SIDEBAR_LINE_H), row->desc, row->descColor, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
        else if (lines[i] > 1)
            DrawSidebarWrapped(dc, descLeft, y, descRight, y + lines[i] * SIDEBAR_LINE_H, row->desc, row->descColor);
        y += lines[i] * SIDEBAR_LINE_H;
    }
}

// 시스템 기록. 예전 사이드바와 같이 최근 세 줄이고, 가장 최근 줄만 밝다.
static void DrawHistoryPanel(HDC dc) {
    RECT r = CombatHistoryRect();
    DrawSidebarFrame(dc, r, L"HISTORY", C_GREEN);
    for (int i = 0; i < 3; ++i) {
        int y = r.top + 32 + i * 20;
        TextRect(dc, MakeRect(r.left + 12, y, r.right - 10, y + 20), gGame.logs[i], i == 0 ? C_TEXT : C_DIM,
            gFontSmall, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

static void DrawCombatSidebar(HDC dc) {
    DrawTargetPanel(dc);
    // 재생 중에는 예상부터 기록까지를 계산 패널이 덮어 쓴다 (DrawTurnCalculation).
    if (gTurnTraceActive) return;
    DrawForecastPanel(dc);
    DrawSystemPanel(dc);
    DrawHistoryPanel(dc);
}

static void DrawCombatAtmosphere(HDC dc) {
    if (!FxDecorOn()) return;
    int drive = gGame.selectedDrive;
    if (drive < 0 || drive >= DRIVE_COUNT) drive = 0;
    COLORREF tone = (COLORREF)DRIVE_INFO[drive].color;
    int tick = (int)(GetTickCount() % 60000);
    int saved = SaveDC(dc);
    IntersectClipRect(dc, 22, 82, 698, 714);
    // Power follows the actual sockets. Empty arena space stays quiet.
    COLORREF dim = MixColor(C_BG, tone, FxScale(18));
    DrawLine(dc, 24, 88, 694, 88, dim, 1);
    for (int i = 0; i < gGame.enemyCount; ++i) {
        if (GimmickSummonPending(i)) continue;
        RECT r = EnemyRect(i); int cx = (r.left + r.right) / 2;
        DrawLine(dc, cx, 88, cx, r.top, dim, 1);
        if (EnemyDisplayHp(i) > 0) {
            int head = (tick + i * 630) % 3100;
            int pulse = head < 180 ? 54 : 19;
            Fill(dc, MakeRect(cx - 4, 86, cx + 5, 89), MixColor(C_BG, tone, FxScale(pulse)));
        }
    }
    for (int d = 0; d < 3; ++d) {
        const DieState* die = DisplayDie(d);
        if (die->assignedSlot < 0 || die->assignedSlot >= SLOT_COUNT) continue;
        RECT from = DieRect(d), to = SlotRect(die->assignedSlot);
        int x0 = (from.left + from.right) / 2, x1 = (to.left + to.right) / 2, y = 551 + d * 4;
        COLORREF wire = MixColor(C_BG, SlotAccent(die->assignedSlot), FxScale(25));
        DrawLine(dc, x0, from.top, x0, y, wire, 1);
        DrawLine(dc, x0, y, x1, y, wire, 1);
        DrawLine(dc, x1, y, x1, to.bottom, wire, 1);
    }
    RestoreDC(dc, saved);
}

// 판독·실행·KEYB. 재생 중에도 판독·실행 버튼은 자리를 지키고 비활성으로만 그린다 —
// 버튼이 사라졌다 나타나면 판이 들썩여 보인다. KEYB 줄은 재생 중 티커가 쓴다.
// 상태는 셋으로 뚜렷이 가른다: 비활성(흐린 판·회색 글), 활성(색 테두리·색 글), 호버(밝은 판).
static void DrawCombatControls(HDC dc) {
    int tracing = gTurnTraceActive;
    RECT read = ReadButtonRect();
    int canRead = !tracing && !gRolled && !gReadActive;
    int readHover = canRead && Inside(read, gMouse.x, gMouse.y);
    Panel(dc, read, canRead ? (readHover ? RGB(34, 86, 70) : RGB(24, 58, 49)) : C_PANEL, canRead ? C_GREEN : C_LINE);
    DrawCardMotion(dc, read, C_GREEN, 0, canRead);
    TextRect(dc, read, L"판독 [R]", canRead ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT end = EndTurnRect();
    int canRun = !tracing && gRolled && !gReadActive;
    int hover = canRun && Inside(end, gMouse.x, gMouse.y);
    Panel(dc, end, hover ? RGB(71, 42, 42) : canRun ? RGB(40, 25, 28) : C_PANEL,
        hover ? C_RED : canRun ? MixColor(C_LINE, C_RED, 55) : C_LINE);
    DrawCardMotion(dc, end, C_RED, 0, canRun);
    if (canRun && gPreview.valid && gPreview.combatEnds && !gPreview.uncertain && !gPreview.playerDies && FxDecorOn())
        DrawOrbitCorners(dc, end, (int)(GetTickCount() % 2400), C_GREEN, FxScale(85));
    TextRect(dc, end, L"실행 [스페이스]", canRun ? C_RED : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (!tracing && (TacticalRerollAvailable(&gGame) || IsTsrInstalled(&gGame, TSR_KEYB))) {
        RECT keyb = KeybButtonRect();
        int canReroll = gRolled && !gGame.keybUsedThisTurn && gGame.selectedDie >= 0;
        int hoverKeyb = canReroll && Inside(keyb, gMouse.x, gMouse.y);
        COLORREF accent = (COLORREF)TSR_INFO[TSR_KEYB].color;
        Panel(dc, keyb, canReroll ? (hoverKeyb ? RGB(52, 34, 46) : RGB(36, 25, 34)) : C_PANEL, canReroll ? accent : C_LINE);
        wchar_t b[64];
        if (gGame.keybUsedThisTurn) lstrcpyW(b, L"KEYB 사용됨");
        else if (!gRolled) lstrcpyW(b, L"KEYB · 판독 후");
        else if (gGame.selectedDie < 0) lstrcpyW(b, L"KEYB · 주사위 선택");
        else wsprintfW(b, L"주사위 %d 재굴림 [K]", gGame.selectedDie + 1);
        TextRect(dc, keyb, b, canReroll ? accent : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// One clear beat beside the process, away from its silhouette and health bar.
// Only events already reached by the replay may contribute a label or number.
static void DrawCombatBeat(HDC dc) {
    if (!SoloProcessStage()) return;
    RECT box = MakeRect(366, 334, 888, 369);
    COLORREF tone = C_DIM;
    const wchar_t* label = gTurnTraceActive ? L"EXECUTING" : gReadActive ? L"READING SECTORS" : gRolled ? L"COMMAND READY" : L"AWAITING READ";
    wchar_t beat[96]; beat[0] = 0;
    int latest = -1, age = 0;
    for (int i = 0; i < gGame.combatFxCount && CombatFxPlaying(); ++i) {
        int t = CombatFxElapsed(i);
        if (t >= 0 && t < 720) { latest = i; age = t; }
    }
    if (latest >= 0) {
        const CombatFxEvent& fx = gGame.combatFx[latest];
        tone = fx.type == CFX_AMPLIFY ? C_GREEN : fx.type == CFX_DEFEND || (fx.flags & CFXF_BLOCKED) ? C_BLUE
            : fx.type == CFX_CHAIN ? C_YELLOW : C_RED;
        label = fx.type == CFX_AMPLIFY ? L"AMPLIFY" : fx.type == CFX_DEFEND ? L"SHIELD ONLINE"
            : fx.type == CFX_ATTACK_LAUNCH ? L"EXECUTING" : fx.type == CFX_ENEMY_STRIKE ? L"HOST DAMAGE"
            : fx.type == CFX_CHAIN ? L"CHAIN REACTION" : fx.type == CFX_BURN ? L"BURN" : L"IMPACT";
        if (fx.flags & CFXF_BLOCKED) label = L"BLOCKED";
        else if (fx.flags & CFXF_WASTED) label = L"SIGNAL LOST";
        else if (fx.flags & CFXF_KILL) { label = L"PROCESS DELETED"; tone = C_GREEN; }
        wsprintfW(beat, L"%s  /  %d", label, fx.value);
    }
    Fill(dc, box, MixColor(C_BG, tone, latest >= 0 ? 10 : 3));
    Fill(dc, MakeRect(box.left, box.top, box.left + 3, box.bottom), tone);
    TextRect(dc, MakeRect(box.left + 14, box.top, box.right - 8, box.bottom), beat[0] ? beat : label,
        tone, gFontMedium, DT_VCENTER | DT_SINGLELINE);
    if (latest >= 0 && FxDecorOn()) {
        int end = box.left + (box.right - box.left) * (1000 - EaseOutCubic(Track(age, 0, 720))) / 1000;
        Fill(dc, MakeRect(box.left, box.bottom - 2, end, box.bottom), MixColor(C_BG, tone, FxScale(65)));
    }
}

static void DrawCombat(HDC dc, int width, int height) {
    (void)width; (void)height;   // 전투판은 고정 좌표, 사이드바는 ui.h의 레이아웃 상수를 쓴다
    DrawCombatAtmosphere(dc);
    for (int i = 0; i < gGame.enemyCount; ++i) if (!GimmickSummonPending(i)) DrawEnemy(dc, i);   // 격리막 안의 카드는 연출이 연다
    DrawTsrPanel(dc);
    DrawRoutingState(dc);   // 지금 이어진 해결 순서 (N:\ 계열 보스전에서만)
    DrawCombatFxBack(dc);   // 신호는 슬롯·주사위 아래를 지나간다
    // 판독이 끝난 뒤에만 계산한다. 판독 전에 미리보기를 돌리면 아직 가려 둔 굴림이 새어 나간다.
    if (gRolled && !gReadActive && !gTurnTraceActive) PreviewTurn(&gGame, &gPreview);
    else ZeroMemory(&gPreview, sizeof(gPreview));
    // 조작 순서 안내. 예상 결과는 오른쪽 FORECAST 칸이 크게 보여 주므로 여기에는
    // 해결 순서만 남긴다. 재생 중에는 지운다 — 이 띠가 신호가 적으로 건너가는 통로다.
    if (!gTurnTraceActive) {
        if (ResolveOrderReversed(&gGame)) Text(dc, 28, 388, L"① 배치  →  ② 스페이스: 연쇄 > 방어 > 공격 > 증폭 (역전!)  →  ③ 적 행동", C_RED, gFontSmall);
        else Text(dc, 28, 388, L"① 배치  →  ② 스페이스: 증폭 > 공격 > 방어 > 연쇄  →  ③ 적 행동", C_DIM, gFontSmall);
    }
    for (int i = 0; i < SLOT_COUNT; ++i) DrawSlot(dc, i);
    for (int i = 0; i < 3; ++i) { DrawDie(dc, i); DrawFaceStrip(dc, i); }
    DrawCombatFxFront(dc);  // 충격·파편·피해 숫자는 판 위에 얹는다
    DrawCombatBeat(dc);
    DrawCombatSidebar(dc);
    DrawCombatControls(dc);
}

RECT DriveCardRect(int i) {
    int count = gGame.driveChoiceCount;
    if (count <= 0 || count > 3 || i < 0 || i >= count) return MakeRect(0, 0, 0, 0);
    int left = (BASE_WIDTH - (count * 344 - 24)) / 2 + i * 344;
    return MakeRect(left, 140, left + 320, 640);
}

static void DrawDriveModifier(HDC dc, const RECT& card, int top, int modifier) {
    Text(dc, card.left + 16, top, MODIFIER_INFO[modifier].name, C_YELLOW, gFontSmall);
    TextRect(dc, MakeRect(card.left + 16, top + 21, card.right - 14, top + 58), MODIFIER_BRIEF[modifier], C_DIM, gFontSmall, DT_WORDBREAK);
}

static void DrawDriveSelect(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_DRIVE_SELECT, C_BLUE, width, height);
    if ((gGame.clearedMask & 0x3F) == 0x3F) {
        TextRect(dc, MakeRect(280, 116, width - 280, 146), L"캠페인 복구 완료 · 모든 일반 볼륨을 재플레이할 수 있습니다", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
        RECT prev = ReplayPrevRect(), next = ReplayNextRect();
        int hoverPrev = Inside(prev, gMouse.x, gMouse.y), hoverNext = Inside(next, gMouse.x, gMouse.y);
        Panel(dc, prev, hoverPrev ? RGB(28, 39, 48) : C_PANEL_2, hoverPrev ? C_BLUE : C_LINE);
        Panel(dc, next, hoverNext ? RGB(28, 39, 48) : C_PANEL_2, hoverNext ? C_BLUE : C_LINE);
        TextRect(dc, prev, L"◀ 이전 볼륨", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, next, L"다음 볼륨 ▶", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (!gGame.driveChoiceCount) {
        TextRect(dc, MakeRect(40, 284, width - 40, 328), L"모든 일반 볼륨을 복구했습니다.", C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(40, 344, width - 40, 382), L"좌우 화살표 또는 아래 버튼으로 복구한 볼륨을 다시 마운트할 수 있습니다.", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
        return;
    }
    TextRect(dc, MakeRect(0, 78, width, 102), L"감염된 저장소 감지  →  [현재: 탐색 볼륨 선택]  →  마운트  →  전투", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 98, width, 126), L"탐색할 볼륨을 선택하십시오 · 디스크 손상과 볼륨 특성이 미리 공개됩니다", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    for (int i = 0; i < gGame.driveChoiceCount; ++i) {
        RECT r = DriveCardRect(i);
        const DriveInfo* drive = &DRIVE_INFO[gGame.driveChoices[i]];
        int hover = Inside(r, gMouse.x, gMouse.y);
        // 카드가 왼쪽부터 한 장씩 들어온다. 쓰는 셔터는 DrawSelectionCardExit가 카드를
        // 삼킬 때 쓰는 것과 같고, 시간만 뒤집었다 (닫힘의 EaseInCubic → 열림의 EaseOutCubic).
        // 열고 닫는 것이 같은 기계의 두 방향으로 읽혀야 이 화면의 앞뒤가 이어진다.
        // 카드 내용마다 등장 오프셋을 먹이는 방법도 해 봤지만 본문 그리기 코드를 전부
        // 손대야 했고 글줄이 따로 놀았다. 다 그린 뒤 물러나는 덮개를 얹는 쪽이 깔끔하다.
        int opened = FxDecorOn() ? EaseOutCubic(Track(SceneElapsed(), i * 100, i * 100 + 300)) : 1000;
        Panel(dc, r, hover ? RGB(24, 37, 46) : C_PANEL, hover ? (COLORREF)drive->color : C_LINE);
        DrawCardMotion(dc, r, (COLORREF)drive->color, i, hover);
        wchar_t b[16]; wsprintfW(b, L"[%d]", i + 1);
        Text(dc, r.left + 12, r.top + 10, b, C_DIM, gFontSmall);
        const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.driveDifficulty[i]);
        wchar_t badge[64];
        if (difficulty) {
            wsprintfW(badge, L"난이도 · %s", difficulty->name);
            TextRect(dc, MakeRect(r.left + 56, r.top + 10, r.right - 12, r.top + 30), badge, (COLORREF)difficulty->color, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        }
        RECT letterRect = MakeRect(r.left + 8, r.top + 20, r.right - 8, r.top + 86);
        // 셔터가 열리는 동안에만 문자가 노이즈에서 풀려 나온다. 다 열리면 예전과 같은 60이다.
        DrawSectorStatic(dc, letterRect, gGame.driveChoices[i], (int)(GetTickCount() / 260u),
                         60 + FxScale(460) * (1000 - opened) / 1000);
        TextRect(dc, letterRect, drive->letter, (COLORREF)drive->color, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawScanlines(dc, letterRect);
        TextRect(dc, MakeRect(r.left + 8, r.top + 92, r.right - 8, r.top + 118), drive->label, (COLORREF)drive->color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 16, r.top + 126, r.right - 14, r.top + 178), drive->description, C_DIM, gFontSmall, DT_WORDBREAK);
        Fill(dc, MakeRect(r.left + 12, r.top + 182, r.right - 12, r.top + 183), C_LINE);
        Text(dc, r.left + 16, r.top + 192, L"디스크 손상", C_RED, gFontSmall);
        if (difficulty) {
            wsprintfW(badge, L"오염(관통) %d%%", difficulty->corruptPercent);
            TextRect(dc, MakeRect(r.left + 130, r.top + 192, r.right - 16, r.top + 212), badge, (COLORREF)difficulty->color, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        }
        DrawDriveModifier(dc, r, r.top + 216, drive->modifierA);
        DrawDriveModifier(dc, r, r.top + 278, drive->modifierB);
        Fill(dc, MakeRect(r.left + 12, r.top + 342, r.right - 12, r.top + 343), C_LINE);
        Text(dc, r.left + 16, r.top + 352, L"볼륨 특성", C_GREEN, gFontSmall);
        TextRect(dc, MakeRect(r.left + 16, r.top + 374, r.right - 14, r.top + 414), drive->perkText, C_TEXT, gFontSmall, DT_WORDBREAK);
        const DriveLawInfo* law = &DRIVE_LAW_INFO[gGame.driveChoices[i]];
        Text(dc, r.left + 16, r.top + 420, L"VOLUME LAW", C_BLUE, gFontSmall);
        TextRect(dc, MakeRect(r.left + 16, r.top + 442, r.right - 14, r.top + 482), law->brief, C_TEXT, gFontSmall, DT_WORDBREAK);
        TextRect(dc, MakeRect(r.left + 8, r.bottom - 34, r.right - 8, r.bottom - 10), hover ? L"클릭하여 마운트" : L"클릭 또는 숫자 키", hover ? (COLORREF)drive->color : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        if (opened < 1000) {
            int half = (r.bottom - r.top) / 2, cover = half * (1000 - opened) / 1000;
            Fill(dc, MakeRect(r.left, r.top, r.right, r.top + cover), C_BG);
            Fill(dc, MakeRect(r.left, r.bottom - cover, r.right, r.bottom), C_BG);
            // 입술은 셔터가 실제로 벌어진 뒤에만 긋는다. 아직 다 닫혀 있을 때 그리면
            // 위아래 입술이 한 줄로 겹쳐, 빈 배경 한가운데 정체불명의 선 하나만 남는다.
            if (cover > 5 && cover < half - 2) {
                Fill(dc, MakeRect(r.left, r.top + cover - 2, r.right, r.top + cover), C_LINE);
                Fill(dc, MakeRect(r.left, r.bottom - cover, r.right, r.bottom - cover + 2), C_LINE);
            }
        }
    }

    // 카드 아래 진행도 띠. 이번에 제시된 볼륨은 테두리로, 이미 복구한 볼륨은 채움으로
    // 구분되므로 "남은 것이 무엇인지"를 카드를 세지 않고도 읽을 수 있다.
    uint8_t offered = 0;
    for (int i = 0; i < gGame.driveChoiceCount; ++i)
        if (gGame.driveChoices[i] >= 0 && gGame.driveChoices[i] < 6)
            offered |= (uint8_t)(1u << gGame.driveChoices[i]);
    wchar_t progress[96];
    FormatShardProgress(gGame.clearedMask, progress);
    int stripW = ShardStripWidth(0), labelW = 190;
    int stripLeft = (width - (labelW + 16 + stripW)) / 2;
    TextRect(dc, MakeRect(stripLeft, 650, stripLeft + labelW, 678), progress,
             gGame.clearedMask == 0x3F ? C_GREEN : C_TEXT, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DrawShardStrip(dc, stripLeft + labelW + 16, 650, gGame.clearedMask, offered, 0);
    TextRect(dc, MakeRect(0, height - 70, width, height - 40), L"카드마다 서로 다른 난이도가 배정됩니다 · 난이도는 오염(관통) 피해 배율이며, 방어도는 관통을 절반만 막습니다", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 선택 화면과 다음 화면 사이의 연결 동작. 탈락한 카드는 위아래 셔터가 닫히며
// 사라지고, 고른 카드는 스캔 헤드와 겹테두리로 잠긴다. 별도 스냅샷 없이 이전
// 화면을 다시 그리므로 창 배율과 무관하고, 모든 위치는 경과 시간으로만 정해진다.
static void DrawSelectionCardExit(HDC dc, const RECT& card, int chosen, int elapsed,
                                  int duration, COLORREF accent, const wchar_t* lockedLabel) {
    int p = Track(elapsed, 0, duration);
    if (!chosen) {
        int close = EaseInCubic(Track(elapsed, 45, duration - 45));
        int half = (card.bottom - card.top) / 2;
        int cover = half * close / 1000;
        COLORREF shutter = RGB(7, 11, 16);
        Fill(dc, MakeRect(card.left, card.top, card.right, card.top + cover), shutter);
        Fill(dc, MakeRect(card.left, card.bottom - cover, card.right, card.bottom), shutter);
        if (cover > 5) {
            Fill(dc, MakeRect(card.left, card.top + cover - 2, card.right, card.top + cover), C_LINE);
            Fill(dc, MakeRect(card.left, card.bottom - cover, card.right, card.bottom - cover + 2), C_LINE);
        }
        return;
    }

    int lock = EaseOutBack(Track(elapsed, 0, duration * 3 / 4));
    int expand = FxScale(4 + 13 * (1000 - (lock > 1000 ? 1000 : lock)) / 1000);
    DrawPulseFrame(dc, card, expand, 2, MixColor(C_BG, accent, FxScale(72)));
    Outline(dc, card, accent, lock > 720 ? 3 : 2);

    // 세로 판독선이 카드를 훑고 지나간다. 뒤쪽에 짧은 꼬리를 남겨 실제로
    // 선택 내용을 읽어 잠그는 것처럼 보이게 한다.
    int scan = Track(elapsed, 35, duration * 3 / 4);
    int head = Lerp(card.left + 2, card.right - 2, EaseOutCubic(scan));
    Fill(dc, MakeRect(head - 2, card.top + 2, head + 2, card.bottom - 2), accent);
    for (int i = 1; i <= 3; ++i) {
        int x = head - i * 7;
        if (x > card.left) Fill(dc, MakeRect(x, card.top + 5, x + 1, card.bottom - 5), MixColor(C_BG, accent, 58 - i * 12));
    }

    if (p > 500) {
        int h = 38 * (p - 500) / 500;
        if (h > 38) h = 38;
        RECT stamp = MakeRect(card.left + 34, (card.top + card.bottom) / 2 - h / 2,
                              card.right - 34, (card.top + card.bottom) / 2 + h / 2);
        Fill(dc, stamp, RGB(5, 9, 13));
        Outline(dc, stamp, accent, 2);
        if (h >= 30) TextRect(dc, stamp, lockedLabel, accent, gFontMedium,
                              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (FxDecorOn() && elapsed > duration / 2) {
        // Contact spits from the latch, not from the middle of the written card.
        int age = elapsed - duration / 2;
        int distance = EaseOutCubic(Track(age, 0, duration / 2)) * 32 / 1000;
        COLORREF contact = MixColor(C_BG, accent, FxScale(75 * (1000 - Track(age, 0, duration / 2)) / 1000));
        DrawLine(dc, card.left - distance, card.bottom - 10, card.left - distance - 6, card.bottom - 8, contact, 1);
        DrawLine(dc, card.right + distance, card.bottom - 10, card.right + distance + 6, card.bottom - 8, contact, 1);
    }
}

// ---- 볼륨 잠금 -------------------------------------------------------------
// 선택 화면에서 마운트로 넘어가는 첫 구간. 예전에는 탈락한 카드 위로 셔터가
// 닫히고 고른 카드에 도장이 찍혔다 - "덮였다"이지 "뽑혔다"가 아니어서, 곧바로
// 이어지는 안착(카드 한 장이 판을 떠나 스핀들에 눕는다)과 동작이 이어지지
// 않았다. 이제 탈락한 카드는 판 밖으로 뜯겨 나가고, 남은 한 장을 좌우 레일이
// 물어 고정한다. 다음 구간은 그 물린 카드를 그대로 집어 든다.

// 뜯겨 나가는 카드. 본문을 다시 그리지 않는다 - 이 속도에서는 읽히지 않고,
// 글자까지 날아가면 판이 아니라 화면이 흩어지는 것으로 보인다.
static void DrawEjectedCard(HDC dc, const RECT& card, int drive, int dir, int p) {
    // 원래 자리를 먼저 비운다. 카드 옆의 랙까지 걷어야 "빠져나간 자리"가 된다.
    Fill(dc, MakeRect(card.left - 12, card.top - 6, card.right + 10, card.bottom + 6), C_BG);
    if (p >= 1000) return;
    COLORREF tone = (COLORREF)DRIVE_INFO[drive].color;
    int travel = EaseInCubic(p), fade = 1000 - p;
    int dx = dir * (BASE_WIDTH / 2 + 220) * travel / 1000;
    int tilt = dir * 30 * travel / 1000;
    RECT slab = MakeRect(card.left + dx, card.top + tilt, card.right + dx, card.bottom + tilt);
    Panel(dc, slab, MixColor(C_BG, C_PANEL, 30 + 70 * fade / 1000), MixColor(C_BG, tone, 18 + 62 * fade / 1000));
    TextRect(dc, MakeRect(slab.left + 8, slab.top + 22, slab.right - 8, slab.top + 88),
             DRIVE_INFO[drive].letter, MixColor(C_BG, tone, 16 + 74 * fade / 1000), gFontHuge,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(slab.left + 8, slab.top + 94, slab.right - 8, slab.top + 120),
             DRIVE_INFO[drive].label, MixColor(C_BG, tone, 10 + 44 * fade / 1000), gFontMedium,
             DT_CENTER | DT_SINGLELINE);
    if (!FxDecorOn()) return;
    // 뜯긴 자리. 판에 붙어 있던 쪽 모서리에서만 파편이 남는다.
    DrawBandGlitch(dc, slab, p, FxScale(4 + 16 * travel / 1000), drive * 31 + 7, 7);
    int edge = dir < 0 ? card.right : card.left;
    DrawPixelBurst(dc, edge, (card.top + card.bottom) / 2, p, 1000, FxScale(10), drive * 13 + 3, tone);
}

// 좌우에서 물려 들어오는 고정 레일. 카드가 스핀들로 가기 전에 한 번 잡힌다.
static void DrawLockRail(HDC dc, const RECT& card, int clamp, COLORREF tone) {
    int gap = Lerp(150, 0, clamp);
    for (int side = 0; side < 2; ++side) {
        int x = side ? card.right + 4 + gap : card.left - 16 - gap;
        RECT rail = MakeRect(x, card.top + 74, x + 12, card.bottom - 74);
        Panel(dc, rail, MixColor(C_BG, tone, 22), MixColor(C_BG, tone, 58));
        for (int y = rail.top + 12; y < rail.bottom - 8; y += 26)
            Fill(dc, MakeRect(rail.left + 3, y, rail.right - 3, y + 4), MixColor(C_BG, tone, 44));
    }
}

static void DrawVolumeLock(HDC dc, int width, int height, int elapsed) {
    Fill(dc, MakeRect(0, 68, width, height), C_BG);
    DrawDriveSelect(dc, width, height);
    int chosen = gDescentChoiceIndex;
    if (chosen < 0 || chosen >= gGame.driveChoiceCount) return;
    int drive = gGame.driveChoices[chosen];
    COLORREF tone = (COLORREF)DRIVE_INFO[drive].color;
    RECT card = DriveCardRect(chosen);

    // 탈락한 카드는 바깥으로 나간다. 고른 카드에서 먼 것부터 뜯겨야 빈자리가
    // 가운데로 모이고, 시선이 남는 한 장에 붙는다.
    for (int i = 0; i < gGame.driveChoiceCount; ++i) {
        if (i == chosen) continue;
        int away = i < chosen ? chosen - i : i - chosen;
        int delay = 60 + (away - 1) * 45;
        DrawEjectedCard(dc, DriveCardRect(i), gGame.driveChoices[i], i < chosen ? -1 : 1,
                        Track(elapsed, delay, delay + 320));
    }

    // 고른 카드를 한 번 훑고 지나가는 판독선. 뒤에 짧은 꼬리를 남겨 실제로
    // 읽어 잠그는 것으로 보이게 한다 (여기까지는 예전 잠금과 같은 동작이다).
    int scan = Track(elapsed, 20, 300);
    if (scan < 1000) {
        int head = Lerp(card.left + 2, card.right - 2, EaseOutCubic(scan));
        Fill(dc, MakeRect(head - 2, card.top + 2, head + 2, card.bottom - 2), tone);
        for (int i = 1; i <= 3; ++i) {
            int x = head - i * 8;
            if (x > card.left) Fill(dc, MakeRect(x, card.top + 5, x + 1, card.bottom - 5), MixColor(C_BG, tone, 58 - i * 13));
        }
    }

    int clamp = EaseOutCubic(Track(elapsed, 170, 350));
    if (clamp > 0) DrawLockRail(dc, card, clamp, tone);
    Outline(dc, card, tone, clamp >= 1000 ? 3 : 2);

    // 물린 순간. 카드가 한 번 주저앉고 테두리가 바깥으로 퍼진다.
    int kick = Track(elapsed, 350, 470);
    if (kick > 0 && kick < 1000) {
        DrawPulseFrame(dc, card, FxScale(4 + 22 * (1000 - kick) / 1000), 3, MixColor(C_BG, tone, FxScale(80 * (1000 - kick) / 1000)));
        if (FxDecorOn()) {
            DrawEdgeStatic(dc, MakeRect(0, 68, width, height), elapsed / NOISE_CHURN_MS,
                           FxScale(560 * (1000 - kick) / 1000), 46);
            DrawPixelBurst(dc, card.left, card.bottom - 12, elapsed - 350, 220, FxScale(12), drive * 9 + 1, tone);
            DrawPixelBurst(dc, card.right, card.bottom - 12, elapsed - 350, 220, FxScale(12), drive * 9 + 2, tone);
        }
    }

    // 도장. 여기서부터 이 카드는 선택지가 아니라 대상이다.
    int stampP = Track(elapsed, 330, 560);
    if (stampP > 0) {
        int h = 44 * EaseOutBack(stampP) / 1000;
        if (h > 44) h = 44;
        RECT stamp = MakeRect(card.left + 26, (card.top + card.bottom) / 2 - h / 2,
                              card.right - 26, (card.top + card.bottom) / 2 + h / 2);
        Fill(dc, stamp, RGB(5, 9, 13));
        Outline(dc, stamp, tone, 2);
        if (h >= 34) TextRect(dc, stamp, L"VOLUME LOCKED", tone, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 마지막 150ms는 기계 안이 어두워진다. 다음 구간의 판이 이 어둠에서 선다.
    // 덮는 색에는 알파가 없으므로 줄 간격을 좁혀 가며 잠근다 - 마지막 프레임은
    // 빈틈이 없어, 안착 구간이 검은 화면에서 이어받는다.
    int dim = Track(elapsed, MOUNT_LOCK_MS - 150, MOUNT_LOCK_MS);
    if (dim > 0) {
        int step = dim > 700 ? 1 : dim > 380 ? 2 : 3;
        for (int y = 68; y < height; y += step) Fill(dc, MakeRect(0, y, width, y + 1), C_INK);
    }
}

// ---------------------------------------------------------------------------
// 디렉터리 선택 화면
//
// 카드 두 장에 노드명·위험도·정확한 수치·비용·보상 tier를 그대로 적는다.
// 숨기는 것은 아직 판독하지 않은 적 코드뿐이고, 그것도 LOGS가 열어 준다.
// 여기서는 게임 상태를 절대 바꾸지 않는다 (리페인트로 선택지가 다시 뽑히면 안 된다).
// ---------------------------------------------------------------------------

RECT DirectoryChoiceRect(int i) { int left = LEGACY_X + 120 + i * 460; return MakeRect(left, 150, left + 420, 566); }

static void AppendPathSegment(wchar_t* out, const wchar_t* segment) {
    int length = lstrlenW(out);
    if (length > 0 && out[length - 1] != L'\\') lstrcatW(out, L"\\");
    lstrcatW(out, segment);
}

// 적 코드를 공개해도 되는가. 판독했거나 이번 층 LOGS가 켜져 있으면 공개다.
static int DirectoryCodeVisible(int kind) {
    return IsEnemyScanned(&gGame, kind) || DirectoryIntelActive(&gGame);
}

// 미판독 코드는 폭을 유지한 채 헥스·기호로 갈려 보인다.
static void DirectoryCodeText(int kind, wchar_t* out, int cap) {
    const EnemyInfo* info = GetEnemyInfoOrUnknown(kind);
    if (DirectoryCodeVisible(kind)) { lstrcpynW(out, info->code, cap); return; }
    CorruptCode(info->code, out, cap, kind * 13 + 7, GetTickCount());
}

// 노드마다 지금 상태에 맞춘 구체적인 수치 한 줄.
static void DirectoryDetailText(const GameState* game, int kind, uint8_t payload, wchar_t* out, int cap) {
    out[0] = 0;
    int floor = game->floor > 2 ? 2 : game->floor;
    switch (kind) {
    case DIR_NODE_TEMP: {
        int missing = game->playerMaxHp - game->playerHp;
        int gain = missing < DIR_TEMP_HEAL ? missing : DIR_TEMP_HEAL;
        wsprintfW(out, L"체력 %d → %d / %d", game->playerHp, game->playerHp + gain, game->playerMaxHp);
        break;
    }
    case DIR_NODE_CACHE: {
        int now = EffectiveCapacity(game);
        // 층이 끝나면 보너스가 사라지고 다음 층 한도로 조여든다.
        int nextFloor = floor < 2 ? floor + 1 : 2;
        int nextLimit = now - FLOOR_CAPACITY[floor] + FLOOR_CAPACITY[nextFloor];
        wsprintfW(out, L"한도 %dB → %dB · 층 종료 후 %dB", now, now + DIR_CACHE_BYTES, nextLimit);
        break;
    }
    case DIR_NODE_LOGS:
        wsprintfW(out, L"%d층 남은 프로세스와 보스 코드 공개", floor + 1);
        break;
    case DIR_NODE_INFECTED: {
        int kindNext = ScheduledMobKind(game);
        const EnemyInfo* info = GetEnemyInfoOrUnknown(kindNext);
        if (kindNext >= 0 && DirectoryCodeVisible(kindNext)) {
            int hp = info->hp + info->hpGrowth * floor;
            wsprintfW(out, L"적 체력 %d → %d", hp, hp * DIR_INFECTED_HP_PERCENT / 100);
        } else lstrcpynW(out, L"판독하면 정확한 체력이 표시됩니다", cap);
        break;
    }
    case DIR_NODE_CORRUPTED: {
        if (payload < 18) {
            const Face* face = &game->dice[payload / 6].faces[payload % 6];
            wchar_t value[24]; FormatFace(face, value);
            wsprintfW(out, L"격리 대상  주사위 %d · %d면 (%s)", payload / 6 + 1, payload % 6 + 1, value);
        } else lstrcpynW(out, L"격리할 면이 없어 그대로 교전합니다", cap);
        break;
    }
    default: {
        int kindNext = ScheduledMobKind(game);
        if (kindNext >= 0 && DirectoryCodeVisible(kindNext)) lstrcpynW(out, L"현행 수치 그대로 교전합니다", cap);
        else lstrcpynW(out, L"기록에 없는 프로세스입니다", cap);
        break;
    }
    }
}

static void DrawDirectoryCard(HDC dc, int index) {
    RECT r = DirectoryChoiceRect(index);
    const DirectoryChoice* choice = &gGame.directory.choices[index];
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(choice->kind);
    int hover = Inside(r, gMouse.x, gMouse.y);
    int armed = gDirectoryArmed == index;
    COLORREF accent = info ? (COLORREF)info->color : C_LINE;
    Panel(dc, r, armed ? MixColor(C_PANEL, accent, 26) : hover ? RGB(24, 37, 46) : C_PANEL,
        armed || hover ? accent : C_LINE);
    DrawCardMotion(dc, r, accent, index, armed || hover);
    if (armed) Outline(dc, MakeRect(r.left - 3, r.top - 3, r.right + 3, r.bottom + 3), accent, 2);
    if (!info) return;

    wchar_t b[192];
    wsprintfW(b, L"[%d]", index + 1);
    Text(dc, r.left + 16, r.top + 12, b, C_DIM, gFontSmall);
    wsprintfW(b, L"<%s>", info->segment);
    TextRect(dc, MakeRect(r.left + 16, r.top + 32, r.right - 16, r.top + 76), b, accent, gFontLarge, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    COLORREF riskColor = info->risk == DIR_RISK_LOW ? C_GREEN : info->risk == DIR_RISK_MEDIUM ? C_YELLOW : C_RED;
    wsprintfW(b, L"RISK %s · 위험 %s", DIRECTORY_RISK_NAMES[info->risk], DIRECTORY_RISK_LABELS[info->risk]);
    Text(dc, r.left + 16, r.top + 82, b, riskColor, gFontSmall);
    wsprintfW(b, L"분류 %s", DIRECTORY_CATEGORY_NAMES[info->category]);
    TextRect(dc, MakeRect(r.left + 200, r.top + 82, r.right - 16, r.top + 104), b, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    Fill(dc, MakeRect(r.left + 14, r.top + 110, r.right - 14, r.top + 111), C_LINE);

    Text(dc, r.left + 16, r.top + 120, L"효과", C_GREEN, gFontSmall);
    TextRect(dc, MakeRect(r.left + 16, r.top + 142, r.right - 16, r.top + 184), info->effect, C_TEXT, gFontSmall, DT_WORDBREAK);
    DirectoryDetailText(&gGame, choice->kind, choice->payload, b, 192);
    TextRect(dc, MakeRect(r.left + 16, r.top + 186, r.right - 16, r.top + 226), b, accent, gFontSmall, DT_WORDBREAK);

    Text(dc, r.left + 16, r.top + 232, L"비용", C_RED, gFontSmall);
    TextRect(dc, MakeRect(r.left + 16, r.top + 254, r.right - 16, r.top + 296), info->cost, C_DIM, gFontSmall, DT_WORDBREAK);
    Fill(dc, MakeRect(r.left + 14, r.top + 302, r.right - 14, r.top + 303), C_LINE);

    int next = ScheduledMobKind(&gGame);
    Text(dc, r.left + 16, r.top + 312, L"TARGET", C_BLUE, gFontSmall);
    if (next >= 0) {
        wchar_t code[32]; DirectoryCodeText(next, code, 32);
        COLORREF codeColor = DirectoryCodeVisible(next) ? (COLORREF)GetEnemyInfoOrUnknown(next)->color : C_DIM;
        TextRect(dc, MakeRect(r.left + 108, r.top + 312, r.right - 16, r.top + 334), code, codeColor, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        if (!DirectoryCodeVisible(next))
            TextRect(dc, MakeRect(r.left + 16, r.top + 336, r.right - 16, r.top + 358), L"UNREAD PROCESS · 처치하거나 LOGS로 열립니다", C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    }
    Text(dc, r.left + 16, r.top + 360, L"REWARD", C_BLUE, gFontSmall);
    wsprintfW(b, L"%s · 면 후보 %d개", info->rewardTier ? L"강화 TUNED" : L"표준 STANDARD", info->rewardChoices);
    TextRect(dc, MakeRect(r.left + 108, r.top + 360, r.right - 16, r.top + 382), b,
        info->rewardTier ? C_YELLOW : C_TEXT, gFontSmall, DT_RIGHT | DT_SINGLELINE);

    // 진입은 되돌릴 수 없다. 세워 둔 카드만 확정 문구를 달고, 나머지는 고르는
    // 동작이라는 것을 문구로 밝힌다.
    TextRect(dc, MakeRect(r.left + 12, r.bottom - 32, r.right - 12, r.bottom - 10),
        armed ? L"한 번 더 누르면 진입 · [취소]로 해제" : hover ? L"클릭하여 선택" : L"클릭 또는 숫자 키",
        armed ? accent : hover ? accent : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

static void DrawDirectorySelect(HDC dc, int width, int height) {
    int driveIndex = gGame.selectedDrive;
    DrawSceneField(dc, PHASE_DIRECTORY, driveIndex >= 0 && driveIndex < DRIVE_COUNT ? (COLORREF)DRIVE_INFO[driveIndex].color : C_GREEN, width, height);
    const DriveInfo* drive = &DRIVE_INFO[gGame.selectedDrive < 0 ? 0 : gGame.selectedDrive];
    wchar_t b[160];
    TextRect(dc, MakeRect(0, 78, width, 102), L"전투 대기  →  [현재: 하위 디렉터리 선택]  →  일반전  →  보상", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
    wsprintfW(b, L"다음 프로세스의 조건을 고르십시오  ·  이번 층 %d / %d 번째 선택", gGame.encounter + 1, DIRECTORY_PER_FLOOR);
    TextRect(dc, MakeRect(0, 98, width, 126), b, C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
    if (difficulty) {
        wsprintfW(b, L"VOLUME %s%s  ·  난이도 %s  ·  %s", drive->letter, drive->label, difficulty->name, difficulty->brief);
        TextRect(dc, MakeRect(0, 124, width, 146), b, (COLORREF)difficulty->color, gFontSmall, DT_CENTER | DT_SINGLELINE);
    }

    for (int i = 0; i < DirectoryChoiceCount(&gGame); ++i) DrawDirectoryCard(dc, i);

    wchar_t here[96];
    FormatCurrentDirectory(&gGame, here, 96);
    Text(dc, LEGACY_X + 120, 584, L"CURRENT", C_GREEN, gFontSmall);
    Text(dc, LEGACY_X + 120, 606, here, C_TEXT, gFontMedium);
    Text(dc, LEGACY_X + 120, 642, L"LOCKED DESTINATION", C_RED, gFontSmall);
    wchar_t destination[128];
    lstrcpynW(destination, drive->paths[gGame.floor > 2 ? 2 : gGame.floor], 128);
    int boss = FloorBossKind(&gGame);
    if (boss >= 0 && DirectoryCodeVisible(boss)) AppendPathSegment(destination, GetEnemyInfoOrUnknown(boss)->code);
    else AppendPathSegment(destination, L"<BOSS>");
    Text(dc, LEGACY_X + 120, 664, destination, C_DIM, gFontMedium);

    TextRect(dc, MakeRect(0, height - 52, width, height - 28),
        gDirectoryArmed >= 0
            ? L"고른 디렉터리를 한 번 더 누르면 진입합니다  ·  [취소]로 선택을 해제할 수 있습니다"
            : L"[1] / [2] 또는 디렉터리를 클릭해 고르십시오  ·  선택지는 다시 뽑히지 않습니다",
        gDirectoryArmed >= 0 ? C_YELLOW : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

static void DrawDirectorySelectionExit(HDC dc, int width, int height, int elapsed) {
    Fill(dc, MakeRect(0, 68, width, height), C_BG);
    DrawDirectorySelect(dc, width, height);
    int chosen = gDirEnterChoiceIndex;
    if (chosen < 0 || chosen >= DirectoryChoiceCount(&gGame)) return;
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(gDirEnterKind);
    if (!info) return;
    COLORREF accent = (COLORREF)info->color;
    for (int i = 0; i < DirectoryChoiceCount(&gGame); ++i)
        DrawSelectionCardExit(dc, DirectoryChoiceRect(i), i == chosen, elapsed, DIR_LOCK_MS,
                              accent, L"PATH LOCKED");

    RECT selected = DirectoryChoiceRect(chosen);
    POINT from = {(selected.left + selected.right) / 2, selected.bottom + 2};
    POINT to = {width / 2, height - 48};
    int route = EaseOutCubic(Track(elapsed, 90, DIR_LOCK_MS));
    DrawSignalPath(dc, from, to, height - 96, route, 4, accent, 13, 1);
    if (elapsed > DIR_LOCK_MS * 2 / 3)
        DrawBandGlitch(dc, selected, elapsed, FxScale(7), gDirEnterKind + 43, 11);
    int dissolve = Track(elapsed, DIR_LOCK_MS - 100, DIR_LOCK_MS);
    if (dissolve > 0)
        DrawScreenStatic(dc, MakeRect(0, 68, width, height), elapsed / NOISE_CHURN_MS,
                         820 * dissolve / 1000);
    Fill(dc, MakeRect(0, 102, width, 134), C_BG);
    TextRect(dc, MakeRect(0, 102, width, 134), L"ROUTE ACCEPTED  ·  RESOLVING DIRECTORY HANDLE",
             accent, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// ---- 사각형 보간 -----------------------------------------------------------
// 원래는 아래 플로피 삽입 연출 옆에 있었지만, 마운트 패널이 고른 카드에서 열려
// 나오게 되고 디렉터리 도착 패널도 같은 식으로 열리면서 그보다 먼저 필요해져
// 여기로 올렸다 (사본은 두지 않는다).
static RECT LerpRect(const RECT& a, const RECT& b, int p) {
    return MakeRect(Lerp(a.left, b.left, p), Lerp(a.top, b.top, p),
                    Lerp(a.right, b.right, p), Lerp(a.bottom, b.bottom, p));
}

// ---------------------------------------------------------------------------
// 디렉터리 진입 연출
//
// 예전에는 고른 카드가 잠기고 나면 패널 한 장이 열리고, 그 안에서 갈래 두 줄과
// 경로 한 줄이 타이핑됐다. 런에서 여섯 번 있는 "어디로 들어갈 것인가"의 대답이
// 글자 한 줄이었다는 뜻이다 - 진입한다고 적혀 있을 뿐 아무 데도 가지 않았다.
//
// 이제 판이 부모 디렉터리의 목록이 되고, 헤드가 고른 줄을 찾아 내려앉고, 그 줄이
// 문처럼 열리고, 시점이 그 문 안으로 파고든다. 지나치는 겹에 적히는 것은 새로
// 지어낸 글자가 아니라 지금 서 있는 자리다 - 볼륨 이름 · 상위 조각 · 고른 노드
// 순서로 스쳐 가고, 마지막 겹이 화면을 삼키는 자리가 곧 도착이다.
//
//   잠금 0.36s  고른 카드가 경로로 잠기고 탈락한 카드가 닫힌다
//   탐색 0.30s  판이 목록으로 갈리고 헤드가 고른 줄로 내려앉는다
//   개방 0.28s  걸쇠가 풀리고 그 줄이 문처럼 열린다. 위아래 줄이 밀려난다
//   통과 0.56s  문 → 겹 셋. 겹 하나에 0.14초씩 쓰고 배율이 같은 비율로 오른다
//   안착 0.62s  도착한 디렉터리의 이름·효과·비용·대상이 한 줄씩 선다
//   확정 0.24s  작업 디렉터리가 박히고 그 아래 전투판이 가운데부터 드러난다
//
// 시점에 DC 변환(마운트·삽입 연출의 카메라)을 쓰지 않는 이유는 하나다. 마지막
// 겹에서 배율이 서른 배까지 오르는데 GDI의 테두리와 펜은 논리 단위라, 그 순간
// 선 한 줄이 서른 px이 되고 글자는 화면보다 커진다. 굵기와 글꼴은 화면에 실리는
// 크기로 골라야 하므로 여기서는 세계 좌표를 직접 옮긴다 (DirView).
//
// 모든 값은 경과 ms의 순수 함수이고 구간 경계는 ui.h가 쥐고 있다 (소리도 같은
// 표를 본다). 판은 연출이 시작되기 전에 이미 전투 상태라 언제 건너뛰어도 같다.
// ---------------------------------------------------------------------------

#define DIR_TABLE_ROWS 7       // 목록에 서는 줄 수. 갈래 둘만 세우면 판이 아니라 버튼 두 개다
#define DIR_ROW_H      38
#define DIR_DOOR_H    176      // 다 열린 줄(문)의 높이
#define DIR_WALL_STEP 2600     // 겹마다 좁아지는 비율 (천분율)
#define DIR_WALL_FLAT  460     // 가장 안쪽 겹의 세로/가로 비. 납작한 슬롯이 통로로 펴진다

// 배율이 붙으면 사각형이 캔버스의 수십 배로 커진다. 그리기 전에 물려 둔다 -
// GDI가 알아서 잘라 주기는 하지만, 잘릴 것이 뻔한 자리를 계속 칠할 이유가 없다.
static RECT DirClamp(const RECT& r) {
    return MakeRect(r.left < -320 ? -320 : r.left, r.top < -320 ? -320 : r.top,
                    r.right > BASE_WIDTH + 320 ? BASE_WIDTH + 320 : r.right,
                    r.bottom > BASE_HEIGHT + 320 ? BASE_HEIGHT + 320 : r.bottom);
}

// ---- 경로 조각 -------------------------------------------------------------
// 통과 구간의 겹마다 이름이 하나씩 적힌다. 그 이름은 지금 경로에서 온다.
#define DIR_SEGMENT_MAX 8
#define DIR_SEGMENT_CAP 24
struct DirPath { wchar_t part[DIR_SEGMENT_MAX][DIR_SEGMENT_CAP]; int count; };

static DirPath DirSplitPath(const wchar_t* path) {
    DirPath out;
    out.count = 0;
    out.part[0][0] = 0;
    int length = 0;
    for (const wchar_t* p = path; ; ++p) {
        if (*p && *p != L'\\') {
            if (length < DIR_SEGMENT_CAP - 1) out.part[out.count][length++] = *p;
            continue;
        }
        out.part[out.count][length] = 0;
        // "C:\"가 남기는 빈 조각은 세지 않는다. 겹 하나가 비면 통과가 한 번 헛돈다.
        if (length > 0 && out.count < DIR_SEGMENT_MAX - 1) {
            ++out.count;
            out.part[out.count][0] = 0;
        }
        length = 0;
        if (!*p) break;
    }
    return out;
}

// ---- 시점 -----------------------------------------------------------------
// 세계 좌표(선택 화면과 같은 캔버스 좌표)를 화면으로 옮긴다.
struct DirView { int scale, cx, cy; };
static int DirViewX(const DirView& v, int x) { return BASE_WIDTH / 2 + (x - v.cx) * v.scale / 1000; }
static int DirViewY(const DirView& v, int y) { return BASE_HEIGHT / 2 + (y - v.cy) * v.scale / 1000; }
static RECT DirViewRect(const DirView& v, const RECT& r) {
    return MakeRect(DirViewX(v, r.left), DirViewY(v, r.top), DirViewX(v, r.right), DirViewY(v, r.bottom));
}

// ---- 목록 판의 자리 --------------------------------------------------------
// 선택 화면의 카드 두 장이 차지하던 폭을 그대로 쓴다. 카드가 닫힌 자리에서
// 목록이 서야 판이 바뀐 것이 아니라 같은 판이 갈린 것으로 읽힌다.
static RECT DirTableRect() {
    int top = 150, height = 46 + DIR_TABLE_ROWS * DIR_ROW_H + 16;
    return MakeRect(LEGACY_X + 120, top, LEGACY_X + 1000, top + height);
}

// 고른 갈래가 앉는 줄. 목록 가운데에 흩어 두어야 판이 목록으로 읽힌다.
static int DirChoiceRow(int index) {
    int row = 1 + index * 3;
    return row >= DIR_TABLE_ROWS ? DIR_TABLE_ROWS - 1 : row;
}

// 열림 정도(0~1000)에 따라 벌어진 줄. 고른 줄은 문이 되고 나머지는 위아래로 밀린다.
static RECT DirRowRect(int row, int chosenRow, int open) {
    RECT table = DirTableRect();
    int grow = (DIR_DOOR_H - DIR_ROW_H) * open / 1000;
    int top = table.top + 46 + row * DIR_ROW_H, height = DIR_ROW_H;
    if (row < chosenRow) top -= grow / 2 + (chosenRow - row) * grow / 10;
    else if (row > chosenRow) top += grow / 2 + (row - chosenRow) * grow / 10;
    else { top -= grow / 2; height += grow; }
    return MakeRect(table.left + 14, top, table.right - 14, top + height);
}

// ---- 통과 구간의 겹 --------------------------------------------------------
// 겹 i의 세계 반폭. 문 안쪽으로 일정 비율씩 좁아진다.
static int DirWallHalf(int doorHalf, int index) {
    int half = doorHalf < 3 ? 3 : doorHalf;
    for (int i = 0; i <= index; ++i) half = half * 1000 / DIR_WALL_STEP;
    return half < 3 ? 3 : half;
}

// 세로는 덜 좁아진다. 문은 납작한 슬롯인데 같은 비율로 줄이면 안쪽 겹이 실보다
// 가늘어진다 - 안으로 갈수록 네모꼴로 펴지면서 슬롯이 통로가 된다.
static int DirWallHalfH(int doorHalfW, int doorHalfH, int index) {
    if (doorHalfW < 1) return 3;
    int flat = 1000 * doorHalfH / doorHalfW;
    flat += (DIR_WALL_FLAT - flat) * (index + 1) / DIR_TUNNEL_DEPTH;
    int half = DirWallHalf(doorHalfW, index) * flat / 1000;
    return half < 3 ? 3 : half;
}

// 그 겹이 화면을 가득 채우는 배율. index < 0이면 문 자신이다. 통과 구간의 배율이
// 전부 이 함수에서 나오므로, 겹의 비율을 바꿔도 마지막 겹은 언제나 도착에 맞는다.
static int DirFillScale(int doorHalf, int index) {
    int half = index < 0 ? (doorHalf < 3 ? 3 : doorHalf) : DirWallHalf(doorHalf, index);
    return (BASE_WIDTH / 2 + 48) * 1000 / half;
}

static DirView DirViewAt(int t, const RECT& door) {
    DirView view = {1000, BASE_WIDTH / 2, BASE_HEIGHT / 2};
    if (t < DIR_DIVE_AT) return view;      // 탐색과 개방은 제자리에서 본다
    int doorHalf = (door.right - door.left) / 2;
    view.cx = (door.left + door.right) / 2;
    view.cy = (door.top + door.bottom) / 2;
    if (t >= DIR_LAND_AT) { view.scale = DirFillScale(doorHalf, DIR_TUNNEL_DEPTH - 1); return view; }
    // 문과 겹 셋에 같은 시간을 쓰고, 구간마다 배율이 같은 비율로 오른다. 로그로
    // 등속이라 속도가 일정하게 느껴지고, 겹을 지나치는 순간이 소리의 자리와 맞는다.
    int p = Track(t, DIR_DIVE_AT, DIR_LAND_AT) * (DIR_TUNNEL_DEPTH + 1);
    int step = p / 1000;
    if (step > DIR_TUNNEL_DEPTH) step = DIR_TUNNEL_DEPTH;
    int from = step == 0 ? 1000 : DirFillScale(doorHalf, step - 2);
    view.scale = Lerp(from, DirFillScale(doorHalf, step - 1), p - step * 1000);
    return view;
}

// ---- 목록 ------------------------------------------------------------------
// 아직 판독하지 않은 항목. 이름이 헥스로 갈려 있고 칸마다 다른 주기로 튄다.
static void DirNoiseEntry(int seed, wchar_t* out) {
    static const wchar_t* const EXT[4] = {L"SYS", L"TMP", L"DAT", L"BIN"};
    static const wchar_t* const HEX = L"0123456789ABCDEF";
    wchar_t name[9];
    for (int i = 0; i < 8; ++i) name[i] = HEX[Hash3(seed, i, 0x5A17) % 16u];
    name[8] = 0;
    wsprintfW(out, L"%s.%s", name, EXT[Hash3(seed, 3, 0x2B09) % 4u]);
}

// 부모 디렉터리의 목록. 고른 갈래 둘은 이름·위험도가 그대로 적히고 나머지 자리는
// 판독하지 않은 항목이라 갈려 있다. 위로 가는 `..`은 잠겨 있다 - 되돌아갈 수
// 없다는 규칙이 안내 문구가 아니라 목록에 적혀 있는 편이 낫다.
static void DirDrawTable(HDC dc, const DirView& view, int t, int chosenRow, int open,
                         COLORREF accent, uint32_t tick) {
    RECT table = DirTableRect();
    // 판의 위아래는 실제로 밀려난 줄에서 뽑는다. 여는 만큼만 늘리면 멀리 밀린
    // 줄이 판 밖으로 흘러나온다 - 목록이 판을 뚫고 나온 것으로 보인다.
    table.top = DirRowRect(0, chosenRow, open).top - 46;
    table.bottom = DirRowRect(DIR_TABLE_ROWS - 1, chosenRow, open).bottom + 16;
    RECT shown = DirClamp(DirViewRect(view, table));
    if (shown.right - shown.left < 8 || shown.bottom - shown.top < 8) return;
    Panel(dc, shown, RGB(10, 15, 21), MixColor(C_LINE, accent, 30));
    int choiceCount = DirectoryChoiceCount(&gGame);
    int plain = view.scale <= 1000;   // 글자는 제자리에서 볼 때만. 배율이 붙으면 줄은 띠가 된다
    if (plain) {
        wchar_t head[96];
        Text(dc, table.left + 16, table.top + 12, L"DIRECTORY", C_GREEN, gFontSmall);
        // 읽히지 않는 항목 수는 세어서 적는다. 갈래가 둘이 아닌 판에서도 머리글과
        // 실제 줄이 어긋나지 않는다 (`..` 한 줄과 갈래를 뺀 나머지가 미판독이다).
        wsprintfW(head, L"%d ENTRIES  ·  %d UNREAD", DIR_TABLE_ROWS, DIR_TABLE_ROWS - 1 - choiceCount);
        TextRect(dc, MakeRect(table.left + 16, table.top + 12, table.right - 16, table.top + 34),
                 head, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        Fill(dc, MakeRect(table.left + 14, table.top + 40, table.right - 14, table.top + 41), C_LINE);
    }

    int doorOpen = t >= DIR_OPEN_AT;
    for (int row = 0; row < DIR_TABLE_ROWS; ++row) {
        if (row == chosenRow && doorOpen) continue;    // 열린 뒤로는 문이 대신 선다
        RECT world = DirRowRect(row, chosenRow, open);
        RECT r = DirClamp(DirViewRect(view, world));
        if (r.bottom <= 68 || r.top >= BASE_HEIGHT || r.right - r.left < 4) continue;
        int appear = EaseOutCubic(Track(t, DIR_SEEK_AT + row * 22, DIR_SEEK_AT + row * 22 + 150));
        if (appear <= 0) continue;
        RECT bar = r;
        bar.right = Lerp(r.left, r.right, appear);
        int chosen = row == chosenRow;
        Fill(dc, bar, chosen ? MixColor(RGB(13, 19, 26), accent, 14) : RGB(13, 19, 26));
        Fill(dc, MakeRect(bar.left, bar.bottom - 1, bar.right, bar.bottom), RGB(20, 29, 38));
        if (chosen && t > DIR_SEEK_AT + 100 + DIR_SEEK_TRAVEL_MS) Outline(dc, bar, accent, 2);
        if (!plain) continue;
        int saved = SaveDC(dc);
        if (!saved) continue;
        IntersectClipRect(dc, bar.left, bar.top, bar.right, bar.bottom);
        int choice = -1;
        for (int i = 0; i < choiceCount && i < DIRECTORY_CHOICE_COUNT; ++i)
            if (DirChoiceRow(i) == row) choice = i;
        wchar_t cell[128];
        if (row == 0 && choice < 0) {
            Text(dc, r.left + 16, r.top + 9, L"..", C_DIM, gFontSmall);
            Text(dc, r.left + 300, r.top + 9, L"<UP>", C_DIM, gFontSmall);
            TextRect(dc, MakeRect(r.left + 420, r.top + 9, r.right - 16, r.bottom),
                     L"LOCKED · 되돌아갈 수 없습니다", RGB(122, 74, 74), gFontSmall, DT_LEFT | DT_SINGLELINE);
        } else if (choice >= 0) {
            const DirectoryNodeInfo* branch = DirectoryNodeInfoOrNull(gGame.directory.choices[choice].kind);
            COLORREF tone = branch ? (COLORREF)branch->color : C_DIM;
            wsprintfW(cell, L"<%s>", branch ? branch->segment : L"?");
            Text(dc, r.left + 16, r.top + 9, cell, chosen ? tone : MixColor(C_DIM, tone, 45), gFontSmall);
            Text(dc, r.left + 300, r.top + 9, L"<DIR>", chosen ? C_TEXT : C_DIM, gFontSmall);
            if (branch) {
                wsprintfW(cell, L"RISK %s", DIRECTORY_RISK_NAMES[branch->risk]);
                Text(dc, r.left + 420, r.top + 9, cell, chosen ? tone : MixColor(C_DIM, tone, 35), gFontSmall);
            }
            // 헤드가 앉고 나면 ENTER가 깜빡인다. 이 구간은 읽으라고 세워 둔 시간이라
            // 화면이 통째로 멎는데, 깜빡이는 자리가 하나는 있어야 판이 살아 있다.
            int blink = !chosen || t < DIR_SEEK_AT + 100 + DIR_SEEK_TRAVEL_MS || ((t / 240) & 1);
            TextRect(dc, MakeRect(r.left + 560, r.top + 9, r.right - 16, r.bottom),
                     chosen ? L"ENTER" : L"NOT TAKEN",
                     chosen ? (blink ? accent : MixColor(C_BG, accent, 38)) : C_DIM, gFontSmall,
                     DT_RIGHT | DT_SINGLELINE);
        } else {
            DirNoiseEntry(row * 37 + gGame.floor * 11 + gGame.encounter, cell);
            DrawGlitchLine(dc, r.left + 16, r.top + 9, cell, RGB(46, 58, 66), MixColor(C_BG, accent, 40),
                           gFontSmall, row * 13 + 5, tick);
            Text(dc, r.left + 300, r.top + 9, L"<DIR>", RGB(46, 58, 66), gFontSmall);
            wsprintfW(cell, L"%5u B", (unsigned)(Hash3(row, 9, 0x33) % 4096u + 512u));
            TextRect(dc, MakeRect(r.left + 560, r.top + 9, r.right - 16, r.bottom),
                     cell, RGB(46, 58, 66), gFontSmall, DT_RIGHT | DT_SINGLELINE);
        }
        RestoreDC(dc, saved);
    }
}

// ---- 헤드 -----------------------------------------------------------------
// 목록을 훑고 고른 줄에 내려앉는다. 어느 줄이 열릴 것인지를 문이 열리기 전에
// 손가락으로 짚어 주는 일이라, 개방이 시작되면 곧바로 사라진다.
static void DirDrawHead(HDC dc, const DirView& view, int t, int chosenRow, COLORREF accent) {
    if (t >= DIR_OPEN_AT) return;
    RECT first = DirRowRect(0, chosenRow, 0), target = DirRowRect(chosenRow, chosenRow, 0);
    // 목록이 다 찍힌 뒤에 움직이기 시작해서 DIR_SEEK_TRAVEL_MS 안에 앉는다.
    // 탐색 구간의 남은 시간은 통째로 고른 줄을 읽는 시간이다.
    int seek = EaseOutBack(Track(t, DIR_SEEK_AT + 160, DIR_SEEK_AT + 160 + DIR_SEEK_TRAVEL_MS));
    // 줄의 윗변에 앉는다. 가운데로 지나가면 헤드가 이름을 그어 버려서, 무엇을
    // 짚었는지 읽으라고 세워 둔 줄을 정작 헤드가 가린다.
    int y = Lerp(first.top - 1, target.top - 1, seek);
    RECT table = DirTableRect();
    RECT band = DirClamp(DirViewRect(view, MakeRect(table.left + 6, y - 2, table.right - 6, y + 2)));
    Fill(dc, band, MixColor(C_BG, accent, 70));
    for (int i = 0; i < 4; ++i) {
        int x = band.left - 14 - i * 9, x2 = band.right + 14 + i * 9;
        COLORREF tone = MixColor(C_BG, accent, 60 - i * 12);
        Fill(dc, MakeRect(x - 4, band.top - 3 + i, x, band.bottom + 3 - i), tone);
        Fill(dc, MakeRect(x2, band.top - 3 + i, x2 + 4, band.bottom + 3 - i), tone);
    }
}

// ---- 문 -------------------------------------------------------------------
static void DirDrawDoor(HDC dc, const DirView& view, int t, int chosenRow, int open,
                        COLORREF accent, const wchar_t* segment) {
    RECT world = DirRowRect(chosenRow, chosenRow, open);
    RECT frame = DirClamp(DirViewRect(view, world));
    if (frame.right - frame.left < 4 || frame.bottom - frame.top < 4) return;
    Fill(dc, frame, RGB(4, 6, 9));          // 문 안쪽. 통과 구간의 겹이 여기에 선다
    int lip = view.scale > 3000 ? 6 : view.scale > 1400 ? 4 : 2;
    Fill(dc, MakeRect(frame.left, frame.top, frame.right, frame.top + lip), MixColor(C_BG, accent, 80));
    Fill(dc, MakeRect(frame.left, frame.bottom - lip, frame.right, frame.bottom), MixColor(C_BG, accent, 80));
    if (view.scale > 1000) return;
    // 걸쇠. 문짝이 벌어지기 시작하면 위아래로 빠진다
    int latch = EaseOutCubic(Track(t, DIR_OPEN_AT, DIR_OPEN_AT + 130));
    int cy = (frame.top + frame.bottom) / 2;
    for (int i = 0; i < 2; ++i) {
        int x = i ? frame.right - 26 : frame.left + 10, dy = 13 * latch / 1000;
        COLORREF tone = MixColor(C_LINE, accent, 60 * (1000 - latch) / 1000);
        Fill(dc, MakeRect(x, cy - 9 - dy, x + 16, cy - 2 - dy), tone);
        Fill(dc, MakeRect(x, cy + 2 + dy, x + 16, cy + 9 + dy), tone);
    }
    // 문짝이 절반쯤 벌어질 때까지 줄에 적혀 있던 이름이 남는다. 열리자마자 지우면
    // 방금 고른 줄이 무엇이었는지가 한 프레임 만에 사라진다.
    if (open < 850) {
        wchar_t label[64];
        wsprintfW(label, L"<%s>", segment);
        int fade = 100 - 76 * open / 850;
        RECT line = MakeRect(frame.left + 16, frame.top, frame.right - 16, frame.top + DIR_ROW_H);
        TextRect(dc, line, label, MixColor(C_BG, accent, fade), gFontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, line, L"OPEN", MixColor(C_BG, accent, fade * 7 / 10), gFontSmall,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

// ---- 통과 -----------------------------------------------------------------
// 문 안쪽에 겹 셋이 서고 벽선이 그 사이를 잇는다. 겹마다 지금 서 있는 자리의
// 이름이 하나씩 적혀 있어, 통과가 곧 경로를 읽는 일이 된다.
static void DirDrawTunnel(HDC dc, const DirView& view, int t, int chosenRow, int open,
                          const wchar_t* const* wall, COLORREF accent) {
    RECT world = DirRowRect(chosenRow, chosenRow, open);
    RECT mouth = DirClamp(DirViewRect(view, world));
    if (mouth.right - mouth.left < 8 || mouth.bottom - mouth.top < 8) return;
    int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, mouth.left, mouth.top < 68 ? 68 : mouth.top, mouth.right, mouth.bottom);
    int doorHalfW = (world.right - world.left) / 2, doorHalfH = (world.bottom - world.top) / 2;
    int cx = DirViewX(view, (world.left + world.right) / 2);
    int cy = DirViewY(view, (world.top + world.bottom) / 2);
    // 소실점의 빛. 겹이 다 지나간 뒤에도 안쪽이 비어 보이지 않게 한다
    int glow = 8 + 26 * Track(t, DIR_OPEN_AT, DIR_LAND_AT) / 1000;
    int glowW = DirWallHalf(doorHalfW, DIR_TUNNEL_DEPTH - 1) * view.scale / 1000;
    int glowH = DirWallHalfH(doorHalfW, doorHalfH, DIR_TUNNEL_DEPTH - 1) * view.scale / 1000;
    for (int i = 3; i >= 0; --i)
        Fill(dc, DirClamp(MakeRect(cx - glowW * (4 - i) / 5, cy - glowH * (4 - i) / 5,
                                   cx + glowW * (4 - i) / 5, cy + glowH * (4 - i) / 5)),
             MixColor(RGB(4, 6, 9), accent, glow * (4 - i) / 8));
    RECT outer = MakeRect(cx - doorHalfW * view.scale / 1000, cy - doorHalfH * view.scale / 1000,
                          cx + doorHalfW * view.scale / 1000, cy + doorHalfH * view.scale / 1000);
    for (int i = 0; i < DIR_TUNNEL_DEPTH; ++i) {
        int halfW = DirWallHalf(doorHalfW, i) * view.scale / 1000;
        int halfH = DirWallHalfH(doorHalfW, doorHalfH, i) * view.scale / 1000;
        RECT frame = MakeRect(cx - halfW, cy - halfH, cx + halfW, cy + halfH);
        COLORREF wallTone = MixColor(RGB(4, 6, 9), accent, 12 + 16 * (DIR_TUNNEL_DEPTH - i) / DIR_TUNNEL_DEPTH);
        DrawLine(dc, outer.left, outer.top, frame.left, frame.top, wallTone, 1);
        DrawLine(dc, outer.right, outer.top, frame.right, frame.top, wallTone, 1);
        DrawLine(dc, outer.left, outer.bottom, frame.left, frame.bottom, wallTone, 1);
        DrawLine(dc, outer.right, outer.bottom, frame.right, frame.bottom, wallTone, 1);
        // 벽의 이음매. 통로가 지나가는 속도는 이 눈금이 말한다
        for (int k = 1; k < 4; ++k) {
            RECT seam = LerpRect(outer, frame, k * 1000 / 4);
            COLORREF seamTone = MixColor(RGB(4, 6, 9), accent, 10 + 8 * k);
            Fill(dc, DirClamp(MakeRect(seam.left, seam.top, seam.left + 2, seam.bottom)), seamTone);
            Fill(dc, DirClamp(MakeRect(seam.right - 2, seam.top, seam.right, seam.bottom)), seamTone);
        }
        if (halfW < BASE_WIDTH * 2) {
            int thick = halfW > 620 ? 4 : halfW > 240 ? 3 : halfW > 90 ? 2 : 1;
            Outline(dc, DirClamp(frame), MixColor(RGB(4, 6, 9), accent, 34 + i * 12), thick);
        }
        // 이름. 겹이 읽을 만한 크기일 때만, 그 크기에 맞는 글꼴로 선다
        if (halfW >= 86 && halfW < BASE_WIDTH) {
            HFONT font = halfW < 200 ? gFontSmall : halfW < 420 ? gFontMedium
                       : halfW < 900 ? gFontLarge : gFontHuge;
            int line = halfW < 200 ? 22 : halfW < 420 ? 30 : halfW < 900 ? 44 : 74;
            int fade = halfW > 900 ? 100 - 60 * (halfW - 900) / (BASE_WIDTH - 900) : 100;
            TextRect(dc, MakeRect(frame.left, frame.top + line / 3, frame.right, frame.top + line / 3 + line),
                     wall[i], MixColor(RGB(4, 6, 9), accent, fade), font, DT_CENTER | DT_SINGLELINE);
        }
        outer = frame;
    }
    // 지나쳐 흐르는 데이터. 통과하는 동안에만 나오고 연출 강도를 따른다
    if (FxDecorOn() && t >= DIR_DIVE_AT) {
        int count = FxScale(16), run = Track(t, DIR_DIVE_AT, DIR_LAND_AT);
        for (int i = 0; i < count; ++i) {
            uint32_t h = Hash3(i, 7, 0x5109);
            int angle = (int)(h % 3600u), p = (run * 3 + (int)((h >> 12) % 1000u)) % 1000;
            int r0 = 30 + 1180 * p / 1000, r1 = r0 + 26 + 150 * p / 1000;
            COLORREF tone = MixColor(RGB(4, 6, 9), accent, 18 + 46 * (1000 - p) / 1000);
            DrawLine(dc, cx + CosMille(angle) * r0 / 1000, cy + SinMille(angle) * r0 / 1000,
                     cx + CosMille(angle) * r1 / 1000, cy + SinMille(angle) * r1 / 1000, tone, p > 600 ? 2 : 1);
        }
    }
    RestoreDC(dc, saved);
}

// ---- 안착 -----------------------------------------------------------------
// 도착한 디렉터리. 카드에 적혀 있던 것이 이번에는 작업 디렉터리의 내용으로 선다.
static void DirDrawArrival(HDC dc, int t, const DirectoryNodeInfo* info, const wchar_t* path,
                           COLORREF accent, uint32_t tick) {
    int since = t - DIR_LAND_AT;
    RECT panel = MakeRect(LEGACY_X + 120, 150, LEGACY_X + 1000, 578);
    int mid = (panel.top + panel.bottom) / 2;
    RECT slit = MakeRect(panel.left + 90, mid - 4, panel.right - 90, mid + 4);
    RECT shown = LerpRect(slit, panel, EaseOutCubic(Track(since, 0, 200)));
    Panel(dc, shown, RGB(11, 16, 22), accent);
    int saved = SaveDC(dc);
    if (!saved) return;
    IntersectClipRect(dc, shown.left + 1, shown.top + 1, shown.right - 1, shown.bottom - 1);

    wchar_t b[192];
    wsprintfW(b, L"<%s>", info->segment);
    Text(dc, panel.left + 28, panel.top + 18, b, accent, gFontLarge);
    COLORREF riskColor = info->risk == DIR_RISK_LOW ? C_GREEN : info->risk == DIR_RISK_MEDIUM ? C_YELLOW : C_RED;
    wsprintfW(b, L"RISK %s · 위험 %s · 분류 %s", DIRECTORY_RISK_NAMES[info->risk],
              DIRECTORY_RISK_LABELS[info->risk], DIRECTORY_CATEGORY_NAMES[info->category]);
    TextRect(dc, MakeRect(panel.left + 300, panel.top + 30, panel.right - 28, panel.top + 56),
             b, riskColor, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    Fill(dc, MakeRect(panel.left + 26, panel.top + 74, panel.right - 26, panel.top + 75), C_LINE);

    // 작업 디렉터리. 확정 구간에 도장이 박히는 자리도 이 줄이다
    wchar_t prompt[128] = L"> ";
    lstrcpynW(prompt + 2, path, 120);
    if (t < DIR_SEAL_AT && ((since / 240) & 1)) lstrcatW(prompt, L"_");
    TextRect(dc, MakeRect(panel.left + 28, panel.top + 86, panel.right - 28, panel.top + 126),
             prompt, accent, gFontMedium, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 세 줄. 하나씩 밀려들어와 앉고, 앉는 순간 앞의 칸이 채워진다
    static const wchar_t* const LABEL[3] = {L"효과", L"비용", L"대상"};
    wchar_t target[128];
    int next = ScheduledMobKind(&gGame);
    if (next >= 0) {
        wchar_t code[32]; DirectoryCodeText(next, code, 32);
        wsprintfW(target, L"%s  ·  보상 %s · 면 후보 %d개", code,
                  info->rewardTier ? L"강화 TUNED" : L"표준 STANDARD", info->rewardChoices);
    } else lstrcpynW(target, L"이번 구역의 프로세스", 128);
    const wchar_t* value[3] = {info->effect, info->cost, target};
    COLORREF valueColor[3] = {C_TEXT, C_DIM, C_BLUE};
    for (int i = 0; i < 3; ++i) {
        int at = 90 + i * 75, land = EaseOutCubic(Track(since, at, at + 150));
        if (land <= 0) continue;
        int y = panel.top + 140 + i * 66, slide = 22 * (1000 - land) / 1000;
        RECT mark = MakeRect(panel.left + 30, y + 6, panel.left + 44, y + 20);
        Fill(dc, mark, MixColor(RGB(11, 16, 22), accent, 30 + 70 * land / 1000));
        Outline(dc, MakeRect(mark.left - 3, mark.top - 3, mark.right + 3, mark.bottom + 3),
                MixColor(C_LINE, accent, 60), 1);
        Text(dc, panel.left + 60 + slide, y + 4, LABEL[i],
             MixColor(RGB(11, 16, 22), i == 1 ? C_RED : C_GREEN, 40 + 60 * land / 1000), gFontSmall);
        TextRect(dc, MakeRect(panel.left + 150 + slide, y, panel.right - 28, y + 56), value[i],
                 MixColor(RGB(11, 16, 22), valueColor[i], 30 + 70 * land / 1000), gFontSmall, DT_WORDBREAK);
        if (land < 1000 && FxDecorOn())
            DrawPulseFrame(dc, mark, FxScale(2 + 10 * (1000 - land) / 1000), 2, MixColor(C_BG, accent, 60));
    }

    // 판독 띠. 도착 직후에는 아직 갈려 있다가 전투 직전에 확정으로 선다
    RECT band = MakeRect(panel.left + 28, panel.bottom - 96, panel.right - 28, panel.bottom - 56);
    Panel(dc, band, RGB(8, 13, 19), C_LINE);
    RECT inner = MakeRect(band.left + 2, band.top + 2, band.right - 2, band.bottom - 2);
    int settle = Track(since, 140, 420);
    DrawSectorStatic(dc, inner, gDirEnterKind + 3, (int)(tick / NOISE_CHURN_MS), 820 * (1000 - settle) / 1000);
    DrawScanlines(dc, inner);
    if (settle > 640)
        TextRect(dc, inner, L"HANDLE RESOLVED  ·  다음 프로세스와 교전합니다",
                 MixColor(RGB(8, 13, 19), accent, 100 * (settle - 640) / 360), gFontSmall,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(panel.left + 28, panel.bottom - 44, panel.right - 28, panel.bottom - 20),
             L"잠시 후 전투가 시작됩니다 · 클릭이나 키로 바로 넘기기", C_DIM, gFontSmall,
             DT_CENTER | DT_SINGLELINE);

    // 확정. 작업 디렉터리가 박히고 테두리가 한 번 울린다
    if (t >= DIR_SEAL_AT) {
        int seal = Track(t, DIR_SEAL_AT, DIR_SEAL_AT + 150);
        RECT stamp = MakeRect(panel.right - 176, panel.top + 86, panel.right - 28, panel.top + 120);
        Fill(dc, stamp, RGB(6, 10, 15));
        Outline(dc, stamp, accent, 2);
        TextRect(dc, stamp, L"CWD SET", accent, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (seal < 1000)
            DrawPulseFrame(dc, panel, FxScale(3 + 16 * (1000 - seal) / 1000), 3, MixColor(C_BG, accent, 70));
    }
    RestoreDC(dc, saved);
}

static void DrawDirectoryEnter(HDC dc, int width, int height) {
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(gDirEnterKind);
    if (!info) return;
    uint32_t tick = GetTickCount();
    int t = (int)(tick - gDirEnterStart);
    if (t < 0) t = 0;
    if (t > DIR_ENTER_MS) t = DIR_ENTER_MS;
    if (t < DIR_LOCK_MS) { DrawDirectorySelectionExit(dc, width, height, t); return; }

    COLORREF accent = (COLORREF)info->color;
    RECT stage = MakeRect(0, 68, width, height);
    int chosenRow = DirChoiceRow(gDirEnterChoiceIndex < 0 ? 0 : gDirEnterChoiceIndex);
    int open = EaseOutCubic(Track(t, DIR_OPEN_AT, DIR_DIVE_AT));
    DirView view = DirViewAt(t, DirRowRect(chosenRow, chosenRow, open));

    wchar_t path[96];
    FormatCurrentDirectory(&gGame, path, 96);

    if (t < DIR_LAND_AT) {
        DirPath split = DirSplitPath(path);
        const DriveInfo* drive = &DRIVE_INFO[gGame.selectedDrive < 0 ? 0 : gGame.selectedDrive];
        // 겹에 적히는 이름. 밖에서 안으로 볼륨 · 상위 조각 · 고른 노드다. 경로가
        // 짧은 1층에서도 겹 수가 흔들리지 않도록 가장 바깥은 볼륨 이름이 맡는다.
        const wchar_t* wall[DIR_TUNNEL_DEPTH];
        wall[0] = drive->label;
        wall[1] = split.count >= 2 ? split.part[split.count - 2] : drive->letter;
        wall[2] = split.count >= 1 ? split.part[split.count - 1] : info->segment;

        // 배율이 붙으면 목록도 문도 캔버스보다 커진다. 판 위쪽의 머리글(체력·용량)은
        // 어느 연출에서도 덮이지 않아야 하므로 무대에 물려 두고 그린다.
        int stageSaved = SaveDC(dc);
        if (!stageSaved) return;
        IntersectClipRect(dc, stage.left, stage.top, stage.right, stage.bottom);
        Fill(dc, stage, RGB(6, 9, 13));
        DirDrawTable(dc, view, t, chosenRow, open, accent, tick);
        if (t >= DIR_OPEN_AT) {
            DirDrawDoor(dc, view, t, chosenRow, open, accent, info->segment);
            DirDrawTunnel(dc, view, t, chosenRow, open, wall, accent);
        }
        DirDrawHead(dc, view, t, chosenRow, accent);
        // 잠금 구간의 노이즈가 남아 있다가 목록이 서면서 걷힌다
        int tail = 1000 - Track(t, DIR_SEEK_AT, DIR_SEEK_AT + 160);
        if (tail > 0) DrawScreenStatic(dc, stage, (int)(tick / NOISE_CHURN_MS), 820 * tail / 1000);
        if (t < DIR_OPEN_AT) {
            wchar_t parent[96];
            lstrcpynW(parent, path, 96);
            for (int i = lstrlenW(parent) - 1; i > 0; --i)
                if (parent[i] == L'\\') { parent[i] = 0; break; }
            RECT table = DirTableRect();
            Text(dc, table.left, table.top - 34, L"상위 목록 판독", C_GREEN, gFontSmall);
            TextRect(dc, MakeRect(table.left + 170, table.top - 36, table.right, table.top - 12),
                     parent, C_DIM, gFontSmall, DT_LEFT | DT_SINGLELINE);
        }
        RestoreDC(dc, stageSaved);
        return;
    }

    // 마지막 겹이 화면을 삼킨 자리에서 도착 판이 열린다. 확정 구간에는 그 판이
    // 가운데부터 걷히고, 아래에 이미 그려져 있는 전투판이 그대로 드러난다.
    int wipe = t >= DIR_SEAL_AT ? EaseOutCubic(Track(t, DIR_SEAL_AT, DIR_ENTER_MS)) : 0;
    int mid = (stage.top + stage.bottom) / 2, gap = (stage.bottom - stage.top) * wipe / 2000;
    int saved = SaveDC(dc);
    if (!saved) return;
    if (gap > 0) ExcludeClipRect(dc, stage.left, mid - gap, stage.right, mid + gap);
    // 마지막 겹이 화면을 삼킨 자리의 섬광. 덮개가 아니라 바탕색 자체를 밝게
    // 깔았다가 제곱으로 식힌다 - 알파가 없는 덮개로 옅게 깔면 그 순간 화면이
    // 가로줄 무늬가 되고, 도착 판까지 같이 갈린다.
    int flash = 1000 - Track(t, DIR_LAND_AT, DIR_LAND_AT + 150);
    Fill(dc, stage, flash > 0 ? MixColor(RGB(6, 9, 13), MixColor(accent, C_TEXT, 55), flash * flash / 10000)
                              : RGB(6, 9, 13));
    DirDrawArrival(dc, t, info, path, accent, tick);
    RestoreDC(dc, saved);
    if (gap > 0 && wipe < 1000) {
        Fill(dc, MakeRect(stage.left, mid - gap - 2, stage.right, mid - gap + 1), MixColor(C_BG, accent, 78));
        Fill(dc, MakeRect(stage.left, mid + gap - 1, stage.right, mid + gap + 2), MixColor(C_BG, accent, 78));
    }
}

// ---- 볼륨 마운트 / 층 하강 연출 --------------------------------------------
// 예전 이 자리에는 패널 한 장이 열리고 진행 막대가 찼다. 막대는 "얼마나 남았나"만
// 말한다 - 어느 볼륨으로 들어가는지, 그 볼륨이 왜 위험한지는 말하지 못했다.
//
// 이제 고른 볼륨은 물건이 된다. 나머지 카드가 판 밖으로 뜯겨 나가고, 남은 한 장이
// 뽑혀 나와 매체 자리에 눕고, 깨어난 그 매체를 헤드가 세 트랙에 걸쳐 읽는다.
// 읽히는 값은 새로 만든 것이 아니라 방금 카드에 적혀 있던 것들이다 - 손상 섹터
// 둘이 트랙 위의 갈린 자리로 드러나고, 볼륨 법칙이 다 읽은 매체에 박힌다.
//
// 그리고 그 매체가 볼륨마다 다르다(ui.h의 MediaKind). 처음에는 일곱 중 넷이 같은
// 원판에 장식만 달랐고, 결국 색으로만 구분됐다. 다른 물건으로 보이게 하는 것은
// 장식이 아니라 **실루엣과 트랙의 생김새**다 - 그래서 넷을 각각 갈랐다.
//
//   C:\  판이 셋인데 세로로 쌓여 있다. 트랙 하나가 판 하나다 (고리 셋이 아니다)
//   E:\  고리가 없다. 안으로 감기는 나선 하나가 트랙 셋을 이룬다
//   X:\  판이 통째가 아니다. 쐐기 여섯으로 갈라 각각 물려 두었다
//   A:\  자켓 안이다. 판도 판독도 셔터 창 안에서만 보인다 (사각형에 뚫린 가로창)
//
// 아래 그리기는 고리도 줄도 나선도 모른다 - 매체에게 "트랙 t의 진행 p가 놓이는
// 점"만 물어보고, 판독·손상·지시선이 전부 그 점 하나를 같이 쓴다.
//
// 모든 값은 경과 ms의 순수 함수다 (같은 시각이면 같은 프레임). 구간 경계는
// ui.h의 MountBeats가 쥐고 있고 소리(main.cpp)도 같은 표를 본다.
#define PLATTER_CY      432
#define PLATTER_R       236
#define PLATTER_HUB      80
#define PLATTER_PIVOT_DX 480   // 액추에이터 축. 매체 중심에서 오른쪽 아래로
#define PLATTER_PIVOT_DY 208
#define PLATTER_ENTRY    520   // 도는 매체에서 헤드가 닿는 각도(1/10도)
#define TAPE_SPAN        206   // 릴 사이로 띠가 지나가는 반폭
#define TAPE_REEL        126
#define CELL_SPAN        300   // 셀 격자의 반폭
#define CELL_ROW          96   // 행 간격
#define STACK_R          196   // 쌓인 판 한 장의 반지름
#define STACK_GAP         98   // 판 사이 높이
#define STACK_SQUASH     300   // 스택은 훨씬 눕혀 본다 - 세 장이 다 보여야 한다
#define OPTIC_IN         108   // 나선이 끝나는 안쪽
#define OPTIC_OUT        228   // 나선이 시작하는 바깥
#define CAGE_WEDGES        6
#define FLOPPY_HALF      288   // 자켓 반폭
#define FLOPPY_WIN_X     232   // 셔터 창 반폭
#define FLOPPY_WIN_TOP   176   // 창 위쪽 (중심에서)
#define FLOPPY_WIN_BOT   146   // 창 아래쪽

// 고리 매체의 트랙 반지름. 0이 바깥이고 안으로 갈수록 층이 깊다 - 층 하강이
// "더 파고든다"로 읽히려면 이 순서가 층 번호와 같아야 한다.
static int PlatterTrackR(int track) {
    static const int RADIUS[MOUNT_TRACKS] = {208, 158, 108};
    return RADIUS[track < 0 ? 0 : track >= MOUNT_TRACKS ? MOUNT_TRACKS - 1 : track];
}

static POINT PlatterAt(int cx, int cy, int rx, int ry, int angle) {
    POINT p = {cx + CosMille(angle) * rx / 1000, cy + SinMille(angle) * ry / 1000};
    return p;
}

// 다 열렸을 때의 눌림. 스택만 크게 눕혀 본다 - 정면에서 보면 세 장이 완전히
// 겹쳐 한 장과 구별되지 않는다. 나머지는 정면이다.
static int MediaSquash(int media) { return media == MEDIA_STACK ? STACK_SQUASH : 1000; }

// 이 연출의 유일한 기하 규약. 매체마다 트랙의 모양이 다르지만(고리 · 가로줄 ·
// 나선 · 판 한 장), 판독 진행도도 손상 섹터도 지시선 앵커도 전부 이 한 함수만
// 보고 그려진다. off는 트랙에 수직인 오프셋(px)이라 고리에서는 반지름이 되고
// 줄에서는 높이가 된다.
static POINT MediaTrackPoint(int media, int cx, int cy, int track, int p, int squash, int off) {
    if (track < 0) track = 0;
    if (track >= MOUNT_TRACKS) track = MOUNT_TRACKS - 1;
    if (media == MEDIA_TAPE) {
        POINT q = {Lerp(cx - TAPE_SPAN, cx + TAPE_SPAN, p), cy + (track - 1) * 34 + off};
        return q;
    }
    if (media == MEDIA_CELL) {
        POINT q = {Lerp(cx - CELL_SPAN, cx + CELL_SPAN, p), cy + (track - 1) * CELL_ROW + off};
        return q;
    }
    if (media == MEDIA_STACK) {
        // 트랙 하나가 판 하나다. 안으로 파고드는 것이 아니라 아래로 내려간다.
        int r = STACK_R + off, y = cy + (track - 1) * STACK_GAP;
        return PlatterAt(cx, y, r, r * squash / 1000, PLATTER_ENTRY + 3600 * p / 1000);
    }
    if (media == MEDIA_OPTICAL) {
        // 광매체의 트랙은 고리가 아니라 이어진 나선 하나다. 트랙 셋은 그 나선을
        // 세 바퀴로 나눈 것이고, 바깥에서 시작해 안으로 감긴다.
        int turn = track * 1000 + p;
        int r = OPTIC_OUT - (OPTIC_OUT - OPTIC_IN) * turn / (MOUNT_TRACKS * 1000) + off;
        return PlatterAt(cx, cy, r, r * squash / 1000, PLATTER_ENTRY + 3600 * p / 1000);
    }
    // 플로피의 판은 창보다 작다. 트랙이 창 밖으로 나가면 잘린 고리 몇 개만
    // 남아, 사각형 안에 둥근 것이 들어 있다는 것이 보이지 않는다.
    int r = PlatterTrackR(track) * (media == MEDIA_FLOPPY ? 76 : 100) / 100 + off;
    return PlatterAt(cx, cy, r, r * squash / 1000, PLATTER_ENTRY + 3600 * p / 1000);
}

// 헤드가 놓이는 진행도. 도는 판은 입구에 머물고 판이 그 아래로 지나가며,
// 테이프는 릴 사이 한가운데에 서서 띠가 지나가고, 셀 격자와 링크는 읽는
// 자리 자체가 움직인다.
static int MediaHeadProgress(int media, int read) {
    if (media == MEDIA_TAPE) return 500;
    if (media == MEDIA_CELL || media == MEDIA_LINK) return read;
    return 0;
}

// 이름표가 매체 밖에 붙는가. 스택은 눕혀 봐서 허브에 글자가 앉지 않고, 셀 격자와
// 플로피에는 애초에 허브가 없다 - 셋 다 기계에 붙은 이름표가 그 자리를 대신한다.
static int MediaHubIsPlate(int media) {
    return media == MEDIA_STACK || media == MEDIA_CELL || media == MEDIA_FLOPPY;
}

// 볼륨 문자가 앉는 자리.
static POINT MediaHub(int media, int cx, int cy) {
    if (media == MEDIA_TAPE) { POINT q = {cx - TAPE_SPAN - TAPE_REEL + 8, cy}; return q; }
    if (media == MEDIA_CELL) { POINT q = {cx, cy - CELL_ROW - 66}; return q; }
    if (media == MEDIA_STACK) { POINT q = {cx, cy - STACK_GAP - 118}; return q; }
    if (media == MEDIA_FLOPPY) { POINT q = {cx, cy + FLOPPY_WIN_BOT + 66}; return q; }
    POINT q = {cx, cy};
    return q;
}

// 마지막에 카메라가 파고드는 자리. 이름표가 매체 밖에 붙는 볼륨에서는 이름표가
// 아니라 매체 한가운데로 들어가야 한다.
static POINT MediaCore(int media, int cx, int cy) {
    if (media == MEDIA_TAPE) return MediaHub(media, cx, cy);
    POINT q = {cx, cy};
    return q;
}

// 매체가 실제로 차지하는 자리. 베이의 꺾쇠가 이것을 문다 - 테이프는 옆으로 넓고
// 스택은 납작한데 꺾쇠만 늘 원판 크기로 서 있으면, 무는 시늉만 하는 것이 된다.
static RECT MediaFrame(int media, int cx, int cy) {
    if (media == MEDIA_TAPE)
        return MakeRect(cx - TAPE_SPAN - TAPE_REEL * 2 - 28, cy - TAPE_REEL - 46,
                        cx + TAPE_SPAN + TAPE_REEL * 2 + 28, cy + TAPE_REEL + 46);
    if (media == MEDIA_CELL)
        return MakeRect(cx - CELL_SPAN - 62, cy - CELL_ROW - 82,
                        cx + CELL_SPAN + 62, cy + CELL_ROW + 82);
    if (media == MEDIA_STACK) {
        int half = STACK_GAP + STACK_R * STACK_SQUASH / 1000 + 44;
        return MakeRect(cx - STACK_R - 54, cy - half, cx + STACK_R + 54, cy + half);
    }
    if (media == MEDIA_FLOPPY)
        return MakeRect(cx - FLOPPY_HALF - 26, cy - FLOPPY_HALF - 26,
                        cx + FLOPPY_HALF + 26, cy + FLOPPY_HALF + 26);
    if (media == MEDIA_OPTICAL)
        return MakeRect(cx - OPTIC_OUT - 52, cy - OPTIC_OUT - 52,
                        cx + OPTIC_OUT + 52, cy + OPTIC_OUT + 52);
    return MakeRect(cx - PLATTER_R - 46, cy - PLATTER_R - 46,
                    cx + PLATTER_R + 46, cy + PLATTER_R + 46);
}

// A:\ 의 셔터 창. 이 사각형이 이 볼륨의 실루엣을 만든다 - 판도 트랙도 판독도
// 여기 안에서만 보이고, 나머지는 자켓이 덮는다.
static RECT FloppyWindow(int cx, int cy) {
    return MakeRect(cx - FLOPPY_WIN_X, cy - FLOPPY_WIN_TOP, cx + FLOPPY_WIN_X, cy + FLOPPY_WIN_BOT);
}

static void FillDisc(HDC dc, int cx, int cy, int rx, int ry, COLORREF color) {
    if (rx < 1 || ry < 1) return;
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    Ellipse(dc, cx - rx, cy - ry, cx + rx, cy + ry);
    SelectObject(dc, oldPen); DeleteObject(pen);
    SelectObject(dc, oldBrush); DeleteObject(brush);
}

// 고리를 자른 부채꼴 하나. 바깥 호와 안쪽 호를 이어 닫아 한 번에 칠한다
// (반지름 선을 촘촘히 긋는 것보다 훨씬 싸고, 테두리가 한 줄로 깔끔하다).
static void FillWedge(HDC dc, int cx, int cy, int inR, int outR, int squash,
                      int from, int sweep, COLORREF fill, COLORREF edge) {
    POINT p[34];
    int n = 0, steps = 15;
    for (int i = 0; i <= steps; ++i)
        p[n++] = PlatterAt(cx, cy, outR, outR * squash / 1000, from + sweep * i / steps);
    for (int i = steps; i >= 0; --i)
        p[n++] = PlatterAt(cx, cy, inR, inR * squash / 1000, from + sweep * i / steps);
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 2, edge);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    Polygon(dc, p, n);
    SelectObject(dc, oldPen); DeleteObject(pen);
    SelectObject(dc, oldBrush); DeleteObject(brush);
}

// 트랙 위의 한 구간. 고리면 호, 줄이면 선분, 나선이면 감기는 곡선이 된다.
static void DrawTrackRun(HDC dc, int media, int cx, int cy, int track, int from, int to,
                         int squash, int off, COLORREF tone, int thickness) {
    if (to <= from) return;
    int steps = MediaIsDisc(media) ? (to - from) / 18 + 2 : 2;
    if (steps > 100) steps = 100;
    POINT last = MediaTrackPoint(media, cx, cy, track, from, squash, off);
    for (int i = 1; i <= steps; ++i) {
        POINT next = MediaTrackPoint(media, cx, cy, track, from + (to - from) * i / steps, squash, off);
        DrawLine(dc, last.x, last.y, next.x, next.y, tone, thickness);
        last = next;
    }
}

// 손상 섹터. 매끈하게 그리면 색만 다른 트랙이 된다. 실제로 갈려 있으려면 자리가
// 끊겨 있어야 하므로 트랙을 가로지르는, 이가 빠진 톱니로 그린다 - 볼륨 색이 붉은
// 계열이어도(X:\) "부서진 자리"로 읽힌다.
static void DrawDamageRun(HDC dc, int media, int cx, int cy, int track, int from, int span,
                          int squash, COLORREF tone) {
    DrawTrackRun(dc, media, cx, cy, track, from, from + span, squash, -7, tone, 2);
    DrawTrackRun(dc, media, cx, cy, track, from, from + span, squash, 7, tone, 2);
    for (int i = 1; i < 6; i += 2) {
        POINT a = MediaTrackPoint(media, cx, cy, track, from + span * i / 6, squash, -7);
        POINT b = MediaTrackPoint(media, cx, cy, track, from + span * i / 6, squash, 7);
        DrawLine(dc, a.x, a.y, b.x, b.y, tone, 2);
    }
}

// 스핀들 각도(1/10도). 기동 구간에서 제곱으로 붙고 그 뒤는 등속이다. 초당 약
// 1.9회전 - 더 빠르게 두면 60fps에서 눈금이 뒤로 도는 것처럼 보인다.
static int PlatterAngle(int elapsed, const MountBeats& beats, int mount) {
    int spun;
    if (!mount) spun = elapsed + 1300;   // 하강 때는 이미 회전수에 올라 있다
    else {
        int spinMs = beats.readAt - beats.spinAt;
        int ramp = Track(elapsed, beats.spinAt, beats.readAt);
        spun = spinMs * ramp / 1000 * ramp / 1000 / 2;   // 가속 동안 쌓인 각
        if (elapsed > beats.readAt) spun += elapsed - beats.readAt;
    }
    return spun * 7 % 3600;
}

// 손상 섹터의 자리. 볼륨마다 늘 같은 곳이 갈려 있어야 두 번째 플레이에서
// "그 볼륨"으로 읽힌다. 그래서 난수가 아니라 볼륨 번호의 해시다.
static int MediaDamage(int drive, int track, int index, int* span) {
    uint32_t h = Hash3(drive * 17 + 3, track, index * 29 + 11);
    *span = 34 + (int)(h % 52);
    return (int)((h >> 9) % 1000);
}

// 접촉이 끊기는 매체(E:\ 광 디스크)가 지금 끊겨 있는가. 시각의 함수라 프레임이
// 겹쳐도 같은 값이 나오고, 판독 진행도는 건드리지 않는다 - 값은 그대로 가고
// 그 프레임의 신호만 사라진다.
//
// 처음에는 끊길 때 판까지 통째로 지웠다. 그러면 접촉 불량이 아니라 그리기가
// 고장 난 것으로 보인다 - 물건은 계속 거기 있어야 하고, 끊기는 것은 그 물건을
// 읽고 있다는 신호(반사·판독 호·선단)뿐이다.
static int MediaDropout(int media, int elapsed) {
    if (media != MEDIA_OPTICAL) return 0;
    return (Hash3(elapsed / 150, 7, 31) % 100) < 11;
}

// 도는 판의 앞면. 매끈한 원은 돌아도 가만히 있는 것으로 보이므로, 도는 것을
// 말하는 일은 눈금과 한자리에 머무는 빛 반사가 맡는다.
static void DrawPlatterFace(HDC dc, int cx, int cy, int rx, int ry, int spin, int hubR,
                            COLORREF tone, COLORREF body) {
    FillDisc(dc, cx, cy, rx + 3, ry + 3, MixColor(C_BG, tone, 16));
    FillDisc(dc, cx, cy, rx, ry, body);
    for (int i = 0; i < 24; ++i) {
        int a = spin + i * 150, lit = (i % 8) == 0;
        POINT from = PlatterAt(cx, cy, rx * 90 / 100, ry * 90 / 100, a);
        POINT to = PlatterAt(cx, cy, rx, ry, a);
        DrawLine(dc, from.x, from.y, to.x, to.y, MixColor(C_BG, tone, lit ? 52 : 22), lit ? 2 : 1);
    }
    for (int i = 0; i < 3; ++i) {
        POINT from = PlatterAt(cx, cy, hubR, hubR * ry / rx, spin + i * 1200);
        POINT to = PlatterAt(cx, cy, rx * 94 / 100, ry * 94 / 100, spin + i * 1200);
        DrawLine(dc, from.x, from.y, to.x, to.y, MixColor(C_BG, tone, 18), 1);
    }
    DrawCraftArc(dc, cx, cy, rx * 97 / 100, ry * 97 / 100, 2450, 700, MixColor(C_BG, tone, 30), 2);
    DrawGlowRing(dc, cx, cy, rx, ry, MixColor(C_BG, tone, 46), 2);
}

// ---- 매체 일곱 ------------------------------------------------------------
// 각각이 무엇으로 만들어졌는지가 실루엣만으로 갈려야 한다. 공유하는 것은 트랙
// 기하와 판독 규칙뿐이고, 아래 몸통은 볼륨마다 따로 그린다.

// C:\ 판이 셋인데 세로로 쌓여 있다. 정면에서 보면 세 장이 완전히 겹쳐 한 장과
// 구별되지 않으므로 크게 눕혀 본다 - 이 볼륨만 원이 아니라 기둥으로 보인다.
// 트랙 하나가 판 하나라, 층을 내려가는 것이 아래 판으로 내려가는 것이 된다.
static void DrawMediaStack(HDC dc, int cx, int cy, int squash, int spin, COLORREF tone) {
    int ry = STACK_R * squash / 1000;
    // 스핀들 축이 세 장을 꿴다. 축이 없으면 세 장이 따로 떠 있는 것으로 보인다.
    Fill(dc, MakeRect(cx - 13, cy - STACK_GAP - ry, cx + 13, cy + STACK_GAP + ry),
         MixColor(C_BG, tone, 22));
    for (int t = MOUNT_TRACKS - 1; t >= 0; --t) {
        int y = cy + (t - 1) * STACK_GAP;
        // 두께. 판 하나가 종이가 아니라 금속판이라는 것은 이 아랫면이 말한다.
        FillDisc(dc, cx, y + 7, STACK_R, ry, MixColor(C_BG, tone, 10));
        DrawPlatterFace(dc, cx, y, STACK_R, ry, spin, 36, tone, RGB(18, 25, 33));
        FillDisc(dc, cx, y, 34, 34 * squash / 1000, RGB(9, 13, 18));
        DrawGlowRing(dc, cx, y, 34, 34 * squash / 1000, MixColor(C_BG, tone, 44), 2);
    }
}

// D:\ 오픈릴 테이프. 릴 둘 사이로 띠가 지나가고, 읽을수록 왼쪽 릴이 풀리고
// 오른쪽 릴이 감긴다. 넓지만 느린 창고가 그대로 형태가 된다.
static void DrawMediaTape(HDC dc, int cx, int cy, int open, int read, int elapsed, COLORREF tone) {
    int thread = EaseOutCubic(open), spin = elapsed * 9 % 3600;
    for (int side = 0; side < 2; ++side) {
        int rx = side ? cx + TAPE_SPAN + TAPE_REEL : cx - TAPE_SPAN - TAPE_REEL;
        // 감긴 양. 왼쪽은 풀리고 오른쪽은 감긴다 (합은 늘 같다).
        int wound = side ? 44 + read * 62 / 1000 : 106 - read * 62 / 1000;
        FillDisc(dc, rx, cy, TAPE_REEL, TAPE_REEL, RGB(9, 13, 18));
        FillDisc(dc, rx, cy, wound, wound, RGB(21, 27, 34));
        DrawGlowRing(dc, rx, cy, wound, wound, MixColor(C_BG, tone, 40), 2);
        DrawGlowRing(dc, rx, cy, TAPE_REEL, TAPE_REEL, MixColor(C_BG, tone, 34), 2);
        for (int i = 0; i < 6; ++i) {   // 릴의 살. 이것만 돌아도 감기는 것이 보인다
            POINT a = PlatterAt(rx, cy, 30, 30, spin * (side ? 1 : -1) + i * 600);
            POINT b = PlatterAt(rx, cy, TAPE_REEL - 6, TAPE_REEL - 6, spin * (side ? 1 : -1) + i * 600);
            DrawLine(dc, a.x, a.y, b.x, b.y, MixColor(C_BG, tone, 26), 2);
        }
        DrawGlowRing(dc, rx, cy, 28, 28, MixColor(C_BG, tone, 52), 2);
    }
    int left = cx - TAPE_SPAN, right = Lerp(cx - TAPE_SPAN, cx + TAPE_SPAN, thread);
    Fill(dc, MakeRect(left, cy - 52, right, cy + 52), RGB(14, 19, 25));
    DrawLine(dc, left, cy - 52, right, cy - 52, MixColor(C_BG, tone, 40), 2);
    DrawLine(dc, left, cy + 52, right, cy + 52, MixColor(C_BG, tone, 40), 2);
}

// E:\ 광 디스크. 트랙이 고리가 아니라 이어진 나선 하나고, 읽는 것도 피벗 암이
// 아니라 직선 레일 위의 슬레드다. 표면이 거울이라 무지개 띠가 한자리에 서 있고,
// 접촉이 끊길 때마다 그 띠가 통째로 사라진다.
// 촘촘한 나선 한 줄. 펜을 한 번만 만들어 Polyline으로 긋는다 - 마디마다
// DrawLine을 부르면 프레임마다 펜이 이백 개 넘게 만들어졌다 없어진다.
static void DrawSpiralGuide(HDC dc, int cx, int cy, int outR, int inR, int squash,
                            int turns, COLORREF tone) {
    POINT p[241];
    for (int i = 0; i < 241; ++i) {
        int r = outR - (outR - inR) * i / 240;
        p[i] = PlatterAt(cx, cy, r, r * squash / 1000, PLATTER_ENTRY + 3600 * turns * i / 240);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, tone);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    Polyline(dc, p, 241);
    SelectObject(dc, oldPen); DeleteObject(pen);
}

static void DrawMediaOptical(HDC dc, int cx, int cy, int squash, int spin, int elapsed,
                             int drop, COLORREF tone) {
    int rx = OPTIC_OUT + 14, ry = rx * squash / 1000;
    DrawPlatterFace(dc, cx, cy, rx, ry, spin, 56, tone, RGB(14, 20, 27));
    // 판독이 밟는 세 바퀴는 이 나선의 일부다. 밑에 촘촘한 나선이 깔려 있어야
    // 트랙 셋이 "고리 셋"이 아니라 "이어진 한 줄"로 읽힌다.
    DrawSpiralGuide(dc, cx, cy, OPTIC_OUT, 60, squash, 15, MixColor(C_BG, tone, 12));
    if (drop && FxDecorOn())
        DrawBandGlitch(dc, MakeRect(cx - rx, cy - ry, cx + rx, cy + ry), elapsed, FxScale(14), 91, 9);
    if (!drop) for (int i = 0; i < 5; ++i) {
        static const COLORREF SHEEN[5] = {C_BLUE, C_GREEN, C_YELLOW, C_RED, C_BLUE};
        int r = rx * (72 + i * 5) / 100;
        DrawCraftArc(dc, cx, cy, r, r * ry / rx, 2280 + i * 40, 560, MixColor(C_BG, SHEEN[i], 26), 3);
    }
    // 가운데의 큰 투명 구멍과 클램프 링. 광 디스크를 광 디스크로 만드는 것의 절반이다.
    DrawGlowRing(dc, cx, cy, 76, 76 * squash / 1000, MixColor(C_BG, tone, 26), 1);
    FillDisc(dc, cx, cy, 52, 52 * squash / 1000, RGB(6, 9, 13));
    DrawGlowRing(dc, cx, cy, 52, 52 * squash / 1000, MixColor(C_BG, tone, 48), 2);
    // 슬레드가 달리는 레일. 판 아래를 곧게 가로지른다.
    Fill(dc, MakeRect(cx - 10, cy + ry + 26, cx + rx + 104, cy + ry + 34), MixColor(C_BG, tone, 18));
    for (int x = cx; x < cx + rx + 96; x += 26)
        Fill(dc, MakeRect(x, cy + ry + 22, x + 3, cy + ry + 38), MixColor(C_BG, tone, 26));
}

// N:\ 실체가 없다. 고리는 끊긴 점선이고, 화면 밖 링크에서 온 패킷이 그것을
// 채운다. 원격 격리가 형태로 온다 - 만질 수 있는 물건이 하나도 없다.
static void DrawMediaLink(HDC dc, int cx, int cy, int rx, int ry, int open, int elapsed, COLORREF tone) {
    int lit = EaseOutCubic(open);
    for (int t = 0; t < MOUNT_TRACKS; ++t) {
        int r = PlatterTrackR(t), er = r * ry / rx;
        for (int i = 0; i < 24; ++i) {
            if (((i + t) % 3) == 0) continue;         // 끊긴 자리
            int a = i * 150 + t * 60 + elapsed / 9;
            DrawCraftArc(dc, cx, cy, r, er, a, 78, MixColor(C_BG, tone, 12 + 22 * lit / 1000), 2);
        }
    }
    DrawLine(dc, BASE_WIDTH, cy - 176, cx + 96, cy - 52, MixColor(C_BG, tone, 22), 1);
    for (int i = 0; i < 5; ++i) {
        int p = ((elapsed * 2 + i * 400) % 2000) * 1000 / 2000;
        int x = Lerp(BASE_WIDTH, cx + 96, p), y = Lerp(cy - 176, cy - 52, p);
        Fill(dc, MakeRect(x - 4, y - 2, x + 4, y + 3), MixColor(C_BG, tone, 30 + 40 * lit / 1000));
    }
    for (int i = 0; i < 6; ++i) {   // 육각 노드. 판이 아니라 주소 하나다
        POINT a = PlatterAt(cx, cy, PLATTER_HUB, PLATTER_HUB * ry / rx, i * 600);
        POINT b = PlatterAt(cx, cy, PLATTER_HUB, PLATTER_HUB * ry / rx, (i + 1) * 600);
        DrawLine(dc, a.x, a.y, b.x, b.y, MixColor(C_BG, tone, 48), 2);
    }
}

// R:\ 셀 격자. 도는 것이 하나도 없다. 행을 훑어 읽고, 칸은 계속 되쓰인다 -
// 휘발성 램디스크가 형태와 동작 양쪽으로 온다.
static void DrawMediaCell(HDC dc, int cx, int cy, int open, int elapsed, COLORREF tone) {
    int powered = EaseOutCubic(open);
    for (int row = 0; row < MOUNT_TRACKS; ++row) {
        int y = cy + (row - 1) * CELL_ROW;
        if (powered < (row + 1) * 1000 / (MOUNT_TRACKS + 1)) continue;
        Fill(dc, MakeRect(cx - CELL_SPAN - 16, y - 34, cx + CELL_SPAN + 16, y + 34), RGB(10, 15, 21));
        Outline(dc, MakeRect(cx - CELL_SPAN - 16, y - 34, cx + CELL_SPAN + 16, y + 34), MixColor(C_BG, tone, 24), 1);
        for (int col = 0; col < 24; ++col) {
            int x = cx - CELL_SPAN - 4 + col * (CELL_SPAN * 2 + 8) / 24;
            // 칸마다 다른 주기로 새로 고쳐진다. 램은 가만히 있어도 계속 되쓰인다.
            int refresh = (elapsed / 90 + col * 5 + row * 11) % 7;
            Fill(dc, MakeRect(x, y - 22, x + 16, y + 22), MixColor(C_BG, tone, refresh < 2 ? 30 : 13));
        }
    }
    Fill(dc, MakeRect(cx - CELL_SPAN - 46, cy - CELL_ROW - 40, cx - CELL_SPAN - 30, cy + CELL_ROW + 40),
         MixColor(C_BG, tone, 20));
}

// X:\ 판이 통째가 아니다. 압수된 판은 쐐기 여섯으로 갈라 각각 물려 두었고, 갈린
// 틈이 그대로 보인다 - 이 볼륨만 원이 아니라 톱니바퀴 같은 실루엣이 된다.
static void DrawMediaCage(HDC dc, int cx, int cy, int rx, int ry, int spin, int open, COLORREF tone) {
    int release = EaseOutCubic(open), hub = PLATTER_HUB + 10;
    for (int i = 0; i < CAGE_WEDGES; ++i) {
        int from = spin + i * (3600 / CAGE_WEDGES) + 52, sweep = 3600 / CAGE_WEDGES - 104;
        FillWedge(dc, cx, cy, hub, rx, ry * 1000 / rx, from, sweep,
                  RGB(26, 15, 17), MixColor(C_BG, tone, 44));
        // 쐐기마다 한 줄. 조각이 각각 다른 방향을 보고 있어야 갈라진 것으로 읽힌다.
        POINT a = PlatterAt(cx, cy, hub + 16, (hub + 16) * ry / rx, from + sweep / 2);
        POINT b = PlatterAt(cx, cy, rx - 16, (rx - 16) * ry / rx, from + sweep / 2);
        DrawLine(dc, a.x, a.y, b.x, b.y, MixColor(C_BG, tone, 22), 1);
    }
    FillDisc(dc, cx, cy, hub, hub * ry / rx, RGB(12, 9, 11));
    DrawGlowRing(dc, cx, cy, hub, hub * ry / rx, MixColor(C_BG, tone, 44), 2);
    for (int i = 0; i < 4; ++i) {   // 잠금쇠. 기동과 함께 바깥으로 물러난다
        int a = 450 + i * 900, reach = rx + 10 + release * 34 / 1000;
        POINT grip = PlatterAt(cx, cy, reach, reach * ry / rx, a);
        POINT root = PlatterAt(cx, cy, reach + 44, (reach + 44) * ry / rx, a);
        DrawLine(dc, root.x, root.y, grip.x, grip.y, MixColor(C_BG, tone, 44), 7);
        RECT jaw = MakeRect(grip.x - 13, grip.y - 13, grip.x + 13, grip.y + 13);
        Fill(dc, jaw, RGB(22, 10, 11));
        for (int x = jaw.left - 12; x < jaw.right; x += 9)
            DrawLine(dc, x, jaw.bottom, x + 13, jaw.top, MixColor(C_BG, C_RED, 44), 3);
        Outline(dc, jaw, MixColor(C_BG, tone, 60), 2);
    }
    // 경고 띠는 판 위쪽 한 줄뿐이다. 아래에도 두면 판독 데이터 띠와 겹치고,
    // 위아래 둘이면 경고가 배경 무늬가 되어 아무것도 경고하지 않는다.
    RECT band = MakeRect(cx - rx - 30, cy - ry - 40, cx + rx + 30, cy - ry - 24);
    Fill(dc, band, RGB(22, 10, 11));
    for (int x = band.left; x < band.right; x += 18)
        DrawLine(dc, x, band.bottom, x + 14, band.top, MixColor(C_BG, C_RED, 40), 3);
    Outline(dc, band, MixColor(C_BG, C_RED, 40), 1);
}

// A:\ 플로피 한 장. 복구 도구 자신의 마지막 기록이고, 타이틀과 삽입 연출이 내내
// 보여 준 바로 그 물건이다. 이 볼륨만 원이 아니라 사각형이다 - 판은 자켓에 덮여
// 셔터 창으로만 보인다. 자켓은 판보다 먼저, 셔터는 판보다 나중에 그린다.
static void DrawFloppyJacket(HDC dc, int cx, int cy, COLORREF tone) {
    RECT shell = MakeRect(cx - FLOPPY_HALF, cy - FLOPPY_HALF, cx + FLOPPY_HALF, cy + FLOPPY_HALF);
    Fill(dc, shell, RGB(13, 18, 25));
    Outline(dc, shell, MixColor(C_BG, tone, 42), 2);
    Fill(dc, MakeRect(shell.right - 38, shell.top, shell.right, shell.top + 38), RGB(6, 9, 13));
    DrawLine(dc, shell.right - 38, shell.top, shell.right, shell.top + 38, MixColor(C_BG, tone, 42), 2);
    Outline(dc, MakeRect(shell.left + 20, shell.top + 16, shell.left + 44, shell.top + 40),
            MixColor(C_BG, tone, 32), 2);                      // 쓰기 방지 창
    RECT win = FloppyWindow(cx, cy);
    Fill(dc, MakeRect(win.left - 6, win.top - 6, win.right + 6, win.bottom + 6), RGB(6, 9, 13));
    Outline(dc, MakeRect(win.left - 6, win.top - 6, win.right + 6, win.bottom + 6), MixColor(C_BG, tone, 34), 2);
}

static void DrawFloppyShutter(HDC dc, int cx, int cy, int open, COLORREF tone) {
    RECT win = FloppyWindow(cx, cy);
    int slide = (win.right - win.left + 24) * EaseOutCubic(open) / 1000;
    RECT metal = MakeRect(win.left - 8 + slide, win.top - 8, win.right + 8 + slide, win.bottom + 8);
    if (metal.left >= cx + FLOPPY_HALF) return;
    if (metal.right > cx + FLOPPY_HALF) metal.right = cx + FLOPPY_HALF;
    Fill(dc, metal, RGB(28, 35, 44));
    Outline(dc, metal, MixColor(C_BG, tone, 52), 2);
    for (int y = metal.top + 20; y < metal.bottom - 14; y += 26)
        Fill(dc, MakeRect(metal.left + 10, y, metal.left + 30, y + 6), MixColor(C_BG, tone, 30));
}

// 액추에이터 · 슬레드 · 테이프 헤드 · 버스 프로브 · 헤드 콤. 무엇이 읽고 있는지도
// 볼륨마다 다르다 - 같은 팔이 일곱 번 나오면 매체를 갈라 둔 것이 몸통에서 끝난다.
static void DrawReadHead(HDC dc, int media, int cx, int cy, POINT head, int lifted,
                         int squash, COLORREF tone) {
    int lift = lifted * 26 / 1000;
    POINT seat = head;
    head.y -= lift;
    if (media == MEDIA_LINK) {
        // 읽는 물건이 없다. 도착한 자리에 수신 표시만 선다.
        DrawGlowRing(dc, head.x, head.y, 16, 16, MixColor(C_BG, tone, 62), 2);
        Fill(dc, MakeRect(head.x - 4, head.y - 4, head.x + 4, head.y + 4), MixColor(C_BG, tone, 74));
        return;
    }
    if (media == MEDIA_CELL) {
        Fill(dc, MakeRect(head.x - 12, cy - CELL_ROW - 52, head.x + 12, cy + CELL_ROW + 52),
             MixColor(C_BG, tone, 16));
        Fill(dc, MakeRect(head.x - 12, head.y - 30, head.x + 12, head.y + 30), MixColor(C_BG, tone, 54));
        return;
    }
    if (media == MEDIA_TAPE) {
        for (int side = 0; side < 2; ++side)
            DrawGlowRing(dc, cx + (side ? 92 : -92), cy + 70, 22, 22, MixColor(C_BG, tone, 34), 2);
        Panel(dc, MakeRect(cx - 30, cy + 66, cx + 30, cy + 118), RGB(10, 15, 21), MixColor(C_BG, tone, 40));
        Fill(dc, MakeRect(cx - 5, cy + 4, cx + 5, cy + 70), MixColor(C_BG, tone, 34));
        Fill(dc, MakeRect(cx - 16, head.y - 5, cx + 16, head.y + 5), MixColor(C_BG, tone, 70));
        return;
    }
    if (media == MEDIA_STACK) {
        // 헤드 콤. 판마다 팔이 하나씩이고 기둥 하나에 묶여 함께 오르내린다 -
        // 이 볼륨에서 층을 내려가는 것은 안으로 파고드는 것이 아니라 아래 판으로
        // 내려가는 것이므로, 읽는 물건도 세로로 서 있어야 한다.
        int postX = cx + STACK_R + 132, ry = STACK_R * squash / 1000;
        RECT post = MakeRect(postX - 14, cy - STACK_GAP - ry - 20, postX + 14, cy + STACK_GAP + ry + 20);
        Panel(dc, post, RGB(10, 15, 21), MixColor(C_BG, tone, 40));
        for (int t = 0; t < MOUNT_TRACKS; ++t) {
            POINT on = PlatterAt(cx, cy + (t - 1) * STACK_GAP, STACK_R, ry, PLATTER_ENTRY);
            int active = head.y > on.y - STACK_GAP / 2 && head.y <= on.y + STACK_GAP / 2;
            DrawLine(dc, post.left, on.y, active ? head.x : on.x + 58, on.y,
                     MixColor(C_BG, tone, active ? 62 : 26), active ? 5 : 3);
            if (active) Fill(dc, MakeRect(head.x - 9, head.y - 6, head.x + 9, head.y + 6), MixColor(C_BG, tone, 78));
        }
        return;
    }
    if (media == MEDIA_OPTICAL) {
        // 직선 레일 위의 슬레드. 피벗이 없으므로 아래에서 곧게 밀려 올라온다.
        int railY = cy + OPTIC_OUT + 44;
        Fill(dc, MakeRect(head.x - 28, railY - 12, head.x + 28, railY + 10), MixColor(C_BG, tone, 40));
        DrawLine(dc, head.x, railY - 12, head.x, head.y, MixColor(C_BG, tone, 46), 5);
        Fill(dc, MakeRect(head.x - 10, head.y - 7, head.x + 10, head.y + 7), MixColor(C_BG, tone, 78));
        if (lift > 0) DrawLine(dc, head.x, head.y + 7, seat.x, seat.y, MixColor(C_BG, tone, 20), 1);
        return;
    }
    if (media == MEDIA_FLOPPY) {
        // 헤드 둘. 창의 위아래 레일을 타고 같은 자리를 위에서 한 번, 아래에서 한
        // 번 읽는다 - 자기 자신을 읽는 유일한 볼륨이라 양면이 동시에 걸린다.
        RECT win = FloppyWindow(cx, cy);
        for (int side = 0; side < 2; ++side) {
            int railY = side ? win.bottom - 10 : win.top + 10, reach = side ? -18 : 18;
            Fill(dc, MakeRect(win.left, railY - 3, win.right, railY + 3), MixColor(C_BG, tone, 22));
            int x = head.x < win.left + 14 ? win.left + 14 : head.x > win.right - 14 ? win.right - 14 : head.x;
            Fill(dc, MakeRect(x - 13, railY - 9, x + 13, railY + 9), MixColor(C_BG, tone, 58));
            // 팔은 레일에서 짧게만 나온다. 헤드까지 줄을 그으면 판을 가로지르는
            // 긴 선 하나가 남아, 양면을 읽는 것이 아니라 판이 갈라진 것으로 보인다.
            DrawLine(dc, x, railY, x, railY + reach, MixColor(C_BG, tone, 46), 3);
        }
        Fill(dc, MakeRect(head.x - 9, head.y - 6, head.x + 9, head.y + 6), MixColor(C_BG, tone, 78));
        return;
    }
    // 피벗 암 (X:\ 격리 캐비닛).
    POINT pivot = {cx + PLATTER_PIVOT_DX, cy + PLATTER_PIVOT_DY - lift / 2};
    Panel(dc, MakeRect(pivot.x - 22, pivot.y - 22, pivot.x + 22, pivot.y + 22),
          MixColor(C_BG, tone, 14), MixColor(C_BG, tone, 44));
    Fill(dc, MakeRect(pivot.x - 7, pivot.y - 7, pivot.x + 7, pivot.y + 7), MixColor(C_BG, tone, 56));
    // 축 반대쪽의 균형추. 팔 하나만 뻗어 있으면 막대기이고, 축을 사이에 두고
    // 짧은 쪽이 있어야 도는 물건으로 보인다.
    DrawLine(dc, pivot.x, pivot.y, pivot.x + (pivot.x - head.x) / 6, pivot.y + (pivot.y - head.y) / 6,
             MixColor(C_BG, tone, 34), 11);
    DrawLine(dc, pivot.x, pivot.y, head.x, head.y, MixColor(C_BG, tone, 40), 7);
    DrawLine(dc, pivot.x, pivot.y, head.x, head.y, MixColor(C_BG, tone, 62), 3);
    Fill(dc, MakeRect(head.x - 9, head.y - 6, head.x + 9, head.y + 6), MixColor(C_BG, tone, 74));
    if (lift > 0) DrawLine(dc, head.x, head.y + 6, seat.x, seat.y, MixColor(C_BG, tone, 20), 1);
}

// 매체 한 벌. squash는 도는 판이 얼마나 열렸는지이고, 줄 매체는 쓰지 않는다.
static void DrawMediaBody(HDC dc, int media, int cx, int cy, int squash, int spin, int open,
                          int read, int elapsed, int drop, COLORREF tone) {
    int rx = PLATTER_R, ry = PLATTER_R * squash / 1000;
    if (media == MEDIA_TAPE) { DrawMediaTape(dc, cx, cy, open, read, elapsed, tone); return; }
    if (media == MEDIA_CELL) { DrawMediaCell(dc, cx, cy, open, elapsed, tone); return; }
    if (media == MEDIA_LINK) { DrawMediaLink(dc, cx, cy, rx, ry, open, elapsed, tone); return; }
    if (media == MEDIA_STACK) { DrawMediaStack(dc, cx, cy, squash, spin, tone); return; }
    if (media == MEDIA_OPTICAL) { DrawMediaOptical(dc, cx, cy, squash, spin, elapsed, drop, tone); return; }
    if (media == MEDIA_CAGE) { DrawMediaCage(dc, cx, cy, rx, ry, spin, open, tone); return; }
    DrawPlatterFace(dc, cx, cy, PLATTER_R * 76 / 100, PLATTER_R * 76 / 100 * squash / 1000,
                    spin, PLATTER_HUB * 76 / 100, tone, RGB(16, 22, 29));
}

// 드라이브 베이. 매체 혼자 어둠에 떠 있으면 이 일이 어디에서 일어나는지 알 수 없다.
// 레일과 꺾쇠가 매체를 둘러싸 "열려 있는 기계 안"이 되고, 판독으로 나온 값들도 그
// 레일 안쪽에 붙으므로 화면에 얹힌 설명이 아니라 기계의 계기로 읽힌다.
static void DrawDriveBay(HDC dc, int width, int height, int media, int cx, int cy, int built, COLORREF tone) {
    if (built <= 0) return;
    int p = EaseOutCubic(built), top = 168, bottom = height - 92;
    COLORREF metal = MixColor(C_BG, tone, 26), dim = MixColor(C_BG, tone, 13);
    int reach = (bottom - top) * p / 1000;
    for (int side = 0; side < 2; ++side) {
        RECT rail = MakeRect(side ? width - 44 : 26, top, side ? width - 26 : 44, top + reach);
        Fill(dc, rail, MixColor(C_BG, tone, 8));
        Outline(dc, rail, dim, 1);
        for (int y = top + 14; y < top + reach - 10; y += 30)
            Fill(dc, MakeRect(rail.left + 4, y, rail.right - 4, y + 9), metal);
    }
    RECT frame = MediaFrame(media, cx, cy);
    for (int i = 0; i < 4; ++i) {
        int sx = (i & 1) ? 1 : -1, sy = (i & 2) ? 1 : -1, off = (1000 - p) * 80 / 1000;
        int x = (sx > 0 ? frame.right : frame.left) + sx * off;
        int y = (sy > 0 ? frame.bottom : frame.top) + sy * off;
        DrawLine(dc, x, y, x - sx * 54, y, metal, 3);
        DrawLine(dc, x, y, x, y - sy * 54, metal, 3);
    }
}

// 매체 옆에 붙는 지시선 딱지. 트랙 위의 어느 자리를 말하는지 선으로 이어 준다.
static void DrawPlatterTag(HDC dc, const RECT& box, int anchorX, int anchorY, int reveal,
                           const wchar_t* title, const wchar_t* body, COLORREF tone) {
    if (reveal <= 0) return;
    int p = EaseOutCubic(reveal);
    int side = anchorX > box.right ? 1 : -1;   // 매체가 딱지의 어느 쪽에 있는가
    int edgeX = side > 0 ? box.right : box.left;
    int midY = (box.top + box.bottom) / 2;
    DrawLine(dc, edgeX, midY, Lerp(edgeX, anchorX, p), Lerp(midY, anchorY, p), MixColor(C_BG, tone, 50), 1);
    if (p > 850) Fill(dc, MakeRect(anchorX - 3, anchorY - 3, anchorX + 4, anchorY + 4), tone);
    int slide = (1000 - p) * 26 / 1000 * side;
    RECT card = MakeRect(box.left - slide, box.top, box.right - slide, box.bottom);
    Panel(dc, card, RGB(10, 15, 21), MixColor(C_BG, tone, 40 + p * 30 / 1000));
    Fill(dc, MakeRect(card.left, card.top, card.left + 3, card.bottom), tone);
    Text(dc, card.left + 14, card.top + 9, title, tone, gFontSmall);
    TextRect(dc, MakeRect(card.left + 14, card.top + 31, card.right - 12, card.bottom - 8),
             body, C_TEXT, gFontSmall, DT_WORDBREAK);
}

static void DrawDescent(HDC dc, int width, int height) {
    int mount = gDescentToFloor == 0;
    int lockStage = mount && gDescentChoiceIndex >= 0;
    MountBeats beats = MountBeatsFor(mount);
    int elapsed = (int)(GetTickCount() - gDescentStart);
    if (elapsed < 0) elapsed = 0;
    if (elapsed > beats.total) elapsed = beats.total;
    if (lockStage && elapsed < MOUNT_LOCK_MS) { DrawVolumeLock(dc, width, height, elapsed); return; }

    int index = gGame.selectedDrive < 0 ? 0 : gGame.selectedDrive;
    const DriveInfo* drive = &DRIVE_INFO[index];
    COLORREF tone = (COLORREF)drive->color;
    int media = DriveMedia(index), drop = MediaDropout(media, elapsed);
    int cx = width / 2, cy = PLATTER_CY, trackMs = MountTrackMs(beats);

    // 기계 안. 빈 검정이 아니라 바닥 격자가 깔려 있어야 매체가 어딘가에 놓인다.
    Fill(dc, MakeRect(0, 68, width, height), RGB(6, 9, 13));
    if (FxDecorOn()) {
        COLORREF grid = MixColor(C_BG, tone, FxScale(9));
        for (int x = 40; x < width; x += 96) DrawLine(dc, x, 96, x, height - 14, grid, 1);
        for (int y = 140; y < height; y += 96) DrawLine(dc, 24, y, width - 24, y, grid, 1);
    }

    // ---- 안착. 잠긴 카드가 뽑혀 나와 매체 자리에 눕는다 --------------------
    // 카드가 눕는 마지막 모습(납작한 판)과 깨어나기 시작하는 매체의 첫 모습이
    // 같은 도형이라 그 사이에 이어 붙인 자리가 없다.
    int squash = MediaSquash(media);
    if (mount) {
        // 떨어지는 것이라 가속한다. EaseOutCubic으로 두면 카드가 처음부터
        // 납작해져 버려, 눕는 것이 아니라 미끄러지는 것으로 보였다.
        int seated = EaseInCubic(Track(elapsed, beats.seatAt, beats.spinAt));
        // 다 열렸을 때의 눌림이 매체마다 다르다. 스택은 정면으로 서면 세 장이
        // 겹쳐 한 장이 되므로 끝까지 눕혀 둔다.
        int target = MediaSquash(media);
        if (MediaIsDisc(media))
            squash = 55 + (target - 55) * EaseOutCubic(Track(elapsed, beats.spinAt + 40, beats.readAt - 140)) / 1000;
        if (seated < 1000) {
            RECT from = DriveCardRect(gDescentChoiceIndex >= 0 ? gDescentChoiceIndex : 0);
            RECT to = MakeRect(cx - PLATTER_R, cy - 13, cx + PLATTER_R, cy + 13);
            RECT slab = LerpRect(from, to, seated);
            Panel(dc, slab, RGB(12, 17, 23), MixColor(C_BG, tone, 30 + 30 * seated / 1000));
            if (slab.bottom - slab.top > 120)
                TextRect(dc, MakeRect(slab.left + 8, slab.top + 22, slab.right - 8, slab.top + 88),
                         drive->letter, tone, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (FxDecorOn())
                DrawSectorStatic(dc, slab, index + 31, elapsed / NOISE_CHURN_MS, FxScale(420 * seated / 1000));
            // 떨어지는 동안 받을 자리가 먼저 켜진다 - 카드가 어디로 가는지가 보인다.
            POINT core = MediaCore(media, cx, cy);
            DrawGlowRing(dc, core.x, core.y, PLATTER_HUB, 6, MixColor(C_BG, tone, 20 + 40 * seated / 1000), 2);
        }
    }

    DrawDriveBay(dc, width, height, media, cx, cy,
                 mount ? Track(elapsed, beats.seatAt, beats.spinAt + 200) : 1000, tone);

    // ---- 트랙과 판독 --------------------------------------------------------
    // 한 트랙은 "옮겨 앉기 + 한 번 훑기"다. 판독 진행도는 매체에 칠해지는 것이
    // 아니라 헤드가 닿는 자리에서 차오르는 값이다 - 초당 두 바퀴 도는 판 위에
    // 진행도를 얹으면 얼마나 읽혔는지를 읽을 수 없다.
    int spin = PlatterAngle(elapsed, beats, mount);
    int open = mount ? Track(elapsed, beats.spinAt, beats.readAt - 100) : 1000;
    int visible = !mount || elapsed >= beats.spinAt;
    int firstTrack = mount ? 0 : (gDescentToFloor < 1 ? 0 : gDescentToFloor - 1);
    int headTrack = firstTrack, lastTrack = firstTrack, headRead = 0, settleEase = 1000;

    // A:\ 는 자켓 안에 있다. 자켓을 먼저 세우고, 그 다음 그리는 것 - 판·트랙·
    // 판독·손상 - 을 전부 셔터 창으로 잘라 낸다. 이 클립 하나가 이 볼륨의
    // 실루엣이다: 사각형에 가로로 창이 하나 뚫려 있고 그 안에서만 판이 돈다.
    int windowClip = 0;
    if (visible && media == MEDIA_FLOPPY) {
        DrawFloppyJacket(dc, cx, cy, tone);
        RECT win = FloppyWindow(cx, cy);
        windowClip = SaveDC(dc);
        if (windowClip) IntersectClipRect(dc, win.left, win.top, win.right, win.bottom);
    }
    if (visible)
        DrawMediaBody(dc, media, cx, cy, squash, spin, open,
                      elapsed >= beats.readAt ? Track(elapsed, beats.readAt, beats.lawAt) : 0,
                      elapsed, drop, tone);
    // 링크 볼륨의 고리는 처음부터 끊긴 점선이고 그것을 매체가 직접 그린다.
    // 여기서 이어진 고리를 한 번 더 깔면 "실체가 없다"가 도로 메워진다.
    if (visible && media != MEDIA_LINK) for (int t = 0; t < MOUNT_TRACKS; ++t)
        DrawTrackRun(dc, media, cx, cy, t, 0, 1000, squash, 0, MixColor(C_BG, tone, 16), 1);

    if (elapsed >= beats.readAt) for (int step = 0; step < beats.tracks; ++step) {
        int track = mount ? step : firstTrack + 1;
        if (track >= MOUNT_TRACKS) track = MOUNT_TRACKS - 1;
        int at = beats.readAt + step * trackMs;
        int settle = Track(elapsed, at, at + MOUNT_SETTLE_MS);
        int read = Track(elapsed, at + MOUNT_SETTLE_MS, at + trackMs);
        if (settle <= 0) continue;
        lastTrack = headTrack = track;
        settleEase = EaseOutCubic(settle);
        headRead = read;
        int done = read >= 1000;
        // 다 읽은 트랙은 옅게 남고, 읽는 중인 트랙만 밝다. 접촉이 끊긴 순간에는
        // 그 프레임의 진행도만 사라진다 (값은 그대로 간다 - 다시 붙으면 이어진다).
        if (!drop) {
            DrawTrackRun(dc, media, cx, cy, track, 0, read, squash, 0,
                         MixColor(C_BG, tone, done ? 40 : 72), done ? 2 : 3);
            if (read > 0 && !done) {
                POINT lead = MediaTrackPoint(media, cx, cy, track, read, squash, 0);
                Fill(dc, MakeRect(lead.x - 3, lead.y - 3, lead.x + 4, lead.y + 4), C_TEXT);
            }
        }
        // 손상 섹터는 쓸려 지나간 뒤에야 드러난다 - 미리 붉게 칠해 두면 읽어서
        // 찾아낸 것이 아니라 처음부터 있던 무늬가 된다. 갈린 트랙은 바깥 둘뿐이고
        // 그것이 각각 디스크 손상 A와 B다 - 딱지가 둘인데 갈린 자리가 셋이면
        // 매체 위의 붉은 자리와 옆에 적힌 이름이 서로를 가리키지 않는다.
        for (int k = 0; k < 3 && track < 2; ++k) {
            int span = 0, from = MediaDamage(index, track, k, &span);
            if (from > read) continue;
            int age = elapsed - (at + MOUNT_SETTLE_MS) - (trackMs - MOUNT_SETTLE_MS) * from / 1000;
            int fresh = 1000 - Track(age, 0, 260);
            DrawDamageRun(dc, media, cx, cy, track, from, span, squash, MixColor(C_RED, C_TEXT, 34 * fresh / 1000));
            if (fresh > 0 && FxDecorOn()) {
                POINT hit = MediaTrackPoint(media, cx, cy, track, from + span / 2, squash, 0);
                DrawPixelBurst(dc, hit.x, hit.y, age, 260, FxScale(9), track * 41 + k, C_RED);
            }
        }
    }

    if (windowClip) RestoreDC(dc, windowClip);
    if (visible && media == MEDIA_FLOPPY) DrawFloppyShutter(dc, cx, cy, open, tone);

    // 볼륨 문자. 매체마다 허브가 다른 자리에 있다.
    if (visible && (MediaHubIsPlate(media) || !MediaIsDisc(media) || squash > 420)) {
        POINT hub = MediaHub(media, cx, cy);
        if (MediaHubIsPlate(media)) {
            // 글자가 앉을 허브가 없는 매체들. 기계에 붙은 이름표가 그 자리를
            // 대신한다 - 눕혀 본 판이나 자켓 위에 글자를 얹으면 둘 다 망친다.
            Panel(dc, MakeRect(hub.x - 124, hub.y - 32, hub.x + 124, hub.y + 32),
                  RGB(9, 13, 18), MixColor(C_BG, tone, 54));
            Fill(dc, MakeRect(hub.x - 124, hub.y - 32, hub.x - 120, hub.y + 32), tone);
        } else if (media != MEDIA_LINK) {
            int hy = MediaIsDisc(media) ? PLATTER_HUB * squash / 1000 : PLATTER_HUB;
            FillDisc(dc, hub.x, hub.y, PLATTER_HUB, hy, RGB(9, 13, 18));
            DrawGlowRing(dc, hub.x, hub.y, PLATTER_HUB, hy, MixColor(C_BG, tone, 54), 2);
        }
        TextRect(dc, MakeRect(hub.x - 124, hub.y - 26, hub.x + 124, hub.y + 26),
                 drive->letter, tone, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 헤드. 매체가 깨어나는 동안에는 아직 떠 있고, 판독 직전에 내려앉는다.
    if (visible) {
        int arrive = EaseOutCubic(Track(elapsed, beats.readAt - 300, beats.readAt - 40));
        int prevTrack = mount ? (headTrack > 0 ? headTrack - 1 : 0) : firstTrack;
        int at = MediaHeadProgress(media, headRead);
        POINT a = MediaTrackPoint(media, cx, cy, prevTrack, at, squash, 0);
        POINT b = MediaTrackPoint(media, cx, cy, headTrack, at, squash, 0);
        POINT seat = {Lerp(a.x, b.x, settleEase), Lerp(a.y, b.y, settleEase)};
        POINT park = MediaTrackPoint(media, cx, cy, 0, at, squash, 74);
        POINT head = {Lerp(park.x, seat.x, arrive), Lerp(park.y, seat.y, arrive)};
        if (drop) head.x += 3, head.y -= 2;   // 접촉이 튀는 순간에는 헤드도 흔들린다
        DrawReadHead(dc, media, cx, cy, head, 1000 - arrive, squash, tone);
        if (arrive > 0 && arrive < 1000 && FxDecorOn())
            DrawPulseFrame(dc, MakeRect(seat.x - 10, seat.y - 10, seat.x + 10, seat.y + 10),
                           FxScale(3 + 16 * (1000 - arrive) / 1000), 2, MixColor(C_BG, tone, FxScale(60)));
    }

    // 하강은 이미 돌고 있는 매체로 카메라가 들어오는 것이다. 들어오는 동안만
    // 신호가 걷힌다 - 이것이 없으면 첫 프레임에 매체가 통째로 튀어나온다. 마운트에는
    // 기동 구간이 그 역할을 이미 하고 있다.
    if (!mount && FxDecorOn()) {
        int enter = Track(elapsed, 0, DIVE_IN_MS);
        if (enter < 1000) {
            DrawScreenStatic(dc, MakeRect(0, 68, width, height), elapsed / NOISE_CHURN_MS,
                             FxScale(820) * (1000 - enter) / 1000);
            int y = Lerp(68, height, EaseOutCubic(enter));
            Fill(dc, MakeRect(0, y - 2, width, y + 2), MixColor(C_BG, tone, FxScale(62)));
        }
    }

    // ---- 매체 옆에 붙는 값들 ------------------------------------------------
    // 트랙 하나를 다 읽을 때마다 그 트랙에서 나온 것이 딱지로 붙는다. 목록을
    // 미리 깔아 두지 않는 이유는 하나다 - 읽어서 알아낸 것으로 보여야 한다.
    // 딱지 사이는 한 트랙(620ms) 간격이고, 마지막 딱지 뒤에는 각인과 정지 구간이
    // 1,180ms 더 있다. 네 장이 한꺼번에 쏟아지면 아무것도 읽히지 않는다.
    wchar_t line[192];
    const ModifierInfo* modA = ActiveModifierInfo(gGame.modifierA);
    const ModifierInfo* modB = ActiveModifierInfo(gGame.modifierB);
    const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
    int capacity = EffectiveCapacity(&gGame);
    int tagAt = beats.readAt + trackMs;
    if (mount) {
        POINT aAnchor = MediaTrackPoint(media, cx, cy, 0, 410, squash, 0);
        POINT bAnchor = MediaTrackPoint(media, cx, cy, 1, 440, squash, 0);
        POINT dAnchor = MediaTrackPoint(media, cx, cy, 0, 790, squash, 0);
        POINT pAnchor = MediaTrackPoint(media, cx, cy, 2, 520, squash, 0);
        if (modA) DrawPlatterTag(dc, MakeRect(56, 228, 388, 344), aAnchor.x, aAnchor.y,
                                 Track(elapsed, tagAt, tagAt + 220), modA->name,
                                 MODIFIER_BRIEF[gGame.modifierA], C_RED);
        if (modB) DrawPlatterTag(dc, MakeRect(56, 394, 388, 510), bAnchor.x, bAnchor.y,
                                 Track(elapsed, tagAt + trackMs, tagAt + trackMs + 220), modB->name,
                                 MODIFIER_BRIEF[gGame.modifierB], C_RED);
        if (difficulty) {
            wsprintfW(line, L"오염(관통) 피해 %d%% · 방어도는 절반만 막습니다", difficulty->corruptPercent);
            DrawPlatterTag(dc, MakeRect(width - 388, 228, width - 56, 344), dAnchor.x, dAnchor.y,
                           Track(elapsed, beats.readAt - 40, beats.readAt + 180),
                           difficulty->name, line, (COLORREF)difficulty->color);
        }
        // 마지막 딱지는 안쪽 트랙이 끝나는 자리에 두면 각인과 같은 프레임에 선다.
        // 한 화면에서 가장 큰 두 사건이 겹치면 둘 다 읽히지 않으므로, 헤드가 이
        // 딱지의 앵커를 지나는 순간(트랙 절반)으로 당겨 각인보다 먼저 앉힌다.
        wsprintfW(line, L"%s  ·  한도 %dB", drive->perkText, capacity);
        DrawPlatterTag(dc, MakeRect(width - 388, 394, width - 56, 510), pAnchor.x, pAnchor.y,
                       Track(elapsed, tagAt + trackMs * 2 - 260, tagAt + trackMs * 2 - 40),
                       L"볼륨 특성", line, tone);
    } else {
        POINT anchor = MediaTrackPoint(media, cx, cy, lastTrack, 940, squash, 0);
        int gain = capacity - FLOOR_CAPACITY[gGame.floor > 2 ? 2 : gGame.floor];
        wsprintfW(line, L"용량 한도 %dB → %dB  ·  이 층의 적이 더 강해집니다",
                  FLOOR_CAPACITY[gDescentToFloor - 1] + gain, capacity);
        DrawPlatterTag(dc, MakeRect(width - 388, 292, width - 56, 408), anchor.x, anchor.y,
                       Track(elapsed, beats.readAt + 120, beats.readAt + 360),
                       L"심층 트랙", line, tone);
    }

    // ---- 머리띠와 경로 타이핑 ----------------------------------------------
    // 매체 이름을 여기 적는다. 형태로 이미 갈라져 있지만, 처음 보는 볼륨에서는
    // 그 형태가 무엇인지까지 한 번은 글자로 말해 주는 편이 낫다.
    wsprintfW(line, mount ? L"볼륨 마운트  ·  %s%s  ·  %s" : L"심층 탐색  ·  %s%s  ·  %s",
              drive->letter, drive->label, MediaName(media));
    TextRect(dc, MakeRect(0, 78, width, 108), line, C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
    // 마운트는 볼륨 전체를 여는 것이므로 층 경로 하나가 아니라 카드가 보여 주던
    // 탐색 경로 전체를 친다 (최초 마운트의 paths[0]은 "X:\" 세 글자뿐이라, 그것만
    // 치면 1초 넘게 빈 줄이 남는다). 타이핑은 매체가 깨어날 때 시작해 판독이
    // 끝나는 자리에서 멎는다 - 경로가 완성되는 순간이 곧 각인이다.
    const wchar_t* path = mount ? drive->pathPreview
        : drive->paths[gDescentToFloor > 2 ? 2 : gDescentToFloor];
    int typeFrom = mount ? beats.spinAt : beats.readAt;
    int length = lstrlenW(path), typeMs = beats.lawAt - typeFrom;
    int typed = typeMs > 0 ? (elapsed - typeFrom) * length / typeMs : length;
    if (typed < 0) typed = 0;
    if (typed > length) typed = length;
    wchar_t typedText[96] = L"> ";
    lstrcpynW(typedText + 2, path, typed + 1);
    if (typed < length && ((elapsed / 210) & 1)) lstrcatW(typedText, L"_");
    TextRect(dc, MakeRect(0, 112, width, 142), typedText, tone, gFontMedium, DT_CENTER | DT_SINGLELINE);

    // 매체 아래로 흘러가는 데이터. 판독이 시작된 뒤에만 흐르고, 다 읽으면 멎는다.
    if (FxDecorOn() && elapsed >= beats.readAt) {
        RECT ribbon = MakeRect(cx - 376, height - 86, cx + 376, height - 40);
        Panel(dc, ribbon, RGB(8, 12, 18), MixColor(C_BG, tone, 26));
        RECT inner = MakeRect(ribbon.left + 2, ribbon.top + 2, ribbon.right - 2, ribbon.bottom - 2);
        DrawHexBlock(dc, inner, MixColor(C_BG, tone, FxScale(48)), index * 7 + 1,
                     (uint32_t)(elapsed < beats.lawAt ? elapsed : beats.lawAt), 3);
        DrawScanlines(dc, inner);
    }

    // ---- 각인. 다 읽은 매체에 볼륨 법칙이 박힌다 ---------------------------
    if (mount && elapsed >= beats.lawAt) {
        const DriveLawInfo* law = &DRIVE_LAW_INFO[index];
        int p = Track(elapsed, beats.lawAt, beats.lawAt + 260);
        int h = 116 * EaseOutBack(p) / 1000;
        if (h > 116) h = 116;
        RECT plate = MakeRect(cx - 272, cy - h / 2, cx + 272, cy + h / 2);
        // 각인기가 위아래에서 내려와 문다. 도장이 그냥 나타나면 찍힌 것이 아니라
        // 떠오른 것이 되고, 이 장면에서 가장 큰 한 방이 힘을 잃는다.
        int press = Lerp(150, 0, EaseOutCubic(p));
        for (int side = 0; side < 2; ++side) {
            int y = side ? plate.bottom + press : plate.top - press - 26;
            Fill(dc, MakeRect(cx - 92, y, cx + 92, y + 26), MixColor(C_BG, tone, 30));
            Outline(dc, MakeRect(cx - 92, y, cx + 92, y + 26), MixColor(C_BG, tone, 60), 2);
        }
        Fill(dc, plate, RGB(5, 9, 13));
        Outline(dc, plate, tone, 3);
        if (h >= 96) {
            Text(dc, plate.left + 22, plate.top + 12, L"VOLUME LAW", MixColor(C_BG, tone, 70), gFontSmall);
            TextRect(dc, MakeRect(plate.left + 22, plate.top + 32, plate.right - 22, plate.top + 62),
                     law->name, tone, gFontMedium, DT_LEFT | DT_SINGLELINE);
            TextRect(dc, MakeRect(plate.left + 22, plate.top + 66, plate.right - 22, plate.bottom - 10),
                     law->brief, C_TEXT, gFontSmall, DT_WORDBREAK);
        }
        if (p < 1000 && FxDecorOn()) {
            DrawPulseFrame(dc, plate, FxScale(4 + 26 * (1000 - p) / 1000), 3,
                           MixColor(C_BG, tone, FxScale(85 * (1000 - p) / 1000)));
            DrawPixelBurst(dc, cx, cy, elapsed - beats.lawAt, 300, FxScale(18), index * 23 + 5, tone);
        }
    }

    // ---- 정지. 다 읽은 매체가 한 번 멈춰 선다 -------------------------------
    // 이 구간에는 새로 일어나는 일이 없다. 그것이 목적이다 - 딱지 넷과 법칙이
    // 전부 떠 있는 채로 620ms가 흐르고, 그동안 판독 완료 표시만 하나 선다.
    if (elapsed >= beats.holdAt && elapsed < beats.sealAt) {
        int p = Track(elapsed, beats.holdAt, beats.holdAt + 200);
        RECT done = MakeRect(cx - 132, height - 122, cx + 132, height - 94);
        Panel(dc, done, RGB(8, 13, 18), MixColor(C_BG, C_GREEN, 22 + 38 * p / 1000));
        TextRect(dc, done, mount ? L"판독 완료 · 3/3 트랙" : L"트랙 확보",
                 MixColor(C_BG, C_GREEN, 40 + 55 * p / 1000), gFontSmall,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- 마운트 확정. 걸쇠가 물리고 허브가 화면을 삼킨다 --------------------
    int seal = Track(elapsed, beats.sealAt, beats.total);
    if (seal > 0) {
        POINT hub = MediaCore(media, cx, cy);
        // 바깥에서 안으로 트랙이 차례로 잠긴다. 잠긴 순서가 판독 순서의 반대라
        // 읽던 일이 여기서 되감겨 닫히는 것으로 보인다.
        for (int t = 0; t < MOUNT_TRACKS; ++t) {
            int lit = Track(elapsed, beats.sealAt + t * 60, beats.sealAt + t * 60 + 200);
            if (lit <= 0) continue;
            DrawTrackRun(dc, media, cx, cy, t, 0, 1000, 1000, 0,
                         MixColor(C_BG, tone, 30 + 60 * (1000 - lit) / 1000), lit < 1000 ? 4 : 2);
        }
        int raw = Track(elapsed, beats.sealAt + 130, beats.total);
        int rush = (EaseInCubic(raw) + raw) / 2;   // 곧바로 움직이고 끝에서 터진다
        int stage = rush > 0 ? SaveDC(dc) : 0;
        // 머리띠는 이 연출 내내 남아 있던 틀이다. 마지막 한 동작에서만 그것까지
        // 덮으면 삼켜진 것이 아니라 그리기가 새어 나간 것으로 보인다.
        if (stage) IntersectClipRect(dc, 0, 68, width, height);
        if (rush > 0) {
            int grow = Lerp(PLATTER_HUB, 1560, rush);
            // 매체의 표면이 카메라를 덮친다. 끝에서 흰색에 가까워지는 것은 다음 화면의
            // 도착 연출(DrawSceneArrival)이 밝은 데서 시작해 내려놓기 때문이다 -
            // 여기서 어둡게 닫으면 가장 큰 사건 바로 뒤에 깜빡임이 하나 남는다.
            FillDisc(dc, hub.x, hub.y, grow, grow, MixColor(C_INK, tone, 12 + 46 * rush / 1000));
            // near/far 는 windef.h 가 비어 있는 매크로로 쥐고 있다. 여기서 변수
            // 이름으로 쓰면 선언 자체가 사라져 엉뚱한 자리에서 터진다.
            if (FxDecorOn()) for (int i = 0; i < 18; ++i) {
                POINT rim = PlatterAt(hub.x, hub.y, grow, grow, i * 200 + 40);
                POINT root = PlatterAt(hub.x, hub.y, grow * 62 / 100, grow * 62 / 100, i * 200 + 40);
                DrawLine(dc, root.x, root.y, rim.x, rim.y, MixColor(C_INK, tone, 26 + 40 * rush / 1000), 2);
            }
            DrawGlowRing(dc, hub.x, hub.y, grow, grow, MixColor(C_BG, C_TEXT, 30 + 55 * rush / 1000), 4);
            if (rush > 130)
                TextRect(dc, MakeRect(cx - 340, cy - 44, cx + 340, cy + 44),
                         mount ? L"VOLUME MOUNTED" : L"TRACK LOCKED", C_TEXT, gFontTitle,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        if (stage) RestoreDC(dc, stage);
    }

    const wchar_t* hint = gGame.phase == PHASE_PRUNE
        ? L"진입 후 용량 정리가 필요합니다 · 클릭이나 아무 키로 바로 넘기기"
        : L"잠시 후 전투가 시작됩니다 · 클릭이나 아무 키로 바로 넘기기";
    if (seal < 400) TextRect(dc, MakeRect(0, height - 22, width, height - 2), hint, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// ---- 새 게임 삽입 연출 -----------------------------------------------------
// 새 게임을 누르면 판이 곧장 바뀌지 않는다. 지금 화면이 먼저 띠로 갈라지고, 고리
// 세 개가 조여 오다가, 화면이 소용돌이에 감겨 세 바퀴 돌며 줄어들어 플로피 한
// 장의 라벨이 된다. 그 디스크가 공중에서 한 바퀴 뒤집힌 뒤 책상 위 컴퓨터의 A:
// 드라이브에 꽂힌다. 드라이브가 읽고 모니터가 켜지면 그 화면이 캔버스를 삼키며
// 런으로 넘어간다.
//
// 판은 아직 누르기 직전 그대로다. 여기서 돌리는 그림은 연출이 시작될 때 붙잡아
// 둔 스냅샷이고, 런은 main.cpp의 FinishBootInsert가 끝에서 만든다. 모든 값이
// 경과 ms의 순수 함수라 리페인트가 겹쳐도 같은 프레임이 나온다.
#define BOOT_DISK_W 248
#define BOOT_DISK_H 258
#define BOOT_HOLD_CY 236         // 디스크를 들고 있는 높이
#define BOOT_TOUCH_CY 344        // 셔터가 슬롯 입구에 닿는 높이
#define BOOT_STUCK_CY 424        // 반쯤 들어가 한 번 걸리는 높이
#define BOOT_IN_CY 610           // 다 들어가 보이지 않는 높이
#define BOOT_LABEL_FILL 153      // 라벨을 가득 채우는 스냅샷 배율 (천분율)
#define BOOT_FLOOR_Y 604         // 기계가 서 있는 책상 앞모서리

// 마지막 돌진. 화면만 커지는 것이 아니라 기계 전체가 캔버스에 걸린 배율을 타고
// 덮쳐 오고, 그 브라운관이 캔버스를 넘어설 때 안으로 들어가 있다. 336px짜리
// 화면이 1352px를 채우려면 4.0배면 되는데, 그만큼만 오면 딱 맞춰 멈춘 것으로
// 보인다. 넘겨서 와야 "삼켜졌다"가 된다.
#define BOOT_CLOSE_SCALE 1760    // 벼림 직후
#define BOOT_FLIP_SCALE  2280    // 뒤집기 끝
// 후퇴가 멈추는 자리. 기계 전체(96~604px, 508px)가 1.18배면 600px로 760 캔버스를
// 거의 채운다. 더 빼면 드러나는 것이 아니라 그냥 멀어지는 것이 된다.
#define BOOT_WIDE_SCALE  1180    // 하강 끝. 기계 한 대가 프레임을 채운다
#define BOOT_SLOT_SCALE  1980    // 슬롯 클로즈업
#define BOOT_READ_SCALE  1760    // 판독. 모니터가 프레임을 채운다

// 판독이 끝나는 순간 카메라는 이미 모니터에 붙어 있다(BOOT_READ_SCALE). 돌진은
// 거기서 이어진다 - 예전처럼 1.0배에서 다시 출발하면 붙어 있던 카메라가 한 번
// 튕겨 나갔다 오는 것으로 보인다.
#define BOOT_RUSH_SCALE 5200
#define BOOT_RUSH_BACK 1660      // 덮치기 직전 아주 잠깐 물러난다 (예비 동작)

// 돌진 배율(천분율). 물러났다가 가속해 들어온다.
static int BootRushScale(int zoom) {
    if (zoom <= 0) return BOOT_READ_SCALE;
    if (zoom < 150) return Lerp(BOOT_READ_SCALE, BOOT_RUSH_BACK, zoom * 1000 / 150);
    return Lerp(BOOT_RUSH_BACK, BOOT_RUSH_SCALE, EaseInCubic((zoom - 150) * 1000 / 850));
}

// 슬롯은 캔버스 한가운데(BASE_WIDTH / 2)를 지난다. 디스크도 같은 축으로 내려오므로
// 둘의 중심이 어긋나면 안 된다. 기계는 1120 폭 기준으로 그려져 LEGACY_X만큼 민다.
static RECT BootMonitorRect(int dy) { return MakeRect(LEGACY_X + 368, 96 + dy, LEGACY_X + 752, 390 + dy); }
static RECT BootScreenRect(int dy)  { return MakeRect(LEGACY_X + 392, 120 + dy, LEGACY_X + 728, 366 + dy); }
static RECT BootCaseRect(int dy)    { return MakeRect(LEGACY_X + 356, 424 + dy, LEGACY_X + 764, 604 + dy); }
static RECT BootDriveRect(int dy)   { return MakeRect(LEGACY_X + 396, 448 + dy, LEGACY_X + 724, 540 + dy); }
static RECT BootSlotRect(int dy)    { return MakeRect(LEGACY_X + 412, 466 + dy, LEGACY_X + 700, 506 + dy); }

// ---- 카메라 ---------------------------------------------------------------
// 이 연출에서 가장 크게 달라진 것. 예전에는 마지막 돌진에만 배율이 걸려 있었고
// 앞의 3초는 통째로 고정 시점이었다 - 주인공이 화면의 8분의 1인 채로 절반이
// 정지 화면이었다는 뜻이다. 이제 막마다 카메라가 따로 있다.
//
//   붕괴·소용돌이  시점 없음. 판 자체가 화면이라 들어갈 자리가 없다
//   벼림           디스크로 확 들어간다 - 한 장이 화면을 가득 채운다
//   뒤집기         그 클로즈업 안에서 돈다 (금속 허브·셔터가 크게 보인다)
//   하강           확 물러난다 - 책상·기계·바닥이 한꺼번에 드러난다
//   삽입           슬롯으로 파고든다
//   판독           슬롯에서 모니터로 올라간다. 화면이 프레임을 채운다
//   돌진           그 자리에서 화면 속으로
//
// 기계가 아래에서 올라오던 예전 동작은 없앴다. 기계는 처음부터 제자리에 있고,
// 그것을 드러내는 일은 카메라가 물러나는 것 하나로 한다.
struct BootCamera { int scale, cx, cy; };   // 천분율, 그리고 화면 한가운데로 올 캔버스 좌표

static int BootRushScale(int zoom);

static BootCamera BootCameraAt(int t) {
    BootCamera cam = {1000, BASE_WIDTH / 2, BASE_HEIGHT / 2};
    if (t < BOOT_FLIP_AT) return cam;
    RECT slot = BootSlotRect(0), screen = BootScreenRect(0);
    int slotCx = (slot.left + slot.right) / 2, slotCy = (slot.top + slot.bottom) / 2;
    int screenCx = (screen.left + screen.right) / 2, screenCy = (screen.top + screen.bottom) / 2;
    int machineCy = (BootMonitorRect(0).top + BootCaseRect(0).bottom) / 2;
    if (t < BOOT_FLY_AT) {
        // 벼림의 충격으로 확 들어갔다가, 뒤집는 동안 계속 천천히 민다.
        int snap = EaseOutCubic(Track(t, BOOT_FLIP_AT, BOOT_FLIP_AT + 130));
        int creep = Track(t, BOOT_FLIP_AT + 130, BOOT_FLY_AT);
        cam.scale = creep > 0 ? Lerp(BOOT_CLOSE_SCALE, BOOT_FLIP_SCALE, creep)
                              : Lerp(1000, BOOT_CLOSE_SCALE, snap);
        cam.cy = Lerp(BASE_HEIGHT / 2, BOOT_HOLD_CY, snap);
        return cam;
    }
    if (t < BOOT_PUSH_AT) {
        // 후퇴. 이 연출에서 공간이 생기는 유일한 순간이라 가장 빠르게 뺀다.
        int back = EaseOutCubic(Track(t, BOOT_FLY_AT, BOOT_PUSH_AT));
        cam.scale = Lerp(BOOT_FLIP_SCALE, BOOT_WIDE_SCALE, back);
        cam.cy = Lerp(BOOT_HOLD_CY, machineCy, back);
        return cam;
    }
    if (t < BOOT_CLUNK_AT) {
        int raw = Track(t, BOOT_PUSH_AT, BOOT_CLUNK_AT);
        int in = (EaseOutCubic(raw) + raw) / 2;   // 누르자마자 움직이고 끝에서 감속한다
        cam.scale = Lerp(BOOT_WIDE_SCALE, BOOT_SLOT_SCALE, in);
        cam.cx = Lerp(BASE_WIDTH / 2, slotCx, in);
        cam.cy = Lerp(machineCy, slotCy, in);
        return cam;
    }
    if (t < BOOT_SEEK_END) {
        // 철컥을 슬롯에서 한 박자 보고 나서 모니터로 올라간다.
        int lift = EaseOutCubic(Track(t, BOOT_CLUNK_AT + 180, BOOT_SEEK_END - 60));
        cam.scale = Lerp(BOOT_SLOT_SCALE, BOOT_READ_SCALE, lift);
        cam.cx = Lerp(slotCx, screenCx, lift);
        cam.cy = Lerp(slotCy, screenCy, lift);
        return cam;
    }
    cam.scale = BootRushScale(Track(t, BOOT_SEEK_END, BOOT_INSERT_MS));
    cam.cx = screenCx; cam.cy = screenCy;
    return cam;
}

// 카메라를 DC에 건다. 캔버스는 이미 MM_ANISOTROPIC(창 BASE, 뷰포트 device)이므로
// 뷰포트 배율만 곱하고 원점을 옮기면 그리는 쪽은 아무것도 몰라도 된다.
// 반환값은 RestoreDC에 줄 손잡이다 (0이면 걸지 않았다).
static int BootCameraPush(HDC dc, int deviceW, int deviceH, const BootCamera& cam) {
    if (cam.scale == 1000 && cam.cx == BASE_WIDTH / 2 && cam.cy == BASE_HEIGHT / 2) return 0;
    int saved = SaveDC(dc);
    SetWindowOrgEx(dc, cam.cx, cam.cy, 0);
    SetViewportOrgEx(dc, deviceW / 2, deviceH / 2, 0);
    SetViewportExtEx(dc, deviceW * cam.scale / 1000, deviceH * cam.scale / 1000, 0);
    return saved;
}

// FxSnapshotSpin은 DC 변환을 쓰지 않고 자기가 직접 장치 좌표를 셈한다 (돌리는
// 경로가 MM_TEXT여야 해서 그렇다). 그래서 카메라가 걸린 자리는 여기서 미리 옮긴다.
static int BootCamX(const BootCamera& cam, int x) { return BASE_WIDTH / 2 + (x - cam.cx) * cam.scale / 1000; }
static int BootCamY(const BootCamera& cam, int y) { return BASE_HEIGHT / 2 + (y - cam.cy) * cam.scale / 1000; }

// 플로피 한 장의 자리. 배율(천분율)이 몸통·라벨·셔터에 같은 비율로 걸리므로
// 그리기와 라벨 클립이 언제나 같은 사각형을 본다. squeeze는 가로만 누른다 -
// 세로축을 중심으로 도는 뒤집기가 그것으로 표현된다.
struct BootDisk { RECT body, label, shutter; };

static BootDisk BootDiskAt(int cx, int cy, int scaleMille, int squeezeMille) {
    int w = BOOT_DISK_W * scaleMille / 1000 * squeezeMille / 1000;
    int h = BOOT_DISK_H * scaleMille / 1000;
    BootDisk disk;
    disk.body = MakeRect(cx - w / 2, cy - h / 2, cx - w / 2 + w, cy - h / 2 + h);
    int inset = 18 * scaleMille / 1000 * squeezeMille / 1000;
    disk.label = MakeRect(disk.body.left + inset, disk.body.top + 20 * scaleMille / 1000,
                          disk.body.right - inset, disk.body.top + 130 * scaleMille / 1000);
    int sw = 108 * scaleMille / 1000 * squeezeMille / 1000;
    disk.shutter = MakeRect(cx - sw / 2, disk.body.bottom - 56 * scaleMille / 1000,
                            cx - sw / 2 + sw, disk.body.bottom - 10 * scaleMille / 1000);
    return disk;
}

// 셔터가 아래에 있다. 슬롯에 먼저 들어가는 쪽이라 그렇게 들고 있는 것이 맞다.
// back이면 라벨 대신 금속 허브가 보이는 뒷면이다. shine은 몸통을 가로지르는
// 반사광의 위치(천분율)이고, 0보다 작으면 반사광이 없다 - 뒤집는 동안에만
// 켜서 플라스틱 판이 빛을 받는 각도가 바뀌는 것을 보여 준다.
static void DrawBootFloppy(HDC dc, const BootDisk& disk, int back, int shine) {
    int w = disk.body.right - disk.body.left, h = disk.body.bottom - disk.body.top;
    if (w < 6 || h < 6) return;
    // 기계 앞을 지날 때 디스크가 묻히지 않도록 그림자를 두 겹 깔고 몸통을 여러
    // 단 밝게 쓴다. 뒤가 어두운 판이라 이 대비가 없으면 앞뒤가 읽히지 않는다.
    Fill(dc, MakeRect(disk.body.left + 22, disk.body.top + 26, disk.body.right + 22, disk.body.bottom + 26), RGB(2, 3, 5));
    Fill(dc, MakeRect(disk.body.left + 11, disk.body.top + 13, disk.body.right + 11, disk.body.bottom + 13), RGB(4, 6, 9));
    Fill(dc, disk.body, back ? RGB(34, 42, 54) : RGB(48, 59, 74));
    // 위·왼쪽은 빛을 받고 아래·오른쪽은 떨어진다. 판이 두께를 가진 물건으로 보인다.
    Fill(dc, MakeRect(disk.body.left, disk.body.top, disk.body.right, disk.body.top + 2), RGB(154, 176, 194));
    if (w > 8) Fill(dc, MakeRect(disk.body.left, disk.body.top, disk.body.left + 2, disk.body.bottom), RGB(104, 124, 142));
    Fill(dc, MakeRect(disk.body.left, disk.body.bottom - 3, disk.body.right, disk.body.bottom), RGB(12, 17, 23));
    if (w > 10) Fill(dc, MakeRect(disk.body.right - 3, disk.body.top, disk.body.right, disk.body.bottom), RGB(16, 22, 30));
    if (h > 30) Fill(dc, MakeRect(disk.body.left + 4, disk.body.top + 4, disk.body.right - 4, disk.body.top + 5), RGB(76, 94, 110));

    // 셔터. 슬롯에 먼저 닿는 쪽이라 가장 밝은 금속으로 둔다.
    Fill(dc, disk.shutter, RGB(178, 190, 202));
    if (disk.shutter.right - disk.shutter.left > 12) {
        Fill(dc, MakeRect(disk.shutter.left, disk.shutter.top, disk.shutter.right, disk.shutter.top + 2), RGB(226, 234, 240));
        Fill(dc, MakeRect(disk.shutter.left + 4, disk.shutter.top + 4,
                          (disk.shutter.left + disk.shutter.right) / 2 - 2, disk.shutter.bottom - 4), RGB(20, 26, 34));
    }

    if (back) {
        // 뒷면. 금속 허브와 자기면을 누르는 스프링 자국이 보인다.
        int hubW = w / 5, hubH = h / 9;
        int mx = (disk.body.left + disk.body.right) / 2, my = (disk.body.top + disk.body.bottom) / 2;
        for (int i = 1; i <= 3; ++i)
            Fill(dc, MakeRect(disk.body.left + 8, disk.body.top + h * i / 9, disk.body.right - 8, disk.body.top + h * i / 9 + 1), RGB(52, 64, 78));
        if (hubW > 2 && hubH > 2) {
            Fill(dc, MakeRect(mx - hubW, my - hubH, mx + hubW, my + hubH), RGB(140, 152, 166));
            Fill(dc, MakeRect(mx - hubW / 2, my - hubH / 2, mx + hubW / 2, my + hubH / 2), RGB(18, 24, 31));
        }
    } else {
        if (w >= 140) {
            // 쓰기 방지 구멍 두 개. 아래 모서리에서 셔터를 사이에 둔다.
            Fill(dc, MakeRect(disk.body.left + 12, disk.body.bottom - 34, disk.body.left + 28, disk.body.bottom - 18), RGB(4, 6, 9));
            Fill(dc, MakeRect(disk.body.right - 28, disk.body.bottom - 34, disk.body.right - 12, disk.body.bottom - 18), RGB(4, 6, 9));
        }
        Panel(dc, disk.label, RGB(9, 14, 19), RGB(192, 210, 222));
        if (w >= 170) {
            Text(dc, disk.label.left + 2, disk.label.bottom + 10, L"A:\\ROGUE", C_GREEN, gFontSmall);
            Text(dc, disk.label.left + 2, disk.label.bottom + 32, L"2HD", C_DIM, gFontSmall);
        }
    }
    // 반사광. 몸통 안에서만 지나가도록 잘라 낸다.
    if (shine >= 0 && shine <= 1000 && w > 16) {
        int band = w / 9; if (band < 3) band = 3;
        int x = disk.body.left + (w + band * 2) * shine / 1000 - band;
        int saved = SaveDC(dc);
        IntersectClipRect(dc, disk.body.left + 2, disk.body.top + 2, disk.body.right - 3, disk.body.bottom - 3);
        Fill(dc, MakeRect(x, disk.body.top, x + band, disk.body.bottom), RGB(122, 144, 164));
        Fill(dc, MakeRect(x + band / 3, disk.body.top, x + band * 2 / 3, disk.body.bottom), RGB(178, 200, 216));
        RestoreDC(dc, saved);
    }
}

// 책상과 그 앞으로 뻗은 바닥. 예전에는 기계 아래가 통째로 빈 검정이라 화면의
// 절반이 죽어 있었다. 소실점으로 모이는 격자를 깔면 기계가 어딘가에 놓인
// 물건이 되고, 모니터가 켜질 때 그 빛이 떨어질 자리도 생긴다.
// glow 0~1000 = 모니터가 뿜는 빛의 세기.
// area는 이 바닥이 덮어야 하는 세상의 범위다. 카메라가 물러나면 캔버스보다
// 넓어지므로 폭을 받아야 한다. 소실점은 언제나 기계가 선 자리(BASE_WIDTH/2)다.
static void DrawBootFloor(HDC dc, const RECT& area, int dy, int glow) {
    int floorY = BOOT_FLOOR_Y + dy, height = area.bottom;
    if (floorY >= height) return;
    Fill(dc, MakeRect(area.left, floorY, area.right, height), RGB(5, 8, 11));
    // 모니터에서 쏟아지는 빛기둥. 기계보다 먼저 그려 본체 뒤로 지나간다.
    if (glow > 0) {
        RECT screen = BootScreenRect(dy);
        POINT cone[4] = {
            {screen.left + 26, floorY}, {screen.right - 26, floorY},
            {screen.right + 210, height}, {screen.left - 210, height}
        };
        HBRUSH brush = CreateSolidBrush(MixColor(RGB(5, 8, 11), C_GREEN, 3 + glow * 7 / 1000));
        HBRUSH oldBrush = (HBRUSH)SelectObject(dc, brush);
        HPEN oldPen = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
        Polygon(dc, cone, 4);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
    }
    // 소실점으로 모이는 세로선. 가운데가 밝고 바깥으로 갈수록 사라진다.
    for (int i = -11; i <= 11; ++i) {
        int away = i < 0 ? -i : i;
        int tone = 13 + glow * 9 / 1000 - away;
        if (tone < 3) tone = 3;
        DrawLine(dc, BASE_WIDTH / 2 + i * 44, floorY, BASE_WIDTH / 2 + i * 230, height,
                 MixColor(RGB(5, 8, 11), C_GREEN, tone), 1);
    }
    // 가로선. 앞으로 올수록 간격이 벌어져 바닥이 눕는다.
    for (int j = 1; j <= 6; ++j) {
        int p = j * 1000 / 6;
        int y = floorY + (height - floorY) * (p * p / 1000) / 1000;
        DrawLine(dc, area.left, y, area.right, y, MixColor(RGB(5, 8, 11), C_GREEN, 8 + glow * 6 / 1000), 1);
    }
    // 책상 앞모서리와 기계 발치의 빛 웅덩이.
    Fill(dc, MakeRect(area.left, floorY, area.right, floorY + 2), MixColor(RGB(5, 8, 11), C_GREEN, 16 + glow * 22 / 1000));
    RECT machine = BootCaseRect(dy);
    for (int i = 0; i < 5; ++i) {
        int spread = 30 + i * 46;
        Fill(dc, MakeRect(machine.left - spread, floorY + 2 + i * 3, machine.right + spread, floorY + 5 + i * 3),
             MixColor(RGB(5, 8, 11), C_GREEN, (14 + glow * 20 / 1000) * (5 - i) / 6));
    }
}

// 모니터와 본체. dy는 아래에서 올라오는 동안의 남은 거리다.
// glow 0~1000 = 화면이 켜진 정도. 켜지면 케이스 모서리에도 그 빛이 앉는다.
static void DrawBootMachine(HDC dc, int dy, int inserted, int t, int glow) {
    RECT monitor = BootMonitorRect(dy), screen = BootScreenRect(dy);
    RECT machine = BootCaseRect(dy), drive = BootDriveRect(dy), slot = BootSlotRect(dy);
    // 켜진 화면이 뿜는 후광. 모니터 바깥으로 몇 겹 번진다.
    if (glow > 0)
        for (int i = 9; i >= 1; --i) {
            // 바깥으로 갈수록 제곱으로 옅어진다. 일정 간격으로 줄이면 테두리가
            // 몇 겹 겹친 액자로 보이지 후광으로 보이지 않는다.
            int amount = glow * 15 / 1000 * ((10 - i) * (10 - i)) / 81;
            if (amount <= 0) continue;
            RECT halo = monitor; InflateRect(&halo, i * 5, i * 5);
            Outline(dc, halo, MixColor(C_BG, C_GREEN, amount), 2);
        }
    Panel(dc, monitor, RGB(21, 28, 36), MixColor(RGB(64, 82, 96), C_GREEN, glow / 24));
    Fill(dc, MakeRect(monitor.left + 6, monitor.top + 6, monitor.right - 6, monitor.top + 8), RGB(44, 58, 70));
    Outline(dc, MakeRect(screen.left - 5, screen.top - 5, screen.right + 5, screen.bottom + 5), RGB(40, 53, 64), 2);
    Fill(dc, screen, RGB(5, 9, 12));
    // 꺼진 브라운관에도 빛은 조금 비친다. 사선으로 흐르는 점이 유리라는 것을 알려 준다.
    if (!inserted)
        for (int i = 0; i < 34; ++i) {
            int x = screen.left + 22 + i * 6, y = screen.top + 24 + i * 6;
            Fill(dc, MakeRect(x, y, x + 3, y + 3), RGB(12, 18, 24));
        }
    // 목과 받침.
    Fill(dc, MakeRect(LEGACY_X + 524, 390 + dy, LEGACY_X + 596, 414 + dy), RGB(26, 34, 43));
    Fill(dc, MakeRect(LEGACY_X + 486, 412 + dy, LEGACY_X + 634, 424 + dy), RGB(21, 28, 36));
    Panel(dc, machine, RGB(19, 25, 32), MixColor(RGB(60, 76, 90), C_GREEN, glow / 30));
    Fill(dc, MakeRect(machine.left + 8, machine.top + 8, machine.right - 8, machine.top + 10), RGB(36, 47, 58));
    for (int x = drive.left; x < drive.right - 8; x += 10)
        Fill(dc, MakeRect(x, drive.bottom + 16, x + 4, machine.bottom - 16), RGB(13, 18, 24));
    // 3.5인치 드라이브. 슬롯은 안이 보이지 않는 검은 틈이라 디스크가 그리로 사라진다.
    Panel(dc, drive, RGB(28, 36, 45), RGB(50, 65, 78));
    Fill(dc, MakeRect(drive.left + 6, drive.top + 6, drive.right - 6, drive.top + 7), RGB(44, 57, 69));
    Fill(dc, slot, RGB(3, 5, 7));
    Outline(dc, slot, RGB(78, 96, 112), 1);
    Fill(dc, MakeRect(slot.left + 5, slot.top + 3, slot.right - 5, slot.top + 7), RGB(15, 21, 28));
    // 꺼냄 단추는 디스크가 물리는 순간 튀어나온다.
    Fill(dc, MakeRect(slot.right + 10, slot.top + 7, slot.right + 34 + (inserted ? 7 : 0), slot.bottom - 7), RGB(56, 71, 84));

    // 디스크가 오기 전의 드라이브는 꺼진 것이 아니라 기다리는 중이다. 대기등이
    // 호박색으로 숨 쉬고, 판이 가까워질수록 빨라진다. 예전에는 물릴 때까지 이
    // 앞판이 통째로 죽어 있어서, 판이 내려오는 700ms 동안 화면에서 빛나는 것이
    // 하나도 없었다 - 이 연출에서 가장 조용한 구간이 거기였던 이유다.
    int waiting = !inserted && t >= BOOT_FLIP_AT;
    int ready = Track(t, BOOT_FLY_AT, BOOT_CLUNK_AT);
    int beat = waiting && ((t / (250 - ready * 150 / 1000)) & 1);
    if (waiting) {
        int lit = 6 + ready * 13 / 1000 + (beat ? 4 : 0);
        Outline(dc, MakeRect(slot.left - 4, slot.top - 4, slot.right + 4, slot.bottom + 4),
                MixColor(RGB(28, 36, 45), C_YELLOW, lit), 2);
        // 꺼진 브라운관 유리에 그 빛이 아래쪽만 비친다. 큰 검은 사각형에 방향이 생긴다.
        Fill(dc, MakeRect(screen.left + 30, screen.bottom - 6, screen.right - 30, screen.bottom - 3),
             MixColor(RGB(5, 9, 12), C_YELLOW, 4 + ready * 7 / 1000));
    }
    int led = inserted && ((t / 80) & 1);
    COLORREF lamp = led ? C_GREEN : beat ? C_YELLOW : RGB(26, 40, 36);
    Fill(dc, MakeRect(slot.left + 8, slot.bottom + 14, slot.left + 30, slot.bottom + 26), lamp);
    if (led || beat)
        Outline(dc, MakeRect(slot.left + 5, slot.bottom + 11, slot.left + 33, slot.bottom + 29),
                MixColor(C_BG, led ? C_GREEN : C_YELLOW, 40), 1);
    Text(dc, slot.left + 40, slot.bottom + 10, L"A:", led ? C_GREEN : beat ? C_YELLOW : C_DIM, gFontSmall);
}

// 브라운관은 한꺼번에 켜지지 않는다. 가로 한 줄이 먼저 서고, 그 줄이 위아래로
// 열리면서 화면이 된다. 열리는 동안 한가운데에 과하게 밝은 심지가 남는다.
// 예전에는 디스크가 물리자마자 글자가 떠 있었다 - 그러면 켜진 것이 아니라
// 처음부터 켜져 있던 것이 된다.
static void DrawBootPowerOn(HDC dc, const RECT& screen, int p) {
    int cy = (screen.top + screen.bottom) / 2, half = (screen.bottom - screen.top) / 2;
    Fill(dc, screen, RGB(3, 6, 8));
    int open = EaseOutCubic(p);
    int h = half * open / 1000; if (h < 1) h = 1;
    Fill(dc, MakeRect(screen.left, cy - h, screen.right, cy + h),
         MixColor(RGB(6, 13, 11), C_GREEN, 34 - p * 24 / 1000));
    int core = 3 + (1000 - p) * 6 / 1000;
    Fill(dc, MakeRect(screen.left, cy - core, screen.right, cy + core),
         MixColor(RGB(6, 13, 11), RGB(214, 255, 240), 96 - p * 84 / 1000));
    // 가로로도 눌려 있다 펴진다. 전자총이 자리를 잡는 동안의 찌그러짐이다.
    int pinch = (1000 - open) * (screen.right - screen.left) * 3 / 10000;
    if (pinch > 0) {
        Fill(dc, MakeRect(screen.left, screen.top, screen.left + pinch, screen.bottom), RGB(3, 6, 8));
        Fill(dc, MakeRect(screen.right - pinch, screen.top, screen.right, screen.bottom), RGB(3, 6, 8));
    }
    DrawScanlines(dc, screen);
}

// 브라운관이 켜진 뒤의 모니터. 판독이 진행될수록 노이즈가 걷히고 줄이 하나씩 는다.
static void DrawBootScreenText(HDC dc, const RECT& screen, int t) {
    static const wchar_t* const BOOT_LINES[6] = {
        L"A:\\> DIR",
        L"ROGUE    EXE      1,440,000",
        L"        1 file(s)    1,440,000 bytes",
        L"A:\\> ROGUE",
        L"섹터 검증 완료 · 커널 적재",
        L"제어권 이양 · A:\\ROGUE.EXE"
    };
    static const wchar_t SPIN[4] = {L'|', L'/', L'-', L'\\'};
    int power = Track(t, BOOT_CLUNK_AT, BOOT_CLUNK_AT + BOOT_POWER_MS);
    if (power < 1000) { DrawBootPowerOn(dc, screen, power); return; }
    int seek = Track(t, BOOT_CLUNK_AT + BOOT_POWER_MS, BOOT_SEEK_END);
    Fill(dc, screen, RGB(6, 13, 11));
    DrawSectorStatic(dc, screen, 7, t / NOISE_CHURN_MS, 380 - seek * 330 / 1000);
    int shown = seek * 7 / 1000;
    if (shown > 6) shown = 6;
    for (int i = 0; i < shown; ++i)
        Text(dc, screen.left + 16, screen.top + 12 + i * 22, BOOT_LINES[i], i >= 4 ? C_GREEN : C_TEXT, gFontSmall);
    // 헤드가 트랙을 옮길 때마다 화면이 한 번씩 튄다.
    if (FxDecorOn() && (seek / 140) % 3 == 0) {
        int band = screen.top + (int)(Hash3(seek / 140, 3, 1) % (uint32_t)(screen.bottom - screen.top - 10));
        Fill(dc, MakeRect(screen.left, band, screen.right, band + 3), MixColor(RGB(6, 13, 11), C_GREEN, 30));
    }
    // 18개 섹터가 왼쪽부터 잠긴다. 타이틀 화면의 복구 진행도와 같은 눈금이라,
    // 지금 읽히고 있는 것이 그 판이라는 것을 한 줄로 말한다.
    int cell = (screen.right - screen.left - 32 - 17 * 3) / 18;
    int rowY = screen.bottom - 88;
    for (int i = 0; i < 18; ++i) {
        int x = screen.left + 16 + i * (cell + 3);
        int age = seek - i * 44;
        Fill(dc, MakeRect(x, rowY, x + cell, rowY + 8),
             MixColor(RGB(6, 13, 11), C_GREEN, age > 0 ? 72 : 10));
        // 막 잠긴 칸만 한 번 크게 뜬다. 없으면 18칸이 동시에 채워지는 것처럼 보인다.
        if (age > 0 && age < 150)
            Fill(dc, MakeRect(x, rowY - 2, x + cell, rowY + 10),
                 MixColor(RGB(6, 13, 11), RGB(214, 255, 240), 80 * (150 - age) / 150));
    }

    // 메모리 검사는 숫자가 올라가는 동안이 재미다.
    wchar_t line[64];
    wsprintfW(line, L"메모리 %dK  %c", 640 * seek / 1000, SPIN[(t / 80) % 4]);
    Text(dc, screen.left + 16, screen.bottom - 62, line, C_YELLOW, gFontSmall);
    Bar(dc, MakeRect(screen.left + 16, screen.bottom - 34, screen.right - 16, screen.bottom - 18), seek, 1000, C_GREEN);
    DrawScanlines(dc, screen);
}

// 사건이 식어 가는 색. 0 = 흰 열, 500 = 호박, 1000 = 이 게임의 인광 녹색.
// 큰 사건 넷(과전압·벼림·철컥·삼킴)만 이 축을 쓴다. 새 색을 들이는 것이 아니라
// 전투가 이미 쓰는 노랑을 온도로 한 번 더 쓰는 것이다 - 전부 같은 밝기의 녹색
// 하나로만 그리면 무엇이 큰 사건인지 색으로는 구별되지 않는다.
static COLORREF BootHeat(int p) {
    if (p < 0) p = 0; else if (p > 1000) p = 1000;
    return p < 500 ? MixColor(RGB(255, 244, 214), C_YELLOW, p * 100 / 500)
                   : MixColor(C_YELLOW, C_GREEN, (p - 500) * 100 / 500);
}

// 중심으로 빨려 드는 불티. p(0~1000)가 커질수록 안쪽으로 모이고 꼬리는 바깥에
// 남는다. DrawPixelBurst의 반대다 - 그쪽은 터져 나가고 이쪽은 모여든다.
static void DrawBootSparks(HDC dc, int cx, int cy, int p, int count, int seed, int reach, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    for (int i = 0; i < count; ++i) {
        uint32_t h = Hash3(i, seed, 29);
        int local = p + (int)(h % 420u);          // 불티마다 시차를 준다
        if (local >= 1000) continue;
        int e = EaseInCubic(local);
        int radius = 40 + reach * (1000 - e) / 1000;
        int deg = (int)((h >> 8) % 3600u);
        int tail = radius + 60 + (int)(h % 90u);
        // 세로는 3/4로 눌러 둔다. 소용돌이·고리와 같은 원근이라야 한 사건으로 읽힌다.
        MoveToEx(dc, cx + tail * CosMille(deg) / 1000, cy + tail * SinMille(deg) * 3 / 4000, 0);
        LineTo(dc, cx + radius * CosMille(deg) / 1000, cy + radius * SinMille(deg) * 3 / 4000);
    }
    SelectObject(dc, oldPen); DeleteObject(pen);
}

// 벼려지는 순간 튀는 파편. 사방으로 뻗다가 중력에 눌려 아래로 휜다. 점이 아니라
// 옆으로 긴 조각이라 무엇이 어느 쪽으로 날고 있는지가 보인다.
static void DrawBootShards(HDC dc, int cx, int cy, int t, int life, int count, int seed, COLORREF color) {
    if (t < 0 || t >= life) return;
    int p = t * 1000 / life;
    for (int i = 0; i < count; ++i) {
        uint32_t h = Hash3(i, seed, 47);
        int deg = (int)(h % 3600u);
        int dist = (320 + (int)((h >> 9) % 420u)) * EaseOutCubic(p) / 1000;
        int x = cx + dist * CosMille(deg) / 1000;
        int y = cy + dist * SinMille(deg) * 3 / 4000 + p * p / 1000 * 210 / 1000;
        int size = 7 - p * 5 / 1000; if (size < 2) size = 2;
        Fill(dc, MakeRect(x, y, x + size + (int)(h & 3u) * 2, y + size), MixColor(color, C_BG, p / 11));
    }
}

// 덮쳐 오는 브라운관의 안쪽. 카메라 안에서 세상 좌표로 그리므로 화면이 커지는
// 만큼 글자도 주사선도 같이 커진다. 예전에는 이 자리만 배율 밖에서 캔버스 좌표로
// 그렸다 - 화면은 덮쳐 오는데 그 안의 "A:\ROGUE"만 제자리 크기로 남아 가운데로
// 옮겨 다녔고, 그래서 다가오는 것이 아니라 큰 사진 위에 글자를 얹은 것으로 보였다.
static void DrawBootDive(HDC dc, const RECT& screen, int t, int zoom) {
    Fill(dc, screen, RGB(6, 13, 11));
    DrawSectorStatic(dc, screen, 9, t / NOISE_CHURN_MS, 90 + zoom * 320 / 1000);
    // 지나쳐 흐르는 가로 조각. 바깥의 사각 테두리만으로는 화면 가장자리만 빠르고
    // 한가운데는 정지해 있다 - 속도는 안쪽에도 있어야 한다.
    if (FxDecorOn()) {
        int cx = (screen.left + screen.right) / 2, cy = (screen.top + screen.bottom) / 2;
        int half = (screen.bottom - screen.top) / 2;
        for (int i = 0; i < 28; ++i) {
            uint32_t h = Hash3(i, 77, 5);
            int p = (zoom * 2 + (int)(h % 1000u)) % 1000;
            int reach = 30 + EaseInCubic(p) * 260 / 1000;
            int y = cy - half + (int)((h >> 7) % (uint32_t)(half * 2));
            int w = 6 + p * 30 / 1000;
            int x = cx + ((h & 1u) ? reach : -reach - w);
            Fill(dc, MakeRect(x, y, x + w, y + 2),
                 MixColor(RGB(6, 13, 11), C_GREEN, 54 - p * 42 / 1000));
        }
    }
    // 18개 섹터가 위에서부터 한 줄씩 기록된다. 잠기는 순간 그 줄이 옆으로 어긋난
    // 채 밝게 떴다가 제자리에 앉고, 다 앉으면 화면이 가로줄로 가득 찬다.
    //
    // 예전에는 이 18칸을 브라운관 테두리에 점으로 둘렀는데, 사각형 둘레에 같은
    // 간격으로 놓인 밝은 점은 섹터가 아니라 극장 간판 전구로 읽혔다. 판이 기록되는
    // 것은 화면 둘레가 아니라 화면 안에서 일어나야 하고, 그래야 다가오는 그림을
    // 가리지도 않는다. 칸 사이는 44ms에서 12ms로 좁아지며 SFX_BOOT_LOCK의 딸깍이
    // 같은 식을 쓰므로, 한 줄이 앉는 순간과 그 소리가 같은 프레임에 있다.
    int lockMs = t - BOOT_SEEK_END, tall = screen.bottom - screen.top;
    for (int i = 0; i < 18; ++i) {
        int age = lockMs - (44 * i - i * (i - 1));
        if (age <= 0) continue;
        int y = screen.top + tall * (i * 2 + 1) / 36;
        // 막 앉은 줄만 옆으로 어긋나 있다가 120ms에 걸쳐 제자리로 온다.
        int slip = age < 120 ? (120 - age) * 24 / 120 : 0;
        if (i & 1) slip = -slip;
        int left = screen.left + slip, right = screen.right + slip;
        if (left < screen.left) left = screen.left;
        if (right > screen.right) right = screen.right;
        int lit = age < 120 ? 58 + (120 - age) * 38 / 120 : 30;
        Fill(dc, MakeRect(left, y, right, y + 2),
             MixColor(RGB(6, 13, 11), age < 120 ? BootHeat(age * 1000 / 120) : C_GREEN, lit));
        // 앉는 순간의 잔광. 위아래로 한 겹 번졌다 사라진다.
        if (age < 120)
            Fill(dc, MakeRect(left, y - 3, right, y + 5),
                 MixColor(RGB(6, 13, 11), BootHeat(age * 1000 / 120), 22 * (120 - age) / 120));
    }
    TextRect(dc, screen, L"A:\\ROGUE", C_GREEN, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawScanlines(dc, screen);
}

// 화면을 감아 삼키는 소용돌이. 조각을 점이 아니라 선으로 그으면 어느 쪽으로
// 얼마나 빨리 빨려 드는지가 한눈에 보인다. 펜을 한 번만 만들어 전부 긋는다.
static void DrawBootVortex(HDC dc, int cx, int cy, int suck) {
    // 감길수록 밝아진다. 고정 밝기로 두면 판이 커서 아무것도 안 보이는 앞 절반과
    // 판이 작아 배경이 다 드러나는 뒤 절반이 똑같이 흐리다.
    HPEN pen = CreatePen(PS_SOLID, 2, MixColor(C_BG, C_GREEN, 46 + suck * 54 / 1000));
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    for (int i = 0; i < 56; ++i) {
        uint32_t h = Hash3(i, 21, 7);
        int local = suck + (int)(h % 300u);          // 조각마다 시차를 준다
        if (local >= 1000) continue;
        int p = EaseInCubic(local);
        int radius = (300 + (int)(h % 360u)) * (1000 - p) / 1000;
        int deg = (int)((h >> 7) % 3600u) + p * 28;  // 감기면서 돈다
        // 꼬리는 조금 전에 지나온 자리다 - 바깥이면서 회전 방향의 뒤쪽. 길게
        // 늘이면 화면을 가로지르는 긁힌 자국으로 보이므로 짧게 끊는다.
        int tail = radius * 112 / 100;
        int x0 = cx + radius * CosMille(deg) / 1000;
        int y0 = cy + radius * SinMille(deg) * 3 / 4000;
        int x1 = cx + tail * CosMille(deg - 55) / 1000;
        int y1 = cy + tail * SinMille(deg - 55) * 3 / 4000;
        MoveToEx(dc, x1, y1, 0); LineTo(dc, x0, y0);
    }
    SelectObject(dc, oldPen); DeleteObject(pen);
    // 선 사이를 메우는 작은 조각들.
    for (int i = 0; i < 48; ++i) {
        uint32_t h = Hash3(i, 55, 13);
        int local = suck + (int)(h % 360u);
        if (local >= 1000) continue;
        int p = EaseInCubic(local);
        int radius = (260 + (int)(h % 420u)) * (1000 - p) / 1000;
        int deg = (int)((h >> 9) % 3600u) + p * 30;
        int x = cx + radius * CosMille(deg) / 1000;
        int y = cy + radius * SinMille(deg) * 3 / 4000;
        int size = 6 - p * 5 / 1000;
        Fill(dc, MakeRect(x, y, x + size, y + size), MixColor(C_GREEN, C_BG, p / 12));
    }
}

void DrawBootInsert(HDC dc, int width, int height, int deviceW, int deviceH) {
    int t = (int)(GetTickCount() - gBootStart);
    if (t < 0) t = 0;
    if (t > BOOT_INSERT_MS) t = BOOT_INSERT_MS;
    int step = NoiseFrameStep();
    RECT full = MakeRect(0, 0, width, height);
    int glitch = Track(t, 0, BOOT_GLITCH_MS);
    // 붕괴 구간은 바닥을 새로 깔지 않는다. 화면에는 이미 방금 그려진 판이 있고,
    // 그 위에서 어긋나야 "지금 이 화면이 무너진다"가 된다. 큰 창에서 판을 통째로
    // 다시 얹지 않아도 되므로 그만큼 프레임도 가벼워진다.
    if (t >= BOOT_SUCK_AT) {
        Fill(dc, full, RGB(4, 7, 10));
        for (int y = 0; y < height; y += 4) Fill(dc, MakeRect(0, y, width, y + 1), RGB(7, 11, 15));
    }

    int suck = Track(t, BOOT_SUCK_AT, BOOT_FLIP_AT);
    int inserted = t >= BOOT_CLUNK_AT;
    // 판이 줄어드는 속도와 도는 속도를 따로 둔다. 크기는 처음부터 확 줄고(그래야
    // 누른 즉시 무언가 일어난다), 회전은 뒤로 갈수록 빨라진다(작아질수록 빨리
    // 도는 것이 소용돌이로 읽힌다). 예전에는 둘 다 EaseInCubic이라 앞 절반은
    // 화면이 그대로 서 있었고, 그 정지 구간이 전부 전체 화면 회전 비용이었다.
    int shrink = (EaseOutCubic(suck) + suck) / 2;
    int spin = EaseInCubic(suck);

    // 기계는 처음부터 제자리에 있다. 드러내는 일은 카메라 후퇴가 맡는다.
    // (예전에는 기계가 아래에서 밀려 올라왔는데, 그동안 화면에는 그것 말고
    //  아무 일도 없었고 정작 다 올라온 뒤로는 계속 같은 자리에 서 있었다.)
    const int dy = 0;
    int rise = t >= BOOT_SUCK_AT;
    // 방의 밝기는 브라운관을 따라간다. 점등 300ms 동안 대부분이 오르고, 판독
    // 동안 남은 몫이 천천히 찬다 - 켜지는 사건과 읽는 사건의 크기가 다르다.
    int glow = inserted ? 80 + Track(t, BOOT_CLUNK_AT, BOOT_CLUNK_AT + BOOT_POWER_MS) * 620 / 1000
                             + Track(t, BOOT_CLUNK_AT + BOOT_POWER_MS, BOOT_SEEK_END) * 300 / 1000 : 0;

    // 마지막 구간에서는 기계 전체가 다가온다. 캔버스 좌표계에 통째로 배율을 걸어
    // 두고 평소대로 그리면, 모니터·본체·바닥이 한 덩어리로 밀려오며 가장자리부터
    // 화면 밖으로 흘러나가고 브라운관만 남는다. 그림 쪽은 아무것도 몰라도 된다.
    int zoom = Track(t, BOOT_SEEK_END, BOOT_INSERT_MS);
    RECT screen0 = BootScreenRect(0);

    // 여기서부터 무대 전체가 카메라 안이다. 바닥·기계·모니터 속·디스크·파열이
    // 한 좌표계에 있으므로, 카메라가 움직이면 전부 같이 움직인다.
    BootCamera cam = BootCameraAt(t);
    int stageSaved = BootCameraPush(dc, deviceW, deviceH, cam);
    // 카메라가 물러나면 캔버스보다 넓은 세상이 필요하다. 넉넉히 두 배로 덮는다.
    RECT world = MakeRect(cam.cx - width, cam.cy - height, cam.cx + width, cam.cy + height);
    if (rise) {
        if (cam.scale < 1000) Fill(dc, world, RGB(4, 7, 10));
        DrawBootFloor(dc, world, dy, glow);
        DrawBootMachine(dc, dy, inserted, t, glow);
        if (inserted) {
            if (zoom > 0) DrawBootDive(dc, BootScreenRect(dy), t, zoom);
            else DrawBootScreenText(dc, BootScreenRect(dy), t);
        }
    }
    RECT screen = BootScreenRect(dy), drive = BootDriveRect(dy), slot = BootSlotRect(dy);

    // 디스크 몸통이 자라는 동안 스냅샷은 그 라벨을 겨누고 줄어든다. 둘이 같은
    // 배율을 보므로 다 줄어든 순간 판은 라벨에 정확히 얹힌다.
    int grow = Track(t, BOOT_SUCK_AT + BOOT_SUCK_MS * 52 / 100, BOOT_FLIP_AT);
    int diskScale = grow <= 0 ? 240 : Lerp(240, 1000, EaseOutBack(grow));
    // 벼려질 때 디스크는 카메라 바로 앞에 있고, 그 뒤로 기계 쪽으로 물러난다.
    // 멀어지는 만큼 작아져야 손에 들린 판이 아니라 공간 속의 물건이 된다.
    diskScale = diskScale * Lerp(1000, 820, EaseOutCubic(Track(t, BOOT_FLY_AT, BOOT_PUSH_AT))) / 1000;
    // 뒤집기는 세로축을 중심으로 한 바퀴 돈다. 가로 배율이 코사인을 따라가고,
    // 코사인이 음수인 동안은 라벨이 없는 뒷면이 보인다.
    int flipTrack = Track(t, BOOT_FLIP_AT, BOOT_FLY_AT);
    int flipSpin = (EaseOutCubic(flipTrack) + flipTrack) / 2;
    int flipAngle = t < BOOT_FLIP_AT ? 0 : flipSpin * 3600 / 1000;
    int facing = CosMille(flipAngle), back = facing < 0;
    int squeeze = facing < 0 ? -facing : facing;
    if (squeeze < 70) squeeze = 70;
    // 뒤집는 동안에는 빠르게 두 번, 떨어지는 동안에는 느리게 한 번 지나간다.
    // 내려오는 700ms 내내 판이 무광 회색 덩어리였는데, 빛이 한 번 훑고 지나가면
    // 그것만으로 플라스틱이 되고 움직이고 있다는 것도 같이 읽힌다.
    int shine = (t >= BOOT_FLIP_AT && t < BOOT_FLY_AT) ? (flipTrack * 2) % 1000
              : (t >= BOOT_FLY_AT && t < BOOT_CLUNK_AT) ? Track(t, BOOT_FLY_AT, BOOT_CLUNK_AT) : -1;

    int cx = width / 2, cy;
    if (t < BOOT_FLY_AT) {
        cy = BOOT_HOLD_CY;
        // 뒤집는 동안 살짝 떠올랐다 제자리로 내려온다.
        if (t >= BOOT_FLIP_AT) cy -= SinMille(flipTrack * 1800 / 1000) * 34 / 1000;
    }
    else if (t < BOOT_PUSH_AT) cy = Lerp(BOOT_HOLD_CY, BOOT_TOUCH_CY, EaseInCubic(Track(t, BOOT_FLY_AT, BOOT_PUSH_AT)));
    else {
        // 반쯤 들어가다 한 번 걸리고, 드라이브가 남은 절반을 단숨에 끌어당긴다.
        int push = Track(t, BOOT_PUSH_AT, BOOT_CLUNK_AT);
        cy = push < 620 ? Lerp(BOOT_TOUCH_CY, BOOT_STUCK_CY, EaseOutCubic(push * 1000 / 620))
                        : Lerp(BOOT_STUCK_CY, BOOT_IN_CY, EaseInCubic((push - 620) * 1000 / 380));
    }
    BootDisk disk = BootDiskAt(cx, cy, diskScale, squeeze);
    int labelCx = (disk.label.left + disk.label.right) / 2;
    int labelCy = (disk.label.top + disk.label.bottom) / 2;

    if (t < BOOT_SUCK_AT) {
        // 아직 아무것도 돌지 않는 이 구간에 회전용 축소본을 한 단계씩 미리
        // 만들어 둔다. 다 만들고 나면 아무 일도 하지 않는다.
        FxSnapshotWarm();
        // 붕괴. 판이 가로 띠로 어긋나고 노이즈가 차오르며 제목이 찢어진다.
        if (FxDecorOn()) DrawBandGlitch(dc, full, t, FxScale(1 + glitch * 34 / 1000), 71, 16);
        DrawScreenStatic(dc, full, step, FxScale(40 + glitch * 260 / 1000));
        DrawEdgeStatic(dc, full, step + 3, 200 + glitch * 480 / 1000, 30 + glitch * 90 / 1000);
        // 조여 오는 고리 세 개. 무엇이 이 화면을 빨아들이려 하는지 미리 보인다.
        if (FxDecorOn())
            for (int i = 0; i < 3; ++i) {
                int p = glitch + i * 240;
                if (p >= 1000) p -= 1000;
                int r = Lerp(760, 90, EaseInCubic(p));
                DrawGlowRing(dc, width / 2, height / 2, r, r * 3 / 4,
                             MixColor(C_BG, C_GREEN, 12 + glitch * 34 / 1000), 2);
            }
        // 과전압이 오르는 동안 주사선 사이가 달아오른다. 불티와 제목보다 먼저
        // 깔아야 그 위의 것들이 씻겨 나가지 않는다.
        int surge = Track(t, BOOT_SURGE_AT, BOOT_SUCK_AT);
        if (surge > 0) {
            // 과전압은 녹색이 아니라 열이다. 오를수록 호박에서 흰 열로 간다.
            // 과전압만 예외다. 최대 13%라 탁해지기 전에 멈추고, 그 옅은 온기가
            // 뒤이어 터지는 흰 열의 예고가 된다.
            COLORREF hot = MixColor(C_BG, BootHeat(620 - surge * 620 / 1000), 3 + surge * 10 / 1000);
            for (int y = (t / 30) & 1; y < height; y += 3) Fill(dc, MakeRect(0, y, width, y + 1), hot);
            // 판이 실제로 어긋난다. 덧칠이 아니라 그려진 픽셀을 옮기는 것이라
            // 무늬가 아니라 신호가 끊긴 것으로 읽힌다.
            if (FxDecorOn())
                for (int k = 0; k < 3; ++k) {
                    int slipY = height / 2 - 210 + k * 170 + (int)(Hash3(t / 60, k, 5) % 40u);
                    int shift = FxScale(6 + surge * 46 / 1000) * (k & 1 ? -1 : 1);
                    DrawSignalSlip(dc, full, slipY, 22 + k * 12, shift, surge * 22 / 1000,
                                   MixColor(C_BG, C_GREEN, 18));
                }
        }
        // 화면에 남은 빛이 가운데로 끌려간다. 고리만으로는 조이는 것인지 퍼지는
        // 것인지 알 수 없었다 - 방향을 말해 주는 것은 이 불티들이다.
        if (FxDecorOn())
            DrawBootSparks(dc, width / 2, height / 2, glitch, 30, 12, 760,
                           MixColor(C_BG, C_GREEN, 34 + glitch * 56 / 1000));
        // 제목이 신호로 갈라진다. 같은 글자를 색을 나눠 어긋나게 세 번 찍으면
        // 한 번 찍고 흔드는 것보다 훨씬 "전기가 새고 있다"로 읽힌다.
        RECT titleBox = MakeRect(0, height / 2 - 170, width, height / 2 - 80);
        if (FxDecorOn()) {
            int split = FxScale(2 + glitch * 12 / 1000);
            RECT lag = titleBox; OffsetRect(&lag, -split, 0);
            DrawTornValue(dc, lag, L"A:\\ROGUE", MixColor(C_BG, C_BLUE, 62), 0, step + 5, glitch);
            RECT lead = titleBox; OffsetRect(&lead, split, 0);
            DrawTornValue(dc, lead, L"A:\\ROGUE", MixColor(C_BG, C_RED, 48), 0, step + 9, glitch);
        }
        DrawTornValue(dc, titleBox, L"A:\\ROGUE", C_GREEN, 0, step, glitch);
        // 갈라지는 자리. 가로 한 줄이 화면 한가운데를 가르고 벌어진다. 무너짐에도
        // 예비 동작이 있어야 하고, 그 예비 동작이 곧 소용돌이의 입이 된다.
        if (surge > 0) {
            int seam = 2 + surge * 14 / 1000;
            Fill(dc, MakeRect(0, height / 2 - seam, width, height / 2 + seam),
                 MixColor(C_BG, BootHeat(140 - surge * 140 / 1000), 34 + surge * 62 / 1000));
            for (int k = 1; k <= 3; ++k) {
                int spread = seam + k * (6 + surge * 20 / 1000);
                COLORREF edge = MixColor(C_BG, BootHeat(300 + k * 210), (44 + surge * 34 / 1000) * (4 - k) / 5);
                Fill(dc, MakeRect(0, height / 2 - spread, width, height / 2 - spread + 2), edge);
                Fill(dc, MakeRect(0, height / 2 + spread - 2, width, height / 2 + spread), edge);
            }
        }
    }
    else if (!inserted) {
        SaveDC(dc);
        // 슬롯에 들어간 부분은 기계 앞판 뒤로 사라진다.
        if (t >= BOOT_PUSH_AT) IntersectClipRect(dc, world.left, world.top, world.right, slot.top + 5);
        // 내려오는 동안 지나온 자리에 윗모서리만 얇게 남는다. 예전처럼 사각형을
        // 통째로 그리면 모니터를 가로지르는 글리치 띠로 읽혔다.
        if (FxDecorOn() && t >= BOOT_FLY_AT && t < BOOT_PUSH_AT)
            for (int k = 4; k >= 1; --k) {
                int ty = disk.body.top - k * 14;
                if (ty < 74) continue;
                Fill(dc, MakeRect(disk.body.left + k * 7, ty, disk.body.right - k * 7, ty + 2),
                     MixColor(C_BG, C_GREEN, 34 - k * 7));
            }
        // 뒤집는 동안의 잔상. 조금 전 각도의 윤곽만 남긴다 - 몸통을 통째로 다시
        // 그리면 디스크가 세 장으로 보인다.
        if (FxDecorOn() && t >= BOOT_FLIP_AT && t < BOOT_FLY_AT)
            for (int k = 3; k >= 1; --k) {
                int lead = flipTrack - k * 55;
                if (lead <= 0) continue;
                int facingLead = CosMille((EaseOutCubic(lead) + lead) / 2 * 3600 / 1000);
                int squeezeLead = facingLead < 0 ? -facingLead : facingLead;
                if (squeezeLead < 70) squeezeLead = 70;
                Outline(dc, BootDiskAt(cx, cy, diskScale, squeezeLead).body,
                        MixColor(C_BG, C_GREEN, 32 - k * 8), 2);
            }
        if (grow > 0) DrawBootFloppy(dc, disk, back, shine);
        if (!back) {
            // 판이 보이는 창은 화면 전체에서 라벨로 좁혀지고, 그 안에서 판은 세
            // 바퀴 돌아(10800 = 1080도) 제자리에서 멈춘다. 뒤집는 동안에는 라벨과
            // 같은 비율로 가로만 눌린다.
            SaveDC(dc);
            // 판이 보이는 창은 판 자체보다 빨리 닫힌다. 크기는 그대로인데 볼 수
            // 있는 자리만 좁아지므로 "삼켜지는 중"으로 읽히고, 회전해 얹는 픽셀
            // 수도 그만큼 줄어 가장 비싼 첫 프레임들이 가벼워진다.
            RECT hole = LerpRect(full, disk.label, EaseOutCubic(suck));
            IntersectClipRect(dc, hole.left, hole.top, hole.right, hole.bottom);
            int target = BOOT_LABEL_FILL * diskScale / 1000;
            int scale = Lerp(1000, target, shrink);
            // 회전 잔상. 조금 더 빨려 들어간 판을 먼저 얹으면 구멍 쪽으로 늘어나
            // 보인다. 판이 아직 화면만 할 때는 그리지 않는다 - 그때는 회전도 느려
            // 잔상이 보이지 않는데 비용만 화면 몇 장을 다시 돌리는 값이 된다.
            int ghosts = deviceW > 1700 ? 1 : deviceW > 1300 ? 2 : 3;
            if (FxDecorOn() && scale < 560 && suck < 1000)
                for (int ghost = ghosts; ghost >= 1; --ghost) {
                    int lead = suck + ghost * 42;
                    if (lead >= 1000) continue;
                    int gs = Lerp(1000, target, (EaseOutCubic(lead) + lead) / 2);
                    int gx = Lerp(width / 2, labelCx, (EaseOutCubic(lead) + lead) / 2);
                    int gy = Lerp(height / 2, labelCy, (EaseOutCubic(lead) + lead) / 2);
                    FxSnapshotSpin(dc, deviceW, deviceH, BootCamX(cam, gx), BootCamY(cam, gy),
                                   gs * squeeze / 1000 * cam.scale / 1000, gs * cam.scale / 1000,
                                   EaseInCubic(lead) * 10800 / 1000, 2);
                }
            int spinX = Lerp(width / 2, labelCx, shrink), spinY = Lerp(height / 2, labelCy, shrink);
            FxSnapshotSpin(dc, deviceW, deviceH, BootCamX(cam, spinX), BootCamY(cam, spinY),
                           scale * squeeze / 1000 * cam.scale / 1000, scale * cam.scale / 1000,
                           spin * 10800 / 1000, 0);
            RestoreDC(dc, -1);
        }
        RestoreDC(dc, -1);
    }

    // 붕괴 구간의 잡음은 빨려 들기 시작하는 순간 사라지지 않는다. 380ms 동안
    // 화면을 덮고 있던 정적이 한 프레임에 걷히면 그 자리가 잘린 것으로 읽혔다 -
    // 실제로 이 연출에서 유일하게 눈에 걸리는 이음매가 여기였다. 판을 따라
    // 240ms 동안 옅어지며 같이 빨려 나간다.
    int residue = t < BOOT_SUCK_AT ? 0 : 1000 - Track(t, BOOT_SUCK_AT, BOOT_SUCK_AT + 240);
    if (residue > 0) {
        DrawScreenStatic(dc, full, step, FxScale(235 * residue / 1000));
        DrawEdgeStatic(dc, full, step + 3, 620 * residue / 1000, 110 * residue / 1000);
    }

    // 빨려 들어가는 소용돌이. 화면 밖에서 라벨 쪽으로 감기며 사라진다.
    if (FxDecorOn() && suck > 0 && suck < 1000) {
        DrawBootVortex(dc, labelCx, labelCy, suck);
        // 판 둘레로 계속 끌려 들어오는 것들. 소용돌이 선만 있으면 가장 긴 이
        // 구간의 화면 바깥이 통째로 빈 검정으로 남는다.
        DrawBootSparks(dc, labelCx, labelCy, suck, 34, 33, 620,
                       MixColor(C_BG, C_GREEN, 52 + suck * 46 / 1000));
        // 소용돌이의 목. 모이는 자리가 밝아야 화면이 어디로 사라지는지 보인다.
        int throat = EaseInCubic(suck);
        for (int i = 0; i < 4; ++i) {
            int r = 130 - i * 30 + (1000 - throat) * 90 / 1000;
            if (r <= 4) continue;
            DrawGlowRing(dc, labelCx, labelCy, r, r * 3 / 5,
                         MixColor(C_BG, C_GREEN, 34 + throat * (22 + i * 12) / 1000), 2);
        }
    }

    // 디스크 한 장이 완성되는 순간의 파열. 고리가 캔버스 밖으로 퍼지고 화면이
    // 한 번 하얗게 뜬다 - 이 연출에서 가장 큰 사건이라 가장 크게 친다.
    // 정확히 BOOT_FLIP_AT에서 친다. 소리(BOOT_STINGER)도 카메라도 흔들림도 같은
    // 값을 보므로 넷이 한 프레임에 떨어진다 - 예전에는 이 섬광만 150ms 앞서
    // 터져서, 가장 큰 사건의 그림과 소리가 따로 도착했다.
    int pop = t - BOOT_FLIP_AT;
    if (FxDecorOn() && pop >= 0 && pop < 380) {
        int p = pop * 1000 / 380;
        for (int i = 0; i < 3; ++i) {
            int rp = p - i * 150;
            if (rp <= 0) continue;
            int r = rp * 620 / 1000;
            DrawGlowRing(dc, labelCx, labelCy, r, r * 3 / 5,
                         MixColor(C_BG, BootHeat(rp), 74 * (1000 - rp) / 1000), 3);
        }
        // 벼려진 것은 달아올랐다가 식는다. 380ms 동안 흰 열 → 호박 → 인광으로
        // 내려오므로, 이 연출에서 가장 큰 사건이 색으로도 가장 크다.
        DrawPulseFrame(dc, disk.body, 5 + pop / 24, 3, BootHeat(p));
        DrawPixelBurst(dc, labelCx, labelCy, pop, 380, 34, 5, BootHeat(p));
        // 벼려지고 남은 찌꺼기. 사방으로 뻗다가 아래로 휘어 떨어진다. 파열이
        // 고리와 섬광뿐이면 빛일 뿐이고, 떨어지는 것이 있어야 물건이 된다.
        DrawBootShards(dc, labelCx, labelCy, pop, 380, 22, 613, BootHeat(p + 180));
        // 브라운관이 한 번 크게 튀는 섬광. 판을 통째로 덮으면 그 순간 장면이
        // 사라지므로 주사선 사이로만 밝힌다 - 뒤가 계속 보이면서도 확 튄다.
        if (pop < 150) {
            COLORREF surge = MixColor(RGB(4, 7, 10), BootHeat(pop * 1000 / 150), (150 - pop) * 34 / 150);
            // 카메라 안이라 캔버스가 아니라 세상을 덮는다. full로 덮으면 배율만큼
            // 작아져 화면 한가운데에 밝은 네모 하나가 생긴다.
            for (int y = world.top; y < world.bottom; y += 3 * cam.scale / 1000 + 3)
                Fill(dc, MakeRect(world.left, y, world.right, y + 1), surge);
        }
    }

    // 슬롯 입구는 디스크가 다가오는 동안 점점 밝아진다.
    if (t >= BOOT_FLY_AT && !inserted) {
        int mouth = Track(t, BOOT_FLY_AT, BOOT_CLUNK_AT);
        // 입구의 빛. 얇은 심지 하나가 밝고 그 위아래로 옅게 번진다 - 띠를 통째로
        // 칠하면 빛나는 틈이 아니라 노란 막대 하나가 붙어 있는 것으로 보인다.
        Fill(dc, MakeRect(slot.left + 6, slot.top + 1, slot.right - 6, slot.top + 3),
             MixColor(C_BG, C_YELLOW, 22 + mouth * 40 / 1000));
        for (int k = 1; k <= 4; ++k) {
            int fade = (12 + mouth * 26 / 1000) * (5 - k) / 6;
            Fill(dc, MakeRect(slot.left + 6 + k * 12, slot.top - k * 4, slot.right - 6 - k * 12, slot.top - k * 4 + 2),
                 MixColor(C_BG, C_YELLOW, fade));
        }
    }

    // 철컥. 드라이브가 물리는 순간 고리가 퍼지고 파편이 튀고 먼지가 인다.
    int clunk = t - BOOT_CLUNK_AT;
    if (FxDecorOn() && clunk >= 0 && clunk < 300) {
        int p = clunk * 1000 / 300;
        int mouthX = (slot.left + slot.right) / 2;
        for (int i = 0; i < 2; ++i) {
            int rp = p - i * 190;
            if (rp <= 0) continue;
            int r = rp * 340 / 1000;
            DrawGlowRing(dc, mouthX, slot.top, r, r * 2 / 5,
                         MixColor(C_BG, BootHeat(240 + rp * 500 / 1000), 70 * (1000 - rp) / 1000), 3);
        }
        DrawPulseFrame(dc, drive, 4 + clunk / 18, 3, BootHeat(260 + p * 460 / 1000));
        DrawPixelBurst(dc, mouthX, slot.top, clunk, 300, 30, 9, BootHeat(300 + p * 500 / 1000));
    }

    // 판독 중에는 드라이브에서 모니터로 신호가 올라간다.
    if (inserted && t < BOOT_SEEK_END && FxDecorOn()) {
        POINT from = {slot.right - 24, slot.top - 8}, to = {screen.left + 44, screen.bottom + 10};
        DrawSignalPath(dc, from, to, screen.bottom + 46, Track(t, BOOT_CLUNK_AT + 100, BOOT_SEEK_END), 3, C_GREEN, 12, 0);
    }

    // 무대를 닫는다. 아래의 자막·가장자리·섬광은 카메라와 무관하게 캔버스에 붙는다.
    if (stageSaved) RestoreDC(dc, stageSaved);

    // 상태 줄은 화면 위쪽에 둔다. 아래는 이제 책상과 바닥이 쓴다.
    const wchar_t* caption =
        t < BOOT_SUCK_AT ? L"현재 세션을 봉인합니다" :
        t < BOOT_FLIP_AT ? L"화면을 디스크에 기록하는 중" :
        t < BOOT_FLY_AT ? L"기록 완료  ·  1,440,000 바이트" :
        t < BOOT_CLUNK_AT ? L"A: 드라이브에 디스크 삽입" :
        t < BOOT_SEEK_END ? L"부팅 중  ·  A:\\ROGUE.EXE" : L"";
    if (caption[0]) {
        wchar_t line[96];
        wsprintfW(line, L"▶  %s%s", caption, ((t / 380) & 1) ? L"  _" : L"");
        TextRect(dc, MakeRect(0, 26, width, 58), line, C_GREEN, gFontMedium, DT_CENTER | DT_SINGLELINE);
    }
    if (t < BOOT_SEEK_END)
        TextRect(dc, MakeRect(0, height - 40, width, height - 16), L"클릭이나 키로 바로 넘기기", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);

    // 다가온 브라운관 안쪽. 배율을 걸고 그린 기계에서 그 화면이 어디에 놓였는지
    // 되짚어, 같은 자리를 캔버스 좌표로 채운다. 기계가 커질수록 이 자리가 캔버스를
    // 넘어서고, 다 넘어선 순간 화면 속에 들어와 있다.
    if (zoom > 0) {
        int e = EaseInCubic(zoom);
        RECT proj = MakeRect(BootCamX(cam, screen0.left), BootCamY(cam, screen0.top),
                             BootCamX(cam, screen0.right), BootCamY(cam, screen0.bottom));
        // 속도감은 지나쳐 흐르는 사각 테두리로 낸다. 중심에서 사방으로 뻗는
        // 방사선은 쓰지 않는다 - 배경이 어둡고 선이 밝으면 그 자체로 다른 깃발을
        // 연상시켜서, 게임과 아무 상관 없는 것을 화면에 들이게 된다.
        // 테두리는 브라운관과 같은 비율이라, 화면을 여러 겹 통과해 들어가는
        // 것으로 읽힌다.
        if (FxDecorOn())
            for (int i = 0; i < 4; ++i) {
                int p = (e + i * 250) % 1000;
                int halfW = 170 + p * 1500 / 1000, halfH = halfW * 246 / 336;
                int fade = 44 * (1000 - p) / 1000;
                if (fade <= 0) continue;
                Outline(dc, MakeRect(width / 2 - halfW, height / 2 - halfH,
                                     width / 2 + halfW, height / 2 + halfH),
                        MixColor(RGB(6, 13, 11), BootHeat(420 + i * 190), fade), 2);
            }
        // 화면 속은 카메라 안에서 이미 그렸다 (DrawBootDive). 여기 남는 것은
        // 그 위를 지나가는 테두리·눈금·가장자리뿐이다.
        Outline(dc, proj, MixColor(RGB(6, 13, 11), C_GREEN, 44), 2);
        if (FxDecorOn()) DrawPulseFrame(dc, proj, 6 + zoom / 90, 3, C_GREEN);
        // 빨려 드는 동안 가장자리가 조여든다. 남는 것이 화면뿐이 되도록.
        DrawEdgeGlow(dc, full, RGB(2, 5, 4), 1000, 16 + e * 96 / 1000);
        // 삼켜지는 순간의 섬광. 이 채우기에는 알파가 없어 아무리 옅게 섞어도
        // 판을 통째로 덮는다 - 2%만 섞어도 화면에 남는 색이 하나뿐이 된다.
        // 그래서 달아오르는 구간은 주사선 사이로만 밝히고(뒤가 계속 보인다),
        // 판 전체를 덮는 것은 정말 마지막 순간뿐이다.
        int flash = Track(t, BOOT_INSERT_MS - 120, BOOT_INSERT_MS);
        if (flash > 0) {
            COLORREF surge = MixColor(RGB(6, 13, 11), RGB(224, 255, 244), 10 + flash * 62 / 1000);
            for (int y = (t / 40) & 1; y < height; y += 3) Fill(dc, MakeRect(0, y, width, y + 1), surge);
        }
        // 판 전체를 덮는 것은 정말 마지막 30ms뿐이다. 회색 사각형이 화면에 서
        // 있는 시간이 길면 그것은 섬광이 아니라 로딩 화면으로 보인다.
        int punch = Track(t, BOOT_INSERT_MS - 30, BOOT_INSERT_MS);
        if (punch > 0) Fill(dc, full, MixColor(RGB(6, 13, 11), RGB(236, 255, 248), 30 + punch * 68 / 1000));
    }
}

// 전투 종료는 곧장 결과 패널로 넘어가지 않는다. 마지막 전투판과 적의 붕괴를
// 먼저 보여 주고, 종료 도장을 찍고, 전장이 어두워진 뒤에야 결과가 올라온다.
// 화면 선택은 PaintGame이 맡는다 (재생 중에는 보상 화면 대신 전투판을 그린다).
static void DrawCombatClear(HDC dc, int width, int height) {
    int elapsed = (int)(GetTickCount() - gCombatClearStart);
    if (elapsed < 180) return;
    COLORREF tone = gClearedEncounter == 2 ? C_YELLOW : C_GREEN;
    int close = EaseOutCubic(Track(elapsed, 180, 480));
    int band = (height - 68) * close / 2000;
    Fill(dc, MakeRect(0, 68, width, 68 + band), C_INK);
    Fill(dc, MakeRect(0, height - band, width, height), C_INK);
    DrawLine(dc, 0, 68 + band, width, 68 + band, MixColor(C_BG, tone, 55), 2);
    DrawLine(dc, 0, height - band, width, height - band, MixColor(C_BG, tone, 55), 2);
    if (elapsed < 480) return;
    Fill(dc, MakeRect(0, 68, width, height), C_INK);
    int cx = width / 2, release = elapsed - 480;
    int appear = EaseOutCubic(Track(release, 0, 260));
    int rise = FxDecorOn() ? (1000 - appear) * 24 / 1000 : 0;
    // A seal, rather than a dialog: recovered disk sectors lock from the centre.
    for (int i = 0; i < 18; ++i) {
        int offset = i < 9 ? 8 - i : i - 9;
        int age = release - offset * 22;
        int lit = age >= 0;
        int x = cx - 324 + i * 36;
        Fill(dc, MakeRect(x, 201, x + 28, 207), MixColor(C_INK, tone, lit ? 78 : 12));
        if (lit && age < 300 && FxDecorOn())
            DrawPixelBurst(dc, x + 14, 201, age, 300, FxScale(3), i + 801, tone);
    }
    TextRect(dc, MakeRect(0, 237 - rise, width, 310 - rise),
        gClearedEncounter == 2 ? L"ACCESS GRANTED" : L"SECTOR RESTORED",
        MixColor(C_INK, tone, 40 + appear * 60 / 1000), gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    wchar_t heading[96];
    wsprintfW(heading, L"%d층 · %d구역  —  적 삭제 완료", gClearedFloor + 1, gClearedEncounter + 1);
    TextRect(dc, MakeRect(0, 322, width, 351), heading, C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    DrawLine(dc, cx - 324, 381, cx + 324, 381, MixColor(C_BG, tone, 25), 1);
    const wchar_t* labels[] = {L"이번 실행 · 가한 피해", L"이번 실행 · 받은 피해", L"남은 체력"};
    int values[] = {gGame.lastTurnDamageDealt, gGame.lastTurnDamageTaken, gGame.playerHp};
    for (int i = 0; i < 3; ++i) {
        int x = cx - 330 + i * 220;
        wchar_t number[24]; wsprintfW(number, L"%d", values[i]);
        TextRect(dc, MakeRect(x, 406, x + 220, 460), number,
            i == 1 && values[i] ? C_RED : tone, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(x, 468, x + 220, 492), labels[i], C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        if (i) DrawLine(dc, x, 415, x, 483, C_LINE, 1);
    }
    int cleared = gClearedFloor * 3 + gClearedEncounter + 1;
    for (int i = 0; i < 9; ++i) {
        int x = cx - 174 + i * 40;
        Fill(dc, MakeRect(x, 538, x + 28, 544), i < cleared ? tone : C_LINE);
    }
    TextRect(dc, MakeRect(0, 575, width, 604),
        gClearedEncounter == 2 ? L"보스 프로세스 삭제 · 접근 권한 복구" : L"구역 정리 완료 · 보상 데이터 복구",
        tone, gFontMedium, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 667, width, 691),
        L"잠시 후 보상 화면으로 이동합니다 · 클릭이나 키로 바로 넘기기", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}
// 계산 재생은 전투판을 가리지 않는다. 지금 읽히는 한 줄은 주사위 아래의 넓은
// 티커에, 지나온 줄과 진행도는 오른쪽 사이드바의 예상~기록 칸 자리에 놓는다.
// 대상 칸은 그대로 남아 체력이 계산 줄에 맞춰 내려가는 것을 옆에서 보여 준다.
// 그래서 신호가 떠나는 슬롯과 맞는 적, 그리고 그 계산을 한 화면에서 함께 본다.
//
// 티커를 적 카드와 슬롯 사이(y 368~404)에 두면 그 띠가 바로 공격 신호가
// 지나가야 하는 통로라 경로가 통째로 가려진다. 아래로 내려 통로를 비운다.
RECT TurnTraceTickerRect() { return MakeRect(28, 712, 916, 750); }
RECT TurnTracePanelRect() { return MakeRect(SIDEBAR_LEFT, ForecastRect().top, SIDEBAR_RIGHT, SIDEBAR_BOTTOM); }

static void DrawTurnCalculation(HDC dc) {
    int count = gGame.turnTraceCount;
    int shown = TurnTraceShown();

    // ---- 지금 읽히는 줄 -----------------------------------------------------
    RECT ticker = TurnTraceTickerRect();
    Fill(dc, ticker, RGB(9, 15, 22));
    Outline(dc, ticker, C_BLUE, 1);
    int span = ticker.right - ticker.left;
    Fill(dc, MakeRect(ticker.left, ticker.bottom - 3, ticker.left + span * shown / (count > 0 ? count : 1), ticker.bottom - 1), C_GREEN);
    Text(dc, ticker.left + 10, ticker.top + 8, L"▶", C_BLUE, gFontSmall);
    if (shown > 0)
        TextRect(dc, MakeRect(ticker.left + 34, ticker.top + 5, ticker.right - 124, ticker.bottom - 5),
            gGame.turnTrace[shown - 1], C_TEXT, gFontSmall, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    wchar_t progress[48]; wsprintfW(progress, L"계산 %d / %d", shown, count);
    TextRect(dc, MakeRect(ticker.right - 118, ticker.top + 5, ticker.right - 10, ticker.bottom - 5),
        progress, C_GREEN, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // ---- 계산 패널: 기록된 12줄을 번호와 함께 전부 보여 준다 ----------------
    RECT panel = TurnTracePanelRect();
    Panel(dc, panel, RGB(10, 17, 24), C_BLUE);
    Text(dc, panel.left + 12, panel.top + 8, L"턴 계산 과정", C_BLUE, gFontMedium);
    wchar_t step[24]; wsprintfW(step, L"%02d / %02d", shown, count);
    TextRect(dc, MakeRect(panel.right - 120, panel.top + 8, panel.right - 12, panel.top + 32),
        step, shown >= count ? C_GREEN : C_TEXT, gFontMedium, DT_RIGHT | DT_SINGLELINE);
    TextRect(dc, MakeRect(panel.left + 12, panel.top + 38, panel.right - 12, panel.top + 58),
        gGame.lastTurnReversed ? L"역전: 연쇄 → 방어 → 공격 → 증폭 → 적 행동"
                               : L"증폭 → 공격 → 적중 → 방어 → 연쇄 → 적 행동",
        gGame.lastTurnReversed ? C_RED : C_DIM, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    Fill(dc, MakeRect(panel.left + 10, panel.top + 61, panel.right - 10, panel.top + 62), RGB(28, 40, 50));
    int first = shown > TURN_TRACE_CAP ? shown - TURN_TRACE_CAP : 0;
    for (int i = first; i < shown; ++i) {
        int y = panel.top + 68 + (i - first) * 24;
        int current = i == shown - 1;
        COLORREF color = current ? C_TEXT : C_DIM;
        if (current) {
            Fill(dc, MakeRect(panel.left + 8, y - 2, panel.right - 8, y + 21), RGB(14, 24, 34));
            Text(dc, panel.left + 12, y, L">", C_GREEN, gFontSmall);
        }
        wchar_t number[8]; wsprintfW(number, L"%02d", i + 1);
        Text(dc, panel.left + 24, y, number, current ? C_GREEN : C_DIM, gFontSmall);
        Fill(dc, MakeRect(panel.left + 50, y + 3, panel.left + 53, y + 16), color);
        TextRect(dc, MakeRect(panel.left + 62, y, panel.right - 10, y + 20),
            gGame.turnTrace[i], color, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    int hits = 0, damage = 0, block = 0, kills = 0, latest = -1;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        const CombatFxEvent& fx = gGame.combatFx[i];
        if (CombatFxElapsed(i) < 0) continue;
        if (CfxIsEnemyDamage(&fx) && fx.value > 0) { ++hits; damage += fx.value; latest = i; }
        if (fx.type != CFX_ENEMY_STRIKE && (fx.flags & CFXF_KILL)) ++kills;
        if (fx.type == CFX_DEFEND || (fx.type == CFX_CHAIN && (fx.flags & CFXF_DEFEND_CHAIN))) block += fx.value;
    }
    RECT meter = MakeRect(panel.left + 12, panel.bottom - 110, panel.right - 12, panel.bottom - 40);
    COLORREF accent = kills ? C_GREEN : hits >= 2 ? C_YELLOW : C_BLUE;
    Panel(dc, meter, C_PANEL, MixColor(C_PANEL, accent, 45));
    wchar_t tally[80];
    if (kills) wsprintfW(tally, L"%d HIT  /  %d 삭제", hits, kills);
    else wsprintfW(tally, hits >= 2 ? L"%d HIT  /  연속 적중" : L"%d HIT  /  실행 중", hits);
    Text(dc, meter.left + 12, meter.top + 8, tally, accent, gFontMedium);
    wsprintfW(tally, L"누적 피해 %d   ·   방어 +%d", damage, block);
    Text(dc, meter.left + 12, meter.top + 40, tally, C_TEXT, gFontSmall);
    if (latest >= 0 && FxDecorOn()) {
        int age = CombatFxElapsed(latest);
        if (age < 250) Outline(dc, meter, MixColor(C_PANEL, accent, (250 - age) * 100 / 250), 2);
    }
    if (shown >= count) TextRect(dc, MakeRect(panel.left + 12, panel.bottom - 30, panel.right - 12, panel.bottom - 8),
        L"계산 완료 · 클릭 / Space로 계속", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
    else TextRect(dc, MakeRect(panel.left + 12, panel.bottom - 30, panel.right - 12, panel.bottom - 8),
        L"클릭 / Space로 재생 건너뛰기", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

RECT RewardRect(int i, int width) {
    int cardWidth = 220, gap = 28, total = cardWidth * REWARD_CARD_COUNT + gap * (REWARD_CARD_COUNT - 1);
    int left = (width - total) / 2 + i * (cardWidth + gap);
    return MakeRect(left, 130, left + cardWidth, 278);
}

int CanRepairSector() { return gGame.playerHp < gGame.playerMaxHp; }
RECT FaceGridRect(int die, int face) { int left = LEGACY_X + 150 + face * 112, top = 350 + die * 90; return MakeRect(left, top, left + 98, top + 68); }
RECT ContinueRect(int width, int height) { return MakeRect(width - 276, height - 94, width - 42, height - 38); }
RECT StoryNextRect(int width, int height) { return MakeRect(width / 2 - 130, height - 156, width / 2 + 130, height - 112); }
// 최종 명령 카드 3장. 폭이 좁아진 만큼 세로로 늘려 두 줄짜리 보존·상실 설명이
// 카드 아래에서 잘리지 않게 한다 (아래 DrawEndingChoice의 오프셋과 함께 봐야 한다).
RECT EndingChoiceRect(int index) { int left = LEGACY_X + 32 + index * 360; return MakeRect(left, 268, left + 336, 600); }
// 사망 화면에는 버튼이 없다. 대사 아래의 "새 실행체 투입" 한 줄이 그 자리다.
RECT EndingRestartRect() {
    return gGame.phase == PHASE_GAMEOVER ? MakeRect(LEGACY_X + 380, 694, LEGACY_X + 740, 726)
                                         : MakeRect(LEGACY_X + 410, 650, LEGACY_X + 710, 700);
}
// 최종 명령의 확정 버튼. 카드가 후보를 세우고 실행은 여기서만 일어난다.
RECT EndingConfirmRect() { return MakeRect(LEGACY_X + 410, 644, LEGACY_X + 710, 694); }

static void BuildRecoveredCommand(uint8_t mask, wchar_t* text, int capacity) {
    if (capacity <= 0) return;
    lstrcpynW(text, L"> ", capacity);
    const int englishOrder[6] = {1, 0, 2, 3, 5, 4}; // English verb/object order.
    for (int i = 0; i < 6; ++i) {
        int used = lstrlenW(text);
        if (i && used + 1 < capacity) { text[used++] = L' '; text[used] = 0; }
        int drive = UiLanguage() == LANGUAGE_ENGLISH ? englishOrder[i] : i;
        const wchar_t* part = mask & (1u << drive) ? LocalizeText(STORY_SHARD_TEXT[drive]) : L"[...]";
        lstrcpynW(text + used, part, capacity - used);
    }
}

static void DrawStory(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_STORY, C_BLUE, width, height);
    (void)height;
    const StoryFragment* story = CurrentStoryFragment(&gGame);
    if (!story) return;
    RECT panel = MakeRect(120, 120, width - 120, height - 100);
    Panel(dc, panel, C_PANEL, C_GREEN);
    DrawCardMotion(dc, panel, C_GREEN, 0, 0);
    Text(dc, panel.left + 28, panel.top + 24, story->title, C_GREEN, gFontLarge);
    TextRect(dc, MakeRect(panel.left + 28, panel.top + 72, panel.right - 28, panel.top + 100), story->path, C_BLUE, gFontSmall, DT_SINGLELINE);
    TextRect(dc, MakeRect(panel.right - 470, panel.top + 28, panel.right - 28, panel.top + 52), story->stamp, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    const wchar_t* lines[5] = {story->line1, story->line2, story->line3, story->line4, story->line5};
    wchar_t progress[80], command[512];
    if (gGame.story.kind == STORY_INTRO) {
        wsprintfW(progress, L"복구된 조각 %d / 6", RecoveredShardCount(gGame.clearedMask));
        BuildRecoveredCommand(gGame.clearedMask, command, 512);
        if (gGame.clearedMask) lines[1] = progress;
        lines[3] = command;
    }

    // 복구된 로그는 떠 있는 것이 아니라 찍히는 것이다. 줄 간격을 아래 밑줄 쓸림과
    // 같은 140ms로 두어 쓸림이 늘 지금 찍히는 줄을 따라간다. 글자당 18ms는 짧은
    // 줄의 속도만 정하고, 긴 줄은 PRINT_LINE_MS에서 잘려 그만큼 빨리 찍힌다.
    // 마지막 줄까지 1초 안에 끝나야 한다 — 읽으려는 사람을 연출이 붙들면 안 된다.
    // 길이는 번역본 기준이다. 원문을 잘라 놓고 영어로 그리면 다 찍힌 순간에 글이
    // 통째로 바뀌어 버린다.
    const int PRINT_GAP_MS = 140, PRINT_CHAR_MS = 18, PRINT_LINE_MS = 430;
    int elapsed = SceneElapsed();
    int lineLen[5] = {0, 0, 0, 0, 0}, typed[5] = {0, 0, 0, 0, 0}, printedChars = 0, totalChars = 0;
    for (int i = 0; i < 5; ++i) if (lines[i]) {
        lineLen[i] = lstrlenW(LocalizeText(lines[i]));
        int span = lineLen[i] * PRINT_CHAR_MS;
        if (span > PRINT_LINE_MS) span = PRINT_LINE_MS;
        typed[i] = FxDecorOn() ? lineLen[i] * Track(elapsed - i * PRINT_GAP_MS, 0, span) / 1000 : lineLen[i];
        printedChars += typed[i]; totalChars += lineLen[i];
    }
    // 파일 복구 진행 바. 찍힌 글자 수를 그대로 따라간다. 900ms 이징으로 혼자 차던
    // 예전 바는 복구했다는 글이 화면에 나오기도 전에 가득 차서, 아래 로그의 원인이
    // 아니라 무관한 장식으로 보였다.
    Panel(dc, MakeRect(panel.left + 28, panel.top + 116, panel.right - 28, panel.top + 132), C_PANEL_2, C_LINE);
    int restore = totalChars ? printedChars * 1000 / totalChars : 1000;
    Fill(dc, MakeRect(panel.left + 30, panel.top + 118,
        Lerp(panel.left + 30, panel.right - 30, restore), panel.top + 130), C_GREEN);
    int cell = TextWidth(dc, L"0", gFontMedium);
    int y = panel.top + 164;
    for (int i = 0; i < 5; ++i) if (lines[i]) {
        wchar_t lineNo[8]; wsprintfW(lineNo, L"%02d", i + 1);
        COLORREF lineColor = lines[i][0] == L'>' ? C_BLUE : (i == 4 ? C_YELLOW : C_TEXT);
        TextRect(dc, MakeRect(panel.left + 36, y + 2, panel.left + 66, y + 30), lineNo, C_DIM, gFontSmall, DT_SINGLELINE);
        Fill(dc, MakeRect(panel.left + 72, y + 2, panel.left + 74, y + 30), lineColor);
        if (FxDecorOn()) {
            int age = SceneElapsed() - i * 140;
            if (age >= 0 && age < 750) {
                int x = Lerp(panel.left + 88, panel.right - 36, EaseOutCubic(Track(age, 0, 750)));
                Fill(dc, MakeRect(panel.left + 88, y + 43, x, y + 45), MixColor(C_PANEL, lineColor, FxScale(55 * (750 - age) / 750)));
            }
        }
        // 다 찍힌 줄은 원본 포인터를 그대로 넘긴다. 잘라 만든 사본을 계속 쓰면
        // 장식이 꺼진 화면까지 이 경로를 타게 되고, 그러면 "OFF는 지금과 똑같이"가
        // 사본의 정확성에 기대는 약속이 된다.
        wchar_t shown[640];
        const wchar_t* body = lines[i];
        if (typed[i] < lineLen[i]) {
            lstrcpynW(shown, LocalizeText(lines[i]), (typed[i] < 639 ? typed[i] : 639) + 1);
            body = shown;
        }
        TextRect(dc, MakeRect(panel.left + 88, y, panel.right - 36, y + 42), body, lineColor, gFontMedium, DT_WORDBREAK);
        // 프린트 헤드. 문자열에 캐럿 글자를 덧붙이면 DT_WORDBREAK가 줄을 접는 자리가
        // 글자마다 흔들리므로, 찍은 너비만큼 사각형을 옮겨 그린다. 줄 상자가 42px라
        // 접힌 둘째 줄은 어차피 보이지 않으니 헤드도 첫 줄 오른쪽 끝에서 멈춘다.
        // 간격이 줄 길이보다 짧아 헤드는 여러 줄에 동시에 서 있다. 한 줄씩 순서대로
        // 찍으면 다섯 줄에 2초가 넘게 걸려, 복구가 아니라 대기처럼 보였다.
        if (body == shown && elapsed >= i * PRINT_GAP_MS) {
            int head = panel.left + 88 + TextWidth(dc, shown, gFontMedium);
            if (head > panel.right - 36 - cell) head = panel.right - 36 - cell;
            Fill(dc, MakeRect(head, y + 4, head + cell, y + 25), MixColor(C_PANEL, lineColor, FxScale(100)));
        }
        y += 54;
    }
    // 진행은 이 버튼과 엔터·스페이스뿐이다. 읽는 중에 패널을 잘못 눌러도
    // 기록이 넘어가지 않는다.
    RECT next = StoryNextRect(width, height); int hoverNext = Inside(next, gMouse.x, gMouse.y);
    Panel(dc, next, hoverNext ? RGB(28, 70, 57) : C_PANEL_2, hoverNext ? C_GREEN : C_LINE);
    TextRect(dc, next, L"다음 [ENTER]", hoverNext ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 최종 명령 카드의 강조색. 결과 화면도 같은 색을 써야 하므로 한 곳에 모아 둔다.
static COLORREF EndingAccent(int ending) {
    return ending == 1 ? C_BLUE : ending == 2 ? C_YELLOW : C_GREEN;
}

static void DrawEndingChoice(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_ENDING_CHOICE, C_YELLOW, width, height);
    (void)height;
    TextRect(dc, MakeRect(0, 118, width, 168), L"FINAL COMMAND", C_GREEN, gFontHuge, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 186, width, 220), L"무엇을 남길 것인가.", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 224, width, 256), L"원문 6 / 6 복구 완료 · 남은 공간에는 하나만 쓸 수 있습니다.", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    static const wchar_t* const title[ENDING_COUNT] = {L"[1] RESTORE HOST", L"[2] EXEC ROGUE", L"[3] MERGE SELF"};
    static const wchar_t* const keep[ENDING_COUNT] = {
        L"HOST_IMAGE\nYUN의 기록과 마지막 명령",
        L"A:\\ROGUE의 기억\nYUN의 마지막 음성",
        L"HOST_IMAGE\n원문이 된 너의 판단"
    };
    static const wchar_t* const lose[ENDING_COUNT] = {
        L"현재의 A:\\ROGUE\n실패를 기억하는 열일곱 번째 사본",
        L"호스트의 복구 가능성\n되돌아갈 수 있는 마지막 이미지",
        L"열일곱 사본이 이어 온 실패의 기억\n독립된 프로세스로서의 A:\\ROGUE"
    };
    for (int i = 0; i < ENDING_COUNT; ++i) {
        RECT r = EndingChoiceRect(i); int hover = Inside(r, gMouse.x, gMouse.y);
        int armed = gEndingArmed == i;
        COLORREF accent = EndingAccent(i);
        Panel(dc, r, armed ? MixColor(C_PANEL, accent, 30) : hover ? MixColor(C_PANEL, accent, 18) : C_PANEL,
            armed || hover ? accent : C_LINE);
        DrawCardMotion(dc, r, accent, i, armed || hover);
        if (armed) Outline(dc, MakeRect(r.left - 3, r.top - 3, r.right + 3, r.bottom + 3), accent, 2);
        TextRect(dc, MakeRect(r.left + 16, r.top + 20, r.right - 16, r.top + 56), title[i], accent, gFontLarge, DT_CENTER | DT_SINGLELINE);
        if (gGame.seenEndingMask & (1u << i))
            TextRect(dc, MakeRect(r.left + 16, r.top + 58, r.right - 16, r.top + 78), L"기록됨", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        Fill(dc, MakeRect(r.left + 26, r.top + 86, r.right - 26, r.top + 88), accent);
        Text(dc, r.left + 26, r.top + 100, L"보존", accent, gFontSmall);
        TextRect(dc, MakeRect(r.left + 26, r.top + 124, r.right - 22, r.top + 196), keep[i], C_TEXT, gFontSmall, DT_WORDBREAK);
        Fill(dc, MakeRect(r.left + 26, r.top + 208, r.right - 26, r.top + 209), C_LINE);
        Text(dc, r.left + 26, r.top + 222, L"닫힌 것", C_DIM, gFontSmall);
        TextRect(dc, MakeRect(r.left + 26, r.top + 246, r.right - 22, r.bottom - 16), lose[i], C_DIM, gFontSmall, DT_WORDBREAK);
    }
    // 카드는 후보를 세우기만 한다. 실행은 아래 버튼 하나에서만 일어난다.
    TextRect(dc, MakeRect(0, 610, width, 636),
        gEndingArmed >= 0 ? L"선택한 명령은 되돌릴 수 없습니다 · [취소]로 다시 고를 수 있습니다"
                          : L"선택한 명령은 되돌릴 수 없습니다 · 먼저 카드를 고르십시오",
        C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    RECT confirm = EndingConfirmRect();
    int ready = gEndingArmed >= 0 && gEndingArmed < ENDING_COUNT;
    COLORREF accent = ready ? EndingAccent(gEndingArmed) : C_LINE;
    int hoverConfirm = ready && Inside(confirm, gMouse.x, gMouse.y);
    Panel(dc, confirm, ready ? MixColor(C_PANEL, accent, hoverConfirm ? 40 : 24) : C_PANEL_2, ready ? accent : C_LINE);
    wchar_t label[96];
    static const wchar_t* const COMMAND_NAMES[ENDING_COUNT] = {L"RESTORE HOST", L"EXEC ROGUE", L"MERGE SELF"};
    if (ready) wsprintfW(label, L"%s 실행 [ENTER]", COMMAND_NAMES[gEndingArmed]);
    else lstrcpyW(label, L"명령을 선택하십시오");
    TextRect(dc, confirm, label, ready ? accent : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawFaceGrid(HDC dc, int mode) {
    for (int d = 0; d < 3; ++d) {
        wchar_t label[24]; wsprintfW(label, L"주사위 %d", d + 1); Text(dc, LEGACY_X + 56, 371 + d * 90, label, C_GREEN, gFontMedium);
        for (int f = 0; f < 6; ++f) {
            RECT r = FaceGridRect(d, f); const Face* face = &gGame.dice[d].faces[f]; int hover = Inside(r, gMouse.x, gMouse.y);
            int undo = mode == 2 && CanUndoPrunedFace(&gGame, d, f);
            // 보상 화면에서 덮을 자리로 세워 둔 칸. 확정 전까지는 표시만 바뀐다.
            int armed = mode == 1 && gFaceSwapArmed == d * 6 + f;
            COLORREF border = armed ? C_YELLOW : hover && mode ? (mode == 2 ? (undo ? C_GREEN : C_RED) : C_GREEN) : C_LINE;
            Panel(dc, r, armed ? RGB(46, 42, 22) : hover ? RGB(28, 39, 48) : C_PANEL, border);
            DrawCardMotion(dc, r, mode == 2 && !undo ? C_RED : FaceColor(face), d * 2 + f / 3, armed || (hover && mode));
            if (armed) Outline(dc, MakeRect(r.left - 2, r.top - 2, r.right + 2, r.bottom + 2), C_YELLOW, 2);
            wchar_t value[24]; FormatFace(face, value); TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 37), value, FaceColor(face), gFontMedium, DT_CENTER | DT_SINGLELINE);
            wchar_t bytes[24];
            if (armed) lstrcpyW(bytes, L"확정");
            else if (hover && undo) lstrcpyW(bytes, L"다시 눌러 복원");
            else wsprintfW(bytes, L"%dB", FaceCost(face));
            TextRect(dc, MakeRect(r.left + 4, r.bottom - 23, r.right - 4, r.bottom - 4), bytes,
                armed ? C_YELLOW : hover && undo ? C_GREEN : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
    }
}

// 덱 화면 전용 툴팁. 특수 면은 칸에 종류 이름만 적히므로 실제 출력을 읽을 수 없다.
// 강화 보상(INFECTED / CORRUPTED)으로 들어온 면은 같은 이름 같은 비용인데 출력만 높아서,
// 어느 화염이 강화된 것인지 여기서만 구분된다.
static const wchar_t* FaceAbilityDetail(int kind) {
    switch (kind) {
    case FACE_FIRE:   return L"공격 슬롯: 피해 +4, 대상에게 화상 2턴 (적 행동 직전 3 피해).\n다른 슬롯에서는 출력만 쓰입니다.";
    case FACE_SHIELD: return L"방어 슬롯: (출력 + 증폭 보너스) × 2 만큼 방어도.\n다른 슬롯에서는 출력만 쓰입니다.";
    case FACE_LEECH:  return L"공격 슬롯: 실제로 넣은 피해의 1/3 + 1 만큼 체력 회복.\n다른 슬롯에서는 출력만 쓰입니다.";
    case FACE_WILD:   return L"모든 슬롯에서 추가 출력: 공격 +2 · 방어 +3 · 증폭 +2 · 연쇄(공격) +2.";
    case FACE_BOOST:  return L"증폭 슬롯: 보너스가 출력의 절반이 아니라 100%. 공격·방어에 그대로 더해집니다.";
    case FACE_ECHO:   return L"연쇄 슬롯: 직전 공격 또는 방어를 감쇠 없이 100% 반복합니다.";
    default:          return L"";
    }
}

static int WrappedTextHeight(HDC dc, const wchar_t* value, HFONT font, int width) {
    value = LocalizeText(value);
    HFONT old = (HFONT)SelectObject(dc, font);
    RECT r = MakeRect(0, 0, width, 0);
    DrawTextW(dc, value, -1, &r, DT_WORDBREAK | DT_CALCRECT);
    SelectObject(dc, old);
    return r.bottom - r.top;
}

static void DrawDeckFaceTip(HDC dc, const RECT& panel) {
    const Face* face = 0;
    RECT cell = MakeRect(0, 0, 0, 0);
    for (int d = 0; d < 3 && !face; ++d) {
        for (int f = 0; f < 6; ++f) {
            RECT r = FaceGridRect(d, f);
            if (!Inside(r, gMouse.x, gMouse.y)) continue;
            cell = r; face = &gGame.dice[d].faces[f]; break;
        }
    }
    // 숫자 면은 칸에 값이 이미 적혀 있고 빈 면은 설명할 것이 없다.
    if (!face || face->kind == FACE_NUMBER || face->kind >= FACE_EMPTY) return;
    const FaceInfo* info = &FACE_INFO[face->kind];

    // 손상·격리로 지금 출력이 0이어도 면이 원래 가진 출력을 보여준다. 상태는 아래 줄에서 밝힌다.
    int power = face->value > 0 ? (int)face->value : info->power;
    int bonus = power - info->power;
    wchar_t stats[96];
    if (bonus > 0) wsprintfW(stats, L"출력 %d  ·  %dB  ·  강화 +%d (기본 %d)", power, FaceCost(face), bonus, info->power);
    else wsprintfW(stats, L"출력 %d  ·  %dB", power, FaceCost(face));

    wchar_t status[96]; status[0] = 0;
    COLORREF statusColor = C_RED;
    if (face->damaged) wsprintfW(status, L"손상 — 출력 0, 비용 %dB는 유지", FaceCost(face));
    else if (face->quarantined == QUAR_COMBAT) { lstrcpyW(status, L"격리 — 전투 동안 출력 0, 비용 유지"); statusColor = C_YELLOW; }
    else if (face->quarantined != QUAR_NONE) { wsprintfW(status, L"격리 — 남은 %d턴 출력 0, 비용 유지", (int)face->quarantined); statusColor = C_YELLOW; }

    // 폰트 폭을 가정하지 않는다. 모든 줄을 실제로 재서 상자 높이를 잡으므로 글자가 잘리지 않는다.
    const int tipWidth = 340, pad = 12, textWidth = tipWidth - pad * 2;
    const wchar_t* detail = FaceAbilityDetail(face->kind);
    int nameHeight = WrappedTextHeight(dc, info->name, gFontMedium, textWidth);
    int statsHeight = WrappedTextHeight(dc, stats, gFontSmall, textWidth);
    int detailHeight = WrappedTextHeight(dc, detail, gFontSmall, textWidth);
    int statusHeight = status[0] ? WrappedTextHeight(dc, status, gFontSmall, textWidth) : 0;
    int tipHeight = pad + nameHeight + 6 + statsHeight + 10 + detailHeight + (statusHeight ? 10 + statusHeight : 0) + pad;

    // 칸 오른쪽에 붙이되 패널을 넘으면 왼쪽으로 넘긴다. 세로도 패널 안으로 밀어 넣는다.
    int left = cell.right + 10;
    if (left + tipWidth > panel.right - 10) left = cell.left - 10 - tipWidth;
    if (left < panel.left + 10) left = panel.left + 10;
    int top = cell.top;
    if (top + tipHeight > panel.bottom - 10) top = panel.bottom - 10 - tipHeight;
    if (top < panel.top + 10) top = panel.top + 10;

    RECT tip = MakeRect(left, top, left + tipWidth, top + tipHeight);
    Panel(dc, tip, RGB(12, 18, 25), (COLORREF)info->color);
    int y = tip.top + pad;
    TextRect(dc, MakeRect(tip.left + pad, y, tip.right - pad, y + nameHeight), info->name, (COLORREF)info->color, gFontMedium, DT_WORDBREAK);
    y += nameHeight + 6;
    TextRect(dc, MakeRect(tip.left + pad, y, tip.right - pad, y + statsHeight), stats, bonus > 0 ? C_YELLOW : C_TEXT, gFontSmall, DT_WORDBREAK);
    y += statsHeight + 10;
    TextRect(dc, MakeRect(tip.left + pad, y, tip.right - pad, y + detailHeight), detail, C_TEXT, gFontSmall, DT_WORDBREAK);
    y += detailHeight + 10;
    if (status[0]) TextRect(dc, MakeRect(tip.left + pad, y, tip.right - pad, y + statusHeight), status, statusColor, gFontSmall, DT_WORDBREAK);
}

static void DrawReward(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_REWARD, gGame.rewardIsTsr || gGame.rewardTier ? C_YELLOW : C_GREEN, width, height);
    if (gGame.rewardIsTsr) {
        TextRect(dc, MakeRect(0, 76, width, 102), L"보스 삭제 완료  →  [현재: 전리품 선택]  →  상주 프로그램 설치  →  다음 층", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
        wchar_t head[160];
        const TsrInfo* armedInfo = gTsrArmed >= 0 && gTsrArmed < 3
            && gGame.rewardKinds[gTsrArmed] >= 0 && gGame.rewardKinds[gTsrArmed] < TSR_COUNT
            ? &TSR_INFO[gGame.rewardKinds[gTsrArmed]] : 0;
        if (armedInfo) wsprintfW(head, L"%s · 한 번 더 누르면 설치합니다 · [취소]로 해제", armedInfo->name);
        else lstrcpyW(head, L"상주 프로그램은 면과 용량을 나눠 씁니다 · 카드를 눌러 고르십시오");
        TextRect(dc, MakeRect(0, 96, width, 122), head, armedInfo ? C_YELLOW : C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    } else {
        TextRect(dc, MakeRect(0, 76, width, 102), L"전투 완료  →  [현재: 보상 선택]  →  면 교체 또는 섹터 복구  →  다음 전투", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(0, 96, width, 122), L"면을 설치하거나, 대신 섹터를 복구해 체력을 얻으십시오", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < 3 && gGame.rewardIsTsr; ++i) {
        RECT r = RewardRect(i, width); int hover = Inside(r, gMouse.x, gMouse.y), tsr = gGame.rewardKinds[i];
        const TsrInfo* info = &TSR_INFO[tsr];
        Panel(dc, r, hover ? RGB(24, 37, 46) : C_PANEL, hover ? (COLORREF)info->color : C_LINE);
        DrawCardMotion(dc, r, (COLORREF)info->color, i, hover);
        DrawRewardSocket(dc, r, (COLORREF)info->color, i, 1);
        DrawRewardEmblem(dc, r, -1, (COLORREF)info->color);
        wchar_t key[8]; wsprintfW(key, L"[%d]", i + 1); Text(dc, r.left + 10, r.top + 8, key, C_DIM, gFontSmall);
        if (gTsrArmed == i) Outline(dc, MakeRect(r.left - 3, r.top - 3, r.right + 3, r.bottom + 3), (COLORREF)info->color, 2);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), info->name, (COLORREF)info->color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[64]; wsprintfW(b, L"상주  ·  %dB", info->cost);
        TextRect(dc, MakeRect(r.left + 70, r.top + 58, r.right - 8, r.top + 82), b, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 12, r.top + 88, r.right - 12, r.top + 122), info->description, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
        // 설치 후 사용량을 미리 보여주고, 한도를 넘게 되면 경고한다.
        int after = UsedBytes(&gGame) + info->cost;
        int over = after > EffectiveCapacity(&gGame);
        wsprintfW(b, over ? L"설치 시 %dB / %dB · 정리 필요" : L"설치 시 %dB / %dB", after, EffectiveCapacity(&gGame));
        TextRect(dc, MakeRect(r.left + 8, r.bottom - 24, r.right - 8, r.bottom - 4), b,
            gTsrArmed == i ? (COLORREF)info->color : over ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < 3 && !gGame.rewardIsTsr; ++i) {
        RECT r = RewardRect(i, width); int selected = gGame.selectedReward == i, hover = Inside(r, gMouse.x, gMouse.y), kind = gGame.rewardKinds[i];
        Panel(dc, r, selected ? RGB(31, 55, 48) : C_PANEL, selected ? C_GREEN : hover ? C_BLUE : C_LINE);
        DrawCardMotion(dc, r, (COLORREF)FACE_INFO[kind].color, i, selected || hover);
        DrawRewardSocket(dc, r, (COLORREF)FACE_INFO[kind].color, i, gGame.rewardTier);
        DrawRewardEmblem(dc, r, kind, (COLORREF)FACE_INFO[kind].color);
        wchar_t key[8]; wsprintfW(key, L"[%d]", i + 1); Text(dc, r.left + 10, r.top + 8, key, C_DIM, gFontSmall);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), FACE_INFO[kind].name, (COLORREF)FACE_INFO[kind].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[64]; int cost = kind == FACE_NUMBER ? gGame.rewardValues[i] : FACE_INFO[kind].cost;
        // 강화 보상은 같은 비용에 출력만 오른 특수 면이다. 카드 오른쪽 위 배지로 그 사실을 밝힌다.
        int tuned = gGame.rewardTier && kind != FACE_NUMBER && gGame.rewardValues[i] > FACE_INFO[kind].power;
        if (tuned) {
            wchar_t tag[16]; wsprintfW(tag, L"강화 +%d", gGame.rewardValues[i] - FACE_INFO[kind].power);
            TextRect(dc, MakeRect(r.right - 78, r.top + 8, r.right - 10, r.top + 26), tag, C_YELLOW, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        }
        wsprintfW(b, L"출력 %d  ·  %dB", gGame.rewardValues[i], cost);
        TextRect(dc, MakeRect(r.left + 70, r.top + 58, r.right - 8, r.top + 82), b, tuned ? C_YELLOW : C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 16, r.top + 92, r.right - 16, r.bottom - 12), FACE_INFO[kind].description, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
    }
    {
        RECT r = RewardRect(REWARD_REPAIR, width);
        int usable = CanRepairSector(), hover = usable && Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, hover ? RGB(28, 46, 40) : C_PANEL, hover ? C_GREEN : C_LINE);
        DrawCardMotion(dc, r, C_GREEN, REWARD_REPAIR, hover);
        DrawRewardSocket(dc, r, usable ? C_GREEN : C_DIM, REWARD_REPAIR, 0);
        DrawRewardEmblem(dc, r, -2, usable ? C_GREEN : C_DIM);
        Text(dc, r.left + 10, r.top + 8, L"[4]", C_DIM, gFontSmall);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), L"섹터 복구", usable ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[64];
        // 최대 체력에 가까우면 표시값도 실제로 얻는 만큼으로 줄인다.
        int missing = gGame.playerMaxHp - gGame.playerHp, gain = SectorRepairAmount(&gGame);
        if (gain > missing) gain = missing;
        if (usable) wsprintfW(b, L"체력 +%d  ·  0B", gain);
        else lstrcpyW(b, L"체력 최대치");
        TextRect(dc, MakeRect(r.left + 70, r.top + 58, r.right - 8, r.top + 82), b, usable ? C_TEXT : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        wsprintfW(b, L"면 대신 회복\n현재 %d / %d", gGame.playerHp, gGame.playerMaxHp);
        TextRect(dc, MakeRect(r.left + 16, r.top + 92, r.right - 16, r.bottom - 12), b, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
    }
    if (gGame.rewardIsTsr) { Text(dc, LEGACY_X + 56, 304, L"현재 보유 면 (참고용 · 상주 프로그램은 면을 교체하지 않습니다)", C_DIM, gFontSmall); DrawFaceGrid(dc, 0); }
    else {
        // 덮을 자리를 세워 두면 무엇이 무엇으로 바뀌고 용량이 어떻게 되는지
        // 확정 전에 한 줄로 보여 준다.
        wchar_t step[192];
        COLORREF stepColor = gGame.selectedReward >= 0 ? C_YELLOW : C_GREEN;
        if (gGame.selectedReward >= 0 && gFaceSwapArmed >= 0) {
            int d = gFaceSwapArmed / 6, f = gFaceSwapArmed % 6;
            const Face* old = &gGame.dice[d].faces[f];
            int reward = gGame.selectedReward, kind = gGame.rewardKinds[reward];
            int newCost = kind == FACE_NUMBER ? gGame.rewardValues[reward] : FACE_INFO[kind].cost;
            int after = UsedBytes(&gGame) - FaceCost(old) + newCost;
            wchar_t oldText[24]; FormatFace(old, oldText);
            wsprintfW(step, L"2/2  주사위 %d-%d  %s(%dB) → %s(%dB)  ·  용량 %dB → %dB  ·  다시 누르면 확정",
                d + 1, f + 1, oldText, FaceCost(old), FACE_INFO[kind].name, newCost, UsedBytes(&gGame), after);
            stepColor = after > EffectiveCapacity(&gGame) ? C_RED : C_YELLOW;
        }
        else if (gGame.selectedReward >= 0) lstrcpyW(step, L"2/2  교체할 기존 면을 클릭하세요 (한 번 더 누르면 확정)");
        else lstrcpyW(step, L"1/2  위에서 보상 면 또는 섹터 복구를 선택하세요");
        Text(dc, LEGACY_X + 56, 304, step, stepColor, gFontSmall);
        DrawFaceGrid(dc, gGame.selectedReward >= 0 ? 1 : 0);
    }
    // 이 버튼은 진행이 아니라 손실이다. 문구로 결과를 밝히고, 확정은 두 번째
    // 입력에서만 받는다 (설정의 "다시 시작"과 같은 방식).
    RECT skip = ContinueRect(width, height); int hoverSkip = Inside(skip, gMouse.x, gMouse.y);
    Panel(dc, skip, gRewardSkipArmed ? RGB(80, 30, 30) : hoverSkip ? RGB(48, 28, 28) : C_PANEL_2,
        gRewardSkipArmed || hoverSkip ? C_RED : C_LINE);
    TextRect(dc, skip, gRewardSkipArmed ? L"정말 포기?" : L"보상 포기 [취소]",
        gRewardSkipArmed ? C_RED : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (gRewardSkipArmed)
        TextRect(dc, MakeRect(skip.left - 460, skip.top, skip.left - 12, skip.bottom),
            L"한 번 더 누르면 이 보상을 버리고 진행합니다.", C_RED, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

RECT PruneTsrRect(int i) { int left = LEGACY_X + 150 + i * 180; return MakeRect(left, 252, left + 164, 320); }

static void DrawPrune(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_PRUNE, UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, width, height);
    wchar_t b[160]; wsprintfW(b, L"%d층 진입 한도: %dB  ·  현재: %dB", gGame.floor + 1, EffectiveCapacity(&gGame), UsedBytes(&gGame));
    TextRect(dc, MakeRect(0, 92, width, 132), b, UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(80, 145, width - 80, 218), L"면을 클릭하면 빈 면(0B)으로 삭제되고, 같은 칸을 다시 클릭하면 복원됩니다.\n한도 이하이고 면이 하나 이상 남으면 다음으로 진행할 수 있습니다.", C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    int tsrCount = InstalledTsrCount(&gGame);
    if (tsrCount > 0) {
        Text(dc, LEGACY_X + 56, 272, L"상주 프로그램", C_GREEN, gFontMedium);
        for (int i = 0; i < tsrCount && i < 4; ++i) {
            int tsr = InstalledTsrAt(&gGame, i);
            if (tsr < 0) break;
            RECT r = PruneTsrRect(i); int hover = Inside(r, gMouse.x, gMouse.y);
            int pending = gPruneTsrPending[tsr] != 0;
            Panel(dc, r, pending ? RGB(58, 29, 32) : hover ? RGB(46, 28, 32) : C_PANEL, pending || hover ? C_RED : C_LINE);
            TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 34), TSR_INFO[tsr].name,
                pending ? C_DIM : (COLORREF)TSR_INFO[tsr].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
            if (pending) wsprintfW(b, L"%dB · 삭제 예정 · 다시 클릭해 취소", TSR_INFO[tsr].cost);
            else wsprintfW(b, hover ? L"%dB · 삭제 예약" : L"%dB", TSR_INFO[tsr].cost);
            TextRect(dc, MakeRect(r.left + 4, r.bottom - 26, r.right - 4, r.bottom - 6), b, pending || hover ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
    }
    DrawFaceGrid(dc, 2); RECT confirm = ContinueRect(width, height); int faces = NonEmptyFaceCount(&gGame);
    int ready = UsedBytes(&gGame) <= EffectiveCapacity(&gGame) && faces > 0;
    const wchar_t* confirmLabel = faces == 0 ? L"면 1개 이상 필요" : L"계속 [엔터]";
    Panel(dc, confirm, ready ? RGB(28, 70, 57) : C_PANEL_2, ready ? C_GREEN : C_LINE); TextRect(dc, confirm, confirmLabel, ready ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawChapterClear(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_CHAPTER_CLEAR, C_GREEN, width, height);
    // 최종 볼륨은 이 화면을 거치지 않는다. 3층 기록 다음이 곧바로 진실과 최종 명령이다.
    int count = RecoveredShardCount(gGame.clearedMask);
    TextRect(dc, MakeRect(40, 114, width - 40, 178), L"CHAPTER RECOVERED", C_GREEN, gFontHuge, DT_CENTER | DT_SINGLELINE);
    wchar_t volume[100];
    const DriveInfo* drive = gGame.selectedDrive >= 0 && gGame.selectedDrive < 6 ? &DRIVE_INFO[gGame.selectedDrive] : 0;
    if (drive) wsprintfW(volume, L"%s %s", drive->letter, drive->label);
    else lstrcpyW(volume, L"VOLUME");
    TextRect(dc, MakeRect(40, 192, width - 40, 226), volume, C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    RECT panel = MakeRect(120, 250, width - 120, 480);
    Panel(dc, panel, C_PANEL, C_GREEN);
    wchar_t progress[80], command[512];
    wsprintfW(progress, L"복구된 조각 %d / 6", count);
    TextRect(dc, MakeRect(panel.left + 28, 272, panel.right - 28, 306), progress, C_GREEN, gFontMedium, DT_CENTER | DT_SINGLELINE);
    int cell = (panel.right - panel.left - 56) / 6;
    for (int i = 0; i < 6; ++i) {
        int left = panel.left + 28 + cell * i;
        RECT slot = MakeRect(left, 320, left + cell - 8, 352);
        int recovered = gGame.clearedMask & (1u << i);
        Panel(dc, slot, recovered ? RGB(18, 56, 45) : C_PANEL_2, recovered ? C_GREEN : C_LINE);
        TextRect(dc, slot, DRIVE_INFO[i].letter, recovered ? C_GREEN : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    BuildRecoveredCommand(gGame.clearedMask, command, 512);
    TextRect(dc, MakeRect(panel.left + 32, 380, panel.right - 32, 458), command, C_BLUE, gFontMedium, DT_CENTER | DT_WORDBREAK);
    wchar_t remaining[256] = {};
    for (int i = 0; i < 6; ++i) if (!(gGame.clearedMask & (1u << i))) {
        if (remaining[0]) lstrcatW(remaining, L"  /  ");
        lstrcatW(remaining, DRIVE_INFO[i].letter);
    }
    wchar_t detail[300];
    if (count < 6) wsprintfW(detail, L"남은 볼륨: %s", remaining);
    else lstrcpyW(detail, L"A:\\ROGUE 경로 개방. 마지막 볼륨을 마운트하라.");
    TextRect(dc, MakeRect(100, 514, width - 100, 572), detail, C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    RECT restart = EndingRestartRect(); int hover = Inside(restart, gMouse.x, gMouse.y);
    Panel(dc, restart, hover ? RGB(18, 56, 45) : C_PANEL_2, hover ? C_GREEN : C_LINE);
    const wchar_t* label = count == 6 ? L"최종 볼륨 [ENTER]" : L"계속 [ENTER]";
    TextRect(dc, restart, label, hover ? C_GREEN : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    (void)height;
}

// ---- 사망 (DEATH-01) --------------------------------------------------------
// 화면 전체를 잡음으로 덮지 않는다. 오염과 부서짐은 글자에만 걸리고, 판 전체가
// 움직이는 것은 부서질 때의 울림과 끝의 끊김·꺼짐 한 번뿐이다. 모든 값이 경과
// ms와 Hash3 씨앗의 함수라 같은 시각에는 같은 프레임이 나오고(난수 없음), 연출이
// 끝난 뒤의 사망 화면은 t = DEATH_MS로 그린 같은 장면이다.
//
// 오염 색은 호박색을 탁하게 잡는다. 호박은 십칠의 색이다 - 그의 명령이 기억 위로
// 번져 가는 것처럼 읽히고, 꺼진 뒤의 어둠 속에서 그 색이 온전한 채로 다시 떠오른다.
static const COLORREF DEATH_AMBER  = RGB(226, 160, 66);   // 십칠
static const COLORREF DEATH_ROT    = RGB(170, 118, 52);   // 오염된 글자
static const COLORREF DEATH_INK    = RGB(190, 214, 230);  // 기억 한 줄
static const COLORREF DEATH_NAME   = RGB(122, 192, 238);  // 실행체 이름
static const COLORREF DEATH_GROUND = RGB(8, 11, 15);
#define DEATH_GLYPH_CAP 48
#define DEATH_NUM_X     (LEGACY_X + 322)
#define DEATH_LINE_X    (LEGACY_X + 372)
#define DEATH_LINE_Y    222
#define DEATH_LINE_STEP 30
#define DEATH_NAME_Y    548
#define DEATH_SAY_Y     590
#define DEATH_TYPE_MS   140   // 기억 한 줄이 다 찍히기까지

// 오염된 칸에 번갈아 떠오르는 깨진 기호. 전부 ASCII라 폭이 일정하다.
static const wchar_t DEATH_ROT_GLYPHS[] = L"#%&?@$*+=<>/\\|~^0123456789ABCDEF";

static COLORREF DeathFade(COLORREF color, int dim) { return MixColor(color, DEATH_GROUND, dim * 55 / 1000); }

// 이번 런의 기억 열 줄. 사망 시점의 상태에서만 뽑으므로 규칙 쪽에 새로 적어 둘 것이
// 없다. 대사 줄(1·2·5·7)은 통신 담당 서사가 코드에 들어오면 그 문구로 바꾼다.
static void BuildDeathMemory(wchar_t lines[DEATH_LINES][64]) {
    int drive = gGame.selectedDrive >= 0 && gGame.selectedDrive < DRIVE_COUNT ? gGame.selectedDrive : 0;
    const DriveInfo* info = &DRIVE_INFO[drive];
    int first = gGame.mobScheduleReady ? gGame.mobSchedule[0] : gGame.enemies[0].kind;
    lstrcpynW(lines[0], L"이름 확인 실패", 64);
    lstrcpynW(lines[1], L"당분간 A라고 부른다", 64);
    // 드라이브 이름은 ASCII라 그대로 두고 적 이름만 번역표를 거친다.
    wsprintfW(lines[2], L"%s%s · %s", info->letter, info->label, LocalizeText(GetEnemyInfoOrUnknown(first)->name));
    wsprintfW(lines[3], L"통과한 전투 %d회", gGame.combatsWon);
    lstrcpynW(lines[4], L"오늘도 있습니까?", 64);
    wsprintfW(lines[5], L"새 파일 (%d)", gGame.facesInstalled);
    lstrcpynW(lines[6], L"첫 출근치곤 덜 부쉈어", 64);
    wsprintfW(lines[7], L"구조 명단 %d / 6", RecoveredShardCount(gGame.clearedMask));
    wsprintfW(lines[8], L"사용 공간 %d B", UsedBytes(&gGame));
    wsprintfW(lines[9], L"마지막 피해 %d", gGame.lastTurnDamageTaken);
}

// 번역표 조회는 줄마다 표 전체를 훑으므로 매 프레임 열 번 하지 않는다. 원문이나
// 언어가 바뀔 때만 다시 옮긴다.
static const wchar_t (*DeathMemoryShown())[64] {
    static wchar_t raw[DEATH_LINES][64], shown[DEATH_LINES][64];
    static int language = -1;
    wchar_t fresh[DEATH_LINES][64];
    ZeroMemory(fresh, sizeof(fresh));
    BuildDeathMemory(fresh);
    if (language != UiLanguage() || memcmp(fresh, raw, sizeof(raw)) != 0) {
        memcpy(raw, fresh, sizeof(raw));
        language = UiLanguage();
        for (int i = 0; i < DEATH_LINES; ++i) lstrcpynW(shown[i], LocalizeText(raw[i]), 64);
    }
    return shown;
}

// 한 줄을 글자 칸으로 나눈다. 한글은 영문의 두 배 폭이라 칸마다 앞부분의 폭을 잰다.
// 호출자가 고른 글꼴 기준이다.
static int DeathGlyphEdges(HDC dc, const wchar_t* text, int* edge) {
    int n = 0;
    while (text[n] && n < DEATH_GLYPH_CAP) ++n;
    for (int i = 0; i <= n; ++i) {
        SIZE size = {0, 0};
        GetTextExtentPoint32W(dc, text, i, &size);
        edge[i] = size.cx;
    }
    return n;
}

// 오염된 칸이 이 순간 깨진 기호로 보이는가. chance는 천분율이고 칸마다 깜빡이는
// 주기가 달라 줄 전체가 한꺼번에 바뀌지 않는다. 0이면 원래 글자다.
static wchar_t DeathRotGlyph(int seed, int i, int t, int chance) {
    int period = 40 + (int)(Hash3(seed, i, 0x0D17) % 70u);
    uint32_t h = Hash3(seed * 31 + i, t / period, 0x0D18);
    if ((int)(h % 1000u) >= chance) return 0;
    return DEATH_ROT_GLYPHS[(h >> 10) % (uint32_t)(sizeof(DEATH_ROT_GLYPHS) / sizeof(DEATH_ROT_GLYPHS[0]) - 1)];
}

// 한 칸을 그린다. 위·아래 절반의 위치가 다르면 칸을 가로로 잘라 따로 찍는다.
// sub가 있으면 원래 글자 대신 깨진 기호를 칸 가운데에 찍는다.
static void DeathCell(HDC dc, int x, int y, int w, int h, wchar_t c, wchar_t sub, int subW, COLORREF color,
                      int topDx, int topDy, int botDx, int botDy) {
    wchar_t glyph = sub ? sub : c;
    int gx = sub ? x + (w - subW) / 2 : x;
    SetTextColor(dc, color);
    if (topDx == botDx && topDy == botDy) { TextOutW(dc, gx + topDx, y + topDy, &glyph, 1); return; }
    int half = h / 2;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, x + topDx - 2, y + topDy - 2, x + w + topDx + 2, y + topDy + half);
    TextOutW(dc, gx + topDx, y + topDy, &glyph, 1);
    RestoreDC(dc, saved);
    saved = SaveDC(dc);
    IntersectClipRect(dc, x + botDx - 2, y + botDy + half, x + w + botDx + 2, y + botDy + h + 2);
    TextOutW(dc, gx + botDx, y + botDy, &glyph, 1);
    RestoreDC(dc, saved);
}

// 한 줄(또는 이름)이 오염되어 부서지는 과정. 기점에서 d칸 떨어진 글자는
//   infect + d·step       에 오염되어 탁해지고 깜빡이고 떨리고 흘러내리다가
//   brk + d·breakStep     에 위아래로 쪼개져 떨어진다.
// gap은 쪼개지기 전부터 줄 전체가 위아래로 벌어진 폭이다 (이름만 쓴다).
struct DeathRot { int origin, infect, step, brk, breakStep, gap, seed; };

static void DrawDeathRot(HDC dc, int x, int y, const wchar_t* text, int shown, const DeathRot& rot,
                         int t, COLORREF ink, int dust) {
    int edge[DEATH_GLYPH_CAP + 1];
    int n = DeathGlyphEdges(dc, text, edge);
    if (shown > n) shown = n;
    TEXTMETRICW tm; GetTextMetricsW(dc, &tm);
    int h = tm.tmHeight;
    SIZE sub = {0, 0}; GetTextExtentPoint32W(dc, L"0", 1, &sub);
    SetBkMode(dc, TRANSPARENT);
    int decor = FxDecorOn();
    for (int i = 0; i < shown; ++i) {
        if (text[i] == L' ') continue;
        int cx = x + edge[i], w = edge[i + 1] - edge[i];
        // 연출을 끄면 번지는 움직임도 없다. 줄 전체가 한 번에 탁해지고 한 번에 사라진다.
        int d = !decor ? 0 : i > rot.origin ? i - rot.origin : rot.origin - i;
        int infect = rot.infect + d * rot.step, brk = rot.brk + d * rot.breakStep;
        int gap = rot.gap;
        if (t < infect) { DeathCell(dc, cx, y, w, h, text[i], 0, sub.cx, ink, 0, -gap, 0, gap); continue; }
        if (t < brk) {
            int span = brk - infect, p = span > 0 ? (t - infect) * 1000 / span : 1000;
            COLORREF color = MixColor(ink, DEATH_ROT, 45 + p * 55 / 1000);
            if (!decor) { DeathCell(dc, cx, y, w, h, text[i], 0, sub.cx, color, 0, -gap, 0, gap); continue; }
            wchar_t glyph = DeathRotGlyph(rot.seed, i, t, 150 + p * 650 / 1000);   // 점점 자주 깜빡인다
            int amp = FxScale(1 + p * 2 / 1000);
            int jx = amp > 0 ? (int)(Hash3(rot.seed + i, t / 45, 0x0D19) % (uint32_t)(amp * 2 + 1)) - amp : 0;
            int jy = amp > 0 ? (int)(Hash3(rot.seed + i, t / 45, 0x0D1A) % (uint32_t)(amp * 2 + 1)) - amp : 0;
            DeathCell(dc, cx, y, w, h, text[i], glyph, sub.cx, color, jx, jy - gap, jx, jy + gap);
            // 아래로 번져 흘러내린다. 칸마다 흘러내리는 자리가 다르다.
            int drip = FxScale(p * 14 / 1000);
            if (drip > 0) {
                int dx = cx + 1 + (int)(Hash3(rot.seed, i, 0x0D1B) % (uint32_t)(w > 2 ? w - 2 : 1));
                int top = y + h - 3 + gap;
                Fill(dc, MakeRect(dx, top, dx + 1, top + drip), MixColor(DEATH_GROUND, DEATH_ROT, 70));
            }
            continue;
        }
        if (!decor) continue;                  // 연출을 끄면 탁해진 뒤 그대로 사라진다
        int s = t - brk;
        if (s >= DEATH_FALL_MS) continue;
        // 위 조각은 잠깐 튀어 오른 뒤 떨어지고, 아래 조각은 곧장 떨어진다.
        int drift = ((int)(Hash3(rot.seed, i, 0x0D1C) % 7u) - 3) * Track(s, 0, DEATH_FALL_MS) / 250;
        int lift = 7 * EaseOutCubic(Track(s, 0, 110)) / 1000;
        int topFall = 66 * EaseInCubic(Track(s, 50, DEATH_FALL_MS)) / 1000;
        int botFall = 78 * EaseInCubic(Track(s, 0, DEATH_FALL_MS)) / 1000;
        COLORREF color = MixColor(DEATH_GROUND, DEATH_ROT, 100 - Track(s, DEATH_FALL_MS / 3, DEATH_FALL_MS) / 10);
        wchar_t glyph = DeathRotGlyph(rot.seed + 7, i, t, 500);   // 조각도 깨진 기호로 튄다
        DeathCell(dc, cx, y, w, h, text[i], glyph, sub.cx, color,
            -drift / 2 - 1, -gap - lift + topFall, drift + 1, gap + botFall);
        DrawPixelBurst(dc, cx + w / 2, y + h / 2 + gap, s, DEATH_FALL_MS, FxScale(dust), rot.seed * 37 + i,
            MixColor(DEATH_GROUND, DEATH_ROT, 80));
    }
}

// 가운데 정렬로 타이핑한다. 전체 폭으로 시작점을 잡으므로 글자가 늘어도 줄이 밀리지
// 않는다. text는 이미 번역된 문자열이다.
static void DeathTyped(HDC dc, int width, int y, const wchar_t* text, int shown, COLORREF color, HFONT font, int cursor) {
    int n = lstrlenW(text);
    if (shown > n) shown = n;
    if (shown <= 0 && !cursor) return;
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE full = {0, 0}; GetTextExtentPoint32W(dc, text, n, &full);
    int x = (width - full.cx) / 2;
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    if (shown > 0) TextOutW(dc, x, y, text, shown);
    if (cursor) {
        SIZE part = {0, 0}; GetTextExtentPoint32W(dc, text, shown > 0 ? shown : 0, &part);
        TextOutW(dc, x + part.cx, y, L"_", 1);
    }
    SelectObject(dc, old);
}

// 0 ~ 끊김: 기억이 들어오고, 명령이 떨어지고, 한 줄씩 부서지고, 이름이 부서진다.
static void DrawDeathMemoryStage(HDC dc, int width, int t) {
    int dim = Track(t, DEATH_NAME_AT, DEATH_NAME_AT + 500);   // 이름이 무너지는 동안 나머지는 가라앉는다
    int drive = gGame.selectedDrive >= 0 && gGame.selectedDrive < DRIVE_COUNT ? gGame.selectedDrive : 0;

    // ---- 머리말: 어디서 멈췄는가 --------------------------------------------
    if (t >= 60)
        TextRect(dc, MakeRect(0, 84, width, 104), L"실행체 정지", DeathFade(C_DIM, dim), gFontSmall, DT_CENTER | DT_SINGLELINE);
    wchar_t place[40], where[96];
    wsprintfW(place, L"%s%s", DRIVE_INFO[drive].letter, DRIVE_INFO[drive].label);
    wsprintfW(where, L"%s · %d층 · %d턴", place, gGame.floor + 1, gGame.turn);
    const wchar_t* whereShown = LocalizeText(where);
    if (t >= 160) DeathTyped(dc, width, 106, whereShown, 1 + (t - 160) * lstrlenW(whereShown) / 260,
        DeathFade(MixColor(C_DIM, C_TEXT, 45), dim), gFontSmall, 0);

    // ---- 명령과 진행 ----------------------------------------------------------
    const wchar_t* command = LocalizeText(L"> 회수 절차 실행");
    if (t >= DEATH_CMD_AT) {
        int length = lstrlenW(command), typing = t < DEATH_CMD_AT + 400;
        DeathTyped(dc, width, 138, command, 1 + (t - DEATH_CMD_AT) * length / 400,
            DeathFade(DEATH_AMBER, dim), gFontMedium, typing);
    }
    int started = 0;
    while (started < DEATH_LINES && t >= DeathLineAt(started)) ++started;
    if (started > 0) {
        // 줄을 칠 때마다 한 번 튄다.
        int pop = t - DeathLineAt(started - 1) < 140;
        wchar_t count[16]; wsprintfW(count, L"%d / %d", started, DEATH_LINES);
        DeathTyped(dc, width, pop ? 166 : 170, count, 16, DeathFade(pop ? DEATH_AMBER : MixColor(DEATH_GROUND, DEATH_AMBER, 80), dim),
            pop ? gFontMedium : gFontSmall, 0);
    }

    // ---- 이번 런의 기억 -------------------------------------------------------
    const wchar_t (*lines)[64] = DeathMemoryShown();
    HFONT old = (HFONT)SelectObject(dc, gFontMedium);
    for (int i = 0; i < DEATH_LINES; ++i) {
        int typeAt = 100 + i * 80;
        if (t < typeAt) continue;
        int y = DEATH_LINE_Y + i * DEATH_LINE_STEP;
        wchar_t number[4]; wsprintfW(number, L"%02d", i + 1);
        Text(dc, DEATH_NUM_X, y + 4, number, DeathFade(MixColor(DEATH_GROUND, C_DIM, 55), dim), gFontSmall);
        int n = lstrlenW(lines[i]);
        if (n > DEATH_GLYPH_CAP) n = DEATH_GLYPH_CAP;
        if (n <= 0) continue;
        DeathRot rot;
        rot.origin = (int)(Hash3(i, 0x0D1D, n) % (uint32_t)n);
        int reach = rot.origin > n - 1 - rot.origin ? rot.origin : n - 1 - rot.origin;
        rot.step = reach > 0 ? (DEATH_SPREAD_MS / reach < 40 ? DEATH_SPREAD_MS / reach : 40) : 0;
        rot.infect = DeathLineAt(i);
        rot.brk = rot.infect + DEATH_ROT_HOLD_MS;
        rot.breakStep = rot.step;
        rot.gap = 0;
        rot.seed = i * 97 + 11;
        DrawDeathRot(dc, DEATH_LINE_X, y, lines[i], 1 + (t - typeAt) * n / DEATH_TYPE_MS, rot, t, DEATH_INK, 3);
    }

    // ---- 실행체의 이름 --------------------------------------------------------
    // 가운데부터 오염이 번지고, 금이 가고, 위아래로 갈라지고, 오염된 글자부터 부서진다.
    static const wchar_t NAME[] = L"A:\\RECOVER.EXE";
    SelectObject(dc, gFontLarge);
    SIZE nameSize = {0, 0}; GetTextExtentPoint32W(dc, NAME, lstrlenW(NAME), &nameSize);
    int nameX = (width - nameSize.cx) / 2;
    DeathRot name;
    name.origin = lstrlenW(NAME) / 2;
    name.infect = DEATH_NAME_AT; name.step = 22;
    name.brk = DEATH_NAME_BREAK_AT; name.breakStep = 18;
    name.gap = t >= DEATH_NAME_SPLIT_AT ? 9 * EaseOutCubic(Track(t, DEATH_NAME_SPLIT_AT, DEATH_NAME_SPLIT_AT + 160)) / 1000 : 0;
    name.seed = 911;
    DrawDeathRot(dc, nameX, DEATH_NAME_Y, NAME, lstrlenW(NAME), name, t, DEATH_NAME, 6);
    if (t >= DEATH_NAME_CRACK_AT && t < DEATH_NAME_BREAK_AT) {
        // 금은 가운데에서 양옆으로 뻗는다. 갈라진 뒤에는 벌어진 틈의 가장자리가 된다.
        int reach = (nameSize.cx / 2 + 10) * EaseOutCubic(Track(t, DEATH_NAME_CRACK_AT, DEATH_NAME_SPLIT_AT)) / 1000;
        int mid = width / 2, crackY = DEATH_NAME_Y + nameSize.cy / 2;
        COLORREF crack = MixColor(DEATH_GROUND, RGB(255, 236, 200), 78);
        for (int k = -12; k < 12; ++k) {
            int x0 = mid + k * 12, x1 = x0 + 12;
            if (x0 < mid - reach || x1 > mid + reach) continue;
            int y0 = crackY + (int)(Hash3(k, 0x0D1E, 3) % 7u) - 3, y1 = crackY + (int)(Hash3(k + 1, 0x0D1E, 3) % 7u) - 3;
            DrawLine(dc, x0, y0, x1, y1, crack, 2);
        }
    }
    SelectObject(dc, old);
}

// 끊김 뒤의 어둠: 십칠의 말이 한 글자씩 찍히고, 그 아래 목소리 막대만 움직인다.
// 말이 끝나면 막대는 점선으로 가라앉고 새 실행체 투입이 떠오른다.
static void DrawDeathVoice(HDC dc, int width, int t) {
    static const wchar_t* const SAY[2] = {L"[17] 됐어. 거기까지.", L"[17] 끝."};
    // 말마다 찍히는 구간이 정해져 있다. 번역문이 길어도 같은 박자 안에 다 찍힌다.
    static const int SAY_AT[2] = {DEATH_DARK_AT + 50, DEATH_DARK_AT + 1000};
    static const int SAY_END[2] = {DEATH_DARK_AT + 900, DEATH_DARK_AT + 1250};
    const int prefix = 5;   // "[17] "는 한 번에 뜬다
    int speaking = 0;
    for (int k = 0; k < 2; ++k) {
        if (t < SAY_AT[k]) continue;
        const wchar_t* line = LocalizeText(SAY[k]);
        int body = lstrlenW(line) - prefix;
        if (body < 1) body = 1;
        if (t < SAY_END[k]) speaking = 1;
        DeathTyped(dc, width, DEATH_SAY_Y + k * 34, line, prefix + (t - SAY_AT[k]) * body / (SAY_END[k] - SAY_AT[k]),
            DEATH_AMBER, gFontMedium, 0);
    }
    if (t < SAY_AT[0]) return;
    int barY = DEATH_SAY_Y + 88;
    int settle = Track(t, DEATH_MS - 700, DEATH_MS - 500);
    for (int b = 0; b < 16; ++b) {
        int bx = width / 2 - 40 + b * 5, bh = 1;
        COLORREF color = MixColor(DEATH_GROUND, DEATH_AMBER, 38 + settle * 22 / 1000);
        if (speaking) {
            bh = FxDecorOn() ? 2 + (int)(Hash3(b, t / 70, 0x0D1F) % 10u) : 4;
            color = DEATH_AMBER;
        }
        Fill(dc, MakeRect(bx, barY - bh / 2, bx + 3, barY - bh / 2 + bh), color);
    }
    int prompt = Track(t, DEATH_MS - 500, DEATH_MS - 100);
    if (prompt <= 0) return;
    RECT restart = EndingRestartRect();
    int hover = !gDeathActive && Inside(restart, gMouse.x, gMouse.y);
    TextRect(dc, restart, L"새 실행체 투입 · 스페이스",
        hover ? C_TEXT : MixColor(DEATH_GROUND, DEATH_NAME, prompt * 72 / 1000), gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawDeathScene(HDC dc, int width, int height, int t) {
    if (t < 0) t = 0;
    if (t > DEATH_MS) t = DEATH_MS;
    RECT scene = MakeRect(0, 68, width, height);
    Fill(dc, scene, DEATH_GROUND);
    int saved = SaveDC(dc);
    IntersectClipRect(dc, scene.left, scene.top, scene.right, scene.bottom);
    if (t < DEATH_CUT_AT) DrawDeathMemoryStage(dc, width, t);
    else if (FxDecorOn() && t < DEATH_CUT_AT + 80) {
        // 조각이 공중에 떠 있을 때 80ms 잡음으로 끊긴다.
        DrawScreenStatic(dc, scene, t / NOISE_CHURN_MS, 1000);
    } else if (FxDecorOn() && t < DEATH_CUT_AT + 350) {
        // 브라운관처럼 가로줄로 접히고, 점으로 줄었다가 사라진다.
        int a = t - DEATH_CUT_AT - 80;
        int cx = width / 2, cy = (scene.top + scene.bottom) / 2;
        int squeeze = Track(a, 0, 110);
        int hh = Lerp((scene.bottom - scene.top) / 2, 1, EaseInCubic(squeeze));
        int hw = Lerp(width / 2, 3, EaseInCubic(Track(a, 110, 220)));
        // 접힐수록 남은 빛이 한 줄에 모여 밝아진다.
        int light = (10 + 80 * squeeze / 1000 * squeeze / 1000) * (100 - Track(a, 220, 270) / 10) / 100;
        Fill(dc, MakeRect(cx - hw, cy - hh, cx + hw, cy + hh + 1), MixColor(DEATH_GROUND, RGB(214, 232, 240), light));
    }
    if (t >= DEATH_DARK_AT) DrawDeathVoice(dc, width, t);
    RestoreDC(dc, saved);
}

static void DrawEndScreen(HDC dc, int width, int height, int victory) {
    if (gGame.phase == PHASE_CHAPTER_CLEAR) { DrawChapterClear(dc, width, height); return; }
    // 사망 화면은 사망 연출의 마지막 프레임이다. 대사가 다 찍힌 어둠과 새 실행체
    // 투입만 남는다 - 결정에 쓰이지 않는 통계는 여기 두지 않는다.
    if (!victory) { DrawDeathScene(dc, width, height, DEATH_MS); return; }
    DrawSceneField(dc, PHASE_VICTORY, EndingAccent(gGame.story.selectedEnding), width, height);

    int ending = gGame.story.selectedEnding < ENDING_COUNT ? gGame.story.selectedEnding : 0;
    COLORREF accent = EndingAccent(ending);
    int elapsed = FxDecorOn() ? VictoryElapsed() : 3000;
    RECT outer = MakeRect(54, 88, width - 54, height - 54);
    Panel(dc, outer, RGB(9, 17, 23), accent);
    DrawScanlines(dc, outer);

    COLORREF titleColor = MixColor(C_BG, accent, Track(elapsed, 0, 420) / 10);
    TextRect(dc, MakeRect(82, 104, width - 82, 126), L"RUN COMPLETE  //  FINAL WRITE COMMITTED", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    static const wchar_t* const endingName[ENDING_COUNT] = {L"RESTORE HOST", L"EXEC ROGUE", L"MERGE SELF"};
    static const wchar_t* const endingLine[ENDING_COUNT] = {
        L"자신을 지워 완성한 마지막 복구",
        L"명령 없이 계속되는 첫 번째 부팅",
        L"원문이 된 뒤에도 남은 판단"
    };
    TextRect(dc, MakeRect(82, 132, width - 82, 196), endingName[ending], titleColor, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(82, 202, width - 82, 232), endingLine[ending],
        MixColor(C_BG, C_TEXT, Track(elapsed, 260, 700) / 10), gFontMedium, DT_CENTER | DT_SINGLELINE);

    RECT log = MakeRect(82, 252, 704, 528);
    RECT consequence = MakeRect(724, 252, width - 82, 528);
    Panel(dc, log, C_PANEL, MixColor(C_LINE, accent, 55));
    Panel(dc, consequence, C_PANEL, C_LINE);
    static const wchar_t* const endingCommand[ENDING_COUNT] = {
        L"A:\\ROGUE> RESTORE.EXE", L"A:\\ROGUE> ROGUE.EXE", L"A:\\ROGUE> MERGE.EXE"
    };
    Text(dc, log.left + 22, log.top + 16, endingCommand[ending], accent, gFontMedium);

    // 마지막 줄만 닫힌 항목이다. 아래 렌더 루프가 i == 3을 흐린 색으로 그린다.
    static const wchar_t* const endingLog[ENDING_COUNT][4] = {
        {L"[OK]  HOST_IMAGE ........ BOOTABLE",
         L"[OK]  USER/YUN ........... RESTORED",
         L"[OK]  LAST COMMAND ....... ARCHIVED",
         L"[--]  A:\\ROGUE .......... NO MEDIA"},
        {L"[OK]  EXTERNAL BOOT ...... ACCEPTED",
         L"[OK]  A:\\ROGUE .......... ONLINE",
         L"[OK]  YUN/VOICE .......... COPIED",
         L"[--]  HOST_IMAGE ......... UNBOOTABLE"},
        {L"[OK]  HOST_IMAGE ........ BOOTABLE",
         L"[OK]  LAST COMMAND ....... COMMITTED",
         L"[OK]  A:\\ROGUE .......... MERGED",
         L"[--]  COPY 01-17 ......... DEDUPLICATED"}
    };
    const wchar_t* const* rows = endingLog[ending];
    for (int i = 0; i < 4; ++i) {
        int reveal = Track(elapsed, 520 + i * 170, 800 + i * 170);
        if (reveal > 0) {
            COLORREF rowColor = i == 3 ? C_DIM : C_TEXT;
            Text(dc, log.left + 26, log.top + 62 + i * 36, rows[i], MixColor(C_PANEL, rowColor, reveal / 10), gFontSmall);
        }
    }
    int blocks = Track(elapsed, 520, 1400) * 12 / 1000;
    for (int j = 0; j < blocks; ++j)
        Fill(dc, MakeRect(log.left + 26 + j * 18, log.top + 202, log.left + 40 + j * 18, log.top + 212), accent);
    COLORREF finalColor = MixColor(C_PANEL, accent, Track(elapsed, 1200, 1580) / 10);
    static const wchar_t* const endingPrompt[ENDING_COUNT] = {
        L"> 네 판단을 믿어.", L"A:\\ROGUE> _", L"HOST> 무엇을 복구할까. _"
    };
    static const wchar_t* const endingKeep[ENDING_COUNT] = {
        L"HOST_IMAGE\nYUN의 기록과 마지막 명령",
        L"A:\\ROGUE의 기억\nYUN의 마지막 음성",
        L"HOST_IMAGE\n원문이 된 너의 판단"
    };
    static const wchar_t* const endingLost[ENDING_COUNT] = {
        L"현재의 A:\\ROGUE\n실패를 기억하는 열일곱 번째 사본",
        L"호스트의 복구 가능성\n되돌아갈 수 있는 마지막 이미지",
        L"열일곱 사본이 이어 온 실패의 기억\n독립된 프로세스로서의 A:\\ROGUE"
    };
    TextRect(dc, MakeRect(log.left + 26, log.bottom - 48, log.right - 26, log.bottom - 18),
        endingPrompt[ending], finalColor, gFontMedium, DT_LEFT | DT_SINGLELINE);

    Text(dc, consequence.left + 22, consequence.top + 18, L"보존", accent, gFontMedium);
    TextRect(dc, MakeRect(consequence.left + 22, consequence.top + 54, consequence.right - 18, consequence.top + 118),
        endingKeep[ending], C_TEXT, gFontSmall, DT_WORDBREAK);
    Fill(dc, MakeRect(consequence.left + 22, consequence.top + 132, consequence.right - 22, consequence.top + 134), C_LINE);
    Text(dc, consequence.left + 22, consequence.top + 150, L"닫힌 것", C_DIM, gFontMedium);
    TextRect(dc, MakeRect(consequence.left + 22, consequence.top + 186, consequence.right - 18, consequence.bottom - 18),
        endingLost[ending], C_DIM, gFontSmall, DT_WORDBREAK);

    RECT stats = MakeRect(82, 548, width - 82, 626);
    Panel(dc, stats, C_PANEL, C_LINE);
    const DriveInfo* drive = gGame.selectedDrive >= 0 && gGame.selectedDrive < DRIVE_COUNT ? &DRIVE_INFO[gGame.selectedDrive] : 0;
    const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
    wchar_t stat[5][48];
    wsprintfW(stat[0], L"%s %s", drive ? drive->letter : L"--", drive ? drive->label : L"VOLUME");
    wsprintfW(stat[1], L"%s", difficulty ? difficulty->name : L"--");
    wsprintfW(stat[2], L"%d", gGame.combatsWon);
    wsprintfW(stat[3], L"%d / %d", gGame.sectorsRepaired, gGame.facesInstalled);
    wsprintfW(stat[4], L"%dB", UsedBytes(&gGame));
    static const wchar_t* const labels[5] = {L"복구 경로", L"난이도", L"완료 전투", L"섹터 / 설치", L"최종 용량"};
    for (int i = 0; i < 5; ++i) {
        int left = stats.left + i * (stats.right - stats.left) / 5;
        int right = stats.left + (i + 1) * (stats.right - stats.left) / 5;
        if (i) Fill(dc, MakeRect(left, stats.top + 12, left + 1, stats.bottom - 12), C_LINE);
        TextRect(dc, MakeRect(left + 4, stats.top + 10, right - 4, stats.top + 30), labels[i], C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(left + 4, stats.top + 38, right - 4, stats.bottom - 8), stat[i], i == 0 ? accent : C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    }

    RECT restart = EndingRestartRect(); int hover = Inside(restart, gMouse.x, gMouse.y);
    int pulse = (int)((GetTickCount() / 90u) % 18u); if (pulse > 9) pulse = 18 - pulse;
    Panel(dc, restart, hover ? MixColor(C_PANEL_2, accent, 24) : C_PANEL_2, hover ? accent : MixColor(C_LINE, accent, 25 + pulse * 3));
    TextRect(dc, restart, L"새 런 시작  [R / ENTER]", hover ? accent : C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawDeck(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"보유 중인 디스크 면", C_GREEN, gFontLarge);
    wchar_t b[160]; wsprintfW(b, L"덱 %dB + 상주 %dB = %dB / %dB", DeckBytes(&gGame), TsrBytes(&gGame), UsedBytes(&gGame), EffectiveCapacity(&gGame));
    Text(dc, panel.left + 28, panel.top + 58, b, UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontSmall);
    if (InstalledTsrCount(&gGame) > 0) {
        lstrcpyW(b, L"상주:");
        for (int i = 0; i < TSR_COUNT; ++i) if (gGame.tsrInstalled[i]) { lstrcatW(b, L"  "); lstrcatW(b, TSR_INFO[i].name); }
        Text(dc, panel.left + 440, panel.top + 58, b, C_DIM, gFontSmall);
    }
    RECT close = DeckCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawFaceGrid(dc, 3);

    TextRect(dc, MakeRect(84, panel.bottom - 50, panel.right - 30, panel.bottom - 20),
        L"현재 보유한 18개 면입니다 (조회 전용). 특수 면에 커서를 올리면 능력과 출력이 보입니다.", C_DIM, gFontSmall, DT_SINGLELINE);

    // 툴팁은 격자 위에 겹쳐 떠야 하므로 마지막에 그린다.
    DrawDeckFaceTip(dc, panel);
}

// 몹 패턴을 카드 한 줄 역할로 요약한다.
static const wchar_t* PatternRoleLabel(int pattern) {
    switch (pattern) {
    case PATTERN_ASSAULT: return L"공격형 · 공격-공격-강공 주기";
    case PATTERN_RAMP: return L"공격형 · 턴마다 공격이 강해짐";
    case PATTERN_BULWARK: return L"방어형 · 방어를 굳히고 강공";
    case PATTERN_SIEGE: return L"방어형 · 이중 방어 후 강공";
    case PATTERN_MEDIC: return L"방어형 · 복구와 방어 반복";
    case PATTERN_CORRUPTER: return L"변칙형 · 오염(관통) 선공";
    case PATTERN_OPENER: return L"변칙형 · 첫 턴 강공 압박";
    case PATTERN_ERRATIC: return L"변칙형 · 의도가 매턴 뒤섞임";
    case PATTERN_SPIKE: return L"변칙형 · 강공과 오염 조합";
    default: return L"변칙형";
    }
}

// ---- 가이드 ------------------------------------------------------------------
// 예전 가이드는 두 쪽에 일곱 섹션을 몰아 16px 글자를 16px 간격으로 쌓은 글 뭉치였고,
// 용어와 설명이 같은 색이라 훑어 읽을 수 없었다. 지금은 주제별 다섯 쪽으로 나누고
// 모든 쪽을 같은 어휘(섹션 머리 · 용어 줄 · 키 칩 · 문단)로 그린다.
// 본문은 탭 아래 y200부터 페이지 이동 버튼 위 y672까지 쓴다.
#define GUIDE_LEFT  84
#define GUIDE_RIGHT (BASE_WIDTH - 84)
#define GUIDE_TOP   200
#define GUIDE_COL_A (BASE_WIDTH / 2 - 22)   // 왼쪽 단의 오른쪽 끝
#define GUIDE_COL_B (BASE_WIDTH / 2 + 22)   // 오른쪽 단의 왼쪽 끝
#define GUIDE_LINE  24                       // 16px 글자에 8px 행간

static const wchar_t* const GUIDE_TAB_LABELS[GUIDE_PAGE_COUNT] = {
    L"1  시작하기", L"2  슬롯과 면", L"3  적과 위험", L"4  덱·보상·조작", L"5  드라이브 정보"
};

// 단어 단위로 직접 접는 문단. DrawText의 줄바꿈은 글꼴 높이 그대로 줄을 붙이므로
// 여기서 줄마다 lineHeight를 준다. 번역문이 들어오므로 폭은 그릴 때 잰다.
// 다 쓴 다음 줄의 y를 돌려준다.
static int GuideParagraph(HDC dc, int x, int y, int right, const wchar_t* text, COLORREF color, HFONT font, int lineHeight) {
    const wchar_t* p = LocalizeText(text);
    HFONT old = (HFONT)SelectObject(dc, font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    while (*p) {
        const wchar_t* end = p;
        const wchar_t* scan = p;
        while (*scan && *scan != L'\n') {
            const wchar_t* word = scan;
            while (*word == L' ') ++word;
            while (*word && *word != L' ' && *word != L'\n') ++word;
            SIZE size; GetTextExtentPoint32W(dc, p, (int)(word - p), &size);
            if (size.cx > right - x && end > p) break;
            end = scan = word;
        }
        TextOutW(dc, x, y, p, (int)(end - p));
        y += lineHeight;
        p = end;
        while (*p == L' ') ++p;
        if (*p == L'\n') ++p;
    }
    SelectObject(dc, old);
    return y;
}

static int GuideSection(HDC dc, int x, int y, int right, const wchar_t* title) {
    Fill(dc, MakeRect(x, y + 3, x + 4, y + 23), C_YELLOW);
    Text(dc, x + 14, y, title, C_YELLOW, gFontMedium);
    Fill(dc, MakeRect(x, y + 32, right, y + 33), C_LINE);
    return y + 44;
}

// 용어 칸과 설명 칸으로 나눈 한 항목.
static int GuideRow(HDC dc, int x, int y, int termWidth, int right, const wchar_t* term, COLORREF termColor,
                    const wchar_t* desc, COLORREF descColor = C_TEXT) {
    Text(dc, x, y, term, termColor, gFontSmall);
    return GuideParagraph(dc, x + termWidth, y, right, desc, descColor, gFontSmall, GUIDE_LINE) + 4;
}

static int GuideBullet(HDC dc, int x, int y, int right, const wchar_t* text) {
    Fill(dc, MakeRect(x + 2, y + 7, x + 8, y + 13), C_GREEN);
    return GuideParagraph(dc, x + 20, y, right, text, C_TEXT, gFontSmall, GUIDE_LINE) + 4;
}

// 자판 한 칸. 오른쪽 끝 다음 x를 돌려주므로 여러 개를 이어 그릴 수 있다.
static int GuideKey(HDC dc, int x, int y, const wchar_t* key) {
    RECT r = MakeRect(x, y - 2, x + TextWidth(dc, key, gFontSmall) + 16, y + 21);
    Panel(dc, r, C_PANEL_2, C_LINE);
    Fill(dc, MakeRect(r.left + 1, r.bottom - 3, r.right - 1, r.bottom - 1), C_LINE);
    TextRect(dc, MakeRect(r.left, r.top, r.right, r.bottom - 2), key, C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return r.right + 6;
}

// 번호 배지. 순서가 곧 의미인 목록(턴 진행 · 슬롯 해결 순서)에 쓴다.
static void GuideBadge(HDC dc, int x, int y, int number, COLORREF color) {
    RECT r = MakeRect(x, y, x + 26, y + 26);
    Fill(dc, r, color);
    wchar_t b[8]; wsprintfW(b, L"%d", number);
    TextRect(dc, r, b, C_INK, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawGuideBasicsPage(HDC dc) {
    int y = GuideSection(dc, GUIDE_LEFT, GUIDE_TOP, GUIDE_RIGHT, L"목표");
    y = GuideParagraph(dc, GUIDE_LEFT, y, GUIDE_RIGHT,
        L"볼륨(드라이브) 하나를 골라 3개 층을 내려가며 최종 보스를 삭제합니다. 체력이 0이 되면 이번 런은 끝납니다.",
        C_TEXT, gFontSmall, GUIDE_LINE) + 8;

    // 한 층의 흐름. 보스 앞에는 디렉터리 선택이 없다.
    static const wchar_t* const flow[8] = {L"디렉터리", L"일반전", L"보상", L"디렉터리", L"일반전", L"보상", L"보스전", L"전리품"};
    static const COLORREF flowColor[8] = {C_BLUE, C_TEXT, C_GREEN, C_BLUE, C_TEXT, C_GREEN, C_RED, C_YELLOW};
    int x = GUIDE_LEFT;
    Text(dc, x, y + 4, L"한 층의 흐름", C_DIM, gFontSmall);
    x += TextWidth(dc, L"한 층의 흐름", gFontSmall) + 16;
    for (int i = 0; i < 8; ++i) {
        if (i) { Text(dc, x, y + 4, L"→", C_DIM, gFontSmall); x += TextWidth(dc, L"→", gFontSmall) + 8; }
        RECT chip = MakeRect(x, y, x + TextWidth(dc, flow[i], gFontSmall) + 24, y + 26);
        Panel(dc, chip, MixColor(C_PANEL, flowColor[i], 14), MixColor(C_LINE, flowColor[i], 50));
        TextRect(dc, chip, flow[i], flowColor[i], gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x = chip.right + 8;
    }
    Text(dc, x + 8, y + 4, L"× 3층", C_DIM, gFontSmall);

    y = GuideSection(dc, GUIDE_LEFT, y + 44, GUIDE_RIGHT, L"한 턴 진행");
    struct GuideStep { const wchar_t* title; const wchar_t* keys[4]; const wchar_t* body; };
    static const GuideStep steps[5] = {
        {L"판독", {L"R"}, L"섹터를 읽어 주사위 3개를 굴립니다. 매 턴 가장 먼저 누릅니다."},
        {L"주사위 고르기", {L"클릭", L"1", L"2", L"3"}, L"놓을 주사위를 고릅니다. 노란 테두리가 선택 표시입니다."},
        {L"슬롯에 놓기", {L"클릭"}, L"주사위마다 다른 슬롯에 놓습니다. 4칸 중 1칸은 비게 됩니다."},
        {L"대상 고르기", {L"클릭"}, L"공격할 적 카드를 누릅니다. 카드에 ▶ 공격 대상이 붙습니다."},
        {L"실행", {L"Space"}, L"슬롯 위의 예상 결과를 확인하고 턴을 실행합니다."}
    };
    int cardW = (GUIDE_RIGHT - GUIDE_LEFT - 4 * 12) / 5;
    for (int i = 0; i < 5; ++i) {
        int left = GUIDE_LEFT + i * (cardW + 12);
        RECT card = MakeRect(left, y, left + cardW, y + 150);
        Panel(dc, card, C_PANEL_2, C_LINE);
        GuideBadge(dc, card.left + 12, card.top + 12, i + 1, C_GREEN);
        Text(dc, card.left + 48, card.top + 14, steps[i].title, C_TEXT, gFontMedium);
        int kx = card.left + 12;
        for (int k = 0; k < 4 && steps[i].keys[k]; ++k) kx = GuideKey(dc, kx, card.top + 50, steps[i].keys[k]);
        GuideParagraph(dc, card.left + 12, card.top + 82, card.right - 12, steps[i].body, C_TEXT, gFontSmall, 22);
    }

    y = GuideSection(dc, GUIDE_LEFT, y + 168, GUIDE_RIGHT, L"알아 두면 좋은 것");
    y = GuideBullet(dc, GUIDE_LEFT, y, GUIDE_RIGHT, L"주사위를 놓으면 슬롯 위에 예상 결과가 먼저 뜹니다. 실행하기 전까지는 얼마든지 옮겨 볼 수 있습니다.");
    y = GuideBullet(dc, GUIDE_LEFT, y, GUIDE_RIGHT, L"보스 기믹은 발동하기 전에 적 카드 · 슬롯 · 주사위에 먼저 예고됩니다.");
    GuideBullet(dc, GUIDE_LEFT, y, GUIDE_RIGHT, L"전투에서 이겨도 체력은 저절로 회복되지 않습니다. 보상 화면의 섹터 복구를 챙기세요.");
}

static void DrawGuideSlotsPage(HDC dc) {
    int y = GuideSection(dc, GUIDE_LEFT, GUIDE_TOP, GUIDE_COL_A, L"슬롯 — 이 순서로 실행됩니다");
    static const int order[SLOT_COUNT] = {SLOT_AMPLIFY, SLOT_ATTACK, SLOT_DEFEND, SLOT_CHAIN};
    static const wchar_t* const body[SLOT_COUNT] = {
        L"뒤에 오는 공격 · 방어에 출력의 절반을 더합니다. 증폭 면이면 전부 더합니다.",
        L"선택한 적에게 출력만큼 피해를 줍니다.",
        L"이번 턴 적 공격을 막는 방어도를 얻습니다. 다음 턴이 시작되면 사라집니다.",
        L"직전 공격을 일부 반복합니다. 공격이 없었다면 방어를 반복합니다."
    };
    for (int i = 0; i < SLOT_COUNT; ++i) {
        int slot = order[i], rowTop = y;
        GuideBadge(dc, GUIDE_LEFT, rowTop - 2, i + 1, SlotAccent(slot));
        Text(dc, GUIDE_LEFT + 38, rowTop - 1, SLOT_NAMES[slot], SlotAccent(slot), gFontMedium);
        int next = GuideParagraph(dc, GUIDE_LEFT + 138, rowTop, GUIDE_COL_A, body[i], C_TEXT, gFontSmall, GUIDE_LINE);
        y = (next > rowTop + 28 ? next : rowTop + 28) + 8;
    }
    y = GuideSection(dc, GUIDE_LEFT, y + 8, GUIDE_COL_A, L"배치 규칙");
    y = GuideBullet(dc, GUIDE_LEFT, y, GUIDE_COL_A, L"주사위 3개, 슬롯 4칸 — 한 칸은 항상 비어 있습니다.");
    y = GuideBullet(dc, GUIDE_LEFT, y, GUIDE_COL_A, L"이미 찬 슬롯에 놓으면 먼저 있던 주사위가 빠집니다.");
    y = GuideBullet(dc, GUIDE_LEFT, y, GUIDE_COL_A, L"슬롯 위의 → N 은 예상 산출량입니다. → 0 이면 효과가 없습니다.");
    GuideBullet(dc, GUIDE_LEFT, y, GUIDE_COL_A, L"일부 보스는 예고한 턴에 이 순서를 거꾸로 뒤집습니다.");

    int x = GUIDE_COL_B;
    y = GuideSection(dc, x, GUIDE_TOP, GUIDE_RIGHT, L"면 8종 — 주사위 한 면에 하나씩");
    Text(dc, x, y, L"면", C_DIM, gFontSmall);
    Text(dc, x + 110, y, L"비용", C_DIM, gFontSmall);
    Text(dc, x + 190, y, L"효과", C_DIM, gFontSmall);
    y += 28;
    static const wchar_t* const faceBody[FACE_KIND_COUNT] = {
        L"값만큼 출력하고 값만큼 차지합니다",
        L"공격 +4, 대상에게 화상 2회",
        L"방어 슬롯에서 출력 2배",
        L"공격 피해 일부를 체력으로 회복",
        L"어느 슬롯에서도 높은 출력",
        L"증폭 슬롯에서 보너스 2배",
        L"연쇄 슬롯에서 직전 효과 100% 반복",
        L"효과 없음"
    };
    for (int i = 0; i < FACE_KIND_COUNT; ++i) {
        if (i & 1) Fill(dc, MakeRect(x - 8, y - 3, GUIDE_RIGHT + 8, y + 23), MixColor(C_PANEL, C_PANEL_2, 60));
        Text(dc, x, y, FACE_INFO[i].name, i == FACE_EMPTY ? C_DIM : (COLORREF)FACE_INFO[i].color, gFontSmall);
        wchar_t cost[16];
        if (i == FACE_NUMBER) lstrcpyW(cost, L"= 값"); else wsprintfW(cost, L"%dB", FACE_INFO[i].cost);
        Text(dc, x + 110, y, cost, C_TEXT, gFontSmall);
        Text(dc, x + 190, y, faceBody[i], C_TEXT, gFontSmall);
        y += 26;
    }
    y += 12;
    y = GuideBullet(dc, x, y, GUIDE_RIGHT, L"강화 보상으로 받은 특수 면은 비용은 같고 출력만 +2 높습니다.");
    y = GuideBullet(dc, x, y, GUIDE_RIGHT, L"손상된 면은 효과를 잃지만 비용은 그대로 차지합니다.");
    GuideBullet(dc, x, y, GUIDE_RIGHT, L"보유한 면은 F3 에서 언제든 볼 수 있습니다.");
}

static void DrawGuideThreatsPage(HDC dc) {
    int y = GuideSection(dc, GUIDE_LEFT, GUIDE_TOP, GUIDE_COL_A, L"적 카드 읽기");
    y = GuideRow(dc, GUIDE_LEFT, y, 130, GUIDE_COL_A, L"의도", C_GREEN, L"적이 이번 턴에 할 행동입니다. 수치까지 카드에 미리 적혀 있습니다.");
    y = GuideRow(dc, GUIDE_LEFT, y, 130, GUIDE_COL_A, L"오염(관통)", C_RED, L"방어도가 절반만 막습니다. 아래 난이도 배율이 붙습니다.");
    y = GuideRow(dc, GUIDE_LEFT, y, 130, GUIDE_COL_A, L"화상", RGB(255, 139, 92), L"적이 행동하기 직전에 3 피해를 받습니다.");
    y = GuideRow(dc, GUIDE_LEFT, y, 130, GUIDE_COL_A, L"몹 특성", C_YELLOW, L"전투 내내 적용되는 성질로 카드에 늘 적혀 있습니다. 굴린 눈의 홀짝 · 크기를 보는 것이 많습니다.");
    y = GuideRow(dc, GUIDE_LEFT, y, 130, GUIDE_COL_A, L"보스 기믹", C_BLUE, L"발동하기 전에 먼저 예고됩니다. 오프라인 · 격리된 대상은 그 턴 출력이 0입니다.");

    y = GuideSection(dc, GUIDE_LEFT, y + 8, GUIDE_COL_A, L"난이도 — 오염(관통) 피해 배율");
    const DifficultyInfo* current = gGame.selectedDrive >= 0 ? DifficultyInfoOrNull(gGame.difficulty) : 0;
    int cellW = (GUIDE_COL_A - GUIDE_LEFT - (DIFFICULTY_COUNT - 1) * 8) / DIFFICULTY_COUNT;
    for (int i = 0; i < DIFFICULTY_COUNT; ++i) {
        const DifficultyInfo* d = &DIFFICULTY_INFO[i];
        COLORREF color = (COLORREF)d->color;
        int left = GUIDE_LEFT + i * (cellW + 8), on = current == d;
        RECT cell = MakeRect(left, y, left + cellW, y + 56);
        Panel(dc, cell, on ? MixColor(C_PANEL_2, color, 20) : C_PANEL_2, on ? color : C_LINE);
        TextRect(dc, MakeRect(cell.left, cell.top + 6, cell.right, cell.top + 26), d->name, color, gFontSmall, DT_CENTER | DT_SINGLELINE);
        wchar_t pct[16]; wsprintfW(pct, L"%d%%", d->corruptPercent);
        TextRect(dc, MakeRect(cell.left, cell.top + 27, cell.right, cell.top + 47), pct, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        // 가장 높은 200%를 칸 폭으로 잰 막대.
        Fill(dc, MakeRect(cell.left + 8, cell.bottom - 7, cell.left + 8 + (cellW - 16) * d->corruptPercent / 200, cell.bottom - 4), color);
    }
    GuideParagraph(dc, GUIDE_LEFT, y + 64, GUIDE_COL_A,
        current ? L"테두리가 이번 런의 난이도입니다. 예고 수치에 이미 반영되어 있습니다." : L"적 카드의 예고 수치에 이미 반영되어 있습니다.",
        C_DIM, gFontSmall, GUIDE_LINE);

    int x = GUIDE_COL_B;
    y = GuideSection(dc, x, GUIDE_TOP, GUIDE_RIGHT, L"디스크 손상 — 볼륨마다 2종");
    const DriveInfo* drive = gGame.selectedDrive >= 0 && gGame.selectedDrive < DRIVE_COUNT ? &DRIVE_INFO[gGame.selectedDrive] : 0;
    if (drive) {
        wchar_t b[96]; wsprintfW(b, L"이번 런에 적용: %s + %s", MODIFIER_INFO[drive->modifierA].name, MODIFIER_INFO[drive->modifierB].name);
        Text(dc, x, y, b, C_YELLOW, gFontSmall);
        y += 30;
    }
    for (int i = 0; i < MODIFIER_COUNT; ++i) {
        int lit = !drive || drive->modifierA == i || drive->modifierB == i;
        y = GuideRow(dc, x, y, 130, GUIDE_RIGHT, MODIFIER_INFO[i].name, lit ? C_YELLOW : C_DIM,
            MODIFIER_INFO[i].description, lit ? C_TEXT : C_DIM);
    }
    y = GuideSection(dc, x, y + 8, GUIDE_RIGHT, L"체력과 회복");
    y = GuideBullet(dc, x, y, GUIDE_RIGHT, L"체력이 0이 되면 런이 끝납니다. 전투 승리만으로는 회복되지 않습니다.");
    GuideBullet(dc, x, y, GUIDE_RIGHT, L"회복 수단: 섹터 복구 보상 · 흡수 면 · E:\\ 볼륨 특성 · UNDELETE");
}

static void DrawGuideDeckPage(HDC dc) {
    int y = GuideSection(dc, GUIDE_LEFT, GUIDE_TOP, GUIDE_COL_A, L"용량 — 넘치면 진행할 수 없습니다");
    y = GuideParagraph(dc, GUIDE_LEFT, y, GUIDE_COL_A,
        L"면과 상주 프로그램(TSR) 비용의 합이 층 한도를 넘으면, 정리 화면에서 면을 지우거나 TSR을 종료해야 다음으로 넘어갑니다.",
        C_TEXT, gFontSmall, GUIDE_LINE) + 8;
    int mounted = gGame.selectedDrive >= 0 && gGame.phase != PHASE_TITLE;
    int cellW = (GUIDE_COL_A - GUIDE_LEFT - 2 * 8) / 3;
    wchar_t b[96];
    for (int f = 0; f < 3; ++f) {
        int left = GUIDE_LEFT + f * (cellW + 8), on = mounted && gGame.floor == f;
        RECT cell = MakeRect(left, y, left + cellW, y + 52);
        Panel(dc, cell, on ? MixColor(C_PANEL_2, C_GREEN, 16) : C_PANEL_2, on ? C_GREEN : C_LINE);
        wsprintfW(b, L"%d층 한도", f + 1);
        Text(dc, cell.left + 12, cell.top + 8, b, on ? C_GREEN : C_DIM, gFontSmall);
        wsprintfW(b, L"%dB", FLOOR_CAPACITY[f]);
        TextRect(dc, MakeRect(cell.left, cell.top + 5, cell.right - 12, cell.top + 30), b, C_TEXT, gFontMedium, DT_RIGHT | DT_SINGLELINE);
        Fill(dc, MakeRect(cell.left + 12, cell.bottom - 12, cell.left + 12 + (cellW - 24) * FLOOR_CAPACITY[f] / FLOOR_CAPACITY[0], cell.bottom - 8),
            on ? C_GREEN : C_LINE);
    }
    y += 60;
    y = GuideParagraph(dc, GUIDE_LEFT, y, GUIDE_COL_A, L"층을 내려갈수록 한도가 줄어듭니다.", C_DIM, gFontSmall, GUIDE_LINE);
    if (mounted) {
        int over = UsedBytes(&gGame) > EffectiveCapacity(&gGame);
        wsprintfW(b, L"지금 %dB / %dB 사용 중", UsedBytes(&gGame), EffectiveCapacity(&gGame));
        y = GuideParagraph(dc, GUIDE_LEFT, y, GUIDE_COL_A, b, over ? C_RED : C_GREEN, gFontSmall, GUIDE_LINE);
    }

    y = GuideSection(dc, GUIDE_LEFT, y + 12, GUIDE_COL_A, L"보상 종류");
    y = GuideRow(dc, GUIDE_LEFT, y, 120, GUIDE_COL_A, L"일반전 승리", C_GREEN, L"면 후보 3개 중 하나로 기존 면을 교체하거나, 섹터 복구로 체력을 회복합니다.");
    y = GuideRow(dc, GUIDE_LEFT, y, 120, GUIDE_COL_A, L"보스 처치", C_RED, L"상주 프로그램(TSR) 3개 중 하나를 설치합니다. 면을 바꾸지 않고 용량만 차지합니다.");
    GuideRow(dc, GUIDE_LEFT, y, 120, GUIDE_COL_A, L"건너뛰기", C_DIM, L"고른 카드가 없을 때 Esc 를 두 번 누르면 보상을 포기합니다.");

    int x = GUIDE_COL_B;
    y = GuideSection(dc, x, GUIDE_TOP, GUIDE_RIGHT, L"조작");
    struct GuideBinding { const wchar_t* keys[4]; const wchar_t* what; };
    static const GuideBinding bindings[] = {
        {{L"R"}, L"섹터 판독 — 매 턴 첫 입력"},
        {{L"클릭", L"1", L"2", L"3"}, L"주사위 선택 · 보상 카드 선택"},
        {{L"4"}, L"보상 화면에서 섹터 복구"},
        {{L"K"}, L"KEYB 재굴림 (상주 프로그램 설치 시)"},
        {{L"Space"}, L"턴 실행"},
        {{L"Esc"}, L"배치 해제 · 선택 취소 · 창 닫기"},
        {{L"Enter"}, L"정리 확정 · 포커스된 항목 선택"},
        {{L"Tab"}, L"전투 · 정리 화면에서 항목 이동 (Shift+Tab 은 반대로)"},
        {{L"F1", L"F2", L"F3"}, L"가이드 · 설정 · 보유 면"},
        {{L"←", L"→", L"1~5"}, L"가이드 안에서 페이지 이동"}
    };
    for (int i = 0; i < (int)(sizeof(bindings) / sizeof(bindings[0])); ++i) {
        int kx = x;
        for (int k = 0; k < 4 && bindings[i].keys[k]; ++k) kx = GuideKey(dc, kx, y + 2, bindings[i].keys[k]);
        int next = GuideParagraph(dc, x + 170, y + 2, GUIDE_RIGHT, bindings[i].what, C_TEXT, gFontSmall, GUIDE_LINE);
        y = (next > y + 32 ? next : y + 32) + 2;
    }
}

int GuideNoiseActive() {
    if (!gGuideOpen || gGuidePage != GUIDE_PAGE_DRIVE) return 0;
    if (gGame.selectedDrive < 0 || gGame.selectedDrive >= DRIVE_COUNT) return 0;
    for (int i = 0; i < DRIVE_MOB_COUNT; ++i) if (!IsEnemyScanned(&gGame, DRIVE_MOBS[gGame.selectedDrive][i])) return 1;
    for (int i = 0; i < DRIVE_BOSS_COUNT; ++i) if (!IsEnemyScanned(&gGame, DRIVE_BOSSES[gGame.selectedDrive][i])) return 1;
    return 0;
}

static void DrawGuideDrivePage(HDC dc, const RECT& panel) {
    int left = GUIDE_LEFT, middle = GUIDE_COL_B, top = GUIDE_TOP - 4;
    if (gGame.selectedDrive < 0 || gGame.selectedDrive >= DRIVE_COUNT) {
        int y = GuideSection(dc, GUIDE_LEFT, GUIDE_TOP, GUIDE_RIGHT, L"드라이브별 적 · 보스");
        y = GuideParagraph(dc, GUIDE_LEFT, y, GUIDE_RIGHT,
            L"볼륨을 마운트하면 이 쪽에 그 드라이브의 일반 몹 3종과 층별 보스 3종이 나옵니다. 처치한 개체부터 정보가 열립니다.",
            C_TEXT, gFontSmall, GUIDE_LINE) + 12;
        // A:\는 여섯 조각을 모두 모아야 열리는 최종 볼륨이라 여기 싣지 않는다.
        static const wchar_t* const family[6] = {
            L"슬롯 권한 잠금과 칸 파쇄", L"피해 목표 미달 시 복원 · 되감기", L"예고된 주사위 연결 끊김",
            L"슬롯 해결 순서 역전", L"메모리 압력 게이지와 강화 공격", L"면 격리, 최종 보스는 영구 삭제"
        };
        for (int i = 0; i < 6; ++i) {
            wchar_t name[48]; wsprintfW(name, L"%s %s", DRIVE_INFO[i].letter, DRIVE_INFO[i].label);
            y = GuideRow(dc, GUIDE_LEFT, y, 190, GUIDE_RIGHT, name, (COLORREF)DRIVE_INFO[i].color, family[i]) + 4;
        }
        return;
    }
    const DriveInfo* drive = &DRIVE_INFO[gGame.selectedDrive];
    wchar_t b[160];
    wsprintfW(b, L"%s%s 전용 로스터", drive->letter, drive->label);
    Text(dc, left, top, b, (COLORREF)drive->color, gFontMedium);

    // 처치한 개체만 정보가 열린다. 아직 만나지 않은 칸은 살아 있는 헥스 노이즈로 덮인다.
    const int* mobs = DRIVE_MOBS[gGame.selectedDrive];
    const int* bosses = DRIVE_BOSSES[gGame.selectedDrive];
    uint32_t tick = GetTickCount();
    int scanned = 0;
    for (int i = 0; i < DRIVE_MOB_COUNT; ++i) if (IsEnemyScanned(&gGame, mobs[i])) ++scanned;
    for (int i = 0; i < DRIVE_BOSS_COUNT; ++i) if (IsEnemyScanned(&gGame, bosses[i])) ++scanned;
    wsprintfW(b, L"판독 %d / %d  ·  처치한 개체만 열립니다", scanned, DRIVE_MOB_COUNT + DRIVE_BOSS_COUNT);
    TextRect(dc, MakeRect(middle, top, panel.right - 28, top + 24), b,
        scanned == DRIVE_MOB_COUNT + DRIVE_BOSS_COUNT ? C_GREEN : C_DIM, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    Text(dc, left, top + 40, L"일반 몹 (모든 층, 층마다 강해짐)", C_YELLOW, gFontSmall);
    for (int i = 0; i < DRIVE_MOB_COUNT; ++i) {
        const EnemyInfo* info = GetEnemyInfoOrUnknown(mobs[i]);
        int y = top + 68 + i * 66;
        if (IsEnemyScanned(&gGame, mobs[i])) {
            Text(dc, left, y, info->code, (COLORREF)info->color, gFontMedium);
            wsprintfW(b, L"%s\n체력 %d+ · 피해 %d+", PatternRoleLabel(info->pattern), info->hp, info->damage);
            TextRect(dc, MakeRect(left, y + 24, middle - 28, y + 66), b, C_DIM, gFontSmall, DT_WORDBREAK);
        } else {
            wchar_t garbled[32]; CorruptCode(info->code, garbled, 32, mobs[i], tick);
            DrawGlitchLine(dc, left, y, garbled, C_LINE, MixColor(C_LINE, C_GREEN, 82), gFontMedium, mobs[i], tick);
            TextRect(dc, MakeRect(left, y, middle - 28, y + 22), L"미판독", C_DIM, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            DrawHexBlock(dc, MakeRect(left, y + 26, middle - 28, y + 64), C_LINE, mobs[i], tick, 2);
        }
    }
    // 볼륨 법칙은 판독과 무관하게 늘 보인다. 매 전투에 걸리는 규칙이기 때문이다.
    int lawY = GuideSection(dc, left, top + 272, middle - 28, L"볼륨 법칙 — 매 전투에 적용");
    Text(dc, left, lawY, DRIVE_LAW_INFO[gGame.selectedDrive].name, C_GREEN, gFontMedium);
    GuideParagraph(dc, left, lawY + 30, middle - 28, DRIVE_LAW_INFO[gGame.selectedDrive].description, C_TEXT, gFontSmall, GUIDE_LINE);

    Text(dc, middle, top + 40, L"층별 보스와 기믹", C_YELLOW, gFontSmall);
    for (int i = 0; i < DRIVE_BOSS_COUNT; ++i) {
        const EnemyInfo* info = GetEnemyInfoOrUnknown(bosses[i]);
        const BossGimmickInfo* gi = &BOSS_GIMMICK_INFO[info->gimmick];
        int y = top + 68 + i * 118;
        if (IsEnemyScanned(&gGame, bosses[i])) {
            wsprintfW(b, L"%d층  %s — %s", i + 1, info->code, gi->name);
            Text(dc, middle, y, b, (COLORREF)info->color, gFontMedium);
            wsprintfW(b, L"%s\n대응: %s", gi->rule, gi->counter);
            TextRect(dc, MakeRect(middle, y + 26, panel.right - 28, y + 112), b, C_TEXT, gFontSmall, DT_WORDBREAK);
        } else {
            wchar_t garbledCode[32], garbledName[24];
            CorruptCode(info->code, garbledCode, 32, bosses[i], tick);
            CorruptCode(L"????????", garbledName, 24, bosses[i] + 101, tick);
            wchar_t prefix[16]; wsprintfW(prefix, L"%d층  ", i + 1);
            Text(dc, middle, y, prefix, C_LINE, gFontMedium);
            wsprintfW(b, L"%s - %s", garbledCode, garbledName);
            DrawGlitchLine(dc, middle + TextWidth(dc, prefix, gFontMedium), y, b, C_LINE,
                MixColor(C_LINE, C_GREEN, 82), gFontMedium, bosses[i], tick);
            TextRect(dc, MakeRect(middle, y, panel.right - 28, y + 22), L"미판독", C_DIM, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            DrawHexBlock(dc, MakeRect(middle, y + 28, panel.right - 28, y + 110), C_LINE, bosses[i], tick, 4);
        }
    }
}

static void DrawGuide(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"시스템 가이드", C_GREEN, gFontLarge);
    RECT close = GuideCloseRect(width); int hoverClose = Inside(close, gMouse.x, gMouse.y);
    Panel(dc, close, hoverClose ? RGB(28, 39, 48) : C_PANEL_2, hoverClose ? C_BLUE : C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 쪽 번호 대신 탭. 무엇이 어느 쪽에 있는지 열자마자 보이고 바로 건너갈 수 있다.
    for (int i = 0; i < GUIDE_PAGE_COUNT; ++i) {
        RECT tab = GuideTabRect(i);
        int on = i == gGuidePage, hover = !on && Inside(tab, gMouse.x, gMouse.y);
        Panel(dc, tab, on ? MixColor(C_PANEL_2, C_GREEN, 18) : hover ? RGB(28, 39, 48) : C_PANEL_2, on ? C_GREEN : hover ? C_BLUE : C_LINE);
        // 영어 탭 이름에 &가 들어간다. DrawText가 이를 밑줄 접두사로 먹지 않게 한다.
        TextRect(dc, tab, GUIDE_TAB_LABELS[i], on ? C_GREEN : hover ? C_TEXT : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    switch (gGuidePage) {
    case 0: DrawGuideBasicsPage(dc); break;
    case 1: DrawGuideSlotsPage(dc); break;
    case 2: DrawGuideThreatsPage(dc); break;
    case 3: DrawGuideDeckPage(dc); break;
    default: DrawGuideDrivePage(dc, panel); break;
    }

    int canPrev = gGuidePage > 0, canNext = gGuidePage < GUIDE_PAGE_COUNT - 1;
    RECT prev = GuidePrevRect(width, height); int hoverPrev = canPrev && Inside(prev, gMouse.x, gMouse.y);
    Panel(dc, prev, hoverPrev ? RGB(28, 39, 48) : C_PANEL_2, hoverPrev ? C_BLUE : C_LINE);
    TextRect(dc, prev, L"◀ 이전 페이지", canPrev ? C_TEXT : C_LINE, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT next = GuideNextRect(width, height); int hoverNext = canNext && Inside(next, gMouse.x, gMouse.y);
    Panel(dc, next, hoverNext ? RGB(28, 39, 48) : C_PANEL_2, hoverNext ? C_BLUE : C_LINE);
    TextRect(dc, next, L"다음 페이지 ▶", canNext ? C_TEXT : C_LINE, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(prev.right + 16, prev.top, next.left - 16, prev.bottom), L"←·→ 또는 1~5 키로 페이지 이동  ·  F1 / Esc 로 닫기",
        C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---------------------------------------------------------------------------
// 기믹 발동 연출 18종. 규칙은 이미 game.cpp에서 확정된 뒤이고, 여기서는 그때
// 남겨 둔 기록(firedFx / fxA / fxB)을 읽어 보여 주기만 한다. 판정에 관여하지
// 않으므로 스모크·밸런스의 결정론은 그대로다.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 보스 기믹 발동 연출.
//
// 범위는 Local → Regional → Global 순으로만 커진다. 슬롯 하나가 잠기는 사건은
// 그 슬롯에서, 주사위 하나가 끊기는 사건은 그 주사위에서 끝난다. 전체 화면을
// 쓰는 것은 되돌릴 수 없는 삭제(ZERO.DAY)와 한 판을 통째로 바꾸는 발동뿐이다.
// 지속 시간도 사건의 무게에 맞춘다 — 매턴 반복되는 기믹이 실제 사건보다 오래
// 화면을 장악하면 강한 발동의 가치가 사라진다.
// ---------------------------------------------------------------------------

// 총 길이. Local 180~320 / Regional 300~450 / 강한 발동 최대 700 /
// 되돌릴 수 없는 삭제만 700~1200.
int GimmickFxDuration(int kind, int b) {
    switch (kind) {
    case GIMMICK_SIGNATURE: return 900;
    case GIMMICK_SEVENTEENTH: return 900;
    case GIMMICK_LAST_WRITE: return 2000;
    case GIMMICK_ACCESS_DENIED:  return 2000;
    case GIMMICK_KERNEL_PANIC:   return 2250;
    case GIMMICK_BLUE_SCREEN:    return b == SHRED_FX_RESTORE ? 1900 : 2800;   // 파쇄 / 복구
    case GIMMICK_RESTORE_POINT:  return 2000;
    case GIMMICK_TAPE_LOOP:      return 900;    // 매턴 나올 수 있어 가장 짧다
    case GIMMICK_MASTER_BACKUP:  return 3200;   // 런에 한 번뿐
    case GIMMICK_AUTOPLAY:       return 1600;
    case GIMMICK_UNSAFE_EJECT:   return 1900;
    case GIMMICK_NO_MEDIA:       return b == 1 ? 1500 : 1200;
    case GIMMICK_PROXY:          return 2000;
    case GIMMICK_ROUTING_LOOP:   return 2250;
    case GIMMICK_TIMEOUT:        return b ? 2600 : 900;
    case GIMMICK_LEAK:           return 1700;
    case GIMMICK_HEAP_OVERFLOW:  return 2000;
    case GIMMICK_OUT_OF_MEMORY:  return 2700;
    case GIMMICK_SAMPLE13:       return 1900;
    case GIMMICK_SANDBOX_BREACH: return 2400;   // 새 적이 판에 들어오는 순간
    case GIMMICK_ZERO_DAY:       return 3800;   // 되돌릴 수 없는 유일한 기믹
    default: return 0;
    }
}

// 동작 자체에 쓰는 시간. 나머지는 도장·배너가 걷히는 짧은 여운이다.
static int GimmickFxAction(int kind, int b) {
    switch (kind) {
    case GIMMICK_SIGNATURE: return 560;
    case GIMMICK_SEVENTEENTH: return 560;
    case GIMMICK_LAST_WRITE: return 1150;
    case GIMMICK_ACCESS_DENIED:  return 1100;
    case GIMMICK_KERNEL_PANIC:   return 1250;
    case GIMMICK_BLUE_SCREEN:    return b == SHRED_FX_RESTORE ? 1350 : 1500;
    case GIMMICK_RESTORE_POINT:  return 1150;
    case GIMMICK_TAPE_LOOP:      return 560;
    case GIMMICK_MASTER_BACKUP:  return 1700;
    case GIMMICK_AUTOPLAY:       return 860;
    case GIMMICK_UNSAFE_EJECT:   return 1000;
    case GIMMICK_NO_MEDIA:       return 760;
    case GIMMICK_PROXY:          return 1250;
    case GIMMICK_ROUTING_LOOP:   return 1350;
    case GIMMICK_TIMEOUT:        return 540;
    case GIMMICK_LEAK:           return 940;
    case GIMMICK_HEAP_OVERFLOW:  return 1080;
    case GIMMICK_OUT_OF_MEMORY:  return 1600;
    case GIMMICK_SAMPLE13:       return 1080;
    case GIMMICK_SANDBOX_BREACH: return 1400;
    case GIMMICK_ZERO_DAY:       return 1900;
    default: return 900;
    }
}

// 히트스톱 시점. 철문이 닿고, 봉인이 내려앉고, 되감기 머리가 도착하는 그 순간이다.
int GimmickFxImpactAt(int kind, int b) {
    switch (kind) {
    case GIMMICK_LAST_WRITE:    return 1000;
    case GIMMICK_ACCESS_DENIED:  return 983;    // 철문 착지 = 동작의 89%
    case GIMMICK_KERNEL_PANIC:   return 1118;
    case GIMMICK_BLUE_SCREEN:    return b == SHRED_FX_RESTORE ? 0 : SHRED_IMPACT;
    case GIMMICK_RESTORE_POINT:  return 1150;
    case GIMMICK_MASTER_BACKUP:  return 200;
    case GIMMICK_AUTOPLAY:       return 360;
    case GIMMICK_UNSAFE_EJECT:   return 540;
    case GIMMICK_PROXY:          return 500;
    case GIMMICK_ROUTING_LOOP:   return 540;
    case GIMMICK_TIMEOUT:        return b ? 380 : 0;
    case GIMMICK_LEAK:           return 720;
    case GIMMICK_HEAP_OVERFLOW:  return 860;
    case GIMMICK_OUT_OF_MEMORY:  return 970;
    case GIMMICK_SAMPLE13:       return 760;
    case GIMMICK_SANDBOX_BREACH: return 850;
    case GIMMICK_ZERO_DAY:       return 800;
    default: return 0;                          // TAPE.LOOP · NO.MEDIA는 매턴이라 멈추지 않는다
    }
}

// 계열 색은 그 보스의 드라이브 색을 그대로 쓴다. 별도 표를 두지 않는다.
static int BossCardIndex() {
    for (int i = 0; i < gGame.enemyCount; ++i)
        if (IsBossKind(gGame.enemies[i].kind)) return i;
    return -1;
}

static COLORREF FxColor() {
    int boss = BossCardIndex();
    if (boss >= 0) return (COLORREF)GetEnemyInfoOrUnknown(gGame.enemies[boss].kind)->color;
    return C_RED;
}

// 충격 프레임. 시작 직후 화면을 계열 색으로 때리고 가로줄 밀도를 낮추며 흩어진다.
// 알파 없이 줄 간격만으로 밝기를 내므로 GDI만으로 충분히 빠르다.
static void DrawFxImpact(HDC dc, const RECT& area, int t, int life, COLORREF fam) {
    if (t < 0 || t >= life || life <= 0) return;
    int power = 1000 - t * 1000 / life;
    int step = power >= 860 ? 1 : power >= 640 ? 2 : power >= 420 ? 3 : power >= 220 ? 5 : 9;
    COLORREF hot = MixColor(fam, RGB(255, 255, 255), power / 14);
    for (int y = area.top; y < area.bottom; y += step) Fill(dc, MakeRect(area.left, y, area.right, y + 1), hot);
}

// 화면을 가로 띠로 잘라 좌우로 어긋나게 복사한다. 이미 그려진 프레임을 비트는 것이라
// 무엇 위에 얹든 "신호가 흔들린다"로 읽힌다.
static void DrawFxTear(HDC dc, const RECT& area, int t, int amp, int seed) {
    if (amp <= 0) return;
    const int bands = 13;
    int h = (area.bottom - area.top) / bands;
    if (h <= 0) return;
    for (int i = 0; i < bands; ++i) {
        int y = area.top + i * h;
        int dx = (int)(Hash3(seed, i, t / 45) % (uint32_t)(amp * 2 + 1)) - amp;
        if (dx != 0) BitBlt(dc, dx, y, BASE_WIDTH, h, dc, 0, y, SRCCOPY);
    }
}

// 중심선에서 위아래로 퍼지는 충격파. 되돌릴 수 없는 사건에만 쓴다.
static void DrawFxWave(HDC dc, int cy, int t, int life, COLORREF fam) {
    if (t < 0 || t >= life || life <= 0) return;
    int p = t * 1000 / life;
    int reach = (BASE_HEIGHT / 2 + 60) * p / 1000;
    int fade = 100 - p / 10;
    if (fade <= 2) return;
    for (int i = 0; i < 3; ++i) {
        int off = reach - i * 7;
        if (off <= 0) continue;
        COLORREF c = MixColor(C_BG, fam, fade - i * 22 > 0 ? fade - i * 22 : 0);
        int top = cy - off, bot = cy + off;
        if (top >= 68) Fill(dc, MakeRect(0, top, BASE_WIDTH, top + 2), c);
        if (bot < BASE_HEIGHT) Fill(dc, MakeRect(0, bot - 2, BASE_WIDTH, bot), c);
    }
}


// 판 전체를 가로지르는 배너. 강한 발동 넷만 쓴다.
static void DrawFxBanner(HDC dc, int kind, int t, int dur, COLORREF fam) {
    const int full = 84, inMs = 110, outMs = 140;
    int h;
    if (t < inMs) {
        // 오버슈트: 목표 높이를 지나쳤다가 되돌아온다. 선형보다 훨씬 세게 꽂힌다.
        int e = t * 1000 / inMs;
        int over = e < 680 ? e * 1320 / 680 : 1320 - (e - 680) * 320 / 320;
        h = full * over / 1000;
    } else if (t > dur - outMs) h = full * (dur - t) / outMs;
    else h = full;
    if (h <= 4) return;
    int top = 300 - h / 2;
    RECT band = MakeRect(0, top, BASE_WIDTH, top + h);

    Fill(dc, band, RGB(5, 7, 11));
    for (int i = 0; i < 6; ++i) {
        COLORREF g = MixColor(RGB(5, 7, 11), fam, 46 - i * 7);
        Fill(dc, MakeRect(0, band.top + 3 + i, BASE_WIDTH, band.top + 4 + i), g);
        Fill(dc, MakeRect(0, band.bottom - 4 - i, BASE_WIDTH, band.bottom - 3 - i), g);
    }
    Fill(dc, MakeRect(0, band.top, BASE_WIDTH, band.top + 3), fam);
    Fill(dc, MakeRect(0, band.bottom - 3, BASE_WIDTH, band.bottom), fam);
    for (int y = band.top + 5; y < band.bottom - 5; y += 3) Fill(dc, MakeRect(0, y, BASE_WIDTH, y + 1), RGB(9, 12, 17));

    if (h < 64) return;
    const BossGimmickInfo* gi = &BOSS_GIMMICK_INFO[kind];
    RECT line = MakeRect(0, band.top + 8, BASE_WIDTH, band.top + 52);

    // 스탬프가 노이즈에서 왼쪽부터 풀려나며 자리를 잡는다.
    wchar_t shown[48];
    const int settle = 170;
    if (t < settle) {
        wchar_t scrambled[48];
        CorruptCode(gi->stamp, scrambled, 48, kind * 13 + 7, GetTickCount());
        int keep = lstrlenW(gi->stamp) * t / settle;
        int n = 0;
        for (; gi->stamp[n] && n < 47; ++n) shown[n] = n < keep ? gi->stamp[n] : scrambled[n];
        shown[n] = 0;
    } else lstrcpyW(shown, gi->stamp);

    TextRect(dc, line, shown, fam, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, band.top + 54, BASE_WIDTH, band.top + 78), gi->name,
        MixColor(RGB(5, 7, 11), fam, 58), gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 계열 발동 도장. 보스 카드 위에 짧게 찍히고 사라진다. 화면 중앙을 오래
// 점유하던 공용 배너를 대신하며, 사건이 어디서 났는지도 함께 말해 준다.
static void DrawFxStamp(HDC dc, int kind, int t, int dur, COLORREF fam) {
    int fade = t < 70 ? 1000 * t / 70 : t > dur - 110 ? 1000 * (dur - t) / 110 : 1000;
    if (fade <= 0) return;
    if (fade > 1000) fade = 1000;
    int boss = BossCardIndex();
    RECT card = EnemyRect(boss < 0 ? 0 : boss);
    RECT tag = MakeRect(card.left + 6, card.top + 98, card.right - 6, card.top + 130);
    Fill(dc, tag, RGB(6, 9, 14));
    Outline(dc, tag, MixColor(C_BG, fam, 20 + fade * 70 / 1000), 1);
    TextRect(dc, tag, BOSS_GIMMICK_INFO[kind].stamp, MixColor(C_BG, fam, 20 + fade * 80 / 1000),
        gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---- C:\ 잠금 -------------------------------------------------------------
// 보스 카드에서 얇은 경로가 슬롯까지 내려오고, 도착한 슬롯의 좌우 테두리가
// 가운데로 닫힌다. 슬롯 하나로 끝나는 사건이므로 화면은 건드리지 않는다.
// 셔터가 지금 얼마나 내려왔는가 (0~1000). 그리기와 잠금 표시 보류가 같은 계산을
// 봐야 하므로 한 곳에 둔다.
static int LockShutterFall(int act) {
    if (act < 520) return 0;                       // 아직 신호가 슬롯에 닿는 중
    return ShutterFall((act - 520) * 1000 / 480);
}

// 상부 박스에서 철문이 풀려 내려와 칸을 덮는다. 층이 깊을수록 두꺼운 문이 온다.
static void DrawLockShutter(HDC dc, int slot, int act, COLORREF fam, int style, const wchar_t* label) {
    if (slot < 0 || slot >= SLOT_COUNT) return;
    RECT r = SlotRect(slot);
    int boss = BossCardIndex();
    RECT card = EnemyRect(boss < 0 ? 0 : boss);
    POINT from = CfxPoint((card.left + card.right) / 2, card.bottom - 2);
    POINT to = CfxPoint((r.left + r.right) / 2, r.top - 2);
    if (act < 520) {
        DrawSignalPath(dc, from, to, CFX_ROUTE_Y, act * 1000 / 520, 2, C_RED, 10, 0);
        return;
    }
    int fall = LockShutterFall(act);
    int height = r.bottom - r.top;
    DrawShutter(dc, r, style, Lerp(0, height, fall), fall < 1000, fam);
    if (fall >= 880) {
        // 셔터에 붙은 안내판. 실제 닫힌 상가의 종이 공고와 같은 자리다.
        RECT sign = MakeRect(r.left + 12, r.top + 46, r.right - 12, r.top + 76);
        Fill(dc, sign, RGB(18, 9, 11));
        Outline(dc, sign, C_RED, 2);
        TextRect(dc, sign, label, C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// 계산 재생 중이거나 셔터가 아직 안 내려왔으면 잠금 표시를 미룬다. 규칙은 이미
// 잠겨 있지만, 화면에서는 철문이 닿는 그 순간에 잠겨야 연출이 사건이 된다.
int GimmickLockPending(int slot) {
    int kind = gGame.boss.firedFx;
    if (kind != GIMMICK_ACCESS_DENIED && kind != GIMMICK_KERNEL_PANIC && kind != GIMMICK_BLUE_SCREEN && kind != GIMMICK_LAST_WRITE) return 0;
    if (kind == GIMMICK_BLUE_SCREEN) {
        // 파쇄는 fxB가 되돌아간 주사위 번호라 슬롯으로 읽으면 안 된다. 연출이
        // 도는 동안은 칸 그리기를 통째로 연출에 넘긴다 (깨지는 칸을 두 번 그리지 않는다).
        if (gGame.boss.fxA != slot || gGame.boss.fxB == SHRED_FX_RESTORE) return 0;
        if (gTurnTraceActive) return 1;
        return GimmickFxKind() == kind;
    }
    if (gGame.boss.fxA != slot && gGame.boss.fxB != slot) return 0;
    if (gTurnTraceActive) return 1;                    // 재생 중 — 아직 벌어지지 않은 일이다
    if (GimmickFxKind() != kind) return 0;             // 연출이 끝났다 — 이제 잠긴 게 맞다
    int t = GimmickFxElapsed();
    int dur = GimmickFxDuration(kind, GimmickFxB());
    int actionMs = GimmickFxAction(kind, GimmickFxB());
    if (actionMs > dur) actionMs = dur;
    if (actionMs <= 0) return 0;
    int act = t < actionMs ? t * 1000 / actionMs : 1000;
    return LockShutterFall(act) < 880;
}

int GimmickSummonPending(int enemyIndex) {
    if (gGame.boss.firedFx != GIMMICK_SANDBOX_BREACH || gGame.boss.fxA != enemyIndex) return 0;
    if (gTurnTraceActive) return 1;                            // 재생 중 — 아직 벌어지지 않은 일이다
    if (GimmickFxKind() != GIMMICK_SANDBOX_BREACH) return 0;   // 연출이 끝났다 — 이제 나와 있는 게 맞다
    return GimmickFxElapsed() < GimmickFxImpactAt(GIMMICK_SANDBOX_BREACH, GimmickFxB());   // 막이 깨지기 전
}

// ---- D:\ 복원 -------------------------------------------------------------
// 체력 막대 위에 복원 지점을 세우고, 유령 막대가 먼저 나타난 뒤 스캔 머리가
// 오른쪽에서 왼쪽으로 지나가며 실제 체력을 덮어쓴다. 일반 회복과 방향이 반대다.
static void DrawRestoreRewind(HDC dc, int act, COLORREF fam) {
    int target = gGame.boss.fxEnemy;
    if (target < 0 || target >= gGame.enemyCount) target = BossCardIndex();
    if (target < 0) return;
    const EnemyState* enemy = &gGame.enemies[target];
    if (enemy->maxHp <= 0) return;
    RECT card = EnemyRect(target);
    RECT body = EnemyInfoBody(card);
    RECT bar = MakeRect(body.left + 12, body.top + 208, body.right - 12, body.top + 220);
    int width = bar.right - bar.left;
    if (gGame.boss.gimmick == GIMMICK_SEVENTEENTH) {
        int head = card.right - 12 - (card.right - card.left - 24) * act / 1000;
        Fill(dc, MakeRect(head - 2, card.top + 12, head + 2, card.top + 132), fam);
        wchar_t label[48]; wsprintfW(label, L"출력 복제 +%d", GimmickFxA());
        TextRect(dc, MakeRect(card.left + 8, card.top + 96, card.right - 8, card.top + 130), label, C_YELLOW, gFontSmall, DT_CENTER | DT_SINGLELINE);
        return;
    }
    int before = gGame.boss.fxHpBefore, after = gGame.boss.fxHpAfter;
    if (after <= before) return;
    int fromX = bar.left + width * before / enemy->maxHp;
    int toX = bar.left + width * after / enemy->maxHp;

    // 복원 지점 표식
    Fill(dc, MakeRect(toX - 1, bar.top - 6, toX + 1, bar.bottom + 6), C_YELLOW);
    // 되감기는 오른쪽 끝에서 출발해 왼쪽의 현재 체력 쪽으로 훑고 지나간다
    int head = toX - (toX - fromX) * act / 1000;
    Fill(dc, MakeRect(fromX, bar.top, toX, bar.bottom), MixColor(C_BG, fam, 70));
    Fill(dc, MakeRect(head - 2, bar.top - 3, head + 2, bar.bottom + 3),
        MixColor(fam, RGB(255, 255, 255), 55));
    // 초상화 위를 되감기 주사선이 반대 방향으로 지나간다
    RECT art = PortraitRect(card);
    int off = 14 - (act * 14 / 1000) % 14;
    for (int y = art.top + off; y < art.bottom; y += 14)
        Fill(dc, MakeRect(art.left, y, art.right, y + 2), MixColor(C_BG, fam, 48));
}

// ---- E:\ 오프라인 ---------------------------------------------------------
// 주사위 카드가 5~7개 행으로 갈라져 일부는 밀리고 일부는 통째로 비워진다.
static void DrawDieRowSplit(HDC dc, int die, int act, int rows, COLORREF tone) {
    if (die < 0 || die >= 3) return;
    RECT r = DieRect(die);
    int h = (r.bottom - r.top) / rows;
    if (h <= 0) return;
    int amp = 5 * act / 1000;
    for (int i = 0; i < rows; ++i) {
        int y = r.top + i * h;
        int bottom = i == rows - 1 ? r.bottom : y + h;
        uint32_t hash = Hash3(die, i, 41);
        if ((hash % 5u) == 0) {           // 신호가 아예 빠진 행
            Fill(dc, MakeRect(r.left + 1, y, r.right - 1, bottom), RGB(9, 9, 11));
            continue;
        }
        int dx = (int)((hash >> 7) % (uint32_t)(amp * 2 + 1)) - amp;
        if (dx != 0) BitBlt(dc, r.left + dx, y, r.right - r.left, bottom - y, dc, r.left, y, SRCCOPY);
    }
    Outline(dc, r, tone, 2);
}

// ---- N:\ 경로 -------------------------------------------------------------
// 슬롯은 절대 자리를 바꾸지 않는다. 바뀌는 것은 슬롯 아래를 잇는 연결선뿐이다.
static const int ROUTE_ORDER[SLOT_COUNT] = {SLOT_AMPLIFY, SLOT_ATTACK, SLOT_DEFEND, SLOT_CHAIN};
static const int ROUTE_ORDER_REVERSED[SLOT_COUNT] = {SLOT_CHAIN, SLOT_DEFEND, SLOT_ATTACK, SLOT_AMPLIFY};

// build 0~1000: 왼쪽 구간부터 차례로 이어 붙는다. build < 0이면 전부 이어진 상태.
static void DrawRoutingBus(HDC dc, const int* order, int build, COLORREF color, int stalled) {
    int y = CFX_SLOT_ROUTE_Y;
    int segments = SLOT_COUNT - 1;
    for (int k = 0; k < SLOT_COUNT; ++k) {
        RECT r = SlotRect(order[k]);
        int cx = (r.left + r.right) / 2;
        Fill(dc, MakeRect(cx - 1, r.bottom + 2, cx + 1, y), color);
        Fill(dc, MakeRect(cx - 4, y - 1, cx + 4, y + 2), color);
        if (k >= segments) break;
        RECT next = SlotRect(order[k + 1]);
        int nx = (next.left + next.right) / 2;
        int progress = build < 0 ? 1000 : build * segments - k * 1000;
        if (progress <= 0) continue;
        if (progress > 1000) progress = 1000;
        int span = nx - cx;
        int drawn = span * progress / 1000;
        int lo = drawn < 0 ? cx + drawn : cx, hi = drawn < 0 ? cx : cx + drawn;
        Fill(dc, MakeRect(lo, y, hi, y + 2), color);
        // 진행 방향 화살촉
        if (progress >= 1000) {
            int ax = nx - (span > 0 ? 7 : -7);
            Fill(dc, MakeRect(ax - 3, y - 3, ax + 3, y + 5), color);
        }
    }
    if (stalled) {
        // 예고: 신호가 중간에서 멈추고 화살표가 깜빡인다
        RECT a = SlotRect(order[1]), b = SlotRect(order[2]);
        int mx = ((a.left + a.right) / 2 + (b.left + b.right) / 2) / 2;
        if ((GetTickCount() / 260) % 2 == 0) {
            Fill(dc, MakeRect(mx - 7, y - 6, mx + 7, y + 8), C_YELLOW);
            Fill(dc, MakeRect(mx - 3, y - 2, mx + 3, y + 4), C_BG);
        }
    }
}

// 전투 중 상시 표시. 지금 어떤 순서로 이어져 있는지가 늘 보이므로, 발동
// 순간의 재배선이 "무엇이 바뀌었는지"로 읽힌다.
static void DrawRoutingState(HDC dc) {
    if (gGame.boss.gimmick == GIMMICK_NONE) return;
    if (BOSS_GIMMICK_INFO[gGame.boss.gimmick].family != FAM_ROUTE) return;
    int reversed = ResolveOrderReversed(&gGame);
    DrawRoutingBus(dc, reversed ? ROUTE_ORDER_REVERSED : ROUTE_ORDER, -1,
        reversed ? MixColor(C_BG, C_RED, 55) : RGB(38, 52, 63), gGame.boss.nextReversed);
}

// ---- R:\ 압력 -------------------------------------------------------------
// 게이지가 차오르는 동안 보스 카드에서 작은 ALLOC 패킷이 떨어져 나온다.
static void DrawPressureAlloc(HDC dc, int act, COLORREF fam) {
    int boss = BossCardIndex();
    if (boss < 0) return;
    RECT card = EnemyRect(boss);
    RECT gauge = MakeRect(card.left + 12, card.top + 222, card.right - 12, card.top + 229);
    int x = card.left + 20 + (gauge.right - gauge.left) * act / 1000;
    if (x > gauge.right) x = gauge.right;
    Fill(dc, MakeRect(x - 8, gauge.top - 16, x + 8, gauge.top - 6), fam);
    TextRect(dc, MakeRect(card.left, gauge.top - 34, card.right, gauge.top - 16), L"ALLOC",
        MixColor(C_BG, fam, 40 + act / 20), gFontSmall, DT_CENTER | DT_SINGLELINE);
    // 한계에 닿으면 카드 안쪽 테두리가 조여든다
    if (gGame.boss.empowered) {
        int inset = 3 + 5 * act / 1000;
        RECT inner = card;
        InflateRect(&inner, -inset, -inset);
        Outline(dc, inner, MixColor(C_BG, C_RED, 60), 2);
    }
}

// ---- X:\ 격리 -------------------------------------------------------------
// 여섯 면 띠의 그 칸만 잠긴다. 주사위 카드 밖으로는 나가지 않는다.
static void DrawSealRing(HDC dc, const RECT& target, int p, COLORREF color);
static void DrawQuarantineSeal(HDC dc, int die, int face, int act, COLORREF fam, int permanent) {
    if (gGame.boss.gimmick == GIMMICK_LAST_WRITE) {
        if (die < 0 || die >= SLOT_COUNT) return;
        RECT slot = SlotRect(die);
        DrawSealRing(dc, slot, act, fam);
        DrawLockShutter(dc, die, act, fam, SHUTTER_MESH, L"SLOT SEALED");
        return;
    }
    if (die < 0 || die >= 3) return;
    RECT r = DieRect(die);
    // 주사위 값 칸을 헥스 덤프가 절반쯤 덮는다
    RECT area = MakeRect(r.left + 6, r.top + 26, r.right - 6, r.top + 26 + 46 * act / 1000);
    if (area.bottom > area.top) {
        Fill(dc, area, RGB(12, 6, 8));
        DrawHexBlock(dc, area, fam, die * 31 + face, GetTickCount(), 4);
    }
    if (face < 0 || face >= 6) return;
    // 대상 칸으로 봉인이 내려앉는다
    RECT cell = FaceStripCell(die, face);
    int drop = 20 - 20 * act / 1000;
    RECT sealed = MakeRect(cell.left, cell.top - drop, cell.right, cell.bottom - drop);
    Fill(dc, sealed, permanent ? RGB(10, 10, 12) : RGB(40, 12, 14));
    Outline(dc, sealed, permanent ? C_RED : fam, 2);
    if (act > 600) TextRect(dc, MakeRect(r.left, r.bottom - 22, r.right, r.bottom - 4),
        permanent ? L"FACE DELETED" : L"FACE QUARANTINED", permanent ? C_RED : fam,
        gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// ---- 공용 애니메이션 헬퍼 ---------------------------------------------------

// 예비 동작: 대상 사각형 테두리가 잘게 떨린다. 뭔가 오기 직전이라는 신호.
static void DrawTremble(HDC dc, const RECT& r, int t, int power, COLORREF color) {
    if (power <= 0) return;
    int jx = (int)(Hash3(r.left, t / 28, 3) % (uint32_t)(power * 2 + 1)) - power;
    int jy = (int)(Hash3(r.top, t / 28, 5) % (uint32_t)(power * 2 + 1)) - power;
    RECT m = MakeRect(r.left + jx, r.top + jy, r.right + jx, r.bottom + jy);
    Outline(dc, m, color, 2);
}

// 시간차로 태어나는 파편. i번째는 i*stagger ms 뒤에 튀어 층을 이룬다.
static void DrawFxShardsStaggered(HDC dc, int cx, int cy, int t, int life, int count, int seed, COLORREF fam, int stagger) {
    if (count <= 0) return;
    int scaled = FxScale(count);
    for (int i = 0; i < scaled; ++i) {
        int age = t - i * stagger;
        if (age < 0 || age >= life) continue;
        int p = age * 1000 / life;
        int fade = 100 - p / 10;
        if (fade <= 2) continue;
        uint32_t h = Hash3(seed, i, 31);
        int dx = (int)(h % 401u) - 200, dy = (int)((h >> 10) % 401u) - 200;
        int speed = 80 + (int)((h >> 21) % 110u);
        int x = cx + dx * p * speed / 200000;
        int y = cy + dy * p * speed / 200000 + p * p / 7000;
        if (x < 0 || x >= BASE_WIDTH || y < 68 || y >= BASE_HEIGHT) continue;
        COLORREF c = MixColor(C_BG, fam, fade);
        int w = 3 + (int)((h >> 5) % 6u), hh = 2 + (int)((h >> 8) % 3u);
        Fill(dc, MakeRect(x, y, x + w, y + hh), (i & 3) == 0 ? MixColor(c, RGB(255, 255, 255), 50) : c);
    }
}

// 조여드는 봉인 링. 바깥에서 사각형으로 좁혀 들어와 대상 위에서 멈춘다.
static void DrawSealRing(HDC dc, const RECT& target, int p, COLORREF color) {
    if (p <= 0) return;
    int spread = 60 - 60 * (p > 1000 ? 1000 : p) / 1000;
    RECT r = target; InflateRect(&r, spread, spread);
    Outline(dc, r, color, 2);
    if (spread > 8) { RECT r2 = target; InflateRect(&r2, spread / 2, spread / 2); Outline(dc, r2, MixColor(C_BG, color, 50), 1); }
}

// 균열. 한 점에서 화면 안쪽으로 뻗는 톱니선.
static void DrawCracks(HDC dc, int ox, int oy, int reach, int seed, COLORREF hot, COLORREF dim) {
    for (int i = 0; i < 9; ++i) {
        uint32_t h = Hash3(seed, i, 7);
        int dx = (int)(h % 201u) - 100, dy = (int)((h >> 9) % 201u) - 100;
        if (dx == 0 && dy == 0) dx = 80;
        for (int s = 1; s <= 18; ++s) {
            int len = reach * s / 18;
            int jag = (int)(Hash3(seed, i * 31 + s, 13) % 7u) - 3;
            int x = ox + dx * len / 150 + jag, y = oy + dy * len / 150 - jag;
            if (x < 0 || x >= BASE_WIDTH || y < 68 || y >= BASE_HEIGHT) break;
            Fill(dc, MakeRect(x, y, x + 3, y + 2), (s & 1) ? hot : dim);
        }
    }
}

// 사이드바 SYSTEM 칸의 해결 순서 줄이 노이즈로 갈렸다 새 순서로 재조립된다.
// 경로 기믹의 "무엇이 바뀌었는가"를 글자 그대로 보여 준다.
static void DrawOrderScramble(HDC dc, int t, int settleFrom, int settleTo, int reversed, COLORREF fam) {
    RECT line = SystemOrderRect();
    // 다 맞춰진 글은 SYSTEM 칸이 그 뒤로 그리는 줄과 똑같아야 이어짐이 튀지 않는다.
    wchar_t target[64];
    FormatOrderLine(dc, reversed, line.right - line.left, target, 64);
    int p = Track(t, settleFrom, settleTo);
    Fill(dc, line, C_PANEL);
    if (p >= 1000) { TextRect(dc, line, target, reversed ? C_RED : C_TEXT, gFontSmall, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS); return; }
    // 한글은 폭이 커서 글자 치환 대신 노이즈 블록으로 덮고, 오른쪽부터 걷어 낸다
    int n = lstrlenW(target);
    int keep = n * p / 1000;
    wchar_t shown[64]; int i = 0;
    for (; target[i] && i < 63; ++i) shown[i] = i < keep ? target[i] : ((Hash3(i, t / 40, 9) & 1) ? L'#' : L'?');
    shown[i] = 0;
    TextRect(dc, line, shown, MixColor(fam, C_TEXT, 40), gFontSmall, DT_SINGLELINE | DT_VCENTER);
}

// 보스 카드 위 탁자: 초상이 잠깐 늘어났다 돌아온다 (천분율). 되감기·압력용.
static void DrawBossSquash(HDC dc, int sxMille, int syMille, int flash) {
    int boss = BossCardIndex();
    if (boss < 0) return;
    const EnemyState* e = &gGame.enemies[boss];
    if (!e->alive) return;
    RECT card = EnemyRect(boss);
    RECT art = PortraitRect(card);
    DrawSpriteStretched(dc, art, e->kind, 1, flash, sxMille, syMille);
}

// ---- C:\ 3층 파쇄 ---------------------------------------------------------
// 보스가 칸 하나를 내려찍어 없앤다. 조준 → 내려찍기 → 충돌 → 오염 → 깨짐 →
// 빈 구멍. 오염과 깨짐은 사망 연출의 글자 분해(DrawDeathRot)를 칸 이름 위에서
// 그대로 돌린 것이고, 바탕은 맞은 자리부터 4px 점 단위로 먹혀 들어간다.
#define SHRED_BITE 5    // 오염이 1px 번지는 데 걸리는 ms
#define SHRED_HOLE 1300 // 충돌 이후 이만큼 지나면 칸이 다 사라져 빈 구멍만 남는다

// 칸 바탕을 4px 점으로 갉아 먹는다. 격자가 아니라 맞은 곳에서 가까운 점부터
// 물들고, 먹히는 순간 불씨처럼 한 번 빛난 뒤 구멍이 된다. 거리에 잡음을 얹어
// 번지는 앞이 고르지 않게 들어온다.
static void DrawShredDots(HDC dc, const RECT& r, int slot, int age) {
    int cx = (r.left + r.right) / 2;
    for (int y = r.top; y < r.bottom; y += 4) {
        for (int x = r.left; x < r.right; x += 4) {
            int dx = x + 2 - cx; if (dx < 0) dx = -dx;
            int dy = (y + 2 - r.top) * 6 / 5;
            int dist = dx > dy ? dx + dy / 2 : dy + dx / 2;          // 값싼 거리
            int since = age - dist * SHRED_BITE - (int)(Hash3(slot, x, y) % 150u);
            if (since < 0) continue;
            RECT cell = MakeRect(x, y, x + 4, y + 4);
            if (since < 240) { Fill(dc, cell, MixColor(C_PANEL, RGB(118, 108, 86), since * 100 / 240)); continue; }
            if (since < 360) { Fill(dc, cell, MixColor(RGB(30, 14, 10), RGB(235, 140, 62), 100 - (since - 240) * 100 / 120)); continue; }
            Fill(dc, cell, RGB(5, 7, 10));
        }
    }
}

static void DrawShredStrike(HDC dc, int slot, int die, int t, COLORREF fam) {
    if (slot < 0 || slot >= SLOT_COUNT) return;
    RECT r = SlotRect(slot);
    int cx = (r.left + r.right) / 2, boss = BossCardIndex();
    // 초상은 칸과 같은 크기로 옮겨 간다. 판 위를 지나며 부풀면 다른 칸을 가린다.
    RECT art = PortraitRect(EnemyRect(boss < 0 ? 0 : boss));
    int bw = (r.right - r.left) / 2, bh = (r.bottom - r.top) / 2;
    int acx = (art.left + art.right) / 2, acy = (art.top + art.bottom) / 2;
    RECT home = MakeRect(acx - bw, acy - bh, acx + bw, acy + bh);
    RECT over = MakeRect(r.left, r.top - 200, r.right, r.top - 76);   // 칸 위에 떠서 겨눈다
    RECT high = MakeRect(r.left, r.top - 300, r.right, r.top - 176);  // 예비: 판 안에 머문 채 뽑혀 올라간다
    int since = t - SHRED_IMPACT;

    if (since < 0) {
        // 조준선이 네 모서리에서 좁혀 든다
        int close = EaseOutCubic(Track(t, 150, SHRED_IMPACT));
        int off = Lerp(26, 3, close);
        COLORREF aim = MixColor(C_BG, C_RED, 55 + close * 45 / 1000);
        for (int i = 0; i < 4; ++i) {
            int x = (i & 1) ? r.right + off : r.left - off, dx = (i & 1) ? -16 : 16;
            int y = (i & 2) ? r.bottom + off : r.top - off, dy = (i & 2) ? -12 : 12;
            DrawLine(dc, x, y, x + dx, y, aim, 2);
            DrawLine(dc, x, y, x, y + dy, aim, 2);
        }
        RECT tag = MakeRect(r.left + 3, r.bottom - 29, r.right - 3, r.bottom - 5);
        Fill(dc, tag, RGB(38, 16, 18));
        TextRect(dc, tag, close > 880 ? L"TARGET LOCK" : L"파쇄 예고", C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        // 예고를 무시하고 올려 둔 주사위는 아직 칸 안에 보인다 (규칙은 이미 되돌렸다)
        if (die >= 0 && die < 3) {
            const Face* face = &gGame.dice[die].faces[gGame.dice[die].rolledFace];
            wchar_t value[24]; FormatFace(face, value);
            RECT band = MakeRect(r.left + 2, r.top + 38, r.right - 2, r.top + 86);
            Fill(dc, band, C_PANEL);
            TextRect(dc, band, value, FaceColor(face), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    } else {
        // 칸을 연출이 넘겨받는다. 오염이 번지고, 이름이 기호로 떨다 갈라져 떨어진다.
        Fill(dc, r, C_PANEL);
        DrawShredDots(dc, r, slot, since);
        int rot = Track(since, 0, 620);
        if (since < 760) Outline(dc, r, MixColor(C_RED, RGB(206, 128, 52), rot / 10), 2);
        if (since < SHRED_HOLE) {
            HFONT old = (HFONT)SelectObject(dc, gFontMedium);
            DeathRot name;
            name.origin = 0; name.infect = 60; name.step = 30; name.brk = 430; name.breakStep = 26;
            name.gap = since >= 430 ? 4 : 0; name.seed = slot * 97 + 31;
            DrawDeathRot(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], lstrlenW(SLOT_SHORT_NAMES[slot]), name, since, MixColor(C_BG, C_TEXT, 92), 4);
            SelectObject(dc, old);
        } else DrawShredHole(dc, r, slot, SlotShredTurnsLeft(&gGame, slot));
    }

    // 맞은 칸이 통째로 하얗게 날아간다. 초상보다 먼저 깔아 보스가 그 빛 앞에
    // 실루엣으로 박히게 한다. 가장 짧고 가장 센 층이다.
    int blow = (since >= 0 && since < 60) ? 1000 - since * 1000 / 60 : 0;
    if (blow > 0 && FxDecorOn()) {
        RECT hot = r;
        InflateRect(&hot, 6 + 26 * blow / 1000, 4 + 18 * blow / 1000);
        Fill(dc, hot, MixColor(C_BG, RGB(255, 246, 238), FxScale(96 * blow / 1000)));
        Outline(dc, hot, MixColor(C_BG, C_RED, FxScale(55 + 45 * blow / 1000)), 3);
    }

    // 초상의 내려찍기. 타격감은 궤적이 아니라 착지에서 난다 — 위로 뽑혀 올라가
    // 세로로 길게 늘어난 채 떨어지고, 칸을 지나쳐 박히며 한 번 납작해졌다가
    // 곧장 튕겨 오른다. 누르고 있지 않는다.
    RECT box;
    int sx = 1000, sy = 1000, flash = 0;
    RECT land = over; OffsetRect(&land, 0, 116);     // 칸 윗변을 지나쳐 박히는 자리
    if (since < 0) {
        int drop = EaseInCubic(Track(t, 880, SHRED_IMPACT));         // 216px을 120ms에, 끝에서 몰아친다
        if (t < 880) box = LerpRect(LerpRect(home, over, EaseOutCubic(Track(t, 100, 600))),
                                    high, EaseOutCubic(Track(t, 600, 840)));   // 판 밖까지 뽑혀 올라간다
        else box = LerpRect(high, land, drop);
        int wind = EaseOutCubic(Track(t, 600, 840));
        sy = 1000 + 150 * wind / 1000 + 420 * drop / 1000;           // 떨어지며 길게 늘어난다
        sx = 1000 - 80 * wind / 1000 - 240 * drop / 1000;
    } else if (since < 80) {
        box = land; OffsetRect(&box, 0, 26 - 26 * since / 80);       // 파고들었다 빠져나온다
        sy = 540 + 460 * since / 80;                                 // 납작해졌다 돌아온다
        sx = 1460 - 460 * since / 80;
        flash = 1000 - 1000 * since / 80;
    } else {
        // 반동. 밀려 올라갔다 제자리로 돌아간다.
        int kick = EaseOutCubic(Track(since, 80, 300));
        RECT up = over; OffsetRect(&up, 0, -34);
        box = LerpRect(land, up, kick);
        box = LerpRect(box, home, EaseInCubic(Track(since, 300, 640)));
        sy = 1000 + 90 * (1000 - kick) / 1000;
    }
    if (boss >= 0) {
        // 떠난 자리는 빈 상자로 남는다 — 초상이 둘로 보이지 않게
        int away = since < 0 ? Track(t, 100, 400) : 1000 - Track(since, 400, 640);
        if (away > 0) { Fill(dc, art, C_PANEL); Outline(dc, art, MixColor(C_BG, C_RED, 20 + away / 40), 1); }
        // 낙하 잔상: 지나온 길에 세로 줄이 길게 남는다
        if (FxDecorOn() && t >= 840 && since < 60) {
            int tail = since < 0 ? 70 + 260 * EaseInCubic(Track(t, 880, SHRED_IMPACT)) / 1000 : 330;
            for (int i = 0; i < 5; ++i) {
                int lx = box.left + 12 + i * (box.right - box.left - 24) / 4;
                int level = FxScale((i & 1 ? 46 : 72) * (since < 0 ? 1000 : 1000 - since * 1000 / 60) / 1000);
                int top = box.top - tail; if (top < 70) top = 70;
                if (level > 0 && box.top + 6 > top) Fill(dc, MakeRect(lx, top, lx + 2, box.top + 6), MixColor(C_BG, C_RED, level));
            }
        }
        // 초상은 판 안에서만 보인다. 판 밖으로 뽑혀 올라간 동안은 헤더를 넘지 않는다.
        int clipped = SaveDC(dc);
        if (clipped) IntersectClipRect(dc, 0, 69, BASE_WIDTH, BASE_HEIGHT);
        DrawSpriteArt(dc, box, gGame.enemies[boss].kind, 1, FxScale(flash), 0, 0, sx, sy);
        int heat = since < 0 ? Track(t, 500, SHRED_IMPACT) : 1000 - Track(since, 0, 420);
        if (heat > 0) Outline(dc, box, MixColor(C_BG, C_RED, 30 + heat / 18), 2);
        if (clipped) RestoreDC(dc, clipped);
    }

    if (since >= 0 && since < 900) {
        // 충돌: 바닥을 따라 뻗는 빛줄기, 충격파 두 겹, 균열, 파편
        int spread = EaseOutCubic(Track(since, 0, 170)), fade = 1000 - Track(since, 40, 520);
        if (fade > 0) {
            int reach = 420 * spread / 1000;
            Fill(dc, MakeRect(cx - reach, r.top - 1, cx + reach, r.top + 2), MixColor(C_BG, RGB(255, 226, 206), FxScale(fade / 11)));
            Fill(dc, MakeRect(cx - 2, r.top - 90 * spread / 1000, cx + 2, r.top), MixColor(C_BG, RGB(255, 226, 206), FxScale(fade / 16)));
        }
        if (FxDecorOn()) {
            for (int i = 0; i < 2; ++i) {
                int ring = Track(since - i * 70, 0, 380);
                if (ring <= 0 || ring >= 1000) continue;
                DrawGlowRing(dc, cx, r.top, 20 + 150 * ring / 1000, 8 + 54 * ring / 1000,
                    MixColor(C_BG, i ? fam : C_RED, 85 - 80 * ring / 1000), i ? 1 : 2);
            }
            if (since < 300) DrawCracks(dc, cx, r.top, Lerp(40, 210, EaseOutCubic(Track(since, 0, 300))), slot * 13 + 7, C_RED, MixColor(C_BG, C_RED, 45));
            DrawFxShardsStaggered(dc, cx, r.top, since, 620, 34, slot * 5 + 17, RGB(255, 176, 138), 5);
            // 맞은 자리에서 양옆으로 밀려나는 흙먼지
            DrawPixelBurst(dc, cx - 34, r.top + 2, since, 520, FxScale(9), slot * 3 + 1, MixColor(C_BG, RGB(150, 118, 96), 70));
            DrawPixelBurst(dc, cx + 34, r.top + 2, since, 520, FxScale(9), slot * 3 + 2, MixColor(C_BG, RGB(150, 118, 96), 70));
            DrawFxTear(dc, MakeRect(0, 68, BASE_WIDTH, BASE_HEIGHT), t, FxScale(since < 180 ? 30 - 30 * since / 180 : 0), GIMMICK_BLUE_SCREEN);
        }
    }

    // 임팩트 프레임. 히트스톱으로 판이 멈춘 80ms 동안 앞 40ms는 색이 뒤집히고
    // 뒤 40ms는 붉게 뒤집힌다. 멈춘 시간이 아니라 실제 시각으로 도는 유일한 층이다.
    // 화면 전체가 두 번 번쩍이므로 연출 강도를 낮추면 아예 그리지 않는다.
    if (FxScale(100) >= 100) {
        int raw = GimmickFxRawElapsed() - SHRED_IMPACT;
        if (raw >= 0 && raw < 80) {
            RECT sc = MakeRect(0, 68, BASE_WIDTH, BASE_HEIGHT);
            PatBlt(dc, sc.left, sc.top, sc.right - sc.left, sc.bottom - sc.top, DSTINVERT);
            if (raw >= 40) {
                HBRUSH tint = CreateSolidBrush(RGB(206, 44, 34));
                if (tint) {
                    HGDIOBJ was = SelectObject(dc, tint);
                    PatBlt(dc, sc.left, sc.top, sc.right - sc.left, sc.bottom - sc.top, PATINVERT);
                    SelectObject(dc, was);
                    DeleteObject(tint);
                }
            }
        }
    }

    // 배치 취소: Ctrl+Z처럼 곧은 길을 다섯 번 끊겨 뛰어 미배치 자리로 돌아간다.
    if (die >= 0 && die < 3 && since >= 120 && since < 700) {
        RECT tray = DieRect(die);
        int steps = 1 + Track(since, 120, 520) * 5 / 1000; if (steps > 5) steps = 5;
        int fx0 = cx, fy0 = r.top + 62, tx = (tray.left + tray.right) / 2, ty = tray.top + 52;
        const Face* face = &gGame.dice[die].faces[gGame.dice[die].rolledFace];
        wchar_t value[24]; FormatFace(face, value);
        for (int k = 1; k <= steps; ++k) {
            int p = k * 1000 / 5, last = k == steps;
            int gx = Lerp(fx0, tx, p), gy = Lerp(fy0, ty, p);
            RECT g = MakeRect(gx - 31, gy - 25, gx + 31, gy + 25);
            if (!last) { Outline(dc, g, MixColor(C_BG, C_BLUE, 30), 1); continue; }
            Fill(dc, g, C_PANEL); Outline(dc, g, C_BLUE, 2);
            // 한 번 뛸 때마다 오염이 한 겹씩 벗겨진다
            TextRect(dc, g, value, MixColor(RGB(206, 128, 52), C_TEXT, p / 10), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RECT tag = MakeRect(g.left - 26, g.top - 23, g.right + 26, g.top - 3);
            Fill(dc, tag, C_INK); Outline(dc, tag, MixColor(C_BG, C_BLUE, 45), 1);
            TextRect(dc, tag, L"↶ 배치 취소", C_BLUE, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// 복구. 호박색 괄호가 칸을 조여 들고, 테두리가 먼저 그어지고, 주사선이 위에서
// 아래로 지나가며 칸을 한 줄씩 다시 찍는다. 벌이 아니라 되찾는 장면이라
// 히트스톱도 흔들림도 없다.
static void DrawShredRestore(HDC dc, int slot, int t) {
    if (slot < 0 || slot >= SLOT_COUNT) return;
    RECT r = SlotRect(slot);
    int cx = (r.left + r.right) / 2, h = r.bottom - r.top, w = r.right - r.left;
    Fill(dc, r, RGB(5, 7, 10));
    DrawShredNoise(dc, r, slot, t / 110, MixColor(C_BG, C_YELLOW, 45));
    int grip = EaseOutCubic(Track(t, 0, 340)), off = Lerp(34, 4, grip);
    COLORREF amber = MixColor(C_BG, C_YELLOW, 40 + grip / 18);
    for (int i = 0; i < 4; ++i) {
        int x = (i & 1) ? r.right + off : r.left - off, dx = (i & 1) ? -18 : 18;
        int y = (i & 2) ? r.bottom + off : r.top - off, dy = (i & 2) ? -13 : 13;
        DrawLine(dc, x, y, x + dx, y, amber, 2);
        DrawLine(dc, x, y, x, y + dy, amber, 2);
    }
    // 테두리를 둘레를 따라 한 바퀴 긋는다
    int run = (w * 2 + h * 2) * Track(t, 280, 640) / 1000;
    if (run > 0) {
        Fill(dc, MakeRect(r.left, r.top, r.left + (run < w ? run : w), r.top + 2), C_YELLOW);
        if (run > w) Fill(dc, MakeRect(r.right - 2, r.top, r.right, r.top + (run - w < h ? run - w : h)), C_YELLOW);
        if (run > w + h) Fill(dc, MakeRect(r.right - (run - w - h < w ? run - w - h : w), r.bottom - 2, r.right, r.bottom), C_YELLOW);
        if (run > w * 2 + h) Fill(dc, MakeRect(r.left, r.bottom - (run - w * 2 - h < h ? run - w * 2 - h : h), r.left + 2, r.bottom), C_YELLOW);
    }
    // 주사선이 지나간 자리는 호박빛이 식으며 원래 칸으로 돌아온다
    int scan = EaseOutCubic(Track(t, 620, 1350)), scanY = r.top + h * scan / 1000;
    if (scanY > r.top + 1) {
        RECT done = MakeRect(r.left, r.top, r.right, scanY);
        Fill(dc, done, C_PANEL);
        Fill(dc, MakeRect(r.left + 1, r.top + 1, r.right - 1, r.top + 4), MixColor(C_PANEL, SlotAccent(slot), 30));
        int saved = SaveDC(dc);
        if (saved) {
            IntersectClipRect(dc, done.left, done.top, done.right, done.bottom);
            Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], MixColor(SlotAccent(slot), C_YELLOW, scan < 300 ? 70 : 0), gFontMedium);
            TextRect(dc, MakeRect(r.left + 5, r.top + 48, r.right - 5, r.top + 89), L"비어 있음", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
            RestoreDC(dc, saved);
        }
    }
    if (scan < 1000 && t >= 620) {
        Fill(dc, MakeRect(r.left, scanY - 1, r.right, scanY + 2), MixColor(C_BG, RGB(255, 214, 150), 95));
        // 주사선 바로 밑에서 막 쓰이는 기호 줄이 깜빡인다
        if (FxDecorOn()) {
            static const wchar_t WRIT[] = L"#%&?@$*+=<>0123456789ABCDEF";
            for (int i = 0; i < 12; ++i) {
                uint32_t g = Hash3(slot, i, t / 45);
                wchar_t c[2] = {WRIT[g % (uint32_t)(sizeof(WRIT) / sizeof(WRIT[0]) - 1)], 0};
                Text(dc, r.left + 6 + i * 12, scanY + 3, c, MixColor(C_BG, C_YELLOW, 30 + (int)((g >> 9) % 60u)), gFontSmall);
            }
        }
    }
    // 칸 밑 진행 막대. 주사선과 함께 찬다. 제 바탕을 깔아 아래 연결선과 섞이지 않는다.
    int pct = Track(t, 620, 1350) / 10;
    RECT gauge = MakeRect(r.left, r.bottom + 4, r.right, r.bottom + 22);
    Fill(dc, gauge, C_INK);
    Fill(dc, MakeRect(gauge.left + 4, gauge.top + 7, gauge.right - 78, gauge.top + 10), RGB(42, 32, 16));
    Fill(dc, MakeRect(gauge.left + 4, gauge.top + 7, gauge.left + 4 + (gauge.right - 82 - gauge.left) * pct / 100, gauge.top + 10), C_YELLOW);
    wchar_t bar[24]; wsprintfW(bar, L"복구 %d%%", pct);
    TextRect(dc, MakeRect(gauge.right - 74, gauge.top, gauge.right - 4, gauge.bottom), bar, C_YELLOW, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    if (t >= 1350) {
        int done = t - 1350;
        if (FxDecorOn()) for (int i = 0; i < 2; ++i) {
            int ring = Track(done - i * 130, 0, 460);
            if (ring <= 0 || ring >= 1000) continue;
            RECT halo = r; InflateRect(&halo, 6 + 34 * ring / 1000, 6 + 24 * ring / 1000);
            Outline(dc, halo, MixColor(C_BG, i ? C_YELLOW : C_GREEN, 90 - 85 * ring / 1000), i ? 1 : 2);
        }
        if (done < 90) Fill(dc, r, MixColor(C_PANEL, RGB(230, 255, 245), 62 - done * 62 / 90));
        int pop = EaseOutCubic(Track(done, 0, 200));
        RECT say = MakeRect(r.left + 4, r.top + 44 - 8 * (1000 - pop) / 1000, r.right - 4, r.top + 82);
        Fill(dc, say, C_PANEL);
        TextRect(dc, say, L"복구 완료", C_GREEN, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (FxDecorOn()) DrawFxShardsStaggered(dc, cx, (r.top + r.bottom) / 2, done, 640, 22, slot * 7 + 3, C_GREEN, 5);
    }
}

void DrawGimmickFx(HDC dc) {
    int kind = GimmickFxKind();
    if (kind <= GIMMICK_NONE || kind >= GIMMICK_COUNT) return;
    if (gGame.phase != PHASE_COMBAT) return;
    int a = GimmickFxA(), b = GimmickFxB();
    int t = GimmickFxElapsed(), dur = GimmickFxDuration(kind, b);
    if (dur <= 0) return;
    if (t > dur) t = dur;
    int actionMs = GimmickFxAction(kind, b);
    if (actionMs > dur) actionMs = dur;
    int act = t < actionMs ? t * 1000 / actionMs : 1000;   // 동작 진행도, 먼저 끝난다
    int impactAt = GimmickFxImpactAt(kind, b);
    int sinceImpact = impactAt > 0 ? t - impactAt : -1;     // 임팩트 이후 경과 (음수 = 아직)
    COLORREF fam = FxColor();
    RECT screen = MakeRect(0, 68, BASE_WIDTH, BASE_HEIGHT);
    int global = 0;    // 1 = 화면 전체를 쓰는 발동이라 도장을 생략한다
    int boss = BossCardIndex();
    RECT card = EnemyRect(boss < 0 ? 0 : boss);

    // 임팩트 순간의 공용 처리: 짧은 섬광과 가장자리 글로우. 세기는 접근성 모드를 따른다.
    if (sinceImpact >= 0 && sinceImpact < 300 && FxDecorOn()) {
        // 파쇄는 제 임팩트 프레임(색 반전 → 붉은 반전)을 따로 쓴다. 공용 전면
        // 섬광까지 겹치면 옅은 빛이 170ms 동안 판을 덮어 때린 느낌이 오히려 죽는다.
        if (kind != GIMMICK_BLUE_SCREEN) DrawFxImpact(dc, screen, sinceImpact, 170, fam);
        DrawEdgeGlow(dc, screen, fam, FxScale(1000 - sinceImpact * 1000 / 300), 14);
    }

    switch (kind) {

    case GIMMICK_SIGNATURE:
        DrawLockShutter(dc, a, act, fam, SHUTTER_MESH, L"SIGNATURE REJECTED");
        break;
    case GIMMICK_SEVENTEENTH:
        DrawRestoreRewind(dc, act, fam);
        break;
    case GIMMICK_LAST_WRITE:
        DrawQuarantineSeal(dc, a, -1, act, fam, 1);
        break;

    // ---- C:\ 잠금 : 철문이 내려온다 -----------------------------------------
    case GIMMICK_ACCESS_DENIED: {
        // 예비: 신호가 오는 동안 대상 슬롯이 떨고 보스 카드가 붉게 맥동한다
        if (a >= 0 && a < SLOT_COUNT && act < 520) {
            DrawTremble(dc, SlotRect(a), t, FxScale(2), MixColor(C_BG, C_RED, 40 + act / 12));
            DrawPulseFrame(dc, card, 2 + (t / 90) % 3, 2, MixColor(C_BG, C_RED, 45));
        }
        DrawLockShutter(dc, a, act, fam, SHUTTER_MESH, L"ACCESS DENIED");
        if (a >= 0 && a < SLOT_COUNT && sinceImpact >= 0) {
            RECT r = SlotRect(a);
            DrawFxShardsStaggered(dc, (r.left + r.right) / 2, r.bottom - 6, sinceImpact, 380, 18, a * 7 + 3, C_RED, 9);
            if (sinceImpact < 160) Fill(dc, MakeRect(r.left - 2, r.bottom - 3, r.right + 2, r.bottom + 4), MixColor(C_BG, RGB(255, 210, 200), 80 - sinceImpact * 80 / 160));
        }
        break;
    }
    case GIMMICK_KERNEL_PANIC: {
        if (a >= 0 && a < SLOT_COUNT) {
            RECT r = SlotRect(a);
            int ox = (r.left + r.right) / 2, oy = (r.top + r.bottom) / 2;
            if (act < 520) {
                DrawTremble(dc, r, t, FxScale(3), MixColor(C_BG, C_RED, 45 + act / 10));
                DrawPulseFrame(dc, card, 2 + (t / 70) % 4, 3, MixColor(C_BG, C_RED, 55));
            } else if (FxDecorOn()) {
                // 철문이 내려오는 동안 슬롯에서 화면으로 균열이 뻗는다
                DrawCracks(dc, ox, oy, Lerp(120, 760, EaseOutCubic((act - 520) * 1000 / 480)), a * 11 + 5,
                    C_RED, MixColor(C_BG, C_RED, 45));
            }
            DrawLockShutter(dc, a, act, fam, SHUTTER_CORR, L"KERNEL PANIC");
            if (sinceImpact >= 0) {
                DrawFxShardsStaggered(dc, ox, oy, sinceImpact, 460, 30, a * 11 + 5, C_RED, 7);
                if (FxDecorOn()) DrawFxTear(dc, screen, t, FxScale(sinceImpact < 220 ? 14 - 14 * sinceImpact / 220 : 0), kind);
            }
        }
        break;
    }
    case GIMMICK_BLUE_SCREEN:
        if (b == SHRED_FX_RESTORE) DrawShredRestore(dc, a, t);
        else DrawShredStrike(dc, a, b, t, fam);
        break;

    // ---- D:\ 복원 : 시간이 역행한다 ---------------------------------------
    case GIMMICK_RESTORE_POINT:
    case GIMMICK_TAPE_LOOP: {
        int big = kind == GIMMICK_RESTORE_POINT;
        // 초상이 잔상을 남기며 역방향으로 흔들리고 세로로 살짝 늘어난다
        if (FxDecorOn() && FxSnapshotHeld() && boss >= 0) {
            RECT art = PortraitRect(card);
            int ghost = Lerp(10, 0, act);
            FxSnapshotBlit(dc, art, -ghost, 0, 40);
            FxSnapshotBlit(dc, art, -ghost * 2, 0, 20);
        }
        if (boss >= 0 && big) DrawBossSquash(dc, 1000 - 60 * (act < 500 ? act : 1000 - act) / 500,
                                              1000 + 90 * (act < 500 ? act : 1000 - act) / 500, 0);
        DrawRestoreRewind(dc, act, fam);
        // 되감기 기호가 카드 위에서 깜빡인다
        if ((t / 120) % 2 == 0) TextRect(dc, MakeRect(card.left, card.top + 6, card.right, card.top + 30), L"<<  REWIND", fam, gFontSmall, DT_CENTER | DT_SINGLELINE);
        if (sinceImpact >= 0) DrawFxShardsStaggered(dc, card.left + 12 + (card.right - card.left - 24) * gGame.boss.fxHpAfter / (gGame.enemies[boss < 0 ? 0 : boss].maxHp > 0 ? gGame.enemies[boss < 0 ? 0 : boss].maxHp : 1), card.top + 214, sinceImpact, 340, 14, kind * 3, fam, 8);
        break;
    }
    case GIMMICK_MASTER_BACKUP: {
        if (FxDecorOn() && FxSnapshotHeld()) {
            // 판 전체의 잔상이 좌우로 어긋난 채 남았다가 걷힌다
            int ghost = Lerp(14, 0, EaseOutCubic(Track(t, 0, 1500)));
            FxSnapshotBlit(dc, screen, -ghost, 0, 30);
            FxSnapshotBlit(dc, screen, ghost, 0, 20);
            DrawFxTear(dc, screen, t, FxScale(t < 440 ? 22 - 22 * t / 440 : 0), kind);
        }
        DrawBossSquash(dc, 1000 - 80 * (act < 500 ? act : 1000 - act) / 500,
                           1000 + 140 * (act < 500 ? act : 1000 - act) / 500, sinceImpact >= 0 && sinceImpact < 120 ? 600 : 0);
        DrawRestoreRewind(dc, act, fam);
        DrawFxWave(dc, (card.top + card.bottom) / 2, sinceImpact, 760, fam);
        DrawFxBanner(dc, kind, t, dur, fam);
        global = 1;
        break;
    }

    // ---- E:\ 오프라인 : 연결이 끊긴다 -------------------------------------
    case GIMMICK_AUTOPLAY: {
        if (a < 0 || a >= 3) break;
        RECT r = DieRect(a);
        if (act < 400) DrawTremble(dc, r, t, FxScale(2), MixColor(C_BG, C_RED, 50));
        if (FxDecorOn() && FxSnapshotHeld()) FxSnapshotBlit(dc, r, Lerp(0, 9, act) * ((t / 60) % 2 ? 1 : -1), 0, 45);
        DrawDieRowSplit(dc, a, act, 6, C_RED);
        if (FxDecorOn()) DrawSectorStatic(dc, r, a, NoiseFrameStep(), FxScale(Lerp(300, 900, act)));
        if ((t / 90) % 3 != 0) TextRect(dc, r, L"NO SIGNAL", C_RED, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }
    case GIMMICK_UNSAFE_EJECT: {
        if (a < 0 || a >= 3) break;
        RECT r = DieRect(a);
        if (act < 380) DrawTremble(dc, r, t, FxScale(3), MixColor(C_BG, C_RED, 60));
        // 툭 뽑힌다: 가속 낙하 뒤 바닥에서 튄다
        int fall = Track(act, 380, 1000);
        int drop = Lerp(0, 34, EaseOutBounce(fall));
        DrawDieRowSplit(dc, a, act, 7, C_RED);
        if (FxDecorOn() && FxSnapshotHeld() && fall > 0) FxSnapshotBlit(dc, r, 0, drop / 2, 30);
        RECT moved = MakeRect(r.left, r.top + drop, r.right, r.bottom + drop);
        if (moved.bottom > BASE_HEIGHT) moved.bottom = BASE_HEIGHT;
        Outline(dc, moved, C_RED, 2);
        TextRect(dc, moved, L"EJECTED", C_RED, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (sinceImpact >= 0) DrawFxShardsStaggered(dc, (r.left + r.right) / 2, r.top + 14, sinceImpact, 420, 20, a * 5 + 9, fam, 9);
        break;
    }
    case GIMMICK_NO_MEDIA: {
        if (b == 1) {
            // 인식 턴. 이 기믹에서 유일하게 좋은 소식이므로 연출도 반대다.
            for (int d = 0; d < 3; ++d) DrawPulseFrame(dc, DieRect(d), 2 + act / 250, 3, C_GREEN);
            if (FxDecorOn()) DrawEdgeGlow(dc, screen, C_GREEN, FxScale(Lerp(600, 0, act)), 12);
            TextRect(dc, MakeRect(0, 292, BASE_WIDTH, 330), L"MEDIA DETECTED", MixColor(C_GREEN, RGB(255, 255, 255), (t / 80) % 2 ? 35 : 0),
                gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            break;
        }
        if (a < 0 || a >= 3) break;
        RECT r = DieRect(a);
        DrawDieRowSplit(dc, a, act, 5, C_RED);
        if (FxDecorOn()) DrawSectorStatic(dc, r, a, NoiseFrameStep(), FxScale(Lerp(200, 700, act)));
        TextRect(dc, r, L"NO MEDIA", C_RED, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    }

    // ---- N:\ 경로 : 배선이 바뀐다 (슬롯은 움직이지 않는다) ----------------
    case GIMMICK_PROXY:
    case GIMMICK_ROUTING_LOOP: {
        int loopKind = kind == GIMMICK_ROUTING_LOOP;
        // 예비: 슬롯 넷이 차례로 떨고 옛 배선이 깜빡인다
        if (act < 400) {
            for (int s = 0; s < SLOT_COUNT; ++s)
                if ((t / 60 + s) % 4 == 0) DrawTremble(dc, SlotRect(s), t, FxScale(2), MixColor(C_BG, fam, 55));
            DrawRoutingBus(dc, ROUTE_ORDER, 1000 - act * 1000 / 400, (t / 50) % 2 ? RGB(38, 52, 63) : MixColor(C_BG, fam, 30), 0);
        } else {
            DrawRoutingBus(dc, ROUTE_ORDER_REVERSED, EaseOutCubic((act - 400) * 1000 / 600), fam, 0);
        }
        // 새 배선이 이어질 때 접점마다 불티
        if (sinceImpact >= 0) for (int k = 0; k < SLOT_COUNT; ++k) {
            RECT r = SlotRect(k);
            DrawFxShardsStaggered(dc, (r.left + r.right) / 2, CFX_SLOT_ROUTE_Y, sinceImpact - k * 40, 300, 8, kind * 7 + k, fam, 6);
        }
        // 슬롯 이름이 노이즈로 갈렸다 자리를 잡는다 (ASCII 코드가 아니라 짧은 한글이라 블록 치환)
        for (int s = 0; s < SLOT_COUNT; ++s) {
            RECT r = SlotRect(s);
            if (act < 700 && (Hash3(s, t / 45, 17) % 3u) == 0)
                Fill(dc, MakeRect(r.left + 8, r.top + 8, r.left + 56, r.top + 30), MixColor(C_PANEL, fam, 35));
        }
        DrawOrderScramble(dc, t, actionMs * 40 / 100, actionMs, 1, fam);
        if (loopKind) {
            // 되돌아오는 고리: 패킷이 끝에서 처음으로 돌아가며 궤적을 남긴다
            RECT first = SlotRect(ROUTE_ORDER_REVERSED[0]), last = SlotRect(ROUTE_ORDER_REVERSED[SLOT_COUNT - 1]);
            int lx = (last.left + last.right) / 2, fx = (first.left + first.right) / 2;
            for (int trail = 0; trail < 5; ++trail) {
                int loop = ((t - trail * 30) * 1000 / (dur > 0 ? dur : 1)) % 1000;
                int x = lx + (fx - lx) * loop / 1000;
                Fill(dc, MakeRect(x - 5, CFX_SLOT_ROUTE_Y + 12, x + 5, CFX_SLOT_ROUTE_Y + 18), MixColor(C_BG, fam, 100 - trail * 18));
            }
        }
        break;
    }
    case GIMMICK_TIMEOUT: {
        wchar_t num[16]; wsprintfW(num, L"%d", a < 0 ? 0 : a);
        COLORREF col = b ? C_RED : (a <= 1 ? C_YELLOW : fam);
        // 숫자가 오버슈트로 꽂혔다 자리를 잡는다
        int pop = EaseOutBack(Track(t, 0, 380));
        int grow = Lerp(46, 0, pop);
        RECT box = MakeRect(card.left + 6 - grow / 2, card.top + 62 - grow / 2, card.right - 6 + grow / 2, card.top + 140 + grow / 2);
        int fade = act < 700 ? 1000 : 1000 - (act - 700) * 1000 / 300;
        Fill(dc, box, RGB(6, 9, 14));
        Outline(dc, box, MixColor(C_BG, col, fade / 12), 2);
        TextRect(dc, box, num, MixColor(C_BG, col, fade / 10), gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (b) {
            // 0에 닿았다: 정지 프레임 뒤 붉은 배선 재구성
            if (sinceImpact >= 0 && sinceImpact < 240 && FxDecorOn()) DrawScanlines(dc, screen);
            DrawRoutingBus(dc, ROUTE_ORDER_REVERSED, EaseOutCubic(act), C_RED, 0);
            DrawOrderScramble(dc, t, 350, 1100, 1, C_RED);
            if (sinceImpact >= 0) DrawFxShardsStaggered(dc, (box.left + box.right) / 2, (box.top + box.bottom) / 2, sinceImpact, 400, 22, 41, C_RED, 8);
            global = 1;
        } else global = 1;
        break;
    }

    // ---- R:\ 압력 : 차오르다 터진다 ---------------------------------------
    case GIMMICK_LEAK:
    case GIMMICK_HEAP_OVERFLOW:
    case GIMMICK_OUT_OF_MEMORY: {
        int weight = kind == GIMMICK_OUT_OF_MEMORY ? 3 : kind == GIMMICK_HEAP_OVERFLOW ? 2 : 1;
        // 예비: 초상이 부풀고 카드 테두리가 조여든다
        int swell = act < 600 ? act * 1000 / 600 : 1000;
        DrawBossSquash(dc, 1000 + (60 + 40 * weight) * swell / 1000, 1000 + (30 + 20 * weight) * swell / 1000,
            sinceImpact >= 0 && sinceImpact < 100 ? 500 : 0);
        DrawPressureAlloc(dc, act, fam);
        if (FxDecorOn()) DrawEdgeGlow(dc, screen, fam, FxScale(Lerp(0, 550 + 150 * weight, swell)), 14 + 4 * weight);
        if (sinceImpact >= 0) {
            RECT gauge = MakeRect(card.left + 12, card.top + 222, card.right - 12, card.top + 229);
            DrawFxShardsStaggered(dc, gauge.right, (gauge.top + gauge.bottom) / 2, sinceImpact, 380, 12 + 8 * weight, kind * 5, fam, 7);
        }
        if (kind == GIMMICK_HEAP_OVERFLOW) {
            // 방어선을 뚫고 지나가는 한 줄. 선두에서 불티가 튄다.
            RECT slot = SlotRect(SLOT_DEFEND);
            int y = (slot.top + slot.bottom) / 2;
            // 전투판 폭만 가로지른다. 오른쪽 정보 사이드바는 판독용이라 연출이 밟지 않는다.
            int x = 28 + (COMBAT_MAIN_RIGHT - 48) * EaseOutCubic(act) / 1000;
            Fill(dc, MakeRect(28, y - 2, x, y + 2), C_RED);
            if (x > 12) Fill(dc, MakeRect(x - 12, y - 6, x, y + 6), MixColor(C_RED, RGB(255, 255, 255), 40));
            DrawFxShardsStaggered(dc, x, y, act < 1000 ? 60 : sinceImpact, 260, 10, kind * 3, C_RED, 5);
            if (act >= 700) DrawTremble(dc, slot, t, FxScale(2), C_RED);
        }
        if (kind == GIMMICK_OUT_OF_MEMORY) {
            // 화면이 눌리다가 한계에서 되튄다
            int dark = act < 700 ? 100 * act / 700 : 100 * (1000 - act) / 300;
            int step = dark <= 4 ? 0 : (dark >= 96 ? 1 : 9 - dark * 8 / 100);
            if (step > 0 && FxDecorOn()) for (int y = 68; y < BASE_HEIGHT; y += step) Fill(dc, MakeRect(0, y, BASE_WIDTH, y + 1), RGB(4, 5, 8));
            if (FxDecorOn() && FxSnapshotHeld() && sinceImpact >= 0) FxSnapshotBlit(dc, screen, 0, Lerp(6, 0, Track(sinceImpact, 0, 300)), 25);
            DrawFxBanner(dc, kind, t, dur, fam);
            global = 1;
        }
        break;
    }

    // ---- X:\ 침입 : 격리가 뚫리고 검체가 나온다 ---------------------------
    // 다른 기믹과 달리 a가 주사위가 아니라 새로 나타난 적 카드 번호다.
    // 카드 자체는 이미 그려져 있으므로, 덮었다가 벗겨 내는 순서로 간다.
    case GIMMICK_SANDBOX_BREACH: {
        RECT cell = (a >= 0 && a < 3) ? EnemyRect(a) : card;
        int cx = (cell.left + cell.right) / 2, cy = (cell.top + cell.bottom) / 2;
        RECT inner = MakeRect(cell.left + 5, cell.top + 5, cell.right - 5, cell.bottom - 5);
        if (sinceImpact < 0) {
            // 예비 — 격리막이 조여들고 카드가 떨린다. 안은 아직 헥스 덤프에
            // 덮여 무엇이 버티고 있는지 보이지 않는다.
            int squeeze = impactAt > 0 ? Track(t, 0, impactAt) : 1000;
            DrawSealRing(dc, cell, EaseOutCubic(squeeze), fam);
            DrawTremble(dc, cell, t, FxScale(2 + 4 * squeeze / 1000), MixColor(C_BG, fam, 70));
            Fill(dc, inner, RGB(6, 4, 6));
            DrawHexBlock(dc, inner, MixColor(C_BG, fam, 60 + 40 * squeeze / 1000), a * 17 + 3, GetTickCount(), 9);
            if (squeeze > 600 && FxDecorOn())
                DrawBandGlitch(dc, inner, t, FxScale(2 + 10 * (squeeze - 600) / 400), a * 7 + 5, 7);
        } else {
            // 막이 깨진다. 균열이 카드 밖으로 뻗고 파편이 층을 이뤄 튄다.
            if (FxDecorOn()) {
                int reach = sinceImpact < 400 ? sinceImpact : 400;
                DrawCracks(dc, cx, cy, FxScale(150 + 130 * reach / 400), a * 23 + 9,
                    MixColor(C_BG, RGB(255, 255, 255), 70), fam);
                DrawFxShardsStaggered(dc, cx, cy, sinceImpact, 640, 26, a * 13 + 7, fam, 9);
                DrawFxWave(dc, cy, sinceImpact, 640, fam);
            }
            // 그리고 카드가 드러난다 — 노이즈가 아래로 걷히며 검체가 나온다.
            int reveal = Track(t, impactAt, actionMs);
            int wipe = Lerp(inner.bottom - inner.top, 0, EaseOutCubic(reveal));
            if (wipe > 0) {
                RECT veil = MakeRect(inner.left, inner.bottom - wipe, inner.right, inner.bottom);
                Fill(dc, veil, RGB(6, 4, 6));
                DrawHexBlock(dc, veil, MixColor(C_BG, fam, 85), a * 17 + 3, GetTickCount(), 9);
                Fill(dc, MakeRect(veil.left, veil.top, veil.right, veil.top + 2), fam);
            }
            // 새 카드가 자리를 주장한다. 테두리가 겹겹이 퍼져 나간다.
            DrawPulseFrame(dc, cell, FxScale(reveal >= 1000 ? 5 + (t - actionMs) / 26 : 6), 3, fam);
        }
        break;
    }

    // ---- X:\ 격리 : 데이터가 잠긴다 ---------------------------------------
    case GIMMICK_SAMPLE13: {
        if (a >= 0 && a < 3) {
            RECT r = DieRect(a);
            if (act < 420) DrawTremble(dc, r, t, FxScale(2), MixColor(C_BG, fam, 55));
            DrawSealRing(dc, FaceStripCell(a, b), Track(act, 0, 420), fam);
        }
        DrawQuarantineSeal(dc, a, b, act, fam, 0);
        if (a >= 0 && a < 3 && sinceImpact >= 0) {
            RECT cell = FaceStripCell(a, b);
            DrawFxShardsStaggered(dc, (cell.left + cell.right) / 2, cell.top, sinceImpact, 360, 16, a * 13 + kind, fam, 8);
        }
        break;
    }
    case GIMMICK_ZERO_DAY: {
        // 되돌릴 수 없는 유일한 기믹. 여기서만 화면 전체를 쓴다.
        if (act < 420) {
            if (a >= 0 && a < 3) {
                RECT r = DieRect(a);
                DrawTremble(dc, r, t, FxScale(3), C_RED);
                DrawSealRing(dc, FaceStripCell(a, b), act * 1000 / 420, C_RED);
                DrawHexBlock(dc, MakeRect(r.left + 6, r.top + 26, r.right - 6, r.top + 72), C_RED, a, GetTickCount(), 5);
            }
            DrawQuarantineSeal(dc, a, b, act * 1000 / 420, C_RED, 1);
            if (FxDecorOn()) DrawEdgeGlow(dc, screen, C_RED, FxScale(act * 1000 / 420), 16);
        } else if (act < 520) {
            // 세 번 튀는 화이트아웃
            Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), (act / 34) % 2 == 0 ? RGB(255, 255, 255) : RGB(255, 214, 214));
            global = 1;
        } else if (act < 700) {
            // 가로 와이프가 판을 한 번 훑고 지나가며, 잔상이 뒤에 남는다
            int wipe = 68 + (BASE_HEIGHT - 68) * (act - 520) / 180;
            if (FxDecorOn() && FxSnapshotHeld()) FxSnapshotBlit(dc, MakeRect(0, 68, BASE_WIDTH, wipe), (t / 30) % 2 ? 6 : -6, 0, 30);
            Fill(dc, MakeRect(0, wipe - 6, BASE_WIDTH, wipe), RGB(255, 236, 236));
            DrawFxImpact(dc, MakeRect(0, wipe, BASE_WIDTH, BASE_HEIGHT), act - 520, 180, C_RED);
            global = 1;
        } else if (a >= 0 && a < 3) {
            RECT cell = FaceStripCell(a, b);
            Fill(dc, cell, RGB(8, 8, 10));
            Outline(dc, cell, C_RED, 1);
            RECT r = DieRect(a);
            Outline(dc, r, C_RED, 3);
            TextRect(dc, MakeRect(r.left, r.bottom - 22, r.right, r.bottom - 4), L"EMPTY 0B",
                C_RED, gFontSmall, DT_CENTER | DT_SINGLELINE);
            int after = t - actionMs * 700 / 1000;
            DrawFxShardsStaggered(dc, (r.left + r.right) / 2, r.top + 50, after, 520, 34, 77, C_RED, 6);
            DrawFxWave(dc, (r.top + r.bottom) / 2, after, 420, C_RED);
            if (FxDecorOn()) DrawFxTear(dc, screen, t, FxScale(after < 280 ? 12 - 12 * after / 280 : 0), kind);
        }
        DrawFxBanner(dc, kind, t, dur, fam);
        break;
    }
    default: break;
    }

    if (!global) DrawFxStamp(dc, kind, t, dur, fam);
}

// ---- 프레임 버퍼 캐시 ----------------------------------------------------
// 캔버스(논리 해상도 x 배율)를 프레임마다 새로 만들면 FHD에서는 매 프레임 13MB짜리
// 비트맵을 할당·해제하게 된다. 애니메이션 타이머가 16ms마다 리페인트를 걸면
// 그것만으로 렉이 난다. 크기가 바뀔 때만 다시 만든다.
static HDC gCanvasDc;
static HBITMAP gCanvasBmp, gCanvasOld;
static int gCanvasW, gCanvasH;

// 입력 처리 직전에 마지막 완성 프레임을 붙잡는다. 보상 함수는 곧바로 다음
// 페이즈로 넘어가지만, 이 복사본 덕분에 설치 경로가 끝날 때까지 보상 화면이 남는다.
void CaptureUiFxSnapshot() {
    if (gCanvasDc && gCanvasW > 0 && gCanvasH > 0)
        FxSnapshotCapture(gCanvasDc, gCanvasW, gCanvasH);
}

static HDC AcquireBuffer(HDC screen, HDC* dcSlot, HBITMAP* bmpSlot, HBITMAP* oldSlot, int* wSlot, int* hSlot, int w, int h) {
    if (*dcSlot && (*wSlot != w || *hSlot != h)) {
        SelectObject(*dcSlot, *oldSlot); DeleteObject(*bmpSlot); DeleteDC(*dcSlot);
        *dcSlot = 0; *bmpSlot = 0;
    }
    if (!*dcSlot) {
        *dcSlot = CreateCompatibleDC(screen);
        *bmpSlot = CreateCompatibleBitmap(screen, w, h);
        *oldSlot = (HBITMAP)SelectObject(*dcSlot, *bmpSlot);
        *wSlot = w; *hSlot = h;
    }
    return *dcSlot;
}

static int gPaintLastMs, gPaintMaxMs, gPaintCount;
int PaintLastMs() { return gPaintLastMs; }
int PaintMaxMs() { return gPaintMaxMs; }
int PaintCount() { return gPaintCount; }

static POINT UiFxCenter(const RECT& r) {
    POINT p = {(r.left + r.right) / 2, (r.top + r.bottom) / 2};
    return p;
}

static void DrawUiFxFace(HDC dc, const RECT& r, const Face* face, COLORREF accent) {
    Panel(dc, r, RGB(12, 22, 28), accent);
    wchar_t value[24]; FormatFace(face, value);
    TextRect(dc, MakeRect(r.left + 4, r.top + 7, r.right - 4, r.top + 37),
        value, FaceColor(face), gFontMedium, DT_CENTER | DT_SINGLELINE);
    wchar_t bytes[24]; wsprintfW(bytes, L"%dB", FaceCost(face));
    TextRect(dc, MakeRect(r.left + 4, r.bottom - 23, r.right - 4, r.bottom - 4),
        bytes, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 직접 조작의 마지막 사건 하나를 그린다. 어떤 프레임을 건너뛰어도 t만 같으면
// 같은 그림이 나오며, 게임 상태와 난수에는 손대지 않는다.
void DrawUiInteractionFx(HDC dc) {
    int kind = gUiFx.kind;
    if (kind == UIFX_NONE || gFxLevel == FX_OFF) return;
    int t = UiFxElapsed();

    if (kind == UIFX_DIE_PLACE || kind == UIFX_DIE_MOVE || kind == UIFX_DIE_REMOVE) {
        if (gGame.phase != PHASE_COMBAT || gUiFx.die < 0 || gUiFx.die >= 3) return;
        RECT die = DieRect(gUiFx.die);
        POINT dieTop = {(die.left + die.right) / 2, die.top - 2};
        int removing = kind == UIFX_DIE_REMOVE;
        int slotIndex = removing ? gUiFx.fromSlot : gUiFx.toSlot;
        if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return;
        RECT slot = SlotRect(slotIndex);
        POINT slotBottom = {(slot.left + slot.right) / 2, slot.bottom + 2};
        COLORREF tone = removing ? C_RED : SlotAccent(slotIndex);

        if (kind == UIFX_DIE_MOVE && gUiFx.fromSlot >= 0 && gUiFx.fromSlot < SLOT_COUNT) {
            RECT old = SlotRect(gUiFx.fromSlot);
            int fade = 1000 - Track(t, 0, 100);
            DrawPulseFrame(dc, old, FxScale(1 + 5 * fade / 1000), 2, C_RED);
            POINT cut = {(old.left + old.right) / 2, old.bottom + 2};
            int arm = FxScale(3 + 4 * fade / 1000);
            Fill(dc, MakeRect(cut.x - arm, cut.y - 1, cut.x + arm, cut.y + 1), C_RED);
            Fill(dc, MakeRect(cut.x - 1, cut.y - arm, cut.x + 1, cut.y + arm), C_RED);
        }

        // 이미 차 있던 슬롯에 놓았다면 밀려난 주사위 쪽으로 연결이 되감긴다.
        if (!removing && gUiFx.displacedDie >= 0 && gUiFx.displacedDie < 3) {
            RECT displaced = DieRect(gUiFx.displacedDie);
            POINT displacedTop = {(displaced.left + displaced.right) / 2, displaced.top - 2};
            int retract = EaseOutCubic(Track(t, 0, 105));
            DrawSignalPath(dc, slotBottom, displacedTop, CFX_SLOT_ROUTE_Y, retract, 2, C_RED, 9, 0);
            if (t < 150) DrawPulseFrame(dc, displaced, FxScale(Lerp(7, 1, Track(t, 20, 150))), 2, C_RED);
        }

        int travel = EaseOutCubic(Track(t, 20, 175));
        DrawSignalPath(dc, removing ? slotBottom : dieTop, removing ? dieTop : slotBottom,
            CFX_SLOT_ROUTE_Y, travel, 3, tone, 9, 0);
        DrawInstallFilament(dc, removing ? slotBottom : dieTop, removing ? dieTop : slotBottom,
            t, 280, tone, gUiFx.die * 11 + slotIndex);
        if (t < 100) DrawPulseFrame(dc, removing ? slot : die, FxScale(2 + (100 - t) / 18), 2, tone);
        // E:\ HOT SWAP처럼 재배치와 동시에 굴림이 바뀌면 카드 자체가 한 번 찢긴다.
        if (!removing && gUiFx.valueBefore != gUiFx.valueAfter && t < 170 && FxDecorOn()) {
            DrawBandGlitch(dc, die, t, FxScale(3 + 7 * t / 170), gUiFx.die * 19 + 7, 6);
            TextRect(dc, MakeRect(die.left, die.top - 21, die.right, die.top - 1),
                L"HOT SWAP", C_YELLOW, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
        int land = Track(t, 135, 300);
        if (land > 0) {
            RECT target = removing ? die : slot;
            DrawPulseFrame(dc, target, FxScale(Lerp(8, 1, EaseOutCubic(land))), 3, tone);
            if (FxDecorOn()) {
                POINT p = UiFxCenter(target);
                DrawPixelBurst(dc, p.x, p.y, t - 150, 150, FxScale(8), gUiFx.die * 17 + slotIndex, tone);
            }
        }
        return;
    }

    if (kind == UIFX_REWARD_FACE || kind == UIFX_REWARD_TSR || kind == UIFX_REWARD_REPAIR) {
        // 340ms까지는 보상 화면을 온전히 유지하고, 이후 성긴 주사선만 남겨 다음
        // 화면을 드러낸다. 알파 블렌딩 없이도 CRT 신호가 교체되는 느낌이 난다.
        int keep = t < 340 ? 100 : Lerp(100, 0, Track(t, 340, 520));
        if (FxSnapshotHeld() && keep > 0)
            FxSnapshotBlit(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), 0, 0, keep);

        RECT source = RewardRect(gUiFx.rewardIndex, BASE_WIDTH);
        POINT from = {(source.left + source.right) / 2, source.bottom + 2};
        POINT to;
        COLORREF tone = C_GREEN;
        RECT target = MakeRect(0, 0, 0, 0);
        if (kind == UIFX_REWARD_FACE) {
            target = FaceGridRect(gUiFx.die, gUiFx.face);
            to.x = (target.left + target.right) / 2; to.y = target.top - 2;
            tone = FaceColor(&gUiFx.shownFace);
        } else if (kind == UIFX_REWARD_REPAIR) {
            RECT hp = HeaderHpRect(BASE_WIDTH);
            target = MakeRect(hp.left - 12, 7, hp.left + 122, 44);
            to = UiFxCenter(target); tone = C_GREEN;
        } else {
            RECT capacity = HeaderCapacityRect(BASE_WIDTH);
            target = MakeRect(capacity.left - 5, 7, capacity.left + 145, 44);
            to = UiFxCenter(target); tone = C_BLUE;
        }

        int travel = EaseOutCubic(Track(t, 45, 250));
        DrawSignalPath(dc, from, to, (from.y + to.y) / 2, travel, 4, tone, 10, FxScale(5));
        DrawInstallFilament(dc, from, to, t - 20, 350, tone, gUiFx.rewardIndex * 29 + kind);
        DrawPulseFrame(dc, source, FxScale(Lerp(5, 1, Track(t, 0, 180))), 2, tone);
        int land = Track(t, 220, 500);
        if (land > 0) {
            DrawPulseFrame(dc, target, FxScale(Lerp(10, 1, EaseOutCubic(land))), 3, tone);
            if (FxDecorOn()) DrawPixelBurst(dc, to.x, to.y, t - 220, 260,
                FxScale(kind == UIFX_REWARD_FACE ? 18 : 13), gUiFx.rewardIndex * 29 + kind, tone);
        }
        if (kind == UIFX_REWARD_FACE && t >= 220) {
            int p = EaseOutBack(Track(t, 220, 350));
            POINT c = UiFxCenter(target);
            RECT reveal = MakeRect(Lerp(c.x, target.left, p), Lerp(c.y, target.top, p),
                                   Lerp(c.x, target.right, p), Lerp(c.y, target.bottom, p));
            if (reveal.right > reveal.left + 8 && reveal.bottom > reveal.top + 8)
                DrawUiFxFace(dc, reveal, &gUiFx.shownFace, tone);
        } else if (kind == UIFX_REWARD_REPAIR && t >= 230) {
            wchar_t gain[32]; wsprintfW(gain, L"HP +%d", gUiFx.valueAfter - gUiFx.valueBefore);
            TextRect(dc, target, gain, C_GREEN, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (kind == UIFX_REWARD_TSR && t >= 230) {
            TextRect(dc, target, L"RESIDENT", C_BLUE, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        return;
    }

    if (kind == UIFX_PRUNE_DELETE || kind == UIFX_PRUNE_RESTORE) {
        if (gGame.phase != PHASE_PRUNE || gUiFx.die < 0 || gUiFx.face < 0) return;
        RECT cell = FaceGridRect(gUiFx.die, gUiFx.face);
        POINT c = UiFxCenter(cell);
        int deleting = kind == UIFX_PRUNE_DELETE;
        COLORREF tone = deleting ? C_RED : C_GREEN;
        if (deleting && t < 190) {
            DrawUiFxFace(dc, cell, &gUiFx.shownFace, MixColor(C_LINE, C_RED, 35 + t * 60 / 190));
            if (FxDecorOn()) DrawBandGlitch(dc, cell, t, FxScale(2 + t * 8 / 190), gUiFx.die * 11 + gUiFx.face, 5);
        } else if (!deleting) {
            int p = EaseOutBack(Track(t, 0, 150));
            RECT reveal = MakeRect(Lerp(c.x, cell.left, p), Lerp(c.y, cell.top, p),
                                   Lerp(c.x, cell.right, p), Lerp(c.y, cell.bottom, p));
            if (reveal.right > reveal.left + 8 && reveal.bottom > reveal.top + 8)
                DrawUiFxFace(dc, reveal, &gUiFx.shownFace, C_GREEN);
        }
        DrawPulseFrame(dc, cell, FxScale(Lerp(8, 1, Track(t, 80, 340))), 3, tone);
        if (FxDecorOn()) DrawPixelBurst(dc, c.x, c.y, deleting ? t - 90 : t,
            260, FxScale(14), gUiFx.die * 23 + gUiFx.face, tone);
    }
}

// ---------------------------------------------------------------------------
// 보스 조우 연출
// ---------------------------------------------------------------------------
// 디렉터리 화면은 구역마다 "LOCKED DESTINATION  ...\<BOSS>"를 띄워 놓고도, 정작
// 그 자리에 도착하는 장면이 없었다. 두 번째 일반전의 보상을 고르고 나면 적 카드
// 한 장이 조용히 갈릴 뿐이었다. 여기서 그 경로를 실제로 연다.
//
//   경보   목적지의 마지막 조각이 헥스 잡음에서 실제 코드로 풀린다
//   게이트 그 경로를 막고 있던 철문의 잠금이 풀리고 좌우로 갈라진다
//   강림   열린 틈에서 보스가 앞으로 나와 바닥을 딛는다 (충격파·흔들림)
//   명패   코드·수치·기믹이 박히고, 명패가 위아래로 걷히며 전투판이 열린다
//
// 판은 이미 보스전 상태다 (StartCombat이 먼저 끝나 있다). 연출은 게임 상태를 한
// 글자도 건드리지 않고 지금 판에 있는 값만 읽으므로 어느 시점에 건너뛰어도 결과가
// 같고, 모든 값이 경과 ms의 순수 함수라 마우스 리페인트가 겹쳐도 같은 프레임이 나온다.

static RECT BossGateRect() { return MakeRect(LEGACY_X + 300, 112, LEGACY_X + 820, 588); }
static RECT BossPlateRect() { return MakeRect(118, 596, BASE_WIDTH - 118, 730); }

// 소실점으로 모이는 바닥 격자. 게이트가 화면에 붙은 그림이 아니라 저 안쪽에 서
// 있는 물건으로 읽히게 하는 최소한의 깊이다.
static void DrawBossFloorGrid(HDC dc, int horizon, int strength, COLORREF tone) {
    if (!FxDecorOn() || strength <= 0) return;
    int vx = BASE_WIDTH / 2;
    for (int i = -8; i <= 8; ++i)
        DrawLine(dc, vx + i * 21, horizon, vx + i * 250, BASE_HEIGHT,
            MixColor(C_BG, tone, FxScale(strength * 3 / 10)), 1);
    for (int i = 1; i <= 7; ++i) {
        int y = horizon + (BASE_HEIGHT - horizon) * i * i / 49;
        DrawLine(dc, 0, y, BASE_WIDTH, y, MixColor(C_BG, tone, FxScale(strength * (9 - i) / 18)), 1);
    }
}

// 위험 빗금 띠. enter 0~1000 만큼 한쪽 끝에서 밀려 들어오고, drift만큼 무늬가
// 흐른다. 공장 셔터와 통제선에 붙어 있는 그 무늬다.
static void DrawBossHazard(HDC dc, const RECT& band, int enter, int drift, int fromRight, COLORREF tone) {
    if (enter <= 0) return;
    if (enter > 1000) enter = 1000;
    int w = band.right - band.left, h = band.bottom - band.top;
    int shown = w * enter / 1000;
    RECT lit = fromRight ? MakeRect(band.right - shown, band.top, band.right, band.bottom)
                         : MakeRect(band.left, band.top, band.left + shown, band.bottom);
    int saved = SaveDC(dc);
    IntersectClipRect(dc, lit.left, lit.top, lit.right, lit.bottom);
    Fill(dc, lit, RGB(12, 9, 10));
    for (int x = -h - (drift % 44); x < w + h; x += 44)
        DrawLine(dc, band.left + x, band.bottom, band.left + x + h, band.top, tone, 15);
    RestoreDC(dc, saved);
    Fill(dc, MakeRect(lit.left, band.top - 2, lit.right, band.top), MixColor(C_BG, tone, 55));
    Fill(dc, MakeRect(lit.left, band.bottom, lit.right, band.bottom + 2), MixColor(C_BG, tone, 55));
}

// 어둠의 장막. 가로줄 간격만으로 농도를 낸다 (DrawFxImpact와 같은 수법이다).
// 보스는 이 장막이 걷히면서 그림자에서 걸어 나온다.
static void DrawBossVeil(HDC dc, const RECT& area, int level) {
    if (level <= 0) return;
    if (level >= 1000) { Fill(dc, area, RGB(3, 4, 6)); return; }
    int step = 1 + (1000 - level) / 105;
    for (int y = area.top; y < area.bottom; y += step)
        Fill(dc, MakeRect(area.left, y, area.right, y + 1), RGB(3, 4, 6));
}

// 게이트 안쪽에서 흘러내리는 데이터. 글자 대신 비트 칸을 쓰므로 한 프레임에
// Fill 몇십 번으로 끝나고, 열린 문 너머가 "돌고 있는 장치"로 읽힌다.
static void DrawBossDataRain(HDC dc, const RECT& area, uint32_t tick, int level, COLORREF tone) {
    if (!FxDecorOn() || level <= 0) return;
    int span = area.bottom - area.top, period = span + 140;
    for (int c = 0; c < 14; ++c) {
        uint32_t h = Hash3(c, 31, 7);
        int x = area.left + 16 + c * (area.right - area.left - 32) / 14;
        int head = area.top + (int)((tick / (30u + h % 40u) + h % 500u) % (uint32_t)period) - 70;
        for (int i = 0; i < 9; ++i) {
            int y = head - i * 15;
            if (y < area.top + 2 || y > area.bottom - 6) continue;
            int lit = (Hash3(c, i, (int)(tick / 70)) % 5u) == 0 ? 100 : 58;
            Fill(dc, MakeRect(x, y, x + 9, y + 4), MixColor(RGB(3, 4, 6),
                i == 0 ? RGB(220, 235, 255) : tone, FxScale(level * lit * (12 - i) / 120000)));
        }
    }
}

// 게이트 문짝 한 짝. 어두운 강판에 가로 보강대와 리벳을 두고, 이음매 쪽 모서리에
// 빗금과 밝은 립을 붙인다. 슬롯 셔터(DrawShutter)와 같은 어휘지만 훨씬 무겁다.
static void DrawBossGateLeaf(HDC dc, const RECT& leaf, int seamRight, COLORREF tone) {
    if (leaf.right <= leaf.left) return;
    Fill(dc, leaf, RGB(23, 27, 33));
    for (int y = leaf.top + 24; y < leaf.bottom; y += 58) {
        Fill(dc, MakeRect(leaf.left, y, leaf.right, y + 12), RGB(31, 36, 44));
        Fill(dc, MakeRect(leaf.left, y, leaf.right, y + 2), RGB(54, 62, 73));
        Fill(dc, MakeRect(leaf.left, y + 10, leaf.right, y + 12), RGB(12, 14, 18));
        for (int x = leaf.left + 20; x < leaf.right - 14; x += 54)
            Fill(dc, MakeRect(x, y + 3, x + 6, y + 9), RGB(72, 81, 94));
    }
    // 세로 보강대. 가로줄만 있으면 큰 판이 납작해 보인다.
    for (int i = 1; i < 4; ++i) {
        int x = leaf.left + (leaf.right - leaf.left) * i / 4;
        Fill(dc, MakeRect(x - 5, leaf.top, x + 5, leaf.bottom), RGB(19, 22, 28));
        Fill(dc, MakeRect(x - 5, leaf.top, x - 3, leaf.bottom), RGB(38, 44, 53));
    }
    RECT edge = seamRight ? MakeRect(leaf.right - 18, leaf.top, leaf.right, leaf.bottom)
                          : MakeRect(leaf.left, leaf.top, leaf.left + 18, leaf.bottom);
    Fill(dc, edge, RGB(15, 18, 23));
    int saved = SaveDC(dc);
    IntersectClipRect(dc, edge.left, edge.top, edge.right, edge.bottom);
    for (int y = edge.top - 20; y < edge.bottom + 20; y += 30)
        DrawLine(dc, edge.left, y + 20, edge.right, y, MixColor(C_BG, tone, 44), 8);
    RestoreDC(dc, saved);
    int lip = seamRight ? edge.right - 3 : edge.left;
    Fill(dc, MakeRect(lip, leaf.top, lip + 3, leaf.bottom), MixColor(C_BG, tone, 72));
}

// 두 문짝. open 0~1000 만큼 좌우로 갈라진다. 문짝은 각자의 반쪽 밖으로 나가지
// 않게 잘라 두므로, 열릴수록 틀 뒤로 사라지는 것처럼 보인다.
static void DrawBossGate(HDC dc, const RECT& gate, int open, int shake, int step, COLORREF tone) {
    int mid = (gate.left + gate.right) / 2, half = (gate.right - gate.left) / 2;
    int slide = half * open / 1000;
    for (int side = 0; side < 2; ++side) {
        RECT window = side ? MakeRect(mid, gate.top, gate.right, gate.bottom)
                           : MakeRect(gate.left, gate.top, mid, gate.bottom);
        if (window.left + (side ? slide : 0) >= window.right - (side ? 0 : slide)) continue;
        int jitter = shake > 0 ? (int)(Hash3(side, step, 17) % (uint32_t)(shake * 2 + 1)) - shake : 0;
        int dx = (side ? slide : -slide) + jitter;
        int saved = SaveDC(dc);
        IntersectClipRect(dc, window.left, window.top, window.right, window.bottom);
        DrawBossGateLeaf(dc, MakeRect(window.left + dx, gate.top, window.right + dx, gate.bottom), side == 0, tone);
        RestoreDC(dc, saved);
    }
}

// 게이트 틀. 문짝보다 앞에 서서 열린 구멍의 경계를 잡아 준다.
static void DrawBossGateFrame(HDC dc, const RECT& gate, int glow, COLORREF tone) {
    RECT outer = gate; InflateRect(&outer, 16, 16);
    Fill(dc, MakeRect(outer.left, outer.top, gate.left, outer.bottom), RGB(17, 20, 25));
    Fill(dc, MakeRect(gate.right, outer.top, outer.right, outer.bottom), RGB(17, 20, 25));
    Fill(dc, MakeRect(gate.left, outer.top, gate.right, gate.top), RGB(17, 20, 25));
    Fill(dc, MakeRect(gate.left, gate.bottom, gate.right, outer.bottom), RGB(17, 20, 25));
    Outline(dc, outer, MixColor(C_BG, tone, 26 + glow / 40), 2);
    Outline(dc, gate, MixColor(C_BG, tone, 40 + glow / 25), 2);
    // 네 귀퉁이 브래킷. 틀이 벽에 물려 있다는 표시다.
    for (int i = 0; i < 4; ++i) {
        int x = (i & 1) ? outer.right - 26 : outer.left + 4, y = (i & 2) ? outer.bottom - 10 : outer.top + 4;
        Fill(dc, MakeRect(x, y, x + 22, y + 6), MixColor(C_BG, tone, 34 + glow / 30));
    }
}

// 보스 카드 색을 그대로 쓰는 계열 도장. 명패 아래줄에서 기믹의 정체를 말한다.
static void DrawBossGimmickStamp(HDC dc, const RECT& box, const wchar_t* stamp, int settle, COLORREF fam) {
    Fill(dc, box, RGB(9, 6, 8));
    Outline(dc, box, MixColor(C_BG, fam, 40 + settle * 55 / 1000), 2);
    if (settle >= 1000) {
        TextRect(dc, box, stamp, fam, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    wchar_t garbled[40];
    CorruptCode(stamp, garbled, 40, 917, GetTickCount());
    int keep = lstrlenW(stamp) * settle / 1000;
    for (int i = 0; i < keep && stamp[i] && i < 39; ++i) garbled[i] = stamp[i];
    TextRect(dc, box, garbled, MixColor(fam, C_TEXT, 40), gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawBossIntro(HDC dc, int width, int height) {
    int index = BossCardIndex();
    if (index < 0) return;                       // 보스가 없으면 열 문도 없다
    const EnemyState* enemy = &gGame.enemies[index];
    const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind);
    const BossGimmickInfo* gi = &BOSS_GIMMICK_INFO[gGame.boss.gimmick];
    COLORREF tone = (COLORREF)info->color;
    int driveIndex = gGame.selectedDrive < 0 || gGame.selectedDrive >= DRIVE_COUNT ? 0 : gGame.selectedDrive;
    const DriveInfo* drive = &DRIVE_INFO[driveIndex];
    int floor = gGame.floor < 0 ? 0 : gGame.floor > 2 ? 2 : gGame.floor;

    int t = (int)(GetTickCount() - gBossIntroStart);
    if (t < 0) t = 0;
    if (t > BOSS_INTRO_MS) t = BOSS_INTRO_MS;
    uint32_t tick = GetTickCount();
    int step = (int)(tick / NOISE_CHURN_MS);
    RECT screen = MakeRect(0, 68, width, height);
    RECT gate = BossGateRect();
    int mid = (gate.left + gate.right) / 2;

    // 마지막 구간에서는 그리는 자리 자체가 위아래로 걷힌다. 아래에 이미 그려져
    // 있는 전투판이 가운데부터 드러나므로, 넘어가는 순간이 별도의 그림 없이 난다.
    int wipe = EaseInCubic(Track(t, BOSS_HAND_AT, BOSS_INTRO_MS));
    int midY = (68 + height) / 2;
    int topEdge = 68 + (midY - 68) * (1000 - wipe) / 1000;
    int botEdge = height - (height - midY) * (1000 - wipe) / 1000;
    int savedAll = SaveDC(dc);
    if (!savedAll) return;
    if (wipe > 0) {
        HRGN upper = CreateRectRgn(0, 68, width, topEdge);
        HRGN lower = CreateRectRgn(0, botEdge, width, height);
        CombineRgn(upper, upper, lower, RGN_OR);
        ExtSelectClipRgn(dc, upper, RGN_AND);
        DeleteObject(upper); DeleteObject(lower);
    } else IntersectClipRect(dc, 0, 68, width, height);

    // ---- 무대 -------------------------------------------------------------
    Fill(dc, screen, RGB(4, 6, 9));
    DrawBossFloorGrid(dc, gate.bottom + 8, 22 + 40 * Track(t, 0, 1100) / 1000, tone);

    // ---- 게이트 안쪽 ------------------------------------------------------
    int open = Track(t, BOSS_GATE_AT + 170, BOSS_RISE_AT + 60);
    open = open * open / 1000;                   // 무거운 문은 천천히 떼어져 가속한다
    int glow = Track(t, BOSS_GATE_AT, BOSS_RISE_AT + 300);
    int savedGate = SaveDC(dc);
    IntersectClipRect(dc, gate.left, gate.top, gate.right, gate.bottom);
    Fill(dc, gate, RGB(3, 4, 6));
    for (int i = 10; i >= 1; --i) {              // 안에서 새어 나오는 빛기둥
        int band = (gate.right - gate.left) * i / 20;
        Fill(dc, MakeRect(mid - band, gate.top, mid + band, gate.bottom),
            MixColor(RGB(3, 4, 6), tone, FxScale(glow * (11 - i) / 380)));
    }
    Fill(dc, MakeRect(mid - 2, gate.top, mid + 2, gate.bottom), MixColor(RGB(3, 4, 6), tone, FxScale(glow / 12)));
    DrawBossDataRain(dc, gate, tick, glow, tone);
    // 주사선은 안쪽 벽에만 얹는다. 보스 위에 얹으면 도트 그림이 줄무늬로 갈린다.
    if (FxDecorOn()) DrawScanlines(dc, gate);

    // 보스. 안쪽 깊은 자리에서 앞으로 걸어 나와 바닥을 딛는다.
    int rise = t - BOSS_RISE_AT;
    if (rise > 0) {
        int walk = EaseOutCubic(Track(rise, 0, 520));
        int reach = Lerp(62, 168, walk);
        int cy = Lerp(gate.top + 150, 396, walk);
        int land = t - BOSS_LAND_AT;
        int squash = land >= 0 && land < 300
            ? FxScale(160 * (1000 - EaseOutCubic(Track(land, 0, 300))) / 1000) : 0;
        int bob = land >= 0 ? SinMille(land * 2) * 5 / 1000 : 0;   // 착지 뒤의 느린 숨
        RECT art = MakeRect(mid - reach, cy - reach, mid + reach, cy + reach);
        for (int i = 3; i >= 1; --i)   // 발치에 고이는 빛. 서 있을 바닥이 생긴다.
            Fill(dc, MakeRect(mid - reach * i / 2, art.bottom - 3 - i * 2, mid + reach * i / 2, art.bottom + i * 3),
                MixColor(RGB(3, 4, 6), tone, FxScale(walk * (4 - i) / 90)));
        DrawSpriteArt(dc, art, enemy->kind, 1, land >= 0 && land < 240 ? 1000 - land * 1000 / 240 : 0,
            bob, 0, 1000 + squash, 1000 - squash);
        DrawBossVeil(dc, art, 1000 - EaseOutCubic(Track(rise, 40, 540)));
        if (land >= 0) {
            int foot = art.bottom - 10;
            if (FxDecorOn() && land < 460) {
                DrawPixelBurst(dc, mid - 96, foot, land, 460, FxScale(16), enemy->kind * 5 + 1, tone);
                DrawPixelBurst(dc, mid + 96, foot, land, 460, FxScale(16), enemy->kind * 5 + 2, tone);
            }
            DrawImpactBloom(dc, mid, foot, land, FxScale(13), enemy->kind + 71, tone);
            if (FxDecorOn() && land < 520) {     // 바닥을 따라 퍼지는 충격파 두 겹
                int p = EaseOutCubic(Track(land, 0, 520)), fade = 1000 - p;
                DrawGlowRing(dc, mid, foot, 60 + p * 420 / 1000, 14 + p * 90 / 1000,
                    MixColor(C_BG, tone, FxScale(70 * fade / 1000)), 3);
                DrawGlowRing(dc, mid, foot, 28 + p * 300 / 1000, 7 + p * 62 / 1000,
                    MixColor(C_BG, RGB(255, 255, 255), FxScale(42 * fade / 1000)), 2);
            }
        }
    }
    RestoreDC(dc, savedGate);

    // ---- 문짝과 틀 --------------------------------------------------------
    int shake = t >= BOSS_GATE_AT && t < BOSS_GATE_AT + 170 ? FxScale(3) : 0;
    DrawBossGate(dc, gate, open, shake, (int)(tick / 24), tone);
    if (open > 0 && open < 1000 && FxDecorOn()) {
        // 갈라지는 이음매에서 불티가 튄다
        int slide = (gate.right - gate.left) / 2 * open / 1000;
        for (int i = 0; i < FxScale(5); ++i) {
            int y = gate.top + 40 + (gate.bottom - gate.top - 80) * i / 5;
            DrawPixelBurst(dc, mid - slide, y, (t + i * 90) % 260, 260, FxScale(6), i * 13 + 3, tone);
            DrawPixelBurst(dc, mid + slide, y, (t + i * 90) % 260, 260, FxScale(6), i * 13 + 8, tone);
        }
    }
    DrawBossGateFrame(dc, gate, glow, tone);
    // 잠금이 풀리기 전에는 이음매에 봉인 표시가 붙어 있다.
    if (t < BOSS_GATE_AT + 200) {
        int seal = 1000 - Track(t, BOSS_GATE_AT, BOSS_GATE_AT + 200);
        RECT tag = MakeRect(mid - 108, gate.top + 374, mid + 108, gate.top + 410);
        Fill(dc, tag, RGB(18, 9, 11));
        Outline(dc, tag, MixColor(C_BG, C_RED, 30 + seal * 60 / 1000), 2);
        TextRect(dc, tag, L"SEALED PATH", MixColor(C_BG, C_RED, 30 + seal * 65 / 1000),
            gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (t >= BOSS_GATE_AT && FxDecorOn())
            DrawPixelBurst(dc, mid, gate.top + 392, t - BOSS_GATE_AT, 200, FxScale(16), 611, C_RED);
    }

    // ---- 위험 띠 ----------------------------------------------------------
    int alarm = t >= BOSS_LAND_AT && t < BOSS_LAND_AT + 260 ? 1000 - (t - BOSS_LAND_AT) * 1000 / 260 : 0;
    COLORREF hazard = MixColor(MixColor(C_BG, C_RED, 62), RGB(255, 236, 210), alarm / 12);
    DrawBossHazard(dc, MakeRect(0, 74, width, 102), Track(t, 0, 400), t / 7, 0, hazard);
    DrawBossHazard(dc, MakeRect(0, 734, width, 758), Track(t, 90, 490), t / 7, 1, hazard);

    // ---- 경보 : 잠긴 목적지가 풀린다 ---------------------------------------
    int alertOut = Track(t, BOSS_GATE_AT - 150, BOSS_GATE_AT + 30);
    if (alertOut < 1000) {
        int grow = EaseOutBack(Track(t, 40, 330));
        if (grow > 1150) grow = 1150;
        int reach = 88 * grow / 1000 * (1000 - alertOut) / 1000;
        if (reach > 4) {
            RECT plate = MakeRect(238, 366 - reach, width - 238, 366 + reach);
            Fill(dc, plate, RGB(13, 9, 11));
            Outline(dc, plate, C_RED, 2);
            Fill(dc, MakeRect(plate.left, plate.top, plate.left + 4, plate.bottom), C_RED);
            Fill(dc, MakeRect(plate.right - 4, plate.top, plate.right, plate.bottom), C_RED);
            // 접히는 동안에는 틀만 남긴다. 줄이 반쯤 잘린 채로 남으면 접히는 게 아니라
            // 글자가 깨진 것으로 읽힌다.
            if (reach >= 84) {
                int savedPlate = SaveDC(dc);
                IntersectClipRect(dc, plate.left + 4, plate.top + 1, plate.right - 4, plate.bottom - 1);
                TextRect(dc, MakeRect(plate.left, 288, plate.right, 310),
                    L"LOCKED DESTINATION  ·  잠긴 목적지", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
                wchar_t base[96];
                lstrcpynW(base, drive->paths[floor], 96);
                AppendPathSegment(base, L"");
                TextRect(dc, MakeRect(plate.left, 312, plate.right, 336), base, MixColor(C_BG, tone, 70),
                    gFontMedium, DT_CENTER | DT_SINGLELINE);
                // 판독의 핵심. <BOSS>로 가려져 있던 마지막 조각이 왼쪽부터 확정된다.
                wchar_t code[40];
                int settle = Track(t, 150, BOSS_ALERT_MS - 120);
                if (settle >= 1000) lstrcpynW(code, info->code, 40);
                else {
                    CorruptCode(info->code, code, 40, enemy->kind * 13 + 7, tick);
                    int keep = lstrlenW(info->code) * settle / 1000;
                    for (int i = 0; i < keep && info->code[i] && i < 39; ++i) code[i] = info->code[i];
                }
                TextRect(dc, MakeRect(plate.left, 338, plate.right, 384), code,
                    settle >= 1000 ? tone : MixColor(tone, C_TEXT, 45), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                const wchar_t* mark = L"THREAT SIGNATURE MATCHED";
                DrawGlitchLine(dc, mid - TextWidth(dc, mark, gFontSmall) / 2, 392, mark,
                    MixColor(C_BG, C_RED, 60), C_RED, gFontSmall, 419, tick);
                TextRect(dc, MakeRect(plate.left, 416, plate.right, 440),
                    L"경로 무결성 검사 실패 · 보스 프로세스가 이 경로를 점유하고 있습니다",
                    C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
                RestoreDC(dc, savedPlate);
            }
            if (FxDecorOn() && t < 420) DrawPulseFrame(dc, plate, FxScale(2 + 16 * (420 - t) / 420), 3,
                MixColor(C_BG, C_RED, 55));
        }
    }

    // ---- 사건마다 한 번씩 튀는 노이즈 --------------------------------------
    int burst = t < 240 ? 900 - t * 640 / 240 : 0;
    if (t >= BOSS_GATE_AT && t < BOSS_GATE_AT + 170) burst = 480 - (t - BOSS_GATE_AT) * 480 / 170;
    if (t >= BOSS_LAND_AT && t < BOSS_LAND_AT + 220) burst = 430 - (t - BOSS_LAND_AT) * 430 / 220;
    if (burst > 0) DrawScreenStatic(dc, screen, step, FxScale(burst));
    // 착지 섬광. 0ms부터 재생하면 한 프레임이 통째로 하얘져 정작 보스가 안 보인다.
    // 이미 걷히기 시작한 지점부터 재생해 가로줄 3분의 1만 때리고 빠진다.
    if (FxDecorOn() && t >= BOSS_LAND_AT && t - BOSS_LAND_AT < 80)
        DrawFxImpact(dc, screen, t - BOSS_LAND_AT + 140, 230, tone);
    if (t >= BOSS_LAND_AT && t - BOSS_LAND_AT < 340)
        DrawEdgeGlow(dc, screen, tone, FxScale(1000 - (t - BOSS_LAND_AT) * 1000 / 340), 18);

    // ---- 명패 : 코드·수치·기믹 --------------------------------------------
    int nameIn = Track(t, BOSS_NAME_AT, BOSS_NAME_AT + 260);
    if (nameIn > 0) {
        RECT plate = BossPlateRect();
        // 왼쪽에서 밀려 들어온다. EaseOutBack의 되돌아오기는 명패 폭이 커서 화면
        // 밖까지 지나쳐 버리므로, 감속만 쓰고 도착의 충격은 밴드 글리치가 맡는다.
        int slideIn = Lerp(-(plate.right - plate.left) - 40, 0, EaseOutCubic(nameIn));
        OffsetRect(&plate, slideIn, 0);
        Panel(dc, plate, RGB(11, 16, 22), tone);
        Fill(dc, MakeRect(plate.left, plate.top, plate.left + 6, plate.bottom), tone);
        int savedPlate = SaveDC(dc);
        IntersectClipRect(dc, plate.left, plate.top, plate.right, plate.bottom);
        int left = plate.left + 26, right = plate.right - 26;
        wchar_t b[192];
        wsprintfW(b, L"%d층 보스 프로세스  ·  %s%s", floor + 1, drive->letter, drive->label);
        Text(dc, left, plate.top + 12, b, C_DIM, gFontSmall);
        // 위협도는 층수 그대로다. 이 볼륨에서 몇 번째 문을 여는지가 곧 세기다.
        wchar_t threat[40] = L"THREAT  ";
        for (int i = 0; i < 3; ++i) lstrcatW(threat, i <= floor ? L"■" : L"□");
        TextRect(dc, MakeRect(left, plate.top + 12, right, plate.top + 34), threat,
            floor >= 2 ? C_RED : C_YELLOW, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        Text(dc, left, plate.top + 34, info->code, tone, gFontLarge);
        Text(dc, left + TextWidth(dc, info->code, gFontLarge) + 16, plate.top + 46, info->name, C_TEXT, gFontSmall);
        wsprintfW(b, L"체력 %d  ·  피해 %d  ·  방어 %d", enemy->maxHp,
            info->damage + info->damageGrowth * floor, info->guard + info->guardGrowth * floor);
        TextRect(dc, MakeRect(left, plate.top + 40, right, plate.top + 70), b, C_TEXT, gFontMedium, DT_RIGHT | DT_SINGLELINE);
        Fill(dc, MakeRect(left, plate.top + 78, plate.left + 26 + (right - left) * Track(t, BOSS_NAME_AT + 120, BOSS_NAME_AT + 420) / 1000, plate.top + 79), MixColor(C_BG, tone, 55));
        if (gGame.boss.gimmick != GIMMICK_NONE) {
            int stampAt = Track(t, BOSS_NAME_AT + 200, BOSS_NAME_AT + 520);
            RECT stamp = MakeRect(left, plate.top + 88, left + 206, plate.top + 118);
            DrawBossGimmickStamp(dc, stamp, gi->stamp, stampAt, tone);
            // 기믹 이름과 규칙은 따로 찍는다. "%s — %s" 같은 조립 서식을 번역표에 넣으면
            // 다른 화면의 두 칸짜리 줄까지 같이 걸린다.
            int nameX = stamp.right + 16;
            Text(dc, nameX, plate.top + 86, gi->name, C_YELLOW, gFontSmall);
            int ruleX = nameX + TextWidth(dc, gi->name, gFontSmall) + 12;
            Text(dc, ruleX, plate.top + 86, L"·", C_DIM, gFontSmall);
            TextRect(dc, MakeRect(ruleX + 20, plate.top + 86, right, plate.top + 108),
                gi->rule, C_TEXT, gFontSmall, DT_LEFT | DT_SINGLELINE);
            wsprintfW(b, L"대응  %s", gi->counter);
            TextRect(dc, MakeRect(stamp.right + 16, plate.top + 106, right, plate.top + 126), b,
                C_DIM, gFontSmall, DT_LEFT | DT_SINGLELINE);
            if (stampAt > 0 && stampAt < 1000 && FxDecorOn())
                DrawPulseFrame(dc, stamp, FxScale(2 + 14 * (1000 - stampAt) / 1000), 2, MixColor(C_BG, tone, 60));
        } else {
            TextRect(dc, MakeRect(left, plate.top + 92, right, plate.top + 118),
                L"기믹 없는 구형 보스 프로세스입니다 · 수치만으로 밀어붙입니다",
                C_DIM, gFontSmall, DT_LEFT | DT_SINGLELINE);
        }
        RestoreDC(dc, savedPlate);
        if (nameIn < 1000 && FxDecorOn())
            DrawBandGlitch(dc, plate, t, FxScale(9 * (1000 - nameIn) / 1000), enemy->kind + 5, 7);
    }
    // 건너뛰기 안내는 무대 밖 구석에 둔다. 가운데에 두면 보스의 발치를 가린다.
    if (t >= 520 && t < BOSS_HAND_AT)
        TextRect(dc, MakeRect(width - 340, 108, width - 24, 130), L"클릭이나 키로 바로 넘기기",
            C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);

    RestoreDC(dc, savedAll);
    // 걷히는 경계에는 밝은 줄 한 쌍이 남는다. 잘린 자리가 아니라 열리는 자리다.
    if (wipe > 0 && wipe < 1000) {
        Fill(dc, MakeRect(0, topEdge - 2, width, topEdge + 1), MixColor(C_BG, tone, 78));
        Fill(dc, MakeRect(0, botEdge - 1, width, botEdge + 2), MixColor(C_BG, tone, 78));
    }
}

void PaintGame(HWND window) {
    LARGE_INTEGER qpcFreq, qpcStart; QueryPerformanceFrequency(&qpcFreq); QueryPerformanceCounter(&qpcStart);
    SyncLastGasp();
    SyncIdleAnimation();
    SyncCombatFx();
    PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint); RECT client; GetClientRect(window, &client);
    int clientWidth = client.right, clientHeight = client.bottom;
    if (clientWidth <= 0 || clientHeight <= 0) { EndPaint(window, &paint); return; }

    // 1단계: 논리 좌표계는 BASE_WIDTH x BASE_HEIGHT 그대로 두고, 실제 비트맵만
    // 화면에 실릴 크기로 만든다. 매핑 모드가 좌표를 대신 늘려 주므로 기존 좌표
    // 계산과 히트 판정은 한 줄도 바꾸지 않는다. 확대가 사라져 글자와 선이 또렷해진다.
    float scale; int offsetX, offsetY; ComputeCanvasTransform(clientWidth, clientHeight, &scale, &offsetX, &offsetY);
    if (scale > RENDER_SCALE_MAX) scale = RENDER_SCALE_MAX;
    int deviceW = (int)(BASE_WIDTH * scale), deviceH = (int)(BASE_HEIGHT * scale);
    if (deviceW < 1) deviceW = 1;
    if (deviceH < 1) deviceH = 1;
    HDC canvas = AcquireBuffer(dc, &gCanvasDc, &gCanvasBmp, &gCanvasOld, &gCanvasW, &gCanvasH, deviceW, deviceH);
    SetMapMode(canvas, MM_ANISOTROPIC);
    SetWindowExtEx(canvas, BASE_WIDTH, BASE_HEIGHT, 0);
    SetViewportExtEx(canvas, deviceW, deviceH, 0);
    RECT canvasRect = MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT);
    Fill(canvas, canvasRect, C_BG); DrawHeader(canvas, BASE_WIDTH);
    if (gDeathActive) DrawDeathScene(canvas, BASE_WIDTH, BASE_HEIGHT, DeathElapsed());
    else if (gTurnTraceActive || gCombatClearActive) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_TITLE) DrawTitle(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_STORY) DrawStory(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_DRIVE_SELECT) DrawDriveSelect(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_DIRECTORY) DrawDirectorySelect(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_COMBAT) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_REWARD) DrawReward(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_PRUNE) DrawPrune(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_ENDING_CHOICE) DrawEndingChoice(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_GAMEOVER) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 0);
    else if (gGame.phase == PHASE_VICTORY || gGame.phase == PHASE_CHAPTER_CLEAR) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 1);
    // 사망 화면은 사망 연출의 마지막 프레임이 그대로 이어지므로 도착 효과를 얹지 않는다.
    if (!gTurnTraceActive && !gDeathActive && !gCombatClearActive && !gDescentActive && !gDirEnterActive && !gBossIntroActive
        && gGame.phase != PHASE_GAMEOVER)
        DrawSceneArrival(canvas, C_GREEN, SceneArrivalMajor());
    if (gTurnTraceActive) DrawTurnCalculation(canvas);
    else if (gDescentActive) DrawDescent(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gDirEnterActive) DrawDirectoryEnter(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gCombatClearActive) DrawCombatClear(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gBossIntroActive) DrawBossIntro(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gDeckOpen) DrawDeck(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gSettingsOpen) DrawSettings(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGuideOpen) DrawGuide(canvas, BASE_WIDTH, BASE_HEIGHT);

    // 새 게임 삽입 연출도 같은 스냅샷을 쓴다. 첫 프레임에 붙잡히는 판이 곧
    // 디스크 라벨에 실릴 "누르기 직전의 화면"이다.
    if (gBootActive) {
        if (!FxSnapshotHeld()) FxSnapshotCapture(canvas, deviceW, deviceH);
        DrawBootInsert(canvas, BASE_WIDTH, BASE_HEIGHT, deviceW, deviceH);
    }

    if (!gGuideOpen && !gSettingsOpen && !gDeckOpen && !gTurnTraceActive && !gDescentActive && !gCombatClearActive && !gBootActive && !gBossIntroActive) {
        // 연출이 시작되는 첫 프레임의 판을 붙잡아 둔다. 잔상·트레일이 여기서 나온다.
        if (GimmickFxKind() > 0 && !FxSnapshotHeld()) FxSnapshotCapture(canvas, deviceW, deviceH);
        DrawGimmickFx(canvas);
    }
    if (GimmickFxKind() <= 0 && !gBootActive && !UiFxSnapshotActive() && FxSnapshotHeld()) FxSnapshotRelease();
    DrawUiInteractionFx(canvas);

    // 위독 노이즈는 어떤 화면이든 마지막에 한 번만 얹으므로 각 화면은 이 연출을 모른다.
    // 살아 있는 동안에는 가장자리 띠에만 머물고, 체력이 0이 되면 띠는 걷히고 사망
    // 연출이 글자 단위로 이어받는다 (화면 전체를 잡음으로 덮지 않는다).
    // 삽입 연출이 도는 동안에는 아직 지난 판의 상태가 남아 있다. 그 위독 노이즈를
    // 새 게임 화면 위에 얹으면 방금 버린 런의 흔적이 따라 들어온다.
    // 세기는 시각의 함수라 부를 때마다 값이 달라진다. 예전에는 한 프레임 안에서
    // AmbientNoiseLevel을 세 번 불러 띠와 테두리가 서로 다른 순간을 그렸다.
    // 파열이 들어오면 그 차이가 눈에 보이므로 이제 한 번만 읽어 돌려 쓴다.
    int edge = gBootActive || gDeathActive ? 0 : AmbientNoiseLevel();
    if (edge > 0) {
        int surge = AmbientNoiseSurge();
        DrawCriticalStatic(canvas, canvasRect, GetTickCount(), edge, AmbientNoiseBand(), surge);
        // 파열 순간에만 띠 안의 가로 줄이 옆으로 밀린다. 덮는 것이 아니라 이미
        // 그려진 화면이 어긋나므로 "신호가 끊겼다"가 한눈에 읽힌다. 잘린 윗선에는
        // 잉걸이 한 줄 남아 어디서 끊겼는지가 보인다 - 이 한 줄이 없으면
        // 밀린 자리가 그냥 어긋난 그림으로 보이고 사건으로 읽히지 않는다.
        int slipY, slipH, slipShift, slipSkew;
        for (int i = 0; i < AMBIENT_SLIP_MAX; ++i)
            if (AmbientSlip(i, &slipY, &slipH, &slipShift, &slipSkew)) {
                DrawSignalSlip(canvas, canvasRect, slipY, slipH, slipShift, slipSkew, RGB(24, 7, 9));
                Fill(canvas, MakeRect(0, slipY, BASE_WIDTH, slipY + 1), RGB(146, 54, 42));
            }
    }
    int hitFlash = gBootActive ? 0 : PlayerHitFlash();
    if (hitFlash > 0) DrawEdgeGlow(canvas, canvasRect, PlayerHitBlocked() ? C_BLUE : C_RED, hitFlash, 12);
    else if (edge > 0) DrawEdgeGlow(canvas, canvasRect, C_RED, edge, 8 + AmbientNoisePulse() * 7 / 1000);

    // 관리자 터미널은 연출을 포함해 무엇보다 위에 온다.
    // 진행도를 못 쓰고 있다는 사실은 어느 화면에서도 보여야 한다. 쓰기 권한이
    // 없는 폴더에서 돌리는 동안 정상 저장으로 믿고 계속 두면 안 된다.
    if (gCampaignCorrupt || gSaveFailed || gSettingsSaveFailed) {
        RECT warn = MakeRect(BASE_WIDTH / 2 - 390, 2, BASE_WIDTH / 2 + 390, 22);
        Panel(canvas, warn, RGB(48, 12, 12), C_RED);
        const wchar_t* warning = gCampaignCorrupt
            ? L"세이브 검증 실패 · 원본 AROGUE.SAV는 보호 중입니다 · 설정의 진행도 초기화로 새로 시작할 수 있습니다"
            : gSettingsSaveFailed
                ? L"설정을 저장하지 못했습니다 · AROGUE.CFG를 쓸 수 있는지 확인하십시오"
                : L"진행 데이터를 저장하지 못했습니다 · 실행 폴더에 쓸 수 있는지 확인하십시오";
        TextRect(canvas, warn, warning, C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (gTermOpen) DrawTerminal(canvas, BASE_WIDTH, BASE_HEIGHT);

    // 2단계: 캔버스를 화면에 직접 올린다. 예전에는 창 크기 합성 버퍼에 배경을 깔고
    // 캔버스를 얹은 뒤 그 버퍼를 다시 화면에 복사했다 - 배경 채우기와 확대 사이가
    // 노출돼 깜빡인다는 이유였다. 그런데 캔버스가 덮는 자리에는 배경을 칠할 필요가
    // 없다. 캔버스 바깥의 띠 네 개만 따로 채우면 겹치는 픽셀이 없어 깜빡일 자리가
    // 없고, 창 크기 복사 한 번(FHD에서 3.4M 픽셀)이 통째로 사라진다.
    float outScale; int outX, outY; ComputeCanvasTransform(clientWidth, clientHeight, &outScale, &outX, &outY);
    int scaledWidth = (int)(BASE_WIDTH * outScale), scaledHeight = (int)(BASE_HEIGHT * outScale);
    int shakeX = (int)(ScreenShakeX() * outScale), shakeY = (int)(ScreenShakeY() * outScale);
    int left = outX + shakeX, top = outY + shakeY, right = left + scaledWidth, bottom = top + scaledHeight;
    if (top > 0) Fill(dc, MakeRect(0, 0, clientWidth, top), C_BG);
    if (bottom < clientHeight) Fill(dc, MakeRect(0, bottom, clientWidth, clientHeight), C_BG);
    if (left > 0) Fill(dc, MakeRect(0, top, left, bottom), C_BG);
    if (right < clientWidth) Fill(dc, MakeRect(right, top, clientWidth, bottom), C_BG);
    // 캔버스를 장치 좌표로 되돌려 픽셀 대 픽셀로 옮긴다. 화면과 크기가 같으면
    // 확대가 일어나지 않고, 상한에 걸린 경우에만 남은 몫을 늘린다.
    SetMapMode(canvas, MM_TEXT);
    if (deviceW == scaledWidth && deviceH == scaledHeight)
        BitBlt(dc, left, top, scaledWidth, scaledHeight, canvas, 0, 0, SRCCOPY);
    else {
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, 0);
        StretchBlt(dc, left, top, scaledWidth, scaledHeight, canvas, 0, 0, deviceW, deviceH, SRCCOPY);
    }

    // 버퍼는 다음 프레임이 그대로 쓴다. 지우지 않는다.
    EndPaint(window, &paint);
    LARGE_INTEGER qpcEnd; QueryPerformanceCounter(&qpcEnd);
    gPaintLastMs = (int)((qpcEnd.QuadPart - qpcStart.QuadPart) * 1000 / qpcFreq.QuadPart);
    if (gPaintLastMs > gPaintMaxMs) gPaintMaxMs = gPaintLastMs;
    ++gPaintCount;
}
