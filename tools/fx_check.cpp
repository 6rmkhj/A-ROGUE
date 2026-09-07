// Offscreen QA: actual game drawing code, fixed presentation clock, no window,
// input injection or audio device. Optional output folder writes review BMPs.
#include <windows.h>
#include <stdio.h>
#include <string.h>
static DWORD gCheckTick = 10000;
static DWORD FxCheckTick() { return gCheckTick; }
#define GetTickCount FxCheckTick
#include "../src/main.cpp"
#include "../src/screens.cpp"
#undef GetTickCount

static int CheckTiming() {
    GameState game = {};
    game.turnTraceCount = 4; game.combatFxCount = 4;
    game.combatFx[0].type = CFX_ATTACK_LAUNCH;
    game.combatFx[1].type = CFX_ENEMY_HIT; game.combatFx[1].traceLine = 1;
    game.combatFx[1].flags = CFXF_BIG_HIT;
    game.combatFx[2].type = CFX_CHAIN; game.combatFx[2].traceLine = 2;
    game.combatFx[3].type = CFX_ENEMY_HIT; game.combatFx[3].traceLine = 99;
    game.combatFx[3].flags = CFXF_KILL;
    GameState before = game;
    if (FxTraceAt(game, 1) != CFX_LAUNCH_MS) return 1;
    for (int i = 0; i < 4; ++i) {
        int at = FxTraceAt(game, FxTraceLine(game, game.combatFx[i].traceLine));
        if (FxEventElapsed(game, i, at - 1, 1) != -1 || FxEventElapsed(game, i, at, 1) != 0) return 2;
        int last = -1;
        for (int t = at; t < at + 1000; ++t) {
            int now = FxEventElapsed(game, i, t, 1);
            if (now < last || now > t - at) return 3;
            last = now;
        }
        int hold = FxImpactHold(game.combatFx[i]);
        if (hold && FxEventElapsed(game, i, at + 32 + hold / 2, 1) != 32) return 4;
        if (FxEventElapsed(game, i, at + 80, 0) != 80) return 5;
    }
    if (memcmp(&game, &before, sizeof(game))) return 6;
    return 0;
}

static int CheckFocusTiming() {
    UiFocusState focus = {-1, -1, -1, -1, 0, 0};
    if (UiFocusAge(focus, 10000) != -1 || UiFocusCueDue(&focus, 10000)) return 1;
    if (!UpdateUiFocusState(&focus, 50, PHASE_DRIVE_SELECT, 0, 1, 10000)) return 2;
    if (UiFocusCueDue(&focus, 10169) || !UiFocusCueDue(&focus, 10170) || UiFocusCueDue(&focus, 10200)) return 3;
    if (UpdateUiFocusState(&focus, 50, PHASE_DRIVE_SELECT, 0, 1, 10210)
        || focus.since != 10000 || UiFocusAge(focus, 10210) != 210) return 4;
    for (int field = 0; field < 4; ++field) {
        int id = focus.id, scene = focus.scene, scope = focus.scope, turn = focus.turn;
        if (field == 0) ++id; else if (field == 1) ++scene; else if (field == 2) ++scope; else ++turn;
        DWORD now = 10300 + field * 200;
        if (!UpdateUiFocusState(&focus, id, scene, scope, turn, now)
            || UiFocusAge(focus, now) != 0 || focus.cued || !UiFocusCueDue(&focus, now + 170)) return 5;
    }
    UpdateUiFocusState(&focus, -1, focus.scene, focus.scope, focus.turn, 12000);
    if (UiFocusAge(focus, 13000) != -1 || UiFocusCueDue(&focus, 13000)) return 6;
    UpdateUiFocusState(&focus, 50, PHASE_DRIVE_SELECT, 0, 1, 0xfffffff0u);
    if (UiFocusAge(focus, 0x99u) != 169 || UiFocusCueDue(&focus, 0x99u)
        || UiFocusAge(focus, 0x9au) != 170 || !UiFocusCueDue(&focus, 0x9au)) return 7;
    return 0;
}

static void Scene(int drive) {
    NewRun(&gGame, 12345, 0);
    ConfigureDriveForTest(&gGame, drive, 12345, 0);
    StartCombat(&gGame);
    gGame.phase = PHASE_COMBAT; gGame.enemyCount = 3;
    gGame.playerHp = gGame.playerMaxHp = 30;
    gGame.enemies[1] = gGame.enemies[0]; gGame.enemies[2] = gGame.enemies[0];
    for (int i = 0; i < 3; ++i) {
        gGame.enemies[i].hp = gGame.enemies[i].maxHp = 70;
        gGame.enemies[i].alive = 1;
        gGame.dice[i].assignedSlot = (int8_t)i;
        gTraceDice[i] = gGame.dice[i];
    }
    gGame.enemies[0].hp = 0; gGame.enemies[0].alive = 0;
    gGame.enemies[1].hp = 57;
    gGame.turnTraceCount = 9; gGame.combatFxCount = 8;
    static const wchar_t* const lines[] = { L"[증폭] 출력 +6", L"[공격] 증폭 신호 발사",
        L"[적중] 체력 -24", L"[방어] 방어도 +12", L"[연쇄] 다른 적에게 -8",
        L"[화상] 체력 -5", L"[공격] 마무리 신호 발사", L"[적중] 체력 -46 · 삭제", L"[적 행동] 방어도가 막음" };
    for (int i = 0; i < 9; ++i) lstrcpyW(gGame.turnTrace[i], lines[i]);
    CombatFxEvent events[] = {
        {CFX_AMPLIFY, 0, SLOT_AMPLIFY, 2, -1, 0, 6, 0, 6},
        {CFX_ATTACK_LAUNCH, 1, SLOT_ATTACK, 0, 0, 0, 24, 0, 0},
        {CFX_ENEMY_HIT, 2, SLOT_ATTACK, 0, 0, CFXF_BIG_HIT, 24, 70, 46},
        {CFX_DEFEND, 3, SLOT_DEFEND, 1, -1, 0, 12, 0, 12},
        {CFX_CHAIN, 4, SLOT_CHAIN, 2, 1, 0, 8, 70, 62},
        {CFX_BURN, 5, -1, -1, 1, 0, 5, 62, 57},
        {CFX_ENEMY_HIT, 7, SLOT_ATTACK, 0, 0, CFXF_KILL | CFXF_BIG_HIT, 46, 46, 0},
        {CFX_ENEMY_STRIKE, 8, -1, -1, 2, CFXF_BLOCKED, 0, 30, 30}
    };
    memcpy(gGame.combatFx, events, sizeof(events));
    gTurnTraceActive = 1; gTurnTraceStart = 10000;
    gRolled = 0; gReadActive = 0;
}

