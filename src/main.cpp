#include <windows.h>
#include <windowsx.h>
#include "ui.h"
#include "render.h"
#include "audio.h"

// 게임 상태와 창·입력을 담당한다. 그리기는 screens.cpp, 소리는 audio.cpp가 맡는다.
GameState gGame;
HWND gWindow;
POINT gMouse;
int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
int gGuidePage;
int gRestartArmed;
static int gHoverId = -1;

// ---- corrupted-sector dice reveal (display only) --------------------------
// The turn's real result is already fixed in gGame.dice[].rolledFace by the
// seeded RNG in game.cpp; everything here only decides how it is revealed, so
// smoke/balance determinism is untouched. Every value below is a pure function
// of elapsed time -- WM_MOUSEMOVE repaints too, and state advanced per frame
// would make the effect race whenever the mouse moves.
#define NOISE_STAGGER_MS 80
#define NOISE_SCAN_MS 200      // opaque static, the face is not readable yet
#define NOISE_LOCK_MS 180      // the face tears its way through the static
#define NOISE_SETTLE_MS 110    // border flash once a cell locks on
#define NOISE_TOTAL_MS (NOISE_SCAN_MS + NOISE_LOCK_MS)

static DWORD gReadStart;
int gReadActive, gReadLanded, gRolled;
static int gRollFloor = -1, gRollEncounter = -1, gRollTurn = -1;

int gCombatClearActive;
static DWORD gCombatClearStart;
int gClearedFloor, gClearedEncounter;
int gTurnTraceActive, gTurnTracePendingClear;
static int gTurnTracePendingDeath;
DWORD gTurnTraceStart;
static int gTraceFloor, gTraceEncounter;

// ---- 피격·위독·정지 연출 ---------------------------------------------------
// 전투는 game.cpp 안에서 한 번에 끝난다. 그래서 "누가 언제 때렸는지"는 규칙을
// 건드리지 않고 game.cpp가 남긴 기록(lastTurnEnemyStruck)을 계산 재생 진행도에
// 맞춰 되짚는 방식으로 보여준다. 여기 있는 값은 전부 경과 시간의 함수라
// 마우스가 움직여 다시 그려져도 연출이 어긋나지 않는다.
#define TRACE_DEATH_HOLD_MS 900   // 마지막 줄을 읽을 틈을 준 뒤 화면이 무너진다

static DWORD gEnemyStrikeAt[3];
static int gEnemyStrikeDamage[3];
static int gStrikeFired;            // 이미 달려든 적 비트마스크
static DWORD gPlayerHitAt;
static int gPlayerHitDamage, gPlayerHitBlockedAll;
static DWORD gLastGaspAt;           // 체력이 1로 떨어진 시각 (0 = 아님)

int gDeathActive;
DWORD gDeathStart;

int TurnTraceShown() {
    int count = gGame.turnTraceCount;
    if (!gTurnTraceActive) return count;
    int shown = (int)(GetTickCount() - gTurnTraceStart) / TURN_TRACE_STEP_MS + 1;
    return shown > count ? count : shown;
}

static void BeginPlayerHit(int damage) {
    gPlayerHitAt = GetTickCount();
    gPlayerHitDamage = damage;
    gPlayerHitBlockedAll = damage <= 0;
    // 방어도가 전부 받아낸 타격은 같은 소리를 높여 가볍게 튕겨낸 느낌을 준다.
    if (damage > 0) PlaySfx(SFX_PLAYER_HIT); else PlaySfxPitched(SFX_PLAYER_HIT, 5);
}

// 계산 재생이 그 적의 [적 행동] 줄에 닿는 순간 달려들게 한다. 12줄을 넘겨
// 기록이 잘린 타격은 재생이 끝나는 시점에 몰아서 발동한다.
void SyncEnemyStrikes() {
    int shown = TurnTraceShown();
    for (int i = 0; i < 3; ++i) {
        if (gStrikeFired & (1 << i)) continue;
        if (!gGame.lastTurnEnemyStruck[i]) continue;
        int line = gGame.lastTurnEnemyStrikeTrace[i];
        int ready = (line >= 0 && line < gGame.turnTraceCount) ? shown > line : shown >= gGame.turnTraceCount;
        if (!ready) continue;
        gStrikeFired |= 1 << i;
        gEnemyStrikeAt[i] = GetTickCount();
        gEnemyStrikeDamage[i] = gGame.lastTurnEnemyStrikeDamage[i];
        BeginPlayerHit(gGame.lastTurnEnemyStrikeDamage[i]);
    }
}

