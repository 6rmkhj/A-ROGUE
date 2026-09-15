// Authored narrative presentation. This file is included by screens.cpp after
// the ordinary board layout; every animation is visual only and FX OFF retains
// all dialogue, stage labels, costs and actionable control highlights.
RECT NarrativeNameRect() { return MakeRect(436, 391, 916, 443); }
RECT NarrativeNameConfirmRect() { return MakeRect(546, 520, 806, 566); }
RECT TutorialNextRect() { return MakeRect(1040, 720, 1180, 755); }
RECT TutorialSkipRect() { return MakeRect(1190, 720, 1324, 755); }
RECT TutorialReplayRect() { return MakeRect(830, 164, 1238, 206); }

static const wchar_t* NarrativeName() {
    return gGame.narrative.playerName[0] ? gGame.narrative.playerName : LocalizeText(L"나");
}

static void NarrativeText(const wchar_t* source, wchar_t* out, int capacity) {
    // Translation must happen before player input is inserted. In particular a
    // Korean name is never passed back through the translation lookup.
    source = LocalizeText(source ? source : L"");
    const wchar_t* name = NarrativeName();
    int used = 0;
    if ((source[0] == L'나' && source[1] == L':') ||
        (source[0] == L'M' && source[1] == L'e' && source[2] == L':')) {
        for (int i = 0; name[i] && used + 1 < capacity; ++i) out[used++] = name[i];
        source += source[0] == L'나' ? 1 : 2;
    }
    for (int i = 0; source[i] && used + 1 < capacity;) {
        int token = source[i] == L'[' && source[i + 1] == L'이' && source[i + 2] == L'름' && source[i + 3] == L']' ? 4 : 0;
        if (!token && source[i] == L'[' && source[i + 1] == L'N' && source[i + 2] == L'A' &&
            source[i + 3] == L'M' && source[i + 4] == L'E' && source[i + 5] == L']') token = 6;
        if (token) {
            for (int n = 0; name[n] && used + 1 < capacity; ++n) out[used++] = name[n];
            i += token;
        } else out[used++] = source[i++];
    }
    out[used] = 0;
}

static int NarrativeParagraph(HDC dc, const RECT& rect, const wchar_t* text, COLORREF color, HFONT font, int measure) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    wchar_t wrapped[1024];
    const wchar_t* value = WrapAtSpaces(dc, text, rect.right - rect.left, wrapped, 1024, 1);
    RECT draw = rect;
    if (measure) draw.bottom = draw.top;
    DrawTextW(dc, value, -1, &draw, DT_WORDBREAK | DT_NOPREFIX | (measure ? DT_CALCRECT : 0));
    SelectObject(dc, old);
    return draw.bottom - draw.top;
}

// ROGUE in the enemy sprites' own 16x16 language (see sprites.h): a hood whose
// bent tip carries the call signal, two lit eyes in the dark opening, and
// three lights on the cloak (the number ROGUE comes to like). The accent is
// the base colour, shaded exactly like an enemy's own colour.
#define ROGUE_SIZE 16
static const char* const ROGUE_SPRITE[ROGUE_SIZE] = {
    "...........XX...",
    ".........XXeX...",
    ".......XX44X....",
    "......X5443X....",
    ".....X544443X...",
    "...X54444443X...",
    "...X54XXXX43X...",
    "..X54XooooX43X..",
    "..X4XoeooeoX3X..",
    "..X4XoeooeoX3X..",
    "..X44XooooX33X..",
    ".X544XXXXXX433X.",
    "X54444333333322X",
    "X4443e3ee3e3222X",
    "X33332222222211X",
    "XXXXXXXXXXXXXXXX",
};

