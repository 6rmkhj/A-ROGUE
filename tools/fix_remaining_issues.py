from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(p): return (ROOT/p).read_text(encoding='utf-8')
def write(p,s): (ROOT/p).write_text(s,encoding='utf-8',newline='\n')
def once(s,old,new,label):
    n=s.count(old)
    if n!=1: raise RuntimeError(f'{label}: expected 1 got {n}')
    return s.replace(old,new,1)
def rx(s,pat,repl,label):
    out,n=re.subn(pat,lambda m: repl.replace('\\1', m.group(1) if m.lastindex else ''),s,count=1,flags=re.S)
    if n!=1: raise RuntimeError(f'{label}: expected 1 got {n}')
    return out

# ---------------------------------------------------------------------------
# #97: persistent enemy codex lives independently from campaign progression.
# ---------------------------------------------------------------------------
h=read('src/campaign.h')
h += '''\n// Enemy codex is intentionally independent from campaign reset/new-run state.\n// The caller supplies ENEMY_KIND_COUNT bytes and receives only 0/1 values.\nbool LoadCodex(uint8_t* scanned, int count, const wchar_t* path = 0);\nbool SaveCodex(const uint8_t* scanned, int count, const wchar_t* path = 0);\n'''
write('src/campaign.h',h)

c=read('src/campaign.cpp')
c += r'''

// ---- Persistent codex -----------------------------------------------------
// Fixed 32-byte bitset supports up to 256 enemy kinds without tying this file
// to game.h. Resetting AROGUE.SAV deliberately leaves AROGUE.CDX intact.
static const uint32_t CODEX_MAGIC = 0x58444341u; // On disk: ACDX
static const uint16_t CODEX_VERSION = 1;
static const DWORD CODEX_SIZE = 44;

static uint32_t CodexChecksum(const uint8_t* bytes) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 40; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

bool LoadCodex(uint8_t* scanned, int count, const wchar_t* overridePath) {
    if (!scanned || count < 0 || count > 256) return false;
    ZeroMemory(scanned, count);
    wchar_t path[MAX_PATH];
    if (!BesideExecutable(path, overridePath, L"AROGUE.CDX")) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    uint8_t bytes[CODEX_SIZE + 1] = {0}; DWORD got = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes), &got, 0) != 0;
    CloseHandle(file);
    uint16_t version = bytes[4] | ((uint16_t)bytes[5] << 8);
    uint16_t savedCount = bytes[6] | ((uint16_t)bytes[7] << 8);
    if (!ok || got != CODEX_SIZE || Get32(bytes) != CODEX_MAGIC || version != CODEX_VERSION
        || savedCount != count || Get32(bytes + 40) != CodexChecksum(bytes)) return false;
    for (int i = 0; i < count; ++i) scanned[i] = (uint8_t)((bytes[8 + i / 8] >> (i & 7)) & 1u);
    return true;
}

bool SaveCodex(const uint8_t* scanned, int count, const wchar_t* overridePath) {
    if (!scanned || count < 0 || count > 256) return false;
    uint8_t bytes[CODEX_SIZE] = {0};
    Put32(bytes, CODEX_MAGIC); bytes[4] = (uint8_t)CODEX_VERSION; bytes[5] = (uint8_t)(CODEX_VERSION >> 8);
    bytes[6] = (uint8_t)count; bytes[7] = (uint8_t)(count >> 8);
    for (int i = 0; i < count; ++i) if (scanned[i]) bytes[8 + i / 8] |= (uint8_t)(1u << (i & 7));
    Put32(bytes + 40, CodexChecksum(bytes));
    wchar_t path[MAX_PATH];
    return BesideExecutable(path, overridePath, L"AROGUE.CDX") && WriteFileAtomically(path, bytes, CODEX_SIZE);
}
'''
write('src/campaign.cpp',c)

