// Authored narrative presentation. This file is included by screens.cpp after
// the ordinary board layout; every animation is visual only and FX OFF retains
// all dialogue, stage labels, costs and actionable control highlights.
RECT NarrativeNameRect() { return MakeRect(436, 391, 916, 443); }
RECT NarrativeNameConfirmRect() { return MakeRect(546, 520, 806, 566); }
// ROGUE's training guide sits over the lower sidebar; its buttons live inside it.
static RECT TutorialGuideRect() { return MakeRect(SIDEBAR_LEFT - 8, ForecastRect().bottom + 12, SIDEBAR_RIGHT + 8, 740); }
RECT TutorialNextRect() { return MakeRect(SIDEBAR_LEFT + 10, 690, SIDEBAR_LEFT + 188, 726); }
RECT TutorialSkipRect() { return MakeRect(SIDEBAR_RIGHT - 164, 690, SIDEBAR_RIGHT - 10, 726); }
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

// The story art panel and where ROGUE sits in it. The first-contact beat moves
// ROGUE onto exactly this seat before the dialogue layout appears.
static RECT StoryArtRect() { return MakeRect(64, 136, 394, 638); }
static RECT StoryPortraitRect(const RECT& r) { return MakeRect(r.left + 24, r.top + 52, r.right - 24, r.top + 292); }

