// Authored narrative presentation. This file is included by screens.cpp after
// the ordinary board layout; every animation is visual only and FX OFF retains
// all dialogue, stage labels, costs and actionable control highlights.
RECT NarrativeNameRect() { return MakeRect(436, 391, 916, 443); }
RECT NarrativeNameConfirmRect() { return MakeRect(546, 520, 806, 566); }
RECT TutorialNextRect() { return MakeRect(1040, 720, 1180, 755); }
RECT TutorialSkipRect() { return MakeRect(1190, 720, 1324, 755); }
RECT TutorialReplayRect() { return MakeRect(830, 164, 1238, 206); }

static void NarrativeText(const wchar_t* source, wchar_t* out, int capacity) {
    // Translation must happen before player input is inserted. In particular a
    // Korean name is never passed back through the translation lookup.
    source = LocalizeText(source ? source : L"");
    const wchar_t* name = gGame.narrative.playerName[0] ? gGame.narrative.playerName : LocalizeText(L"나");
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
    const wchar_t* value = WrapAtSpaces(dc, text, rect.right - rect.left, wrapped, 1024);
    RECT draw = rect;
    if (measure) draw.bottom = draw.top;
    DrawTextW(dc, value, -1, &draw, DT_WORDBREAK | DT_NOPREFIX | (measure ? DT_CALCRECT : 0));
    SelectObject(dc, old);
    return draw.bottom - draw.top;
}

static void DrawRoguePortrait(HDC dc, RECT rect, int restored, int corrupted, COLORREF accent) {
    const int cx = (rect.left + rect.right) / 2, cy = (rect.top + rect.bottom) / 2;
    int unit = (rect.right - rect.left) / 28; if (unit < 1) unit = 1;
    // A recognizable, deliberately spare face: asymmetric fringe, two eyes,
    // collar and shoulders. Restored regions add detail; corruption occupies
    // different tiles, so becoming whole is not simply becoming a red bar.
    const wchar_t* pixels[17] = {
        L"       #######      ", L"     ##########     ", L"    ####### ####    ",
        L"    ##         ##   ", L"   ###         ##   ", L"   ##           #   ",
        L"   ##   #   #   #   ", L"    #           #   ", L"    #     #     #   ",
        L"    ##         ##   ", L"     ##  ###  ##    ", L"      ##     ##     ",
        L"       #######      ", L"      ##     ##     ", L"   #####     #####  ",
        L" ###   ### ###   ###", L"##       ###       #"
    };
    int top = cy - 9 * unit, left = cx - 10 * unit;
    for (int y = 0; y < 17; ++y) for (int x = 0; pixels[y][x]; ++x) {
        if (pixels[y][x] != L'#') continue;
        int detail = (x * 7 + y * 3) % 7;
        COLORREF tone = detail <= restored ? accent : MixColor(C_PANEL, accent, 35);
        int bad = corrupted && (int)(Hash3(x, y, 33) % 7u) < corrupted;
        if (bad) tone = MixColor(accent, C_RED, 85);
        int shift = bad && FxDecorOn() ? ((int)(Hash3(y, GetTickCount() / 180, 4) % 3u) - 1) * unit : 0;
        Fill(dc, MakeRect(left + x * unit + shift, top + y * unit, left + (x + 1) * unit - 1 + shift, top + (y + 1) * unit - 1), tone);
    }
    if (corrupted) {
        for (int i = 0; i < corrupted; ++i) {
            int x = rect.left + 12 + i * (rect.right - rect.left - 24) / 6;
            Fill(dc, MakeRect(x, rect.bottom - 20, x + 16, rect.bottom - 17), C_RED);
        }
    }
}