# ---------------------------------------------------------------------------
# #83/#84/#89/#93/#97/#99 main input/state fixes.
# ---------------------------------------------------------------------------
m=read('src/main.cpp')
m=once(m,'static UserSettings gSettings;','''static UserSettings gSettings;
// Persistent discovery state. NewRun clears GameState, so this copy is merged
// back into each run and written independently from campaign progression.
static uint8_t gCodex[ENEMY_KIND_COUNT];
// TSR removals on the prune screen are staged until Continue, making a second
// click an undo rather than an irreversible mistake.
uint8_t gPruneTsrPending[TSR_COUNT] = {};
static int gKeyboardFocus = -1;''','persistent globals')

# #83: no dead settle tail after the last die visibly lands.
m=once(m,
'''    if (elapsed >= DieReadEnd(2) + NOISE_SETTLE_MS) StopRead();''',
'''    if (elapsed >= DieReadEnd(2)) StopRead();''','read end input gap')

# #99: start the read automatically on each new combat turn. R remains a skip.
m=once(m,
'''    gRolled = 0;
}''',
'''    gRolled = 0;
    // Reading is presentation, not a tax the player must pay every turn.
    // Start it automatically; any key/click can still skip the animation.
    BeginRead();
}''','automatic turn read')

# #97 merge persistent codex on every normal persistence checkpoint.
pat=r'''static void PersistCampaignProgress\(\) \{(.*?)\n\}'''
match=re.search(pat,m,re.S)
if not match: raise RuntimeError('PersistCampaignProgress not found')
body=match.group(1)
newbody=body+'''\n    bool codexChanged = false;
    for (int i = 0; i < ENEMY_KIND_COUNT; ++i) {
        if (gGame.enemyScanned[i] && !gCodex[i]) { gCodex[i] = 1; codexChanged = true; }
        if (gCodex[i]) gGame.enemyScanned[i] = 1;
    }
    if (codexChanged && !SaveCodex(gCodex, ENEMY_KIND_COUNT)) gSaveFailed = 1;'''
m=m[:match.start()]+f'static void PersistCampaignProgress() {{{newbody}\n}}'+m[match.end():]

# Ensure a fresh/new run immediately inherits discoveries.
# BeginNewRun is compact and already centralizes every normal restart.
m=rx(m,r'''static void BeginNewRun\(\) \{(.*?)\n\}''',
r'''static void BeginNewRun() {\1
    for (int i = 0; i < ENEMY_KIND_COUNT; ++i) if (gCodex[i]) gGame.enemyScanned[i] = 1;
}''','merge codex after NewRun')

# #89 stage TSR deletions and restore them if Continue cannot leave prune.
old='''static void ClickPrune(int x, int y) {
    int tsrCount = InstalledTsrCount(&gGame);
    for (int i = 0; i < tsrCount && i < 4; ++i) if (Inside(PruneTsrRect(i), x, y)) {
        UninstallTsr(&gGame, InstalledTsrAt(&gGame, i)); PlaySfx(SFX_PRUNE); return;
    }'''
new='''static void ClickPrune(int x, int y) {
    int tsrCount = InstalledTsrCount(&gGame);
    for (int i = 0; i < tsrCount && i < 4; ++i) if (Inside(PruneTsrRect(i), x, y)) {
        int tsr = InstalledTsrAt(&gGame, i);
        if (tsr >= 0 && tsr < TSR_COUNT) {
            gPruneTsrPending[tsr] ^= 1;
            PlaySfx(gPruneTsrPending[tsr] ? SFX_PRUNE : SFX_REWARD_SET);
            InvalidateRect(gWindow, 0, FALSE);
        }
        return;
    }'''
m=once(m,old,new,'stage prune TSR')
m=once(m,
'''    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { ConfirmPrune(&gGame); PlaySfx(SFX_CONFIRM); }
}''',
'''    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) {
        uint8_t removed[TSR_COUNT] = {0};
        for (int tsr = 0; tsr < TSR_COUNT; ++tsr) if (gPruneTsrPending[tsr] && gGame.tsrInstalled[tsr]) {
            removed[tsr] = 1; gGame.tsrInstalled[tsr] = 0;
        }
        ConfirmPrune(&gGame);
        if (gGame.phase == PHASE_PRUNE) {
            for (int tsr = 0; tsr < TSR_COUNT; ++tsr) if (removed[tsr]) gGame.tsrInstalled[tsr] = 1;
        } else ZeroMemory(gPruneTsrPending, sizeof(gPruneTsrPending));
        PlaySfx(SFX_CONFIRM);
    }
}''','commit pending TSR')