// 0 = 제자리, 1000 = 가장 깊이 파고든 순간. 빠르게 달려들고 천천히 돌아온다.
static int StrikeAdvance(int index) {
    if (!gEnemyStrikeAt[index]) return 0;
    int since = (int)(GetTickCount() - gEnemyStrikeAt[index]);
    if (since < 0 || since >= STRIKE_MS) return 0;
    int lunge = STRIKE_MS * 28 / 100;
    if (since < lunge) return since * 1000 / lunge;
    return 1000 - (since - lunge) * 1000 / (STRIKE_MS - lunge);
}

int EnemyStrikeDrop(int index) { return StrikeAdvance(index) * 26 / 1000; }

int EnemyStrikeShift(int index) {
    int advance = StrikeAdvance(index);
    if (advance <= 0) return 0;
    // 화면 안쪽으로 몸을 던지면서, 부딪히는 동안 잘게 떨린다.
    int toward = index == 0 ? 1 : index == 2 ? -1 : 0;
    int jitter = (int)(Hash3((int)(GetTickCount() / 30), index, 91) % 5u) - 2;
    return (advance * 9 * toward + advance * jitter) / 1000;
}

int EnemyStrikePop(int index) {
    if (!gEnemyStrikeAt[index]) return 0;
    int since = (int)(GetTickCount() - gEnemyStrikeAt[index]);
    if (since < 0 || since >= STRIKE_POP_MS) return 0;
    return 1000 - since * 1000 / STRIKE_POP_MS;
}

int EnemyStrikeDamage(int index) { return gEnemyStrikeDamage[index]; }

int PlayerHitFlash() {
    if (!gPlayerHitAt) return 0;
    int since = (int)(GetTickCount() - gPlayerHitAt);
    if (since < 0 || since >= PLAYER_HIT_MS) return 0;
    return 1000 - since * 1000 / PLAYER_HIT_MS;
}

int PlayerHitBlocked() { return gPlayerHitBlockedAll; }

static int ShakeAmplitude() {
    if (!gPlayerHitAt) return 0;
    int since = (int)(GetTickCount() - gPlayerHitAt);
    if (since < 0 || since >= SHAKE_MS) return 0;
    int damage = gPlayerHitDamage > 14 ? 14 : gPlayerHitDamage;
    int peak = 3 + damage / 2;                       // 3 ~ 10픽셀
    return peak * (SHAKE_MS - since) / SHAKE_MS;
}

int ScreenShakeX() {
    int amp = ShakeAmplitude();
    if (amp <= 0) return 0;
    return (int)(Hash3((int)(GetTickCount() / 24), 3, 17) % (uint32_t)(amp * 2 + 1)) - amp;
}

int ScreenShakeY() {
    int amp = ShakeAmplitude() * 2 / 3;
    if (amp <= 0) return 0;
    return (int)(Hash3((int)(GetTickCount() / 24), 5, 29) % (uint32_t)(amp * 2 + 1)) - amp;
}

// 정지 연출은 주사위 판독과 같은 빠른 주기로 끓고, 오래 지속되는 위독 노이즈는
// 화면 전체가 빠르게 깜빡이지 않도록 느리게 섞인다.
int NoiseFrameStep() { return (int)(GetTickCount() / (gDeathActive ? NOISE_CHURN_MS : 90)); }

// 위독 연출은 화면 가장자리에서 시작한다. 체력이 CRITICAL_HP 이하로 떨어지면
// 테두리 띠에서만 신호가 무너지고, 체력이 줄수록 띠가 두꺼워지고 짙어진다.
// 판 한가운데는 건드리지 않으므로 다음 수를 두는 데 방해가 되지 않는다.
static int CriticalSeverity() {
    if (gGuideOpen || gSettingsOpen || gDeckOpen) return 0;
    if (gGame.phase != PHASE_COMBAT && gGame.phase != PHASE_REWARD && gGame.phase != PHASE_PRUNE) return 0;
    if (gGame.playerHp <= 0 || gGame.playerHp > CRITICAL_HP) return 0;
    return CRITICAL_HP + 1 - gGame.playerHp;                  // 1 ~ CRITICAL_HP
}

static int CriticalPulse() {
    int pulse = (int)((GetTickCount() / 70) % 20u);
    return pulse > 10 ? 20 - pulse : pulse;                   // 0 ~ 10
}

// 마지막 한 칸이 남은 뒤로는 버틴 시간만큼 띠가 더 두꺼워지고 짙어진다.
// 화면 전체로 번지지는 않는다 - 판을 삼키는 것은 정지 연출의 몫이다.
static int LastGaspBoost() {
    if (gGame.playerHp != 1 || !gLastGaspAt) return 0;
    int held = (int)(GetTickCount() - gLastGaspAt);
    if (held < 0) held = 0;
    int boost = held * 1000 / 14000;                          // 14초에 걸쳐 0 → 1000
    return boost > 1000 ? 1000 : boost;
}

