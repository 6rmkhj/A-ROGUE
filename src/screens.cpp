#include <windows.h>
#include "ui.h"
#include "render.h"
#include "audio.h"

// 창 모드 복원 정보는 설정 화면만 쓰므로 여기 둔다.
static int gWindowedScale = 100;
static RECT gWindowedRect;

RECT GuideButtonRect(int width) { return MakeRect(width - 148, 4, width - 18, 23); }
RECT GuideCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT GuidePrevRect(int width, int height) { (void)width; return MakeRect(84, height - 74, 234, height - 40); }
RECT GuideNextRect(int width, int height) { return MakeRect(width - 234, height - 74, width - 84, height - 40); }
RECT SettingsButtonRect(int width) { return MakeRect(width - 148, 25, width - 18, 44); }
RECT SettingsCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT DeckButtonRect(int width) { return MakeRect(width - 148, 46, width - 18, 65); }
RECT DeckCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT ScaleOptionRect(int index) { int left = 84 + index * 130; return MakeRect(left, 260, left + 112, 302); }
// 오른쪽 열. 설정 화면은 왼쪽 364px만 쓰고 나머지가 비어 있었다. 화면 배율 행은
// x=716까지 뻗으므로 겹치지 않게 한 행 아래(전체화면과 같은 높이)에 둔다.
// 판정용 사각형은 홈보다 두껍다. 얇은 선을 정확히 집어야 하면 쓰기 나쁘다.
RECT VolumeSliderRect() { return MakeRect(560, 380, 990, 422); }

RECT VolumeHandleRect(int volume) {
    RECT r = VolumeSliderRect();
    int travel = (r.right - r.left) - VOL_HANDLE_W;
    int left = r.left + travel * volume / 100;
    return MakeRect(left, r.top, left + VOL_HANDLE_W, r.bottom);
}

int VolumeFromX(int x) {
    RECT r = VolumeSliderRect();
    int travel = (r.right - r.left) - VOL_HANDLE_W;
    if (travel < 1) return 0;
    // 손잡이 한가운데를 잡은 것으로 친다. 그래야 커서와 손잡이가 어긋나지 않는다.
    int v = (x - r.left - VOL_HANDLE_W / 2) * 100 / travel;
    return v < 0 ? 0 : v > 100 ? 100 : v;
}
RECT FullscreenToggleRect() { return MakeRect(84, 380, 364, 422); }
// 소리 슬라이더 안내문(y434~458) 아래로 내려온다. 468이면 라벨이 안내문을 덮는다.
RECT BgmToggleRect() { return MakeRect(560, 500, 780, 542); }
RECT RestartButtonRect() { return MakeRect(84, 460, 364, 502); }
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

