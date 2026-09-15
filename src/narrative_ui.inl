// A native EDIT control keeps the name text, selection and clipboard editing,
// but the game draws the box (DrawNarrativeName). The control itself is a
// caret-less 1px window: left visible, its white field and the IME's own
// composition window showed over the canvas and broke Korean glyphs.
static LRESULT CALLBACK NarrativeNameProcedure(HWND edit, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_IME_SETCONTEXT:
        lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;   // composition is drawn in the name box
        break;
    case WM_IME_STARTCOMPOSITION:
        gNarrativeNameComposing = 1; gNarrativeNameComposition[0] = 0;
        return 0;
    case WM_IME_COMPOSITION: {
        HIMC context = ImmGetContext(edit);
        if (context) {
            if (lParam & GCS_RESULTSTR) {
                wchar_t result[32];
                LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, result, sizeof(result) - sizeof(wchar_t));
                result[bytes > 0 ? bytes / sizeof(wchar_t) : 0] = 0;
                if (result[0]) SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)result);
            }
            LONG bytes = lParam & GCS_COMPSTR ? ImmGetCompositionStringW(context, GCS_COMPSTR,
                gNarrativeNameComposition, sizeof(gNarrativeNameComposition) - sizeof(wchar_t)) : 0;
            gNarrativeNameComposition[bytes > 0 ? bytes / sizeof(wchar_t) : 0] = 0;
            ImmReleaseContext(edit, context);
        }
        InvalidateRect(gWindow, 0, FALSE);
        return 0;
    }
    case WM_IME_ENDCOMPOSITION:
        gNarrativeNameComposing = 0; gNarrativeNameComposition[0] = 0;
        InvalidateRect(gWindow, 0, FALSE);
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_RETURN && !gNarrativeNameComposing) {
        PostMessageW(gWindow, NARRATIVE_CONFIRM_MESSAGE, 0, 0);
        return 0;
    }
    if (message == WM_CHAR && (wParam == L'\r' || wParam == L'\n')) return 0;
    if (message == WM_KEYDOWN && (wParam == VK_F1 || wParam == VK_F2 || wParam == VK_F3)) {
        HandleKey(wParam); SyncNarrativeControls(); return 0;
    }
    LRESULT result = CallWindowProcW(gNarrativeNameEditProc, edit, message, wParam, lParam);
    if (message == WM_SETFOCUS) HideCaret(edit);   // the name box draws its own caret
    if (message == WM_KEYDOWN || message == WM_CHAR || message == WM_PASTE || message == WM_CUT || message == WM_UNDO)
        InvalidateRect(gWindow, 0, FALSE);
    return result;
}

void ReadNarrativeNameInput(NarrativeNameInput* out) {
    ZeroMemory(out, sizeof(*out));
    if (!gNarrativeNameEdit) {
        lstrcpynW(out->text, gGame.narrative.playerName, NARRATIVE_NAME_MAX + 1);
        out->selStart = out->selEnd = lstrlenW(out->text);
        return;
    }
    GetWindowTextW(gNarrativeNameEdit, out->text, NARRATIVE_NAME_MAX + 1);
    DWORD start = 0, end = 0;
    SendMessageW(gNarrativeNameEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    out->selStart = (int)start; out->selEnd = (int)end;
    lstrcpynW(out->composition, gNarrativeNameComposition, 16);
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
        gNarrativeNameComposing = 0; gNarrativeNameComposition[0] = 0;
    }
    // Keep the 1px control at the start of the name box so an IME candidate
    // list (Hanja) still opens next to it.
    RECT client; GetClientRect(gWindow, &client);
    float scale; int ox, oy;
    ComputeCanvasTransform(client.right, client.bottom, &scale, &ox, &oy);
    if (scale <= 0) return;
    RECT logical = NarrativeNameRect();
    int x = (int)(ox + (logical.left + 16) * scale), y = (int)(oy + (logical.top + logical.bottom) / 2 * scale);
    RECT old; GetWindowRect(gNarrativeNameEdit, &old);
    MapWindowPoints(HWND_DESKTOP, gWindow, (POINT*)&old, 2);
    if (old.left != x || old.top != y)
        SetWindowPos(gNarrativeNameEdit, HWND_TOP, x, y, 1, 1, SWP_NOACTIVATE);
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
