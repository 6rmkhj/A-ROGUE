#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>   // timeBeginPeriod: 연출 타이머를 15.6ms 틱에서 풀어 준다
#include "ui.h"
#include "render.h"
#include "audio.h"
#include "music.h"
#include "localization.h"
#include "campaign.h"

// 게임 상태와 창·입력을 담당한다. 그리기는 screens.cpp, 소리는 audio.cpp가 맡는다.
GameState gGame;
CampaignState gCampaign;
// 언어·배율·전체화면·연출 강도·BGM·소리. 캠페인 세이브와 따로 두어
// "진행도 초기화"가 환경까지 되돌리지 않게 한다.
static UserSettings gSettings;
// Persistent discovery state. NewRun clears GameState, so this copy is merged
// back into each run and written independently from campaign progression.
static uint8_t gCodex[ENEMY_KIND_COUNT];
// TSR removals on the prune screen are staged until Continue, making a second
// click an undo rather than an irreversible mistake.
uint8_t gPruneTsrPending[TSR_COUNT] = {};
static int gKeyboardFocus = -1;
static int gReplayPage;
HWND gWindow;
POINT gMouse;
int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
int gTermOpen;
// 개발 빌드(-DAROGUE_DEV)이거나 실행 인자에 -dev가 있을 때만 1. 관리자 터미널의
// 유일한 관문이다. 0이면 백틱이 아무 일도 하지 않는다.
#ifdef AROGUE_DEV
int gDevMode = 1;
#else
int gDevMode = 0;
#endif
wchar_t gTermLog[TERM_LOG_LINES][TERM_LOG_CAP];
int gTermLogCount;
wchar_t gTermInput[TERM_INPUT_MAX + 1];
int gTermInputLen;
// 소리 슬라이더를 붙잡고 있는 동안 채널 번호를 기억한다. -1이면 드래그 중이 아니다.
static int gVolumeDragging = -1;
static int gVolumeKeyboardChannel = AUDIO_VOLUME_MASTER;
static int AudioChannelVolume(int channel) {
    return channel == AUDIO_VOLUME_BGM ? AudioMusicVolume() : channel == AUDIO_VOLUME_SFX ? AudioSfxVolume() : AudioVolume();
}
static void SetAudioChannelVolume(int channel, int value) {
    if (channel == AUDIO_VOLUME_BGM) AudioSetMusicVolume(value);
    else if (channel == AUDIO_VOLUME_SFX) AudioSetSfxVolume(value);
    else SetAudioVolume(value);
}
int gGuidePage;
int gRestartArmed;
int gCampaignResetArmed;
int gRewardSkipArmed;
int gDirectoryArmed = -1;
int gTsrArmed = -1;
int gFaceSwapArmed = -1;
int gEndingArmed = -1;
// 마지막 세이브 시도가 실패했으면 1. 쓰기 권한이 없는 폴더에서 돌리는 동안
// 조용히 진행하다 기록을 통째로 잃는 일을 막으려고 화면에 띄운다.
int gSaveFailed;
// Settings and corrupt-input failures are tracked separately so a successful
// campaign write cannot hide a failed CFG write, and vice versa.
int gSettingsSaveFailed;
int gCampaignCorrupt;
int gFxLevel = FX_FULL;

// 직접 조작 연출은 게임 판정과 분리된 마지막 사건 하나만 기억한다. 연타가 가능한
// 배치·정리는 새 입력이 이전 연출을 자연스럽게 덮고, 화면 전환을 동반하는 보상만
// 입력을 잠깐 막아 설치 경로를 끝까지 보여 준다.
UiFxState gUiFx = {};
static int gUiFxPendingDescent = -1;
static void BeginDescent(int toFloor, int choiceIndex);

#define UIFX_TIMER_ID 11
#define BOSS_INTRO_TIMER_ID 12
#define UIFX_PLACE_MS 300
#define UIFX_REWARD_MS 520
#define UIFX_PRUNE_MS 340

int UiFxElapsed() { return gUiFx.kind == UIFX_NONE ? 0 : (int)(GetTickCount() - gUiFx.start); }

int UiFxSnapshotActive() {
    return gUiFx.kind == UIFX_REWARD_FACE || gUiFx.kind == UIFX_REWARD_TSR
        || gUiFx.kind == UIFX_REWARD_REPAIR;
}

static int UiFxDuration() {
    if (gUiFx.kind >= UIFX_DIE_PLACE && gUiFx.kind <= UIFX_DIE_REMOVE) return UIFX_PLACE_MS;
    if (UiFxSnapshotActive()) return UIFX_REWARD_MS;
    if (gUiFx.kind == UIFX_PRUNE_DELETE || gUiFx.kind == UIFX_PRUNE_RESTORE) return UIFX_PRUNE_MS;
    return 0;
}

static void FinishUiFx() {
    if (gUiFx.kind == UIFX_NONE) return;
    int held = UiFxSnapshotActive();
    gUiFx.kind = UIFX_NONE;
    KillTimer(gWindow, UIFX_TIMER_ID);
    if (held && FxSnapshotHeld()) FxSnapshotRelease();
    if (gUiFxPendingDescent >= 0) {
        int floor = gUiFxPendingDescent;
        gUiFxPendingDescent = -1;
        BeginDescent(floor, -1);
    }
    InvalidateRect(gWindow, 0, FALSE);
}

static int BeginUiFx(int kind) {
    if (gFxLevel == FX_OFF) return 0;
    if (gUiFx.kind != UIFX_NONE) FinishUiFx();
    ZeroMemory(&gUiFx, sizeof(gUiFx));
    gUiFx.kind = kind;
    gUiFx.start = GetTickCount();
    gUiFx.die = gUiFx.face = gUiFx.displacedDie = gUiFx.fromSlot = gUiFx.toSlot = gUiFx.rewardIndex = -1;
    if (kind == UIFX_REWARD_FACE || kind == UIFX_REWARD_TSR || kind == UIFX_REWARD_REPAIR)
        CaptureUiFxSnapshot();
    SetTimer(gWindow, UIFX_TIMER_ID, FX_TIMER_MS, 0);
    return 1;
}

static int UiFxBlocksInput() { return UiFxSnapshotActive(); }

int FxDecorOn() { return gFxLevel != FX_OFF; }

int FxScale(int amount) {
    if (gFxLevel == FX_OFF) return 0;
    return gFxLevel == FX_REDUCED ? amount / 2 : amount;
}
static int gMouseInClient;
static void SyncUiFocus();

// This clock follows one actionable target, not mouse-move frequency. Keeping
// the reducer free of window/audio calls also makes dwell and reset rules
// testable with a fixed presentation clock.
struct UiFocusState {
    int id, scene, scope, turn, cued;
    DWORD since;
};
static UiFocusState gUiFocus = {-1, -1, -1, -1, 0, 0};
static int UpdateUiFocusState(UiFocusState* focus, int id, int scene, int scope, int turn, DWORD now) {
    if (focus->id == id && focus->scene == scene && focus->scope == scope && focus->turn == turn) return 0;
    focus->id = id; focus->scene = scene; focus->scope = scope; focus->turn = turn;
    focus->since = now; focus->cued = 0;
    return 1;
}
static int UiFocusAge(const UiFocusState& focus, DWORD now) {
    if (focus.id < 0) return -1;
    DWORD age = now - focus.since;
    return age > 0x7fffffffu ? 0x7fffffff : (int)age;
}
static int UiFocusCueDue(UiFocusState* focus, DWORD now) {
    if (focus->cued || UiFocusAge(*focus, now) < 170) return 0;
    focus->cued = 1;
    return 1;
}
int UiFocusElapsed() { return UiFocusAge(gUiFocus, GetTickCount()); }

// ---- corrupted-sector dice reveal (display only) --------------------------
// The turn's real result is already fixed in gGame.dice[].rolledFace by the
// seeded RNG in game.cpp; everything here only decides how it is revealed, so
// smoke/balance determinism is untouched. Every value below is a pure function
// of elapsed time -- WM_MOUSEMOVE repaints too, and state advanced per frame
// would make the effect race whenever the mouse moves.
#define NOISE_STAGGER_MS 80
#define NOISE_SCAN_MS 200      // opaque static, the face is not readable yet
#define NOISE_LOCK_MS 180      // the face tears its way through the static
#define NOISE_SETTLE_MS 260    // sequential lock, bounce and settling sparks
#define NOISE_TOTAL_MS (NOISE_SCAN_MS + NOISE_LOCK_MS)

static DWORD gReadStart;
int gReadActive, gReadLanded, gRolled;
static int gRollFloor = -1, gRollEncounter = -1, gRollTurn = -1;

int gCombatClearActive;
DWORD gCombatClearStart;
int gClearedFloor, gClearedEncounter;
int gTurnTraceActive, gTurnTracePendingClear;
static int gTurnTracePendingDeath;
DWORD gTurnTraceStart;
static int gTraceFloor, gTraceEncounter;
static DieState gTraceDice[3];

const DieState* DisplayDie(int index) {
    return gTurnTraceActive ? &gTraceDice[index] : &gGame.dice[index];
}

// ---- 피격·위독·정지 연출 ---------------------------------------------------
// 전투는 game.cpp 안에서 한 번에 끝난다. 그래서 "누가 언제 무엇을 했는지"는
// 규칙을 건드리지 않고 game.cpp가 남긴 CombatFxEvent 기록을 계산 재생 진행도에
// 맞춰 되짚는 방식으로 보여준다. 여기 있는 값은 전부 경과 시간의 함수라
// 마우스가 움직여 다시 그려져도 연출이 어긋나지 않는다.
#define TRACE_DEATH_HOLD_MS 900   // 마지막 줄을 읽을 틈을 준 뒤 화면이 무너진다
#define TRACE_FX_TAIL_MS 520      // 마지막 줄의 타격 연출이 끝날 때까지 더 그린다

static DWORD gEnemyStrikeAt[3];
static int gEnemyStrikeDamage[3];
static int gStrikeFired;            // 이미 달려든 이벤트 비트마스크
static int gFxSfxFired;             // 이미 소리를 낸 이벤트 비트마스크
static DWORD gPlayerHitAt;
static int gPlayerHitDamage, gPlayerHitBlockedAll;
static DWORD gLastGaspAt;           // 체력이 1로 떨어진 시각 (0 = 아님)

int gDeathActive;
DWORD gDeathStart;

int TurnTraceShown() {
    int count = gGame.turnTraceCount;
    if (!gTurnTraceActive) return count;
    int elapsed = (int)(GetTickCount() - gTurnTraceStart), shown = 0;
    while (shown < count && elapsed >= FxTraceAt(gGame, shown)) ++shown;
    return shown > count ? count : shown;
}

static void BeginPlayerHit(int damage) {
    gPlayerHitAt = GetTickCount();
    gPlayerHitDamage = damage;
    gPlayerHitBlockedAll = damage <= 0;
    // 완전 방어는 짧은 유리 공명음으로 구분한다.
    if (damage > 0) PlaySfx(SFX_PLAYER_HIT); else PlaySfx(SFX_SHIELD_BLOCK);
}

// ---- 전투 시각 이벤트 재생 -------------------------------------------------
// 이벤트 하나의 시작 시각은 그 사건이 적힌 계산 줄이 드러나는 순간이다.
//   eventStart = gTurnTraceStart + FxTraceAt(gGame, traceLine)
// 화면은 이 경과 시간만 읽어 위치와 강도를 계산한다 (프레임마다 쌓는 상태 없음).
int CombatFxPlaying() { return gTurnTraceActive; }

int CombatFxElapsed(int index) {
    if (!gTurnTraceActive) return -1;
    if (index < 0 || index >= (int)gGame.combatFxCount) return -1;
    return FxEventElapsed(gGame, index, (int)(GetTickCount() - gTurnTraceStart), FxDecorOn());
}

int CombatFxLeadElapsed(int index, int leadMs) {
    if (!gTurnTraceActive || index < 0 || index >= gGame.combatFxCount) return -1;
    return (int)(GetTickCount() - gTurnTraceStart)
        - FxTraceAt(gGame, FxTraceLine(gGame, gGame.combatFx[index].traceLine)) + leadMs;
}

// 사건이 실제로 일어나는 순간에만 한 번 울린다. 연출 시작이 아니라 결과가
// 확정되는 줄에 맞춰 나오므로 소리와 화면이 같은 사건을 가리킨다.
static void PlayCombatFxCue(const CombatFxEvent* fx) {
    switch (fx->type) {
    case CFX_AMPLIFY:       PlaySfx((fx->flags & CFXF_WASTED) ? SFX_SLOT_SET : SFX_CHARGE); break;
    case CFX_ATTACK_LAUNCH: PlaySfxPitched(SFX_EXECUTE, 4); break;
    case CFX_ENEMY_HIT:     PlaySfx((fx->flags & CFXF_BIG_HIT) ? SFX_HEAVY_HIT : SFX_HIT_IMPACT); break;
    case CFX_DEFEND:        PlaySfx(SFX_SHIELD_RISE); break;
    case CFX_CHAIN:         PlaySfxPitched((fx->flags & CFXF_DEFEND_CHAIN) ? SFX_SHIELD_RISE : SFX_CHAIN_ARC, fx->traceLine % 5); break;
    case CFX_BURN:          PlaySfxPitched(SFX_FX_QUARANTINE, 4); break;
    default: break;   // 적의 타격은 BeginPlayerHit이 낸다
    }
    if (fx->flags & CFXF_KILL) PlaySfx(SFX_ENEMY_DOWN);
}

