// Exercise the real input handlers and renderer in a console test process.
// Only the app's own hidden EDIT test control is created; no desktop input.
#define main ExistingFxCheckMain
#include "fx_check.cpp"
#undef main

static int NFail(const char* reason) { printf("FAIL narrative UI: %s\n", reason); return 1; }
static void ClickCenter(RECT rect) { HandleClick((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2); }

#include "narrative_persistence_check.inl"

static int CheckNarrativeInput() {
    InitCampaign(&gCampaign); gCampaignCorrupt = 1;
    ResetPresentation(); gFxLevel = FX_OFF;
    NewRun(&gGame, 100, 0); AttachNarrative(&gGame, &gCampaign.narrative);
    HandleKey(VK_RETURN);
    if (gGame.phase != PHASE_NAME_ENTRY) return NFail("P01 to name entry");
    gWindow = CreateWindowExW(0, L"STATIC", L"Narrative test", WS_POPUP,
        0, 0, BASE_WIDTH, BASE_HEIGHT, 0, 0, GetModuleHandleW(0), 0);
    if (!gWindow) return NFail("hidden test parent");
    SyncNarrativeControls();
    if (!gNarrativeNameEdit) return NFail("native name input creation");
    SetWindowTextW(gNarrativeNameEdit, L"민서");
    ConfirmNarrativeName();
    if (wcscmp(gGame.narrative.playerName, L"민서") || gGame.phase != PHASE_STORY)
        return NFail("native Korean name commit");
    HandleKey(VK_RETURN);
    if (!gGame.tutorial.active || gGame.tutorial.step != TUTORIAL_READ) return NFail("P02 starts training");
    HandleKey(VK_SPACE);
    if (gGame.tutorial.step != TUTORIAL_READ) return NFail("training cannot execute before reading");
    HandleKey('R');
    if (!gReadActive) return NFail("R starts actual read animation");
    StopRead();
    if (!gRolled || gGame.tutorial.step != TUTORIAL_PLACE) return NFail("read completion advances training");
    int slots[3] = {SLOT_ATTACK, SLOT_DEFEND, SLOT_AMPLIFY};
    for (int d = 0; d < 3; ++d) {
        ClickCenter(DieRect(d)); ClickCenter(SlotRect(slots[d]));
        if (gGame.dice[d].assignedSlot != slots[d]) return NFail("actual mouse placement");
    }
    if (gGame.tutorial.step != TUTORIAL_PREVIEW) return NFail("placement opens forecast step");
    HandleKey(VK_SPACE);
    if (gGame.tutorial.step != TUTORIAL_PREVIEW) return NFail("preview cannot be skipped by execute");
    ClickCenter(TutorialNextRect());
    if (gGame.tutorial.step != TUTORIAL_EXECUTE) return NFail("forecast acknowledgment");
    HandleKey(VK_SPACE);
    if (gGame.tutorial.step != TUTORIAL_COMPLETE) return NFail("training resolves real combat");
    if (gTurnTraceActive) FinishTurnTrace();
    ClickCenter(TutorialNextRect());
    if (gGame.tutorial.active || !gGame.narrative.tutorialSeen || gGame.phase != PHASE_STORY)
        return NFail("training completion opens P03");
    HandleKey(VK_RETURN);
    if (gGame.phase != PHASE_DRIVE_SELECT || gGame.selectedDrive != -1 || gGame.combatsWon)
        return NFail("training cannot alter campaign combat state");
    // Rehearsing from an active, already-read combat must restore exact state.
    SelectDrive(&gGame, 0);
    SelectDirectoryChoice(&gGame, 0);
    while (gGame.phase == PHASE_STORY) AdvanceStory(&gGame);
    SyncRollAnimation(); gRolled = 1;
    GameState original = gGame;
    gSettingsOpen = 1; BeginTutorialReplay();
    if (!gTutorialPracticeActive || !gGame.tutorial.active) return NFail("settings practice starts");
    FinishTutorialUi(1);
    if (gTutorialPracticeActive || memcmp(&original, &gGame, sizeof(gGame)) || !gRolled)
        return NFail("practice must restore current combat, deck and read state");
    DestroyWindow(gWindow); gWindow = 0; gNarrativeNameEdit = 0;
    if (gNarrativeNameFont) { DeleteObject(gNarrativeNameFont); gNarrativeNameFont = 0; }
    gNarrativeNameFontHeight = 0;
    ResetPresentation();
    return 0;
}

static int CheckStoryFrame(HDC dc, void* bits, int w, int h, const char* folder, const char* name, int* frames) {
    GameState before = gGame;
    if (gGame.phase == PHASE_STORY) {
        const StoryFragment* story = CurrentStoryFragment(&gGame);
        const wchar_t* source[5] = {story->line1, story->line2, story->line3, story->line4, story->line5};
        int total = 0;
        for (int pass = 0; pass < 2; ++pass) {
            total = 0;
            for (int i = 0; i < 5; ++i) {
                wchar_t text[640]; NarrativeText(source[i], text, 640);
                if (text[0]) total += NarrativeParagraph(dc, MakeRect(0, 0, BASE_WIDTH - 64 - 416 - 74, 0),
                    text, C_TEXT, pass ? gFontSmall : gFontMedium, 1) + 15;
            }
            if (total <= 322) break;
        }
        if (total > 322) { printf("Overflow at %s: %d pixels\n", name, total); return NFail("localized dialogue overflow"); }
    }
    DrawFixture(dc); GdiFlush();
    uint32_t first = FrameHash(bits, w, h);
    DrawFixture(dc); GdiFlush();
    if (first != FrameHash(bits, w, h)) return NFail("fixed-time frame changed");
    if (memcmp(&before, &gGame, sizeof(gGame))) return NFail("renderer changed game state");
    if (folder && !SaveFrame(folder, name, w, h, bits)) return NFail("cannot save review frame");
    ++*frames; return 0;
}

int main(int argc, char** argv) {
    LoadTranslations(); CreateRenderFonts();
    if (CheckNarrativeInput()) return 1;
    if (CheckNarrativePersistenceUi()) return 1;
    int frames = 0;
    for (int language = 0; language < LANGUAGE_COUNT; ++language)
    for (int fx = 0; fx < FX_LEVEL_COUNT; ++fx)
    for (int scale = 1; scale <= 2; ++scale) {
        int w = BASE_WIDTH * scale, h = BASE_HEIGHT * scale;
        BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* bits = 0; HDC dc = CreateCompatibleDC(0);
        HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, 0, 0);
        if (!dc || !bmp || !bits) return NFail("offscreen allocation");
        HGDIOBJ old = SelectObject(dc, bmp);
        SetMapMode(dc, MM_ANISOTROPIC); SetWindowExtEx(dc, BASE_WIDTH, BASE_HEIGHT, 0); SetViewportExtEx(dc, w, h, 0);
        ResetPresentation(); SetUiLanguage(language); gFxLevel = fx;
        const char* folder = argc > 1 && scale == 1 && fx == FX_FULL ? argv[1] : 0;
        NarrativeProgress progress = {}; progress.introSeen = progress.tutorialSeen = 1; progress.milestoneSeen = 0x3F;
        wcscpy_s(progress.playerName, L"아주긴이름열여섯글자확인용가나다");
        NewRun(&gGame, 4242, 0x3F); AttachNarrative(&gGame, &progress);
        for (int kind = STORY_INTRO; kind <= STORY_A_GREETING; ++kind) {
            int variants = kind == STORY_MILESTONE ? 6 : kind == STORY_INTRO ? 3 :
                kind == STORY_BOSS || kind == STORY_LOGS ? 21 : kind == STORY_SHARD ? 6 : 1;
            for (int variant = 0; variant < variants; ++variant) {
                gGame.selectedDrive = kind == STORY_BOSS || kind == STORY_LOGS ? variant / 3 : kind == STORY_SHARD ? variant : DRIVE_FINAL;
                int fragment = kind == STORY_BOSS || kind == STORY_LOGS ? variant % 3 : variant;
                gGame.clearedMask = kind == STORY_INTRO ? 0 : kind == STORY_MILESTONE ? (uint8_t)((1u << (fragment + 1)) - 1u)
                    : kind == STORY_SHARD ? (uint8_t)(1u << variant) : 0x3F;
                BeginStory(&gGame, kind, fragment, PHASE_DRIVE_SELECT);
                if (kind >= STORY_ENDING_RESTORE && kind <= STORY_ENDING_MERGE)
                    gGame.story.selectedEnding = (uint8_t)(kind - STORY_ENDING_RESTORE);
                for (int page = 0; page < StoryPageCount(&gGame); ++page) {
                    gGame.story.page = (uint8_t)page;
                    if (!CurrentStoryFragment(&gGame)) return NFail("missing authored story page");
                    gSceneKey = 1; gSceneStart = 10000; gCheckTick = 12800;
                    char name[90]; sprintf_s(name, "story_%s_%02d_%02d_%d", language ? "en" : "ko", kind, variant, page);
                    if (CheckStoryFrame(dc, bits, w, h, folder, name, &frames)) return 1;
                }
            }
        }
        gGame.phase = PHASE_ENDING_CHOICE;
        char name[90]; sprintf_s(name, "ending_choice_%s", language ? "en" : "ko");
        if (CheckStoryFrame(dc, bits, w, h, folder, name, &frames)) return 1;
        NewRun(&gGame, 101, 0); AttachNarrative(&gGame, &progress); BeginTutorial(&gGame);
        for (int step = TUTORIAL_READ; step <= TUTORIAL_COMPLETE; ++step) {
            if (step == TUTORIAL_PLACE) TutorialReadDice(&gGame);
            if (step == TUTORIAL_PREVIEW) {
                AssignDieToSlot(&gGame, 0, SLOT_ATTACK);
                AssignDieToSlot(&gGame, 1, SLOT_DEFEND);
                AssignDieToSlot(&gGame, 2, SLOT_AMPLIFY);
            }
            if (step == TUTORIAL_EXECUTE) AcknowledgeTutorialPreview(&gGame);
            if (step == TUTORIAL_COMPLETE) EndTurn(&gGame);
            gRolled = step != TUTORIAL_READ;
            sprintf_s(name, "tutorial_%s_%d", language ? "en" : "ko", step);
            if (CheckStoryFrame(dc, bits, w, h, folder, name, &frames)) return 1;
        }
        SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc);
    }
    FxSnapshotDestroy(); DestroyRenderFonts();
    printf("PASS narrative UI: Korean native name entry, real read/place/forecast/execute/finish, practice restore, %d fixed-time frames in 2 languages x 3 FX levels x 2 scales.\n", frames);
    return 0;
}
