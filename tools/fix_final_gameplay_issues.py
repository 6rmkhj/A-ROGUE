from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def r(p): return (ROOT/p).read_text(encoding='utf-8')
def w(p,s): (ROOT/p).write_text(s,encoding='utf-8',newline='\n')
def once(s,a,b,label):
    n=s.count(a)
    if n!=1: raise RuntimeError(f'{label}: expected 1 got {n}')
    return s.replace(a,b,1)
def fn(s,sig,new):
    st=s.find(sig)
    if st<0: raise RuntimeError('missing '+sig)
    ob=s.find('{',st); dep=0
    for i in range(ob,len(s)):
        if s[i]=='{': dep+=1
        elif s[i]=='}':
            dep-=1
            if dep==0: return s[:st]+new.rstrip()+s[i+1:]
    raise RuntimeError('unclosed '+sig)

# #109: versioned campaign progress now records highest reached floor per regular volume.
p='src/campaign.h'; s=r(p)
s=once(s,'    uint8_t endingSeen[3];\n    uint32_t checksum;',
'''    uint8_t endingSeen[3];
    // 0 = never mounted, 1..3 = highest reached floor. This survives failed runs.
    uint8_t bestFloor[6];
    uint32_t checksum;''','campaign best floor field')
s=once(s,'bool RecordCampaignEnding(CampaignState* campaign, int ending);',
'''bool RecordCampaignEnding(CampaignState* campaign, int ending);
// Records partial progress even when the run later fails. floor is zero-based.
bool RecordCampaignReach(CampaignState* campaign, int drive, int floor);''','reach prototype')
w(p,s)