// 계산 재생이 그 사건의 줄에 닿는 순간 소리와 달려들기를 발동한다. 재생을
// 건너뛰었으면(gTurnTraceActive = 0) 남은 사건을 그 자리에서 몰아 처리한다.
void SyncCombatFx() {
    int count = gGame.combatFxCount;
    if (count > COMBAT_FX_CAP) count = COMBAT_FX_CAP;
    for (int i = 0; i < count; ++i) {
        const CombatFxEvent* fx = &gGame.combatFx[i];
        if (gTurnTraceActive && CombatFxElapsed(i) < 0) continue;
        if (!(gFxSfxFired & (1 << i))) {
            gFxSfxFired |= 1 << i;
            if (gTurnTraceActive) PlayCombatFxCue(fx);
        }
        if (fx->type != CFX_ENEMY_STRIKE || (gStrikeFired & (1 << i))) continue;
        gStrikeFired |= 1 << i;
        int enemy = fx->targetEnemy;
        if (enemy >= 0 && enemy < 3) {
            gEnemyStrikeAt[enemy] = GetTickCount();
            gEnemyStrikeDamage[enemy] = fx->value;
        }
        BeginPlayerHit(fx->value);
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

static int GimmickShakeAmplitude();
static int BootShakeAmplitude();
static int BossIntroShakeAmplitude();
static int DeathShakeAmplitude();

static int ShakeAmplitude() {
    int fx = GimmickShakeAmplitude();
    int arrive = BossIntroShakeAmplitude();
    if (arrive > fx) fx = arrive;
    for (int i = 0; i < gGame.combatFxCount; ++i) {
        int t = CombatFxElapsed(i);
        const CombatFxEvent& event = gGame.combatFx[i];
        if (t < 0 || t >= 200 || !FxImpactHold(event)) continue;
        int kick = FxScale(((event.flags & CFXF_KILL) ? 7 : 4) * (200 - t) / 200);
        if (kick > fx) fx = kick;
    }
    int boot = BootShakeAmplitude();
    if (boot > fx) fx = boot;
    int death = DeathShakeAmplitude();
    if (death > fx) fx = death;
    if (!gPlayerHitAt) return fx;
    int since = (int)(GetTickCount() - gPlayerHitAt);
    if (since < 0 || since >= SHAKE_MS) return fx;
    int damage = gPlayerHitDamage > 14 ? 14 : gPlayerHitDamage;
    int peak = 3 + damage / 2;                       // 3 ~ 10픽셀
    int hit = FxScale(peak * (SHAKE_MS - since) / SHAKE_MS);
    return hit > fx ? hit : fx;
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

// 위독 노이즈는 오래 지속되므로 화면 전체가 빠르게 깜빡이지 않도록 느리게 섞인다.
int NoiseFrameStep() { return (int)(GetTickCount() / 90); }

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
    if (level > 680) level = 680;
    return FxScale(level);
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

// ---- 사망 연출 (DEATH-01) --------------------------------------------------
// 체력이 0이 되면 전장이 걷히고 이번 런의 기억이 한 줄씩 오염되어 부서진다.
// 그림은 screens.cpp가 DeathElapsed만 보고 그리므로 여기서는 시각·소리·흔들림만
// 정한다. 어둠이 오기 전까지는 건너뛸 수 없다 - 그 구간이 곧 죽음이다.
int DeathElapsed() {
    if (!gDeathActive) return DEATH_MS;
    int elapsed = (int)(GetTickCount() - gDeathStart);
    return elapsed < 0 ? 0 : elapsed > DEATH_MS ? DEATH_MS : elapsed;
}

// 줄 간격은 330ms에서 190ms로 좁혀지고, 마지막 줄 앞에서만 350ms 쉰다.
int DeathLineAt(int line) {
    int at = DEATH_ROT_AT;
    for (int i = 1; i <= line && i < DEATH_LINES; ++i)
        at += i == DEATH_LINES - 1 ? DEATH_LAST_GAP_MS : Lerp(330, 190, (i - 1) * 1000 / (DEATH_LINES - 3));
    return at;
}

// 박자마다 한 번씩 울린다. 줄이 부서지는 시점은 간격이 가속하므로 표로 적지 않고
// DeathLineAt에서 계산한다 (오염이 시작된 첫 글자가 쪼개지는 순간). 모두 시간순이다.
static int DeathCue(int index, int* sfx, int* pitch) {
    *pitch = 0;
    if (index == 0) { *sfx = SFX_GAMEOVER; return 0; }                      // 실행체 정지
    if (index == 1) { *sfx = SFX_CONFIRM; return DEATH_CMD_AT; }            // 회수 절차 실행
    index -= 2;
    if (index < DEATH_LINES) {                                               // 한 줄이 부서진다
        *sfx = SFX_DIE_LOCK; *pitch = index % 6;
        return DeathLineAt(index) + DEATH_ROT_HOLD_MS;
    }
    index -= DEATH_LINES;
    if (index == 0) { *sfx = SFX_FX_QUARANTINE; return DEATH_NAME_AT; }     // 이름이 오염된다
    if (index == 1) { *sfx = SFX_HEAVY_HIT; return DEATH_NAME_BREAK_AT; }   // 이름이 부서진다
    if (index == 2) { *sfx = SFX_CRASH; return DEATH_CUT_AT; }              // 화면이 끊긴다
    return -1;
}
static int gDeathCue;

// 줄이 부서질 때마다 작게, 이름이 부서지는 순간 한 번 크게 울린다. 화면 전체를
// 움직이는 것은 이 울림과 끝의 끊김뿐이다.
static int DeathShakeAmplitude() {
    if (!gDeathActive) return 0;
    int t = DeathElapsed(), amp = 0;
    int name = t - DEATH_NAME_BREAK_AT;
    if (name >= 0 && name < 320) amp = FxScale(9 * (320 - name) / 320);
    for (int i = 0; i < DEATH_LINES; ++i) {
        int since = t - DeathLineAt(i) - DEATH_ROT_HOLD_MS;
        if (since < 0 || since >= 140) continue;
        int kick = FxScale(3 * (140 - since) / 140);
        if (kick > amp) amp = kick;
    }
    return amp;
}

static void FinishDeath() {
    if (!gDeathActive) return;
    gDeathActive = 0;
    KillTimer(gWindow, 7);
    InvalidateRect(gWindow, 0, FALSE);
}

// 체력이 0이 된 직후. 끝나면 그 마지막 프레임이 그대로 사망 화면으로 남는다.
static void BeginDeath() {
    // 설정 화면을 강제로 닫으므로 "정말 다시 시작?" 확인 상태도 같이 풀어 준다.
    gGuideOpen = 0; gSettingsOpen = 0; gDeckOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0;
    gDeathCue = 0;
    gDeathStart = GetTickCount();
    gDeathActive = 1;
    SetTimer(gWindow, 7, FX_TIMER_MS, 0);
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
    SetTimer(gWindow, 3, FX_TIMER_MS, 0);
}

static int TurnTraceRevealDuration() {
    int count = gGame.turnTraceCount > 0 ? gGame.turnTraceCount : 1;
    return FxTraceAt(gGame, count);
}

static void BeginGimmickFx(int kind, int a, int b);

static void FinishTurnTrace() {
    if (!gTurnTraceActive) return;
    gTurnTraceActive = 0;
    KillTimer(gWindow, 4);
    SyncCombatFx();   // 재생을 건너뛰었어도 맞았다는 사실은 화면에 남는다
    if (gTurnTracePendingDeath) { gTurnTracePendingDeath = 0; BeginDeath(); }
    else if (gTurnTracePendingClear) BeginCombatClear(gTraceFloor, gTraceEncounter);
    // 새 턴 화면이 드러난 지금이 기믹 연출을 보여 줄 자리다.
    else if (gGame.boss.firedFx != GIMMICK_NONE)
        BeginGimmickFx(gGame.boss.firedFx, gGame.boss.fxA, gGame.boss.fxB);
    InvalidateRect(gWindow, 0, FALSE);
}

// ---- 기믹 발동 연출 -------------------------------------------------------
static int gFxActive, gFxKind, gFxA, gFxB;
static DWORD gFxStart, gFxShakeAt;
static int gFxShakePeak;

int GimmickFxKind() { return gFxActive ? gFxKind : 0; }
// 히트스톱. 착지 시점에 시간을 잠깐 얼린다. 모든 트랙이 이 값을 읽으므로
// 여기 한 곳에서 18종 전부가 함께 멈춘다. 총 벽시계 길이는 그만큼 늘어난다.
#define FX_HITSTOP_MS 110
static int gFxImpactPlayed;
int GimmickFxElapsed() {
    if (!gFxActive) return 0;
    int raw = (int)(GetTickCount() - gFxStart);
    int at = GimmickFxImpactAt(gFxKind, gFxB);
    if (at <= 0 || raw < at) return raw;
    if (raw < at + FX_HITSTOP_MS) return at;
    return raw - FX_HITSTOP_MS;
}
int GimmickFxA() { return gFxA; }
int GimmickFxB() { return gFxB; }

static void BeginGimmickFx(int kind, int a, int b) {
    if (kind <= 0 || kind >= GIMMICK_COUNT) return;
    gFxKind = kind; gFxA = a; gFxB = b;
    gFxStart = GetTickCount();
    gFxActive = 1;
    int family = BOSS_GIMMICK_INFO[kind].family;
    static const int FAMILY_SFX[] = {SFX_UI_CLICK, SFX_FX_LOCK, SFX_FX_RESTORE, SFX_FX_OFFLINE,
                                     SFX_FX_ROUTE, SFX_FX_PRESSURE, SFX_FX_QUARANTINE};
    // 같은 계열 안에서도 층이 깊을수록 낮게 울린다.
    int depth = (kind - 1) % 3;
    PlaySfxPitched(FAMILY_SFX[family], depth == 0 ? 2 : depth == 1 ? 1 : 0);
    gFxImpactPlayed = 0;
    // 흔들림은 임팩트(히트스톱) 시점에 건다. 매턴 반복되는 것들은 흔들지 않는다.
    int perTurn = kind == GIMMICK_TAPE_LOOP || kind == GIMMICK_NO_MEDIA || kind == GIMMICK_SIGNATURE || kind == GIMMICK_SEVENTEENTH || (kind == GIMMICK_TIMEOUT && !b);
    gFxShakeAt = 0;
    gFxShakePeak = perTurn ? 0 : (kind == GIMMICK_BLUE_SCREEN || kind == GIMMICK_ZERO_DAY
                || kind == GIMMICK_MASTER_BACKUP || kind == GIMMICK_OUT_OF_MEMORY ? 9 : 5);
    SetTimer(gWindow, 8, FX_TIMER_MS, 0);
}

static void FinishGimmickFx() {
    if (!gFxActive) return;
    gFxActive = 0;
    KillTimer(gWindow, 8);
    InvalidateRect(gWindow, 0, FALSE);
}

static int GimmickShakeAmplitude() {
    if (!gFxActive || gFxShakePeak <= 0 || !gFxShakeAt) return 0;
    int since = (int)(GetTickCount() - gFxShakeAt);
    if (since < 0 || since >= SHAKE_MS) return 0;
    return FxScale(gFxShakePeak * (SHAKE_MS - since) / SHAKE_MS);
}

static void BeginTurnTrace(int floor, int encounter, int pendingClear) {
    gTraceFloor = floor;
    gTraceEncounter = encounter;
    gTurnTracePendingClear = pendingClear;
    gTurnTracePendingDeath = gGame.phase == PHASE_GAMEOVER;
    gStrikeFired = 0;
    gFxSfxFired = 0;
    gTurnTraceStart = GetTickCount();
    gTurnTraceActive = 1;
    SetTimer(gWindow, 4, FX_TIMER_MS, 0);
}

// ---- drive mount / descent transition -------------------------------------
// 볼륨 마운트(런 시작)와 층 하강(보스 처치 후) 때 재생되는 탐색 연출.
// 게임 상태는 이미 game.cpp에서 확정된 뒤라 여기서는 보여주는 방식만 정한다.
int gDescentActive;
DWORD gDescentStart;
int gDescentToFloor;   // 진입하는 층 (0 = 최초 마운트)
int gDescentChoiceIndex;
static int gDescentSeekPhase; // 진행 중 한 번씩 울리는 섹터 안착 신호 단계

static void FinishDescent() {
    if (!gDescentActive) return;
    gDescentActive = 0;
    KillTimer(gWindow, 6);
    PlaySfx(SFX_BOOT);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginDescent(int toFloor, int choiceIndex) {
    gDescentToFloor = toFloor;
    gDescentChoiceIndex = choiceIndex;
    gDescentSeekPhase = 0;
    gDescentStart = GetTickCount();
    gDescentActive = 1;
    PlaySfx(SFX_READ_START);
    SetTimer(gWindow, 6, FX_TIMER_MS, 0);   // 5번은 오디오 펌프(AUDIO_TIMER_ID)가 쓴다
}

// ---- 디렉터리 진입 연출 ----------------------------------------------------
// 선택은 이미 game.cpp에서 확정됐고 전투도 시작된 뒤다. 여기서는 선택 카드를
// 경로에 잠근 뒤 라우팅 신호와 타이핑으로 전투 화면까지 이어 준다.
int gDirEnterActive, gDirEnterKind, gDirEnterChoiceIndex;
DWORD gDirEnterStart;
static int gDirEnterCuePhase;

static void FinishDirectoryEnter() {
    if (!gDirEnterActive) return;
    gDirEnterActive = 0;
    KillTimer(gWindow, 9);
    InvalidateRect(gWindow, 0, FALSE);
}

// 타이머 8은 기믹 발동 연출이 쓰고 있다. 같은 번호를 나눠 쓰면 둘 중 하나가
// 상대의 타이머를 죽여 연출이 멈추므로 진입 연출은 9번을 쓴다.
static void BeginDirectoryEnter(int kind, int choiceIndex) {
    gDirEnterKind = kind;
    gDirEnterChoiceIndex = choiceIndex;
    gDirEnterCuePhase = 0;
    gDirEnterStart = GetTickCount();
    gDirEnterActive = 1;
    PlaySfx(SFX_READ_START);
    SetTimer(gWindow, 9, FX_TIMER_MS, 0);
}

// ---- 보스 조우 연출 --------------------------------------------------------
// 보스 구역에는 디렉터리 2택이 없다. 두 번째 일반전의 보상을 고르면 판이 곧장
// 보스전으로 갈렸고, 만나는 장면 자체가 없었다. 여기서 잠긴 목적지를 실제로 연다.
// 판은 이미 보스전 상태라 (StartCombat이 먼저 끝나 있다) 연출은 그림과 소리만
// 맡고, 언제 건너뛰어도 결과가 같다.
int gBossIntroActive;
DWORD gBossIntroStart;

// 구간이 바뀌는 시점마다 한 번씩 울린다. 그림은 경과 시간만 보고 그려지므로
// 타이머가 할 일은 이 소리와 리페인트뿐이다 (삽입 연출과 같은 방식이다).
static const struct BossCue { int at; int sfx; int pitch; } BOSS_CUES[] = {
    { 0,                  SFX_CRASH,       0 },   // 판이 끊기고 경보가 올라온다
    { 200,                SFX_READ_START,  0 },   // 잠긴 목적지를 판독한다
    { BOSS_ALERT_MS - 140, SFX_DIE_LOCK,   5 },   // 마지막 조각이 확정된다
    { BOSS_GATE_AT,       SFX_DIE_LOCK,    0 },   // 잠금이 풀린다 (철컥)
    { BOSS_GATE_AT + 170, SFX_PRUNE,       0 },   // 문짝이 갈라지기 시작한다
    { BOSS_RISE_AT,       SFX_CHARGE,      0 },   // 안쪽에서 무언가 걸어 나온다
    { BOSS_LAND_AT,       SFX_HEAVY_HIT,   0 },   // 바닥을 딛는다
    { BOSS_LAND_AT + 60,  SFX_BOSS_ARRIVE, 0 },   // 보스 등장 신호
    { BOSS_NAME_AT,       SFX_SLOT_SET,    1 },   // 명패가 박힌다
    { BOSS_NAME_AT + 200, SFX_FX_LOCK,     0 },   // 기믹 도장이 찍힌다
    { BOSS_HAND_AT,       SFX_CONFIRM,     0 },   // 명패가 걷히고 전투판이 열린다
};
static int gBossIntroCue;
static int gBossArriveFired;   // 등장 신호가 이미 울렸는가 (건너뛰어도 한 번은 울린다)

// 바닥을 딛는 순간이 가장 크게 흔들리고, 문짝이 갈라지는 동안에는 낮게 떤다.
static int BossIntroShakeAmplitude() {
    if (!gBossIntroActive) return 0;
    int elapsed = (int)(GetTickCount() - gBossIntroStart);
    int land = elapsed - BOSS_LAND_AT;
    if (land >= 0 && land < 340) return FxScale(11 * (340 - land) / 340);
    if (elapsed >= BOSS_GATE_AT && elapsed < BOSS_GATE_AT + 170) return FxScale(4);
    if (elapsed >= BOSS_GATE_AT + 170 && elapsed < BOSS_RISE_AT) return FxScale(2);
    if (elapsed < 200) return FxScale(2 + (200 - elapsed) * 5 / 200);
    return 0;
}

static void FinishBossIntro() {
    if (!gBossIntroActive) return;
    gBossIntroActive = 0;
    KillTimer(gWindow, BOSS_INTRO_TIMER_ID);
    // 중간에 건너뛰었으면 등장 신호가 아직 울리지 않았다. 보스전이 예고 없이
    // 시작되지 않도록 그 한 소리는 반드시 남긴다.
    if (!gBossArriveFired) { gBossArriveFired = 1; PlaySfx(SFX_BOSS_ARRIVE); }
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginBossIntro() {
    if (gBossIntroActive) return;
    gGuideOpen = 0; gSettingsOpen = 0; gDeckOpen = 0;
    gBossIntroCue = 0;
    gBossArriveFired = 0;
    gBossIntroStart = GetTickCount();
    gBossIntroActive = 1;
    SetTimer(gWindow, BOSS_INTRO_TIMER_ID, FX_TIMER_MS, 0);
}

// ---- 새 게임 삽입 연출 -----------------------------------------------------
// 새 게임은 즉시 넘어가지 않는다. 지금 화면이 돌면서 줄어들어 플로피 한 장의
// 라벨이 되고, 그 디스크가 컴퓨터의 3.5인치 드라이브에 꽂힌 뒤 드라이브가 읽고
// 나서야 런이 만들어진다. 런을 끝에서 만드는 이유는 두 가지다. 연출이 붙잡는
// 스냅샷이 "누르기 직전의 화면"이어야 하고, 건너뛰어도 결과가 같아야 한다.
int gBootActive;
DWORD gBootStart;
static uint32_t gBootSeed;

// 구간이 바뀌는 시점마다 한 번씩 울린다. 그림은 경과 시간만 보고 그려지므로
// 타이머가 할 일은 이 소리와 리페인트뿐이다.
static const struct BootCue { int at; int sfx; int pitch; } BOOT_CUES[] = {
    { BOOT_SUCK_AT,        SFX_READ_START, 0 },   // 판이 빨려 들어가기 시작한다
    { BOOT_FLIP_AT - 140,  SFX_REWARD_SET, 0 },   // 디스크 한 장이 만들어진다
    { BOOT_FLY_AT,         SFX_DIE_PICK,   2 },   // 뒤집힌 디스크를 잡는다
    { BOOT_PUSH_AT,        SFX_SLOT_SET,   1 },   // 슬롯에 밀어 넣는다
    { BOOT_PUSH_AT + 200,  SFX_UI_CLICK,   0 },   // 중간에 한 번 걸린다
    { BOOT_CLUNK_AT,       SFX_DIE_LOCK,   0 },   // 철컥
    { BOOT_CLUNK_AT + 150, SFX_READ_START, 0 },
    { BOOT_CLUNK_AT + 380, SFX_DIE_LOCK,   3 },   // 헤드가 트랙을 옮긴다
    { BOOT_CLUNK_AT + 570, SFX_DIE_LOCK,   5 },
    { BOOT_SEEK_END,       SFX_PRUNE,      0 },   // 기계가 덮쳐 오며 화면 속으로 빨려 든다
    { BOOT_INSERT_MS - 150, SFX_CONFIRM,   0 },   // 다 삼킨 순간
};
static int gBootCue;

static void FinishBootInsert() {
    if (!gBootActive) return;
    gBootActive = 0;
    KillTimer(gWindow, 10);
    FxSnapshotRelease();
    NewRun(&gGame, gBootSeed, CampaignClearedMask(&gCampaign));
    SetSeenEndings(&gGame, CampaignSeenEndingMask(&gCampaign));
    PlaySfx(SFX_BOOT);
    InvalidateRect(gWindow, 0, FALSE);
}

static void BeginBootInsert() {
    if (gBootActive) return;
    gGuideOpen = 0; gSettingsOpen = 0; gDeckOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0;
    // 다른 연출이 붙잡아 둔 판이 남아 있으면 삽입 연출이 그 낡은 그림을 디스크에
    // 싣게 된다. 놓아 주고 첫 프레임에서 지금 화면을 새로 잡는다.
    if (FxSnapshotHeld()) FxSnapshotRelease();
    gBootSeed = GetTickCount() ^ (uint32_t)(ULONG_PTR)gWindow;
    gBootCue = 0;
    gBootStart = GetTickCount();
    gBootActive = 1;
    PlaySfx(SFX_PRUNE);            // 화면이 디스크로 빨려 들어가는 소리
    SetTimer(gWindow, 10, FX_TIMER_MS, 0);
}

// 화면이 갈라지는 동안 조금씩 세지고, 디스크가 물리는 철컥에서 한 번 크게 튄다.
static int BootShakeAmplitude() {
    if (!gBootActive) return 0;
    int elapsed = (int)(GetTickCount() - gBootStart);
    if (elapsed < BOOT_SUCK_AT) return FxScale(1 + elapsed * 5 / BOOT_GLITCH_MS);
    // 마지막 돌진: 기계가 가까워질수록 떨림이 세진다. 다가오는 것이 무거워 보인다.
    if (elapsed >= BOOT_SEEK_END) return FxScale(1 + Track(elapsed, BOOT_SEEK_END, BOOT_INSERT_MS) * 5 / 1000);
    int since = elapsed - BOOT_CLUNK_AT;
    if (since >= 0 && since < 260) return FxScale(9 * (260 - since) / 260);
    return 0;
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
    SetTimer(gWindow, 1, FX_TIMER_MS, 0);
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
    // Reading is presentation, not a tax the player must pay every turn.
    // Start it automatically; any key/click can still skip the animation.
    BeginRead();
}

static void TickRollAnimation() {
    int elapsed = ReadElapsed();
    for (int d = 0; d < 3; ++d) {
        if (!(gReadLanded & (1 << d)) && elapsed >= DieReadEnd(d)) { gReadLanded |= 1 << d; PlaySfxPitched(SFX_DIE_LOCK, d * 1); }
    }
    if (elapsed >= DieReadEnd(2)) StopRead();
    InvalidateRect(gWindow, 0, FALSE);
}

// Idle float, phase-shifted per slot so a group never breathes in sync.
int EnemyBob(int index) {
    int phase = (int)((GetTickCount() / 110 + (DWORD)index * 5) % 12u);
    if (phase > 6) phase = 12 - phase;
    return 3 - phase;
}

static int gIdleActive;
// 이미 문을 연 보스 구역 (볼륨, 층). 새 런에서 다시 -1로 돌아간다.
static int gBossIntroFloor = -1, gBossIntroDrive = -1;
static int gSceneKey = -1;
static DWORD gSceneStart;
static int VisibleSceneKey() {
    int phase = (gTurnTraceActive || gDeathActive || gCombatClearActive) ? PHASE_COMBAT : gGame.phase;
    return phase + 16 * (gGame.floor + 4 * gGame.encounter)
        + (phase == PHASE_STORY ? 256 * (gGame.story.kind + 16 * gGame.story.fragment + 256 * gGame.story.page) : 0);
}
int SceneElapsed() { return gSceneKey < 0 ? 1200 : (int)(GetTickCount() - gSceneStart); }
void SyncIdleAnimation() {
    SyncUiFocus();
    int key = VisibleSceneKey();
    // 보스 조우 연출은 그 층의 보스전이 처음 보이는 프레임에 한 번만 연다. 연출이
    // 도는 동안에는 장면 시계를 되돌려 두므로(아래 gSceneKey = -1) 씬이 바뀌었다는
    // 사실만으로는 두 번 여는 것을 막을 수 없다. 어느 볼륨 몇 층의 문을 이미
    // 열었는지 따로 적어 두고, 볼륨 선택으로 돌아가면(= 새 런) 그 기록을 지운다.
    if (gGame.phase == PHASE_TITLE || gGame.phase == PHASE_DRIVE_SELECT) { gBossIntroFloor = -1; gBossIntroDrive = -1; }
    // Count from the first visible frame, not while descent/install covers it.
    if (gDescentActive || gDirEnterActive || gBootActive || gBossIntroActive || UiFxSnapshotActive()) gSceneKey = -1;
    else if (key != gSceneKey) {
        gSceneKey = key; gSceneStart = GetTickCount();
        if (gWindow && !gTurnTraceActive && !gCombatClearActive && !gDeathActive) {
            if (gGame.phase == PHASE_COMBAT && gGame.encounter == 2
                && !(gBossIntroFloor == gGame.floor && gBossIntroDrive == gGame.selectedDrive)) {
                gBossIntroFloor = gGame.floor; gBossIntroDrive = gGame.selectedDrive;
                BeginBossIntro();
            }
            else if (gGame.phase == PHASE_REWARD) PlaySfx(SFX_LOOT_REVEAL);
        }
    }
    // 가이드가 열려 있으면 평소엔 리페인트를 멈추지만, 미판독 칸의 노이즈는
    // 계속 흔들려야 하므로 그때만 예외로 타이머를 살려 둔다.
    int wanted = ((FxDecorOn() || gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_DRIVE_SELECT
        || gGame.phase == PHASE_DIRECTORY || gGame.phase == PHASE_VICTORY || AmbientNoiseLevel() > 0)
        && !gGuideOpen && !gSettingsOpen && !gDeckOpen) || GuideNoiseActive();
    if (wanted == gIdleActive) return;
    gIdleActive = wanted;
    if (wanted) SetTimer(gWindow, 2, 33, 0); else KillTimer(gWindow, 2);
}

// 마지막 에필로그를 닫는 순간부터 결과 화면의 기록이 차례로 올라온다.
// 규칙 계층의 AdvanceStory는 시간이나 소리를 모르므로 UI 진입 처리는 여기서 맡는다.
static DWORD gVictoryStart;
int VictoryElapsed() {
    if (!gVictoryStart) return 3000;
    int elapsed = (int)(GetTickCount() - gVictoryStart);
    return elapsed < 0 ? 0 : elapsed;
}

// 지금 화면에 적용된 값을 그대로 담는다. 저장 시점의 UI 상태가 곧 설정이다.
static void CaptureSettings() {
    gSettings.language = (uint8_t)UiLanguage();
    gSettings.scalePercent = (uint8_t)WindowedScale();
    gSettings.fullscreen = (uint8_t)(gFullscreen ? 1 : 0);
    gSettings.fxLevel = (uint8_t)gFxLevel;
    gSettings.musicEnabled = (uint8_t)(AudioMusicEnabled() ? 1 : 0);
    gSettings.volume = (uint8_t)AudioVolume();
    gSettings.bgmVolume = (uint8_t)AudioMusicVolume();
    gSettings.sfxVolume = (uint8_t)AudioSfxVolume();
}

static void PersistSettings() {
    CaptureSettings();
    gSettingsSaveFailed = SaveSettings(&gSettings) ? 0 : 1;
}

// 읽어 온 값을 검사해 적용한다. 표에 없는 배율이나 범위 밖 연출 강도는 버리고
// 기본값을 쓴다. 창과 소리는 창이 선 뒤에야 만질 수 있다 (AudioOpen이 WM_CREATE
// 에서 잠금을 만들기 때문에, 그 전에 부르면 초기화되지 않은 잠금에 들어간다).
// 그래서 창을 만들기 전 호출은 applyWindow = 0으로 언어와 연출 강도만 세운다.
static void ApplySettings(int applyWindow) {
    SetUiLanguage(gSettings.language);
    if (gSettings.fxLevel < FX_LEVEL_COUNT) gFxLevel = gSettings.fxLevel;
    if (!applyWindow) return;
    for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i)
        if (SCALE_OPTIONS[i] == gSettings.scalePercent) { ApplyWindowedScale(SCALE_OPTIONS[i]); break; }
    if (gSettings.fullscreen) ApplyFullscreen(1);
    AudioSetMusicEnabled(gSettings.musicEnabled ? 1 : 0);
    SetAudioVolume(gSettings.volume);
    AudioSetMusicVolume(gSettings.bgmVolume);
    AudioSetSfxVolume(gSettings.sfxVolume);
}

// 화면을 벗어나면 세워 둔 후보는 남지 않는다. 다음 보상에서 첫 취소가 곧바로
// 포기가 되어 버리면 두 단계로 나눈 뜻이 없다.
static void ClearStaleConfirmations() {
    if (gGame.phase != PHASE_REWARD) { gRewardSkipArmed = 0; gTsrArmed = -1; gFaceSwapArmed = -1; }
    if (gGame.phase != PHASE_DIRECTORY) gDirectoryArmed = -1;
    if (gGame.phase != PHASE_ENDING_CHOICE) gEndingArmed = -1;
    if (gGame.phase != PHASE_PRUNE) ZeroMemory(gPruneTsrPending, sizeof(gPruneTsrPending));
}

static int CampaignSaveExistsBesideExecutable() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(0, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 0;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) lstrcpyW(slash + 1, L"AROGUE.SAV");
    DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void PersistCampaignProgress() {
    bool changed = RecordCampaignClears(&gCampaign, gGame.clearedMask);
    if (gGame.selectedDrive >= 0 && gGame.selectedDrive < 6 && RecordCampaignReach(&gCampaign, gGame.selectedDrive, gGame.floor)) changed = true;
    if (gGame.finalVolumeCleared && !gCampaign.finalCleared) { gCampaign.finalCleared = 1; changed = true; }
    if (RecordCampaignEnding(&gCampaign, CommittedEnding(&gGame))) changed = true;
    // 실패는 조용히 넘기지 않는다. 다음 저장이 성공하면 표시도 내려간다.
    if (changed && !gCampaignCorrupt) gSaveFailed = SaveCampaign(&gCampaign) ? 0 : 1;
    bool codexChanged = false;
    for (int i = 0; i < ENEMY_KIND_COUNT; ++i) {
        if (gGame.enemyScanned[i] && !gCodex[i]) { gCodex[i] = 1; codexChanged = true; }
        if (gCodex[i]) gGame.enemyScanned[i] = 1;
    }
    if (codexChanged && !SaveCodex(gCodex, ENEMY_KIND_COUNT)) gSaveFailed = 1;
}

// 세이브를 비우고 타이틀로 돌아간다. 진행 중이던 런의 clearedMask가 살아남으면
// 다음 클리어가 지운 조각을 다시 써 넣게 되므로, 런까지 함께 끝낸다.
int CampaignBestFloor(int drive) {
    return drive >= 0 && drive < 6 ? gCampaign.bestFloor[drive] : 0;
}

static void ResetCampaignProgress() {
    FinishDeath();
    FinishDirectoryEnter();
    FinishBossIntro();
    FinishUiFx();
    gUiFxPendingDescent = -1;
    gVictoryStart = 0;
    CampaignState previous = gCampaign;
    InitCampaign(&gCampaign);
    // Reset is the explicit authorization to replace a corrupt/old save. Do not
    // show 0/6 unless the replacement reached disk; roll memory back on failure.
    if (!SaveCampaign(&gCampaign)) {
        gCampaign = previous;
        gSaveFailed = 1;
        return;
    }
    gCampaignCorrupt = 0;
    gSaveFailed = 0;
    InitTitle(&gGame, 0, 0);
}

static void AdvanceStoryUi() {
    // 세 엔딩을 일일이 나열하면 넷째가 생길 때 조용히 빠진다. 규칙 계층이 이미
    // 답을 알고 있으므로 그쪽에 묻는다 (에필로그를 넘기는 순간이 결과 화면의 시작이다).
    int wasEnding = gGame.phase == PHASE_STORY && CommittedEnding(&gGame) >= 0;
    AdvanceStory(&gGame);
    PersistCampaignProgress();
    if (gGame.phase == PHASE_CHAPTER_CLEAR) PlaySfx(SFX_VICTORY);
    if (wasEnding && gGame.phase == PHASE_VICTORY) {
        gVictoryStart = GetTickCount();
        PlaySfx(SFX_VICTORY);
    }
}

static void SyncAudioScene() {
    AudioSetDrive(gGame.selectedDrive < 0 ? 0 : gGame.selectedDrive);
    int scene = MUSIC_SCENE_PLAY, intensity = 0;
    if (gGame.phase == PHASE_TITLE || gGame.phase == PHASE_DRIVE_SELECT) scene = MUSIC_SCENE_TITLE;
    else if (gGame.phase == PHASE_STORY || gGame.phase == PHASE_ENDING_CHOICE) scene = MUSIC_SCENE_STORY;
    else if (gGame.phase == PHASE_GAMEOVER) scene = MUSIC_SCENE_GAMEOVER;
    else if (gGame.phase == PHASE_VICTORY || gGame.phase == PHASE_CHAPTER_CLEAR) scene = MUSIC_SCENE_VICTORY;
    if (gGame.phase == PHASE_COMBAT) {
        intensity = 1;
        for (int i = 0; i < gGame.enemyCount; ++i) if (gGame.enemies[i].alive && IsBossKind(gGame.enemies[i].kind)) {
            intensity = gGame.enemies[i].hp * 2 <= gGame.enemies[i].maxHp ? 3 : 2;
            break;
        }
    }
    AudioSetScene(scene); AudioSetIntensity(intensity);
    AudioSetCritical(gGame.playerHp > 0 && gGame.playerHp <= CRITICAL_HP);
    AudioSetEnding(gGame.story.selectedEnding);
}

static void BeginNewRun() {
    gReplayPage = 0;
    FinishDeath();
    FinishDirectoryEnter();
    gUiFxPendingDescent = -1;
    gVictoryStart = 0;
    FinishUiFx();
    gStrikeFired = 0; gFxSfxFired = 0; gPlayerHitAt = 0; gLastGaspAt = 0;
    for (int i = 0; i < 3; ++i) { gEnemyStrikeAt[i] = 0; gEnemyStrikeDamage[i] = 0; }
    // 판을 갈아엎는 것은 연출이 끝날 때다. 그때까지 화면에는 누르기 직전의 판이 남는다.
    BeginBootInsert(); InvalidateRect(gWindow, 0, FALSE);
    for (int i = 0; i < ENEMY_KIND_COUNT; ++i) if (gCodex[i]) gGame.enemyScanned[i] = 1;
}

static int IsEndScreen() {
    return gGame.phase == PHASE_GAMEOVER || gGame.phase == PHASE_VICTORY
        || gGame.phase == PHASE_CHAPTER_CLEAR;
}

static void ContinueFromEnd() {
    PersistCampaignProgress();
    BeginNewRun();
}

static void ExecuteCombatTurn() {
    FinishUiFx();
    int floor = gGame.floor, encounter = gGame.encounter;
    int turn = gGame.turn;
    GamePhase before = gGame.phase;
    for (int i = 0; i < 3; ++i) gTraceDice[i] = gGame.dice[i];
    EndTurn(&gGame);
    PersistCampaignProgress();
    int resolved = before == PHASE_COMBAT && (gGame.phase != before || gGame.turn != turn);
    int cleared = LivingEnemyCount(&gGame) == 0;
    if (resolved) BeginTurnTrace(floor, encounter, cleared);
    // 정지음과 화면 붕괴는 계산 재생이 끝난 뒤 BeginDeath가 맡는다.
    if (gGame.phase == PHASE_GAMEOVER) { if (!resolved) BeginDeath(); }
    else if (gGame.phase == PHASE_VICTORY) PlaySfx(SFX_VICTORY);
    // 처치음은 계산 재생이 [적중] 줄에 닿는 순간 이벤트가 낸다. 재생 없이
    // 결과만 바뀐 경로(미리보기 밖의 예외)에서만 여기서 울린다.
    else if (!resolved && before != gGame.phase) PlaySfx(SFX_ENEMY_DOWN);
    else PlaySfx(SFX_EXECUTE);
}

static void KeybRerollSelected() {
    if (!gRolled || gGame.selectedDie < 0) return;
    if (!TacticalRerollAvailable(&gGame)) return;
    KeybReroll(&gGame, gGame.selectedDie);
    PlaySfxPitched(SFX_DIE_LOCK, 3);
}

static void ClickCombat(int x, int y) {
    if (Inside(ReadButtonRect(), x, y)) { BeginRead(); return; }
    if (TacticalRerollAvailable(&gGame) && Inside(KeybButtonRect(), x, y)) { KeybRerollSelected(); return; }
    if (!gRolled) return;
    for (int i = 0; i < gGame.enemyCount; ++i) if (!GimmickSummonPending(i) && Inside(EnemyRect(i), x, y)) { SelectEnemy(&gGame, i); PlaySfx(SFX_TARGET); return; }
    for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) { gGame.selectedDie = i; PlaySfxPitched(SFX_DIE_PICK, i * 2); return; }
    for (int i = 0; i < SLOT_COUNT; ++i) if (Inside(SlotRect(i), x, y)) {
        if (gGame.selectedDie >= 0) {
            int die = gGame.selectedDie;
            int oldSlot = gGame.dice[die].assignedSlot;
            int oldFace = gGame.dice[die].rolledFace;
            int displaced = -1;
            for (int d = 0; d < 3; ++d)
                if (d != die && gGame.dice[d].assignedSlot == i) displaced = d;
            // 잠긴 슬롯 등으로 배치가 거부되면 성공 효과음을 재생하지 않는다.
            if (AssignDieToSlot(&gGame, die, i)) {
                int newSlot = gGame.dice[die].assignedSlot;
                int kind = newSlot < 0 ? UIFX_DIE_REMOVE : oldSlot >= 0 ? UIFX_DIE_MOVE : UIFX_DIE_PLACE;
                if (BeginUiFx(kind)) {
                    gUiFx.die = die;
                    gUiFx.displacedDie = displaced;
                    gUiFx.fromSlot = oldSlot;
                    gUiFx.toSlot = newSlot;
                    gUiFx.valueBefore = oldFace;
                    gUiFx.valueAfter = gGame.dice[die].rolledFace;
                }
                PlaySfxPitched(SFX_SLOT_SET, i * 2);
            }
            else PlaySfx(SFX_UI_CLICK);
        }
        else { int die = DieForSlotUI(i); if (die >= 0) gGame.selectedDie = die; } return;
    }
    if (Inside(EndTurnRect(), x, y)) {
        ExecuteCombatTurn();
    }
}

static void TakeTsrReward(int index) {
    if (index < 0 || index >= 3 || !gGame.rewardIsTsr) return;
    int tsr = gGame.rewardKinds[index];
    if (tsr < 0 || tsr >= TSR_COUNT || gGame.tsrInstalled[tsr]) return;
    gTsrArmed = -1;
    int animated = BeginUiFx(UIFX_REWARD_TSR);
    if (animated) { gUiFx.rewardIndex = index; gUiFx.valueAfter = tsr; }
    InstallTsr(&gGame, index);
    PlaySfx(SFX_REWARD_SET);
}

static void TakeRepairReward() {
    if (!CanRepairSector()) return;
    int before = gGame.playerHp;
    int animated = BeginUiFx(UIFX_REWARD_REPAIR);
    if (animated) { gUiFx.rewardIndex = REWARD_REPAIR; gUiFx.valueBefore = before; }
    RepairSector(&gGame);
    if (animated) gUiFx.valueAfter = gGame.playerHp;
    PlaySfx(SFX_REPAIR);
}

// 디렉터리 카드를 고른다. 실패(잘못된 index)면 아무 일도 일어나지 않는다.
static void TakeDirectory(int index) {
    if (index < 0 || index >= DirectoryChoiceCount(&gGame)) return;
    int kind = gGame.directory.choices[index].kind;
    gDirectoryArmed = -1;
    SelectDirectoryChoice(&gGame, index);
    if (gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_STORY) { PlaySfx(SFX_CONFIRM); BeginDirectoryEnter(kind, index); }
}

// 진입은 되돌릴 수 없고 선택지도 다시 뽑히지 않는다. 상세를 읽으려다 스친
// 클릭이 곧바로 확정되지 않도록 첫 입력은 후보만 세운다.
static void ArmOrTakeDirectory(int index) {
    if (index < 0 || index >= DirectoryChoiceCount(&gGame)) return;
    if (gDirectoryArmed != index) { gDirectoryArmed = index; PlaySfxPitched(SFX_DIE_PICK, index * 2); return; }
    TakeDirectory(index);
}

static void ClickDirectory(int x, int y) {
    for (int i = 0; i < DirectoryChoiceCount(&gGame); ++i)
        if (Inside(DirectoryChoiceRect(i), x, y)) { ArmOrTakeDirectory(i); return; }
    gDirectoryArmed = -1;
}

static void CycleReplayPage(int delta) {
    if ((gGame.clearedMask & 0x3F) != 0x3F) return;
    gReplayPage = (gReplayPage + delta) % 3;
    if (gReplayPage < 0) gReplayPage += 3;
    SetReplayDrivePage(&gGame, gReplayPage);
    PlaySfx(SFX_UI_CLICK);
    InvalidateRect(gWindow, 0, FALSE);
}

static void ClickDriveSelect(int x, int y) {
    if ((gGame.clearedMask & 0x3F) == 0x3F) {
        if (Inside(ReplayPrevRect(), x, y)) { CycleReplayPage(-1); return; }
        if (Inside(ReplayNextRect(), x, y)) { CycleReplayPage(1); return; }
    }
    for (int i = 0; i < gGame.driveChoiceCount; ++i) if (Inside(DriveCardRect(i), x, y)) {
        SelectDrive(&gGame, i);
        if (gGame.phase == PHASE_DIRECTORY) { PlaySfx(SFX_CONFIRM); BeginDescent(0, i); }
        return;
    }
}

// 보상 포기는 되돌릴 수 없다. 첫 입력은 버튼을 무장만 시키고, 같은 입력이 한 번
// 더 와야 실제로 포기한다. 무장 중에 다른 곳을 만지면 그대로 풀린다.
static void ArmOrConfirmRewardSkip() {
    if (!gRewardSkipArmed) { gRewardSkipArmed = 1; PlaySfx(SFX_UI_CLICK); return; }
    gRewardSkipArmed = 0;
    SkipReward(&gGame);
    PlaySfx(SFX_UI_CLICK);
}

// 상주 프로그램은 용량을 먹고 이번 층에서는 정리 화면까지 가야 내릴 수 있다.
// 카드를 비교하다 스친 클릭으로 설치되지 않도록 첫 입력은 후보만 세운다.
static void ArmOrTakeTsrReward(int index) {
    if (index < 0 || index >= 3 || !gGame.rewardIsTsr) return;
    int tsr = gGame.rewardKinds[index];
    if (tsr < 0 || tsr >= TSR_COUNT || gGame.tsrInstalled[tsr]) return;
    if (gTsrArmed != index) { gTsrArmed = index; PlaySfxPitched(SFX_REWARD_PICK, index * 2); return; }
    TakeTsrReward(index);
}

// 면 교체는 덱을 영구히 바꾼다. 덮을 자리를 고르는 것과 실제로 덮는 것을 나눈다.
static void InstallRewardOnFace(int d, int f) {
    int reward = gGame.selectedReward;
    if (reward < 0) return;
    int animated = BeginUiFx(UIFX_REWARD_FACE);
    if (animated) {
        gUiFx.rewardIndex = reward; gUiFx.die = d; gUiFx.face = f;
        gUiFx.shownFace.kind = (uint8_t)gGame.rewardKinds[reward];
        gUiFx.shownFace.value = (uint8_t)gGame.rewardValues[reward];
    }
    gFaceSwapArmed = -1;
    InstallSelectedReward(&gGame, d, f);
    PlaySfx(SFX_REWARD_SET);
}

static void ArmOrInstallRewardOnFace(int d, int f) {
    int cell = d * 6 + f;
    if (gFaceSwapArmed != cell) { gFaceSwapArmed = cell; PlaySfx(SFX_DIE_PICK); return; }
    InstallRewardOnFace(d, f);
}

static void ClickReward(int x, int y) {
    if (Inside(RewardRect(REWARD_REPAIR, BASE_WIDTH), x, y)) {
        gRewardSkipArmed = 0;
        TakeRepairReward();
        return;
    }
    if (gGame.rewardIsTsr) {
        // 보스 전리품: 첫 입력은 후보, 같은 카드를 다시 누르면 설치 확정이다.
        for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) { gRewardSkipArmed = 0; ArmOrTakeTsrReward(i); return; }
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { ArmOrConfirmRewardSkip(); return; }
        gRewardSkipArmed = 0; gTsrArmed = -1;
        return;
    }
    // 보상 카드를 바꾸면 세워 둔 교체 자리는 뜻을 잃는다.
    for (int i = 0; i < 3; ++i) if (Inside(RewardRect(i, BASE_WIDTH), x, y)) { gRewardSkipArmed = 0; gFaceSwapArmed = -1; SelectReward(&gGame, i); PlaySfxPitched(SFX_REWARD_PICK, i * 2); return; }
    if (gGame.selectedReward >= 0) for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) {
        gRewardSkipArmed = 0;
        ArmOrInstallRewardOnFace(d, f);
        return;
    }
    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) { ArmOrConfirmRewardSkip(); return; }
    gRewardSkipArmed = 0; gFaceSwapArmed = -1;
}