int AmbientNoiseLevel() {
    int severity = CriticalSeverity();
    if (severity <= 0) return 0;
    int level = 90 + severity * 34 + CriticalPulse() * severity * 2 + LastGaspBoost() * 130 / 1000;
    return level > 680 ? 680 : level;
}

int AmbientNoiseBand() {
    int severity = CriticalSeverity();
    if (severity <= 0) return 0;
    return 34 + severity * 12 + LastGaspBoost() * 40 / 1000;   // 46 ~ 194픽셀
}

// 체력 1이 "언제부터"인지가 띠가 자라는 기준이라 시각을 잡아 둔다.
void SyncLastGasp() {
    if (gGame.playerHp == 1 && gGame.phase != PHASE_TITLE && gGame.phase != PHASE_GAMEOVER) {
        if (!gLastGaspAt) gLastGaspAt = GetTickCount();
    } else gLastGaspAt = 0;
}

// 체력이 0이 되면 가장자리에 머물던 노이즈가 풀려나 화면을 갉아먹는다.
// 처음에는 천천히 번지다가 끝에서 단숨에 삼키도록 제곱 곡선을 쓴다.
int DeathCreepAmount() {
    if (!gDeathActive) return 0;
    int elapsed = (int)(GetTickCount() - gDeathStart);
    if (elapsed <= 0) return 0;
    if (elapsed >= DEATH_CREEP_MS) return 1000;
    int progress = elapsed * 1000 / DEATH_CREEP_MS;
    return progress * progress / 1000;
}

static void FinishDeath() {
    if (!gDeathActive) return;
    gDeathActive = 0;
    KillTimer(gWindow, 7);
    InvalidateRect(gWindow, 0, FALSE);
}

// 체력이 0이 된 직후. 전장이 그대로 노이즈에 잠기고, 다 덮이면 재시작 화면이 나온다.
static void BeginDeath() {
    // 설정 화면을 강제로 닫으므로 "정말 다시 시작?" 확인 상태도 같이 풀어 준다.
    gGuideOpen = 0; gSettingsOpen = 0; gDeckOpen = 0; gRestartArmed = 0;
    gDeathStart = GetTickCount();
    gDeathActive = 1;
    PlaySfx(SFX_CRASH);
    PlaySfx(SFX_GAMEOVER);
    SetTimer(gWindow, 7, 16, 0);
}