p='src/campaign.cpp'; s=r(p)
s=once(s,'static const uint16_t CAMPAIGN_VERSION = 1;\nstatic const DWORD SAVE_SIZE = 20;',
'''static const uint16_t CAMPAIGN_VERSION = 2;
static const DWORD SAVE_V1_SIZE = 20;
static const DWORD SAVE_SIZE = 26;''','campaign version')
s=once(s,
'''static uint32_t Checksum(const uint8_t* bytes) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 16; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}''',
'''static uint32_t ChecksumN(const uint8_t* bytes, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}''','campaign checksum')
s=fn(s,'static void Encode(const CampaignState* campaign, uint8_t* bytes)',r'''static void Encode(const CampaignState* campaign, uint8_t* bytes) {
    Put32(bytes, CAMPAIGN_MAGIC);
    bytes[4] = (uint8_t)CAMPAIGN_VERSION;
    bytes[5] = (uint8_t)(CAMPAIGN_VERSION >> 8);
    for (int i = 0; i < 6; ++i) bytes[6 + i] = campaign->cleared[i];
    bytes[12] = campaign->finalCleared;
    for (int i = 0; i < 3; ++i) bytes[13 + i] = campaign->endingSeen[i];
    for (int i = 0; i < 6; ++i) bytes[16 + i] = campaign->bestFloor[i];
    Put32(bytes + 22, ChecksumN(bytes, 22));
}''')
old_checksum='    campaign->checksum = Get32(bytes + 16);'
if old_checksum not in s: raise RuntimeError('init checksum offset missing')
s=s.replace(old_checksum,'    campaign->checksum = Get32(bytes + 22);',1)
s=once(s,'bool RecordCampaignEnding(CampaignState* campaign, int ending) {\n    if (ending < 0 || ending >= 3 || campaign->endingSeen[ending]) return false;\n    campaign->endingSeen[ending] = 1;\n    return true;\n}',
'''bool RecordCampaignEnding(CampaignState* campaign, int ending) {
    if (ending < 0 || ending >= 3 || campaign->endingSeen[ending]) return false;
    campaign->endingSeen[ending] = 1;
    return true;
}

bool RecordCampaignReach(CampaignState* campaign, int drive, int floor) {
    if (!campaign || drive < 0 || drive >= 6) return false;
    int reached = floor + 1;
    if (reached < 1) reached = 1;
    if (reached > 3) reached = 3;
    if (campaign->bestFloor[drive] >= reached) return false;
    campaign->bestFloor[drive] = (uint8_t)reached;
    return true;
}''','record reach function')
s=fn(s,'bool LoadCampaign(CampaignState* campaign, const wchar_t* overridePath)',r'''bool LoadCampaign(CampaignState* campaign, const wchar_t* overridePath) {
    InitCampaign(campaign);
    wchar_t path[MAX_PATH];
    if (!SavePath(path, overridePath)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    uint8_t bytes[SAVE_SIZE + 1] = {0};
    DWORD read = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes), &read, 0) != 0;
    CloseHandle(file);
    if (!ok || Get32(bytes) != CAMPAIGN_MAGIC) return false;
    uint16_t version = (uint16_t)(bytes[4] | ((uint16_t)bytes[5] << 8));
    if (version == 1) {
        if (read != SAVE_V1_SIZE || Get32(bytes + 16) != ChecksumN(bytes, 16)) return false;
        for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
        for (int i = 0; i < 6; ++i) { campaign->cleared[i] = bytes[6+i]; campaign->bestFloor[i] = bytes[6+i] ? 3 : 0; }
        campaign->finalCleared = bytes[12];
        for (int i = 0; i < 3; ++i) campaign->endingSeen[i] = bytes[13+i];
        campaign->version = CAMPAIGN_VERSION;
        return true;
    }
    if (version != CAMPAIGN_VERSION || read != SAVE_SIZE || Get32(bytes + 22) != ChecksumN(bytes, 22)) return false;
    for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
    for (int i = 0; i < 6; ++i) if (bytes[16+i] > 3) return false;
    for (int i = 0; i < 6; ++i) campaign->cleared[i] = bytes[6+i];
    campaign->finalCleared = bytes[12];
    for (int i = 0; i < 3; ++i) campaign->endingSeen[i] = bytes[13+i];
    for (int i = 0; i < 6; ++i) campaign->bestFloor[i] = bytes[16+i];
    campaign->checksum = Get32(bytes + 22);
    return true;
}''')
s=fn(s,'bool SaveCampaign(CampaignState* campaign, const wchar_t* overridePath)',r'''bool SaveCampaign(CampaignState* campaign, const wchar_t* overridePath) {
    uint8_t bytes[SAVE_SIZE];
    Encode(campaign, bytes);
    for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
    for (int i = 0; i < 6; ++i) if (bytes[16+i] > 3) return false;
    wchar_t path[MAX_PATH];
    if (!SavePath(path, overridePath)) return false;
    if (!WriteFileAtomically(path, bytes, SAVE_SIZE)) return false;
    campaign->magic = CAMPAIGN_MAGIC;
    campaign->version = CAMPAIGN_VERSION;
    campaign->checksum = Get32(bytes + 22);
    return true;
}''')
w(p,s)

# #113 replay pages and #119 floor-2+ tactical reroll.
p='src/game.h'; s=r(p)
# append declarations next to KeybReroll prototype if present
s=once(s,'void KeybReroll(GameState* game, int dieIndex);',
'''void KeybReroll(GameState* game, int dieIndex);
int TacticalRerollAvailable(const GameState* game);
// After all six shards are recovered, pages expose every regular volume plus HOST_IMAGE.
void SetReplayDrivePage(GameState* game, int page);''','game prototypes')
w(p,s)