static void ClickPrune(int x, int y) {
    int tsrCount = InstalledTsrCount(&gGame);
    for (int i = 0; i < tsrCount && i < 4; ++i) if (Inside(PruneTsrRect(i), x, y)) {
        int tsr = InstalledTsrAt(&gGame, i);
        if (tsr >= 0 && tsr < TSR_COUNT) {
            gPruneTsrPending[tsr] ^= 1;
            PlaySfx(gPruneTsrPending[tsr] ? SFX_PRUNE : SFX_REWARD_SET);
            InvalidateRect(gWindow, 0, FALSE);
        }
        return;
    }
    for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f) if (Inside(FaceGridRect(d, f), x, y)) {
        int undo = CanUndoPrunedFace(&gGame, d, f);
        Face before = gGame.dice[d].faces[f];
        if (before.kind == FACE_EMPTY && !undo) return;
        PruneFace(&gGame, d, f);
        if (BeginUiFx(undo ? UIFX_PRUNE_RESTORE : UIFX_PRUNE_DELETE)) {
            gUiFx.die = d; gUiFx.face = f;
            gUiFx.shownFace = undo ? gGame.dice[d].faces[f] : before;
        }
        PlaySfx(undo ? SFX_REWARD_SET : SFX_PRUNE);
        return;
    }
    if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) {
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
}