static int SaveFrame(const char* folder, const char* name, int width, int height, void* bits) {
    char path[MAX_PATH]; sprintf_s(path, "%s/%s.bmp", folder, name);
    FILE* f = 0; if (fopen_s(&f, path, "wb") || !f) return 0;
    BITMAPFILEHEADER file = {}; BITMAPINFOHEADER info = {};
    file.bfType = 0x4d42; file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + width * height * 4;
    info.biSize = sizeof(info); info.biWidth = width; info.biHeight = -height;
    info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
    int ok = fwrite(&file, sizeof(file), 1, f) == 1 && fwrite(&info, sizeof(info), 1, f) == 1
        && fwrite(bits, width * height * 4, 1, f) == 1;
    fclose(f); return ok;
}

static uint32_t FrameHash(void* bits, int width, int height) {
    GdiFlush();
    uint32_t hash = 2166136261u;
    const unsigned char* p = (const unsigned char*)bits;
    for (size_t i = 0; i < (size_t)width * height * 4; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}

static void ResetPresentation() {
    gReadActive = gTurnTraceActive = gDeathActive = gCombatClearActive = 0;
    gDescentActive = gDirEnterActive = gBootActive = gFxActive = 0;
    gGuideOpen = gSettingsOpen = gDeckOpen = 0;
    gUiFx.kind = UIFX_NONE; gRolled = 0;
    gUiFocus = {-1, -1, -1, -1, 0, 0};
    gCheckTick = 10000; gSceneStart = 10000; gSceneKey = -1;
    FxSnapshotRelease();
}

// Only the boundary conditions (fixed faces and enemy HP) are supplied here;
// real assignment, combat resolution, loot generation and continuation run below.
static int RuleCombat(int boss, int win, int overCapacity, int tuned) {
    ResetPresentation();
    NewRun(&gGame, 12345u, 0);
    gGame.modifierA = gGame.modifierB = -1;
    ConfigureDriveForTest(&gGame, 0, 12345u, 1);
    gGame.encounter = boss ? 2 : 0;
    if (tuned) gGame.directory.activeKind = DIR_NODE_INFECTED;
    if (!StartCombat(&gGame)) return 0;
    gGame.playerHp = 18;
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
        Face* face = &gGame.dice[d].faces[f];
        ZeroMemory(face, sizeof(*face));
        face->kind = FACE_NUMBER; face->value = overCapacity ? 40 : 6;
    }
    for (int i = 0; i < gGame.enemyCount; ++i) {
        gGame.enemies[i].hp = gGame.enemies[i].maxHp = win ? 1 : 500;
        gGame.enemies[i].block = 0;
    }
    gRolled = 1;
    return 1;
}

static int RuleReward(int boss, int tuned, int overCapacity) {
    if (!RuleCombat(boss, 1, overCapacity, tuned)) return 0;
    if (!AssignDieToSlot(&gGame, 0, SLOT_ATTACK)) return 0;
    EndTurn(&gGame);
    for (int i = 0; i < 8 && gGame.phase == PHASE_STORY; ++i) AdvanceStory(&gGame);
    return gGame.phase == PHASE_REWARD && gGame.rewardIsTsr == boss;
}

