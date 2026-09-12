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

    const wchar_t* sign = L"RECOVERY SYSTEM  /  1.44 MB  /  BUILD 17";
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

static void DrawDriveSelectionExit(HDC dc, int width, int height, int elapsed) {
    Fill(dc, MakeRect(0, 68, width, height), C_BG);
    DrawDriveSelect(dc, width, height);
    int chosen = gDescentChoiceIndex;
    if (chosen < 0 || chosen >= gGame.driveChoiceCount) return;
    const DriveInfo* drive = &DRIVE_INFO[gGame.driveChoices[chosen]];
    for (int i = 0; i < gGame.driveChoiceCount; ++i)
        DrawSelectionCardExit(dc, DriveCardRect(i), i == chosen, elapsed, DESCENT_LOCK_MS,
                              (COLORREF)drive->color, L"VOLUME LOCKED");

    int commit = Track(elapsed, DESCENT_LOCK_MS * 2 / 3, DESCENT_LOCK_MS);
    if (commit > 0) {
        int y = Lerp(68, height, EaseInCubic(commit));
        if (FxDecorOn()) {
            RECT card = DriveCardRect(chosen);
            DrawSectorStatic(dc, MakeRect(card.left, card.top, card.right, card.bottom), gGame.selectedDrive + 31,
                             elapsed / NOISE_CHURN_MS, FxScale(120 + 320 * commit / 1000));
            Fill(dc, MakeRect(0, y - 1, width, y + 1), MixColor(C_BG, (COLORREF)drive->color, FxScale(50)));
        }
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
        DrawSelectionCardExit(dc, DirectoryChoiceRect(i), i == chosen, elapsed, DIR_SELECT_LOCK_MS,
                              accent, L"PATH LOCKED");

    RECT selected = DirectoryChoiceRect(chosen);
    POINT from = {(selected.left + selected.right) / 2, selected.bottom + 2};
    POINT to = {width / 2, height - 48};
    int route = EaseOutCubic(Track(elapsed, 90, DIR_SELECT_LOCK_MS));
    DrawSignalPath(dc, from, to, height - 96, route, 4, accent, 13, 1);
    if (elapsed > DIR_SELECT_LOCK_MS * 2 / 3)
        DrawBandGlitch(dc, selected, elapsed, FxScale(7), gDirEnterKind + 43, 11);
    int dissolve = Track(elapsed, DIR_SELECT_LOCK_MS - 100, DIR_SELECT_LOCK_MS);
    if (dissolve > 0)
        DrawScreenStatic(dc, MakeRect(0, 68, width, height), elapsed / NOISE_CHURN_MS,
                         820 * dissolve / 1000);
    Fill(dc, MakeRect(0, 102, width, 134), C_BG);
    TextRect(dc, MakeRect(0, 102, width, 134), L"ROUTE ACCEPTED  ·  RESOLVING DIRECTORY HANDLE",
             accent, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 디렉터리 진입 연출. 값이 전부 경과 시간의 함수라 리페인트와 겹쳐도 안전하다.
#define DIR_BRANCH_MS 320   // 갈래를 보여 주고 나서 경로 타이핑이 시작된다

static void DrawDirectoryEnter(HDC dc, int width, int height) {
    const DirectoryNodeInfo* info = DirectoryNodeInfoOrNull(gDirEnterKind);
    if (!info) return;
    int totalElapsed = (int)(GetTickCount() - gDirEnterStart);
    if (totalElapsed < 0) totalElapsed = 0; if (totalElapsed > DIR_ENTER_MS) totalElapsed = DIR_ENTER_MS;
    if (totalElapsed < DIR_SELECT_LOCK_MS) {
        DrawDirectorySelectionExit(dc, width, height, totalElapsed);
        return;
    }
    int elapsed = totalElapsed - DIR_SELECT_LOCK_MS;
    int enterMs = DIR_ENTER_MS - DIR_SELECT_LOCK_MS;
    Fill(dc, MakeRect(0, 68, width, height), RGB(6, 9, 13));
    RECT panel = MakeRect(200, 206, width - 200, height - 206);
    Panel(dc, panel, C_PANEL, (COLORREF)info->color);
    Text(dc, panel.left + 24, panel.top + 16, L"디렉터리 진입", C_GREEN, gFontMedium);

    // 어느 갈래를 골랐는지 먼저 보여 준다. 고르지 않은 쪽은 어두워지고,
    // 작은 패킷이 고른 경로로 건너간 뒤에야 경로가 타이핑되기 시작한다.
    wchar_t base[96];
    FormatCurrentDirectory(&gGame, base, 96);
    // 선택은 이미 확정된 뒤라 현재 경로에 고른 조각이 들어 있다. 갈래를 보여
    // 주는 줄에는 그 조각을 떼어 낸 부모 경로를 적어야 트리가 성립한다.
    wchar_t parent[96];
    lstrcpynW(parent, base, 96);
    for (int i = lstrlenW(parent) - 1; i > 0; --i)
        if (parent[i] == L'\\') { parent[i + 1] = 0; break; }
    Text(dc, panel.left + 24, panel.top + 48, parent, C_DIM, gFontSmall);
    int count = gGame.directory.choiceCount;
    if (count > DIRECTORY_CHOICE_COUNT) count = DIRECTORY_CHOICE_COUNT;
    int rowTop = panel.top + 72;
    for (int i = 0; i < count; ++i) {
        int kind = gGame.directory.choices[i].kind;
        const DirectoryNodeInfo* branch = DirectoryNodeInfoOrNull(kind);
        if (!branch) continue;
        int chosen = kind == gDirEnterKind;
        RECT row = MakeRect(panel.left + 44, rowTop + i * 30, panel.left + 300, rowTop + i * 30 + 26);
        COLORREF tone = chosen ? (COLORREF)branch->color : RGB(52, 62, 70);
        Text(dc, panel.left + 24, row.top + 4, i + 1 == count ? L"└" : L"├", tone, gFontSmall);
        TextRect(dc, row, branch->name, tone, gFontSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (!chosen) continue;
        Outline(dc, MakeRect(row.left - 6, row.top - 2, row.right, row.bottom + 2), tone, 1);
        // 패킷이 고른 갈래를 따라 들어갔다가 도착과 함께 사라진다. 도착한 뒤에도
        // 남아 있으면 아무 데도 가지 않는 점 하나가 화면에 붙어 있게 된다.
        if (elapsed >= DIR_BRANCH_MS) continue;
        int travel = elapsed * 1000 / DIR_BRANCH_MS;
        int x = row.left - 24 + (row.right + 16 - (row.left - 24)) * travel / 1000;
        Fill(dc, MakeRect(x - 6, (row.top + row.bottom) / 2 - 3, x + 6, (row.top + row.bottom) / 2 + 3), tone);
    }

    wchar_t here[96];
    lstrcpynW(here, base, 96);
    int length = lstrlenW(here);
    int typing = elapsed - DIR_BRANCH_MS;
    int typed = typing <= 0 ? 0 : typing * length / (enterMs * 2 / 5);
    if (typed > length) typed = length;
    wchar_t typedText[104] = L"> ";
    lstrcpynW(typedText + 2, here, typed + 1);
    if (typed < length && ((elapsed / 200) & 1)) lstrcatW(typedText, L"_");
    TextRect(dc, MakeRect(panel.left + 24, panel.top + 142, panel.right - 24, panel.top + 190),
        typedText, (COLORREF)info->color, gFontLarge, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT band = MakeRect(panel.left + 24, panel.top + 196, panel.right - 24, panel.top + 236);
    Panel(dc, band, RGB(8, 13, 19), C_LINE);
    RECT inner = MakeRect(band.left + 2, band.top + 2, band.right - 2, band.bottom - 2);
    DrawSectorStatic(dc, inner, gDirEnterKind + 3, elapsed / NOISE_CHURN_MS, 200 + 700 - elapsed * 700 / enterMs);
    DrawScanlines(dc, inner);

    // 고른 갈래가 실제 핸들로 연결되는 순간. 카드 잠금 뒤에도 신호의 방향이
    // 이어져 선택 → 경로 → 전투의 관계가 한 동작으로 보인다.
    int chosenRow = gDirEnterChoiceIndex;
    if (chosenRow < 0 || chosenRow >= count) chosenRow = 0;
    POINT routeFrom = {panel.left + 304, rowTop + chosenRow * 30 + 13};
    POINT routeTo = {panel.right - 34, band.top + 20};
    int routeProgress = EaseOutCubic(Track(elapsed, 80, DIR_BRANCH_MS + 170));
    DrawSignalPath(dc, routeFrom, routeTo, panel.top + 132, routeProgress, 5,
                   (COLORREF)info->color, 12, 1);
    int impact = Track(elapsed, DIR_BRANCH_MS, DIR_BRANCH_MS + 170);
    if (impact > 0) {
        DrawPulseFrame(dc, band, FxScale(3 + 14 * (1000 - impact) / 1000), 3, (COLORREF)info->color);
        if (elapsed < DIR_BRANCH_MS + 260)
            DrawPixelBurst(dc, routeTo.x, routeTo.y, elapsed - DIR_BRANCH_MS, 260,
                           FxScale(20), gDirEnterKind * 9 + 5, (COLORREF)info->color);
    }

    int sweep = Track(elapsed, 0, 260);
    if (sweep < 1000) {
        int y = Lerp(panel.top + 2, panel.bottom - 2, EaseOutCubic(sweep));
        DrawScreenStatic(dc, MakeRect(0, 68, width, height), elapsed / NOISE_CHURN_MS,
                         820 * (1000 - sweep) / 1000);
        Fill(dc, MakeRect(panel.left + 2, y - 2, panel.right - 2, y + 2), (COLORREF)info->color);
    }

    wchar_t b[160];
    wsprintfW(b, L"%s  ·  %s", info->effect, info->cost);
    TextRect(dc, MakeRect(panel.left + 24, panel.top + 246, panel.right - 24, panel.bottom - 44), b, C_TEXT, gFontSmall, DT_WORDBREAK);
    TextRect(dc, MakeRect(panel.left + 24, panel.bottom - 38, panel.right - 24, panel.bottom - 16),
        L"잠시 후 전투가 시작됩니다 · 클릭이나 키로 바로 넘기기", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 사각형 보간. 원래는 아래 플로피 삽입 연출 옆에 있었지만, 마운트 패널이 고른
// 카드에서 열려 나오게 되면서 그보다 먼저 필요해져 여기로 올렸다 (사본은 두지 않는다).
static RECT LerpRect(const RECT& a, const RECT& b, int p) {
    return MakeRect(Lerp(a.left, b.left, p), Lerp(a.top, b.top, p),
                    Lerp(a.right, b.right, p), Lerp(a.bottom, b.bottom, p));
}

// 마운트/심층 진입 연출. 모든 값은 경과 시간의 순수 함수라 마우스 이동 리페인트와 겹쳐도 안전하다.
static void DrawDescent(HDC dc, int width, int height) {
    const DriveInfo* drive = &DRIVE_INFO[gGame.selectedDrive < 0 ? 0 : gGame.selectedDrive];
    int totalElapsed = (int)(GetTickCount() - gDescentStart);
    if (totalElapsed < 0) totalElapsed = 0; if (totalElapsed > DESCENT_MS) totalElapsed = DESCENT_MS;
    int mount = gDescentToFloor == 0;
    if (mount && gDescentChoiceIndex >= 0 && totalElapsed < DESCENT_LOCK_MS) {
        DrawDriveSelectionExit(dc, width, height, totalElapsed);
        return;
    }
    int elapsed = totalElapsed - (mount && gDescentChoiceIndex >= 0 ? DESCENT_LOCK_MS : 0);
    int scanMs = DESCENT_MS - (mount && gDescentChoiceIndex >= 0 ? DESCENT_LOCK_MS : 0);
    Fill(dc, MakeRect(0, 68, width, height), RGB(6, 9, 13));
    RECT panel = MakeRect(170, 150, width - 170, height - 150);
    // 잠금이 끝나는 순간 전혀 다른 판이 통째로 튀어나오던 자리. 이제 패널은 방금 잠근
    // 카드 자리에서 열려 나온다. 패널 안의 모든 배치가 panel 하나만 보고 정해지므로
    // 사각형만 키우면 내용이 따라 펼쳐지고, 아직 좁은 동안 밖으로 삐져나오는 부분은
    // 클립으로 잘라 낸다 (좌표를 따로 접는 것보다 손댈 곳이 없다).
    // 층 하강은 열려 나올 카드가 없으니 예전대로 곧장 제 크기로 선다.
    int cardOpen = (mount && gDescentChoiceIndex >= 0 && gDescentChoiceIndex < gGame.driveChoiceCount
                    && FxDecorOn()) ? Track(elapsed, 0, 260) : 1000;
    int panelClip = 0;
    if (cardOpen < 1000) {
        panel = LerpRect(DriveCardRect(gDescentChoiceIndex), panel, EaseOutCubic(cardOpen));
        panelClip = SaveDC(dc);
        if (panelClip) IntersectClipRect(dc, panel.left, panel.top, panel.right, panel.bottom);
    }
    Panel(dc, panel, C_PANEL, (COLORREF)drive->color);
    Text(dc, panel.left + 26, panel.top + 20, mount ? L"볼륨 마운트" : L"심층 탐색", C_GREEN, gFontLarge);
    wchar_t b[160];
    wsprintfW(b, L"%d층 / 3", gDescentToFloor + 1);
    TextRect(dc, MakeRect(panel.right - 160, panel.top + 28, panel.right - 26, panel.top + 54), b, C_DIM, gFontMedium, DT_RIGHT | DT_SINGLELINE);
    if (mount) wsprintfW(b, L"대상 볼륨  %s%s", drive->letter, drive->label);
    else wsprintfW(b, L"현재 경로  %s", drive->paths[gDescentToFloor - 1]);
    Text(dc, panel.left + 26, panel.top + 68, b, C_DIM, gFontSmall);

    // 목표 경로가 한 글자씩 타이핑된다.
    const wchar_t* path = drive->paths[gDescentToFloor];
    int length = lstrlenW(path);
    int typed = elapsed * length / (scanMs * 3 / 5);
    if (typed > length) typed = length;
    wchar_t typedText[64] = L"> ";
    lstrcpynW(typedText + 2, path, typed + 1);
    if (typed < length && ((elapsed / 220) & 1)) lstrcatW(typedText, L"_");
    TextRect(dc, MakeRect(panel.left + 26, panel.top + 98, panel.right - 26, panel.top + 148), typedText, (COLORREF)drive->color, gFontLarge, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 판독 노이즈 밴드: 진행될수록 정적이 걷힌다.
    int noiseLevel = 900 - elapsed * 900 / scanMs;
    RECT band = MakeRect(panel.left + 26, panel.top + 162, panel.right - 26, panel.top + 272);
    Panel(dc, band, RGB(8, 13, 19), C_LINE);
    RECT inner = MakeRect(band.left + 2, band.top + 2, band.right - 2, band.bottom - 2);
    DrawSectorStatic(dc, inner, gGame.selectedDrive + 11, elapsed / NOISE_CHURN_MS, 200 + noiseLevel);
    DrawSectorHex(dc, inner, gGame.selectedDrive + 5, elapsed / NOISE_CHURN_MS, 300 + noiseLevel);
    DrawScanlines(dc, inner);

    // 디스크 트랙과 탐색 헤드. 세 구간으로 나뉘어 오디오의 seek 신호와 같은
    // 시점에 자리를 옮기므로, 소리가 날 때마다 헤드가 다음 트랙에 안착한다.
    RECT track = MakeRect(panel.left + 26, panel.top + 276, panel.right - 26, panel.top + 284);
    Fill(dc, track, RGB(8, 12, 17));
    for (int x = track.left; x < track.right; x += 9)
        Fill(dc, MakeRect(x, track.top, x + 1, track.bottom), RGB(30, 42, 52));
    int seekPhase = elapsed * 3 / scanMs;
    if (seekPhase > 2) seekPhase = 2;
    int within = elapsed * 3 - seekPhase * scanMs;
    int trackSpan = track.right - track.left;
    int from = trackSpan * seekPhase / 3, to = trackSpan * (seekPhase + 1) / 3;
    int headX = track.left + from + (to - from) * EaseOutCubic(Track(within, 0, 660)) / 1000;
    Fill(dc, MakeRect(track.left, track.top + 3, headX, track.top + 5), MixColor(C_BG, (COLORREF)drive->color, 62));
    Fill(dc, MakeRect(headX - 2, track.top - 6, headX + 2, track.bottom + 6), (COLORREF)drive->color);

    RECT barRect = MakeRect(panel.left + 26, panel.top + 288, panel.right - 26, panel.top + 306);
    Bar(dc, barRect, elapsed, scanMs, (COLORREF)drive->color);
    wsprintfW(b, L"%d%%", elapsed * 100 / scanMs);
    TextRect(dc, MakeRect(panel.right - 106, panel.top + 310, panel.right - 26, panel.top + 332), b, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);

    int capacity = EffectiveCapacity(&gGame);
    if (mount) {
        const ModifierInfo* modA = ActiveModifierInfo(gGame.modifierA);
        const ModifierInfo* modB = ActiveModifierInfo(gGame.modifierB);
        wsprintfW(b, L"층 한도 %dB  ·  디스크 손상: %s + %s", capacity, modA ? modA->name : L"-", modB ? modB->name : L"-");
    } else {
        int bonus = capacity - FLOOR_CAPACITY[gGame.floor > 2 ? 2 : gGame.floor];
        wsprintfW(b, L"용량 한도 %dB → %dB  ·  적이 더 강해집니다", FLOOR_CAPACITY[gDescentToFloor - 1] + bonus, capacity);
    }
    Text(dc, panel.left + 26, panel.top + 322, b, C_YELLOW, gFontSmall);
    if (mount) wsprintfW(b, L"볼륨 특성: %s", drive->perkText);
    else lstrcpyW(b, L"감염 코어에 접근하기 위해 더 깊은 섹터로 진입합니다.");
    Text(dc, panel.left + 26, panel.top + 348, b, C_TEXT, gFontSmall);

    // 카드 잠금의 스캔 헤드가 마운트 패널 안으로 이어진다. 초반에는 외곽이
    // 크게 한 번 숨 쉬고, 각 seek 착지점마다 트랙에서 작은 데이터 파편이 튄다.
    int reveal = Track(elapsed, 0, 240);
    if (reveal < 1000) {
        int sweepY = Lerp(panel.top + 2, panel.bottom - 2, EaseOutCubic(reveal));
        if (FxDecorOn()) {
            Fill(dc, MakeRect(panel.left + 2, sweepY - 1, panel.left + 6, sweepY + 2), (COLORREF)drive->color);
            Fill(dc, MakeRect(panel.right - 6, sweepY - 1, panel.right - 2, sweepY + 2), (COLORREF)drive->color);
            DrawPulseFrame(dc, panel, FxScale(2 + 8 * (1000 - reveal) / 1000), 2,
                MixColor(C_BG, (COLORREF)drive->color, FxScale(50 * (1000 - reveal) / 1000)));
        }
    }
    int phaseElapsed = elapsed % (scanMs / 3);
    if (phaseElapsed < 220)
        DrawPixelBurst(dc, headX, (track.top + track.bottom) / 2, phaseElapsed, 220,
                       FxScale(12), gGame.selectedDrive * 17 + seekPhase, (COLORREF)drive->color);
    const wchar_t* hint = gGame.phase == PHASE_PRUNE
        ? L"진입 후 용량 정리가 필요합니다 · 클릭이나 키로 바로 넘기기"
        : L"잠시 후 전투가 시작됩니다 · 클릭이나 키로 바로 넘기기";
    TextRect(dc, MakeRect(panel.left + 26, panel.bottom - 44, panel.right - 26, panel.bottom - 18), hint, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    if (panelClip) RestoreDC(dc, panelClip);
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
#define BOOT_RUSH_SCALE 4400
#define BOOT_RUSH_BACK 962       // 덮치기 직전 아주 잠깐 물러난다 (예비 동작)

// 돌진 배율(천분율). 물러났다가 가속해 들어온다.
static int BootRushScale(int zoom) {
    if (zoom <= 0) return 1000;
    if (zoom < 150) return Lerp(1000, BOOT_RUSH_BACK, zoom * 1000 / 150);
    return Lerp(BOOT_RUSH_BACK, BOOT_RUSH_SCALE, EaseInCubic((zoom - 150) * 1000 / 850));
}

// 슬롯은 캔버스 한가운데(BASE_WIDTH / 2)를 지난다. 디스크도 같은 축으로 내려오므로
// 둘의 중심이 어긋나면 안 된다. 기계는 1120 폭 기준으로 그려져 LEGACY_X만큼 민다.
static RECT BootMonitorRect(int dy) { return MakeRect(LEGACY_X + 368, 96 + dy, LEGACY_X + 752, 390 + dy); }
static RECT BootScreenRect(int dy)  { return MakeRect(LEGACY_X + 392, 120 + dy, LEGACY_X + 728, 366 + dy); }
static RECT BootCaseRect(int dy)    { return MakeRect(LEGACY_X + 356, 424 + dy, LEGACY_X + 764, 604 + dy); }
static RECT BootDriveRect(int dy)   { return MakeRect(LEGACY_X + 396, 448 + dy, LEGACY_X + 724, 540 + dy); }
static RECT BootSlotRect(int dy)    { return MakeRect(LEGACY_X + 412, 466 + dy, LEGACY_X + 700, 506 + dy); }

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
            Text(dc, disk.label.left + 2, disk.label.bottom + 32, L"1.44 MB  ·  2HD", C_DIM, gFontSmall);
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
static void DrawBootFloor(HDC dc, int width, int height, int dy, int glow) {
    int floorY = BOOT_FLOOR_Y + dy;
    if (floorY >= height) return;
    Fill(dc, MakeRect(0, floorY, width, height), RGB(5, 8, 11));
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
    for (int i = -7; i <= 7; ++i) {
        int away = i < 0 ? -i : i;
        int tone = 13 + glow * 9 / 1000 - away;
        if (tone < 3) tone = 3;
        DrawLine(dc, width / 2 + i * 44, floorY, width / 2 + i * 230, height,
                 MixColor(RGB(5, 8, 11), C_GREEN, tone), 1);
    }
    // 가로선. 앞으로 올수록 간격이 벌어져 바닥이 눕는다.
    for (int j = 1; j <= 6; ++j) {
        int p = j * 1000 / 6;
        int y = floorY + (height - floorY) * (p * p / 1000) / 1000;
        DrawLine(dc, 0, y, width, y, MixColor(RGB(5, 8, 11), C_GREEN, 8 + glow * 6 / 1000), 1);
    }
    // 책상 앞모서리와 기계 발치의 빛 웅덩이.
    Fill(dc, MakeRect(0, floorY, width, floorY + 2), MixColor(RGB(5, 8, 11), C_GREEN, 16 + glow * 22 / 1000));
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
    int led = inserted && ((t / 80) & 1);
    Fill(dc, MakeRect(slot.left + 8, slot.bottom + 14, slot.left + 30, slot.bottom + 26), led ? C_GREEN : RGB(26, 40, 36));
    if (led) Outline(dc, MakeRect(slot.left + 5, slot.bottom + 11, slot.left + 33, slot.bottom + 29), MixColor(C_BG, C_GREEN, 40), 1);
    Text(dc, slot.left + 40, slot.bottom + 10, L"A:", led ? C_GREEN : C_DIM, gFontSmall);
}

// 디스크가 물린 뒤의 모니터. 판독이 진행될수록 노이즈가 걷히고 줄이 하나씩 는다.
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
    int seek = Track(t, BOOT_CLUNK_AT, BOOT_SEEK_END);
    Fill(dc, screen, RGB(6, 13, 11));
    DrawSectorStatic(dc, screen, 7, t / NOISE_CHURN_MS, 380 - seek * 330 / 1000);
    int shown = seek * 7 / 1000;
    if (shown > 6) shown = 6;
    for (int i = 0; i < shown; ++i)
        Text(dc, screen.left + 16, screen.top + 16 + i * 24, BOOT_LINES[i], i >= 4 ? C_GREEN : C_TEXT, gFontSmall);
    // 헤드가 트랙을 옮길 때마다 화면이 한 번씩 튄다.
    if (FxDecorOn() && (seek / 140) % 3 == 0) {
        int band = screen.top + (int)(Hash3(seek / 140, 3, 1) % (uint32_t)(screen.bottom - screen.top - 10));
        Fill(dc, MakeRect(screen.left, band, screen.right, band + 3), MixColor(RGB(6, 13, 11), C_GREEN, 30));
    }
    // 메모리 검사는 숫자가 올라가는 동안이 재미다.
    wchar_t line[64];
    wsprintfW(line, L"메모리 %dK  %c", 640 * seek / 1000, SPIN[(t / 80) % 4]);
    Text(dc, screen.left + 16, screen.bottom - 62, line, C_YELLOW, gFontSmall);
    Bar(dc, MakeRect(screen.left + 16, screen.bottom - 34, screen.right - 16, screen.bottom - 18), seek, 1000, C_GREEN);
    DrawScanlines(dc, screen);
}

// 화면을 감아 삼키는 소용돌이. 조각을 점이 아니라 선으로 그으면 어느 쪽으로
// 얼마나 빨리 빨려 드는지가 한눈에 보인다. 펜을 한 번만 만들어 전부 긋는다.
static void DrawBootVortex(HDC dc, int cx, int cy, int suck) {
    HPEN pen = CreatePen(PS_SOLID, 2, MixColor(C_BG, C_GREEN, 40));
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    for (int i = 0; i < 40; ++i) {
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
    for (int i = 0; i < 34; ++i) {
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

    // 기계는 판이 빨려 들어가는 동안 아래에서 올라와 자리를 잡는다.
    int rise = EaseOutCubic(Track(t, BOOT_SUCK_AT + 90, BOOT_FLIP_AT - 60));
    int dy = Lerp(230, 0, rise);
    int glow = inserted ? 240 + Track(t, BOOT_CLUNK_AT, BOOT_SEEK_END) * 760 / 1000 : 0;

    // 마지막 구간에서는 기계 전체가 다가온다. 캔버스 좌표계에 통째로 배율을 걸어
    // 두고 평소대로 그리면, 모니터·본체·바닥이 한 덩어리로 밀려오며 가장자리부터
    // 화면 밖으로 흘러나가고 브라운관만 남는다. 그림 쪽은 아무것도 몰라도 된다.
    int zoom = Track(t, BOOT_SEEK_END, BOOT_INSERT_MS);
    int rush = BootRushScale(zoom);
    RECT screen0 = BootScreenRect(0);
    int rushCx = (screen0.left + screen0.right) / 2;
    int rushCy = (screen0.top + screen0.bottom) / 2;
    // 다가오는 동안 브라운관 한가운데가 캔버스 한가운데로 옮겨 온다. 다 삼킨
    // 순간의 자리가 런의 첫 화면과 맞아야 넘어가는 지점이 튀지 않는다.
    int rushAimY = Lerp(rushCy, height / 2, EaseInCubic(zoom));

    if (rise > 0) {
        int stageSaved = 0;
        if (zoom > 0) {
            stageSaved = SaveDC(dc);
            SetWindowOrgEx(dc, rushCx, rushCy, 0);
            SetViewportOrgEx(dc, rushCx * deviceW / BASE_WIDTH, rushAimY * deviceH / BASE_HEIGHT, 0);
            SetViewportExtEx(dc, deviceW * rush / 1000, deviceH * rush / 1000, 0);
        }
        DrawBootFloor(dc, width, height, dy, glow);
        DrawBootMachine(dc, dy, inserted, t, glow);
        // 다가오는 동안의 화면 속은 배율 밖에서 그린다. 여기서 그리면 잔글씨와
        // 노이즈 칸까지 배율만큼 부풀어 읽을 수 없는 덩어리가 된다.
        if (inserted) {
            if (zoom > 0) Fill(dc, BootScreenRect(dy), RGB(6, 13, 11));
            else DrawBootScreenText(dc, BootScreenRect(dy), t);
        }
        if (stageSaved) RestoreDC(dc, stageSaved);
    }
    RECT screen = BootScreenRect(dy), drive = BootDriveRect(dy), slot = BootSlotRect(dy);

    // 디스크 몸통이 자라는 동안 스냅샷은 그 라벨을 겨누고 줄어든다. 둘이 같은
    // 배율을 보므로 다 줄어든 순간 판은 라벨에 정확히 얹힌다.
    int grow = Track(t, BOOT_SUCK_AT + BOOT_SUCK_MS * 52 / 100, BOOT_FLIP_AT);
    int diskScale = grow <= 0 ? 240 : Lerp(240, 1000, EaseOutBack(grow));
    // 뒤집기는 세로축을 중심으로 한 바퀴 돈다. 가로 배율이 코사인을 따라가고,
    // 코사인이 음수인 동안은 라벨이 없는 뒷면이 보인다.
    int flipTrack = Track(t, BOOT_FLIP_AT, BOOT_FLY_AT);
    int flipAngle = t < BOOT_FLIP_AT ? 0 : EaseOutCubic(flipTrack) * 3600 / 1000;
    int facing = CosMille(flipAngle), back = facing < 0;
    int squeeze = facing < 0 ? -facing : facing;
    if (squeeze < 70) squeeze = 70;
    // 뒤집는 동안만 몸통에 반사광이 지나간다.
    int shine = (t >= BOOT_FLIP_AT && t < BOOT_FLY_AT) ? (flipTrack * 2) % 1000 : -1;

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
        DrawTornValue(dc, MakeRect(0, height / 2 - 170, width, height / 2 - 80), L"A:\\ROGUE", C_GREEN, 0, step, glitch);
    }
    else if (!inserted) {
        SaveDC(dc);
        // 슬롯에 들어간 부분은 기계 앞판 뒤로 사라진다.
        if (t >= BOOT_PUSH_AT) IntersectClipRect(dc, 0, 0, width, slot.top + 5);
        // 내려오는 동안 지나온 자리에 윗모서리만 얇게 남는다. 예전처럼 사각형을
        // 통째로 그리면 모니터를 가로지르는 글리치 띠로 읽혔다.
        if (FxDecorOn() && t >= BOOT_FLY_AT && t < BOOT_PUSH_AT)
            for (int k = 4; k >= 1; --k) {
                int ty = disk.body.top - k * 14;
                if (ty < 74) continue;
                Fill(dc, MakeRect(disk.body.left + k * 7, ty, disk.body.right - k * 7, ty + 2),
                     MixColor(C_BG, C_GREEN, 34 - k * 7));
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
                    FxSnapshotSpin(dc, deviceW, deviceH,
                                   Lerp(width / 2, labelCx, (EaseOutCubic(lead) + lead) / 2),
                                   Lerp(height / 2, labelCy, (EaseOutCubic(lead) + lead) / 2),
                                   gs * squeeze / 1000, gs, EaseInCubic(lead) * 10800 / 1000, 2);
                }
            FxSnapshotSpin(dc, deviceW, deviceH, Lerp(width / 2, labelCx, shrink), Lerp(height / 2, labelCy, shrink),
                           scale * squeeze / 1000, scale, spin * 10800 / 1000, 0);
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
    if (FxDecorOn() && suck > 0 && suck < 1000) DrawBootVortex(dc, labelCx, labelCy, suck);

    // 디스크 한 장이 완성되는 순간의 파열. 고리가 캔버스 밖으로 퍼지고 화면이
    // 한 번 하얗게 뜬다 - 이 연출에서 가장 큰 사건이라 가장 크게 친다.
    int pop = t - (BOOT_FLIP_AT - 150);
    if (FxDecorOn() && pop >= 0 && pop < 380) {
        int p = pop * 1000 / 380;
        for (int i = 0; i < 3; ++i) {
            int rp = p - i * 150;
            if (rp <= 0) continue;
            int r = rp * 620 / 1000;
            DrawGlowRing(dc, labelCx, labelCy, r, r * 3 / 5,
                         MixColor(C_BG, C_GREEN, 74 * (1000 - rp) / 1000), 3);
        }
        DrawPulseFrame(dc, disk.body, 5 + pop / 24, 3, C_GREEN);
        DrawPixelBurst(dc, labelCx, labelCy, pop, 380, 34, 5, C_GREEN);
        // 브라운관이 한 번 크게 튀는 섬광. 판을 통째로 덮으면 그 순간 장면이
        // 사라지므로 주사선 사이로만 밝힌다 - 뒤가 계속 보이면서도 확 튄다.
        if (pop < 150) {
            COLORREF surge = MixColor(RGB(4, 7, 10), C_GREEN, (150 - pop) * 30 / 150);
            for (int y = (pop & 1); y < height; y += 3) Fill(dc, MakeRect(0, y, width, y + 1), surge);
        }
    }

    // 슬롯 입구는 디스크가 다가오는 동안 점점 밝아진다.
    if (t >= BOOT_FLY_AT && !inserted) {
        int mouth = Track(t, BOOT_FLY_AT, BOOT_CLUNK_AT);
        Fill(dc, MakeRect(slot.left + 2, slot.top, slot.right - 2, slot.top + 4), MixColor(C_BG, C_GREEN, 16 + mouth * 74 / 1000));
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
            DrawGlowRing(dc, mouthX, slot.top, r, r * 2 / 5, MixColor(C_BG, C_GREEN, 70 * (1000 - rp) / 1000), 3);
        }
        DrawPulseFrame(dc, drive, 4 + clunk / 18, 3, C_GREEN);
        DrawPixelBurst(dc, mouthX, slot.top, clunk, 300, 30, 9, C_GREEN);
    }

    // 판독 중에는 드라이브에서 모니터로 신호가 올라간다.
    if (inserted && t < BOOT_SEEK_END && FxDecorOn()) {
        POINT from = {slot.right - 24, slot.top - 8}, to = {screen.left + 44, screen.bottom + 10};
        DrawSignalPath(dc, from, to, screen.bottom + 46, Track(t, BOOT_CLUNK_AT + 100, BOOT_SEEK_END), 3, C_GREEN, 12, 0);
    }

    // 상태 줄은 화면 위쪽에 둔다. 아래는 이제 책상과 바닥이 쓴다.
    const wchar_t* caption =
        t < BOOT_SUCK_AT ? L"현재 세션을 봉인합니다" :
        t < BOOT_FLIP_AT ? L"화면을 디스크에 기록하는 중" :
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
        RECT proj;
        proj.left   = rushCx + (screen0.left  - rushCx) * rush / 1000;
        proj.right  = rushCx + (screen0.right - rushCx) * rush / 1000;
        proj.top    = rushAimY + (screen0.top    - rushCy) * rush / 1000;
        proj.bottom = rushAimY + (screen0.bottom - rushCy) * rush / 1000;
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
                Outline(dc, MakeRect(rushCx - halfW, rushAimY - halfH, rushCx + halfW, rushAimY + halfH),
                        MixColor(RGB(6, 13, 11), C_GREEN, fade), 2);
            }
        // 노이즈와 주사선은 캔버스에 걸리는 만큼만 그린다. 이 자리는 곧 캔버스보다
        // 커지므로, 잘라 두지 않으면 보이지도 않을 칸을 계속 세게 된다.
        RECT inner = proj;
        if (inner.left < 0) inner.left = 0;
        if (inner.top < 0) inner.top = 0;
        if (inner.right > width) inner.right = width;
        if (inner.bottom > height) inner.bottom = height;
        if (inner.right > inner.left && inner.bottom > inner.top) {
            Fill(dc, inner, RGB(6, 13, 11));
            DrawSectorStatic(dc, inner, 9, t / NOISE_CHURN_MS, 120 + zoom * 420 / 1000);
            DrawScanlines(dc, inner);
        }
        Outline(dc, proj, MixColor(RGB(6, 13, 11), C_GREEN, 44), 2);
        TextRect(dc, proj, L"A:\\ROGUE", C_GREEN, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (FxDecorOn()) DrawPulseFrame(dc, proj, 6 + zoom / 90, 3, C_GREEN);
        // 빨려 드는 동안 가장자리가 조여든다. 남는 것이 화면뿐이 되도록.
        DrawEdgeGlow(dc, full, RGB(2, 5, 4), 1000, 16 + e * 96 / 1000);
        // 삼켜지는 순간의 섬광. 이 채우기에는 알파가 없어 아무리 옅게 섞어도
        // 판을 통째로 덮는다 - 2%만 섞어도 화면에 남는 색이 하나뿐이 된다.
        // 그래서 달아오르는 구간은 주사선 사이로만 밝히고(뒤가 계속 보인다),
        // 판 전체를 덮는 것은 정말 마지막 순간뿐이다.
        int flash = Track(t, BOOT_INSERT_MS - 170, BOOT_INSERT_MS);
        if (flash > 0) {
            COLORREF surge = MixColor(RGB(6, 13, 11), RGB(224, 255, 244), 16 + flash * 64 / 1000);
            for (int y = (t / 40) & 1; y < height; y += 3) Fill(dc, MakeRect(0, y, width, y + 1), surge);
        }
        int punch = Track(t, BOOT_INSERT_MS - 60, BOOT_INSERT_MS);
        if (punch > 0) Fill(dc, full, MixColor(RGB(6, 13, 11), RGB(224, 255, 244), 20 + punch * 75 / 1000));
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
    const EnemyState* enemy = &gGame.enemies[index]; const EnemyState* action = DisplayEnemyAction(index);
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
