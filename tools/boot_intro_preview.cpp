// Silent, deterministic preview of the actual native intro composition.
// This reuses the offscreen fixture and fake presentation clock only; no test
// entry point, window, timer, save file, input injection or audio device runs.
#define main ExistingFxCheckMain
#include "fx_check.cpp"
#undef main

static const int PREVIEW_FPS = 30;
static const int PREVIEW_TITLE_MS = 400;
static const int PREVIEW_BOOT_MS = BOOT_INTRO_MS * BOOT_PACE_PCT / 100;
static const int PREVIEW_ARRIVAL_MS = 1280;
static const int PREVIEW_TOTAL_MS = PREVIEW_TITLE_MS + PREVIEW_BOOT_MS + PREVIEW_ARRIVAL_MS;
static const int PREVIEW_FRAMES = PREVIEW_TOTAL_MS * PREVIEW_FPS / 1000;
static const DWORD PREVIEW_CLOCK = 10000;
static_assert(PREVIEW_TOTAL_MS == 8000, "Update the preview timing when the boot duration changes");
static_assert(PREVIEW_FRAMES == 240, "The preview must contain exactly eight seconds at 30 fps");

struct PreviewCanvas {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ oldBitmap;
    void* bits;
    PreviewCanvas() : dc(0), bitmap(0), oldBitmap(0), bits(0) {}
    ~PreviewCanvas() {
        if (dc && oldBitmap) SelectObject(dc, oldBitmap);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }
    bool Open() {
        dc = CreateCompatibleDC(0);
        if (!dc) return false;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = BASE_WIDTH;
        info.bmiHeader.biHeight = -BASE_HEIGHT;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, 0, 0);
        if (!bitmap || !bits) return false;
        oldBitmap = SelectObject(dc, bitmap);
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, BASE_WIDTH, BASE_HEIGHT, 0);
        SetViewportExtEx(dc, BASE_WIDTH, BASE_HEIGHT, 0);
        return true;
    }
};

static void BeginPreviewArrival() {
    // The game-state portion of FinishBootIntro, without its HWND/timer work.
    gBootActive = gBootSkipping = 0;
    FxSnapshotRelease();
    NewRun(&gGame, 12345u, CampaignClearedMask(&gCampaign));
    SetSeenEndings(&gGame, CampaignSeenEndingMask(&gCampaign));
    gGame.finalVolumeCleared = gCampaign.finalCleared;
    AttachNarrative(&gGame, &gCampaign.narrative);
    gSceneKey = VisibleSceneKey();
    gScenePhase = gGame.phase;
    gSceneMajor = 1;
    gSceneStart = PREVIEW_CLOCK + PREVIEW_TITLE_MS + PREVIEW_BOOT_MS;
}

static int RenderPreview(const char* folder) {
    DWORD attributes = GetFileAttributesA(folder);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        printf("FAIL: preview output directory does not exist: %s\n", folder);
        return 1;
    }
    PreviewCanvas canvas;
    if (!canvas.Open()) { puts("FAIL: preview canvas allocation"); return 2; }
    // A user-facing film preview must not silently succeed using fallback art.
    if (!DrawBootFilm(canvas.dc, BASE_WIDTH, BASE_HEIGHT, 0, false)) {
        puts("FAIL: embedded boot film is unavailable; render and package the final asset first");
        return 3;
    }
    InitCampaign(&gCampaign);
    ResetPresentation();
    gFxLevel = FX_FULL;
    InitTitle(&gGame, 0, 0);
    gSceneKey = VisibleSceneKey(); gScenePhase = gGame.phase;
    gSceneStart = PREVIEW_CLOCK - TITLE_SETTLE_AT - 600;
    RECT start = StartButtonRect(BASE_WIDTH, BASE_HEIGHT);
    gMouse = {(start.left + start.right) / 2, (start.top + start.bottom) / 2};
    GameState title = gGame;
    uint32_t clickedHash = 0;
    bool started = false, arrived = false;

    for (int frame = 0; frame < PREVIEW_FRAMES; ++frame) {
        int at = frame * 1000 / PREVIEW_FPS;
        gCheckTick = PREVIEW_CLOCK + at;
        if (at < PREVIEW_TITLE_MS) {
            DrawFixture(canvas.dc);
        } else if (at < PREVIEW_TITLE_MS + PREVIEW_BOOT_MS) {
            if (!started) {
                // Keep the last displayed title frame, exactly as real input
                // captures the completed canvas before its next WM_PAINT.
                clickedHash = FrameHash(canvas.bits, BASE_WIDTH, BASE_HEIGHT);
                FxSnapshotCapture(canvas.dc, BASE_WIDTH, BASE_HEIGHT);
                if (!FxSnapshotHeld()) { puts("FAIL: preview click capture"); return 4; }
                gBootActive = 1; gBootSkipping = 0;
                gBootStart = PREVIEW_CLOCK + PREVIEW_TITLE_MS;
                gSceneKey = -1;
                started = true;
            }
            DrawFixture(canvas.dc);
            DrawBootIntro(canvas.dc, BASE_WIDTH, BASE_HEIGHT);
            if (at == PREVIEW_TITLE_MS
                && FrameHash(canvas.bits, BASE_WIDTH, BASE_HEIGHT) != clickedHash) {
                puts("FAIL: preview changed the first clicked frame"); return 5;
            }
            if (memcmp(&gGame, &title, sizeof(gGame))) {
                puts("FAIL: preview boot rendering mutated the game"); return 6;
            }
        } else {
            if (!arrived) {
                BeginPreviewArrival();
                arrived = true;
            }
            DrawFixture(canvas.dc);
            DrawSceneArrivalAt(canvas.dc, C_GREEN, 1, at - PREVIEW_TITLE_MS - PREVIEW_BOOT_MS);
        }
        GdiFlush();
        char name[48]; sprintf_s(name, "frame_%04d", frame);
        if (!SaveFrame(folder, name, BASE_WIDTH, BASE_HEIGHT, canvas.bits)) {
            printf("FAIL: could not write preview frame %d\n", frame); return 7;
        }
    }
    printf("PASS: silent preview, %d BMP frames, %dx%d at %d fps, 8.000 seconds; "
        "0.400s title + 6.320s native boot + 1.280s arrival; first click frame identical\n",
        PREVIEW_FRAMES, BASE_WIDTH, BASE_HEIGHT, PREVIEW_FPS);
    printf("Output: %s/frame_0000.bmp through frame_0239.bmp (no audio track)\n", folder);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 2) { puts("Usage: boot_intro_preview.exe [existing-output-directory]"); return 1; }
    LoadTranslations();
    SetUiLanguage(LANGUAGE_KOREAN);
    CreateRenderFonts();
    int result = RenderPreview(argc == 2 ? argv[1] : ".");
    ResetPresentation();
    FxSnapshotDestroy();
    DestroyBootFilm();
    DestroyRenderFonts();
    return result;
}