// 현재 페이즈에서 (x, y)가 어떤 상호작용 가능한 사각형 위에 있는지 식별하는 id를 반환한다.
// -1은 "호버 없음". 마우스가 움직여도 이 id가 바뀌지 않으면 화면을 다시 그릴 필요가 없다.
static int HoverId(int x, int y) {
    if (gTermOpen || gDeathActive || gBootActive || UiFxBlocksInput() || gTurnTraceActive
        || gDescentActive || gDirEnterActive || gBossIntroActive || gCombatClearActive) return -1;
    if (gGame.phase != PHASE_TITLE && Inside(DeckButtonRect(BASE_WIDTH), x, y)) return 1000;
    if (gDeckOpen) return Inside(DeckCloseRect(BASE_WIDTH), x, y) ? 1001 : -1;
    if (Inside(SettingsButtonRect(BASE_WIDTH), x, y)) return 900;
    if (gSettingsOpen) {
        if (Inside(SettingsCloseRect(BASE_WIDTH), x, y)) return 901;
        for (int i = 0; i < LANGUAGE_COUNT; ++i) if (Inside(LanguageOptionRect(i), x, y)) return 902 + i;
        for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) if (Inside(ScaleOptionRect(i), x, y)) return 910 + i;
        for (int i = 0; i < FX_LEVEL_COUNT; ++i) if (Inside(FxLevelRect(i), x, y)) return 930 + i;
        if (Inside(BgmToggleRect(), x, y)) return 944;
        for (int channel = 0; channel < AUDIO_VOLUME_COUNT; ++channel)
            if (Inside(VolumeSliderRect(channel), x, y)) return 940 + channel;
        if (Inside(FullscreenToggleRect(), x, y)) return 920;
        if (Inside(RestartButtonRect(), x, y)) return 921;
        if (Inside(CampaignResetRect(), x, y)) return 922;
        return -1;
    }
    if (Inside(GuideButtonRect(BASE_WIDTH), x, y)) return 800;
    if (gGuideOpen) {
        if (Inside(GuideCloseRect(BASE_WIDTH), x, y)) return 801;
        if (gGuidePage > 0 && Inside(GuidePrevRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 802;
        if (gGuidePage < 1 && Inside(GuideNextRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 803;
        return -1;
    }
    if (RollBlocking()) return -1;
    if (gGame.phase == PHASE_TITLE) {
        if (Inside(StartButtonRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 0;
        return -1;
    }
    if (gGame.phase == PHASE_STORY) {
        return Inside(StoryNextRect(BASE_WIDTH, BASE_HEIGHT), x, y) ? 40 : -1;
    }
    if (gGame.phase == PHASE_DRIVE_SELECT) {
        for (int i = 0; i < gGame.driveChoiceCount; ++i) if (Inside(DriveCardRect(i), x, y)) return 50 + i;
        return -1;
    }
    if (gGame.phase == PHASE_ENDING_CHOICE) {
        for (int i = 0; i < ENDING_COUNT; ++i) if (Inside(EndingChoiceRect(i), x, y)) return 60 + i;
        if (gEndingArmed >= 0 && Inside(EndingConfirmRect(), x, y)) return 70;
        return -1;
    }
    if (IsEndScreen()
        && Inside(EndingRestartRect(), x, y)) return 70;
    if (gGame.phase == PHASE_DIRECTORY) {
        for (int i = 0; i < DirectoryChoiceCount(&gGame); ++i) if (Inside(DirectoryChoiceRect(i), x, y)) return 80 + i;
        return -1;
    }
    if (gGame.phase == PHASE_COMBAT) {
        if (!gRolled && !gReadActive && Inside(ReadButtonRect(), x, y)) return 420;
        if (!gRolled) return -1;
        for (int i = 0; i < gGame.enemyCount; ++i)
            if (gGame.enemies[i].alive && !GimmickSummonPending(i) && Inside(EnemyRect(i), x, y)) return 100 + i;
        for (int i = 0; i < 3; ++i) if (Inside(DieRect(i), x, y)) return 200 + i;
        for (int i = 0; i < SLOT_COUNT; ++i)
            if ((gGame.selectedDie >= 0 ? !SlotLockedThisTurn(&gGame, i) : DieForSlotUI(i) >= 0)
                && Inside(SlotRect(i), x, y)) return 300 + i;
        if (Inside(EndTurnRect(), x, y)) return 400;
        if (IsTsrInstalled(&gGame, TSR_KEYB) && !gGame.keybUsedThisTurn && gGame.selectedDie >= 0
            && Inside(KeybButtonRect(), x, y)) return 410;
        return -1;
    }
    if (gGame.phase == PHASE_REWARD) {
        for (int i = 0; i < 3; ++i) {
            int tsr = gGame.rewardKinds[i];
            int usable = !gGame.rewardIsTsr || (tsr >= 0 && tsr < TSR_COUNT && !gGame.tsrInstalled[tsr]);
            if (usable && Inside(RewardRect(i, BASE_WIDTH), x, y)) return 500 + i;
        }
        if (CanRepairSector() && Inside(RewardRect(REWARD_REPAIR, BASE_WIDTH), x, y)) return 500 + REWARD_REPAIR;
        if (!gGame.rewardIsTsr && gGame.selectedReward >= 0)
            for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f)
                if (Inside(FaceGridRect(d, f), x, y)) return 600 + d * 6 + f;
        if (Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 700;
        return -1;
    }
    if (gGame.phase == PHASE_PRUNE) {
        int tsrCount = InstalledTsrCount(&gGame);
        for (int i = 0; i < tsrCount && i < 4; ++i) if (Inside(PruneTsrRect(i), x, y)) return 640 + i;
        for (int d = 0; d < 3; ++d) for (int f = 0; f < 6; ++f)
            if ((gGame.dice[d].faces[f].kind != FACE_EMPTY || CanUndoPrunedFace(&gGame, d, f))
                && Inside(FaceGridRect(d, f), x, y)) return 600 + d * 6 + f;
        if (UsedBytes(&gGame) <= EffectiveCapacity(&gGame) && NonEmptyFaceCount(&gGame) > 0
            && Inside(ContinueRect(BASE_WIDTH, BASE_HEIGHT), x, y)) return 700;
        return -1;
    }
    return -1;
}

static void SyncUiFocus() {
    int scope = gDeckOpen | (gSettingsOpen << 1) | (gGuideOpen << 2) | (gTermOpen << 3)
        | (gGuidePage << 4) | (gDeathActive << 6) | (gBootActive << 7)
        | (UiFxSnapshotActive() << 8) | (gTurnTraceActive << 9) | (gDescentActive << 10)
        | (gDirEnterActive << 11) | (gCombatClearActive << 12) | (gReadActive << 13)
        | (gBossIntroActive << 14);
    int hover = gMouseInClient && gVolumeDragging < 0 ? HoverId(gMouse.x, gMouse.y) : -1;
    if (UpdateUiFocusState(&gUiFocus, hover, VisibleSceneKey(), scope,
            gGame.phase == PHASE_COMBAT ? gGame.turn : -1, GetTickCount())) {
        if (gWindow) InvalidateRect(gWindow, 0, FALSE);
    }
}

static void TickUiFocus() {
    SyncUiFocus();
    if (UiFocusCueDue(&gUiFocus, GetTickCount())) PlaySfx(SFX_UI_FOCUS);
}

static void HandleClick(int x, int y);

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

static void HandleClick(int x, int y) {
    // 사망 연출은 어둠이 오기 전까지 건너뛸 수 없다. 그 뒤로는 대사를 다 채운
    // 사망 화면으로 넘어가고, 한 번 더 눌러야 새 런이 시작된다.
    if (gDeathActive) { if (DeathElapsed() >= DEATH_DARK_AT) FinishDeath(); return; }
    if (UiFxBlocksInput()) return;
    int skippedOne = 0;
    if (gBootActive) { FinishBootInsert(); skippedOne = 1; }
    else if (gTurnTraceActive) { FinishTurnTrace(); skippedOne = 1; }
    else if (gDescentActive) { FinishDescent(); skippedOne = 1; }
    else if (gDirEnterActive) { FinishDirectoryEnter(); skippedOne = 1; }
    else if (gBossIntroActive) { FinishBossIntro(); skippedOne = 1; }
    else if (gCombatClearActive) { FinishCombatClear(); skippedOne = 1; }
    if (skippedOne && (gDeathActive || UiFxBlocksInput() || gTurnTraceActive || gDescentActive
        || gDirEnterActive || gBossIntroActive || gCombatClearActive || gBootActive)) return;
    if (gDeckOpen) {
        if (Inside(DeckCloseRect(BASE_WIDTH), x, y) || Inside(DeckButtonRect(BASE_WIDTH), x, y)) gDeckOpen = 0;
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (gGame.phase != PHASE_TITLE && Inside(DeckButtonRect(BASE_WIDTH), x, y)) { gDeckOpen = 1; gGuideOpen = 0; gSettingsOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gSettingsOpen) {
        if (Inside(RestartButtonRect(), x, y)) {
            gCampaignResetArmed = 0;
            if (gRestartArmed) { gRestartArmed = 0; gSettingsOpen = 0; BeginNewRun(); InvalidateRect(gWindow, 0, FALSE); return; }
            gRestartArmed = 1; InvalidateRect(gWindow, 0, FALSE); return;
        }
        if (Inside(CampaignResetRect(), x, y)) {
            gRestartArmed = 0;
            if (gCampaignResetArmed) { gCampaignResetArmed = 0; gSettingsOpen = 0; ResetCampaignProgress(); InvalidateRect(gWindow, 0, FALSE); return; }
            gCampaignResetArmed = 1; InvalidateRect(gWindow, 0, FALSE); return;
        }
        gRestartArmed = 0; gCampaignResetArmed = 0;
        if (Inside(SettingsCloseRect(BASE_WIDTH), x, y) || Inside(SettingsButtonRect(BASE_WIDTH), x, y)) { gSettingsOpen = 0; PersistSettings(); InvalidateRect(gWindow, 0, FALSE); return; }
        for (int i = 0; i < LANGUAGE_COUNT; ++i) if (Inside(LanguageOptionRect(i), x, y)) {
            // Re-read the external table when a language is selected so copy
            // edits can be previewed without recompiling or restarting.
            LoadTranslations();
            SetUiLanguage(i);
            // 번역 표가 없으면 English 요청은 거부된다. 창 제목은 실제로 적용된
            // 언어를 따라가야 하므로 요청이 아니라 결과를 읽는다.
            SetWindowTextW(gWindow, UiLanguage() == LANGUAGE_ENGLISH ? L"A:\\ROGUE · 1.44MB · English" : L"A:\\ROGUE · 1.44MB");
            PlaySfx(SFX_UI_CLICK); InvalidateRect(gWindow, 0, FALSE); return;
        }
        for (int i = 0; i < SETTINGS_SCALE_COUNT; ++i) if (Inside(ScaleOptionRect(i), x, y)) { ApplyWindowedScale(SCALE_OPTIONS[i]); InvalidateRect(gWindow, 0, FALSE); return; }
        for (int i = 0; i < FX_LEVEL_COUNT; ++i) if (Inside(FxLevelRect(i), x, y)) { gFxLevel = i; PlaySfx(SFX_UI_CLICK); InvalidateRect(gWindow, 0, FALSE); return; }
        if (Inside(BgmToggleRect(), x, y)) { AudioSetMusicEnabled(!AudioMusicEnabled()); PlaySfx(SFX_UI_CLICK); InvalidateRect(gWindow, 0, FALSE); return; }
        // 슬라이더는 누른 순간 값이 따라오고, 놓을 때까지 커서를 붙잡는다.
        // 미리듣기는 놓는 순간에만 울린다. 끄는 동안 계속 울리면 시끄럽다.
        for (int channel = 0; channel < AUDIO_VOLUME_COUNT; ++channel) {
            if (!Inside(VolumeSliderRect(channel), x, y)) continue;
            gVolumeDragging = channel;
            gVolumeKeyboardChannel = channel;
            SetCapture(gWindow);
            SetAudioChannelVolume(channel, VolumeFromX(channel, x));
            InvalidateRect(gWindow, 0, FALSE); return;
        }
        if (Inside(FullscreenToggleRect(), x, y)) { ApplyFullscreen(!gFullscreen); InvalidateRect(gWindow, 0, FALSE); return; }
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(SettingsButtonRect(BASE_WIDTH), x, y)) { gSettingsOpen = 1; gGuideOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) {
        if (Inside(GuideCloseRect(BASE_WIDTH), x, y) || Inside(GuideButtonRect(BASE_WIDTH), x, y)) gGuideOpen = 0;
        else if (Inside(GuidePrevRect(BASE_WIDTH, BASE_HEIGHT), x, y) && gGuidePage > 0) { --gGuidePage; PlaySfx(SFX_UI_CLICK); }
        else if (Inside(GuideNextRect(BASE_WIDTH, BASE_HEIGHT), x, y) && gGuidePage < 1) { ++gGuidePage; PlaySfx(SFX_UI_CLICK); }
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (Inside(GuideButtonRect(BASE_WIDTH), x, y)) { gGuideOpen = 1; gSettingsOpen = 0; gGuidePage = 0; gRestartArmed = 0; gCampaignResetArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
    int floorBefore = gGame.floor;
    if (gGame.phase == PHASE_TITLE) { if (Inside(StartButtonRect(BASE_WIDTH, BASE_HEIGHT), x, y)) BeginNewRun(); }
    // 스토리는 [다음] 버튼에서만 넘어간다. 패널 아무 곳이나 눌러 넘기면
    // 읽는 중 잘못 누른 클릭으로 기록이 사라진다.
    else if (gGame.phase == PHASE_STORY) { if (Inside(StoryNextRect(BASE_WIDTH, BASE_HEIGHT), x, y)) AdvanceStoryUi(); }
    else if (gGame.phase == PHASE_ENDING_CHOICE) {
        // 캠페인 전체에서 가장 되돌릴 수 없는 한 번이다. 카드는 후보만 세우고
        // 실행은 아래 확정 버튼에서만 받는다.
        int hitCard = 0;
        for (int i = 0; i < ENDING_COUNT; ++i) if (Inside(EndingChoiceRect(i), x, y)) {
            if (gEndingArmed != i) { gEndingArmed = i; PlaySfxPitched(SFX_REWARD_PICK, i * 2); }
            hitCard = 1; break;
        }
        if (!hitCard && gEndingArmed >= 0 && Inside(EndingConfirmRect(), x, y)) {
            int ending = gEndingArmed;
            gEndingArmed = -1;
            SelectEnding(&gGame, ending);
            PlaySfx(SFX_CONFIRM);
        }
    }
    else if (gGame.phase == PHASE_DRIVE_SELECT) ClickDriveSelect(x, y);
    else if (gGame.phase == PHASE_DIRECTORY) ClickDirectory(x, y);
    else if (gGame.phase == PHASE_COMBAT) ClickCombat(x, y); else if (gGame.phase == PHASE_REWARD) ClickReward(x, y);
    else if (gGame.phase == PHASE_PRUNE) ClickPrune(x, y);
    else if (IsEndScreen() && Inside(EndingRestartRect(), x, y)) ContinueFromEnd();
    ClearStaleConfirmations();
    PersistCampaignProgress();
    // 층이 실제로 올라간 클릭(보상/정리 확정)이면 심층 진입 연출을 재생한다.
    if (gGame.floor > floorBefore && gGame.selectedDrive >= 0 && gGame.phase != PHASE_VICTORY) {
        if (UiFxSnapshotActive()) gUiFxPendingDescent = gGame.floor;
        else BeginDescent(gGame.floor, -1);
    }
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

// 가장 오래된 줄을 밀어내는 고정 크기 스크롤백. 동적 할당은 쓰지 않는다.
static void TermPrint(const wchar_t* line) {
    if (gTermLogCount >= TERM_LOG_LINES) {
        for (int i = 1; i < TERM_LOG_LINES; ++i) lstrcpynW(gTermLog[i - 1], gTermLog[i], TERM_LOG_CAP);
        gTermLogCount = TERM_LOG_LINES - 1;
    }
    lstrcpynW(gTermLog[gTermLogCount++], line, TERM_LOG_CAP);
}

// 연출이 도는 중에 판을 갈아엎으면 재생과 결과가 어긋난다. 그동안은 막는다.
static int TermBusy() {
    return gTurnTraceActive || gDeathActive || gCombatClearActive
        || gDescentActive || gDirEnterActive || gBossIntroActive || gBootActive || GimmickFxKind() > 0
        || UiFxBlocksInput();
}

static void TermRun() {
    wchar_t echo[TERM_LOG_CAP];
    wsprintfW(echo, L"> %s", gTermInput);
    TermPrint(echo);

    // 명령어와 숫자 인자 하나로 가른다.
    const wchar_t* p = gTermInput;
    wchar_t cmd[TERM_INPUT_MAX + 1];
    int n = 0;
    while (*p == L' ') ++p;
    while (*p && *p != L' ' && n < TERM_INPUT_MAX) cmd[n++] = *p++;
    cmd[n] = 0;
    while (*p == L' ') ++p;
    int arg = 0, hasArg = 0;
    while (*p >= L'0' && *p <= L'9') { arg = arg * 10 + (int)(*p++ - L'0'); hasArg = 1; }

    gTermInput[0] = 0; gTermInputLen = 0;
    if (n == 0) return;

    if (lstrcmpW(cmd, L"win") == 0) {
        if (TermBusy()) { TermPrint(L"  연출이 끝난 뒤에 다시 실행하십시오."); return; }
        if (gGame.phase != PHASE_COMBAT) { TermPrint(L"  전투 중이 아닙니다."); return; }
        int floor = gGame.floor, encounter = gGame.encounter;
        DebugWinCombat(&gGame);
        PersistCampaignProgress();
        PlaySfx(SFX_ENEMY_DOWN);
        TermPrint(L"  적 전멸. 전투 종료 처리 완료.");
        gTermOpen = 0;
        BeginCombatClear(floor, encounter);
        return;
    }
    // 드라이브 하나를 통째로 접는다. 마지막 보스를 이긴 것과 같은 자리로 보내므로
    // 조각 회수와 챕터 종료 기록이 평소 완주와 똑같이 이어진다.
    if (lstrcmpW(cmd, L"winwin") == 0) {
        if (TermBusy()) { TermPrint(L"  연출이 끝난 뒤에 다시 실행하십시오."); return; }
        if (!DebugWinDrive(&gGame)) { TermPrint(L"  드라이브 안이 아닙니다."); return; }
        PersistCampaignProgress();
        PlaySfx(SFX_ENEMY_DOWN);
        wchar_t done[TERM_LOG_CAP];
        wsprintfW(done, L"  %s 드라이브 클리어 처리 완료.", DRIVE_INFO[gGame.selectedDrive].letter);
        TermPrint(done);
        gTermOpen = 0;
        BeginCombatClear(gGame.floor, gGame.encounter);
        return;
    }
    // 보스 조우 연출과 기믹은 층마다 한 번뿐이라 손으로 보려면 두 판을 이겨야 한다.
    if (lstrcmpW(cmd, L"boss") == 0) {
        if (TermBusy()) { TermPrint(L"  연출이 끝난 뒤에 다시 실행하십시오."); return; }
        if (!DebugJumpToBoss(&gGame)) { TermPrint(L"  볼륨 안에서만 됩니다."); return; }
        gTermOpen = 0;
        SyncRollAnimation();
        TermPrint(L"  보스 구역으로 이동했습니다.");
        return;
    }
    if (lstrcmpW(cmd, L"hp") == 0) {
        if (gGame.phase == PHASE_TITLE) { TermPrint(L"  런이 시작되지 않았습니다."); return; }
        int want = hasArg ? arg : gGame.playerMaxHp;
        if (want > gGame.playerMaxHp) want = gGame.playerMaxHp;
        if (want < 1) want = 1;
        gGame.playerHp = want;
        wchar_t msg[TERM_LOG_CAP];
        wsprintfW(msg, L"  체력 %d / %d", gGame.playerHp, gGame.playerMaxHp);
        TermPrint(msg);
        return;
    }
    if (lstrcmpW(cmd, L"help") == 0) {
        TermPrint(L"  win       현재 전투를 즉시 승리 처리한다");
        TermPrint(L"  winwin    현재 드라이브를 즉시 클리어 처리한다");
        TermPrint(L"  boss      지금 층의 보스 구역으로 바로 이동한다");
        TermPrint(L"  hp [n]    체력을 n으로 (생략하면 최대치)");
        TermPrint(L"  perf      페인트 시간과 오디오 언더런");
        TermPrint(L"  clear     기록 지우기");
        TermPrint(L"  help      이 목록");
        return;
    }
    if (lstrcmpW(cmd, L"perf") == 0) {
        // 렉과 끊김을 사용자 PC에서 그대로 잰다. 언더런이 0이 아니면 소리가 실제로 끊긴 것이다.
        RECT rc; GetClientRect(gWindow, &rc);
        wchar_t msg[TERM_LOG_CAP];
        wsprintfW(msg, L"  창 %dx%d · 페인트 최근 %dms · 최대 %dms · %d프레임", rc.right, rc.bottom, PaintLastMs(), PaintMaxMs(), PaintCount());
        TermPrint(msg);
        wsprintfW(msg, L"  오디오 언더런 %d회 (큐 %d x 20ms)", AudioUnderruns(), 4);
        TermPrint(msg);
        return;
    }
    if (lstrcmpW(cmd, L"clear") == 0) { gTermLogCount = 0; return; }

    wchar_t msg[TERM_LOG_CAP];
    wsprintfW(msg, L"  알 수 없는 명령: %s", cmd);
    TermPrint(msg);
}

static void HandleKey(WPARAM key) {
    // 터미널은 어떤 상태에서도 열린다. 연출 중이나 정지 화면에서도 판을 봐야 한다.
    // 다만 개발 모드에서만이다. 배포 빌드에서는 백틱이 그냥 무시된다.
    if (key == VK_OEM_3 && gDevMode) {
        gTermOpen = !gTermOpen;
        if (gTermOpen && gTermLogCount == 0) TermPrint(L"  help 로 명령 목록.");
        gTermInput[0] = 0; gTermInputLen = 0;
        InvalidateRect(gWindow, 0, FALSE);
        return;
    }
    if (gTermOpen && gDevMode) {
        // IME가 켜져 있어도 먹히도록 WM_CHAR가 아니라 가상 키에서 직접 만든다.
        if (key == VK_ESCAPE) gTermOpen = 0;
        else if (key == VK_RETURN) TermRun();
        else if (key == VK_BACK) { if (gTermInputLen > 0) gTermInput[--gTermInputLen] = 0; }
        else if (gTermInputLen < TERM_INPUT_MAX) {
            wchar_t c = 0;
            if (key >= 'A' && key <= 'Z') c = (wchar_t)(key - 'A' + L'a');
            else if (key >= '0' && key <= '9') c = (wchar_t)key;
            else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) c = (wchar_t)(L'0' + key - VK_NUMPAD0);
            else if (key == VK_SPACE) c = L' ';
            if (c) { gTermInput[gTermInputLen++] = c; gTermInput[gTermInputLen] = 0; }
        }
        InvalidateRect(gWindow, 0, FALSE);
        return;
    }
    if (gDeathActive) { if (DeathElapsed() >= DEATH_DARK_AT) FinishDeath(); return; }
    if (gBootActive) { FinishBootInsert(); return; }
    if (UiFxBlocksInput()) return;
    if (gTurnTraceActive) {
        if (key == VK_SPACE || key == VK_RETURN) FinishTurnTrace();
        return;
    }
    if (gDescentActive) { FinishDescent(); return; }
    if (gDirEnterActive) { FinishDirectoryEnter(); return; }
    if (gBossIntroActive) { FinishBossIntro(); return; }
    if (gCombatClearActive) { FinishCombatClear(); return; }
    if (key == VK_F3 && gGame.phase != PHASE_TITLE) { gDeckOpen = !gDeckOpen; gGuideOpen = 0; gSettingsOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gDeckOpen) { if (key == VK_ESCAPE) gDeckOpen = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (key == VK_F2) { gSettingsOpen = !gSettingsOpen; gGuideOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gSettingsOpen) {
        if (key == VK_ESCAPE) { gSettingsOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0; PersistSettings(); }
        // 마우스로 정확히 맞추기 어려운 값을 위해 5씩 움직인다.
        else if (key == VK_LEFT)  { SetAudioChannelVolume(gVolumeKeyboardChannel, AudioChannelVolume(gVolumeKeyboardChannel) - 5); PlaySfx(SFX_UI_CLICK); }
        else if (key == VK_RIGHT) { SetAudioChannelVolume(gVolumeKeyboardChannel, AudioChannelVolume(gVolumeKeyboardChannel) + 5); PlaySfx(SFX_UI_CLICK); }
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (key == VK_F1) { gGuideOpen = !gGuideOpen; gSettingsOpen = 0; gRestartArmed = 0; gCampaignResetArmed = 0; if (gGuideOpen) gGuidePage = 0; InvalidateRect(gWindow, 0, FALSE); return; }
    if (gGuideOpen) {
        if (key == VK_ESCAPE) gGuideOpen = 0;
        else if (key == VK_LEFT && gGuidePage > 0) --gGuidePage;
        else if (key == VK_RIGHT && gGuidePage < 1) ++gGuidePage;
        InvalidateRect(gWindow, 0, FALSE); return;
    }
    if (RollBlocking()) { StopRead(); InvalidateRect(gWindow, 0, FALSE); return; }
    if ((gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_PRUNE) && key == VK_TAB) {
        MoveKeyboardFocus((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1); return;
    }
    if ((gGame.phase == PHASE_COMBAT || gGame.phase == PHASE_PRUNE) && key == VK_RETURN && ActivateKeyboardFocus()) return;
    int floorBefore = gGame.floor;
    if (gGame.phase == PHASE_TITLE) { if (key == VK_RETURN || key == VK_SPACE) BeginNewRun(); }
    else if (gGame.phase == PHASE_STORY) { if (key == VK_RETURN || key == VK_SPACE) AdvanceStoryUi(); }
    else if (gGame.phase == PHASE_ENDING_CHOICE) {
        if (key >= '1' && key < '1' + ENDING_COUNT) {
            int pick = (int)(key - '1');
            if (gEndingArmed != pick) { gEndingArmed = pick; PlaySfxPitched(SFX_REWARD_PICK, pick * 2); }
        }
        else if (key == VK_ESCAPE && gEndingArmed >= 0) { gEndingArmed = -1; PlaySfx(SFX_UI_CLICK); }
        else if ((key == VK_RETURN || key == VK_SPACE) && gEndingArmed >= 0) {
            int ending = gEndingArmed;
            gEndingArmed = -1;
            SelectEnding(&gGame, ending);
            PlaySfx(SFX_CONFIRM);
        }
    }
    else if (gGame.phase == PHASE_DRIVE_SELECT) {
        if ((gGame.clearedMask & 0x3F) == 0x3F && (key == VK_LEFT || key == VK_RIGHT)) { CycleReplayPage(key == VK_LEFT ? -1 : 1); }
        else if (key >= '1' && key <= '0' + gGame.driveChoiceCount) {
            SelectDrive(&gGame, (int)(key - '1'));
            if (gGame.phase == PHASE_DIRECTORY) { PlaySfx(SFX_CONFIRM); BeginDescent(0, (int)(key - '1')); }
        }
    }
    else if (gGame.phase == PHASE_DIRECTORY) {
        // Esc는 선택지를 닫거나 다시 뽑지 않는다. 세워 둔 후보만 내린다.
        if (key >= '1' && key <= '0' + DIRECTORY_CHOICE_COUNT) ArmOrTakeDirectory((int)(key - '1'));
        else if (key == VK_ESCAPE && gDirectoryArmed >= 0) { gDirectoryArmed = -1; PlaySfx(SFX_UI_CLICK); }
    }
    else if (gGame.phase == PHASE_COMBAT) {
        if (key == 'R') BeginRead();
        else if (!gRolled) { /* sector not read yet */ }
        else if (key >= '1' && key <= '3') { gGame.selectedDie = (int)(key - '1'); PlaySfxPitched(SFX_DIE_PICK, gGame.selectedDie * 2); }
        else if (key == 'K') KeybRerollSelected();
        else if (key == VK_SPACE) ExecuteCombatTurn();
        else if (key == VK_ESCAPE && gGame.selectedDie >= 0) {
            int die = gGame.selectedDie, oldSlot = gGame.dice[die].assignedSlot;
            UnassignDie(&gGame, die);
            if (oldSlot >= 0 && BeginUiFx(UIFX_DIE_REMOVE)) {
                gUiFx.die = die; gUiFx.fromSlot = oldSlot; gUiFx.toSlot = -1;
            }
        }
    } else if (gGame.phase == PHASE_REWARD) {
        if (key >= '1' && key <= '3') {
            gRewardSkipArmed = 0;
            if (gGame.rewardIsTsr) ArmOrTakeTsrReward((int)(key - '1'));
            else { gFaceSwapArmed = -1; SelectReward(&gGame, (int)(key - '1')); }
        }
        else if (key == '4') { gRewardSkipArmed = 0; TakeRepairReward(); }
        // 전투의 취소는 배치 해제다. 보상에서도 먼저 고른 카드를 놓는 데 쓰고,
        // 놓을 것이 없을 때만 포기 버튼을 무장한다. 습관적인 취소 한 번으로
        // 보상이 사라지지 않는다.
        else if (key == VK_ESCAPE) {
            if (gFaceSwapArmed >= 0) { gFaceSwapArmed = -1; gRewardSkipArmed = 0; PlaySfx(SFX_UI_CLICK); }
            else if (gTsrArmed >= 0) { gTsrArmed = -1; gRewardSkipArmed = 0; PlaySfx(SFX_UI_CLICK); }
            else if (gGame.selectedReward >= 0) { gGame.selectedReward = -1; gRewardSkipArmed = 0; PlaySfx(SFX_UI_CLICK); }
            else ArmOrConfirmRewardSkip();
        }
    } else if (gGame.phase == PHASE_PRUNE) { if (key == VK_RETURN) ConfirmPrune(&gGame); }
    else if (IsEndScreen()) {
        // 사망 화면은 "새 실행체 투입 · 스페이스"라고 적혀 있다.
        if (key == 'R' || key == VK_RETURN || (key == VK_SPACE && gGame.phase == PHASE_GAMEOVER)) ContinueFromEnd();
    }
    ClearStaleConfirmations();
    PersistCampaignProgress();
    if (gGame.floor > floorBefore && gGame.selectedDrive >= 0 && gGame.phase != PHASE_VICTORY) {
        if (UiFxSnapshotActive()) gUiFxPendingDescent = gGame.floor;
        else BeginDescent(gGame.floor, -1);
    }
    SyncRollAnimation();
    InvalidateRect(gWindow, 0, FALSE);
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateRenderFonts(); AudioOpen(window);
        SetTimer(window, AUDIO_TIMER_ID, 50, 0);   // 게임 상태 -> 음악 씬·강도 동기화
        return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO* info = (MINMAXINFO*)lParam; info->ptMinTrackSize.x = 480; info->ptMinTrackSize.y = 320; return 0; }
    case WM_MOUSEMOVE: {
        gKeyboardFocus = -1;
        gMouse = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (!gMouseInClient) {
            TRACKMOUSEEVENT track = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, window, 0};
            TrackMouseEvent(&track); gMouseInClient = 1;
        }
        if (gVolumeDragging >= 0) { SetAudioChannelVolume(gVolumeDragging, VolumeFromX(gVolumeDragging, gMouse.x)); InvalidateRect(window, 0, FALSE); return 0; }
        SyncUiFocus();
        return 0;
    }
    case WM_MOUSELEAVE:
        gMouseInClient = 0; gMouse.x = gMouse.y = -1;
        SyncUiFocus(); InvalidateRect(window, 0, FALSE); return 0;
    case WM_ACTIVATEAPP:
        if (!wParam) { gMouseInClient = 0; gMouse.x = gMouse.y = -1; SyncUiFocus(); }
        return 0;
    case WM_LBUTTONUP:
        if (gVolumeDragging >= 0) {
            gVolumeDragging = -1;
            ReleaseCapture();
            PlaySfx(SFX_CONFIRM);          // 맞춘 크기를 귀로 확인시킨다
            SyncUiFocus(); gUiFocus.cued = 1;
            InvalidateRect(window, 0, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED: gVolumeDragging = -1; return 0;
    case WM_LBUTTONDOWN: {
        POINT p = ScreenToCanvas(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        HandleClick(p.x, p.y); SyncUiFocus();
        // An immediate click has its own cue; do not trail it with a hover tick.
        gUiFocus.cued = 1; return 0;
    }
    case WM_KEYDOWN:
        if ((lParam & (1u << 30)) == 0) { HandleKey(wParam); SyncUiFocus(); }
        return 0;
    case WM_TIMER:
        if (wParam == AUDIO_TIMER_ID) { SyncAudioScene(); TickUiFocus(); return 0; }   // 믹싱은 오디오 스레드가 한다
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
            // 마지막 줄의 피해 숫자와 파편이 끝나기 전에 타이머를 끄면 연출이
            // 그 프레임에서 얼어붙는다. 꼬리만큼 더 돌리고 나서 멈춘다.
            else if (traceElapsed >= reveal + TRACE_FX_TAIL_MS) KillTimer(window, 4);
            InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 6u) {
            int descentElapsed = (int)(GetTickCount() - gDescentStart);
            int scanMs = DESCENT_MS - (gDescentChoiceIndex >= 0 ? DESCENT_LOCK_MS : 0);
            int scanStart = gDescentChoiceIndex >= 0 ? DESCENT_LOCK_MS : 0;
            if (gDescentSeekPhase == 0 && descentElapsed >= scanStart) { ++gDescentSeekPhase; PlaySfxPitched(SFX_DIE_LOCK, 1); }
            else if (gDescentSeekPhase == 1 && descentElapsed >= scanStart + scanMs / 3) { ++gDescentSeekPhase; PlaySfx(SFX_DIE_LOCK); }
            else if (gDescentSeekPhase == 2 && descentElapsed >= scanStart + scanMs * 2 / 3) { ++gDescentSeekPhase; PlaySfxPitched(SFX_DIE_LOCK, 4); }
            if (descentElapsed >= DESCENT_MS) FinishDescent();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 8u) {
            // 임팩트 시점을 지나는 순간 한 번: 흔들림 시작 + 둔탁한 타격음
            int at = GimmickFxImpactAt(gFxKind, gFxB);
            if (!gFxImpactPlayed && at > 0 && (int)(GetTickCount() - gFxStart) >= at) {
                gFxImpactPlayed = 1;
                gFxShakeAt = GetTickCount();
                if (gFxShakePeak > 0) PlaySfxPitched(SFX_PLAYER_HIT, gFxShakePeak >= 9 ? 0 : 3);
            }
            if (GimmickFxElapsed() >= GimmickFxDuration(gFxKind, gFxB)) FinishGimmickFx();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 7u) {
            int deathElapsed = (int)(GetTickCount() - gDeathStart), sfx = 0, pitch = 0, at;
            while ((at = DeathCue(gDeathCue, &sfx, &pitch)) >= 0 && deathElapsed >= at) {
                PlaySfxPitched(sfx, pitch);
                ++gDeathCue;
            }
            if (deathElapsed >= DEATH_MS) FinishDeath();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 9u) {
            int dirElapsed = (int)(GetTickCount() - gDirEnterStart);
            if (gDirEnterCuePhase == 0 && dirElapsed >= DIR_SELECT_LOCK_MS) { ++gDirEnterCuePhase; PlaySfxPitched(SFX_DIE_LOCK, 2); }
            else if (gDirEnterCuePhase == 1 && dirElapsed >= DIR_SELECT_LOCK_MS + 360) { ++gDirEnterCuePhase; PlaySfxPitched(SFX_DIE_LOCK, 5); }
            if (dirElapsed >= DIR_ENTER_MS) FinishDirectoryEnter();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == 10u) {
            int bootElapsed = (int)(GetTickCount() - gBootStart);
            int cueCount = (int)(sizeof(BOOT_CUES) / sizeof(BOOT_CUES[0]));
            while (gBootCue < cueCount && bootElapsed >= BOOT_CUES[gBootCue].at) {
                PlaySfxPitched(BOOT_CUES[gBootCue].sfx, BOOT_CUES[gBootCue].pitch);
                ++gBootCue;
            }
            if (bootElapsed >= BOOT_INSERT_MS) FinishBootInsert();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == BOSS_INTRO_TIMER_ID) {
            int bossElapsed = (int)(GetTickCount() - gBossIntroStart);
            int cueCount = (int)(sizeof(BOSS_CUES) / sizeof(BOSS_CUES[0]));
            while (gBossIntroCue < cueCount && bossElapsed >= BOSS_CUES[gBossIntroCue].at) {
                if (BOSS_CUES[gBossIntroCue].sfx == SFX_BOSS_ARRIVE) gBossArriveFired = 1;
                PlaySfxPitched(BOSS_CUES[gBossIntroCue].sfx, BOSS_CUES[gBossIntroCue].pitch);
                ++gBossIntroCue;
            }
            if (bossElapsed >= BOSS_INTRO_MS) FinishBossIntro();
            else InvalidateRect(window, 0, FALSE);
        }
        else if (wParam == UIFX_TIMER_ID) {
            if (UiFxElapsed() >= UiFxDuration()) FinishUiFx();
            else InvalidateRect(window, 0, FALSE);
        }
        return 0;
    case WM_PAINT: PaintGame(window); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: {
        // 캠페인 진행도는 남지만 진행 중인 런(층·체력·덱)은 저장되지 않는다.
        // 실수로 닫는 것과 정말 끝내는 것을 구분해 준다.
        int inRun = gGame.phase != PHASE_TITLE && !IsEndScreen();
        if (inRun && MessageBoxW(window,
                L"진행 중인 런(층·체력·덱)은 저장되지 않습니다.\n"
                L"복구한 조각과 엔딩 기록은 그대로 남습니다.\n\n종료하시겠습니까?",
                L"A:\\ROGUE", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
            return 0;
        DestroyWindow(window);
        return 0;
    }
    case WM_DESTROY:
        PersistSettings();
        if (!gCampaignCorrupt) SaveCampaign(&gCampaign);
        KillTimer(window, 1); KillTimer(window, 2); KillTimer(window, 3); KillTimer(window, 4);
        KillTimer(window, 6); KillTimer(window, 7); KillTimer(window, 8); KillTimer(window, 9);
        KillTimer(window, 10); KillTimer(window, UIFX_TIMER_ID); KillTimer(window, BOSS_INTRO_TIMER_ID);
        DestroyRenderFonts();
        AudioClose(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// 실행 인자에 -dev(또는 --dev, /dev)가 있는지만 본다. 인자 파싱을 위해
// CommandLineToArgvW를 끌어오면 shell32가 붙으므로 문자열에서 직접 찾는다.
static int CommandLineHasDevFlag() {
    const wchar_t* line = GetCommandLineW();
    if (!line) return 0;
    for (const wchar_t* at = line; *at; ++at) {
        if (*at != L'-' && *at != L'/') continue;
        if (at != line && at[-1] != L' ' && at[-1] != L'	' && at[-1] != L'"') continue;
        const wchar_t* word = at + 1;
        if (*word == L'-') ++word;
        if ((word[0] == L'd' || word[0] == L'D') && (word[1] == L'e' || word[1] == L'E')
            && (word[2] == L'v' || word[2] == L'V')
            && (word[3] == 0 || word[3] == L' ' || word[3] == L'	')) return 1;
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();
    if (CommandLineHasDevFlag()) gDevMode = 1;
    // 연출 타이머는 전부 16ms로 걸려 있지만, 시스템 틱이 기본 15.6ms라 실제로는
    // 두 틱에 한 번씩 밀려 30fps 언저리로 떨어진다. 틱을 1ms로 당겨 두면 16ms가
    // 16ms로 온다. 끝낼 때 반드시 되돌린다 (전역 설정이다).
    timeBeginPeriod(1);
    int hadCampaignSave = CampaignSaveExistsBesideExecutable();
    if (!LoadCampaign(&gCampaign) && hadCampaignSave) gCampaignCorrupt = 1;
    LoadCodex(gCodex, ENEMY_KIND_COUNT);
    LoadTranslations();
    // 번역을 읽은 뒤라야 English 설정이 실제로 받아들여진다.
    LoadSettings(&gSettings);
    ApplySettings(0);
    InitTitle(&gGame, CampaignClearedMask(&gCampaign), CampaignSeenEndingMask(&gCampaign)); WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure; wc.hInstance = instance; wc.hCursor = LoadCursorW(0, IDC_ARROW); wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1)); wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(1));   // src/arogue.rc
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.lpszClassName = L"ARogueWindowClass"; if (!RegisterClassExW(&wc)) return 1;
    RECT desired = {0, 0, 1120, 760}; AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0); int width = desired.right - desired.left, height = desired.bottom - desired.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    gWindow = CreateWindowExW(0, wc.lpszClassName, L"A:\\ROGUE · 1.44MB", WS_OVERLAPPEDWINDOW, x, y, width, height, 0, 0, instance, 0);
    if (!gWindow) return 2; ShowWindow(gWindow, showCommand); UpdateWindow(gWindow);
    // 배율·전체화면은 창이 있어야 적용된다. 창 제목도 실제로 적용된 언어를 따른다.
    ApplySettings(1);
    if (UiLanguage() == LANGUAGE_ENGLISH) SetWindowTextW(gWindow, L"A:\\ROGUE · 1.44MB · English");
    MSG message; while (GetMessageW(&message, 0, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    timeEndPeriod(1);
    return (int)message.wParam;
}