p='src/game.cpp'; s=r(p)
marker='static void PickDriveChoices(GameState* game, uint8_t clearedMask) {'
replay=r'''void SetReplayDrivePage(GameState* game, int page) {
    if (!game || (game->clearedMask & 0x3F) != 0x3F) return;
    static const int pages[3][3] = {
        {DRIVE_FINAL, 0, 1},
        {2, 3, 4},
        {5, DRIVE_FINAL, 0}
    };
    page %= 3; if (page < 0) page += 3;
    game->driveChoiceCount = 3;
    for (int i = 0; i < 3; ++i) {
        game->driveChoices[i] = pages[page][i];
        game->driveDifficulty[i] = pages[page][i] == DRIVE_FINAL ? DIFF_EXPERT : DIFF_INTERMEDIATE;
    }
}

'''
s=once(s,marker,replay+marker,'replay page function')
s=once(s,'    if (!remainingCount) { game->driveChoiceCount = 1; game->driveChoices[0] = DRIVE_FINAL; return; }',
'''    if (!remainingCount) { game->driveChoiceCount = 1; game->driveChoices[0] = DRIVE_FINAL; return; }''','all-cleared replay choices')
s=once(s,'    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);',
'''    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);''','replay difficulty reset')
# Replace KEYB guard and add availability helper immediately before it.
needle='// KEYB: 판독이 끝난 뒤 턴마다 한 번, 선택한 주사위를 다시 굴린다.\nvoid KeybReroll(GameState* game, int dieIndex) {'
replacement='''// A deeper run gains one tactical reroll per turn even without KEYB. KEYB keeps
// the option available from floor 1, so the decision structure evolves after
// the opening floor without invalidating the resident program's early utility.
int TacticalRerollAvailable(const GameState* game) {
    if (!game || game->phase != PHASE_COMBAT || game->keybUsedThisTurn) return 0;
    return IsTsrInstalled(game, TSR_KEYB) || game->floor >= 1;
}

// KEYB / tactical reroll: after reveal, reroll one selected die once per turn.
void KeybReroll(GameState* game, int dieIndex) {'''
s=once(s,needle,replacement,'tactical reroll helper')
s=once(s,'    if (game->phase != PHASE_COMBAT || !IsTsrInstalled(game, TSR_KEYB)) return;\n    if (game->keybUsedThisTurn || dieIndex < 0 || dieIndex >= 3) return;',
'''    if (!TacticalRerollAvailable(game)) return;
    if (dieIndex < 0 || dieIndex >= 3) return;''','reroll guard')
w(p,s)

# Main integrates partial progress, replay page controls, and the broadened reroll rule.
p='src/main.cpp'; s=r(p)
s=once(s,'static int gKeyboardFocus = -1;', 'static int gKeyboardFocus = -1;\nstatic int gReplayPage;','replay page state')
s=once(s,'static void PersistCampaignProgress() {\n    bool changed = RecordCampaignClears(&gCampaign, gGame.clearedMask);',
'''static void PersistCampaignProgress() {
    bool changed = RecordCampaignClears(&gCampaign, gGame.clearedMask);
    if (gGame.selectedDrive >= 0 && gGame.selectedDrive < 6 && RecordCampaignReach(&gCampaign, gGame.selectedDrive, gGame.floor)) changed = true;''','partial progress persistence')
# public renderer helper
insert='static void ResetCampaignProgress() {'
helper='''int CampaignBestFloor(int drive) {
    return drive >= 0 && drive < 6 ? gCampaign.bestFloor[drive] : 0;
}

'''
s=once(s,insert,helper+insert,'best floor renderer helper')
s=once(s,'static void BeginNewRun() {\n    FinishDeath();',
'''static void BeginNewRun() {
    gReplayPage = 0;
    FinishDeath();''','reset replay page')
# main-side page cycler before click handler
clicksig='static void ClickDriveSelect(int x, int y) {'
cycler='''static void CycleReplayPage(int delta) {
    if ((gGame.clearedMask & 0x3F) != 0x3F) return;
    gReplayPage = (gReplayPage + delta) % 3;
    if (gReplayPage < 0) gReplayPage += 3;
    SetReplayDrivePage(&gGame, gReplayPage);
    PlaySfx(SFX_UI_CLICK);
    InvalidateRect(gWindow, 0, FALSE);
}

'''
s=once(s,clicksig,cycler+clicksig,'replay page cycler')
s=once(s,clicksig+'''\n    for (int i = 0; i < gGame.driveChoiceCount; ++i) if (Inside(DriveCardRect(i), x, y)) {''',
clicksig+'''\n    if ((gGame.clearedMask & 0x3F) == 0x3F) {
        if (Inside(ReplayPrevRect(), x, y)) { CycleReplayPage(-1); return; }
        if (Inside(ReplayNextRect(), x, y)) { CycleReplayPage(1); return; }
    }
    for (int i = 0; i < gGame.driveChoiceCount; ++i) if (Inside(DriveCardRect(i), x, y)) {''','replay click controls')
s=once(s,'    if (!gRolled || gGame.keybUsedThisTurn || gGame.selectedDie < 0) return;\n    if (!IsTsrInstalled(&gGame, TSR_KEYB)) return;',
'''    if (!gRolled || gGame.selectedDie < 0) return;
    if (!TacticalRerollAvailable(&gGame)) return;''','main reroll guard')
