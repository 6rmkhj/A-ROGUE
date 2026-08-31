#include <windows.h>
#include "ui.h"
#include "render.h"
#include "audio.h"
#include "sprites.h"

// 창 모드 복원 정보는 설정 화면만 쓰므로 여기 둔다.
static int gWindowedScale = 100;
static RECT gWindowedRect;

RECT GuideButtonRect(int width) { return MakeRect(width - 148, 4, width - 18, 23); }
RECT GuideCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT SettingsButtonRect(int width) { return MakeRect(width - 148, 25, width - 18, 44); }
RECT SettingsCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT DeckButtonRect(int width) { return MakeRect(width - 148, 46, width - 18, 65); }
RECT DeckCloseRect(int width) { return MakeRect(width - 154, 91, width - 82, 129); }
RECT ScaleOptionRect(int index) { int left = 84 + index * 130; return MakeRect(left, 260, left + 112, 302); }
RECT FullscreenToggleRect() { return MakeRect(84, 380, 364, 422); }

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

static void DrawHeader(HDC dc, int width) {
    Fill(dc, MakeRect(0, 0, width, 68), RGB(10, 16, 22)); Fill(dc, MakeRect(0, 67, width, 68), C_GREEN);
    Text(dc, 24, 14, L"A:\\ROGUE", C_GREEN, gFontLarge);
    if (gGame.phase != PHASE_TITLE && gGame.phase != PHASE_DRIVE_SELECT) {
        wchar_t b[128];
        if (gGame.selectedDrive >= 0) {
            int floorIndex = gGame.floor > 2 ? 2 : gGame.floor;
            wsprintfW(b, L"%s  ·  %d층/3  ·  %d구역/3  ·  %d턴", DRIVE_INFO[gGame.selectedDrive].paths[floorIndex], gGame.floor + 1, gGame.encounter + 1, gGame.turn);
            Text(dc, 230, 18, b, C_TEXT, gFontSmall);
        } else {
            wsprintfW(b, L"%d층/3  ·  %d구역/3  ·  %d턴", gGame.floor + 1, gGame.encounter + 1, gGame.turn);
            Text(dc, 230, 14, b, C_TEXT, gFontMedium);
        }
        wsprintfW(b, L"체력 %d/%d", gGame.playerHp, gGame.playerMaxHp);
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

RECT EnemyRect(int i) { int left = 28 + i * 218; return MakeRect(left, 94, left + 198, 366); }
static RECT PortraitRect(const RECT& panel) { return MakeRect(panel.left + 31, panel.top + 8, panel.left + 167, panel.top + 132); }
RECT SlotRect(int i) { int left = 28 + i * 172; return MakeRect(left, 408, left + 154, 532); }
RECT DieRect(int i) { int left = 48 + i * 220; return MakeRect(left, 574, left + 184, 695); }
RECT EndTurnRect() { return MakeRect(696, 616, 896, 679); }
RECT ReadButtonRect() { return MakeRect(696, 544, 896, 600); }
int DieForSlotUI(int slot) { for (int d = 0; d < 3; ++d) if (gGame.dice[d].assignedSlot == slot) return d; return -1; }

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
        RECT letterRect = MakeRect(r.left + 8, r.top + 20, r.right - 8, r.top + 86);
        DrawSectorStatic(dc, letterRect, gGame.driveChoices[i], (int)(GetTickCount() / 260u), 60);
        TextRect(dc, letterRect, drive->letter, (COLORREF)drive->color, gFontHuge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawScanlines(dc, letterRect);
        TextRect(dc, MakeRect(r.left + 8, r.top + 92, r.right - 8, r.top + 118), drive->label, (COLORREF)drive->color, gFontMedium, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 16, r.top + 126, r.right - 14, r.top + 178), drive->description, C_DIM, gFontSmall, DT_WORDBREAK);
        Fill(dc, MakeRect(r.left + 12, r.top + 182, r.right - 12, r.top + 183), C_LINE);
        Text(dc, r.left + 16, r.top + 192, L"디스크 손상", C_RED, gFontSmall);
        DrawDriveModifier(dc, r, r.top + 216, drive->modifierA);
        DrawDriveModifier(dc, r, r.top + 278, drive->modifierB);
        Fill(dc, MakeRect(r.left + 12, r.top + 342, r.right - 12, r.top + 343), C_LINE);
        Text(dc, r.left + 16, r.top + 352, L"볼륨 특성", C_GREEN, gFontSmall);
        TextRect(dc, MakeRect(r.left + 16, r.top + 374, r.right - 14, r.top + 414), drive->perkText, C_TEXT, gFontSmall, DT_WORDBREAK);
        Text(dc, r.left + 16, r.top + 420, L"탐색 경로", C_BLUE, gFontSmall);
        TextRect(dc, MakeRect(r.left + 16, r.top + 442, r.right - 14, r.top + 466), drive->pathPreview, C_DIM, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
        TextRect(dc, MakeRect(r.left + 8, r.bottom - 34, r.right - 8, r.bottom - 10), hover ? L"클릭하여 마운트" : L"클릭 또는 숫자 키", hover ? (COLORREF)drive->color : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    }
    TextRect(dc, MakeRect(0, height - 100, width, height - 70), L"선택한 볼륨의 디스크 손상 2종이 이번 런 내내 적용됩니다", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
}

// 마운트/심층 진입 연출. 모든 값은 경과 시간의 순수 함수라 마우스 이동 리페인트와 겹쳐도 안전하다.
static void DrawDescent(HDC dc, int width, int height) {
    const DriveInfo* drive = &DRIVE_INFO[gGame.selectedDrive < 0 ? 0 : gGame.selectedDrive];
    int elapsed = (int)(GetTickCount() - gDescentStart);
    if (elapsed < 0) elapsed = 0; if (elapsed > DESCENT_MS) elapsed = DESCENT_MS;
    int mount = gDescentToFloor == 0;
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
    int typed = elapsed * length / (DESCENT_MS * 3 / 5);
    if (typed > length) typed = length;
    wchar_t typedText[64] = L"> ";
    lstrcpynW(typedText + 2, path, typed + 1);
    if (typed < length && ((elapsed / 220) & 1)) lstrcatW(typedText, L"_");
    TextRect(dc, MakeRect(panel.left + 26, panel.top + 98, panel.right - 26, panel.top + 148), typedText, (COLORREF)drive->color, gFontLarge, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 판독 노이즈 밴드: 진행될수록 정적이 걷힌다.
    int noiseLevel = 900 - elapsed * 900 / DESCENT_MS;
    RECT band = MakeRect(panel.left + 26, panel.top + 162, panel.right - 26, panel.top + 272);
    Panel(dc, band, RGB(8, 13, 19), C_LINE);
    RECT inner = MakeRect(band.left + 2, band.top + 2, band.right - 2, band.bottom - 2);
    DrawSectorStatic(dc, inner, gGame.selectedDrive + 11, elapsed / NOISE_CHURN_MS, 200 + noiseLevel);
    DrawSectorHex(dc, inner, gGame.selectedDrive + 5, elapsed / NOISE_CHURN_MS, 300 + noiseLevel);
    DrawScanlines(dc, inner);

    RECT barRect = MakeRect(panel.left + 26, panel.top + 288, panel.right - 26, panel.top + 306);
    Bar(dc, barRect, elapsed, DESCENT_MS, (COLORREF)drive->color);
    wsprintfW(b, L"%d%%", elapsed * 100 / DESCENT_MS);
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
    const wchar_t* hint = gGame.phase == PHASE_PRUNE
        ? L"진입 후 용량 정리가 필요합니다 · 클릭이나 키로 바로 넘기기"
        : L"잠시 후 전투가 시작됩니다 · 클릭이나 키로 바로 넘기기";
    TextRect(dc, MakeRect(panel.left + 26, panel.bottom - 44, panel.right - 26, panel.bottom - 18), hint, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
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

// 카드 0~2는 설치할 면, 마지막 카드는 면 대신 체력을 얻는 섹터 복구다.
#define REWARD_CARD_COUNT 4
#define REWARD_REPAIR 3

RECT RewardRect(int i, int width) {
    int cardWidth = 220, gap = 28, total = cardWidth * REWARD_CARD_COUNT + gap * (REWARD_CARD_COUNT - 1);
    int left = (width - total) / 2 + i * (cardWidth + gap);
    return MakeRect(left, 130, left + cardWidth, 278);
}

int CanRepairSector() { return gGame.playerHp < gGame.playerMaxHp; }
RECT FaceGridRect(int die, int face) { int left = 150 + face * 112, top = 350 + die * 90; return MakeRect(left, top, left + 98, top + 68); }
RECT ContinueRect(int width, int height) { return MakeRect(width - 276, height - 94, width - 42, height - 38); }

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
    TextRect(dc, MakeRect(0, 76, width, 102), L"전투 완료  →  [현재: 보상 선택]  →  면 교체 또는 섹터 복구  →  다음 전투", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(0, 96, width, 122), L"면을 설치하거나, 대신 섹터를 복구해 체력을 얻으십시오", C_TEXT, gFontMedium, DT_CENTER | DT_SINGLELINE);
    for (int i = 0; i < 3; ++i) {
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
    Text(dc, 56, 304, gGame.selectedReward >= 0 ? L"2/2  교체할 기존 면을 클릭하세요" : L"1/2  위에서 보상 면 또는 섹터 복구를 선택하세요", gGame.selectedReward >= 0 ? C_YELLOW : C_GREEN, gFontSmall); DrawFaceGrid(dc, gGame.selectedReward >= 0 ? 1 : 0);
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
    wchar_t b[192]; wsprintfW(b, L"전투 %d회 완료  ·  면 %d개 설치  ·  섹터 복구 %d회  ·  최종 덱 %dB\n\nR 또는 엔터 키로 새 게임", gGame.combatsWon, gGame.facesInstalled, gGame.sectorsRepaired, DeckBytes(&gGame));
    TextRect(dc, MakeRect(120, height / 2 - 40, width - 120, height / 2 + 110), b, C_TEXT, gFontMedium, DT_CENTER | DT_WORDBREAK);
}

static void DrawDeck(HDC dc, int width, int height) {
    RECT shade = MakeRect(0, 68, width, height); Fill(dc, shade, RGB(6, 9, 13));
    RECT panel = MakeRect(54, 82, width - 54, height - 28); Panel(dc, panel, C_PANEL, C_GREEN);
    Text(dc, panel.left + 28, panel.top + 18, L"보유 중인 디스크 면", C_GREEN, gFontLarge);
    wchar_t b[64]; wsprintfW(b, L"덱 %dB / %dB", DeckBytes(&gGame), EffectiveCapacity(&gGame));
    Text(dc, panel.left + 28, panel.top + 58, b, DeckBytes(&gGame) > EffectiveCapacity(&gGame) ? C_RED : C_GREEN, gFontSmall);
    RECT close = DeckCloseRect(width); Panel(dc, close, C_PANEL_2, C_LINE);
    TextRect(dc, close, L"닫기", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawFaceGrid(dc, 0);

    TextRect(dc, MakeRect(84, panel.bottom - 50, panel.right - 30, panel.bottom - 20),
        L"현재 보유한 18개 면입니다 (조회 전용). 취소 키로 닫을 수 있습니다.", C_DIM, gFontSmall, DT_SINGLELINE);
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

    Text(dc, middle, top, L"볼륨과 디스크 손상", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 32, panel.right - 28, top + 190),
        L"볼륨 선택  런 시작 시 드라이브마다 손상 2종 공개 + 고유 특성 1개\n배드 섹터  층 이동 시 무작위 면 영구 손상 (설치해도 복구 안 됨)\n읽기 오류  경고 주사위가 실행 순간 재굴림\n조각화  같은 결과 중 뒤쪽 주사위 비활성화\n과잉 할당  용량 +60B, 적 체력 +30%\n체크섬  굴림 합이 짝수면 공격 +2", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 204, L"덱·보상·용량", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 236, panel.right - 28, top + 350),
        L"18개 면의 비용 합이 덱 용량입니다. 전투 뒤 보상 면을 골라 기존 면과 교체하거나, 면 대신 섹터 복구를 골라 체력을 회복할 수 있습니다. 현재 층 및 다음 층 한도를 넘으면 빈 면(0B)이 되도록 면을 삭제해야 합니다.", C_TEXT, gFontSmall, DT_WORDBREAK);
    Text(dc, middle, top + 364, L"조작", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 396, panel.right - 28, panel.bottom - 24),
        L"클릭 / 1·2·3  선택\n4  보상 화면에서 섹터 복구\n스페이스  턴 실행\n취소  배치 해제·보상 건너뛰기·가이드 닫기\n엔터  용량 정리 확정\nF1  가이드 열기·닫기\nF2  설정 열기·닫기\nF3  보유 면 조회", C_TEXT, gFontSmall, DT_WORDBREAK);
}

void PaintGame(HWND window) {
    SyncIdleAnimation();
    PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint); RECT client; GetClientRect(window, &client);
    int clientWidth = client.right, clientHeight = client.bottom;
    if (clientWidth <= 0 || clientHeight <= 0) { EndPaint(window, &paint); return; }

    // 1단계: 항상 고정된 BASE_WIDTH x BASE_HEIGHT 캔버스에 그린다 - 기존 좌표 계산은 전부 그대로 둔다.
    HDC canvas = CreateCompatibleDC(dc); HBITMAP canvasBitmap = CreateCompatibleBitmap(dc, BASE_WIDTH, BASE_HEIGHT); HBITMAP oldCanvas = (HBITMAP)SelectObject(canvas, canvasBitmap);
    RECT canvasRect = MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT);
    Fill(canvas, canvasRect, C_BG); DrawHeader(canvas, BASE_WIDTH);
    if (gTurnTraceActive) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_TITLE) DrawTitle(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_DRIVE_SELECT) DrawDriveSelect(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_COMBAT) DrawCombat(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_REWARD) DrawReward(canvas, BASE_WIDTH, BASE_HEIGHT); else if (gGame.phase == PHASE_PRUNE) DrawPrune(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGame.phase == PHASE_GAMEOVER) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 0); else if (gGame.phase == PHASE_VICTORY) DrawEndScreen(canvas, BASE_WIDTH, BASE_HEIGHT, 1);
    if (gTurnTraceActive) DrawTurnCalculation(canvas);
    else if (gDescentActive) DrawDescent(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gCombatClearActive) DrawCombatClear(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gDeckOpen) DrawDeck(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gSettingsOpen) DrawSettings(canvas, BASE_WIDTH, BASE_HEIGHT);
    else if (gGuideOpen) DrawGuide(canvas, BASE_WIDTH, BASE_HEIGHT);

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