static void FinishCombatClear() {
    if (!gCombatClearActive) return;
    gCombatClearActive = 0;
    KillTimer(gWindow, 3);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginCombatClear(int floor, int encounter) {
    gClearedFloor = floor;
    gClearedEncounter = encounter;
    gCombatClearStart = GetTickCount();
    gCombatClearActive = 1;
    SetTimer(gWindow, 3, 16, 0);
}

static int TurnTraceRevealDuration() {
    int count = gGame.turnTraceCount > 0 ? gGame.turnTraceCount : 1;
    return count * TURN_TRACE_STEP_MS;
}

static void FinishTurnTrace() {
    if (!gTurnTraceActive) return;
    gTurnTraceActive = 0;
    KillTimer(gWindow, 4);
    SyncEnemyStrikes();   // 재생을 건너뛰었어도 맞았다는 사실은 화면에 남는다
    if (gTurnTracePendingDeath) { gTurnTracePendingDeath = 0; BeginDeath(); }
    else if (gTurnTracePendingClear) BeginCombatClear(gTraceFloor, gTraceEncounter);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginTurnTrace(int floor, int encounter, int pendingClear) {
    gTraceFloor = floor;
    gTraceEncounter = encounter;
    gTurnTracePendingClear = pendingClear;
    gTurnTracePendingDeath = gGame.phase == PHASE_GAMEOVER;
    gStrikeFired = 0;
    gTurnTraceStart = GetTickCount();
    gTurnTraceActive = 1;
    SetTimer(gWindow, 4, 16, 0);
}

// ---- drive mount / descent transition -------------------------------------
// 볼륨 마운트(런 시작)와 층 하강(보스 처치 후) 때 재생되는 탐색 연출.
// 게임 상태는 이미 game.cpp에서 확정된 뒤라 여기서는 보여주는 방식만 정한다.
int gDescentActive;
DWORD gDescentStart;
int gDescentToFloor;   // 진입하는 층 (0 = 최초 마운트)
static int gDescentSeekPhase; // 진행 중 한 번씩 울리는 섹터 안착 신호 단계

static void FinishDescent() {
    if (!gDescentActive) return;
    gDescentActive = 0;
    KillTimer(gWindow, 6);
    PlaySfx(SFX_BOOT);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginDescent(int toFloor) {
    gDescentToFloor = toFloor;
    gDescentSeekPhase = 0;
    gDescentStart = GetTickCount();
    gDescentActive = 1;
    PlaySfx(SFX_READ_START);
    SetTimer(gWindow, 6, 16, 0);   // 5번은 오디오 펌프(AUDIO_TIMER_ID)가 쓴다
}

static int ReadElapsed() { return (int)(GetTickCount() - gReadStart); }
static int DieReadEnd(int die) { return die * NOISE_STAGGER_MS + NOISE_TOTAL_MS; }
int DieSettled(int die) { return !gReadActive || ReadElapsed() >= DieReadEnd(die); }
static int RollBlocking() { return gReadActive && !DieSettled(2); }
static int DieLocalTime(int die) { return ReadElapsed() - die * NOISE_STAGGER_MS; }
int NoiseStep(int die) { int t = DieLocalTime(die); return (t < 0 ? 0 : t) / NOISE_CHURN_MS; }

// 1000 = unreadable static, 0 = clean. Cells still queued read as full static,
// so the whole row goes to snow at once and then locks on one at a time.
int DieNoise(int die) {
    if (!gReadActive) return 0;
    int t = DieLocalTime(die);
    if (t < NOISE_SCAN_MS) return 1000;
    if (t < NOISE_TOTAL_MS) return 1000 - (t - NOISE_SCAN_MS) * 1000 / NOISE_LOCK_MS;
    return 0;
}

int DieSettleFlash(int die) {
    if (!gReadActive) return 0;
    int since = ReadElapsed() - DieReadEnd(die);
    if (since < 0 || since >= NOISE_SETTLE_MS) return 0;
    return 1000 - since * 1000 / NOISE_SETTLE_MS;
}

static void StopRead() {
    if (!gReadActive) return;
    gReadActive = 0; gRolled = 1; KillTimer(gWindow, 1);
}

static void BeginRead() {
    if (gGame.phase != PHASE_COMBAT || gRolled || gReadActive) return;
    gReadStart = GetTickCount(); gReadActive = 1; gReadLanded = 0;
    PlaySfx(SFX_READ_START);
    SetTimer(gWindow, 1, 16, 0);
}

// Dice are rolled inside game.cpp at turn start; watch the turn identity so a
// new turn drops back to an unread sector and waits for the READ button.
static void SyncRollAnimation() {
    if (gGame.phase != PHASE_COMBAT) {
        if (gReadActive) { gReadActive = 0; KillTimer(gWindow, 1); }
        gRolled = 0; gRollTurn = -1; return;
    }
    if (gGame.floor == gRollFloor && gGame.encounter == gRollEncounter && gGame.turn == gRollTurn) return;
    gRollFloor = gGame.floor; gRollEncounter = gGame.encounter; gRollTurn = gGame.turn;
    if (gReadActive) { gReadActive = 0; KillTimer(gWindow, 1); }
    gRolled = 0;
}

static void TickRollAnimation() {
    int elapsed = ReadElapsed();
    for (int d = 0; d < 3; ++d) {
        if (!(gReadLanded & (1 << d)) && elapsed >= DieReadEnd(d)) { gReadLanded |= 1 << d; PlaySfxPitched(SFX_DIE_LOCK, d * 1); }
    }
    if (elapsed >= DieReadEnd(2) + NOISE_SETTLE_MS) StopRead();
    InvalidateRect(gWindow, 0, FALSE);
}

#define HIT_FLASH_MS 240

static int gEnemyShownHp[3];
static DWORD gEnemyHitAt[3];

// Combat resolves in one call inside game.cpp, so damage is detected by watching HP.
void SyncEnemyDamage() {
    for (int i = 0; i < 3; ++i) {
        int hp = i < gGame.enemyCount ? gGame.enemies[i].hp : 0;
        if (hp < gEnemyShownHp[i]) gEnemyHitAt[i] = GetTickCount();
        gEnemyShownHp[i] = hp;
    }
}

int EnemyHitFlash(int index) {
    if (!gEnemyHitAt[index]) return 0;
    int since = (int)(GetTickCount() - gEnemyHitAt[index]);
    if (since < 0 || since >= HIT_FLASH_MS) return 0;
    return 1000 - since * 1000 / HIT_FLASH_MS;
}

// Idle float, phase-shifted per slot so a group never breathes in sync.
int EnemyBob(int index) {
    int phase = (int)((GetTickCount() / 110 + (DWORD)index * 5) % 12u);
    if (phase > 6) phase = 12 - phase;
    return 3 - phase;
}

static int gIdleActive;
void SyncIdleAnimation() {
    // 가이드가 열려 있으면 평소엔 리페인트를 멈추지만, 미판독 칸의 노이즈는
    // 계속 흔들려야 하므로 그때만 예외로 타이머를 살려 둔다.
    int wanted = ((gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_DRIVE_SELECT || AmbientNoiseLevel() > 0)
        && !gGuideOpen && !gSettingsOpen && !gDeckOpen) || GuideNoiseActive();
    if (wanted == gIdleActive) return;
    gIdleActive = wanted;
    if (wanted) SetTimer(gWindow, 2, 55, 0); else KillTimer(gWindow, 2);
}

static void BeginNewRun() {
    FinishDeath();
    gStrikeFired = 0; gPlayerHitAt = 0; gLastGaspAt = 0;
    for (int i = 0; i < 3; ++i) { gEnemyStrikeAt[i] = 0; gEnemyStrikeDamage[i] = 0; }
    NewRun(&gGame, GetTickCount() ^ (uint32_t)(ULONG_PTR)gWindow); PlaySfx(SFX_BOOT); InvalidateRect(gWindow, 0, FALSE);
}

static void ExecuteCombatTurn() {
    int floor = gGame.floor, encounter = gGame.encounter;
    int turn = gGame.turn;
    GamePhase before = gGame.phase;
    EndTurn(&gGame);
    int resolved = before == PHASE_COMBAT && (gGame.phase != before || gGame.turn != turn);
    int cleared = gGame.phase == PHASE_REWARD || gGame.phase == PHASE_VICTORY;
    if (resolved) BeginTurnTrace(floor, encounter, cleared);
    // 정지음과 화면 붕괴는 계산 재생이 끝난 뒤 BeginDeath가 맡는다.
    if (gGame.phase == PHASE_GAMEOVER) { if (!resolved) BeginDeath(); }
    else if (gGame.phase == PHASE_VICTORY) PlaySfx(SFX_VICTORY);
    else if (before != gGame.phase) PlaySfx(SFX_ENEMY_DOWN);
    else PlaySfx(SFX_EXECUTE);
}

static void KeybRerollSelected() {
    if (!gRolled || gGame.keybUsedThisTurn || gGame.selectedDie < 0) return;
    if (!IsTsrInstalled(&gGame, TSR_KEYB)) return;
    KeybReroll(&gGame, gGame.selectedDie);
    PlaySfxPitched(SFX_DIE_LOCK, 3);
}

static void ClickCombat(int x, int y) {
    if (Inside(ReadButtonRect(), x, y)) { BeginRead(); return; }
    if (IsTsrInstalled(&gGame, TSR_KEYB) && Inside(KeybButtonRect(), x, y)) { KeybRerollSelected(); return; }
    if (!gRolled) return;
    for (int i = 0; i < gGame.enemyCount; ++i) if (Inside(EnemyRect(i), x, y)) { SelectEnemy(&gGame, i); PlaySfx(SFX_TARGET); return; }
    for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) { gGame.selectedDie = i; PlaySfxPitched(SFX_DIE_PICK, i * 2); return; }
    for (int i = 0; i < SLOT_COUNT; ++i) if (Inside(SlotRect(i), x, y)) {
        if (gGame.selectedDie >= 0) {
            // 잠긴 슬롯 등으로 배치가 거부되면 성공 효과음을 재생하지 않는다.
            if (AssignDieToSlot(&gGame, gGame.selectedDie, i)) PlaySfxPitched(SFX_SLOT_SET, i * 2);
            else PlaySfx(SFX_UI_CLICK);
        }
        else { int die = DieForSlotUI(i); if (die >= 0) gGame.selectedDie = die; } return;
    }
    if (Inside(EndTurnRect(), x, y)) {
        ExecuteCombatTurn();
    }
}

static void ClickDriveSelect(int x, int y) {
    for (int i = 0; i < 3; ++i) if (Inside(DriveCardRect(i), x, y)) {
        SelectDrive(&gGame, i);
        if (gGame.phase == PHASE_COMBAT) { PlaySfx(SFX_CONFIRM); BeginDescent(0); }
        return;
    }
}

static void ClickReward(int x, int y) {
    if (Inside(RewardRect(REWARD_REPAIR, BASE_WIDTH), x, y)) {
        if (CanRepairSector()) { RepairSector(&gGame); PlaySfx(SFX_REWARD_SET); }
        return;
    }
    if (gGame.rewardIsTsr) {
        // 보스 전리품: 카드 클릭 한 번으로 즉시 상주한다.
        for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) { InstallTsr(&gGame, i); PlaySfx(SFX_REWARD_SET); return; }
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { SkipReward(&gGame); PlaySfx(SFX_UI_CLICK); }
        return;
    }
    for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) { SelectReward(&gGame, i); PlaySfxPitched(SFX_REWARD_PICK, i * 2); return; }
    if (gGame.selectedReward >= 0) for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) { InstallSelectedReward(&gGame, d, f); PlaySfx(SFX_REWARD_SET); return; }
    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { SkipReward(&gGame); PlaySfx(SFX_UI_CLICK); }
}