# If the player leaves prune by any nonstandard route, no stale deletion intent survives.
m=once(m,
'''    if (gGame.phase != PHASE_ENDING_CHOICE) gEndingArmed = -1;
}''',
'''    if (gGame.phase != PHASE_ENDING_CHOICE) gEndingArmed = -1;
    if (gGame.phase != PHASE_PRUNE) ZeroMemory(gPruneTsrPending, sizeof(gPruneTsrPending));
}''','clear staged TSR')

# #84: one skip click may continue into the destination scene, but never skip a
# second chained animation. Finish exactly one blocker, then fall through only
# if no new blocker started.
old='''static void HandleClick(int x, int y) {
    if (gDeathActive) return;
    if (gBootActive) { FinishBootInsert(); return; }
    if (UiFxBlocksInput()) return;
    if (gTurnTraceActive) { FinishTurnTrace(); return; }
    if (gDescentActive) { FinishDescent(); return; }
    if (gDirEnterActive) { FinishDirectoryEnter(); return; }
    if (gBossIntroActive) { FinishBossIntro(); return; }
    if (gCombatClearActive) { FinishCombatClear(); return; }'''
new='''static void HandleClick(int x, int y) {
    if (gDeathActive) return;
    if (UiFxBlocksInput()) return;
    int skippedOne = 0;
    if (gBootActive) { FinishBootInsert(); skippedOne = 1; }
    else if (gTurnTraceActive) { FinishTurnTrace(); skippedOne = 1; }
    else if (gDescentActive) { FinishDescent(); skippedOne = 1; }
    else if (gDirEnterActive) { FinishDirectoryEnter(); skippedOne = 1; }
    else if (gBossIntroActive) { FinishBossIntro(); skippedOne = 1; }
    else if (gCombatClearActive) { FinishCombatClear(); skippedOne = 1; }
    if (skippedOne && (gDeathActive || UiFxBlocksInput() || gTurnTraceActive || gDescentActive
        || gDirEnterActive || gBossIntroActive || gCombatClearActive || gBootActive)) return;'''
m=once(m,old,new,'click-through animation skip')

# #93 keyboard focus uses the same rectangles and same HandleClick path as mouse.
# Build only core combat/prune controls; existing number/space hotkeys remain.
insert='''
static int KeyboardFocusRects(RECT* out, int cap) {
    int n = 0;
#define ADD_FOCUS_RECT(r) do { if (n < cap) out[n++] = (r); } while (0)
    if (gGame.phase == PHASE_COMBAT) {
        if (!gRolled && !gReadActive) ADD_FOCUS_RECT(ReadButtonRect());
        if (gRolled) {
            for (int i = 0; i < gGame.enemyCount; ++i)
                if (gGame.enemies[i].alive && !GimmickSummonPending(i)) ADD_FOCUS_RECT(EnemyRect(i));
            for (int i = 0; i < 3; ++i) ADD_FOCUS_RECT(DieRect(i));
            for (int i = 0; i < SLOT_COUNT; ++i) ADD_FOCUS_RECT(SlotRect(i));
            ADD_FOCUS_RECT(EndTurnRect());
            if (IsTsrInstalled(&gGame, TSR_KEYB) && !gGame.keybUsedThisTurn) ADD_FOCUS_RECT(KeybButtonRect());
        }
    } else if (gGame.phase == PHASE_PRUNE) {
        int tsrCount = InstalledTsrCount(&gGame);
        for (int i = 0; i < tsrCount && i < 4; ++i) ADD_FOCUS_RECT(PruneTsrRect(i));
        for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f)
            if (gGame.dice[d].faces[f].kind != FACE_EMPTY || CanUndoPrunedFace(&gGame, d, f)) ADD_FOCUS_RECT(FaceGridRect(d, f));
        ADD_FOCUS_RECT(ContinueRect(BASE_WIDTH, BASE_HEIGHT));
    }
#undef ADD_FOCUS_RECT
    return n;
}

static void MoveKeyboardFocus(int delta) {
    RECT items[32]; int count = KeyboardFocusRects(items, 32);
    if (count <= 0) { gKeyboardFocus = -1; return; }
    if (gKeyboardFocus < 0 || gKeyboardFocus >= count) gKeyboardFocus = delta < 0 ? count - 1 : 0;
    else gKeyboardFocus = (gKeyboardFocus + delta + count) % count;
    RECT r = items[gKeyboardFocus];
    gMouse.x = (r.left + r.right) / 2; gMouse.y = (r.top + r.bottom) / 2;
    gMouseInClient = 1; SyncUiFocus(); InvalidateRect(gWindow, 0, FALSE);
}

static int ActivateKeyboardFocus() {
    RECT items[32]; int count = KeyboardFocusRects(items, 32);
    if (gKeyboardFocus < 0 || gKeyboardFocus >= count) return 0;
    RECT r = items[gKeyboardFocus]; HandleClick((r.left + r.right) / 2, (r.top + r.bottom) / 2);
    return 1;
}
'''
# forward declaration needed because helper precedes HandleClick and calls it.
m=once(m,'static void HandleClick(int x, int y) {','static void HandleClick(int x, int y);\n'+insert+'\nstatic void HandleClick(int x, int y) {','keyboard focus helpers')

