from pathlib import Path
p = Path(__file__).resolve().parents[1] / "src" / "smoke.cpp"
s = p.read_text(encoding="utf-8")
repls = {
'''    // Exhaust every progress subset with enough seeds to cover both late-game
    // volume choices and all five difficulty grades, without touching save I/O.''':
'''    // Exhaust every progress subset. Campaign offers are now intentionally
    // stable across executable restarts: the seed must not reroll volumes or
    // difficulty (#96). Late game exposes every remaining volume (#95).''',
'''            if (game.driveChoiceCount != (remaining ? 3 : 1)) return Fail("campaign candidate count");''':
'''            int expectedCount = remaining ? (remaining < 3 ? remaining : 3) : 1;
            if (game.driveChoiceCount != expectedCount) return Fail("campaign candidate count");''',
'''                    if ((remaining >= 3) == (game.driveChoices[j] == d)) return Fail("campaign volume uniqueness or late-game repetition");''':
'''                    if (game.driveChoices[j] == d) return Fail("campaign volume choices must stay unique");''',
'''        if (seenVolumes != ((~mask) & 63)) return Fail("all remaining volumes must be reachable across seeds");
        if (remaining && seenGrades != ((1 << DIFFICULTY_COUNT) - 1)) return Fail("all difficulty grades must remain available");''':
'''        if (remaining <= 3 && seenVolumes != ((~mask) & 63))
            return Fail("late campaign must expose every remaining volume at once");
        if (mask == 0 && !(seenGrades & (1 << DIFF_BEGINNER)))
            return Fail("a fresh campaign must always offer beginner difficulty");'''
}
for old,new in repls.items():
    if s.count(old) != 1:
        raise RuntimeError(f"smoke contract target not found exactly once: {old[:60]!r} -> {s.count(old)}")
    s = s.replace(old,new,1)
p.write_text(s, encoding="utf-8", newline="\n")
print("campaign smoke contracts updated")