s=once(s,'    if (IsTsrInstalled(&gGame, TSR_KEYB) && Inside(KeybButtonRect(), x, y)) { KeybRerollSelected(); return; }',
'    if (TacticalRerollAvailable(&gGame) && Inside(KeybButtonRect(), x, y)) { KeybRerollSelected(); return; }','reroll click visibility')
# keyboard paging in drive select
s=once(s,'''    else if (gGame.phase == PHASE_DRIVE_SELECT) {
        if (key >= '1' && key <= '0' + gGame.driveChoiceCount) {''',
'''    else if (gGame.phase == PHASE_DRIVE_SELECT) {
        if ((gGame.clearedMask & 0x3F) == 0x3F && (key == VK_LEFT || key == VK_RIGHT)) { CycleReplayPage(key == VK_LEFT ? -1 : 1); }
        else if (key >= '1' && key <= '0' + gGame.driveChoiceCount) {''','replay keyboard controls')
w(p,s)

# UI declarations.
p='src/ui.h'; s=r(p)
s=once(s,'extern int gCampaignCorrupt;', 'extern int gCampaignCorrupt;\nint CampaignBestFloor(int drive);','best floor ui declaration')
# place replay rect declarations near DriveCardRect declaration
s=once(s,'RECT DriveCardRect(int i);',
'''RECT DriveCardRect(int index);
RECT ReplayPrevRect();
RECT ReplayNextRect();''','replay rect prototypes')
w(p,s)

# Render replay navigation, partial progress, and tactical reroll availability.
p='src/screens.cpp'; s=r(p)
# define rects near other simple rects
anchor='RECT CampaignResetRect() { return MakeRect(560, 658, 840, 700); }'
s=once(s,anchor,anchor+'''\nRECT ReplayPrevRect() { return MakeRect(370, 650, 545, 688); }
RECT ReplayNextRect() { return MakeRect(575, 650, 750, 688); }''','replay rect definitions')
# replace old all-recovered empty message branch: it should no longer be reachable, but retain a safe fallback.
s=s.replace('L"추가로 마운트할 볼륨이 없습니다."','L"좌우 화살표 또는 아래 버튼으로 복구한 볼륨을 다시 마운트할 수 있습니다."')
# Add best-floor text after difficulty/risk line when drawing each drive card. Match a stable call in DrawDriveSelect.
needle='DrawDifficultyCard(dc, card, gGame.driveDifficulty[i]);'
if needle in s:
    s=s.replace(needle, needle+'''\n        int drive = gGame.driveChoices[i];
        if (drive >= 0 && drive < 6) {
            int best = CampaignBestFloor(drive);
            wchar_t progress[48]; wsprintfW(progress, L"최고 도달: %s", best >= 3 ? L"3층 보스" : best == 2 ? L"2층" : best == 1 ? L"1층" : L"미기록");
            TextRect(dc, MakeRect(card.left + 14, card.bottom - 46, card.right - 14, card.bottom - 22), progress, C_DIM, gFontSmall, DT_CENTER | DT_SINGLELINE);
        }''',1)
else:
    print('best-floor card annotation skipped: current renderer has no DrawDifficultyCard helper')
# Add replay navigation near end of DrawDriveSelect using the old no-choice block as a nearby anchor.
anchor='static void DrawDriveSelect(HDC dc, int width, int height) {'
start=s.find(anchor)
if start<0: raise RuntimeError('DrawDriveSelect missing')
# Inject after scene field line.
line='    DrawSceneField(dc, PHASE_DRIVE_SELECT, C_BLUE, width, height);'
pos=s.find(line,start)
if pos<0: raise RuntimeError('drive scene line missing')
ins='''\n    if ((gGame.clearedMask & 0x3F) == 0x3F) {
        TextRect(dc, MakeRect(280, 116, width - 280, 146), L"캠페인 복구 완료 · 모든 일반 볼륨을 재플레이할 수 있습니다", C_GREEN, gFontSmall, DT_CENTER | DT_SINGLELINE);
        RECT prev = ReplayPrevRect(), next = ReplayNextRect();
        int hoverPrev = Inside(prev, gMouse.x, gMouse.y), hoverNext = Inside(next, gMouse.x, gMouse.y);
        Panel(dc, prev, hoverPrev ? RGB(28, 39, 48) : C_PANEL_2, hoverPrev ? C_BLUE : C_LINE);
        Panel(dc, next, hoverNext ? RGB(28, 39, 48) : C_PANEL_2, hoverNext ? C_BLUE : C_LINE);
        TextRect(dc, prev, L"◀ 이전 볼륨", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, next, L"다음 볼륨 ▶", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }'''