# HandleKey: inject before phase-specific branches. Tab moves, Enter activates.
needle='''    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
'''
addition=needle+'''    if ((gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_PRUNE) && key == VK_TAB) {
        MoveKeyboardFocus((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1); return;
    }
    if ((gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_PRUNE) && key == VK_RETURN && ActivateKeyboardFocus()) return;
'''
# First occurrence inside HandleClick would be wrong. Restrict after HandleKey marker.
pos=m.find('static void HandleKey(WPARAM key)')
if pos<0: raise RuntimeError('HandleKey missing')
pre,tail=m[:pos],m[pos:]
if tail.count(needle)<1: raise RuntimeError('HandleKey RollBlocking missing')
tail=tail.replace(needle,addition,1)
m=pre+tail

# Real mouse movement returns focus ownership to the pointer.
m=once(m,
'''    case WM_MOUSEMOVE: {
        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));''',
'''    case WM_MOUSEMOVE: {
        gKeyboardFocus = -1;
        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));''','mouse clears keyboard focus')

# Load persistent codex at startup.
m=once(m,
'''    LoadCampaign(&gCampaign);
    LoadTranslations();''',
'''    LoadCampaign(&gCampaign);
    LoadCodex(gCodex, ENEMY_KIND_COUNT);
    LoadTranslations();''','load codex')
write('src/main.cpp',m)

# ---------------------------------------------------------------------------
# #89 renderer + #103 progressive guide.
# ---------------------------------------------------------------------------
u=read('src/ui.h')
u=once(u,'extern int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;','''extern int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
extern uint8_t gPruneTsrPending[TSR_COUNT];''','ui prune pending extern')
write('src/ui.h',u)

s=read('src/screens.cpp')
# TSR card clearly shows reversible pending state.
s=once(s,
'''            RECT r = PruneTsrRect(i); int hover = Inside(r, gMouse.x, gMouse.y);
            Panel(dc, r, hover ? RGB(46, 28, 32) : C_PANEL, hover ? C_RED : C_LINE);
            TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 34), TSR_INFO[tsr].name, (COLORREF)TSR_INFO[tsr].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
            wsprintfW(b, hover ? L"%dB · 종료" : L"%dB", TSR_INFO[tsr].cost);
            TextRect(dc, MakeRect(r.left + 4, r.bottom - 26, r.right - 4, r.bottom - 6), b, hover ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);''',
'''            RECT r = PruneTsrRect(i); int hover = Inside(r, gMouse.x, gMouse.y);
            int pending = gPruneTsrPending[tsr] != 0;
            Panel(dc, r, pending ? RGB(58, 29, 32) : hover ? RGB(46, 28, 32) : C_PANEL, pending || hover ? C_RED : C_LINE);
            TextRect(dc, MakeRect(r.left + 4, r.top + 8, r.right - 4, r.top + 34), TSR_INFO[tsr].name,
                pending ? C_DIM : (COLORREF)TSR_INFO[tsr].color, gFontMedium, DT_CENTER | DT_SINGLELINE);
            if (pending) wsprintfW(b, L"%dB · 삭제 예정 · 다시 클릭해 취소", TSR_INFO[tsr].cost);
            else wsprintfW(b, hover ? L"%dB · 삭제 예약" : L"%dB", TSR_INFO[tsr].cost);
            TextRect(dc, MakeRect(r.left + 4, r.bottom - 26, r.right - 4, r.bottom - 6), b, pending || hover ? C_RED : C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);''','prune TSR pending visual')