static void DrawNarrativeDiagram(HDC dc, RECT r, int count, int elapsed) {
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
    RECT portrait = MakeRect(r.left + 24, r.top + 65, r.right - 24, r.top + 318);
    DrawRoguePortrait(dc, portrait, milestone, corruption, accent);
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

static void DrawStory(HDC dc, int width, int height) {
    const StoryFragment* story = CurrentStoryFragment(&gGame);
    if (!story) return;
    const int count = RecoveredShardCount(gGame.clearedMask);
    COLORREF accent = gGame.story.kind == STORY_MILESTONE && gGame.story.fragment == 5 ? C_RED : C_BLUE;
    DrawSceneField(dc, PHASE_STORY, accent, width, height);
    Fill(dc, MakeRect(0, 68, width, 94), RGB(2, 5, 8));
    Fill(dc, MakeRect(0, height - 32, width, height), RGB(2, 5, 8));
    RECT art = MakeRect(64, 136, 394, 638);
    RECT panel = MakeRect(416, 136, width - 64, 638);
    DrawNarrativeDiagram(dc, art, count, FxDecorOn() ? SceneElapsed() : 3000);
    Panel(dc, panel, RGB(10, 19, 28), C_LINE);
    wchar_t title[128], stamp[128];
    NarrativeText(story->title, title, 128);
    NarrativeParagraph(dc, MakeRect(panel.left + 28, panel.top + 25, panel.right - 28, panel.top + 83), title, accent, gFontLarge, 0);
    TextRect(dc, MakeRect(panel.left + 28, panel.top + 85, panel.right - 28, panel.top + 111), story->path, C_DIM, gFontSmall, DT_SINGLELINE | DT_END_ELLIPSIS);
    Fill(dc, MakeRect(panel.left + 28, panel.top + 122, panel.right - 28, panel.top + 123), C_LINE);
    const wchar_t* source[5] = { story->line1, story->line2, story->line3, story->line4, story->line5 };
    wchar_t lines[5][640];
    int row[5], total = 0;
    HFONT font = gFontMedium;
    int lineWidth = panel.right - panel.left - 74;
    for (int i = 0; i < 5; ++i) NarrativeText(source[i], lines[i], 640);
    for (int pass = 0; pass < 2; ++pass) {
        total = 0;
        for (int i = 0; i < 5; ++i) {
            row[i] = lines[i][0] ? NarrativeParagraph(dc, MakeRect(0, 0, lineWidth, 0), lines[i], C_TEXT, font, 1) : 0;
            total += row[i] + (row[i] ? 15 : 0);
        }
        if (total <= 322) break;
        font = gFontSmall;
    }
    int y = panel.top + 146;
    // Measure the complete localized, substituted lines first. Reveal only the
    // decorative margin marks; dialogue never reflows during a typewriter FX.
    for (int i = 0; i < 5; ++i) if (row[i]) {
        COLORREF tone = i == 4 ? C_YELLOW : C_TEXT;
        if (lines[i][0] == L'>') tone = C_BLUE;
        int light = !FxDecorOn() ? 100 : Track(SceneElapsed(), i * 100, i * 100 + 250) / 10;
        Fill(dc, MakeRect(panel.left + 28, y + 4, panel.left + 31, y + row[i] - 2), MixColor(C_PANEL, accent, light));
        NarrativeParagraph(dc, MakeRect(panel.left + 46, y, panel.right - 28, y + row[i]), lines[i], tone, font, 0);
        y += row[i] + 15;
    }
    int pages = StoryPageCount(&gGame);
    if (pages < 1) pages = 1;
    wsprintfW(stamp, L"%s  ·  %d / %d%s", story->stamp ? LocalizeText(story->stamp) : L"ROGUE",
        gGame.story.page + 1, pages, gGame.story.replay ? LocalizeText(L"  ·  기록 재생") : L"");
    TextRect(dc, MakeRect(64, 103, width - 64, 128), stamp, C_DIM, gFontSmall, DT_RIGHT | DT_SINGLELINE);
    RECT next = StoryNextRect(width, height);
    int hover = Inside(next, gMouse.x, gMouse.y);
    Panel(dc, next, hover ? RGB(28, 60, 69) : C_PANEL, hover ? accent : C_LINE);
    TextRect(dc, next, gGame.story.page + 1 < pages ? L"다음 장면 [ENTER]" : L"계속 [ENTER]", C_TEXT, gFontMedium, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawNarrativeName(HDC dc, int width, int height) {
    DrawSceneField(dc, PHASE_NAME_ENTRY, C_BLUE, width, height);
    TextRect(dc, MakeRect(80, 118, width - 80, 156), L"ROGUE / 첫 번째 질문", C_BLUE, gFontSmall, DT_CENTER | DT_SINGLELINE);
    DrawRoguePortrait(dc, MakeRect(width / 2 - 78, 173, width / 2 + 78, 322), 0, 0, C_BLUE);
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