static void DrawRoguePortrait(HDC dc, RECT rect, int restored, int corrupted, COLORREF accent, int talking) {
    int unit = rect.right - rect.left < rect.bottom - rect.top ? rect.right - rect.left : rect.bottom - rect.top;
    unit /= ROGUE_SIZE;
    if (unit < 1) unit = 1;
    const int left = (rect.left + rect.right - ROGUE_SIZE * unit) / 2, top = (rect.top + rect.bottom - ROGUE_SIZE * unit) / 2;
    const int decor = FxDecorOn();
    const DWORD now = decor ? GetTickCount() : 0;
    // Eyes narrow now and then; a lit mouth flickers while ROGUE's line types.
    const int blink = decor && now % 3400 < 130, open = talking && decor && (now / 90) % 2;
    const int lights = restored / 2;   // one cloak light per two recovered volumes
    // Corruption eats in from the right along a ragged front. The eaten cells
    // keep their shade but in red, some drop out, and their rows slip.
    const int front = ROGUE_SIZE - corrupted * 4 / 3;
    const COLORREF rot = accent == C_RED ? RGB(130, 28, 40) : C_RED;   // still readable on a red ROGUE
    for (int y = 0; y < ROGUE_SIZE; ++y) {
        const int edge = corrupted > 0 ? front + (int)(Hash3(y, 0, 77) % 3u) - 1 : ROGUE_SIZE;
        const int slip = corrupted > 0 && decor && Hash3(y, (int)(now / 140), 5) % 9u < (uint32_t)corrupted
            ? ((int)(Hash3(y, (int)(now / 140), 6) % 3u) - 1) * unit / 2 : 0;
        for (int x = 0; x < ROGUE_SIZE; ++x) {
            char c = ROGUE_SPRITE[y][x];
            if (blink && y == 8 && c == 'e') c = 'o';
            if (open && y == 10 && (x == 7 || x == 8)) c = 'e';
            if (y == 13 && c == 'e' && (x == 5 ? 0 : x == 10 ? 2 : 1) >= lights) c = '2';
            COLORREF tone;
            int shift = 0;
            if (x >= edge) {
                uint32_t h = Hash3(x, y, (int)(now / 160)) % 10u;
                if (h == 0) continue;
                if (!SpriteCellColor(h == 1 && c != '.' ? '5' : c, rot, &tone)) continue;
                shift = slip;
            } else if (!SpriteCellColor(c, accent, &tone)) continue;
            Fill(dc, MakeRect(left + x * unit + shift, top + y * unit, left + (x + 1) * unit + shift, top + (y + 1) * unit), tone);
        }
    }
}

