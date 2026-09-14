// Native edit control keeps Korean IME composition and normal clipboard editing.
// The rest of the UI remains on the game's logical canvas.
static LRESULT CALLBACK NarrativeNameProcedure(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_IME_STARTCOMPOSITION) gNarrativeNameComposing = 1;
    if (message == WM_IME_ENDCOMPOSITION) gNarrativeNameComposing = 0;
    if (message == WM_KEYDOWN && wParam == VK_RETURN && !gNarrativeNameComposing) {
        PostMessageW(gWindow, NARRATIVE_CONFIRM_MESSAGE, 0, 0);
        return 0;
    }
    if (message == WM_CHAR && (wParam == L'\r' || wParam == L'\n')) return 0;
    if (message == WM_KEYDOWN && (wParam == VK_F1 || wParam == VK_F2 || wParam == VK_F3)) {
        HandleKey(wParam); SyncNarrativeControls(); return 0;
    }
    return CallWindowProcW(gNarrativeNameEditProc, edit, message, wParam, lParam);
}

static void SyncNarrativeControls() {
    if (!gWindow || !IsWindow(gWindow)) return;
    int visible = gGame.phase == PHASE_NAME_ENTRY && !gSettingsOpen && !gGuideOpen && !gDeckOpen && !gBootActive;
    if (gGame.phase != PHASE_NAME_ENTRY) gNarrativeNameSession = 0;
    if (!visible) {
        if (gNarrativeNameEdit && IsWindowVisible(gNarrativeNameEdit)) {
            ShowWindow(gNarrativeNameEdit, SW_HIDE);
            if (GetFocus() == gNarrativeNameEdit) SetFocus(gWindow);
        }
        return;
    }
    if (!gNarrativeNameEdit) {
        gNarrativeNameEdit = CreateWindowExW(0, L"EDIT", gGame.narrative.playerName,
            WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 1, 1, gWindow, (HMENU)1201,
            (HINSTANCE)GetWindowLongPtrW(gWindow, GWLP_HINSTANCE), 0);
        if (!gNarrativeNameEdit) return;
        SendMessageW(gNarrativeNameEdit, EM_SETLIMITTEXT, NARRATIVE_NAME_MAX, 0);
        gNarrativeNameEditProc = (WNDPROC)SetWindowLongPtrW(gNarrativeNameEdit, GWLP_WNDPROC, (LONG_PTR)NarrativeNameProcedure);
    }
    if (!gNarrativeNameSession) {
        SetWindowTextW(gNarrativeNameEdit, gGame.narrative.playerName);
        gNarrativeNameSession = 1;
        gNarrativeNameComposing = 0;
    }
    RECT client; GetClientRect(gWindow, &client);
    float scale; int ox, oy;
    ComputeCanvasTransform(client.right, client.bottom, &scale, &ox, &oy);
    if (scale <= 0) return;
    RECT logical = NarrativeNameRect();
    RECT placed = {(LONG)(ox + (logical.left + 8) * scale), (LONG)(oy + (logical.top + 6) * scale),
        (LONG)(ox + (logical.right - 8) * scale), (LONG)(oy + (logical.bottom - 6) * scale)};
    RECT old; GetWindowRect(gNarrativeNameEdit, &old);
    MapWindowPoints(HWND_DESKTOP, gWindow, (POINT*)&old, 2);
    if (!EqualRect(&old, &placed))
        SetWindowPos(gNarrativeNameEdit, HWND_TOP, placed.left, placed.top,
            placed.right - placed.left, placed.bottom - placed.top, SWP_NOACTIVATE);
    int fontHeight = (int)(24 * scale); if (fontHeight < 10) fontHeight = 10;
    if (fontHeight != gNarrativeNameFontHeight) {
        HFONT next = CreateFontW(-fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
        if (next) {
            SendMessageW(gNarrativeNameEdit, WM_SETFONT, (WPARAM)next, TRUE);
            if (gNarrativeNameFont) DeleteObject(gNarrativeNameFont);
            gNarrativeNameFont = next; gNarrativeNameFontHeight = fontHeight;
        }
    }
    if (!IsWindowVisible(gNarrativeNameEdit)) {
        ShowWindow(gNarrativeNameEdit, SW_SHOW); SetFocus(gNarrativeNameEdit);
    }
}

static void ConfirmNarrativeName() {
    if (gGame.phase != PHASE_NAME_ENTRY || gNarrativeNameComposing) return;
    wchar_t name[NARRATIVE_NAME_MAX + 1] = {};
    if (gNarrativeNameEdit) GetWindowTextW(gNarrativeNameEdit, name, NARRATIVE_NAME_MAX + 1);
    if (!SubmitNarrativeName(&gGame, name)) {
        PlaySfx(SFX_UI_CLICK);
        if (gNarrativeNameEdit) SetFocus(gNarrativeNameEdit);
        return;
    }
    PersistCampaignProgress();
    SyncNarrativeControls(); PlaySfx(SFX_CONFIRM);
    SetFocus(gWindow); InvalidateRect(gWindow, 0, FALSE);
}

static void BeginTutorialReplay() {
    if (gTutorialPracticeActive || gGame.tutorial.active) {
        gSettingsOpen = 0; return;
    }
    PersistCampaignProgress();
    FinishUiFx();
    gTutorialPracticeBackup = gGame;
    gTutorialPracticeRolled = gRolled;
    gTutorialPracticeActive = 1;
    gSettingsOpen = gGuideOpen = gDeckOpen = 0;
    NewRun(&gGame, 0x524F4755u, CampaignClearedMask(&gCampaign));
    AttachNarrative(&gGame, &gCampaign.narrative);
    BeginTutorial(&gGame);
    gRollTurn = -1; gKeyboardFocus = -1;
    SyncRollAnimation(); SyncNarrativeControls();
    PlaySfx(SFX_CONFIRM); InvalidateRect(gWindow, 0, FALSE);
}

static void FinishTutorialUi(int skip) {
    if (!gGame.tutorial.active) return;
    if (!skip && gGame.tutorial.step != TUTORIAL_COMPLETE) return;
    FinishUiFx();
    if (gReadActive) { gReadActive = 0; KillTimer(gWindow, 1); }
    if (skip) SkipTutorial(&gGame); else FinishTutorial(&gGame);
    if (gTutorialPracticeActive) {
        uint8_t seen = gGame.narrative.tutorialSeen;
        gGame = gTutorialPracticeBackup;
        gGame.narrative.tutorialSeen |= seen;
        gTutorialPracticeActive = 0;
        gRolled = gTutorialPracticeRolled;
        gRollFloor = gGame.floor; gRollEncounter = gGame.encounter; gRollTurn = gGame.turn;
    } else {
        SyncRollAnimation();
    }
    gKeyboardFocus = -1;
    PersistCampaignProgress(); SyncNarrativeControls();
    PlaySfx(SFX_CONFIRM); InvalidateRect(gWindow, 0, FALSE);
}

static void AdvanceTutorialUi() {
    if (!gGame.tutorial.active) return;
    if (gGame.tutorial.step == TUTORIAL_PREVIEW) {
        AcknowledgeTutorialPreview(&gGame); PlaySfx(SFX_UI_CLICK);
    } else if (gGame.tutorial.step == TUTORIAL_COMPLETE) {
        FinishTutorialUi(0);
    }
    InvalidateRect(gWindow, 0, FALSE);
}