static void ClickPrune(int x, int y) {
    int tsrCount = InstalledTsrCount(&gGame);
    for (int i = 0; i < tsrCount && i < 4; ++i) if (Inside(PruneTsrRect(i), x, y)) {
        UninstallTsr(&gGame, InstalledTsrAt(&gGame, i)); PlaySfx(SFX_PRUNE); return;
    }
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) { PruneFace(&gGame, d, f); PlaySfx(SFX_PRUNE); return; }
    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { ConfirmPrune(&gGame); PlaySfx(SFX_CONFIRM); }
}

// 현재 페이즈에서 (x, y)가 어떤 상호작용 가능한 사각형 위에 있는지 식별하는 id를 반환한다.
// -1은 "호버 없음". 마우스가 움직여도 이 id가 바뀌지 않으면 화면을 다시 그릴 필요가 없다.
static int HoverId(int x, int y) {
    if (gGame.phase != PHASE_TITLE && Inside(DeckButtonRect(BASE_WIDTH), x, y)) return 1000;
    if (gDeckOpen) return Inside(DeckCloseRect(BASE_WIDTH), x, y) ? 1001 : -1;
    if (Inside(SettingsButtonRect(BASE_WIDTH), x, y)) return 900;
    if (gSettingsOpen) {
        if (Inside(SettingsCloseRect(BASE_WIDTH), x, y)) return 901;
        for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) if (Inside(ScaleOptionRect(i), x, y)) return 910 + i;
        if (Inside(FullscreenToggleRect(), x, y)) return 920;
        if (Inside(RestartButtonRect(), x, y)) return 921;
        return -1;
    }
    if (Inside(GuideButtonRect(BASE_WIDTH), x, y)) return 800;
    if (gGuideOpen) {
        if (Inside(GuideCloseRect(BASE_WIDTH), x, y)) return 801;
        if (Inside(GuidePrevRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 802;
        if (Inside(GuideNextRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 803;
        return -1;
    }
    if (gGame.phase == PHASE_TITLE) {
        if (Inside(StartButtonRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 0;
        return -1;
    }
    if (gGame.phase == PHASE_DRIVE_SELECT) {
        for (int i = 0; i < 3; ++i) if (Inside(DriveCardRect(i), x, y)) return 50 + i;
        return -1;
    }
    if (gGame.phase == PHASE_COMBAT) {
        for (int i = 0; i < gGame.enemyCount; ++i) if (Inside(EnemyRect(i), x, y)) return 100 + i;
        for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) return 200 + i;
        for (int i = 0; i < SLOT_COUNT; ++i) if (Inside(SlotRect(i), x, y)) return 300 + i;
        if (Inside(EndTurnRect(), x, y)) return 400;
        if (IsTsrInstalled(&gGame, TSR_KEYB) && Inside(KeybButtonRect(), x, y)) return 410;
        return -1;
    }
    if (gGame.phase == PHASE_REWARD) {
        for (int i = 0; i < REWARD_CARD_COUNT; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) return 500 + i;
        for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) return 600 + d * 6 + f;
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 700;
        return -1;
    }
    if (gGame.phase == PHASE_PRUNE) {
        int tsrCount = InstalledTsrCount(&gGame);
        for (int i = 0; i < tsrCount && i < 4; ++i) if (Inside(PruneTsrRect(i), x, y)) return 640 + i;
        for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) return 600 + d * 6 + f;
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 700;
        return -1;
    }
    return -1;
}

static void HandleClick(int x, int y) {
    if (gDeathActive) return;
    if (gTurnTraceActive) { FinishTurnTrace(); return; }
    if (gDescentActive) { FinishDescent(); return; }
    if (gCombatClearActive) { FinishCombatClear(); return; }
    if (gDeckOpen) {
        if (Inside(DeckCloseRect(BASE_WIDTH), x, y) || Inside(DeckButtonRect(BASE_WIDTH), x, y)) gDeckOpen = 0;
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (gGame.phase != PHASE_TITLE && Inside(DeckButtonRect(BASE_WIDTH), x, y)) { gDeckOpen = 1; gGuideOpen = 0; gSettingsOpen = 0; gRestartArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gSettingsOpen) {
        if (Inside(RestartButtonRect(), x, y)) {
            if (gRestartArmed) { gRestartArmed = 0; gSettingsOpen = 0; BeginNewRun(); InvalidateRect(gWindow, 0, FALSE); return; }
            gRestartArmed = 1; InvalidateRect(gWindow, 0, FALSE); return;
        }
        gRestartArmed = 0;
        if (Inside(SettingsCloseRect(BASE_WIDTH), x, y) || Inside(SettingsButtonRect(BASE_WIDTH), x, y)) { gSettingsOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
        for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) if (Inside(ScaleOptionRect(i), x, y)) { ApplyWindowedScale(SCALE_OPTIONS[i]); InvalidateRect(gWindow, 0, FALSE); return; }
        if (Inside(FullscreenToggleRect(), x, y)) { ApplyFullscreen(!gFullscreen); InvalidateRect(gWindow, 0, FALSE); return; }
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(SettingsButtonRect(BASE_WIDTH), x, y)) { gSettingsOpen = 1; gGuideOpen = 0; gRestartArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) {
        if (Inside(GuideCloseRect(BASE_WIDTH), x, y) || Inside(GuideButtonRect(BASE_WIDTH), x, y)) gGuideOpen = 0;
        else if (Inside(GuidePrevRect(BASE_WIDTH, BASE_HEIGHT), x, y) && gGuidePage > 0) { --gGuidePage; PlaySfx(SFX_UI_CLICK); }
        else if (Inside(GuideNextRect(BASE_WIDTH, BASE_HEIGHT), x, y) && gGuidePage < 1) { ++gGuidePage; PlaySfx(SFX_UI_CLICK); }
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(GuideButtonRect(BASE_WIDTH), x, y)) { gGuideOpen = 1; gSettingsOpen = 0; gGuidePage = 0; gRestartArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
    int floorBefore = gGame.floor;
    if (gGame.phase == PHASE_TITLE) { if (Inside(StartButtonRect(BASE_WIDTH, BASE_HEIGHT), x, y)) BeginNewRun(); }
    else if (gGame.phase == PHASE_DRIVE_SELECT) ClickDriveSelect(x, y);
    else if (gGame.phase == PHASE_COMBAT) ClickCombat(x, y); else if (gGame.phase == PHASE_REWARD) ClickReward(x, y);
    else if (gGame.phase == PHASE_PRUNE) ClickPrune(x, y); else BeginNewRun();
    // 층이 실제로 올라간 클릭(보상/정리 확정)이면 심층 진입 연출을 재생한다.
    if (gGame.floor > floorBefore && gGame.selectedDrive >= 0 && gGame.phase != PHASE_VICTORY) BeginDescent(gGame.floor);
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static void HandleKey(WPARAM key) {
    if (gDeathActive) return;
    if (gTurnTraceActive) return;
    if (gDescentActive) { FinishDescent(); return; }
    if (gCombatClearActive) { FinishCombatClear(); return; }
    if (key == VK_F3 && gGame.phase != PHASE_TITLE) { gDeckOpen = !gDeckOpen; gGuideOpen = 0; gSettingsOpen = 0; gRestartArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gDeckOpen) { if (key == VK_ESCAPE) gDeckOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (key == VK_F2) { gSettingsOpen = !gSettingsOpen; gGuideOpen = 0; gRestartArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gSettingsOpen) { if (key == VK_ESCAPE) { gSettingsOpen = 0; gRestartArmed = 0; } InvalidateRect(gWindow, 0, FALSE); return; }
    if (key == VK_F1) { gGuideOpen = !gGuideOpen; gSettingsOpen = 0; gRestartArmed = 0; if (gGuideOpen) gGuidePage = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) {
        if (key == VK_ESCAPE) gGuideOpen = 0;
        else if (key == VK_LEFT && gGuidePage > 0) --gGuidePage;
        else if (key == VK_RIGHT && gGuidePage < 1) ++gGuidePage;
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
    int floorBefore = gGame.floor;
    if (gGame.phase == PHASE_TITLE) { if (key == VK_RETURN || key == VK_SPACE) BeginNewRun(); }
    else if (gGame.phase == PHASE_DRIVE_SELECT) {
        if (key >= '1' && key <= '3') {
            SelectDrive(&gGame, (int)(key - '1'));
            if (gGame.phase == PHASE_COMBAT) { PlaySfx(SFX_CONFIRM); BeginDescent(0); }
        }
    }
    else if (gGame.phase == PHASE_COMBAT) {
        if (key == 'R') BeginRead();
        else if (!gRolled) { /* sector not read yet */ }
        else if (key >= '1' && key <= '3') { gGame.selectedDie = (int)(key - '1'); PlaySfxPitched(SFX_DIE_PICK, gGame.selectedDie * 2); }
        else if (key == 'K') KeybRerollSelected();
        else if (key == VK_SPACE) ExecuteCombatTurn();
        else if (key == VK_ESCAPE && gGame.selectedDie >= 0) UnassignDie(&gGame, gGame.selectedDie);
    } else if (gGame.phase == PHASE_REWARD) {
        if (key >= '1' && key <= '3') {
            if (gGame.rewardIsTsr) { InstallTsr(&gGame, (int)(key - '1')); PlaySfx(SFX_REWARD_SET); }
            else SelectReward(&gGame, (int)(key - '1'));
        }
        else if (key == '4') { if (CanRepairSector()) { RepairSector(&gGame); PlaySfx(SFX_REWARD_SET); } }
        else if (key == VK_ESCAPE) SkipReward(&gGame);
    } else if (gGame.phase == PHASE_PRUNE) { if (key == VK_RETURN) ConfirmPrune(&gGame); }
    else if (key == 'R' || key == VK_RETURN) BeginNewRun();
    if (gGame.floor > floorBefore && gGame.selectedDrive >= 0 && gGame.phase != PHASE_VICTORY) BeginDescent(gGame.floor);
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateRenderFonts(); AudioOpen(window); return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO* info = (MINMAXINFO*)lParam; info->ptMinTrackSize.x = 480; info->ptMinTrackSize.y = 320; return 0; }
    case WM_MOUSEMOVE: {
        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        int hover = HoverId(gMouse.x, gMouse.y);
        if (hover != gHoverId) { gHoverId = hover; InvalidateRect(window, 0, FALSE); }
        return 0;
    }
    case WM_LBUTTONDOWN: { POINT p = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); HandleClick(p.x, p.y); return 0; }
    case WM_KEYDOWN: if ((lParam & (1u << 30)) == 0) HandleKey(wParam); return 0;
    case WM_TIMER:
        if (wParam == AUDIO_TIMER_ID) { AudioPump(); return 0; }
        if (wParam == 1u) TickRollAnimation();
        else if (wParam == 2u) InvalidateRect(window, 0, FALSE);
        else if (wParam == 3u) {
            if ((int)(GetTickCount() - gCombatClearStart) >= COMBAT_CLEAR_MS) FinishCombatClear();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 4u) {
            int traceElapsed = (int)(GetTickCount() - gTurnTraceStart), reveal = TurnTraceRevealDuration();
            // 죽은 판은 클릭을 기다리지 않는다. 마지막 줄을 읽을 틈만 주고 화면이 무너진다.
            if (gTurnTracePendingDeath) { if (traceElapsed >= reveal + TRACE_DEATH_HOLD_MS) { FinishTurnTrace(); return 0; } }
            else if (traceElapsed >= reveal) KillTimer(window, 4);
            InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 6u) {
            int descentElapsed = (int)(GetTickCount() - gDescentStart);
            if (gDescentSeekPhase == 0 && descentElapsed >= DESCENT_MS / 3) { ++gDescentSeekPhase; PlaySfx(SFX_DIE_LOCK); }
            else if (gDescentSeekPhase == 1 && descentElapsed >= DESCENT_MS * 2 / 3) { ++gDescentSeekPhase; PlaySfxPitched(SFX_DIE_LOCK, 4); }
            if (descentElapsed >= DESCENT_MS) FinishDescent();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 7u) {
            if ((int)(GetTickCount() - gDeathStart) >= DEATH_STATIC_MS) FinishDeath();
            else InvalidateRect(window, 0, FALSE);
        }
        return 0;
    case WM_PAINT: PaintGame(window); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        KillTimer(window, 1); KillTimer(window, 2); KillTimer(window, 3); KillTimer(window, 4); KillTimer(window, 6); KillTimer(window, 7);
        DestroyRenderFonts();
        AudioClose(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware(); InitTitle(&gGame); WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure; wc.hInstance = instance; wc.hCursor = LoadCursorW(0, IDC_ARROW); wc.hIcon = LoadIconW(0, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszClassName = L"ARogueWindowClass"; if (!RegisterClassExW(&wc)) return 1;
    RECT desired = {0, 0, 1120, 760}; AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0); int width = desired.right - desired.left, height = desired.bottom - desired.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    gWindow = CreateWindowExW(0, wc.lpszClassName, L"A:\\ROGUE · 1.44MB", WS_OVERLAPPEDWINDOW, x, y, width, height, 0, 0, instance, 0);
    if (!gWindow) return 2; ShowWindow(gWindow, showCommand); UpdateWindow(gWindow);
    MSG message; while (GetMessageW(&message, 0, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return (int)message.wParam;
}