static void DrawNarrativeDiagram(HDC dc, RECT r, int count, int elapsed, int talking) {
    int kind = gGame.story.kind;
    int milestone = kind == STORY_MILESTONE ? gGame.story.fragment + 1 : count;
    int terminal = kind >= STORY_ENDING_RESTORE && kind <= STORY_ENDING_MERGE;
    int antagonist = (kind == STORY_BOSS && gGame.story.drive == 6) || kind == STORY_A_ENTRY || kind == STORY_A_GREETING;
    int revelation = kind == STORY_MILESTONE && milestone == 6;
    COLORREF accent = revelation || antagonist ? C_RED : terminal && gGame.story.selectedEnding == 2 ? C_YELLOW : C_BLUE;
    Panel(dc, r, RGB(7, 15, 23), MixColor(C_LINE, accent, 40));
    if (FxDecorOn()) {
        int sweep = (elapsed / 4) % (r.bottom - r.top);
        Fill(dc, MakeRect(r.left + 1, r.top + sweep, r.right - 1, r.top + sweep + 1), MixColor(C_BG, accent, 15));
    }
    const wchar_t* label = kind == STORY_LOGS || gGame.story.replay ? L"회수된 기록 / 읽기 전용" : kind == STORY_INTRO ? L"접속 / 첫 만남" : terminal ? L"마지막 명령" : revelation ? L"접근 권한 / 전체" : antagonist ? L"ROGUE / 응답 충돌" : L"ROGUE / 복구 동행";
    TextRect(dc, MakeRect(r.left + 14, r.top + 18, r.right - 14, r.top + 46), label, accent, gFontSmall, DT_CENTER | DT_SINGLELINE);

    if (terminal && gGame.story.selectedEnding == 2) {
        // The real room: one monitor and one chair, then a very small surviving
        // record. No fabricated helper hologram after EXIT.
        RECT monitor = MakeRect(r.left + 46, r.top + 112, r.right - 46, r.top + 242);
        Panel(dc, monitor, RGB(12, 17, 20), C_DIM);
        Fill(dc, MakeRect(monitor.left + 12, monitor.top + 12, monitor.right - 12, monitor.bottom - 12), RGB(3, 5, 7));
        Fill(dc, MakeRect((r.left + r.right) / 2 - 4, monitor.bottom, (r.left + r.right) / 2 + 4, monitor.bottom + 23), C_DIM);
        DrawLine(dc, r.left + 24, monitor.bottom + 25, r.right - 24, monitor.bottom + 25, C_DIM, 2);
        const wchar_t* record = gGame.story.page ? L"로그.\n좋아하는 숫자는 3.\n\n_" : L"연결 종료\n\n현재 기억 / 유지";
        TextRect(dc, MakeRect(monitor.left + 20, monitor.top + 30, monitor.right - 16, monitor.bottom - 14), record, C_YELLOW, gFontSmall, DT_WORDBREAK);
        TextRect(dc, MakeRect(r.left + 20, r.top + 298, r.right - 20, r.bottom - 22), L"함께한 시간은\n없었던 일이 되지 않는다.", C_TEXT, gFontSmall, DT_CENTER | DT_WORDBREAK);
        return;
    }
    if (kind == STORY_MILESTONE && milestone == 5) {
        // The attempted solution is visible before its failure: a live copy
        // carries the same red stream; a clean copy has no current memory.
        RECT live = MakeRect(r.left + 24, r.top + 84, r.right - 24, r.top + 212);
        RECT clean = MakeRect(r.left + 24, r.top + 246, r.right - 24, r.top + 374);
        Panel(dc, live, C_PANEL, C_RED); Panel(dc, clean, C_PANEL, C_BLUE);
        TextRect(dc, MakeRect(live.left + 12, live.top + 12, live.right - 12, live.top + 40), L"현재 실행체 / 복사", C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(clean.left + 12, clean.top + 12, clean.right - 12, clean.top + 40), L"깨끗한 데이터 / 추출", C_TEXT, gFontSmall, DT_CENTER | DT_SINGLELINE);
        for (int i = 0; i < 9; ++i) {
            int x = live.left + 16 + i * 26;
            Fill(dc, MakeRect(x, live.top + 63, x + 18, live.top + 85), i % 3 ? C_BLUE : C_RED);
            Fill(dc, MakeRect(x, clean.top + 63, x + 18, clean.top + 85), i % 3 ? C_LINE : C_BLUE);
        }
        TextRect(dc, MakeRect(live.left + 12, live.bottom - 31, live.right - 12, live.bottom - 8), L"기억과 잠식 / 함께 이동", C_RED, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(clean.left + 12, clean.bottom - 31, clean.right - 12, clean.bottom - 8), L"동행의 기억 / 누락", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        TextRect(dc, MakeRect(r.left + 18, r.bottom - 82, r.right - 18, r.bottom - 20), L"분리만으로는\n지금의 로그를 구할 수 없다.", C_YELLOW, gFontSmall, DT_CENTER | DT_WORDBREAK);
        return;
    }
    if (revelation && gGame.story.page < 2) {
        if (!gGame.story.page) {
            RECT origin = MakeRect(r.left + 15, r.top + 86, r.left + 160, r.top + 268);
            RECT backup = MakeRect(r.left + 170, r.top + 86, r.right - 15, r.top + 268);
            DrawRoguePortrait(dc, origin, 6, 0, C_BLUE);
            DrawRoguePortrait(dc, backup, 6, 4, C_RED);
            TextRect(dc, MakeRect(origin.left, origin.bottom, origin.right, origin.bottom + 32), L"현재 사용자", C_BLUE, gFontSmall, DT_CENTER | DT_SINGLELINE);
            TextRect(dc, MakeRect(backup.left, backup.bottom, backup.right, backup.bottom + 32), L"ROGUE", C_RED, gFontSmall, DT_CENTER | DT_SINGLELINE);
            DrawLine(dc, origin.left + 70, origin.bottom + 52, backup.left + 70, backup.bottom + 52, C_YELLOW, 2);
            TextRect(dc, MakeRect(r.left + 14, r.top + 350, r.right - 14, r.bottom - 26), L"인격 원본 / 일치\n정책 작성자 / 현재 사용자", C_YELLOW, gFontSmall, DT_CENTER | DT_WORDBREAK);
        } else {
            for (int i = 0; i < 5; ++i) {
                RECT test = MakeRect(r.left + 25 + i * 3, r.top + 82 + i * 57, r.right - 25 + i * 3, r.top + 126 + i * 57);
                Panel(dc, test, C_PANEL, MixColor(C_LINE, C_RED, 25 + i * 13));
                TextRect(dc, test, L"AUTOMATED TEST  /  FAILED", C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            TextRect(dc, MakeRect(r.left + 18, r.bottom - 89, r.right - 18, r.bottom - 20), L"당신이 접속하기 전의 기록\n실패한 백업이 남긴 목소리", C_TEXT, gFontSmall, DT_CENTER | DT_WORDBREAK);
        }
        return;
    }
    if (kind == STORY_A_ENTRY) {
        int cx = (r.left + r.right) / 2;
        for (int i = 0; i < 5; ++i) {
            int inset = 28 + i * 23;
            Outline(dc, MakeRect(r.left + inset, r.top + 80 + i * 21, r.right - inset, r.bottom - 106 - i * 21), MixColor(C_PANEL, C_RED, 65 - i * 10), 2);
        }
        Fill(dc, MakeRect(cx - 2, r.top + 170, cx + 2, r.bottom - 146), C_RED);
        TextRect(dc, MakeRect(r.left + 10, r.bottom - 87, r.right - 10, r.bottom - 20), L"A:\\ROGUE\\SELF\n남겨진 안내 음성 / 기록 재생", C_TEXT, gFontSmall, DT_CENTER | DT_WORDBREAK);
        return;
    }
    int corruption = antagonist || revelation ? 6 : count > 0 ? count - 1 : 0;
    if (terminal && gGame.story.selectedEnding == 0) corruption = 0;
    RECT portrait = MakeRect(r.left + 24, r.top + 52, r.right - 24, r.top + 292);
    DrawRoguePortrait(dc, portrait, milestone, corruption, accent, talking);
    if (kind == STORY_A_GREETING || (kind == STORY_INTRO && !gGame.story.fragment) || (terminal && !gGame.story.selectedEnding)) {
        RECT welcome = MakeRect(r.left + 8, r.top + 294, r.right - 8, r.top + 340);
        Fill(dc, welcome, RGB(2, 5, 8));
        TextRect(dc, welcome, L"환영합니다. 마스터.", C_GREEN, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (kind == STORY_SHARD || kind == STORY_MILESTONE) {
        // Two distinct streams share a destination: intact memory and residual
        // contamination. Neither stream overwrites a line of dialogue.
        for (int i = 0; i < 6; ++i) {
            int x = r.left + 18 + i * (r.right - r.left - 36) / 6;
            COLORREF volume = (COLORREF)DRIVE_INFO[i].color;
            TextRect(dc, MakeRect(x, r.bottom - 104, x + 40, r.bottom - 78), DRIVE_INFO[i].letter,
                gGame.clearedMask & (1u << i) ? volume : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
            if ((gGame.clearedMask & (1u << i)) && FxDecorOn()) {
                int progress = (elapsed + i * 130) % 1400;
                int px = Lerp(x + 20, (r.left + r.right) / 2, progress * 1000 / 1400);
                int py = Lerp(r.bottom - 112, r.top + 240, progress * 1000 / 1400);
                Fill(dc, MakeRect(px, py, px + 4, py + 4), (i & 1) ? C_RED : volume);
            }
        }
    }
    const wchar_t* state = kind == STORY_LOGS || gGame.story.replay ? L"남겨진 기록을 읽는 중\n현재의 음성 연결이 아닙니다."
        : kind == STORY_INTRO ? (gGame.story.fragment ? L"안내자 식별 / ROGUE" : L"……이번에는.")
        : revelation ? (gGame.story.page == 0 ? L"사용자 서명 = 인격 원본" : gGame.story.page == 1 ? L"실패한 자동 시험 / 기록 연결" : L"기억 연결 완료\n상충하는 명령 / 한 실행체")
        : milestone == 5 && kind == STORY_MILESTONE ? L"분리 시험 / 실패\n현재의 기억도 함께 이동한다"
        : terminal && gGame.story.selectedEnding == 0 ? L"기준 상태 / 복원\n자동 재시작 / 유지"
        : terminal ? L"연결 / 유지\n불안정 / 미해결"
        : antagonist ? L"동일한 서명\n서로 다른 선택"
        : L"기억은 연결되고\n오류는 이곳에 남는다.";
    TextRect(dc, MakeRect(r.left + 18, r.bottom - 68, r.right - 18, r.bottom - 14), state, C_TEXT, gFontSmall, DT_CENTER | DT_WORDBREAK);
}

// ---- Story cutscene ----------------------------------------------------------
// A card opens one line per input. Every line is measured at its final width
// before any is shown, so typing and later lines never move text on screen.
#define STORY_LINE_WIDTH (BASE_WIDTH - 64 - 416 - 74)
#define STORY_LINE_GAP 15
#define STORY_LINE_SPACE 322
#define STORY_CHAR_MS 34
#define STORY_TYPE_MAX_MS 1500

struct StoryLineView { wchar_t text[640]; int speaker; int who; };

// Non-empty lines of the current card, with the "name:" prefix length and
// who is speaking. Detection runs on the translated text, so English
// "ROGUE:" and Korean "로그:" colour alike.
static int StoryLines(StoryLineView* out) {
    const StoryFragment* story = CurrentStoryFragment(&gGame);
    if (!story) return 0;
    const wchar_t* source[5] = { story->line1, story->line2, story->line3, story->line4, story->line5 };
    int count = 0;
    for (int i = 0; i < 5; ++i) {
        StoryLineView* line = out + count;
        NarrativeText(source[i], line->text, 640);
        if (!line->text[0]) continue;
        const wchar_t* localized = LocalizeText(source[i]);
        line->who = STORY_WHO_NARRATION; line->speaker = 0;
        if ((localized[0] == L'나' && localized[1] == L':') ||
            (localized[0] == L'M' && localized[1] == L'e' && localized[2] == L':')) {
            line->who = STORY_WHO_PLAYER; line->speaker = lstrlenW(NarrativeName()) + 1;
        } else for (int c = 1; c < 18 && line->text[c]; ++c) if (line->text[c] == L':' && line->text[c + 1] == L' ') {
            wchar_t name[18]; lstrcpynW(name, line->text, c + 1);
            line->speaker = c + 1;
            line->who = !lstrcmpiW(name, LocalizeText(L"로그")) ? STORY_WHO_ROGUE
                : !lstrcmpiW(name, LocalizeText(L"시스템")) ? STORY_WHO_SYSTEM : STORY_WHO_OTHER;
            break;
        }
        ++count;
    }
    return count;
}

int StoryLineCount() { StoryLineView lines[5]; int count = StoryLines(lines); return count ? count : 1; }

int StoryLineTypeMs(int line) {
    StoryLineView lines[5];
    if (line < 0 || line >= StoryLines(lines)) return 0;
    int ms = (lstrlenW(lines[line].text) - lines[line].speaker) * STORY_CHAR_MS;
    return ms < STORY_TYPE_MAX_MS ? ms : STORY_TYPE_MAX_MS;
}

int StoryLineWho(int line) {
    StoryLineView lines[5];
    return line >= 0 && line < StoryLines(lines) ? lines[line].who : STORY_WHO_NARRATION;
}

static COLORREF StoryWhoColor(int who, COLORREF accent) {
    return who == STORY_WHO_ROGUE ? accent : who == STORY_WHO_PLAYER ? C_GREEN
        : who == STORY_WHO_SYSTEM ? C_RED : who == STORY_WHO_OTHER ? C_YELLOW : C_DIM;
}

// Shared with tools/narrative_check.cpp: the overflow check must measure the
// exact layout the screen draws.
static HFONT StoryDialogueLayout(HDC dc, const StoryLineView* lines, int count, int* rows, int* total) {
    HFONT font = gFontMedium;
    for (;;) {
        *total = 0;
        for (int i = 0; i < count; ++i) {
            rows[i] = NarrativeParagraph(dc, MakeRect(0, 0, STORY_LINE_WIDTH, 0), lines[i].text, C_TEXT, font, 1);
            *total += rows[i] + STORY_LINE_GAP;
        }
        if (*total <= STORY_LINE_SPACE || font == gFontSmall) return font;
        font = gFontSmall;
    }
}

// Draws the first `typed` characters. The speaker prefix is clipped out of the
// body pass and drawn alone in its own colour, so no pixel is painted twice.
// Returns the caret cell: after the last typed character, one text row tall.
static RECT DrawStoryLine(HDC dc, const RECT& rect, const StoryLineView& line, int typed, HFONT font, COLORREF body, COLORREF tag) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    wchar_t wrapped[1024];
    const wchar_t* value = WrapAtSpaces(dc, line.text, rect.right - rect.left, wrapped, 1024, 1);
    int length = lstrlenW(value);
    if (typed < length) length = typed;
    TEXTMETRICW metric; GetTextMetricsW(dc, &metric);
    SIZE name = {0, 0};
    if (line.speaker) GetTextExtentPoint32W(dc, value, line.speaker, &name);
    for (int pass = 0; pass < (line.speaker ? 2 : 1); ++pass) {
        int saved = SaveDC(dc);
        if (line.speaker && pass) IntersectClipRect(dc, rect.left, rect.top, rect.left + name.cx, rect.top + metric.tmHeight);
        else if (line.speaker) ExcludeClipRect(dc, rect.left, rect.top, rect.left + name.cx, rect.top + metric.tmHeight);
        SetTextColor(dc, pass ? tag : body);
        RECT draw = rect;
        DrawTextW(dc, value, length, &draw, DT_WORDBREAK | DT_NOPREFIX);
        RestoreDC(dc, saved);
    }
    int row = 0, start = 0;
    for (int i = 0; i < length; ++i) if (value[i] == L'\n') { ++row; start = i + 1; }
    SIZE tail = {0, 0};
    GetTextExtentPoint32W(dc, value + start, length - start, &tail);
    SelectObject(dc, old);
    int x = rect.left + tail.cx, y = rect.top + row * metric.tmHeight;
    return MakeRect(x, y, x, y + metric.tmHeight);
}

static void DrawStory(HDC dc, int width, int height) {
    const StoryFragment* story = CurrentStoryFragment(&gGame);
    if (!story) return;
    const int count = RecoveredShardCount(gGame.clearedMask);
    const int decor = FxDecorOn();
    const int scene = decor ? SceneElapsed() : 3000;
    COLORREF accent = gGame.story.kind == STORY_MILESTONE && gGame.story.fragment == 5 ? C_RED : C_BLUE;
    DrawSceneField(dc, PHASE_STORY, accent, width, height);

    StoryLineView lines[5];
    int total = StoryLines(lines), rows[5] = {}, used = 0;
    int shown = StoryShownLine();
    if (shown >= total) shown = total - 1;
    int clock = decor ? StoryLineElapsed() : 60000;
    int typing = shown >= 0 && clock < StoryLineTypeMs(shown);
    int who = shown >= 0 ? lines[shown].who : STORY_WHO_NARRATION;
    COLORREF voice = StoryWhoColor(who, accent);

    // Letterbox bars close in like a film frame when the scene first opens.
    int frame = decor && SceneArrivalMajor() ? EaseOutCubic(Track(scene, 0, 380)) : 1000;
    int topBar = 68 + 26 * frame / 1000, bottomBar = height - 32 * frame / 1000;
    Fill(dc, MakeRect(0, 68, width, topBar), RGB(2, 5, 8));
    Fill(dc, MakeRect(0, bottomBar, width, height), RGB(2, 5, 8));
    if (decor) {
        COLORREF lip = MixColor(C_BG, accent, FxScale(30 + 40 * (1000 - frame) / 1000));
        Fill(dc, MakeRect(0, topBar, width, topBar + 1), lip);
        Fill(dc, MakeRect(0, bottomBar - 1, width, bottomBar), lip);
    }

    RECT art = MakeRect(64, 136, 394, 638);
    RECT panel = MakeRect(416, 136, width - 64, 638);
    const COLORREF panelFill = RGB(10, 19, 28);
    DrawNarrativeDiagram(dc, art, count, scene, typing && who == STORY_WHO_ROGUE);
    // The picture tears in with the scene; SYSTEM lines tear it again and flash red.
    if (decor && scene < 520)
        DrawBandGlitch(dc, art, scene, FxScale(16 * (520 - scene) / 520), gGame.story.kind * 7 + gGame.story.page, 14);
    if (decor && who == STORY_WHO_SYSTEM && clock >= 0 && clock < 240) {
        DrawBandGlitch(dc, art, clock, FxScale(11 * (240 - clock) / 240), shown + 91, 9);
        Outline(dc, art, MixColor(C_BG, C_RED, FxScale(90 * (240 - clock) / 240)), 2);
    } else if (decor && typing && who == STORY_WHO_ROGUE) {
        int glow = 30 + 25 * ((clock / 110) % 3);
        Outline(dc, MakeRect(art.left - 4, art.top - 4, art.right + 4, art.bottom + 4), MixColor(C_BG, accent, FxScale(glow)), 1);
    }
    // Voice link: the bars move only while someone is talking.
    for (int i = 0; i < 29; ++i) {
        int x = art.left + 6 + i * 11;
        int level = decor && typing && who != STORY_WHO_NARRATION ? 1 + (int)(Hash3(i, clock / 70, shown) % 7u) : 1;
        Fill(dc, MakeRect(x, 656 - level, x + 6, 657 + level), MixColor(C_BG, voice, typing ? 70 : 28));
    }

    Panel(dc, panel, panelFill, C_LINE);
    wchar_t title[128], path[128], stamp[128];
    NarrativeText(story->title, title, 126);
    int titleLength = lstrlenW(title);
    int titleShown = decor ? titleLength * EaseOutCubic(Track(scene, 80, 520)) / 1000 : titleLength;
    if (titleShown < titleLength) { title[titleShown] = L'_'; title[titleShown + 1] = 0; }
    NarrativeParagraph(dc, MakeRect(panel.left + 28, panel.top + 25, panel.right - 28, panel.top + 83), title, accent, gFontLarge, 0);
    // The record path decodes out of noise, left to right.
    lstrcpynW(path, LocalizeText(story->path), 128);
    if (decor && scene < 760) {
        wchar_t noise[128];
        CorruptCode(path, noise, 128, gGame.story.kind + 3, (uint32_t)scene);
        int clear = lstrlenW(path) * Track(scene, 180, 740) / 1000;
        for (int i = 0; i < clear && noise[i]; ++i) noise[i] = path[i];
        lstrcpyW(path, noise);
    }
    TextRect(dc, MakeRect(panel.left + 28, panel.top + 85, panel.right - 28, panel.top + 111), path, C_DIM, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    Fill(dc, MakeRect(panel.left + 28, panel.top + 122, panel.right - 28, panel.top + 123), C_LINE);

    HFONT font = StoryDialogueLayout(dc, lines, total, rows, &used);
    int y = panel.top + 146;
    for (int i = 0; i <= shown; ++i) {
        const StoryLineView& line = lines[i];
        const int current = i == shown, age = current ? clock : 60000;
        if (age < 0) break;   // The first line waits until the frame has opened.
        const int length = lstrlenW(line.text), span = StoryLineTypeMs(i);
        const int typed = !decor || age >= span ? length : line.speaker + (length - line.speaker) * age / span;
        const int enter = decor && current ? EaseOutCubic(Track(age, 0, 200)) : 1000;
        COLORREF tag = StoryWhoColor(line.who, accent);
        COLORREF body = current ? C_TEXT : MixColor(C_PANEL, C_TEXT, 62);
        if (!current) tag = MixColor(C_PANEL, tag, 62);
        const int dx = FxScale(16) * (1000 - enter) / 1000;
        RECT rect = MakeRect(panel.left + 46 + dx, y, panel.right - 28 + dx, y + rows[i]);

        if (current) Fill(dc, MakeRect(panel.left + 1, y - 6, panel.right - 1, y + rows[i] + 6), MixColor(panelFill, tag, 8));
        COLORREF mark = current ? MixColor(tag, C_TEXT, 70 * (1000 - enter) / 1000) : MixColor(C_PANEL, tag, 40);
        Fill(dc, MakeRect(panel.left + 28, y + 4, panel.left + 31, y + rows[i] - 2), mark);
        // A new line lands with a brief colour split; SYSTEM hits harder.
        if (decor && current && age < 180) {
            int system = line.who == STORY_WHO_SYSTEM, shift = system ? 4 : 2;
            int ghost = FxScale((system ? 80 : 45) * (180 - age) / 180);
            COLORREF red = MixColor(panelFill, C_RED, ghost), blue = MixColor(panelFill, C_BLUE, ghost);
            DrawStoryLine(dc, MakeRect(rect.left - shift, rect.top, rect.right - shift, rect.bottom), line, typed, font, red, red);
            DrawStoryLine(dc, MakeRect(rect.left + shift, rect.top, rect.right + shift, rect.bottom), line, typed, font, blue, blue);
        }
        RECT caret = DrawStoryLine(dc, rect, line, typed, font, body, tag);
        if (decor && current && line.who == STORY_WHO_SYSTEM && age < 220)
            DrawBandGlitch(dc, MakeRect(panel.left + 2, y - 4, panel.right - 2, y + rows[i] + 4), age, FxScale(9 * (220 - age) / 220), i + 51, 3);
        if (current && typed < length) {
            Fill(dc, MakeRect(caret.left + 3, caret.top + 5, caret.left + 12, caret.bottom - 4), tag);
        } else if (current) {
            // Waiting for input: a small bobbing arrow right after the text.
            int wave = decor ? (scene / 70) % 8 : 0, bob = wave < 4 ? wave : 8 - wave;
            int ax = caret.left + 10, ay = (caret.top + caret.bottom) / 2 - 3 + bob;
            for (int r = 0; r < 5; ++r) Fill(dc, MakeRect(ax + r, ay + r, ax + 9 - r, ay + r + 1), tag);
        }
        y += rows[i] + STORY_LINE_GAP;
    }

    int pages = StoryPageCount(&gGame);
    if (pages < 1) pages = 1;
    wsprintfW(stamp, L"%s  ·  %d / %d%s", story->stamp ? LocalizeText(story->stamp) : L"ROGUE",
        gGame.story.page + 1, pages, gGame.story.replay ? LocalizeText(L"  ·  기록 재생") : L"");
    TextRect(dc, MakeRect(64, 103, width - 64, 128), stamp, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    RECT next = StoryNextRect(width, height);
    int hover = Inside(next, gMouse.x, gMouse.y);
    Panel(dc, next, hover ? RGB(28, 60, 69) : C_PANEL, hover ? accent : C_LINE);
    const wchar_t* label = shown + 1 < total ? L"다음 대사 [ENTER]" : gGame.story.page + 1 < pages ? L"다음 장면 [ENTER]" : L"계속 [ENTER]";
    TextRect(dc, next, label, C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    // One pip per line of this card.
    for (int i = 0; i < total; ++i) {
        int x = next.right + 20 + i * 16, cy = (next.top + next.bottom) / 2;
        RECT pip = MakeRect(x, cy - 4, x + 9, cy + 5);
        if (i < shown || (i == shown && clock >= 0)) Fill(dc, pip, i == shown ? accent : MixColor(C_PANEL, accent, 45));
        else Outline(dc, pip, C_LINE, 1);
    }
}

static void DrawNarrativeName(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_NAME_ENTRY, C_BLUE, width, height);
    TextRect(dc, MakeRect(80, 118, width - 80, 156), L"ROGUE / 첫 번째 질문", C_BLUE, gFontSmall, DT_CENTER | DT_SINGLELINE);
    DrawRoguePortrait(dc, MakeRect(width / 2 - 78, 160, width / 2 + 78, 320), 0, 0, C_BLUE);
    TextRect(dc, MakeRect(80, 325, width - 80, 375), L"로그: 어떻게 불러드리면 될까요?", C_TEXT, gFontLarge, DT_CENTER | DT_SINGLELINE);
    RECT input = NarrativeNameRect();
    Panel(dc, MakeRect(input.left - 3, input.top - 3, input.right + 3, input.bottom + 3), C_PANEL, C_BLUE);
    // A native Unicode EDIT control is placed in this rectangle by main.cpp.
    // Its own IME and selection rendering must not be simulated with WM_CHAR.
    TextRect(dc, MakeRect(300, 460, width - 300, 490), L"이 이름으로 당신을 기억합니다. · 최대 16자", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    RECT confirm = NarrativeNameConfirmRect(); int hover = Inside(confirm, gMouse.x, gMouse.y);
    Panel(dc, confirm, hover ? RGB(28, 60, 69) : C_PANEL, hover ? C_BLUE : C_LINE);
    TextRect(dc, confirm, L"이름 전하기 [ENTER]", C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(220, 603, width - 220, 651), L"자기 이름을 말하듯, 당신이 원하는 이름을 입력하세요.", C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
}

static void DrawTutorialOverlay(HDC dc) {
    if (!gGame.tutorial.active || gGame.phase != PHASE_COMBAT || gTurnTraceActive) return;
    int step = gGame.tutorial.step;
    RECT focus = step == TUTORIAL_READ ? ReadButtonRect() : step == TUTORIAL_PREVIEW ? ForecastRect()
        : step == TUTORIAL_EXECUTE ? EndTurnRect() : MakeRect(28, 408, 698, 708);
    if (step != TUTORIAL_COMPLETE) {
        Outline(dc, MakeRect(focus.left - 3, focus.top - 3, focus.right + 3, focus.bottom + 3), C_YELLOW, 2);
        if (step == TUTORIAL_PLACE) {
            const int slots[3] = {SLOT_ATTACK, SLOT_DEFEND, SLOT_AMPLIFY};
            for (int d = 0; d < 3; ++d) {
                RECT die = DieRect(d), slot = SlotRect(slots[d]);
                DrawLine(dc, (die.left + die.right) / 2, die.top - 3,
                    (slot.left + slot.right) / 2, slot.bottom + 3, MixColor(C_BG, C_YELLOW, 70), 1);
                if (gGame.dice[d].assignedSlot != slots[d]) Outline(dc, slot, C_YELLOW, 2);
            }
        }
    }
    Fill(dc, MakeRect(16, 718, BASE_WIDTH - 16, 758), RGB(10, 28, 38));
    Fill(dc, MakeRect(16, 718, 20, 758), C_YELLOW);
    wchar_t instruction[640]; NarrativeText(TutorialInstruction(&gGame), instruction, 640);
    NarrativeParagraph(dc, MakeRect(28, 721, 1020, 757), instruction, C_TEXT, gFontSmall, 0);
    RECT next = TutorialNextRect(), skip = TutorialSkipRect();
    int canNext = step == TUTORIAL_PREVIEW || step == TUTORIAL_COMPLETE;
    Panel(dc, next, C_PANEL, canNext ? C_YELLOW : C_LINE);
    TextRect(dc, next, step == TUTORIAL_COMPLETE ? L"훈련 완료" : L"확인 [ENTER]", canNext ? C_YELLOW : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    Panel(dc, skip, C_PANEL, C_LINE);
    TextRect(dc, skip, L"훈련 건너뛰기", C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawRogueStatus(HDC dc) {
    if (!gGame.narrativeEnabled || gGame.tutorial.active || gGame.phase != PHASE_COMBAT || gTurnTraceActive) return;
    int count = RecoveredShardCount(gGame.clearedMask);
    COLORREF tone = count >= 6 ? C_DIM : count >= 4 ? C_YELLOW : C_BLUE;
    Fill(dc, MakeRect(28, 733, 33, 738), tone);
    const wchar_t* status = count >= 6 ? L"ROGUE / 응답 없음  ·  남겨진 기록만 연결됨"
        : count >= 3 ? L"ROGUE / 연결됨  ·  잠식 신호 감지" : L"ROGUE / 연결됨";
    TextRect(dc, MakeRect(42, 723, 702, 751), status, tone, gFontSmall, DT_VCENTER | DT_SINGLELINE);
}
