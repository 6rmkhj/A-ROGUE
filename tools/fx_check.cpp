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

static void Scene(int drive) {
    NewRun(&gGame, 12345);
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

int main(int argc, char** argv) {
    int timing = CheckTiming();
    if (timing) { printf("FAIL: FX timeline %d\n", timing); return 1; }
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
                if (argc > 1 && pass == 1 && scale == 1 && drive == 3 && (step == 6 || step == 10 || step == 13 || step == 16 || step == 19 || step == 22)) {
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
        SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc);
    }
    if (pass == 0) beforeObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    }
    QueryPerformanceCounter(&finish);
    DWORD afterObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    DestroyRenderFonts();
    if (afterObjects != beforeObjects) { printf("FAIL: GDI objects %lu -> %lu\n", beforeObjects, afterObjects); return 6; }
    printf("PASS: FX timing, no future damage, all 6 drives x 3 modes x 2 scales, %d offscreen frames, no game mutation or GDI leaks; %.2f ms/frame\n",
        frames, (double)(finish.QuadPart - start.QuadPart) * 1000 / freq.QuadPart / frames);
    return 0;
}