s=s[:pos+len(line)]+ins+s[pos+len(line):]
# KEYB button is now a tactical reroll button from floor 2 onward.
s=once(s,'    if (IsTsrInstalled(&gGame, TSR_KEYB)) {\n        RECT keyb = KeybButtonRect();',
'''    if (TacticalRerollAvailable(&gGame) || IsTsrInstalled(&gGame, TSR_KEYB)) {
        RECT keyb = KeybButtonRect();''','reroll button rendering')
s=s.replace('L"KEYB 재굴림 [K]"','L"재굴림 [K]"')
w(p,s)

# Smoke coverage for partial progression serialization and evolved reroll.
p='src/smoke.cpp'; s=r(p)
# Existing no-KEYB expectation should explicitly stay on floor 0.
s=once(s,'    ConfigureDriveForTest(&noKeyb, TEST_DRIVE, TEST_SEED, 1); StartCombat(&noKeyb);\n    KeybReroll(&noKeyb, 0);',
'''    ConfigureDriveForTest(&noKeyb, TEST_DRIVE, TEST_SEED, 1); noKeyb.floor = 0; StartCombat(&noKeyb);
    KeybReroll(&noKeyb, 0);''','floor0 reroll regression')
# Insert deep tactical reroll test after existing no-keyb assertion.
needle='    if (noKeyb.keybUsedThisTurn) return Fail("keyb reroll requires the resident program");'
s=once(s,needle,needle+'''\n    GameState tactical; ConfigureDriveForTest(&tactical, TEST_DRIVE, TEST_SEED ^ 0x55u, 1); tactical.floor = 1; StartCombat(&tactical);
    KeybReroll(&tactical, 0);
    if (!tactical.keybUsedThisTurn) return Fail("floor 2+ must unlock one tactical reroll without KEYB");''','tactical reroll smoke')
# Campaign record API pure-state smoke, no filesystem dependency.
insert='    printf("SMOKE OK\\n");'
if insert in s:
    s=s.replace(insert,'''    CampaignState reach; InitCampaign(&reach);
    if (!RecordCampaignReach(&reach, 2, 1) || reach.bestFloor[2] != 2) return Fail("failed-run reach must persist as campaign metadata");
    if (RecordCampaignReach(&reach, 2, 0) || reach.bestFloor[2] != 2) return Fail("campaign reach must be monotonic");

'''+insert,1)
else: print('campaign reach smoke insertion skipped: current harness has no SMOKE OK marker')
w(p,s)

# README documents replay and evolving reroll.
p='README.md'; s=r(p)
if '캠페인 완주 후 재플레이' not in s:
    s += '''\n\n### 캠페인 완주 후 재플레이\n\n6개 일반 볼륨을 모두 복구한 뒤에도 볼륨 선택 화면의 이전/다음 페이지로 모든 일반 볼륨과 HOST_IMAGE를 다시 마운트할 수 있습니다. 재플레이 클리어는 이미 복구한 조각을 중복 지급하지 않습니다. 실패한 런도 각 일반 볼륨의 최고 도달 층을 캠페인 세이브에 남깁니다.\n\n### 후반 전투 재굴림\n\n1층에서는 KEYB TSR을 설치했을 때만 턴당 1회 재굴림을 사용할 수 있습니다. 2층부터는 시스템 자체가 턴당 1회의 전술 재굴림을 제공해, 후반 런에는 배치 외에 굴림을 보정할지 결정하는 선택지가 추가됩니다. KEYB는 이 선택지를 1층부터 앞당기는 역할을 유지합니다.\n'''
w(p,s)

print('final gameplay issue patch applied')