# #103 page 1 becomes a minimal first-play loop; detail remains on drive/codex page.
pat=r'''static void DrawGuideCommonPage\(HDC dc, int width, const RECT& panel\) \{.*?\n\}\n\nint GuideNoiseActive'''
repl=r'''static void DrawGuideCommonPage(HDC dc, int width, const RECT& panel) {
    int left = panel.left + 30, middle = width / 2 + 12, top = panel.top + 76;
    Text(dc, left, top, L"첫 전투에 필요한 것만", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 38, middle - 28, top + 220),
        L"1. 턴이 시작되면 주사위가 자동 판독됩니다.\n   연출은 클릭/키로 즉시 넘길 수 있습니다.\n2. 주사위를 클릭하거나 1·2·3으로 선택합니다.\n3. 원하는 슬롯을 클릭해 배치합니다.\n4. 공격할 적을 클릭합니다.\n5. 스페이스 키로 턴을 실행합니다.",
        C_TEXT, gFontMedium, DT_WORDBREAK);
    Text(dc, left, top + 250, L"키보드만으로 플레이", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(left, top + 286, middle - 28, panel.bottom - 52),
        L"Tab / Shift+Tab  전투·정리 항목 이동\nEnter  현재 항목 선택/확정\n1·2·3  주사위 바로 선택\nSpace  턴 실행\nEsc  선택 해제·창 닫기\nF1  이 가이드 다시 열기 · F3  보유 면 확인",
        C_TEXT, gFontSmall, DT_WORDBREAK);

    Text(dc, middle, top, L"나머지는 필요할 때", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 38, panel.right - 28, top + 188),
        L"손상·격리·조각화 같은 상태는 실제로 등장할 때 카드와 배너에 표시됩니다.\n\n보상과 TSR은 선택 화면에서 결과와 비용을 먼저 보여 주며, 되돌릴 수 없는 선택은 한 번 더 확인합니다.",
        C_TEXT, gFontMedium, DT_WORDBREAK);
    Text(dc, middle, top + 220, L"상세 정보 위치", C_YELLOW, gFontMedium);
    TextRect(dc, MakeRect(middle, top + 256, panel.right - 28, panel.bottom - 52),
        L"가이드 2/2  현재 드라이브의 적·보스 도감\nF3  보유한 주사위 면과 특수 능력\n전투 카드  적 의도·상태·기믹 예고\n정리 화면  용량과 삭제 결과\n\n처음부터 전부 외울 필요가 없습니다. 화면에 지금 필요한 규칙만 따라가면 됩니다.",
        C_TEXT, gFontSmall, DT_WORDBREAK);
}

int GuideNoiseActive'''
s=rx(s,pat,repl,'progressive guide')
write('src/screens.cpp',s)

# Synchronize a few changed guide strings used by the external translation table.
for rel in ('translations.tsv','build/translations.tsv'):
    p=ROOT/rel
    if not p.exists(): continue
    t=p.read_text(encoding='utf-8')
    # No destructive rewrite of the large table: untranslated new strings cleanly fall back to Korean.
    # Add explicit English rows for the main headings and accessibility hints.
    add='''\n첫 전투에 필요한 것만\tOnly what you need for your first fight\n키보드만으로 플레이\tKeyboard-only play\n나머지는 필요할 때\tLearn the rest when it appears\n상세 정보 위치\tWhere to find details\n'''
    if '첫 전투에 필요한 것만\t' not in t: t += add
    p.write_text(t,encoding='utf-8',newline='\n')

print('remaining issue patch set applied')
