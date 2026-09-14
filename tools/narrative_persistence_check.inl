// Included by narrative_check.cpp after NFail. These tests call the desktop's
// actual persistence path, using only the isolated narrative-qa output folder.
struct NarrativeSaveFixtureGuard {
    wchar_t paths[2][MAX_PATH];
    uint8_t original[2][4096];
    DWORD lengths[2], attributes[2];
    bool active;
    NarrativeSaveFixtureGuard() : active(false) { ZeroMemory(paths, sizeof(paths)); }
    bool Prepare() {
        wchar_t module[MAX_PATH];
        DWORD length = GetModuleFileNameW(0, module, MAX_PATH);
        if (!length || length >= MAX_PATH) return false;
        wchar_t* executable = wcsrchr(module, L'\\');
        if (!executable) return false;
        *executable = 0;
        wchar_t* directory = wcsrchr(module, L'\\');
        if (!directory || lstrcmpiW(directory + 1, L"narrative-qa")) return false;
        const wchar_t* names[2] = {L"\\AROGUE.SAV", L"\\AROGUE.CDX"};
        for (int i = 0; i < 2; ++i) {
            if (lstrlenW(module) + lstrlenW(names[i]) >= MAX_PATH) return false;
            lstrcpyW(paths[i], module); lstrcatW(paths[i], names[i]);
            attributes[i] = GetFileAttributesW(paths[i]); lengths[i] = 0;
            if (attributes[i] == INVALID_FILE_ATTRIBUTES) {
                if (GetLastError() != ERROR_FILE_NOT_FOUND) return false;
                continue;
            }
            if (attributes[i] & FILE_ATTRIBUTE_DIRECTORY) return false;
            HANDLE file = CreateFileW(paths[i], GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            if (file == INVALID_HANDLE_VALUE) return false;
            DWORD size = GetFileSize(file, 0), read = 0;
            bool ok = size <= sizeof(original[i]) && ReadFile(file, original[i], size, &read, 0) && read == size;
            CloseHandle(file);
            if (!ok) return false;
            lengths[i] = size;
        }
        active = true;
        for (int i = 0; i < 2; ++i)
            if (attributes[i] != INVALID_FILE_ATTRIBUTES && !SetFileAttributesW(paths[i], FILE_ATTRIBUTE_NORMAL)) return false;
        return true;
    }
    bool Restore() {
        if (!active) return true;
        bool ok = true;
        for (int i = 0; i < 2; ++i) {
            DWORD current = GetFileAttributesW(paths[i]);
            if (current != INVALID_FILE_ATTRIBUTES && !SetFileAttributesW(paths[i], FILE_ATTRIBUTE_NORMAL)) ok = false;
            if (attributes[i] == INVALID_FILE_ATTRIBUTES) {
                if (current != INVALID_FILE_ATTRIBUTES && !DeleteFileW(paths[i])) ok = false;
                continue;
            }
            HANDLE file = CreateFileW(paths[i], GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if (file == INVALID_HANDLE_VALUE) { ok = false; continue; }
            DWORD written = 0;
            if (!WriteFile(file, original[i], lengths[i], &written, 0) || written != lengths[i]) ok = false;
            if (!CloseHandle(file)) ok = false;
            if (!SetFileAttributesW(paths[i], attributes[i])) ok = false;
        }
        active = false;
        return ok;
    }
    ~NarrativeSaveFixtureGuard() {
        if (!Restore()) printf("FAIL narrative UI: could not restore isolated persistence fixtures\n");
    }
};

struct NarrativePersistenceStateGuard {
    GameState game, practice;
    CampaignState campaign;
    uint8_t codex[ENEMY_KIND_COUNT];
    int corrupt, failed, campaignPending, codexPending, practiceActive, rolled;
    NarrativePersistenceStateGuard() {
        game = gGame; practice = gTutorialPracticeBackup; campaign = gCampaign;
        CopyMemory(codex, gCodex, sizeof(codex));
        corrupt = gCampaignCorrupt; failed = gSaveFailed;
        campaignPending = gCampaignSavePending; codexPending = gCodexSavePending;
        practiceActive = gTutorialPracticeActive; rolled = gRolled;
    }
    ~NarrativePersistenceStateGuard() {
        gGame = game; gTutorialPracticeBackup = practice; gCampaign = campaign;
        CopyMemory(gCodex, codex, sizeof(codex));
        gCampaignCorrupt = corrupt; gSaveFailed = failed;
        gCampaignSavePending = campaignPending; gCodexSavePending = codexPending;
        gTutorialPracticeActive = practiceActive; gRolled = rolled;
    }
};

static int CheckNarrativePersistenceUi() {
    if (gWindow) return NFail("persistence fixtures require the headless harness state");
    NarrativePersistenceStateGuard stateGuard;
    NarrativeSaveFixtureGuard files;
    if (!files.Prepare()) return NFail("persistence fixtures must run inside narrative-qa and preserve prior files");
    ResetPresentation();
    gCampaignCorrupt = gSaveFailed = gCampaignSavePending = gCodexSavePending = gTutorialPracticeActive = 0;
    InitCampaign(&gCampaign); ZeroMemory(gCodex, sizeof(gCodex));
    if (!SaveCampaign(&gCampaign) || !SaveCodex(gCodex, ENEMY_KIND_COUNT)) return NFail("initial persistence fixtures");
    NarrativeProgress progress = {}; progress.introSeen = progress.tutorialSeen = 1;
    lstrcpyW(progress.playerName, L"민서");
    NewRun(&gGame, 710u, 0); AttachNarrative(&gGame, &progress);

    // The first write fails after in-memory flags have already been merged.
    // An unchanged second notification must retry the pending disk write.
    if (!SetFileAttributesW(files.paths[0], FILE_ATTRIBUTE_READONLY)) return NFail("campaign read-only fixture");
    gGame.clearedMask = 1;
    PersistCampaignProgress();
    CampaignState disk;
    if (!gCampaignSavePending || !gSaveFailed || CampaignClearedMask(&gCampaign) != 1
        || !LoadCampaign(&disk) || CampaignClearedMask(&disk)) return NFail("failed campaign save must remain pending and preserve disk");
    if (!SetFileAttributesW(files.paths[0], FILE_ATTRIBUTE_NORMAL)) return NFail("release campaign write failure");
    PersistCampaignProgress();
    if (gCampaignSavePending || gSaveFailed || !LoadCampaign(&disk) || CampaignClearedMask(&disk) != 1
        || wcscmp(disk.narrative.playerName, L"민서")) return NFail("campaign save must retry without a new clear or story flag");

    if (!SetFileAttributesW(files.paths[1], FILE_ATTRIBUTE_READONLY)) return NFail("codex read-only fixture");
    gGame.enemyScanned[0] = 1;
    PersistCampaignProgress();
    uint8_t diskCodex[ENEMY_KIND_COUNT];
    if (!gCodexSavePending || !gSaveFailed || !gCodex[0] || !LoadCodex(diskCodex, ENEMY_KIND_COUNT) || diskCodex[0])
        return NFail("failed codex discovery must remain pending while the previous file survives");
    // A different file's successful save cannot conceal a pending codex failure.
    gGame.narrative.logsSeen = 1;
    PersistCampaignProgress();
    if (gCampaignSavePending || !gCodexSavePending || !gSaveFailed || !LoadCampaign(&disk)
        || disk.narrative.logsSeen != 1) return NFail("campaign success must not hide an outstanding codex save failure");
    if (!SetFileAttributesW(files.paths[1], FILE_ATTRIBUTE_NORMAL)) return NFail("release codex write failure");
    PersistCampaignProgress();
    if (gCodexSavePending || gSaveFailed || !LoadCodex(diskCodex, ENEMY_KIND_COUNT) || !diskCodex[0])
        return NFail("codex save must retry without another new discovery");

    // Reset while practicing used to retain the pretraining backup. The next
    // start briefly restored it and could write old clears back during boot.
    gGame.narrative.milestoneSeen = gGame.narrative.shardSeen = 1;
    gGame.narrative.bossSeen = 7;
    PersistCampaignProgress();
    BeginTutorialReplay();
    if (!gTutorialPracticeActive || !gGame.tutorial.active || gTutorialPracticeBackup.clearedMask != 1)
        return NFail("practice reset fixture must contain a real prior campaign backup");
    ResetCampaignProgress();
    if (gTutorialPracticeActive || gTutorialPracticeBackup.clearedMask || gTutorialPracticeBackup.narrative.playerName[0]
        || gGame.phase != PHASE_TITLE || gGame.clearedMask || CampaignClearedMask(&gCampaign)
        || gCampaignSavePending || !LoadCampaign(&disk) || CampaignClearedMask(&disk))
        return NFail("successful reset must discard practice and its recoverable campaign backup");
    BeginNewRun();
    PersistCampaignProgress(); // Same checkpoint the closing window would run.
    if (gGame.clearedMask || CampaignClearedMask(&gCampaign) || !LoadCampaign(&disk) || CampaignClearedMask(&disk)
        || disk.narrative.playerName[0]) return NFail("starting after practice reset must not resurrect clears during boot");
    ClickCenter(StoryNextRect(BASE_WIDTH, BASE_HEIGHT));
    if (gGame.phase != PHASE_STORY || gGame.story.kind != STORY_INTRO || gGame.story.fragment != 0)
        return NFail("skipping entrance cinematic must not also dismiss the first dialogue");
    if (gGame.clearedMask || gGame.narrative.playerName[0] || !gCodex[0])
        return NFail("reset must begin fresh onboarding while retaining the independent codex");
    ResetPresentation();
    if (!files.Restore()) return NFail("restore persistence fixture bytes and attributes");
    printf("PASS narrative UI: campaign/codex failure retries, independent failure indicators, practice reset cannot restore old progress.\n");
    return 0;
}