// Before P01: the greeting types alone, breaks from its edges into noise, and
// ROGUE is scanned in where it stood, then slides to the story seat.
static void DrawIntroStage(HDC dc, int width, int height, int t) {
    Fill(dc, MakeRect(0, 68, width, height), RGB(2, 5, 8));
    const int cy = (68 + height) / 2;
    if (t < INTRO_ROGUE_AT) {
        const wchar_t* greeting = LocalizeText(L"환영합니다. 마스터.");
        const int length = lstrlenW(greeting);
        HFONT old = (HFONT)SelectObject(dc, gFontHuge);
        SetBkMode(dc, TRANSPARENT);
        SIZE full = {0, 0}; GetTextExtentPoint32W(dc, greeting, length, &full);
        const int x0 = (width - full.cx) / 2, y0 = cy - full.cy / 2;
        const int typed = length * Track(t, INTRO_TYPE_AT, INTRO_TYPE_END) / 1000;
        const int eaten = (length + 1) / 2 * Track(t, INTRO_BREAK_AT, INTRO_ROGUE_AT - 150) / 1000;
        int x = x0;
        for (int i = 0; i < typed; ++i) {
            SIZE cell = {0, 0}; GetTextExtentPoint32W(dc, greeting + i, 1, &cell);
            if (i < eaten || i >= length - eaten) {
                uint32_t h = Hash3(i, t / 60, 5);
                if (h % 3u) {
                    wchar_t noise[2] = { L"#%&?01"[h / 3u % 6u], 0 };
                    SetTextColor(dc, MixColor(C_BG, h % 3u == 1 ? C_RED : C_GREEN, 70));
                    TextOutW(dc, x, y0, noise, 1);
                }
            } else {
                SetTextColor(dc, C_GREEN);
                TextOutW(dc, x, y0, greeting + i, 1);
            }
            x += cell.cx;
        }
        if (t >= INTRO_TYPE_AT && t < INTRO_BREAK_AT && (typed < length || (t / 260) % 2 == 0))
            Fill(dc, MakeRect(x + 6, y0 + 8, x + 6 + full.cy / 3, y0 + full.cy - 8), C_GREEN);
        SelectObject(dc, old);
        if (t >= INTRO_BREAK_AT)
            DrawBandGlitch(dc, MakeRect(x0 - 40, y0 - 12, x0 + full.cx + 40, y0 + full.cy + 12), t,
                FxScale(4 + 16 * Track(t, INTRO_BREAK_AT, INTRO_ROGUE_AT) / 1000), 913, 6);
        return;
    }
    const RECT center = MakeRect(width / 2 - 128, cy - 128, width / 2 + 128, cy + 128);
    const RECT seat = StoryPortraitRect(StoryArtRect());
    const int move = EaseOutCubic(Track(t, INTRO_SETTLE_AT, INTRO_STAGE_MS));
    const RECT face = MakeRect(Lerp(center.left, seat.left, move), Lerp(center.top, seat.top, move),
        Lerp(center.right, seat.right, move), Lerp(center.bottom, seat.bottom, move));
    // Drawn from the top down behind a scan line, the way a sector is read.
    const int reveal = Track(t, INTRO_ROGUE_AT, INTRO_ROGUE_AT + 640);
    const int scanY = face.top + (face.bottom - face.top) * reveal / 1000;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, face.left - 16, face.top - 16, face.right + 16, scanY);
    DrawRoguePortrait(dc, face, 0, 0, C_BLUE, 0);
    RestoreDC(dc, saved);
    if (reveal < 1000) {
        Fill(dc, MakeRect(face.left - 24, scanY - 1, face.right + 24, scanY + 2), MixColor(C_BG, C_BLUE, 90));
        Fill(dc, MakeRect(face.left - 24, scanY + 2, face.right + 24, scanY + 6), MixColor(C_BG, C_BLUE, 30));
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
    RECT portrait = StoryPortraitRect(r);
    DrawRoguePortrait(dc, portrait, milestone, corruption, accent, talking);
    if (kind == STORY_A_GREETING || (terminal && !gGame.story.selectedEnding)) {
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
    const int stage = StoryStageMs();
    if (stage && SceneElapsed() < stage) { DrawIntroStage(dc, width, height, SceneElapsed()); return; }
    // Entrance effects count from the end of any opening beat.
    const int scene = decor ? SceneElapsed() - stage : 3000;
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

    RECT art = StoryArtRect();
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
    TextRect(dc, MakeRect(80, 325, width - 80, 375), L"로그: 뭐라고 불러 드릴까요?", C_TEXT, gFontLarge, DT_CENTER | DT_SINGLELINE);
    RECT input = NarrativeNameRect();
    Panel(dc, MakeRect(input.left - 3, input.top - 3, input.right + 3, input.bottom + 3), C_PANEL, C_BLUE);
    // The name is drawn here in the game font: text and selection from the
    // hidden EDIT, the syllable being composed from the IME (underlined).
    // Player input is never passed through LocalizeText.
    NarrativeNameInput name; ReadNarrativeNameInput(&name);
    int length = lstrlenW(name.text), comp = lstrlenW(name.composition);
    int from = name.selStart < name.selEnd ? name.selStart : name.selEnd;
    int to = name.selStart < name.selEnd ? name.selEnd : name.selStart;
    if (to > length) to = length;
    if (from > to) from = to;
    wchar_t shown[NARRATIVE_NAME_MAX + 17];
    lstrcpynW(shown, name.text, (comp ? from : length) + 1);
    if (comp) { lstrcatW(shown, name.composition); lstrcatW(shown, name.text + to); }
    const int caret = comp ? from + comp : to;
    HFONT oldFont = (HFONT)SelectObject(dc, gFontMedium);
    TEXTMETRICW metric; GetTextMetricsW(dc, &metric);
    const int textX = input.left + 16, textY = (input.top + input.bottom - metric.tmHeight) / 2;
    SIZE a = {0, 0}, b = {0, 0}, c = {0, 0};
    GetTextExtentPoint32W(dc, shown, from, &a);
    GetTextExtentPoint32W(dc, shown, comp ? from + comp : to, &b);
    GetTextExtentPoint32W(dc, shown, caret, &c);
    if (!comp && from != to) Fill(dc, MakeRect(textX + a.cx, textY, textX + b.cx, textY + metric.tmHeight), MixColor(C_PANEL, C_BLUE, 45));
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, C_TEXT);
    TextOutW(dc, textX, textY, shown, lstrlenW(shown));
    if (comp) Fill(dc, MakeRect(textX + a.cx, textY + metric.tmHeight - 2, textX + b.cx, textY + metric.tmHeight), C_BLUE);
    if (!FxDecorOn() || GetTickCount() % 1060 < 530)
        Fill(dc, MakeRect(textX + c.cx + 2, textY + 3, textX + c.cx + 4, textY + metric.tmHeight - 3), C_TEXT);
    SelectObject(dc, oldFont);
    TextRect(dc, MakeRect(300, 460, width - 300, 490), L"이 이름으로 당신을 기억합니다. · 최대 16자", C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
    RECT confirm = NarrativeNameConfirmRect(); int hover = Inside(confirm, gMouse.x, gMouse.y);
    Panel(dc, confirm, hover ? RGB(28, 60, 69) : C_PANEL, hover ? C_BLUE : C_LINE);
    TextRect(dc, confirm, L"이름 전하기 [ENTER]", C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    TextRect(dc, MakeRect(220, 603, width - 220, 651), L"자기 이름을 말하듯, 당신이 원하는 이름을 입력하세요.", C_DIM, gFontSmall, DT_CENTER | DT_WORDBREAK);
}

// ---- Tutorial guide ---------------------------------------------------------
// ROGUE explains each step from a guide window over the lower sidebar (the
// SYSTEM and HISTORY panels carry nothing the practice needs). A tail points
// at the control being explained; the line types while ROGUE's mouth moves.
//
// 표시는 조용하다. 누를 컨트롤 하나에 노란 테두리를 두르고, 배치 단계에서는
// 놓을 칸만 네모로 알린다. 움직이는 장식을 얹어 봤지만 실습 화면에서는
// 시선을 빼앗기만 했다 - 어디를 누르는지가 유일하게 중요한 화면이다.
#define TUTORIAL_SPEECH_W 358
#define TUTORIAL_SPEECH_H 300

// Length of a leading "Name: " speaker tag, including the space, or 0.
static int SpeakerTagLength(const wchar_t* text) {
    for (int c = 1; c < 18 && text[c]; ++c) if (text[c] == L':' && text[c + 1] == L' ') return c + 2;
    return 0;
}

static void TutorialSpeech(wchar_t* out, int capacity) {
    wchar_t text[640];
    NarrativeText(TutorialInstruction(&gGame), text, 640);
    lstrcpynW(out, text + SpeakerTagLength(text), capacity);
}

// Shared with tools/narrative_check.cpp so every language is measured as drawn.
static HFONT TutorialSpeechLayout(HDC dc, const wchar_t* speech, int* height) {
    *height = NarrativeParagraph(dc, MakeRect(0, 0, TUTORIAL_SPEECH_W, 0), speech, C_TEXT, gFontMedium, 1);
    if (*height <= TUTORIAL_SPEECH_H) return gFontMedium;
    *height = NarrativeParagraph(dc, MakeRect(0, 0, TUTORIAL_SPEECH_W, 0), speech, C_TEXT, gFontSmall, 1);
    return gFontSmall;
}

int TutorialTypeMs() {
    wchar_t speech[640]; TutorialSpeech(speech, 640);
    int ms = lstrlenW(speech) * 30;
    return ms < 1400 ? ms : 1400;
}

static void DrawTutorialOverlay(HDC dc) {
    if (!gGame.tutorial.active || gGame.phase != PHASE_COMBAT || gTurnTraceActive) return;
    const int step = gGame.tutorial.step, decor = FxDecorOn();
    const int age = decor ? TutorialStepElapsed() : 60000;
    const int placing = step == TUTORIAL_PLACE || step == TUTORIAL_CHAIN_PLACE;
    RECT focus = TutorialReadStep(step) ? ReadButtonRect() : step == TUTORIAL_PREVIEW ? ForecastRect()
        : TutorialExecuteStep(step) ? EndTurnRect() : MakeRect(28, 408, 698, 708);
    // 배치 단계: 고른 주사위가 들어갈 칸 하나만 네모로 알린다. 아직 고르지 않았거나
    // 고른 주사위가 이미 제자리면, 남은 주사위를 둘러 먼저 고르게 한다.
    if (placing) {
        const int* slots = TutorialExpectedSlots(&gGame);
        const int sel = gGame.selectedDie;
        if (sel >= 0 && sel < 3 && gGame.dice[sel].assignedSlot != slots[sel]) {
            const RECT s = SlotRect(slots[sel]);
            Outline(dc, MakeRect(s.left - 3, s.top - 3, s.right + 3, s.bottom + 3), C_YELLOW, 3);
        } else
            for (int d = 0; d < 3; ++d)
                if (gGame.dice[d].assignedSlot != slots[d]) Outline(dc, DieRect(d), C_YELLOW, 2);
    } else if (step != TUTORIAL_COMPLETE) {
        Outline(dc, MakeRect(focus.left - 3, focus.top - 3, focus.right + 3, focus.bottom + 3), C_YELLOW, 2);
        // Breathing corner brackets, so the eye finds the control first.
        const int reach = 8 + (decor ? 3 * SinMille(age * 3) / 1000 : 0);
        for (int corner = 0; corner < 4; ++corner) {
            const int x = corner & 1 ? focus.right + reach : focus.left - reach;
            const int y = corner & 2 ? focus.bottom + reach : focus.top - reach;
            const int sx = corner & 1 ? -1 : 1, sy = corner & 2 ? -1 : 1;
            Fill(dc, MakeRect(sx > 0 ? x : x - 14, sy > 0 ? y : y - 3, sx > 0 ? x + 14 : x, sy > 0 ? y + 3 : y), C_YELLOW);
            Fill(dc, MakeRect(sx > 0 ? x : x - 3, sy > 0 ? y : y - 14, sx > 0 ? x + 3 : x, sy > 0 ? y + 14 : y), C_YELLOW);
        }
    }

    // The window slides in when training starts; later steps keep it still.
    const RECT home = TutorialGuideRect();
    const int slide = step == TUTORIAL_READ && decor ? FxScale(48) * (1000 - EaseOutCubic(Track(age, 0, 280))) / 1000 : 0;
    const RECT g = MakeRect(home.left + slide, home.top, home.right + slide, home.bottom);
    const COLORREF fill = RGB(9, 18, 27);
    Panel(dc, g, fill, C_BLUE);
    Fill(dc, MakeRect(g.left + 1, g.top + 1, g.right - 1, g.top + 4), MixColor(fill, C_BLUE, 70));
    // Speech tail toward the control: up for the forecast, left otherwise.
    const int fx = (focus.left + focus.right) / 2, fy = (focus.top + focus.bottom) / 2;
    for (int i = 0; i <= 12; ++i) {
        if (focus.bottom <= g.top) {
            const int x = fx < g.left + 40 ? g.left + 40 : fx > g.right - 40 ? g.right - 40 : fx, y = g.top - 12 + i;
            Fill(dc, MakeRect(x - i, y, x + i + 1, y + 1), fill);
            Fill(dc, MakeRect(x - i, y, x - i + 1, y + 1), C_BLUE); Fill(dc, MakeRect(x + i, y, x + i + 1, y + 1), C_BLUE);
        } else {
            const int y = fy < g.top + 40 ? g.top + 40 : fy > g.bottom - 80 ? g.bottom - 80 : fy, x = g.left - 12 + i;
            Fill(dc, MakeRect(x, y - i, x + 1, y + i + 1), fill);
            Fill(dc, MakeRect(x, y - i, x + 1, y - i + 1), C_BLUE); Fill(dc, MakeRect(x, y + i, x + 1, y + i + 1), C_BLUE);
        }
    }

    wchar_t speech[640]; TutorialSpeech(speech, 640);
    const int talk = age - TUTORIAL_TYPE_DELAY_MS, span = TutorialTypeMs();
    const int typing = decor && talk < span;
    // ROGUE hops once when a new explanation begins.
    const int hop = decor && age < 320 ? -8 * SinMille(Track(age, 0, 320) * 18 / 10) / 1000 : 0;
    Panel(dc, MakeRect(g.left + 16, g.top + 16, g.left + 136, g.top + 136), RGB(7, 15, 23), MixColor(C_LINE, C_BLUE, 45));
    DrawRoguePortrait(dc, MakeRect(g.left + 20, g.top + 20 + hop, g.left + 132, g.top + 132 + hop),
        RecoveredShardCount(gGame.clearedMask), 0, C_BLUE, typing);
    TextRect(dc, MakeRect(g.left + 152, g.top + 24, g.right - 18, g.top + 54), L"로그", C_BLUE, gFontMedium, DT_SINGLELINE);
    const int steps = TUTORIAL_COMPLETE + 1;
    static const wchar_t* const titles[TUTORIAL_COMPLETE + 1] = {
        L"판독", L"배치", L"예측 확인", L"실행", L"다시 판독", L"연쇄 배치", L"연쇄 실행", L"결과"};
    wchar_t label[96];
    wsprintfW(label, L"%s  %d / %d", LocalizeText(L"실습 안내"), step + 1, steps);
    TextRect(dc, MakeRect(g.left + 152, g.top + 58, g.right - 18, g.top + 80), label, C_DIM, gFontSmall, DT_SINGLELINE);
    TextRect(dc, MakeRect(g.left + 152, g.top + 82, g.right - 18, g.top + 106), titles[step < steps ? step : steps - 1], C_TEXT, gFontSmall, DT_SINGLELINE);
    for (int i = 0; i < steps; ++i) {
        RECT pip = MakeRect(g.left + 152 + i * 18, g.top + 116, g.left + 164 + i * 18, g.top + 128);
        if (i <= step) Fill(dc, pip, i == step ? C_BLUE : MixColor(fill, C_BLUE, 45)); else Outline(dc, pip, C_LINE, 1);
    }

    const RECT say = MakeRect(g.left + 18, g.top + 150, g.left + 18 + TUTORIAL_SPEECH_W, g.top + 150 + TUTORIAL_SPEECH_H);
    int height;
    HFONT font = TutorialSpeechLayout(dc, speech, &height);
    StoryLineView line = {};
    lstrcpynW(line.text, speech, 640);
    const int length = lstrlenW(line.text);
    const int typed = !typing ? length : talk <= 0 ? 0 : length * talk / span;
    RECT caret = DrawStoryLine(dc, say, line, typed, font, C_TEXT, C_TEXT);
    if (typing && talk > 0) Fill(dc, MakeRect(caret.left + 3, caret.top + 5, caret.left + 12, caret.bottom - 4), C_BLUE);

    RECT next = TutorialNextRect(), skip = TutorialSkipRect();
    const int canNext = step == TUTORIAL_PREVIEW || step == TUTORIAL_COMPLETE;
    Panel(dc, next, C_PANEL, canNext ? C_YELLOW : C_LINE);
    TextRect(dc, next, step == TUTORIAL_COMPLETE ? L"훈련 완료" : L"확인 [ENTER]", canNext ? C_YELLOW : C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    Panel(dc, skip, C_PANEL, C_LINE);
    TextRect(dc, skip, L"훈련 건너뛰기", C_DIM, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---- ROGUE call window ------------------------------------------------------
// The sidebar space under FORECAST belongs to ROGUE during combat. ROGUE stands
// in a small screen that shows the story state (cloak lights, corruption) and
// says one short line only at notable moments; after the handover, and inside
// A, the window no longer answers.
RECT RogueCallRect() { return MakeRect(SIDEBAR_LEFT, ForecastRect().bottom + 10, SIDEBAR_RIGHT, SIDEBAR_BOTTOM); }
#define ROGUE_BARK_TEXT_W 302
#define ROGUE_BARK_TEXT_H 112

// [kind][polite before three recovered volumes, casual after] - the same shift
// the private milestone conversations make.
static const wchar_t* const ROGUE_BARKS[ROGUE_BARK_COUNT][2] = {
    {L"", L""},
    {L"로그: 많이 맞았어요. 방어 좀 챙겨요.", L"로그: 괜찮아? 방어 좀 올려."},
    {L"로그: 다 막았어요. 좋아요.", L"로그: 좋아, 하나도 안 들어왔어."},
    {L"로그: 방금 거 제대로 들어갔어요.", L"로그: 방금 거 세다."},
    {L"로그: 위험해요. 이번 턴은 버티는 쪽으로 가요.", L"로그: 위험해. 이번엔 버티자."},
    {L"로그: 이게 이 층을 잠근 프로세스예요.", L"로그: 저거야. 이 층을 잠근 거."},
    {L"로그: 하나 지웠어요. 남은 것만 봐요.", L"로그: 하나 지웠어. 남은 거 보자."},
};

static void RogueBarkLine(int kind, int casual, wchar_t* out, int capacity) {
    wchar_t text[256];
    NarrativeText(kind > 0 && kind < ROGUE_BARK_COUNT ? ROGUE_BARKS[kind][casual ? 1 : 0] : L"", text, 256);
    lstrcpynW(out, text + SpeakerTagLength(text), capacity);
}

static void RogueBarkText(wchar_t* out, int capacity) {
    RogueBarkLine(RogueBarkKind(), RecoveredShardCount(gGame.clearedMask) >= 3, out, capacity);
}

int RogueBarkTypeMs() {
    wchar_t speech[256]; RogueBarkText(speech, 256);
    int ms = lstrlenW(speech) * 32;
    return ms < 1100 ? ms : 1100;
}

// ---- ROGUE screen -----------------------------------------------------------
// 로그가 서 있는 작은 화면. 전투 사이드바의 호출창과 보상·정리 화면의 조언창이
// 이 그림 하나를 같이 쓴다. 다른 것은 창 크기와 문장뿐이다.
//
// 세로로 초상 - 구분선 - 음성 막대 - 말풍선이 한 기둥으로 서고, 기둥 전체가 검은
// 화면 가운데에 놓인다. 말풍선은 제 글에 맞춰 좌우·위아래로 줄고 꼬리는 늘 그
// 한가운데에서 로그를 가리킨다 - 짧은 한마디 옆에 빈 상자가 남지 않는다.
static const COLORREF ROGUE_INK = RGB(5, 10, 15);
#define ROGUE_RULE_GAP   12   // 초상 아래 구분선까지
#define ROGUE_BAR_GAP    19   // 구분선에서 음성 막대 중심까지
#define ROGUE_BUBBLE_GAP 24   // 막대 중심에서 말풍선 꼬리 끝까지
#define ROGUE_BUBBLE_PAD_X 14
#define ROGUE_BUBBLE_PAD_Y 11
#define ROGUE_TAIL 10

// 접힌 글의 실제 크기. 가장 긴 줄의 폭이 곧 말풍선의 폭이 된다.
static SIZE NarrativeParagraphSize(HDC dc, const wchar_t* text, HFONT font, int maxWidth) {
    HFONT old = (HFONT)SelectObject(dc, font);
    wchar_t wrapped[1024];
    const wchar_t* value = WrapAtSpaces(dc, text, maxWidth, wrapped, 1024, 1);
    RECT r = MakeRect(0, 0, maxWidth, 0);
    DrawTextW(dc, value, -1, &r, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    SelectObject(dc, old);
    SIZE size = { r.right - r.left, r.bottom - r.top };
    return size;
}

// 칸을 넘기면 한 단계 작은 글꼴로 내린다.
// Shared with tools/narrative_check.cpp so every line is measured as drawn.
static HFONT RogueSpeechLayout(HDC dc, const wchar_t* speech, int maxW, int maxH, SIZE* size) {
    *size = NarrativeParagraphSize(dc, speech, gFontMedium, maxW);
    if (size->cy <= maxH) return gFontMedium;
    *size = NarrativeParagraphSize(dc, speech, gFontSmall, maxW);
    return gFontSmall;
}

// 말풍선까지 세운 기둥의 높이. 창 높이를 먼저 정해야 하는 조언창도 이 값을 본다.
static int RogueSpeechHeight(int portrait, int speechHeight) {
    return portrait + ROGUE_RULE_GAP + 2 + ROGUE_BAR_GAP
        + (speechHeight ? ROGUE_BUBBLE_GAP + ROGUE_TAIL + speechHeight + ROGUE_BUBBLE_PAD_Y * 2 : 8);
}

// 창틀·상태 줄과 로그가 설 검은 화면. 여섯 볼륨 이후와 A에서는 잡음만 남으므로
// 몸통을 그릴 필요가 없다는 뜻으로 0을 돌려준다.
static int DrawRogueScreenFrame(HDC dc, const RECT& r, const wchar_t* title, RECT* out) {
    const int count = RecoveredShardCount(gGame.clearedMask), decor = FxDecorOn();
    const int silent = count >= 6 || gGame.selectedDrive == DRIVE_FINAL;
    const DWORD now = decor ? GetTickCount() : 0;
    DrawSidebarFrame(dc, r, title, silent ? C_DIM : C_BLUE);
    TextRect(dc, MakeRect(r.left + 150, r.top + 7, r.right - 12, r.top + 25),
        silent ? L"응답 없음" : count >= 3 ? L"잠식 신호 감지" : L"연결됨",
        silent ? C_DIM : count >= 3 ? C_YELLOW : C_GREEN, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    const RECT screen = MakeRect(r.left + 12, r.top + 38, r.right - 12, r.bottom - 12);
    *out = screen;
    Fill(dc, screen, ROGUE_INK);
    if (decor) DrawScanlines(dc, screen);
    if (!silent) return 1;
    // Only noise remains; inside A a red outline of ROGUE surfaces now and then.
    const int cy = (screen.top + screen.bottom) / 2, cx = (screen.left + screen.right) / 2;
    if (gGame.selectedDrive == DRIVE_FINAL && decor && now % 2600 < 180) {
        // 창이 납작한 조언창에서는 윤곽도 그만큼 작게 선다.
        int span = screen.bottom - screen.top - 20;
        if (span > 240) span = 240;
        DrawRoguePortrait(dc, MakeRect(cx - span / 2, cy - span / 2 - 20, cx + span / 2, cy + span / 2 - 20), 6, 6, C_RED, 0);
    }
    DrawScreenStatic(dc, screen, (int)(now / 90), 240);
    TextRect(dc, MakeRect(screen.left, cy - 16, screen.right, cy + 16), L"NO RESPONSE", C_DIM, gFontMedium, DT_CENTER | DT_SINGLELINE);
    return 0;
}

// typed < 0 이면 문장이 다 서 있고, 0 이상이면 그만큼만 치는 중이다 (캐럿이 붙는다).
static void DrawRogueSpeech(HDC dc, const RECT& screen, int portrait, const wchar_t* speech,
    COLORREF color, int typed, int maxW, int maxH) {
    const int count = RecoveredShardCount(gGame.clearedMask), decor = FxDecorOn();
    const DWORD now = decor ? GetTickCount() : 0;
    const int cx = (screen.left + screen.right) / 2, speaking = speech && speech[0];
    const int typing = speaking && typed >= 0;
    SIZE size = {0, 0};
    HFONT font = speaking ? RogueSpeechLayout(dc, speech, maxW, maxH, &size) : gFontMedium;
    int top = screen.top + (screen.bottom - screen.top - RogueSpeechHeight(portrait, speaking ? size.cy : 0)) / 2;
    if (top < screen.top + 10) top = screen.top + 10;
    const int half = portrait / 2;
    const int bob = decor ? 2 * SinMille((int)(now / 4 % 3600)) / 1000 : 0;
    DrawRoguePortrait(dc, MakeRect(cx - half, top + bob, cx + half, top + portrait + bob),
        count, count > 0 ? count - 1 : 0, C_BLUE, typing);
    int y = top + portrait + ROGUE_RULE_GAP;
    Fill(dc, MakeRect(cx - half + 16, y, cx + half - 16, y + 2), MixColor(ROGUE_INK, C_BLUE, 35));
    // Voice link bars, as wide as ROGUE itself: flat while ROGUE is quiet.
    y += 2 + ROGUE_BAR_GAP;
    const int barW = portrait >= 160 ? 6 : 4, pitch = barW + 5;
    const int bars = (portrait - 8) / pitch, span = bars * pitch - (pitch - barW);
    for (int i = 0; i < bars; ++i) {
        const int x = cx - span / 2 + i * pitch;
        const int level = typing ? 1 + (int)(Hash3(i, typed, 3) % 7u) : 1;
        Fill(dc, MakeRect(x, y - level, x + barW, y + 1 + level), MixColor(ROGUE_INK, C_BLUE, typing ? 70 : 25));
    }
    if (!speaking) return;
    // 말풍선은 글을 감싸고 화면 한가운데에 선다.
    const int wide = size.cx + ROGUE_BUBBLE_PAD_X * 2, tall = size.cy + ROGUE_BUBBLE_PAD_Y * 2;
    const RECT bubble = MakeRect(cx - wide / 2, y + ROGUE_BUBBLE_GAP + ROGUE_TAIL,
        cx - wide / 2 + wide, y + ROGUE_BUBBLE_GAP + ROGUE_TAIL + tall);
    const COLORREF fill = RGB(9, 18, 27);
    Panel(dc, bubble, fill, C_BLUE);
    for (int i = 0; i <= ROGUE_TAIL; ++i) {   // tail up toward ROGUE, on the bubble's own centre
        const int ty = bubble.top - ROGUE_TAIL + i;
        Fill(dc, MakeRect(cx - i, ty, cx + i + 1, ty + 1), fill);
        Fill(dc, MakeRect(cx - i, ty, cx - i + 1, ty + 1), C_BLUE); Fill(dc, MakeRect(cx + i, ty, cx + i + 1, ty + 1), C_BLUE);
    }
    StoryLineView line = {};
    lstrcpynW(line.text, speech, 640);
    const int length = lstrlenW(line.text);
    const RECT say = MakeRect(bubble.left + ROGUE_BUBBLE_PAD_X, bubble.top + ROGUE_BUBBLE_PAD_Y,
        bubble.right - ROGUE_BUBBLE_PAD_X, bubble.bottom - ROGUE_BUBBLE_PAD_Y);
    RECT caret = DrawStoryLine(dc, say, line, typing && typed < length ? typed : length, font, color, color);
    if (typing && typed < length) Fill(dc, MakeRect(caret.left + 3, caret.top + 5, caret.left + 12, caret.bottom - 4), C_BLUE);
}

static void DrawRogueCall(HDC dc) {
    if (!gGame.narrativeEnabled || gGame.tutorial.active || gGame.phase != PHASE_COMBAT || gTurnTraceActive) return;
    RECT screen;
    if (!DrawRogueScreenFrame(dc, RogueCallRect(), L"ROGUE / CALL", &screen)) return;
    wchar_t speech[256]; RogueBarkText(speech, 256);
    const int age = RogueBarkElapsed(), span = RogueBarkTypeMs(), decor = FxDecorOn();
    const int length = lstrlenW(speech);
    const int typed = speech[0] && decor && age < span && span > 0 ? length * age / span : -1;
    DrawRogueSpeech(dc, screen, 240, speech, C_TEXT, typed, ROGUE_BARK_TEXT_W, ROGUE_BARK_TEXT_H);
    // The corruption the story describes stays inside ROGUE's window.
    const DWORD now = decor ? GetTickCount() : 0;
    if (decor && RecoveredShardCount(gGame.clearedMask) >= 3 && now % 4200 < 140)
        DrawBandGlitch(dc, screen, (int)now, FxScale(6), 77, 8);
    if (speech[0] && decor && age > ROGUE_BARK_MS - 260) DrawBandGlitch(dc, screen, age, FxScale(8), 41, 5);
}

// 납작한 창은 기둥 대신 한 줄로 선다 - 초상이 왼쪽, 말풍선이 오른쪽이고 꼬리는
// 옆에서 초상을 가리킨다. 세로로 긴 사이드바(호출창)만 기둥을 쓴다.
static int RogueRowHeight(int portrait, int speechHeight) {
    const int bubble = speechHeight + ROGUE_BUBBLE_PAD_Y * 2;
    return portrait > bubble ? portrait : bubble;
}

static void DrawRogueRow(HDC dc, const RECT& screen, int portrait, const wchar_t* speech,
    COLORREF color, int maxW, int maxH) {
    const int count = RecoveredShardCount(gGame.clearedMask), decor = FxDecorOn();
    SIZE size = {0, 0};
    HFONT font = RogueSpeechLayout(dc, speech, maxW, maxH, &size);
    const int wide = size.cx + ROGUE_BUBBLE_PAD_X * 2, tall = size.cy + ROGUE_BUBBLE_PAD_Y * 2;
    const int row = RogueRowHeight(portrait, size.cy);
    const int top = screen.top + (screen.bottom - screen.top - row) / 2;
    const int left = screen.left + 12;
    const int bob = decor ? 2 * SinMille((int)(GetTickCount() / 4 % 3600)) / 1000 : 0;
    const int portraitTop = top + (row - portrait) / 2 + bob;
    DrawRoguePortrait(dc, MakeRect(left, portraitTop, left + portrait, portraitTop + portrait),
        count, count > 0 ? count - 1 : 0, C_BLUE, 0);
    const int bubbleTop = top + (row - tall) / 2, cy = bubbleTop + tall / 2;
    // 풍선은 초상 바로 옆에서 시작한다. 짧은 한 줄이 오른쪽 끝으로 달아나면
    // 꼬리가 빈자리를 가리킨다.
    const int bubbleLeft = left + portrait + 10 + ROGUE_TAIL;
    const RECT bubble = MakeRect(bubbleLeft, bubbleTop,
        bubbleLeft + wide < screen.right - 12 ? bubbleLeft + wide : screen.right - 12, bubbleTop + tall);
    const COLORREF fill = RGB(9, 18, 27);
    Panel(dc, bubble, fill, C_BLUE);
    for (int i = 0; i <= ROGUE_TAIL; ++i) {   // tail sideways toward ROGUE
        const int x = bubble.left - ROGUE_TAIL + i;
        Fill(dc, MakeRect(x, cy - i, x + 1, cy + i + 1), fill);
        Fill(dc, MakeRect(x, cy - i, x + 1, cy - i + 1), C_BLUE); Fill(dc, MakeRect(x, cy + i, x + 1, cy + i + 1), C_BLUE);
    }
    StoryLineView line = {};
    lstrcpynW(line.text, speech, 640);
    DrawStoryLine(dc, MakeRect(bubble.left + ROGUE_BUBBLE_PAD_X, bubble.top + ROGUE_BUBBLE_PAD_Y,
        bubble.right - ROGUE_BUBBLE_PAD_X, bubble.bottom - ROGUE_BUBBLE_PAD_Y),
        line, lstrlenW(line.text), font, color, color);
}

// ---- ROGUE advice panel -----------------------------------------------------
// 보상·정리 화면은 면 격자 오른쪽이 통째로 비어 있었다. 전투 사이드바의 호출창과
// 같은 자리·같은 틀을 그대로 주고, 여기서는 한마디 대신 지금 화면에서 무엇을
// 정해야 하는지 로그가 일러 준다. 창의 세 상태(연결·잠식·응답 없음)는 호출창과
// 같은 규칙을 따른다 - 여섯 볼륨 이후와 A에서는 이 창도 답하지 않는다.
RECT RogueAdviceRect() { return MakeRect(948, 340, 1310, 640); }
#define ROGUE_ADVICE_TEXT_W 178
#define ROGUE_ADVICE_TEXT_H 104
#define ROGUE_ADVICE_PORTRAIT 88

enum RogueAdviceKind { ADVICE_NONE = 0, ADVICE_REPAIR, ADVICE_PICK, ADVICE_SWAP, ADVICE_OVER,
    ADVICE_CONFIRM, ADVICE_TSR, ADVICE_TSR_ARMED, ADVICE_PRUNE_OVER, ADVICE_PRUNE_EMPTY,
    ADVICE_PRUNE_READY, ADVICE_COUNT };

// [kind][polite before three recovered volumes, casual after] - the same shift
// the call window and the private milestone conversations make.
static const wchar_t* const ROGUE_ADVICES[ADVICE_COUNT][2] = {
    {L"", L""},
    {L"로그: 체력이 반도 안 남았어요. 이번엔 섹터 복구부터 챙겨요.", L"로그: 체력 반도 안 남았어. 이번엔 복구부터 하자."},
    {L"로그: 지금 %dB / %dB 썼어요. 남는 만큼만 실을 수 있어요.", L"로그: 지금 %dB / %dB 썼어. 남는 만큼만 실을 수 있어."},
    {L"로그: 이제 덮을 면을 고르세요. 제일 안 쓰는 걸로요.", L"로그: 이제 덮을 면 골라. 제일 안 쓰는 걸로."},
    {L"로그: 그렇게 바꾸면 %dB예요. 한도를 넘으니 다른 면을 덮어요.", L"로그: 그렇게 바꾸면 %dB야. 한도 넘어, 다른 면으로 가."},
    {L"로그: 좋아요. 한 번 더 누르면 확정이에요.", L"로그: 좋아. 한 번 더 누르면 끝이야."},
    {L"로그: 상주 프로그램은 면을 바꾸지 않아요. 대신 용량을 계속 물고 있어요.", L"로그: 이건 면을 안 바꿔. 대신 용량을 계속 물고 있어."},
    {L"로그: %dB를 계속 내주는 거예요. 그래도 괜찮으면 한 번 더 누르세요.", L"로그: %dB를 계속 내주는 거야. 괜찮으면 한 번 더."},
    {L"로그: %dB 넘었어요. 그만큼 지워야 다음 층으로 갈 수 있어요.", L"로그: %dB 넘었어. 그만큼 지워야 내려가."},
    {L"로그: 전부 지우면 굴릴 게 없어요. 하나는 남겨 둬요.", L"로그: 전부 지우면 굴릴 게 없어. 하나는 남겨."},
    {L"로그: 한도 안으로 들어왔어요. 이대로 진행해도 돼요.", L"로그: 한도 안이야. 이대로 가도 돼."},
};

// 지금 화면이 묻고 있는 것 하나만 고른다. 숫자는 화면이 이미 쓰는 것과 같은 값이다.
static int RogueAdviceFor(int* a, int* b) {
    *a = *b = 0;
    if (gGame.phase == PHASE_PRUNE) {
        if (!NonEmptyFaceCount(&gGame)) return ADVICE_PRUNE_EMPTY;
        int over = UsedBytes(&gGame) - EffectiveCapacity(&gGame);
        if (over <= 0) return ADVICE_PRUNE_READY;
        *a = over; return ADVICE_PRUNE_OVER;
    }
    if (gGame.phase != PHASE_REWARD) return ADVICE_NONE;
    if (gGame.rewardIsTsr) {
        int tsr = gTsrArmed >= 0 && gTsrArmed < 3 ? gGame.rewardKinds[gTsrArmed] : -1;
        if (tsr < 0 || tsr >= TSR_COUNT) return ADVICE_TSR;
        *a = TSR_INFO[tsr].cost; return ADVICE_TSR_ARMED;
    }
    int reward = gGame.selectedReward;
    if (reward < 0 || reward >= 3) {
        // 체력이 반 아래면 면 하나보다 이번 층을 버티는 쪽이 급하다.
        if (CanRepairSector() && gGame.playerHp * 2 <= gGame.playerMaxHp) return ADVICE_REPAIR;
        *a = UsedBytes(&gGame); *b = EffectiveCapacity(&gGame); return ADVICE_PICK;
    }
    if (gFaceSwapArmed < 0 || gFaceSwapArmed >= 18) return ADVICE_SWAP;
    const Face* old = &gGame.dice[gFaceSwapArmed / 6].faces[gFaceSwapArmed % 6];
    int kind = gGame.rewardKinds[reward];
    int newCost = kind == FACE_NUMBER ? gGame.rewardValues[reward] : FACE_INFO[kind].cost;
    int after = UsedBytes(&gGame) - FaceCost(old) + newCost;
    if (after <= EffectiveCapacity(&gGame)) return ADVICE_CONFIRM;
    *a = after; return ADVICE_OVER;
}

// 숫자를 먼저 한국어 문장에 끼우고, 번역은 그 완성된 문장을 표에서 찾는다
// (localization.cpp의 서식 행 규칙). 이름 토큰도 그 뒤에 들어간다.
static void RogueAdviceLine(int kind, int casual, int a, int b, wchar_t* out, int capacity) {
    wchar_t filled[256], text[256];
    wsprintfW(filled, kind > 0 && kind < ADVICE_COUNT ? ROGUE_ADVICES[kind][casual ? 1 : 0] : L"", a, b);
    NarrativeText(filled, text, 256);
    lstrcpynW(out, text + SpeakerTagLength(text), capacity);
}

static void DrawRogueAdvice(HDC dc) {
    if (!gGame.narrativeEnabled || gGame.tutorial.active) return;
    int a = 0, b = 0, kind = RogueAdviceFor(&a, &b);
    if (!kind) return;
    wchar_t speech[256]; RogueAdviceLine(kind, RecoveredShardCount(gGame.clearedMask) >= 3, a, b, speech, 256);
    SIZE size = {0, 0};
    RogueSpeechLayout(dc, speech, ROGUE_ADVICE_TEXT_W, ROGUE_ADVICE_TEXT_H, &size);
    // 창은 제 글에 맞춰 줄어든다. 두 줄짜리 안내 밑에 검은 자리를 남기지 않는다.
    RECT r = RogueAdviceRect();
    const int hugged = r.top + 38 + 20 + RogueRowHeight(ROGUE_ADVICE_PORTRAIT, size.cy) + 12;
    if (hugged < r.bottom) r.bottom = hugged;
    RECT screen;
    if (!DrawRogueScreenFrame(dc, r, L"ROGUE / HINT", &screen)) return;
    DrawRogueRow(dc, screen, ROGUE_ADVICE_PORTRAIT, speech,
        kind == ADVICE_OVER || kind == ADVICE_PRUNE_EMPTY ? C_RED : kind == ADVICE_PRUNE_OVER ? C_YELLOW : C_TEXT,
        ROGUE_ADVICE_TEXT_W, ROGUE_ADVICE_TEXT_H);
    const DWORD now = FxDecorOn() ? GetTickCount() : 0;
    if (now && RecoveredShardCount(gGame.clearedMask) >= 3 && now % 4200 < 140)
        DrawBandGlitch(dc, screen, (int)now, FxScale(6), 77, 8);
}
