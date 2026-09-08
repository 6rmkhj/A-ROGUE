from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")

def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 match, got {count}")
    return text.replace(old, new, 1)

def regex_once(text, pattern, repl, label):
    out, count = re.subn(pattern, repl, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 match, got {count}")
    return out

# #67/#75: an offline die is disconnected. It must neither roll nor become the
# read-error die. This also removes the contradictory combined state behind #69.
game = read("src/game.cpp")
old_roll = '''static void RollDice(GameState* game) {
    for (int d = 0; d < 3; ++d) {
        game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
        game->dice[d].assignedSlot = -1;
        game->dice[d].disabled = 0;
        game->dice[d].unstable = 0;
        game->dice[d].offline = 0;
    }
    game->selectedDie = -1;
    if (IsModifierActive(game, MOD_READ_ERROR)) game->dice[RandomRange(game, 3)].unstable = 1;
}'''
new_roll = '''static void RollDice(GameState* game) {
    const int offline = game->boss.offlineDie;
    for (int d = 0; d < 3; ++d) {
        // A disconnected die has no read at all: keep its last face and consume
        // no gameplay RNG. This makes the visual rule and simulation identical.
        if (d != offline) game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
        game->dice[d].assignedSlot = -1;
        game->dice[d].disabled = 0;
        game->dice[d].unstable = 0;
        game->dice[d].offline = (uint8_t)(d == offline);
    }
    game->selectedDie = -1;
    if (IsModifierActive(game, MOD_READ_ERROR)) {
        int candidates[3], count = 0;
        for (int d = 0; d < 3; ++d) if (d != offline) candidates[count++] = d;
        if (count > 0) game->dice[candidates[RandomRange(game, count)]].unstable = 1;
    }
}'''
game = replace_once(game, old_roll, new_roll, "offline roll/read error")

# #77: duplicates no longer immediately delete a die for the turn. Give the
# later duplicate one deterministic gameplay-RNG reroll; only a repeated clash
# is fragmented. DEFRAG continues to suppress the modifier completely.
frag_pattern = r'''static void ApplyFragmentation\(GameState\* game\) \{.*?\n\}\n\nstatic int RollOutputSum'''
frag_repl = '''static void ApplyFragmentation(GameState* game) {
    for (int d = 0; d < 3; ++d) game->dice[d].disabled = 0;
    if (!IsModifierActive(game, MOD_FRAGMENTATION) || IsTsrInstalled(game, TSR_DEFRAG)) return;
    int rerolled = 0, fragmented = 0;
    for (int d = 1; d < 3; ++d) {
        int duplicate = 0;
        for (int p = 0; p < d; ++p)
            if (!game->dice[p].disabled && game->dice[p].rolledFace == game->dice[d].rolledFace) duplicate = 1;
        if (!duplicate) continue;
        game->dice[d].rolledFace = (uint8_t)RandomRange(game, 6);
        ++rerolled;
        duplicate = 0;
        for (int p = 0; p < d; ++p)
            if (!game->dice[p].disabled && game->dice[p].rolledFace == game->dice[d].rolledFace) duplicate = 1;
        if (duplicate) { game->dice[d].disabled = 1; ++fragmented; }
    }
    if (rerolled > 0) PushLog(game, L"조각화: 중복 주사위를 한 번 재굴림했습니다.");
    if (fragmented > 0) PushLog(game, L"조각화: 재굴림 후에도 겹친 주사위만 비활성화됩니다.");
}

static int RollOutputSum'''
game = regex_once(game, frag_pattern, frag_repl, "fragmentation softening")

# #94/#95/#96: campaign choices are a function of campaign progress, not process
# restart RNG. If <=3 volumes remain, expose every remaining volume. The first
# campaign screen always contains Beginner once.
choices_pattern = r'''static void PickDriveChoices\(GameState\* game, uint8_t clearedMask\) \{.*?\n\}\n\n// 카드 3장에 서로 다른 난이도를 배정한다\.'''
choices_repl = '''static uint32_t CampaignChoiceRandom(uint8_t clearedMask) {
    uint32_t x = 0xA341316Cu ^ ((uint32_t)clearedMask * 0x9E3779B9u);
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x ? x : 0x51ED270Bu;
}

static void PickDriveChoices(GameState* game, uint8_t clearedMask) {
    int remaining[DRIVE_SELECTABLE_COUNT], remainingCount = 0;
    game->driveChoiceCount = 0;
    for (int i = 0; i < 3; ++i) game->driveChoices[i] = game->driveDifficulty[i] = -1;
    for (int i = 0; i < DRIVE_SELECTABLE_COUNT; ++i)
        if (!(clearedMask & (1u << i))) remaining[remainingCount++] = i;
    if (!remainingCount) { game->driveChoiceCount = 1; game->driveChoices[0] = DRIVE_FINAL; return; }

    // Once the campaign has narrowed to three or fewer targets, never hide a
    // remaining volume behind RNG. The UI simply renders the remaining cards.
    if (remainingCount <= 3) {
        game->driveChoiceCount = remainingCount;
        for (int i = 0; i < remainingCount; ++i) game->driveChoices[i] = remaining[i];
        return;
    }

    // Earlier in the campaign, choose three deterministically from progress.
    // Relaunching the executable cannot reroll the offer.
    uint32_t rng = CampaignChoiceRandom(clearedMask);
    for (int i = remainingCount - 1; i > 0; --i) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        int j = (int)(rng % (uint32_t)(i + 1));
        int swap = remaining[i]; remaining[i] = remaining[j]; remaining[j] = swap;
    }
    game->driveChoiceCount = 3;
    for (int i = 0; i < 3; ++i) game->driveChoices[i] = remaining[i];
}

// 카드 3장에 서로 다른 난이도를 배정한다.'''
game = regex_once(game, choices_pattern, choices_repl, "campaign volume choices")

game = replace_once(game,
'''    for (int i = 0; i < game->driveChoiceCount; ++i) game->driveDifficulty[i] = pool[i];
}''',
'''    for (int i = 0; i < game->driveChoiceCount; ++i) game->driveDifficulty[i] = pool[i];
    if (game->clearedMask == 0) {
        int hasBeginner = 0;
        for (int i = 0; i < game->driveChoiceCount; ++i)
            if (game->driveDifficulty[i] == DIFF_BEGINNER) hasBeginner = 1;
        if (!hasBeginner && game->driveChoiceCount > 0) game->driveDifficulty[0] = DIFF_BEGINNER;
    }
}''', "first-run beginner")

game = replace_once(game,
'''    PickDriveDifficulties(game, game->rng ^ 0x9E3779B9u);''',
'''    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);''',
"stable difficulty choices")

# #68/#71: announce the capacity contraction at the exact floor transition.
game = replace_once(game,
'''static void EnterNextFloor(GameState* game) {
    ++game->floor;
    game->encounter = 0;''',
'''static void EnterNextFloor(GameState* game) {
    int previousCapacity = EffectiveCapacity(game);
    ++game->floor;
    game->encounter = 0;
    if (game->floor < 3) {
        int nextCapacity = EffectiveCapacity(game);
        if (nextCapacity < previousCapacity) {
            wchar_t capacityNotice[96];
            wsprintfW(capacityNotice, L"SECTOR LOSS: 용량 한도 %dB → %dB · 손상된 디스크에서 남길 데이터를 고르십시오.", previousCapacity, nextCapacity);
            PushLog(game, capacityNotice);
        }
    }''', "capacity transition notice")
write("src/game.cpp", game)

# #70/#72: tame HIMEM and make ECHO describe what is actually repeated.
data = read("src/data.h")
data = data.replace('TSR_HIMEM = 0,   // 용량 한도 +60B', 'TSR_HIMEM = 0,   // 용량 한도 +45B')
data = replace_once(data,
'{L"HIMEM.SYS", L"용량 한도 +60B",                 20, 60, -1,',
'{L"HIMEM.SYS", L"용량 한도 +45B",                 20, 45, -1,',
"HIMEM balance")
data = replace_once(data,
'{L"메아리", L"메아리", L"연쇄 슬롯에서 직전 효과 반복",',
'{L"메아리", L"메아리", L"연쇄 슬롯에서 이번 턴 공격/방어 효과 반복",',
"echo wording")
write("src/data.h", data)

# #69: when the current read-error die is also announced for offline next turn,
# render one compact state instead of two long badges fighting for the card.
screens = read("src/screens.cpp")
screens = replace_once(screens,
'''    if (die->unstable) { AppendStatus(statuses, L"읽기 오류"); ++statusCount; }
    if (die->disabled) { AppendStatus(statuses, L"조각화"); ++statusCount; }
    if (die->offline) { AppendStatus(statuses, L"오프라인"); ++statusCount; }
    if (gGame.boss.nextOfflineDie == index) { AppendStatus(statuses, L"다음 턴 오프라인"); ++statusCount; }''',
'''    if (die->unstable && gGame.boss.nextOfflineDie == index) { AppendStatus(statuses, L"읽기 오류 → 다음 오프라인"); ++statusCount; }
    else if (die->unstable) { AppendStatus(statuses, L"읽기 오류"); ++statusCount; }
    if (die->disabled) { AppendStatus(statuses, L"조각화"); ++statusCount; }
    if (die->offline) { AppendStatus(statuses, L"오프라인"); ++statusCount; }
    if (!die->unstable && gGame.boss.nextOfflineDie == index) { AppendStatus(statuses, L"다음 턴 오프라인"); ++statusCount; }''',
"combined read-error/offline status")
write("src/screens.cpp", screens)

# Keep English fallbacks synchronized for changed player-facing text.
for rel in ("translations.tsv", "build/translations.tsv"):
    p = ROOT / rel
    if not p.exists():
        continue
    t = p.read_text(encoding="utf-8")
    t = t.replace("용량 한도 +60B\tCapacity limit +60B", "용량 한도 +45B\tCapacity limit +45B")
    t = t.replace("연쇄 슬롯에서 직전 효과 반복\tRepeat the previous effect in the Chain slot", "연쇄 슬롯에서 이번 턴 공격/방어 효과 반복\tRepeat this turn's Attack or Defend effect in the Chain slot")
    p.write_text(t, encoding="utf-8", newline="\n")

print("open-issue patch set applied")