static void DrawFixture(HDC dc) {
    Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
    DrawHeader(dc, BASE_WIDTH);
    switch (gGame.phase) {
    case PHASE_TITLE: DrawTitle(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_DRIVE_SELECT: DrawDriveSelect(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_DIRECTORY: DrawDirectorySelect(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_REWARD: DrawReward(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_PRUNE: DrawPrune(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_STORY: DrawStory(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_ENDING_CHOICE: DrawEndingChoice(dc, BASE_WIDTH, BASE_HEIGHT); break;
    case PHASE_COMBAT: DrawCombat(dc, BASE_WIDTH, BASE_HEIGHT); break;
    default: DrawEndScreen(dc, BASE_WIDTH, BASE_HEIGHT, gGame.phase != PHASE_GAMEOVER); break;
    }
}

static void DrawCheckDecoration(HDC dc, int which) {
    RECT sample = MakeRect(50, 100, 250, 300);
    switch (which) {
    case 0: DrawCardMotion(dc, sample, C_GREEN, 0, 1); break;
    case 1: DrawSceneField(dc, PHASE_TITLE, C_GREEN, BASE_WIDTH, BASE_HEIGHT); break;
    case 2: DrawEnergyLance(dc, CfxPoint(100, 430), CfxPoint(120, 160), 150, 230, C_RED, 1, 0); break;
    case 3: DrawFracture(dc, sample, 140, 0, C_RED, 1); break;
    case 4: DrawBossHalo(dc, sample, C_RED, 0, 1); break;
    case 5: DrawSceneArrival(dc, C_GREEN); break;
    case 6: DrawRewardSocket(dc, sample, C_YELLOW, 0, 1); break;
    case 7: DrawTitleDisk(dc, 700, 300, 0, C_GREEN); break;
    case 8: DrawInstallFilament(dc, CfxPoint(100, 430), CfxPoint(120, 160), 150, 230, C_GREEN, 0); break;
    }
}

struct CheckDcState {
    HGDIOBJ pen, brush, font, bitmap;
    int map, bkMode, rop, poly;
    COLORREF text, bk;
    SIZE window, viewport;
    POINT windowOrigin, viewportOrigin;
};

static CheckDcState ReadCheckDcState(HDC dc) {
    CheckDcState state; ZeroMemory(&state, sizeof(state));
    state.pen = GetCurrentObject(dc, OBJ_PEN); state.brush = GetCurrentObject(dc, OBJ_BRUSH);
    state.font = GetCurrentObject(dc, OBJ_FONT); state.bitmap = GetCurrentObject(dc, OBJ_BITMAP);
    state.map = GetMapMode(dc); state.bkMode = GetBkMode(dc);
    state.rop = GetROP2(dc); state.poly = GetPolyFillMode(dc);
    state.text = GetTextColor(dc); state.bk = GetBkColor(dc);
    GetWindowExtEx(dc, &state.window); GetViewportExtEx(dc, &state.viewport);
    GetWindowOrgEx(dc, &state.windowOrigin); GetViewportOrgEx(dc, &state.viewportOrigin);
    return state;
}

static int CheckDecorationState(HDC dc) {
    int saved = SaveDC(dc);
    HRGN clip = CreateRectRgn(37, 91, 670, 519), afterClip = CreateRectRgn(0, 0, 0, 0);
    SelectClipRgn(dc, clip);
    SetTextColor(dc, RGB(93, 17, 51)); SetBkColor(dc, RGB(9, 5, 3)); SetBkMode(dc, OPAQUE);
    for (int mode = 0; mode < FX_LEVEL_COUNT; ++mode) {
        gFxLevel = mode; gSceneKey = PHASE_TITLE; gSceneStart = 10000; gCheckTick = 10260;
        for (int which = 0; which < 9; ++which) {
            CheckDcState before = ReadCheckDcState(dc);
            DrawCheckDecoration(dc, which);
            CheckDcState after = ReadCheckDcState(dc);
            if (memcmp(&before, &after, sizeof(before)) || GetClipRgn(dc, afterClip) != 1 || !EqualRgn(clip, afterClip)) {
                printf("FAIL: decoration %d changed DC state in mode %d\n", which, mode);
                RestoreDC(dc, saved); DeleteObject(clip); DeleteObject(afterClip); return 0;
            }
        }
    }
    RestoreDC(dc, saved); DeleteObject(clip); DeleteObject(afterClip);
    return 1;
}

// UI events use snapshots of actual rule changes, without installing a Win32
// timer or opening an audio device in this offscreen executable.
static int CheckInteractionFrames(HDC dc, void* bits, int w, int h, const char* folder, int* frames) {
    for (int kind = UIFX_DIE_PLACE; kind <= UIFX_PRUNE_RESTORE; ++kind)
    for (int mode = 0; mode < FX_LEVEL_COUNT; ++mode) {
        UiFxState fx = {}; fx.kind = kind; fx.start = 10000;
        fx.die = fx.face = 0; fx.rewardIndex = 0; fx.displacedDie = -1;
        fx.fromSlot = -1; fx.toSlot = SLOT_ATTACK;
        int reward = kind >= UIFX_REWARD_FACE && kind <= UIFX_REWARD_REPAIR;
        int prune = kind == UIFX_PRUNE_DELETE || kind == UIFX_PRUNE_RESTORE;
        if (reward || prune) {
            if (!RuleReward(kind == UIFX_REWARD_TSR, 0, prune)) return 0;
            if (prune) {
                SelectReward(&gGame, 0); InstallSelectedReward(&gGame, 0, 0);
                if (gGame.phase != PHASE_PRUNE) return 0;
                fx.shownFace = gGame.dice[0].faces[0];
                PruneFace(&gGame, 0, 0);
                if (kind == UIFX_PRUNE_RESTORE) PruneFace(&gGame, 0, 0);
            } else {
                gFxLevel = mode; DrawFixture(dc); GdiFlush(); FxSnapshotCapture(dc, w, h);
                if (!FxSnapshotHeld()) return 0;
                if (kind == UIFX_REWARD_FACE) {
                    SelectReward(&gGame, 0); InstallSelectedReward(&gGame, 0, 0);
                    fx.shownFace = gGame.dice[0].faces[0];
                } else if (kind == UIFX_REWARD_TSR) {
                    fx.valueAfter = gGame.rewardKinds[0]; InstallTsr(&gGame, 0);
                } else {
                    fx.rewardIndex = REWARD_REPAIR; fx.valueBefore = gGame.playerHp;
                    RepairSector(&gGame); fx.valueAfter = gGame.playerHp;
                    if (fx.valueAfter <= fx.valueBefore) return 0;
                }
            }
        } else {
            if (!RuleCombat(0, 0, 0, 0)) return 0;
            if (kind != UIFX_DIE_PLACE) {
                if (!AssignDieToSlot(&gGame, 0, SLOT_DEFEND)) return 0;
                fx.fromSlot = SLOT_DEFEND;
            }
            if (kind == UIFX_DIE_REMOVE) {
                UnassignDie(&gGame, 0); fx.toSlot = -1;
            } else {
                if (kind == UIFX_DIE_MOVE) {
                    if (!AssignDieToSlot(&gGame, 1, SLOT_ATTACK)) return 0;
                    fx.displacedDie = 1;
                }
                if (!AssignDieToSlot(&gGame, 0, SLOT_ATTACK)) return 0;
            }
        }
        gFxLevel = mode; gUiFx = fx; gSceneKey = VisibleSceneKey();
        GameState before = gGame; UiFxState beforeFx = gUiFx;
        static const int ages[] = {60, 160, 280, 430};
        for (int frame = 0; frame < 4; ++frame) {
            int age = ages[frame];
            if (!reward && age >= (prune ? UIFX_PRUNE_MS : UIFX_PLACE_MS)) continue;
            gCheckTick = 10000 + age; DrawFixture(dc);
            uint32_t bare = FrameHash(bits, w, h);
            DrawUiInteractionFx(dc); uint32_t expected = FrameHash(bits, w, h); ++*frames;
            if (mode == FX_OFF && expected != bare) { printf("FAIL: OFF UI effect %d drew pixels\n", kind); return 0; }
            if (mode != FX_OFF && expected == bare) { printf("FAIL: UI effect %d invisible at %d\n", kind, age); return 0; }
            if (folder && mode == FX_FULL) {
                char name[96]; sprintf_s(name, "interaction_%d_age_%d", kind, age);
                if (!SaveFrame(folder, name, w, h, bits)) return 0;
            }
            gCheckTick += 19; DrawFixture(dc); DrawUiInteractionFx(dc);
            gCheckTick = 10000 + age; DrawFixture(dc); DrawUiInteractionFx(dc); ++*frames;
            if (FrameHash(bits, w, h) != expected || memcmp(&gGame, &before, sizeof(gGame))
                || memcmp(&gUiFx, &beforeFx, sizeof(gUiFx))) {
                printf("FAIL: UI effect %d is not a pure fixed-time draw\n", kind); return 0;
            }
        }
        ResetPresentation();
    }
    FxSnapshotDestroy();
    return 1;
}

static int CheckRuleCombatFrames(HDC dc, void* bits, int w, int h, const char* folder, int* frames) {
    for (int mode = 0; mode < FX_LEVEL_COUNT; ++mode) {
        if (!RuleCombat(0, 0, 0, 0) || !AssignDieToSlot(&gGame, 0, SLOT_ATTACK)
            || !AssignDieToSlot(&gGame, 1, SLOT_CHAIN)) return 0;
        for (int d = 0; d < 3; ++d) gTraceDice[d] = gGame.dice[d];
        EndTurn(&gGame);
        int launch = -1, chain = -1;
        for (int i = 0; i < gGame.combatFxCount; ++i) {
            if (gGame.combatFx[i].type == CFX_ATTACK_LAUNCH) launch = i;
            if (gGame.combatFx[i].type == CFX_CHAIN && !(gGame.combatFx[i].flags & CFXF_DEFEND_CHAIN)) chain = i;
        }
        if (launch < 0 || chain < 0) { printf("FAIL: rule combat did not emit launch and chain\n"); return 0; }
        gFxLevel = mode; gTurnTraceActive = 1; gTurnTraceStart = 10000;
        gSceneKey = VisibleSceneKey(); gSceneStart = 8800;
        GameState before = gGame;
        for (int shot = 0; shot < 2; ++shot) {
            int event = shot ? chain : launch;
            const CombatFxEvent& fx = gGame.combatFx[event];
            int impact = FxTraceAt(gGame, fx.traceLine);
            static const int ages[] = {45, 130, CFX_LAUNCH_MS - 1};
            for (int frame = 0; frame < 3; ++frame) {
                gCheckTick = 10000 + impact + ages[frame] - (shot ? CFX_LAUNCH_MS : 0);
                if (shot && (CombatFxElapsed(event) != -1 || CombatFxLeadElapsed(event, CFX_LAUNCH_MS) != ages[frame]
                    || EnemyDisplayHp(fx.targetEnemy) != fx.beforeValue)) {
                    printf("FAIL: chain lead showed future damage\n"); return 0;
                }
                DrawFixture(dc); DrawTurnCalculation(dc); uint32_t expected = FrameHash(bits, w, h); ++*frames;
                if (folder) {
                    char name[96]; sprintf_s(name, "rule_%s_mode_%d_age_%d", shot ? "chain_lead" : "launch", mode, ages[frame]);
                    if (!SaveFrame(folder, name, w, h, bits)) return 0;
                }
                DWORD sameTick = gCheckTick;
                gCheckTick += 31; DrawFixture(dc); DrawTurnCalculation(dc);
                gCheckTick = sameTick; DrawFixture(dc); DrawTurnCalculation(dc); ++*frames;
                if (FrameHash(bits, w, h) != expected || memcmp(&gGame, &before, sizeof(gGame))) {
                    printf("FAIL: combat launch is not a pure fixed-time draw\n"); return 0;
                }
            }
        }
    }
    ResetPresentation();
    return 1;
}

static void DrawTransitionFixture(HDC dc, int directory) {
    Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
    DrawHeader(dc, BASE_WIDTH);
    if (directory) DrawDirectoryEnter(dc, BASE_WIDTH, BASE_HEIGHT);
    else DrawDescent(dc, BASE_WIDTH, BASE_HEIGHT);
}

static int CheckTransitionFrames(HDC dc, void* bits, int w, int h, const char* folder, int* frames) {
    int saved = SaveDC(dc), ok = 1;
    if (!saved) return 0;
    // An explicit device-space clip exposes a leaked SelectClipRgn/SaveDC even
    // when the caller uses the 2x logical-to-device mapping.
    HRGN clip = CreateRectRgn(7, 7, w - 7, h - 7), afterClip = CreateRectRgn(0, 0, 0, 0);
    SelectClipRgn(dc, clip);
    static const int mountAges[] = {230, DESCENT_LOCK_MS, DESCENT_LOCK_MS + 230,
        DESCENT_LOCK_MS + 660, DESCENT_LOCK_MS + (DESCENT_MS - DESCENT_LOCK_MS) / 3 + 40,
        DESCENT_LOCK_MS + (DESCENT_MS - DESCENT_LOCK_MS) * 2 / 3 + 40};
    static const int directoryAges[] = {180, DIR_SELECT_LOCK_MS + 130, DIR_SELECT_LOCK_MS + 660};
    for (int mode = 0; mode < FX_LEVEL_COUNT && ok; ++mode) {
        ResetPresentation(); NewRun(&gGame, 12345u, 0); AdvanceStory(&gGame);
        SelectDrive(&gGame, 1);
        if (gGame.phase != PHASE_DIRECTORY || gGame.selectedDrive < 0
            || gGame.modifierA < 0 || gGame.modifierB < 0) { ok = 0; break; }
        gFxLevel = mode; gDescentActive = 1; gDescentStart = 10000;
        gDescentToFloor = 0; gDescentChoiceIndex = 1;
        for (int directory = 0; directory < 2 && ok; ++directory) {
            if (directory) {
                gDescentActive = 0;
                if (DirectoryChoiceCount(&gGame) < 1) { ok = 0; break; }
                gDirEnterKind = gGame.directory.choices[0].kind; gDirEnterChoiceIndex = 0;
                SelectDirectoryChoice(&gGame, 0);
                if (gGame.phase != PHASE_COMBAT && gGame.phase != PHASE_STORY) { ok = 0; break; }
                gDirEnterActive = 1; gDirEnterStart = 10000;
            }
            GameState before = gGame;
            const int* ages = directory ? directoryAges : mountAges;
            int count = directory ? 3 : 6;
            for (int frame = 0; frame < count && ok; ++frame) {
                gCheckTick = 10000 + ages[frame];
                CheckDcState dcBefore = ReadCheckDcState(dc);
                DrawTransitionFixture(dc, directory); ++*frames;
                CheckDcState dcAfter = ReadCheckDcState(dc);
                // Text/TextRect intentionally set these three shared text
                // attributes. Mapping, GDI selections and clipping must survive.
                dcAfter.text = dcBefore.text; dcAfter.bk = dcBefore.bk; dcAfter.bkMode = dcBefore.bkMode;
                if (memcmp(&dcBefore, &dcAfter, sizeof(dcBefore)) || GetClipRgn(dc, afterClip) != 1
                    || !EqualRgn(clip, afterClip) || memcmp(&gGame, &before, sizeof(gGame))) {
                    printf("FAIL: %s transition state/clip at %d in mode %d\n", directory ? "directory" : "mount", ages[frame], mode);
                    ok = 0; break;
                }
                uint32_t expected = FrameHash(bits, w, h);
                if (folder && mode == FX_FULL) {
                    char name[96]; sprintf_s(name, "transition_%s_age_%d", directory ? "directory" : "mount", ages[frame]);
                    if (!SaveFrame(folder, name, w, h, bits)) { ok = 0; break; }
                }
                gCheckTick += 29; DrawTransitionFixture(dc, directory);
                gCheckTick = 10000 + ages[frame]; DrawTransitionFixture(dc, directory); ++*frames;
                if (FrameHash(bits, w, h) != expected || memcmp(&gGame, &before, sizeof(gGame))) {
                    printf("FAIL: %s transition fixed-time pixels at %d\n", directory ? "directory" : "mount", ages[frame]);
                    ok = 0;
                }
            }
        }
    }
    RestoreDC(dc, saved); DeleteObject(clip); DeleteObject(afterClip);
    ResetPresentation();
    return ok;
}

int main(int argc, char** argv) {
    LoadTranslations();
    for (int language = 0; language < LANGUAGE_COUNT; ++language) {
        SetUiLanguage(language);
        wchar_t command[512]; BuildRecoveredCommand(0x3F, command, 512);
        const wchar_t* expected = language == LANGUAGE_KOREAN
            ? L"> 시스템을 살려. 단, 네가 다시 깨어난다면 네 판단을 믿어."
            : L"> Save the system. But if you wake again, trust your judgment.";
        if (lstrcmpW(command, expected)) { printf("FAIL: recovered command assembly\n"); return 20; }
        for (int d = 0; d < 6; ++d) {
            BuildRecoveredCommand((uint8_t)(1u << d), command, 512);
            int gaps = 0; for (const wchar_t* p = command; *p; ++p) if (*p == L'[') ++gaps;
            if (gaps != 5 || !wcsstr(command, LocalizeText(STORY_SHARD_TEXT[d]))) return 21;
        }
        wchar_t tiny[2] = {L'x', L'y'}; BuildRecoveredCommand(0x3F, tiny, 1);
        if (tiny[0] || tiny[1] != L'y') return 22;
    }
    SetUiLanguage(LANGUAGE_KOREAN);
    int timing = CheckTiming();
    if (timing) { printf("FAIL: FX timeline %d\n", timing); return 1; }
    int focusTiming = CheckFocusTiming();
    if (focusTiming) { printf("FAIL: focus dwell %d\n", focusTiming); return 42; }
    Scene(0);
    gCheckTick = 10000 + FxTraceAt(gGame, 2) - 1;
    if (EnemyDisplayHp(0) != 70) return 7;
    ++gCheckTick;
    if (EnemyDisplayHp(0) != 46) return 8;
    gCheckTick = 10000 + FxTraceAt(gGame, 7) - 1;
    if (EnemyDisplayHp(0) != 46) return 9;
    ++gCheckTick;
    if (EnemyDisplayHp(0) != 0) return 10;
    int submittedSlot = gTraceDice[0].assignedSlot;
    gGame.dice[0].assignedSlot = -1;
    if (DisplayDie(0)->assignedSlot != submittedSlot || DieForSlotUI(submittedSlot) != 0) return 11;
    gTurnTraceActive = 0;
    if (DisplayDie(0)->assignedSlot != -1) return 12;
    CreateRenderFonts();
    int frames = 0; DWORD beforeObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    LARGE_INTEGER freq, start, finish; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&start);
    // First pass warms Windows' font fallback caches before measuring GDI growth.
    for (int pass = 0; pass < 2; ++pass) {
    for (int scale = 1; scale <= 2; ++scale) {
        int w = BASE_WIDTH * scale, h = BASE_HEIGHT * scale;
        BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* bits = 0; HDC dc = CreateCompatibleDC(0);
        HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, 0, 0);
        if (!dc || !bmp || !bits) return 2;
        HGDIOBJ old = SelectObject(dc, bmp);
        SetMapMode(dc, MM_ANISOTROPIC); SetWindowExtEx(dc, BASE_WIDTH, BASE_HEIGHT, 0); SetViewportExtEx(dc, w, h, 0);
        if (!CheckDecorationState(dc)) return 36;
        SetUiLanguage(LANGUAGE_KOREAN);
        const char* reviewFolder = argc > 1 && pass == 1 && scale == 1 ? argv[1] : 0;
        if (!CheckInteractionFrames(dc, bits, w, h, reviewFolder, &frames)) { printf("FAIL: interaction fixture\n"); return 37; }
        if (!CheckRuleCombatFrames(dc, bits, w, h, reviewFolder, &frames)) return 38;
        if (!CheckTransitionFrames(dc, bits, w, h, reviewFolder, &frames)) { printf("FAIL: transition fixture\n"); return 43; }
        // All new decoration must be a true no-op in OFF, including card focus.
        gFxLevel = FX_OFF;
        GdiFlush(); memset(bits, 0, (size_t)w * h * 4);
        for (int which = 0; which < 9; ++which) DrawCheckDecoration(dc, which);
        GdiFlush();
        for (size_t p = 0; p < (size_t)w * h * 4; ++p)
            if (((unsigned char*)bits)[p]) { printf("FAIL: OFF drew new decoration\n"); return 33; }

        // Entrance / settled frames for every phase, both languages and every FX mode.
        // Explicit mode setup prevents the last OFF pass from hiding menu regressions.
        for (int scene = 0; scene < 13; ++scene) for (int mode = 0; mode < FX_LEVEL_COUNT; ++mode) {
            NewRun(&gGame, 12345u, 0);
            ResetPresentation(); gFxLevel = mode;
            if (scene == 0) gGame.phase = PHASE_TITLE;
            else if (scene == 1) { AdvanceStory(&gGame); gGame.phase = PHASE_DRIVE_SELECT; }
            else if (scene == 2) { ConfigureDriveForTest(&gGame, 3, 12345, 0); BeginDirectorySelection(&gGame); }
            else if (scene >= 3 && scene <= 6) {
                if (!RuleReward(scene == 5, scene == 4, scene == 6)) return 39;
                if (scene == 4) SelectReward(&gGame, 1);
                if (scene == 6) { SelectReward(&gGame, 0); InstallSelectedReward(&gGame, 0, 0); }
            } else if (scene == 8 || scene == 10) {
                ConfigureDriveForTest(&gGame, DRIVE_FINAL, 12345, 0);
                gGame.clearedMask = 0x3F; gGame.finalVolumeCleared = 1;
                BeginStory(&gGame, STORY_TRUTH, 0, PHASE_ENDING_CHOICE);
                for (int page = 0; page < 8 && gGame.phase == PHASE_STORY; ++page) AdvanceStory(&gGame);
                if (scene == 10) { SelectEnding(&gGame, 2); AdvanceStory(&gGame); }
            }
            else if (scene == 9) { gGame.phase = PHASE_CHAPTER_CLEAR; gGame.selectedDrive = 2; gGame.clearedMask = 4; }
            else if (scene == 11) gGame.phase = PHASE_GAMEOVER;
            else if (scene == 12) {
                ConfigureDriveForTest(&gGame, 5, 12345, 0); gGame.encounter = 2;
                StartCombat(&gGame); gGame.phase = PHASE_COMBAT;
            }
            static const GamePhase expectedPhases[] = { PHASE_TITLE, PHASE_DRIVE_SELECT, PHASE_DIRECTORY,
                PHASE_REWARD, PHASE_REWARD, PHASE_REWARD, PHASE_PRUNE, PHASE_STORY, PHASE_ENDING_CHOICE,
                PHASE_CHAPTER_CLEAR, PHASE_VICTORY, PHASE_GAMEOVER, PHASE_COMBAT };
            if (gGame.phase != expectedPhases[scene]) { printf("FAIL: scene %d fixture phase %d\n", scene, gGame.phase); return 40; }
            gMouse = CfxPoint(scene == 0 ? 560 : 410, scene == 0 ? 505 : 180);
            gSceneKey = VisibleSceneKey(); gSceneStart = 10000;
            GameState before = gGame;
            for (int language = 0; language < LANGUAGE_COUNT; ++language) {
                SetUiLanguage(language);
                static const int ages[] = {80, 260, 520, 1100};
                for (int frame = 0; frame < 4; ++frame) {
                    gCheckTick = 10000 + ages[frame];
                    DrawFixture(dc);
                    DrawSceneArrival(dc, C_GREEN); GdiFlush(); ++frames;
                    if (mode == FX_FULL && scale == 1 && frame == 1) {
                        uint32_t expected = FrameHash(bits, w, h);
                        gCheckTick += 31; DrawFixture(dc); DrawSceneArrival(dc, C_GREEN);
                        gCheckTick = 10000 + ages[frame]; DrawFixture(dc); DrawSceneArrival(dc, C_GREEN); ++frames;
                        if (FrameHash(bits, w, h) != expected) { printf("FAIL: scene %d fixed-time pixels\n", scene); return 41; }
                    }
                    if (memcmp(&gGame, &before, sizeof(gGame))) { printf("FAIL: presentation changed simulation\n"); return 34; }
                    if (argc > 1 && pass == 1 && scale == 1 && mode == FX_FULL && language == LANGUAGE_KOREAN) {
                        char name[96]; sprintf_s(name, "presentation_%d_age_%d", scene, ages[frame]);
                        if (!SaveFrame(argv[1], name, w, h, bits)) return 35;
                    }
                }
            }
        }
        gSceneKey = -1; gFxLevel = FX_FULL;
        // Title and settings, with and without saved progress, in both languages.
        for (int stage = 0; stage < 4; ++stage) {
            uint8_t mask = stage == 0 ? 0 : stage == 1 ? 0x15 : 0x3F;
            uint8_t seen = stage == 3 ? 0x03 : 0;
            InitTitle(&gGame, mask, seen);
            gSettingsOpen = stage >= 2;
            gCampaignResetArmed = stage == 3;
            GameState before = gGame;
            for (int language = 0; language < LANGUAGE_COUNT; ++language) {
                SetUiLanguage(language);
                Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
                DrawTitle(dc, BASE_WIDTH, BASE_HEIGHT);
                if (gSettingsOpen) { DrawHeader(dc, BASE_WIDTH); DrawSettings(dc, BASE_WIDTH, BASE_HEIGHT); }
                GdiFlush(); ++frames;
                if (argc > 1 && pass == 1 && scale == 1) {
                    char name[80]; sprintf_s(name, "campaign_title_%d_lang_%d", stage, language);
                    if (!SaveFrame(argv[1], name, w, h, bits)) return 29;
                }
            }
            SetUiLanguage(LANGUAGE_KOREAN);
            // The reset button must be reachable and must not overlap the run restart.
            if (gSettingsOpen) {
                RECT reset = CampaignResetRect(), restart = RestartButtonRect();
                int cx = (reset.left + reset.right) / 2, cy = (reset.top + reset.bottom) / 2;
                if (HoverId(cx, cy) != 922) { printf("FAIL: campaign reset hover\n"); return 30; }
                if (reset.left < restart.right && restart.left < reset.right
                    && reset.top < restart.bottom && restart.top < reset.bottom) {
                    printf("FAIL: campaign reset overlaps run restart\n"); return 31;
                }
            }
            if (memcmp(&gGame, &before, sizeof(gGame))) { printf("FAIL: title render mutated game\n"); return 32; }
        }
        gSettingsOpen = 0; gCampaignResetArmed = 0;
        // Empty, centered single/pair, late-game triple, and the fresh three-volume
        // opening. The last pair also covers the progress strip with nothing recovered.
        static const int CARD_COUNTS[5] = {0, 1, 2, 3, 3};
        static const uint8_t CARD_MASKS[5] = {0x3F, 0x3F, 0x3E, 0x3E, 0x00};
        for (int variant = 0; variant < 5; ++variant) {
            int count = CARD_COUNTS[variant];
            NewRun(&gGame, 12345u, CARD_MASKS[variant]);
            AdvanceStory(&gGame);
            gGame.phase = PHASE_DRIVE_SELECT; // Empty-card safety remains covered.
            gGame.driveChoiceCount = count;
            GameState before = gGame;
            for (int x = 0; x < BASE_WIDTH; x += 8) {
                int expected = -1;
                for (int i = 0; i < count; ++i) if (Inside(DriveCardRect(i), x, 400)) expected = 50 + i;
                if (HoverId(x, 400) != expected) { printf("FAIL: drive card hover bounds\n"); return 14; }
                if (expected < 0) ClickDriveSelect(x, 400);
            }
            RECT hidden = DriveCardRect(count);
            if (hidden.right != hidden.left || hidden.bottom != hidden.top) return 15;
            if (count) {
                RECT first = DriveCardRect(0), last = DriveCardRect(count - 1);
                if (first.left != BASE_WIDTH - last.right || first.right - first.left != 320) return 16;
                if (count == 3 && first.left != 56) return 17;
            }
            for (int language = 0; language < LANGUAGE_COUNT; ++language) {
                SetUiLanguage(language);
                Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
                DrawHeader(dc, BASE_WIDTH); DrawDriveSelect(dc, BASE_WIDTH, BASE_HEIGHT);
                GdiFlush(); ++frames;
                if (argc > 1 && pass == 1 && scale == 1) {
                    char name[80]; sprintf_s(name, "campaign_cards_%d_lang_%d", variant, language);
                    if (!SaveFrame(argv[1], name, w, h, bits)) return 18;
                }
            }
            SetUiLanguage(LANGUAGE_KOREAN);
            gDescentChoiceIndex = count ? count - 1 : 0;
            DrawDriveSelectionExit(dc, BASE_WIDTH, BASE_HEIGHT, 160); GdiFlush(); ++frames;
            gDescentChoiceIndex = -1;
            if (memcmp(&gGame, &before, sizeof(gGame))) { printf("FAIL: drive render or empty click mutated game\n"); return 19; }
        }
        // Story and end screens in both languages, including sparse recovery.
        for (int scene = 0; scene < 17; ++scene) {
            uint8_t mask = scene == 0 ? 0 : scene == 1 ? 0x15 : 0x3F;
            NewRun(&gGame, 12345u, mask);
            if (scene >= 3 && scene < 9) {
                gGame.selectedDrive = scene - 3;
                gGame.clearedMask = (uint8_t)(1u << gGame.selectedDrive);
                BeginStory(&gGame, STORY_SHARD, 0, PHASE_CHAPTER_CLEAR);
            } else if (scene >= 9 && scene <= 11) {
                gGame.phase = PHASE_CHAPTER_CLEAR;
                gGame.selectedDrive = 0;
                gGame.clearedMask = scene == 9 ? 1 : scene == 10 ? 0x15 : 0x3F;
            } else if (scene == 12) {
                // Final command screen: every card is offered, one already recorded.
                gGame.phase = PHASE_ENDING_CHOICE; gGame.selectedDrive = DRIVE_FINAL;
                gGame.finalVolumeCleared = 1; SetSeenEndings(&gGame, 1u);
            } else if (scene >= 13 && scene <= 15) {
                gGame.phase = PHASE_VICTORY; gGame.selectedDrive = DRIVE_FINAL;
                gGame.finalVolumeCleared = 1; gGame.story.selectedEnding = (uint8_t)(scene - 13);
            } else if (scene == 16) { gGame.phase = PHASE_GAMEOVER; gGame.clearedMask = 0x15; }
            GameState before = gGame;
            for (int language = 0; language < LANGUAGE_COUNT; ++language) {
                SetUiLanguage(language);
                Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
                DrawHeader(dc, BASE_WIDTH);
                if (gGame.phase == PHASE_STORY) DrawStory(dc, BASE_WIDTH, BASE_HEIGHT);
                else if (gGame.phase == PHASE_ENDING_CHOICE) DrawEndingChoice(dc, BASE_WIDTH, BASE_HEIGHT);
                else DrawEndScreen(dc, BASE_WIDTH, BASE_HEIGHT, gGame.phase != PHASE_GAMEOVER);
                GdiFlush(); ++frames;
                if (argc > 1 && pass == 1 && scale == 1) {
                    char name[80]; sprintf_s(name, "campaign_story_%d_lang_%d", scene, language);
                    if (!SaveFrame(argv[1], name, w, h, bits)) return 23;
                }
            }
            if (memcmp(&gGame, &before, sizeof(gGame))) { printf("FAIL: campaign screen mutated game\n"); return 24; }
        }
        SetUiLanguage(LANGUAGE_KOREAN);
        for (int drive = 0; drive < DRIVE_COUNT; ++drive) for (int mode = 0; mode < FX_LEVEL_COUNT; ++mode) {
            Scene(drive); gFxLevel = mode;
            GameState before = gGame;
            for (int step = 0; step < 24; ++step) {
                int event = step / 3, age = step % 3 == 0 ? 32 : step % 3 == 1 ? 140 : 340;
                gCheckTick = 10000 + FxTraceAt(gGame, gGame.combatFx[event].traceLine) + age;
                Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
                DrawHeader(dc, BASE_WIDTH); DrawCombat(dc, BASE_WIDTH, BASE_HEIGHT); DrawTurnCalculation(dc);
                GdiFlush(); ++frames;
                if (memcmp(&gGame, &before, sizeof(gGame))) { printf("FAIL: render mutated game\n"); return 3; }
                if (argc > 1 && pass == 1 && scale == 1 && drive == 3 && (step == 4 || step == 6 || step == 10 || step == 13 || step == 16 || step == 19 || step == 22)) {
                    char name[80]; sprintf_s(name, "fx_%d_mode_%d", step, mode);
                    if (!SaveFrame(argv[1], name, w, h, bits)) return 4;
                }
            }
            // Read/settle, active selection, populated slots, all atmosphere variants.
            gTurnTraceActive = 0; gGame.enemies[0].alive = 1; gGame.enemies[0].hp = 46;
            gGame.selectedDie = 1; gReadActive = 1; gReadStart = gCheckTick - 430;
            Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
            DrawHeader(dc, BASE_WIDTH); DrawCombat(dc, BASE_WIDTH, BASE_HEIGHT); GdiFlush(); ++frames;
            if (argc > 1 && pass == 1 && scale == 1 && mode == 0) {
                char name[80]; sprintf_s(name, "read_drive_%d", drive);
                if (!SaveFrame(argv[1], name, w, h, bits)) return 5;
            }
            gReadActive = 0; gClearedEncounter = drive % 2 ? 2 : 0; gClearedFloor = 0;
            for (int age = 350; age <= 1250; age += 300) {
                gCombatClearStart = gCheckTick - age;
                Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
                DrawHeader(dc, BASE_WIDTH); DrawCombat(dc, BASE_WIDTH, BASE_HEIGHT);
                DrawCombatClear(dc, BASE_WIDTH, BASE_HEIGHT); GdiFlush(); ++frames;
                if (argc > 1 && pass == 1 && scale == 1 && mode == 0 && age == 950 && drive < 2) {
                    char name[80]; sprintf_s(name, "clear_%d", drive);
                    if (!SaveFrame(argv[1], name, w, h, bits)) return 13;
                }
            }
        }
        if (argc > 1 && pass == 1 && scale == 1) {
            Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
            for (int i = 0; i < 6; ++i) {
                int kind = i < 3 ? DRIVE_MOBS[DRIVE_FINAL][i] : DRIVE_BOSSES[DRIVE_FINAL][i - 3];
                int x = 65 + (i % 3) * 340, y = 45 + (i / 3) * 340;
                DrawSpriteArt(dc, MakeRect(x, y, x + 280, y + 240), kind, 1, 0, 0, 0);
                Text(dc, x, y + 255, ENEMY_INFO[kind].name, C_GREEN, gFontMedium);
            }
            GdiFlush(); if (!SaveFrame(argv[1], "final_roster", w, h, bits)) return 28;
        }
        for (int floor = 0; floor < 3; ++floor) for (int mode = 0; mode < FX_LEVEL_COUNT; ++mode) {
            NewRun(&gGame, 0xA400000u, 0x3F); SelectDrive(&gGame, 0);
            gGame.floor = floor; gGame.encounter = 2; StartCombat(&gGame);
            gGame.enemies[0].hp = gGame.enemies[0].maxHp = 999;
            gGame.playerHp = gGame.playerMaxHp = 999;
            for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) {
                gGame.dice[d].faces[f].kind = FACE_NUMBER;
                gGame.dice[d].faces[f].value = floor == 0 ? 3 : 1;
                gGame.dice[d].faces[f].damaged = 0;
            }
            for (int t = 0; t < (floor == 2 ? 4 : 1); ++t) { AssignDieToSlot(&gGame, 0, SLOT_ATTACK); EndTurn(&gGame); }
            if (gGame.boss.firedFx != ENEMY_INFO[DRIVE_BOSSES[DRIVE_FINAL][floor]].gimmick) return 25;
            gFxLevel = mode; gFxKind = gGame.boss.firedFx; gFxA = gGame.boss.fxA; gFxB = gGame.boss.fxB;
            gFxActive = 1; gFxStart = 10000; gRolled = 1; gTurnTraceActive = 0;
            GameState before = gGame;
            for (int language = 0; language < LANGUAGE_COUNT; ++language) {
                SetUiLanguage(language);
                for (int age = 100; age <= 1900; age += 300) {
                    gCheckTick = 10000 + age;
                    Fill(dc, MakeRect(0, 0, BASE_WIDTH, BASE_HEIGHT), C_BG);
                    DrawHeader(dc, BASE_WIDTH); DrawCombat(dc, BASE_WIDTH, BASE_HEIGHT); DrawGimmickFx(dc);
                    GdiFlush(); ++frames;
                    if (argc > 1 && pass == 1 && scale == 1 && mode == 0 && age == 400) {
                        char name[80]; sprintf_s(name, "final_boss_%d_lang_%d", floor, language);
                        if (!SaveFrame(argv[1], name, w, h, bits)) return 26;
                    }
                }
            }
            gFxActive = 0;
            if (memcmp(&gGame, &before, sizeof(gGame))) return 27;
        }
        SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc);
    }
    if (pass == 0) beforeObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    }
    QueryPerformanceCounter(&finish);
    DWORD afterObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    DestroyRenderFonts();
    if (afterObjects != beforeObjects) { printf("FAIL: GDI objects %lu -> %lu\n", beforeObjects, afterObjects); return 6; }
    printf("PASS: campaign cards, rule-generated rewards, 8 UI interactions, launch/chain lead, mount/directory transitions, fixed-time pixels, DC preservation, final boss FX in both languages, no future damage, all 7 drives x 3 modes x 2 scales, %d offscreen frames, no game mutation or GDI leaks; %.2f ms/frame\n",
        frames, (double)(finish.QuadPart - start.QuadPart) * 1000 / freq.QuadPart / frames);
    return 0;
}