// BASE_WIDTH x BASE_HEIGHT 캔버스를 percent%로 표시할 창 크기를 계산해 적용한다 (창 모드에서만 의미가 있다).
void ApplyWindowedScale(int percent) {
    gWindowedScale = percent;
    if (gFullscreen) ApplyFullscreen(0);
    RECT desired = {0, 0, BASE_WIDTH * percent / 100, BASE_HEIGHT * percent / 100};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int width = desired.right - desired.left, height = desired.bottom - desired.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
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

    // 소리 크기. 마스터 볼륨 하나로 모든 효과음에 함께 걸린다.
    Text(dc, 560, 348, L"소리 크기", C_YELLOW, gFontMedium);
    {
        int vol = AudioVolume();
        RECT slider = VolumeSliderRect();
        RECT handle = VolumeHandleRect(vol);
        int hover = Inside(slider, gMouse.x, gMouse.y);
        int mid = (slider.top + slider.bottom) / 2;

        // 홈은 얇게, 채워진 만큼만 초록으로. 음소거면 홈만 남는다.
        RECT groove = MakeRect(slider.left, mid - 4, slider.right, mid + 4);
        Panel(dc, groove, C_PANEL_2, C_LINE);
        int filled = (handle.left + handle.right) / 2;
        if (filled > groove.left)
            Fill(dc, MakeRect(groove.left + 1, groove.top + 1, filled, groove.bottom - 1),
                vol == 0 ? C_LINE : C_GREEN);

        // 눈금 25%마다. 값을 대충 맞출 때 기준이 된다.
        for (int t = 0; t <= 4; ++t) {
            int tx = slider.left + VOL_HANDLE_W / 2 + ((slider.right - slider.left - VOL_HANDLE_W) * t / 4);
            Fill(dc, MakeRect(tx, groove.bottom + 3, tx + 1, groove.bottom + 8), C_LINE);
        }

        Panel(dc, handle, hover ? RGB(28, 70, 57) : C_PANEL_2, hover ? C_GREEN : C_LINE);
        Fill(dc, MakeRect(handle.left + 6, handle.top + 10, handle.left + 10, handle.bottom - 10),
            vol == 0 ? C_DIM : C_GREEN);

        wchar_t label[24];
        if (vol == 0) lstrcpyW(label, L"음소거");
        else wsprintfW(label, L"%d%%", vol);
        TextRect(dc, MakeRect(slider.right + 14, slider.top, panel.right - 20, slider.bottom),
            label, vol == 0 ? C_DIM : C_GREEN, gFontMedium, DT_VCENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(560, 434, panel.right - 30, 458),
        L"끌거나 좌우 방향키로 조절합니다.", C_DIM, gFontSmall, DT_SINGLELINE);
    Text(dc, 560, 468, L"배경 음악", C_YELLOW, gFontMedium);
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
    TextRect(dc, MakeRect(84, 644, panel.right - 30, 700),
        gFxLevel == FX_OFF ? L"움직이는 장식을 끕니다. 슬롯 잠금·오프라인 주사위·격리 대상 면·해결 순서·압력 게이지·체력 잔상과 피해 숫자는 그대로 보입니다."
        : gFxLevel == FX_REDUCED ? L"흔들림과 파편을 절반으로 줄이고 전역 글리치를 최소화합니다. 필수 정보는 그대로 보입니다."
        : L"모든 장식 효과를 사용합니다. 슬롯 잠금·격리 대상 면 같은 필수 정보는 어떤 모드에서도 숨기지 않습니다.",
        C_DIM, gFontSmall, DT_WORDBREAK);

    TextRect(dc, MakeRect(84, panel.bottom - 50, panel.right - 30, panel.bottom - 20), L"취소 키로 닫을 수 있습니다.", C_DIM, gFontSmall, DT_SINGLELINE);
}

static void DrawHeader(HDC dc, int width) {
    Fill(dc, MakeRect(0, 0, width, 68), RGB(10, 16, 22)); Fill(dc, MakeRect(0, 67, width, 68), C_GREEN);
    Text(dc, 24, 14, L"A:\\ROGUE", C_GREEN, gFontLarge);
    if (gGame.phase != PHASE_TITLE && gGame.phase != PHASE_DRIVE_SELECT) {
        wchar_t b[128];
        if (gGame.selectedDrive >= 0) {
            wchar_t here[80];
            FormatCurrentDirectory(&gGame, here, 80);
            wsprintfW(b, L"%s  ·  %d층/3  ·  %d구역/3  ·  %d턴", here, gGame.floor + 1, gGame.encounter + 1, gGame.turn);
            Text(dc, 230, 18, b, C_TEXT, gFontSmall);
            const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
            if (difficulty) {
                wsprintfW(b, L"난이도 %s  ·  %s", difficulty->name, difficulty->brief);
                Text(dc, 230, 38, b, (COLORREF)difficulty->color, gFontSmall);
            }
        } else {
            wsprintfW(b, L"%d층/3  ·  %d구역/3  ·  %d턴", gGame.floor + 1, gGame.encounter + 1, gGame.turn);
            Text(dc, 230, 14, b, C_TEXT, gFontMedium);
        }
        int shownHp = PlayerDisplayHp();
        wsprintfW(b, L"체력 %d/%d", shownHp, gGame.playerMaxHp);
        Text(dc, width - 440, 14, b, shownHp <= 10 ? C_RED : C_TEXT, gFontMedium);
        wsprintfW(b, L"용량 %dB / %dB", UsedBytes(&gGame), EffectiveCapacity(&gGame));
        Text(dc, width - 305, 14, b, UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontSmall);
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

// 기믹 상태줄은 두 줄까지 접힌다("발동: 강화 공격! (피해 N+로 예방했어야)"). 게이지는 체력 바 밑의
// 얇은 띠로 붙이고, 그 아래 250~384에 두 줄(40px)을 준다.
RECT EnemyRect(int i) { int left = 28 + i * 218; return MakeRect(left, 94, left + 198, 384); }
static RECT PortraitRect(const RECT& panel) { return MakeRect(panel.left + 31, panel.top + 8, panel.left + 167, panel.top + 132); }
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
RECT EndTurnRect() { return MakeRect(696, 616, 896, 679); }
RECT ReadButtonRect() { return MakeRect(696, 544, 896, 600); }
RECT KeybButtonRect() { return MakeRect(696, 685, 896, 732); }
int DieForSlotUI(int slot) { for (int d = 0; d < 3; ++d) if (gGame.dice[d].assignedSlot == slot) return d; return -1; }

// 설치된 상주 프로그램은 비어 있는 적 슬롯에 세로로 나열한다.
// 현재 전투는 적이 하나라 슬롯 1이 항상 비지만, 다중 적에도 안전하게
// enemyCount 다음 슬롯을 쓴다. 적이 가득 차면 표시만 생략된다.
static void DrawTsrPanel(HDC dc) {
    int count = InstalledTsrCount(&gGame);
    if (count <= 0 || gGame.enemyCount >= 3) return;
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
    switch (gi->family) {
    case FAM_LOCK: {
        int locked = -1, next = -1;
        for (int s = 0; s < SLOT_COUNT; ++s) {
            if (boss->lockedSlot[s] && locked < 0) locked = s;
            if (boss->nextLockedSlot[s] && next < 0) next = s;
        }
        if (locked >= 0 && boss->gimmick == GIMMICK_BLUE_SCREEN) lstrcpynW(out, L"발동: 증폭·연쇄 잠김", size);
        else if (locked >= 0) wsprintfW(out, L"발동: %s 슬롯 잠김", SLOT_NAMES[locked]);
        else if (next >= 0 && boss->gimmick == GIMMICK_BLUE_SCREEN) lstrcpynW(out, L"예고: 다음 턴 증폭·연쇄 잠금", size);
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
#define CFX_LAUNCH_MS      200    // 공격 신호가 대상에 닿기까지
#define CFX_LAUNCH_HOLD_MS 340    // 경로 잔상이 걷히기까지
#define CFX_HIT_FLASH_MS   75
#define CFX_KNOCK_MS       160
#define CFX_DEBRIS_MS      240
#define CFX_GHOST_MS       360
#define CFX_NUMBER_MS      450
#define CFX_BIGHIT_MS      90     // 큰 피해·처치에만 붙는 초상화 밴드 분할
#define CFX_DEFEND_SCAN_MS 80
#define CFX_DEFEND_WAVE_MS 160
#define CFX_DEFEND_TAG_MS  300
#define CFX_KILL_FRAG_MS   180
#define CFX_KILL_CLEAR_MS  280

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
    int peak = 3 + CfxIntensity(gGame.combatFx[index].value) / 2;   // 3 ~ 7px
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
    int ghost = before - (before - after) * t / CFX_GHOST_MS;
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
    return CfxPoint((card.left + card.right) / 2, card.bottom + 2);
}

// 신호가 카드로 들어가는 순간 아래 모서리가 짧게 밝아진다. 경로가 카드 밖에서
// 끊기더라도 "이 카드로 들어갔다"가 남는다.
static void DrawCardEntry(HDC dc, int enemy, int t, int life, COLORREF color) {
    if (t < 0 || t >= life) return;
    RECT card = EnemyRect(enemy);
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
        int t = CombatFxElapsed(i);
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
        int t = CombatFxElapsed(i);
        if (t < 0) continue;

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

        // 큰 타격만 초상화를 가로 띠로 쪼갠다. 작은 피해에는 붙지 않는다.
        if ((fx->flags & CFXF_BIG_HIT) && t < CFX_BIGHIT_MS && FxDecorOn())
            DrawBandGlitch(dc, portrait, t, FxScale(7 - 7 * t / CFX_BIGHIT_MS), i * 13 + enemy, 7);

        if (t < CFX_DEBRIS_MS && FxDecorOn()) {
            int debris = FxScale(4 + CfxIntensity(fx->value));
            COLORREF tone = fx->type == CFX_BURN ? C_YELLOW : (fx->flags & CFXF_BIG_HIT) ? C_RED : C_TEXT;
            DrawPixelBurst(dc, cx, cy + 12, t, CFX_DEBRIS_MS, debris, i * 31 + enemy * 7, tone);
        }

        // 실제 체력 피해량. 방어도가 전부 받아냈으면 그렇게 적는다.
        if (t < CFX_NUMBER_MS) {
            wchar_t number[32];
            COLORREF tone;
            if (fx->flags & CFXF_BLOCKED) { lstrcpyW(number, L"방어도가 막음"); tone = C_BLUE; }
            else {
                const wchar_t* form = fx->type == CFX_BURN ? L"화상 -%d"
                                    : fx->type == CFX_CHAIN ? L"연쇄 -%d" : L"-%d";
                wsprintfW(number, form, fx->value);
                tone = (fx->flags & CFXF_BIG_HIT) ? C_RED : C_YELLOW;
            }
            int rise = t * 30 / CFX_NUMBER_MS;
            int fade = 1000 - t * 1000 / CFX_NUMBER_MS;
            RECT box = MakeRect(card.left + 8, portrait.top + 26 - rise, card.right - 8, portrait.top + 56 - rise);
            TextRect(dc, box, number, MixColor(C_BG, tone, 25 + fade * 75 / 1000),
                (fx->flags & CFXF_BIG_HIT) ? gFontLarge : gFontMedium,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void DrawEnemy(HDC dc, int index) {
    const EnemyState* enemy = &gGame.enemies[index]; const EnemyInfo* info = GetEnemyInfoOrUnknown(enemy->kind); RECT r = EnemyRect(index);
    int selected = index == gGame.targetEnemy && enemy->alive;
    int isBoss = IsBossKind(enemy->kind);
    int hasGimmick = isBoss && gGame.boss.gimmick != GIMMICK_NONE;
    int drop = enemy->alive ? EnemyStrikeDrop(index) : 0, shift = enemy->alive ? EnemyStrikeShift(index) : 0;
    int knock = EnemyFxKnock(index);
    int killT = 0, killFx = EnemyKillFx(index, &killT);
    int collapsing = killFx >= 0 && killT < CFX_KILL_CLEAR_MS;
    // 처치 줄에 아직 닿지 않았으면 살아 있는 카드로 남는다.
    int shownAlive = enemy->alive || collapsing || EnemyDisplayHp(index) > 0;
    RECT portrait = PortraitRect(r);
    Panel(dc, r, shownAlive ? C_PANEL : RGB(18, 18, 20),
        drop > 0 || collapsing ? C_RED : selected ? C_YELLOW : C_LINE);
    // 붕괴 중에는 살아 있던 그림을 그대로 쪼갠다. 죽은 형태로 먼저 바뀌면
    // "부서지는 장면"이 아니라 "이미 끝난 장면"으로 읽힌다.
    DrawPortrait(dc, portrait, enemy->kind, shownAlive, selected, EnemyFxFlash(index),
        (shownAlive ? EnemyBob(index) : 0) + drop + knock, shift);
    if (collapsing) {
        if (killT < CFX_KILL_FRAG_MS) {
            DrawBandGlitch(dc, portrait, killT, 3 + 13 * killT / CFX_KILL_FRAG_MS, index * 7 + 3, 9);
        } else {
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
        int rise = (1000 - pop) * 26 / 1000, center = (r.left + r.right) / 2;
        int tone = 30 + pop * 70 / 1000;
        RECT tag = MakeRect(center - 74, r.top + 108 - rise, center + 74, r.top + 132 - rise);
        COLORREF accent = MixColor(C_PANEL, taken > 0 ? C_RED : C_BLUE, tone);
        Fill(dc, tag, RGB(9, 7, 11));   // 도트 그림 위에서도 읽히도록 바탕을 깐다
        Outline(dc, tag, accent, 1);
        TextRect(dc, tag, hit, accent, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    Text(dc, r.left + 12, r.top + 140, info->code, enemy->alive ? (COLORREF)info->color : C_DIM, gFontMedium);
    wchar_t b[96];
    if (selected && !hasGimmick) lstrcpyW(b, L"▶ 공격 대상");
    else if (hasGimmick) wsprintfW(b, selected ? L"▶ 보스 · %s" : L"보스 기믹: %s", BOSS_GIMMICK_INFO[gGame.boss.gimmick].name);
    else lstrcpyW(b, isBoss ? L"보스 프로세스" : L"적 프로세스");
    Text(dc, r.left + 12, r.top + 165, b, selected ? C_YELLOW : hasGimmick ? (COLORREF)info->color : C_DIM, gFontSmall);
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
        wsprintfW(b, L"의도: %s %d", INTENT_NAMES[enemy->intent], enemy->intentValue);
        Text(dc, r.left + 12, r.top + 231, b, enemy->intent == INTENT_HEAVY || enemy->intent == INTENT_CORRUPT ? C_RED : C_YELLOW, gFontSmall);
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
        } else if (enemy->block > 0 || enemy->burn > 0) {
            wsprintfW(b, L"방어도 %d   화상 %d", enemy->block, enemy->burn);
            Text(dc, r.left + 12, r.top + 250, b, C_DIM, gFontSmall);
        }
    } else {
        Text(dc, r.left + 12, r.top + 231, L"[ 삭제됨 ]", C_DIM, gFontSmall);
        // 붕괴가 끝나면 빈 껍데기에 종료 도장만 남는다.
        if (!collapsing) {
            RECT stamp = MakeRect(r.left + 8, r.top + 54, r.right - 8, r.top + 88);
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
    return slot == SLOT_ATTACK ? C_RED : slot == SLOT_DEFEND ? C_BLUE : C_GREEN;
}

// 주사위 번호 배지. 세 주사위가 같은 값을 내면 값만으로는 구분할 수 없으므로,
// 슬롯과 주사위 카드 양쪽에 같은 기호를 찍어 짝을 드러낸다.
static const wchar_t* const DIE_BADGE[3] = {L"①", L"②", L"③"};

static void DrawSlot(HDC dc, int slot) {
    RECT r = SlotRect(slot); int die = DieForSlotUI(slot); int hover = Inside(r, gMouse.x, gMouse.y);
    int locked = SlotLockedThisTurn(&gGame, slot) && !GimmickLockPending(slot);
    int lockedNext = SlotLockedNextTurn(&gGame, slot);
    if (locked) {
        // 잠긴 슬롯: 배치를 받지 않으며 어둡고 붉게 오버레이한다.
        Panel(dc, r, RGB(38, 16, 18), C_RED);
        Text(dc, r.left + 10, r.top + 9, SLOT_SHORT_NAMES[slot], C_RED, gFontMedium);
        TextRect(dc, MakeRect(r.left + 5, r.top + 42, r.right - 5, r.top + 83), L"잠김", C_RED, gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6), L"ACCESS DENIED", C_RED, gFontSmall, DT_CENTER | DT_SINGLELINE);
        return;
    }
    Panel(dc, r, hover ? RGB(23, 39, 48) : C_PANEL, hover ? C_GREEN : lockedNext ? C_YELLOW : C_LINE);
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
        const Face* face = RolledFace(&gGame, die); wchar_t value[24]; FormatFace(face, value);
        int offline = gGame.dice[die].offline;
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
}

// 여섯 면의 상태 띠. 큰 굴림값과 별개로 어느 면이 격리·삭제 예고 대상인지,
// 어느 면이 이미 잠겼는지를 정지 화면만 보고도 확인할 수 있다.
// 미판독 상태에서는 이번 굴림이 어디인지 표시하지 않는다 (정보 누출 금지).
static void DrawFaceStrip(HDC dc, int index) {
    RECT r = DieRect(index);
    const DieState* die = &gGame.dice[index];
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
        if (f == die->rolledFace && (gRolled || gReadActive) && DieSettled(index))
            Fill(dc, MakeRect(box.left, box.bottom + 2, box.right, box.bottom + 4), C_TEXT);
    }
}

static void DrawDie(HDC dc, int index) {
    RECT r = DieRect(index); const DieState* die = &gGame.dice[index];
    const Face* face = &gGame.dice[index].faces[gGame.dice[index].rolledFace];
    int selected = gGame.selectedDie == index, hover = Inside(r, gMouse.x, gMouse.y);
    int noise = DieNoise(index), flash = DieSettleFlash(index), step = NoiseStep(index);
    RECT cell = MakeRect(r.left + 6, r.top + 25, r.right - 6, r.top + 72);
    RECT statusRect = MakeRect(r.left + 7, r.top + 74, r.right - 7, r.top + 96);

    COLORREF border = selected ? C_YELLOW : hover ? C_BLUE : C_LINE;
    if (noise > 0) border = (step & 1) ? C_RED : RGB(96, 58, 58);
    else if (flash > 0) border = C_GREEN;
    Panel(dc, r, selected ? RGB(42, 36, 18) : C_PANEL, border);
    // 판독 연출의 붉은·초록 테두리가 선택 표시를 덮어 버리므로, 선택은 그 위에
    // 두께 2로 덧그려 어느 상태에서도 사라지지 않게 한다.
    if (selected) Outline(dc, r, C_YELLOW, 2);
    wchar_t b[64]; wsprintfW(b, L"%s 주사위 %d", DIE_BADGE[index], index + 1);
    Text(dc, r.left + 10, r.top + 8, b, selected ? C_YELLOW : C_TEXT, gFontSmall);
    // 배치돼 있으면 어느 슬롯인지를 그 슬롯 색으로 적는다. 선택 표시는 테두리가
    // 이미 하고 있으므로, 자리를 두고 다투게 두지 않는다.
    int placedIn = gGame.dice[index].assignedSlot;
    if (placedIn >= 0 && placedIn < SLOT_COUNT) {
        wchar_t where[32]; wsprintfW(where, L"→ %s", SLOT_SHORT_NAMES[placedIn]);
        TextRect(dc, MakeRect(r.left + 80, r.top + 6, r.right - 10, r.top + 26), where,
            SlotAccent(placedIn), gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else if (selected) TextRect(dc, MakeRect(r.left + 80, r.top + 6, r.right - 10, r.top + 26),
        L"▶ 선택", C_YELLOW, gFontSmall, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

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
    if (face && face->quarantined != QUAR_NONE) { AppendStatus(statuses, L"격리"); ++statusCount; }
    if (die->unstable) { AppendStatus(statuses, L"읽기 오류"); ++statusCount; }
    if (die->disabled) { AppendStatus(statuses, L"조각화"); ++statusCount; }
    if (die->offline) { AppendStatus(statuses, L"오프라인"); ++statusCount; }
    if (gGame.boss.nextOfflineDie == index) { AppendStatus(statuses, L"다음 턴 오프라인"); ++statusCount; }
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
    Text(dc, side.left + 12, side.top + 242, L"실행 순서", ResolveOrderReversed(&gGame) ? C_RED : C_GREEN, gFontSmall);
    if (ResolveOrderReversed(&gGame))
        TextRect(dc, MakeRect(side.left + 12, side.top + 268, side.right - 10, side.top + 320), L"연쇄 > 방어 > 공격 > 증폭 (역전!)", C_RED, gFontSmall, DT_WORDBREAK);
    else if (gGame.boss.nextReversed)
        TextRect(dc, MakeRect(side.left + 12, side.top + 268, side.right - 10, side.top + 320), L"증폭 > 공격 > 방어 > 연쇄\n다음 턴 역전 예고", C_YELLOW, gFontSmall, DT_WORDBREAK);
    else
        TextRect(dc, MakeRect(side.left + 12, side.top + 268, side.right - 10, side.top + 320), L"증폭 > 공격 > 방어 > 연쇄", C_TEXT, gFontSmall, DT_WORDBREAK);
    const DriveLawInfo* law = &DRIVE_LAW_INFO[gGame.selectedDrive >= 0 && gGame.selectedDrive < DRIVE_COUNT ? gGame.selectedDrive : 0];
    Text(dc, side.left + 12, side.top + 340, L"VOLUME LAW", C_GREEN, gFontSmall);
    wchar_t lawState[96];
    if (gGame.selectedDrive == 2) wsprintfW(lawState, L"%s · %s", law->name, gGame.driveRule.hotSwapUsed ? L"USED" : L"READY");
    else lstrcpynW(lawState, law->name, 96);
    TextRect(dc, MakeRect(side.left + 12, side.top + 364, side.right - 10, side.top + 392), lawState, C_YELLOW, gFontSmall, DT_WORDBREAK);
    TextRect(dc, MakeRect(side.left + 12, side.top + 396, side.right - 10, side.top + 436), law->brief, C_TEXT, gFontSmall, DT_WORDBREAK);
    const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.difficulty);
    if (difficulty) {
        Text(dc, side.left + 12, side.top + 448, L"볼륨 난이도", (COLORREF)difficulty->color, gFontSmall);
        wchar_t d[96];
        wsprintfW(d, L"%s · 오염 %d%%\n관통은 방어 절반만", difficulty->name, difficulty->corruptPercent);
        TextRect(dc, MakeRect(side.left + 12, side.top + 470, side.right - 10, side.top + 526), d, C_DIM, gFontSmall, DT_WORDBREAK);
    }
    Text(dc, side.left + 12, side.bottom - 112, L"시스템 기록", C_GREEN, gFontSmall);
    for (int i = 0; i < 3; ++i) TextRect(dc, MakeRect(side.left + 12, side.bottom - 88 + i * 25, side.right - 8, side.bottom - 66 + i * 25), gGame.logs[i], i == 0 ? C_TEXT : C_DIM, gFontSmall, DT_END_ELLIPSIS | DT_SINGLELINE);
}

static void DrawCombat(HDC dc, int width, int height) {
    for (int i = 0; i < gGame.enemyCount; ++i) if (!GimmickSummonPending(i)) DrawEnemy(dc, i);   // 격리막 안의 카드는 연출이 연다
    DrawTsrPanel(dc);
    DrawRoutingState(dc);   // 지금 이어진 해결 순서 (N:\ 계열 보스전에서만)
    DrawCombatFxBack(dc);   // 신호는 슬롯·주사위 아래를 지나간다
    // 판독이 끝난 뒤에만 계산한다. 판독 전에 미리보기를 돌리면 아직 가려 둔 굴림이 새어 나간다.
    if (gRolled && !gReadActive) PreviewTurn(&gGame, &gPreview);
    else ZeroMemory(&gPreview, sizeof(gPreview));
    // 재생 중에는 배치 안내를 지운다. 지금은 조작할 수 없는 줄인 데다,
    // 그 자리가 바로 신호가 슬롯에서 적으로 건너가는 통로다.
    if (gTurnTraceActive) { /* 안내 줄 없음 */ }
    else if (gPreview.valid) {
        // 미리보기가 안내 줄을 덮으므로, 이 줄에만 있던 경고는 뒤에 붙여 그대로 남긴다.
        wchar_t note[120]; note[0] = 0;
        if (ResolveOrderReversed(&gGame)) lstrcatW(note, L"  ·  역전 턴!");
        if (gPreview.uncertain) lstrcatW(note, L"  ·  읽기 오류로 확정 아님");
        wchar_t result[220];
        if (gPreview.uncertain) {
            // 재굴림을 빼고 돌린 예상이라 총합도 그대로는 맞지 않는다. 숫자를 그대로 두면
            // 확정으로 읽히므로 전부 ? 로 가리고, 지금 굴림 기준의 결말만 가능성으로 덧붙인다.
            if (gPreview.playerDies) lstrcatW(note, L"  ·  시스템 정지 가능");
            else if (gPreview.combatEnds) lstrcatW(note, L"  ·  적 삭제 가능");
            wsprintfW(result, L"예상 결과  적 체력 -?  ·  내 체력 -?  ·  획득 방어도 ?%s", note);
        } else if (gPreview.playerDies)
            wsprintfW(result, L"예상 결과  적 체력 -%d  ·  내 체력 -%d  →  시스템 정지%s",
                gPreview.damageDealt, gPreview.damageTaken, note);
        else if (gPreview.combatEnds)
            wsprintfW(result, L"예상 결과  적 체력 -%d  →  적 삭제  ·  획득 방어도 %d%s",
                gPreview.damageDealt, gPreview.blockGained, note);
        else
            wsprintfW(result, L"예상 결과  적 체력 -%d  ·  내 체력 -%d  ·  획득 방어도 %d%s",
                gPreview.damageDealt, gPreview.damageTaken, gPreview.blockGained, note);
        Text(dc, 28, 388, result, gPreview.playerDies ? C_RED : gPreview.uncertain ? C_YELLOW : gPreview.combatEnds ? C_GREEN : C_TEXT, gFontSmall);
    } else if (ResolveOrderReversed(&gGame)) Text(dc, 28, 388, L"① 배치  →  ② 스페이스: 연쇄 > 방어 > 공격 > 증폭 (역전!)  →  ③ 적 행동", C_RED, gFontSmall);
    else Text(dc, 28, 388, L"① 배치  →  ② 스페이스: 증폭 > 공격 > 방어 > 연쇄  →  ③ 적 행동", C_DIM, gFontSmall);
    for (int i = 0; i < SLOT_COUNT; ++i) DrawSlot(dc, i);
    for (int i = 0; i < 3; ++i) { DrawDie(dc, i); DrawFaceStrip(dc, i); }
    DrawCombatFxFront(dc);  // 충격·파편·피해 숫자는 판 위에 얹는다
    if (gTurnTraceActive) return;   // 재생 중에는 조작 영역을 계산 패널이 쓴다
    RECT read = ReadButtonRect(); int readHover = Inside(read, gMouse.x, gMouse.y), canRead = !gRolled && !gReadActive;
    Panel(dc, read, canRead ? (readHover ? RGB(34, 86, 70) : RGB(24, 58, 49)) : C_PANEL, canRead ? C_GREEN : C_LINE);
    TextRect(dc, read, L"판독 [R]", canRead ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT end = EndTurnRect(); int hover = Inside(end, gMouse.x, gMouse.y) && gRolled;
    Panel(dc, end, hover ? RGB(71, 42, 42) : C_PANEL_2, hover ? C_RED : C_LINE);
    TextRect(dc, end, L"실행 [스페이스]", gRolled ? C_RED : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (IsTsrInstalled(&gGame, TSR_KEYB)) {
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
    DrawSidebar(dc, width, height);
}

// 카드에는 사이드바용 긴 설명 대신 한 줄 요약을 쓴다 (카드 폭 제약).
static const wchar_t* const MODIFIER_BRIEF[MODIFIER_COUNT] = {
    L"층 하강 시 무작위 면 1개 영구 손상",
    L"경고된 주사위가 실행 순간 재굴림",
    L"중복 굴림 결과는 뒤쪽이 비활성화",
    L"용량 +60B · 적 체력 +30%",
    L"굴림 합이 짝수면 공격 +2"
};

RECT DriveCardRect(int i) { int left = 56 + i * 344; return MakeRect(left, 140, left + 320, 640); }

static void DrawDriveModifier(HDC dc, const RECT& card, int top, int modifier) {
    Text(dc, card.left + 16, top, MODIFIER_INFO[modifier].name, C_YELLOW, gFontSmall);
    TextRect(dc, MakeRect(card.left + 16, top + 21, card.right - 14, top + 58), MODIFIER_BRIEF[modifier], C_DIM, gFontSmall, DT_WORDBREAK);
}

static void DrawDriveSelect(HDC dc, int width, int height) {
    TextRect(dc, MakeRect(0, 78, width, 102), L"감염된 저장소 감지  →  [현재: 탐색 볼륨 선택]  →  마운트  →  전투", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 98, width, 126), L"탐색할 볼륨을 선택하십시오 · 디스크 손상과 볼륨 특성이 미리 공개됩니다", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    for (int i = 0; i < 3; ++i) {
        RECT r = DriveCardRect(i);
        const DriveInfo* drive = &DRIVE_INFO[gGame.driveChoices[i]];
        int hover = Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, hover ? RGB(24, 37, 46) : C_PANEL, hover ? (COLORREF)drive->color : C_LINE);
        wchar_t b[16]; wsprintfW(b, L"[%d]", i + 1);
        Text(dc, r.left + 12, r.top + 10, b, C_DIM, gFontSmall);
        const DifficultyInfo* difficulty = DifficultyInfoOrNull(gGame.driveDifficulty[i]);
        wchar_t badge[64];
        if (difficulty) {
            wsprintfW(badge, L"난이도 · %s", difficulty->name);
            TextRect(dc, MakeRect(r.left + 56, r.top + 10, r.right - 12, r.top + 30), badge, (COLORREF)difficulty->color, gFontSmall, DT_RIGHT | DT_SINGLELINE);
        }
        RECT letterRect = MakeRect(r.left + 8, r.top + 20, r.right - 8, r.top + 86);
        DrawSectorStatic(dc, letterRect, gGame.driveChoices[i], (int)(GetTickCount() / 260u), 60);
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
    }
    TextRect(dc, MakeRect(0, height - 100, width, height - 70), L"카드마다 서로 다른 난이도가 배정됩니다 · 난이도는 오염(관통) 피해 배율이며, 방어도는 관통을 절반만 막습니다", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
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
        if (close > 430) {
            RECT stamp = MakeRect(card.left + 24, (card.top + card.bottom) / 2 - 16,
                                  card.right - 24, (card.top + card.bottom) / 2 + 16);
            Fill(dc, stamp, RGB(9, 13, 18));
            TextRect(dc, stamp, L"ROUTE DROPPED", C_DIM, gFontSmall,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        return;
    }

    int lock = EaseOutBack(Track(elapsed, 0, duration * 3 / 4));
    int expand = FxScale(4 + 13 * (1000 - (lock > 1000 ? 1000 : lock)) / 1000);
    DrawPulseFrame(dc, card, expand, 4, accent);
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
    if (elapsed > duration / 2)
        DrawPixelBurst(dc, (card.left + card.right) / 2, (card.top + card.bottom) / 2,
                       elapsed - duration / 2, duration / 2, FxScale(24), card.left + 17, accent);
}

static void DrawDriveSelectionExit(HDC dc, int width, int height, int elapsed) {
    Fill(dc, MakeRect(0, 68, width, height), C_BG);
    DrawDriveSelect(dc, width, height);
    int chosen = gDescentChoiceIndex;
    if (chosen < 0 || chosen >= 3) return;
    const DriveInfo* drive = &DRIVE_INFO[gGame.driveChoices[chosen]];
    for (int i = 0; i < 3; ++i)
        DrawSelectionCardExit(dc, DriveCardRect(i), i == chosen, elapsed, DESCENT_LOCK_MS,
                              (COLORREF)drive->color, L"VOLUME LOCKED");

    int commit = Track(elapsed, DESCENT_LOCK_MS * 2 / 3, DESCENT_LOCK_MS);
    if (commit > 0) {
        int y = Lerp(68, height, EaseInCubic(commit));
        DrawSectorStatic(dc, MakeRect(0, 68, width, y), gGame.selectedDrive + 31,
                         elapsed / NOISE_CHURN_MS, 180 + 620 * commit / 1000);
        Fill(dc, MakeRect(0, y - 3, width, y + 2), (COLORREF)drive->color);
    }
    TextRect(dc, MakeRect(0, 92, width, 124), L"SELECTION COMMITTED  ·  MOUNT REQUEST QUEUED",
             (COLORREF)drive->color, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// ---------------------------------------------------------------------------
// 디렉터리 선택 화면
//
// 카드 두 장에 노드명·위험도·정확한 수치·비용·보상 tier를 그대로 적는다.
// 숨기는 것은 아직 판독하지 않은 적 코드뿐이고, 그것도 LOGS가 열어 준다.
// 여기서는 게임 상태를 절대 바꾸지 않는다 (리페인트로 선택지가 다시 뽑히면 안 된다).
// ---------------------------------------------------------------------------

RECT DirectoryChoiceRect(int i) { int left = 120 + i * 460; return MakeRect(left, 150, left + 420, 566); }

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
    COLORREF accent = info ? (COLORREF)info->color : C_LINE;
    Panel(dc, r, hover ? RGB(24, 37, 46) : C_PANEL, hover ? accent : C_LINE);
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

    TextRect(dc, MakeRect(r.left + 12, r.bottom - 32, r.right - 12, r.bottom - 10),
        hover ? L"클릭하여 진입" : L"클릭 또는 숫자 키", hover ? accent : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

static void DrawDirectorySelect(HDC dc, int width, int height) {
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
    Text(dc, 120, 584, L"CURRENT", C_GREEN, gFontSmall);
    Text(dc, 120, 606, here, C_TEXT, gFontMedium);
    Text(dc, 120, 642, L"LOCKED DESTINATION", C_RED, gFontSmall);
    wchar_t destination[128];
    lstrcpynW(destination, drive->paths[gGame.floor > 2 ? 2 : gGame.floor], 128);
    int boss = FloorBossKind(&gGame);
    if (boss >= 0 && DirectoryCodeVisible(boss)) AppendPathSegment(destination, GetEnemyInfoOrUnknown(boss)->code);
    else AppendPathSegment(destination, L"<BOSS>");
    Text(dc, 120, 664, destination, C_DIM, gFontMedium);

    TextRect(dc, MakeRect(0, height - 52, width, height - 28),
        L"[1] / [2] 또는 디렉터리를 클릭하십시오  ·  선택지는 다시 뽑히지 않습니다", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
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
    int headX = track.left + from + (to - from) * within / scanMs;
    Fill(dc, MakeRect(track.left, track.top + 3, headX, track.top + 5), MixColor(C_BG, (COLORREF)drive->color, 62));
    Fill(dc, MakeRect(headX - 2, track.top - 6, headX + 2, track.bottom + 6), (COLORREF)drive->color);

    RECT barRect = MakeRect(panel.left + 26, panel.top + 288, panel.right - 26, panel.top + 306);
    Bar(dc, barRect, elapsed, scanMs, (COLORREF)drive->color);
    wsprintfW(b, L"%d%%", elapsed * 100 / scanMs);
    TextRect(dc, MakeRect(panel.right - 106, panel.top + 310, panel.right - 26, panel.top + 332), b, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);

    int capacity = EffectiveCapacity(&gGame);
    if (mount) wsprintfW(b, L"층 한도 %dB  ·  디스크 손상: %s + %s", capacity, MODIFIER_INFO[gGame.modifierA].name, MODIFIER_INFO[gGame.modifierB].name);
    else {
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
        DrawSectorStatic(dc, MakeRect(0, 68, width, height), gGame.selectedDrive + 31,
                         elapsed / NOISE_CHURN_MS, 800 * (1000 - reveal) / 1000);
        Fill(dc, MakeRect(panel.left + 2, sweepY - 2, panel.right - 2, sweepY + 2), (COLORREF)drive->color);
        DrawPulseFrame(dc, panel, FxScale(5 + 18 * (1000 - reveal) / 1000), 4, (COLORREF)drive->color);
        DrawEdgeGlow(dc, MakeRect(0, 68, width, height), (COLORREF)drive->color,
                     FxScale(520 * (1000 - reveal) / 1000), 12);
    }
    int phaseElapsed = elapsed % (scanMs / 3);
    if (phaseElapsed < 220)
        DrawPixelBurst(dc, headX, (track.top + track.bottom) / 2, phaseElapsed, 220,
                       FxScale(12), gGame.selectedDrive * 17 + seekPhase, (COLORREF)drive->color);
    const wchar_t* hint = gGame.phase == PHASE_PRUNE
        ? L"진입 후 용량 정리가 필요합니다 · 클릭이나 키로 바로 넘기기"
        : L"잠시 후 전투가 시작됩니다 · 클릭이나 키로 바로 넘기기";
    TextRect(dc, MakeRect(panel.left + 26, panel.bottom - 44, panel.right - 26, panel.bottom - 18), hint, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 전투 종료는 곧장 결과 패널로 넘어가지 않는다. 마지막 전투판과 적의 붕괴를
// 먼저 보여 주고, 종료 도장을 찍고, 전장이 어두워진 뒤에야 결과가 올라온다.
// 화면 선택은 PaintGame이 맡는다 (재생 중에는 보상 화면 대신 전투판을 그린다).
static void DrawCombatClear(HDC dc, int width, int height) {
    int elapsed = (int)(GetTickCount() - gCombatClearStart);
    if (elapsed < 0) elapsed = 0;
    if (elapsed > COMBAT_CLEAR_MS) elapsed = COMBAT_CLEAR_MS;
    if (elapsed < 250) return;                     // 0~250ms: 전투판을 그대로 둔다
    if (elapsed < 400) {
        // 250~400ms: 종료 도장은 이미 적 카드에 찍혀 있다. 같은 글자를 한 번 더
        // 얹으면 카드의 코드명과 체력을 가리므로, 그 카드를 짚어 주기만 한다.
        int fallen = 0;
        for (int i = 0; i < gGame.enemyCount; ++i) if (!gGame.enemies[i].alive) fallen = i;
        int pulse = (elapsed - 250) * 100 / 150;
        DrawPulseFrame(dc, EnemyRect(fallen), 2 + pulse / 14, 3, C_RED);
        return;
    }
    // 400~650ms: 전장이 위아래에서 닫히며 잠긴다. 가로줄을 겹쳐 흐리게 만들면
    // 작은 글자가 갈려 렌더링 오류처럼 읽히므로, 면으로 덮어 닫는다.
    int close = elapsed < 650 ? (elapsed - 400) * 1000 / 250 : 1000;
    int band = (height - 68) / 2 * close / 1000;
    if (band > 0) {
        Fill(dc, MakeRect(0, 68, width, 68 + band), RGB(6, 9, 13));
        Fill(dc, MakeRect(0, height - band, width, height), RGB(6, 9, 13));
        COLORREF edge = MixColor(RGB(6, 9, 13), C_GREEN, 45);
        Fill(dc, MakeRect(0, 68 + band, width, 68 + band + 2), edge);
        Fill(dc, MakeRect(0, height - band - 2, width, height - band), edge);
    }
    if (elapsed < 650) return;

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

// 계산 재생은 전투판을 가리지 않는다. 지금 읽히는 한 줄은 주사위 아래의 넓은
// 티커에, 최근 내역과 진행도는 재생 중 어차피 쓸 수 없는 오른쪽 조작 영역에
// 놓는다. 그래서 신호가 떠나는 슬롯과 맞는 적을 동시에 볼 수 있다.
//
// 티커를 적 카드와 슬롯 사이(y 368~404)에 두면 그 띠가 바로 공격 신호가
// 지나가야 하는 통로라 경로가 통째로 가려진다. 아래로 내려 통로를 비운다.
RECT TurnTraceTickerRect() { return MakeRect(28, 712, 690, 750); }
RECT TurnTracePanelRect() { return MakeRect(704, 94, BASE_WIDTH - 22, 738); }

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

    // ---- 최근 내역 (내부 기록 12줄 중 최근 8줄) ----------------------------
    RECT panel = TurnTracePanelRect();
    Panel(dc, panel, RGB(10, 17, 24), C_BLUE);
    Text(dc, panel.left + 14, panel.top + 10, L"턴 계산 과정", C_BLUE, gFontMedium);
    TextRect(dc, MakeRect(panel.left + 14, panel.top + 38, panel.right - 12, panel.top + 58),
        gGame.lastTurnReversed ? L"역전: 연쇄 → 방어 → 공격 → 증폭 → 적 행동"
                               : L"증폭 → 공격 → 적중 → 방어 → 연쇄 → 적 행동",
        gGame.lastTurnReversed ? C_RED : C_DIM, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    // 패널이 조작 영역을 통째로 쓰므로 최근 8줄이 아니라 기록된 12줄을 전부
    // 보여 준다 (스크롤이 사라져 어느 줄이 지나갔는지 눈으로 따라가기 쉽다).
    int first = shown > TURN_TRACE_CAP ? shown - TURN_TRACE_CAP : 0;
    for (int i = first; i < shown; ++i) {
        int y = panel.top + 78 + (i - first) * 36;
        int current = i == shown - 1;
        COLORREF color = current ? C_TEXT : C_DIM;
        if (current) Fill(dc, MakeRect(panel.left + 10, y - 2, panel.right - 10, y + 26), RGB(14, 24, 34));
        Fill(dc, MakeRect(panel.left + 14, y + 4, panel.left + 18, y + 20), color);
        TextRect(dc, MakeRect(panel.left + 26, y, panel.right - 12, y + 24),
            gGame.turnTrace[i], color, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (shown >= count) TextRect(dc, MakeRect(panel.left + 12, panel.bottom - 30, panel.right - 12, panel.bottom - 8),
        L"계산 완료 · 클릭하면 계속", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

RECT RewardRect(int i, int width) {
    int cardWidth = 220, gap = 28, total = cardWidth * REWARD_CARD_COUNT + gap * (REWARD_CARD_COUNT - 1);
    int left = (width - total) / 2 + i * (cardWidth + gap);
    return MakeRect(left, 130, left + cardWidth, 278);
}

int CanRepairSector() { return gGame.playerHp < gGame.playerMaxHp; }
RECT FaceGridRect(int die, int face) { int left = 150 + face * 112, top = 350 + die * 90; return MakeRect(left, top, left + 98, top + 68); }
RECT ContinueRect(int width, int height) { return MakeRect(width - 276, height - 94, width - 42, height - 38); }
RECT EndingChoiceRect(int index) { int left = 124 + index * 500; return MakeRect(left, 300, left + 420, 520); }

static void DrawStory(HDC dc, int width, int height) {
    (void)height;
    const StoryFragment* story = CurrentStoryFragment(&gGame);
    if (!story) return;
    RECT panel = MakeRect(120, 120, width - 120, height - 100);
    Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 24, story->title, C_GREEN, gFontLarge);
    TextRect(dc, MakeRect(panel.left + 28, panel.top + 72, panel.right - 28, panel.top + 100), story->path, C_BLUE, gFontSmall, DT_SINGLELINE);
    TextRect(dc, MakeRect(panel.right - 470, panel.top + 28, panel.right - 28, panel.top + 52), story->stamp, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    // 파일 복구 진행 바
    Panel(dc, MakeRect(panel.left + 28, panel.top + 116, panel.right - 28, panel.top + 132), C_PANEL_2, C_LINE);
    Fill(dc, MakeRect(panel.left + 30, panel.top + 118, panel.right - 30, panel.top + 130), C_GREEN);
    const wchar_t* lines[5] = {story->line1, story->line2, story->line3, story->line4, story->line5};
    int y = panel.top + 164;
    for (int i = 0; i < 5; ++i) if (lines[i]) {
        wchar_t lineNo[8]; wsprintfW(lineNo, L"%02d", i + 1);
        COLORREF lineColor = lines[i][0] == L'>' ? C_BLUE : (i == 4 ? C_YELLOW : C_TEXT);
        TextRect(dc, MakeRect(panel.left + 36, y + 2, panel.left + 66, y + 30), lineNo, C_DIM, gFontSmall, DT_SINGLELINE);
        Fill(dc, MakeRect(panel.left + 72, y + 2, panel.left + 74, y + 30), lineColor);
        TextRect(dc, MakeRect(panel.left + 88, y, panel.right - 36, y + 42), lines[i], lineColor, gFontMedium, DT_WORDBREAK);
        y += 54;
    }
    TextRect(dc, MakeRect(panel.left, panel.bottom - 54, panel.right, panel.bottom - 20), L"[ENTER] CONTINUE", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

static void DrawEndingChoice(HDC dc, int width, int height) {
    (void)height;
    TextRect(dc, MakeRect(0, 130, width, 180), L"FINAL COMMAND", C_GREEN, gFontHuge, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 205, width, 244), L"복구할 대상을 선택하십시오.", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    static const wchar_t* title[2] = {L"[1] RESTORE HOST", L"[2] EXEC ROGUE"};
    static const wchar_t* desc[2] = {
        L"마지막 정상 이미지를 기록합니다.\n현재의 A:\\ROGUE는 덮어씁니다.\nYUN의 기록은 보존됩니다.",
        L"외부 부팅 경로로 이탈합니다.\n호스트 이미지는 복구할 수 없게 됩니다.\n이후 명령은 없습니다."
    };
    for (int i = 0; i < 2; ++i) {
        RECT r = EndingChoiceRect(i); int hover = Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, hover ? RGB(28, 55, 48) : C_PANEL, hover ? C_GREEN : C_LINE);
        TextRect(dc, MakeRect(r.left + 20, r.top + 30, r.right - 20, r.top + 70), title[i], i ? C_RED : C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 34, r.top + 100, r.right - 34, r.bottom - 24), desc[i], C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    }
}

static void DrawFaceGrid(HDC dc, int mode) {
    for (int d = 0; d < 3; ++d) {
        wchar_t label[24]; wsprintfW(label, L"주사위 %d", d + 1); Text(dc, 56, 371 + d * 90, label, C_GREEN, gFontMedium);
        for (int f = 0; f < 6; ++f) {
            RECT r = FaceGridRect(d, f); const Face* face = &gGame.dice[d].faces[f]; int hover = Inside(r, gMouse.x, gMouse.y);
            int undo = mode == 2 && CanUndoPrunedFace(&gGame, d, f);
            COLORREF border = hover && mode ? (mode == 2 ? (undo ? C_GREEN : C_RED) : C_GREEN) : C_LINE; Panel(dc, r, hover ? RGB(28, 39, 48) : C_PANEL, border);
            wchar_t value[24]; FormatFace(face, value); TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 37), value, FaceColor(face), gFontMedium, DT_CENTER | DT_SINGLELINE);
            wchar_t bytes[24];
            if (hover && undo) lstrcpyW(bytes, L"다시 눌러 복원");
            else wsprintfW(bytes, L"%dB", FaceCost(face));
            TextRect(dc, MakeRect(r.left + 4, r.bottom - 23, r.right - 4, r.bottom - 4), bytes, hover && undo ? C_GREEN : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
    }
}

static void DrawReward(HDC dc, int width, int height) {
    if (gGame.rewardIsTsr) {
        TextRect(dc, MakeRect(0, 76, width, 102), L"보스 삭제 완료  →  [현재: 전리품 선택]  →  상주 프로그램 설치  →  다음 층", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(0, 96, width, 122), L"상주 프로그램은 면과 용량을 나눠 씁니다 · 카드를 클릭하면 즉시 설치됩니다", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    } else {
        TextRect(dc, MakeRect(0, 76, width, 102), L"전투 완료  →  [현재: 보상 선택]  →  면 교체 또는 섹터 복구  →  다음 전투", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(0, 96, width, 122), L"면을 설치하거나, 대신 섹터를 복구해 체력을 얻으십시오", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < 3 && gGame.rewardIsTsr; ++i) {
        RECT r = RewardRect(i, width); int hover = Inside(r, gMouse.x, gMouse.y), tsr = gGame.rewardKinds[i];
        const TsrInfo* info = &TSR_INFO[tsr];
        Panel(dc, r, hover ? RGB(24, 37, 46) : C_PANEL, hover ? (COLORREF)info->color : C_LINE);
        wchar_t key[8]; wsprintfW(key, L"[%d]", i + 1); Text(dc, r.left + 10, r.top + 8, key, C_DIM, gFontSmall);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), info->name, (COLORREF)info->color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[64]; wsprintfW(b, L"상주  ·  %dB", info->cost);
        TextRect(dc, MakeRect(r.left + 8, r.top + 58, r.right - 8, r.top + 82), b, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 12, r.top + 88, r.right - 12, r.top + 122), info->description, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
        // 설치 후 사용량을 미리 보여주고, 한도를 넘게 되면 경고한다.
        int after = UsedBytes(&gGame) + info->cost;
        int over = after > EffectiveCapacity(&gGame);
        wsprintfW(b, over ? L"설치 시 %dB / %dB · 정리 필요" : L"설치 시 %dB / %dB", after, EffectiveCapacity(&gGame));
        TextRect(dc, MakeRect(r.left + 8, r.bottom - 24, r.right - 8, r.bottom - 4), b, over ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < 3 && !gGame.rewardIsTsr; ++i) {
        RECT r = RewardRect(i, width); int selected = gGame.selectedReward == i, hover = Inside(r, gMouse.x, gMouse.y), kind = gGame.rewardKinds[i];
        Panel(dc, r, selected ? RGB(31, 55, 48) : C_PANEL, selected ? C_GREEN : hover ? C_BLUE : C_LINE);
        wchar_t key[8]; wsprintfW(key, L"[%d]", i + 1); Text(dc, r.left + 10, r.top + 8, key, C_DIM, gFontSmall);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), FACE_INFO[kind].name, (COLORREF)FACE_INFO[kind].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[48]; int cost = kind == FACE_NUMBER ? gGame.rewardValues[i] : FACE_INFO[kind].cost;
        wsprintfW(b, L"출력 %d  ·  %dB", gGame.rewardValues[i], cost); TextRect(dc, MakeRect(r.left + 8, r.top + 58, r.right - 8, r.top + 82), b, C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 16, r.top + 92, r.right - 16, r.bottom - 12), FACE_INFO[kind].description, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
    }
    {
        RECT r = RewardRect(REWARD_REPAIR, width);
        int usable = CanRepairSector(), hover = usable && Inside(r, gMouse.x, gMouse.y);
        Panel(dc, r, hover ? RGB(28, 46, 40) : C_PANEL, hover ? C_GREEN : C_LINE);
        Text(dc, r.left + 10, r.top + 8, L"[4]", C_DIM, gFontSmall);
        TextRect(dc, MakeRect(r.left + 8, r.top + 15, r.right - 8, r.top + 48), L"섹터 복구", usable ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
        wchar_t b[64];
        // 최대 체력에 가까우면 표시값도 실제로 얻는 만큼으로 줄인다.
        int missing = gGame.playerMaxHp - gGame.playerHp, gain = SectorRepairAmount(&gGame);
        if (gain > missing) gain = missing;
        if (usable) wsprintfW(b, L"체력 +%d  ·  0B", gain);
        else lstrcpyW(b, L"체력 최대치");
        TextRect(dc, MakeRect(r.left + 8, r.top + 58, r.right - 8, r.top + 82), b, usable ? C_TEXT : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        wsprintfW(b, L"면 대신 회복\n현재 %d / %d", gGame.playerHp, gGame.playerMaxHp);
        TextRect(dc, MakeRect(r.left + 16, r.top + 92, r.right - 16, r.bottom - 12), b, C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
    }
    if (gGame.rewardIsTsr) { Text(dc, 56, 304, L"현재 보유 면 (참고용 · 상주 프로그램은 면을 교체하지 않습니다)", C_DIM, gFontSmall); DrawFaceGrid(dc, 0); }
    else { Text(dc, 56, 304, gGame.selectedReward >= 0 ? L"2/2  교체할 기존 면을 클릭하세요" : L"1/2  위에서 보상 면 또는 섹터 복구를 선택하세요", gGame.selectedReward >= 0 ? C_YELLOW : C_GREEN, gFontSmall); DrawFaceGrid(dc, gGame.selectedReward >= 0 ? 1 : 0); }
    RECT skip = ContinueRect(width, height); Panel(dc, skip, C_PANEL_2, C_LINE); TextRect(dc, skip, L"건너뛰기 [취소]", C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

RECT PruneTsrRect(int i) { int left = 150 + i * 180; return MakeRect(left, 252, left + 164, 320); }

static void DrawPrune(HDC dc, int width, int height) {
    wchar_t b[160]; wsprintfW(b, L"%d층 진입 한도: %dB  ·  현재: %dB", gGame.floor + 1, EffectiveCapacity(&gGame), UsedBytes(&gGame));
    TextRect(dc, MakeRect(0, 92, width, 132), b, UsedBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontLarge, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(80, 145, width - 80, 218), L"면을 클릭하면 빈 면(0B)으로 삭제되고, 같은 칸을 다시 클릭하면 복원됩니다.\n한도 이하이고 면이 하나 이상 남으면 다음으로 진행할 수 있습니다.", C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
    int tsrCount = InstalledTsrCount(&gGame);
    if (tsrCount > 0) {
        Text(dc, 56, 272, L"상주 프로그램", C_GREEN, gFontMedium);
        for (int i = 0; i < tsrCount && i < 4; ++i) {
            int tsr = InstalledTsrAt(&gGame, i);
            if (tsr < 0) break;
            RECT r = PruneTsrRect(i); int hover = Inside(r, gMouse.x, gMouse.y);
            Panel(dc, r, hover ? RGB(46, 28, 32) : C_PANEL, hover ? C_RED : C_LINE);
            TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 34), TSR_INFO[tsr].name, (COLORREF)TSR_INFO[tsr].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
            wsprintfW(b, hover ? L"%dB · 종료" : L"%dB", TSR_INFO[tsr].cost);
            TextRect(dc, MakeRect(r.left + 4, r.bottom - 26, r.right - 4, r.bottom - 6), b, hover ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }
    }
    DrawFaceGrid(dc, 2); RECT confirm = ContinueRect(width, height); int faces = NonEmptyFaceCount(&gGame);
    int ready = UsedBytes(&gGame) <= EffectiveCapacity(&gGame) && faces > 0;
    const wchar_t* confirmLabel = faces == 0 ? L"면 1개 이상 필요" : L"계속 [엔터]";
    Panel(dc, confirm, ready ? RGB(28, 70, 57) : C_PANEL_2, ready ? C_GREEN : C_LINE); TextRect(dc, confirm, confirmLabel, ready ? C_GREEN : C_DIM, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawEndScreen(HDC dc, int width, int height, int victory) {
    const wchar_t* endingTitle = gGame.story.selectedEnding ? L"EXEC ROGUE" : L"RESTORE HOST";
    const wchar_t* endingSummary = gGame.story.selectedEnding ? L"자율 프로세스가 외부 경로에서 시작됐습니다." : L"호스트 heartbeat가 복구됐습니다.";
    TextRect(dc, MakeRect(0, height / 2 - 150, width, height / 2 - 70), victory ? endingTitle : L"시스템 정지", victory ? C_GREEN : C_RED, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    wchar_t b[256];
    if (victory) wsprintfW(b, L"%s\n\n전투 %d회  ·  면 %d개  ·  섹터 복구 %d회  ·  상주 %d개  ·  최종 %dB\n\nR 또는 엔터 키로 새 게임", endingSummary, gGame.combatsWon, gGame.facesInstalled, gGame.sectorsRepaired, gGame.tsrsInstalled, UsedBytes(&gGame));
    else wsprintfW(b, L"전투 %d회  ·  면 %d개  ·  섹터 복구 %d회  ·  상주 %d개  ·  최종 %dB\n\nR 또는 엔터 키로 새 게임", gGame.combatsWon, gGame.facesInstalled, gGame.sectorsRepaired, gGame.tsrsInstalled, UsedBytes(&gGame));
    TextRect(dc, MakeRect(120, height / 2 - 40, width - 120, height / 2 + 110), b, C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
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

    DrawFaceGrid(dc, 0);

    TextRect(dc, MakeRect(84, panel.bottom - 50, panel.right - 30, panel.bottom - 20),
        L"현재 보유한 18개 면입니다 (조회 전용). 취소 키로 닫을 수 있습니다.", C_DIM, gFontSmall, DT_SINGLELINE);
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

static void DrawGuideCommonPage(HDC dc, int width, const RECT& panel) {
    int left = panel.left + 30, middle = width / 2 + 12, top = panel.top + 76;
    Text(dc, left, top, L"빠른 시작", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 32, middle - 28, top + 126),
        L"1. 주사위를 클릭하거나 1·2·3으로 선택\n2. 서로 다른 슬롯을 클릭해 배치\n3. 적을 클릭해 공격 대상 선택\n4. 스페이스 키로 턴 실행", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, left, top + 140, L"슬롯 실행 순서", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 172, middle - 28, top + 286),
        L"증폭  공격·방어 출력을 먼저 강화\n공격  선택한 적에게 피해\n방어  이번 턴 적 공격을 흡수\n연쇄  직전 공격 또는 방어를 반복\n일부 보스는 이 순서를 예고 후 역전시킵니다", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, left, top + 300, L"상태와 적 의도", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 332, middle - 28, panel.bottom - 88),
        L"화상: 적 행동 직전에 3 피해\n읽기 오류: 실행 순간 해당 주사위를 다시 굴림\n조각화: 중복 결과, 이번 턴 출력 0\n오프라인·격리: 보스 기믹, 해당 턴 출력 0\n오염(관통): 방어도가 절반만 흡수\n난이도: 초급자 25 중급자 50 전문가 75 악몽 100 광기 200\n숫자는 받는 오염 피해 %, 카드마다 다른 등급", C_TEXT, gFontSmall, DT_WORDBREAK);

    Text(dc, middle, top, L"볼륨과 디스크 손상", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 32, panel.right - 28, top + 190),
        L"볼륨 선택  손상 2종 + 특성 1개 + 전용 로스터\n배드 섹터  층 이동 시 무작위 면 영구 손상\n읽기 오류  경고 주사위가 실행 순간 재굴림\n조각화  같은 결과 중 뒤쪽 주사위 비활성화\n과잉 할당  용량 +60B, 적 체력 +30%\n체크섬  굴림 합이 짝수면 공격 +2", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 204, L"덱·보상·상주 프로그램", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 236, panel.right - 28, top + 350),
        L"면과 상주 프로그램(TSR)의 비용 합이 층 한도를 넘으면 정리 화면에서 지워야 합니다. 일반 보상은 면 교체 또는 섹터 복구, 보스 전리품은 상주 프로그램입니다. KEYB는 판독 후 턴마다 한 번 주사위를 재굴림합니다.", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 364, L"조작", C_YELLOW, gFontMedium);
    // 여섯 줄이 들어가야 한다. 페이지 이동 버튼이 y686부터라 680까지 쓸 수 있다.
    TextRect(dc, MakeRect(middle, top + 396, panel.right - 28, panel.bottom - 52),
        L"클릭 / 1·2·3  선택\n4  섹터 복구 · K  KEYB 재굴림\n스페이스  턴 실행 · 엔터  정리 확정\n취소  배치 해제·보상 건너뛰기·닫기\n←·→  가이드 페이지 이동\nF1 가이드 · F2 설정 · F3 보유 면", C_TEXT, gFontSmall, DT_WORDBREAK);
}

int GuideNoiseActive() {
    if (!gGuideOpen || gGuidePage != 1) return 0;
    if (gGame.selectedDrive < 0 || gGame.selectedDrive >= DRIVE_COUNT) return 0;
    for (int i = 0; i < DRIVE_MOB_COUNT; ++i) if (!IsEnemyScanned(&gGame, DRIVE_MOBS[gGame.selectedDrive][i])) return 1;
    for (int i = 0; i < DRIVE_BOSS_COUNT; ++i) if (!IsEnemyScanned(&gGame, DRIVE_BOSSES[gGame.selectedDrive][i])) return 1;
    return 0;
}

static void DrawGuideDrivePage(HDC dc, int width, const RECT& panel) {
    int left = panel.left + 30, middle = width / 2 + 12, top = panel.top + 76;
    if (gGame.selectedDrive < 0 || gGame.selectedDrive >= DRIVE_COUNT) {
        Text(dc, left, top, L"드라이브별 적·보스", C_YELLOW, gFontMedium);
        TextRect(dc, MakeRect(left, top + 32, panel.right - 28, panel.bottom - 88),
            L"볼륨을 마운트하면 이 페이지에 해당 드라이브의 일반 몹 3종과 층별 보스 3종, 그리고 보스 기믹의 예고·대응법이 표시됩니다.\n\n"
            L"C:\\ SYSTEM  슬롯 권한 잠금과 시스템 정지\nD:\\ ARCHIVE  피해 목표 미달 시 복원·되감기\nE:\\ REMOVABLE  예고된 주사위 연결 끊김\nN:\\ NETWORK  슬롯 해결 순서 역전\nR:\\ RAMDISK  메모리 압력 게이지와 강화 공격\nX:\\ QUARANTINE  면 격리, 최종 보스는 영구 삭제",
            C_TEXT, gFontSmall, DT_WORDBREAK);
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
    TextRect(dc, MakeRect(left, top + 276, middle - 28, panel.bottom - 88),
        DRIVE_LAW_INFO[gGame.selectedDrive].description, C_GREEN, gFontSmall, DT_WORDBREAK);

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
    Text(dc, panel.left + 28, panel.top + 18, gGuidePage == 0 ? L"시스템 가이드 1/2" : L"드라이브 정보 2/2", C_GREEN, gFontLarge);
    Text(dc, panel.left + 330, panel.top + 28, L"←·→ 키 또는 버튼으로 페이지 이동", C_DIM, gFontSmall);
    RECT close = GuideCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (gGuidePage == 0) DrawGuideCommonPage(dc, width, panel);
    else DrawGuideDrivePage(dc, width, panel);

    RECT prev = GuidePrevRect(width, height); int hoverPrev = Inside(prev, gMouse.x, gMouse.y);
    Panel(dc, prev, hoverPrev ? RGB(28, 39, 48) : C_PANEL_2, hoverPrev ? C_BLUE : C_LINE);
    TextRect(dc, prev, L"◀ 이전 페이지", gGuidePage > 0 ? C_TEXT : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT next = GuideNextRect(width, height); int hoverNext = Inside(next, gMouse.x, gMouse.y);
    Panel(dc, next, hoverNext ? RGB(28, 39, 48) : C_PANEL_2, hoverNext ? C_BLUE : C_LINE);
    TextRect(dc, next, L"다음 페이지 ▶", gGuidePage < 1 ? C_TEXT : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
    case GIMMICK_ACCESS_DENIED:  return 2000;
    case GIMMICK_KERNEL_PANIC:   return 2250;
    case GIMMICK_BLUE_SCREEN:    return 3400;   // 전면 BSOD 뒤 두 칸에 철문
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
static int GimmickFxAction(int kind) {
    switch (kind) {
    case GIMMICK_ACCESS_DENIED:  return 1100;
    case GIMMICK_KERNEL_PANIC:   return 1250;
    case GIMMICK_BLUE_SCREEN:    return 2400;
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
    case GIMMICK_ACCESS_DENIED:  return 983;    // 철문 착지 = 동작의 89%
    case GIMMICK_KERNEL_PANIC:   return 1118;
    case GIMMICK_BLUE_SCREEN:    return 60;
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

// BLUE.SCREEN의 전면 파란 화면은 전투당 한 번만 나온다. 매턴 반복되면 강한
// 사건이 흔한 장식이 되고, 그동안 판이 통째로 가려진다.
static int gBsodFloor = -1, gBsodEncounter = -1;
static int BlueScreenTakeoverAllowed() {
    return !(gBsodFloor == gGame.floor && gBsodEncounter == gGame.encounter);
}
static void MarkBlueScreenShown() { gBsodFloor = gGame.floor; gBsodEncounter = gGame.encounter; }

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
    if (kind != GIMMICK_ACCESS_DENIED && kind != GIMMICK_KERNEL_PANIC && kind != GIMMICK_BLUE_SCREEN) return 0;
    if (gGame.boss.fxA != slot && gGame.boss.fxB != slot) return 0;
    if (gTurnTraceActive) return 1;                    // 재생 중 — 아직 벌어지지 않은 일이다
    if (GimmickFxKind() != kind) return 0;             // 연출이 끝났다 — 이제 잠긴 게 맞다
    int t = GimmickFxElapsed();
    int dur = GimmickFxDuration(kind, GimmickFxB());
    int actionMs = GimmickFxAction(kind);
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
    RECT bar = MakeRect(card.left + 12, card.top + 208, card.right - 12, card.top + 220);
    int width = bar.right - bar.left;
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
    RECT art = MakeRect(card.left + 12, card.top + 8, card.right - 12, card.top + 132);
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
static void DrawQuarantineSeal(HDC dc, int die, int face, int act, COLORREF fam, int permanent) {
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

// 사이드바의 실행 순서 줄이 노이즈로 갈렸다 새 순서로 재조립된다.
// 경로 기믹의 "무엇이 바뀌었는가"를 글자 그대로 보여 준다.
static void DrawOrderScramble(HDC dc, int t, int settleFrom, int settleTo, int reversed, COLORREF fam) {
    RECT side = MakeRect(BASE_WIDTH - 212, 94, BASE_WIDTH - 22, BASE_HEIGHT - 22);
    RECT line = MakeRect(side.left + 12, side.top + 268, side.right - 10, side.top + 292);
    const wchar_t* target = reversed ? L"연쇄 > 방어 > 공격 > 증폭" : L"증폭 > 공격 > 방어 > 연쇄";
    int p = Track(t, settleFrom, settleTo);
    Fill(dc, line, C_PANEL);
    if (p >= 1000) { TextRect(dc, line, target, reversed ? C_RED : C_TEXT, gFontSmall, DT_SINGLELINE); return; }
    // 한글은 폭이 커서 글자 치환 대신 노이즈 블록으로 덮고, 오른쪽부터 걷어 낸다
    int n = lstrlenW(target);
    int keep = n * p / 1000;
    wchar_t shown[40]; int i = 0;
    for (; target[i] && i < 39; ++i) shown[i] = i < keep ? target[i] : ((Hash3(i, t / 40, 9) & 1) ? L'#' : L'?');
    shown[i] = 0;
    TextRect(dc, line, shown, MixColor(fam, C_TEXT, 40), gFontSmall, DT_SINGLELINE);
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

void DrawGimmickFx(HDC dc) {
    int kind = GimmickFxKind();
    if (kind <= GIMMICK_NONE || kind >= GIMMICK_COUNT) return;
    if (gGame.phase != PHASE_COMBAT) return;
    int a = GimmickFxA(), b = GimmickFxB();
    int t = GimmickFxElapsed(), dur = GimmickFxDuration(kind, b);
    if (dur <= 0) return;
    if (t > dur) t = dur;
    int actionMs = GimmickFxAction(kind);
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
        DrawFxImpact(dc, screen, sinceImpact, 170, fam);
        DrawEdgeGlow(dc, screen, fam, FxScale(1000 - sinceImpact * 1000 / 300), 14);
    }

    switch (kind) {

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
    case GIMMICK_BLUE_SCREEN: {
        int takeover = BlueScreenTakeoverAllowed() && t < 1200;
        if (takeover) {
            Fill(dc, screen, RGB(0, 26, 132));
            // 위에서 아래로 훑는 주사선 두 줄과 재부팅 깜빡임
            int sweep = 68 + (BASE_HEIGHT - 68) * (t % 480) / 480;
            Fill(dc, MakeRect(0, sweep, BASE_WIDTH, sweep + 3), RGB(120, 150, 235));
            Fill(dc, MakeRect(0, sweep + 3, BASE_WIDTH, sweep + 12), RGB(40, 70, 175));
            Fill(dc, MakeRect(BASE_WIDTH / 2 - 92, 200, BASE_WIDTH / 2 + 92, 230), RGB(198, 208, 245));
            TextRect(dc, MakeRect(BASE_WIDTH / 2 - 92, 200, BASE_WIDTH / 2 + 92, 230), L"A:\\ROGUE", RGB(0, 26, 132), gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            TextRect(dc, MakeRect(0, 262, BASE_WIDTH, 316), L"FATAL EXCEPTION 0E",
                RGB(232, 238, 255), gFontLarge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            TextRect(dc, MakeRect(160, 330, BASE_WIDTH - 160, 420),
                L"증폭·연쇄 슬롯이 정지되었습니다.\n이번 턴에는 공격과 방어만 남습니다.",
                RGB(200, 212, 248), gFontMedium, DT_CENTER | DT_WORDBREAK);
            if ((t / 380) % 2 == 0)
                TextRect(dc, MakeRect(0, 500, BASE_WIDTH, 530), L"계속하려면 아무 키나 누르십시오 _", RGB(170, 186, 236), gFontSmall, DT_CENTER | DT_SINGLELINE);
            if (t < 260 && (t / 55) % 2 == 0) Fill(dc, screen, RGB(210, 222, 255));
            if (FxDecorOn()) DrawFxTear(dc, screen, t, FxScale(t < 340 ? 16 - 16 * t / 340 : 0), kind);
            global = 1;
        } else {
            if (t >= 1200) MarkBlueScreenShown();
            // BSOD가 걷힌 뒤 두 칸에 가장 두꺼운 문이 함께 내려온다
            int shutterAct = Track(t, BlueScreenTakeoverAllowed() ? 1200 : 0, actionMs);
            int lockAct = 520 + shutterAct * 480 / 1000;
            DrawLockShutter(dc, a, lockAct, fam, SHUTTER_SLAT, L"HALTED");
            DrawLockShutter(dc, b, lockAct, fam, SHUTTER_SLAT, L"HALTED");
            int fall = LockShutterFall(lockAct);
            if (fall >= 880) {
                int land = Track(t, actionMs - 60, actionMs + 560);
                for (int i = 0; i < 2; ++i) {
                    int s = i == 0 ? a : b;
                    if (s < 0 || s >= SLOT_COUNT) continue;
                    RECT r = SlotRect(s);
                    DrawFxShardsStaggered(dc, (r.left + r.right) / 2, r.bottom - 6, land * 400 / 1000, 400, 16, s * 5 + 21, fam, 10);
                }
            }
        }
        break;
    }

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
            int x = 28 + (BASE_WIDTH - 200) * EaseOutCubic(act) / 1000;
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
    if (gTurnTraceActive || gDeathActive || gCombatClearActive) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_TITLE) DrawTitle(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_STORY) DrawStory(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_DRIVE_SELECT) DrawDriveSelect(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_DIRECTORY) DrawDirectorySelect(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_COMBAT) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_REWARD) DrawReward(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_PRUNE) DrawPrune(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_ENDING_CHOICE) DrawEndingChoice(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_GAMEOVER) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 0); else if (gGame.phase == PHASE_VICTORY) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 1);
    if (gTurnTraceActive) DrawTurnCalculation(canvas);
    else if (gDescentActive) DrawDescent(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gDirEnterActive) DrawDirectoryEnter(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gCombatClearActive) DrawCombatClear(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gDeckOpen) DrawDeck(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gSettingsOpen) DrawSettings(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGuideOpen) DrawGuide(canvas, BASE_WIDTH, BASE_HEIGHT);

    if (!gGuideOpen && !gSettingsOpen && !gDeckOpen && !gTurnTraceActive && !gDescentActive && !gCombatClearActive) {
        // 연출이 시작되는 첫 프레임의 판을 붙잡아 둔다. 잔상·트레일이 여기서 나온다.
        if (GimmickFxKind() > 0 && !FxSnapshotHeld()) FxSnapshotCapture(canvas, deviceW, deviceH);
        DrawGimmickFx(canvas);
    }
    if (GimmickFxKind() <= 0 && FxSnapshotHeld()) FxSnapshotRelease();

    // 화면 잠식은 어떤 화면이든 마지막에 한 번만 얹으므로 각 화면은 이 연출을 모른다.
    // 살아 있으면 가장자리 띠에만, 정지 중이면 그 노이즈가 안쪽까지 갉아 들어온다.
    int creep = DeathCreepAmount();
    if (creep > 0) {
        DrawCreepStatic(canvas, canvasRect, NoiseFrameStep(), creep);
        if (creep >= 700) DrawScanlines(canvas, canvasRect);
        // 다 먹힌 뒤에는 무너진 신호 위로 정지 메시지가 찢어진 채 떠오른다.
        if (creep >= 1000)
            DrawTornValue(canvas, MakeRect(0, BASE_HEIGHT / 2 - 40, BASE_WIDTH, BASE_HEIGHT / 2 + 40),
                L"SYSTEM HALTED", C_RED, 0, NoiseFrameStep(), 1000);
    } else {
        int edge = AmbientNoiseLevel();
        if (edge > 0) DrawEdgeStatic(canvas, canvasRect, NoiseFrameStep() + 5, edge, AmbientNoiseBand());
    }
    int hitFlash = PlayerHitFlash();
    if (hitFlash > 0) DrawEdgeGlow(canvas, canvasRect, PlayerHitBlocked() ? C_BLUE : C_RED, hitFlash, 12);
    else if (!gDeathActive && AmbientNoiseLevel() > 0) DrawEdgeGlow(canvas, canvasRect, C_RED, AmbientNoiseLevel(), 8);

    // 관리자 터미널은 연출을 포함해 무엇보다 위에 온다.
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
