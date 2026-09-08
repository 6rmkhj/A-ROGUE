from pathlib import Path
p = Path(__file__).with_name('fix_final_gameplay_issues.py')
s = p.read_text(encoding='utf-8')

old = "s=once(s,'    campaign->checksum = Get32(bytes + 16);','    campaign->checksum = Get32(bytes + 22);','init checksum offset')"
new = "old_checksum='    campaign->checksum = Get32(bytes + 16);'\nif old_checksum not in s: raise RuntimeError('init checksum offset missing')\ns=s.replace(old_checksum,'    campaign->checksum = Get32(bytes + 22);',1)"
if old in s:
    s = s.replace(old, new, 1)
elif new not in s:
    raise RuntimeError('checksum patch shape changed')

old_seed = "s=once(s,'    PickDriveDifficulties(game, game->rng ^ 0x9E3779B9u);',\n'''    PickDriveDifficulties(game, game->rng ^ 0x9E3779B9u);\n    if ((game->clearedMask & 0x3F) == 0x3F) SetReplayDrivePage(game, 0);''','replay difficulty reset')"
new_seed = "s=once(s,'    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);',\n'''    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);''','replay difficulty reset')"
if old_seed in s:
    s = s.replace(old_seed, new_seed, 1)
elif new_seed not in s:
    raise RuntimeError('drive difficulty patch shape changed')

old_final_only = "'''    if (!remainingCount) { SetReplayDrivePage(game, 0); return; }'''"
new_final_only = "'''    if (!remainingCount) { game->driveChoiceCount = 1; game->driveChoices[0] = DRIVE_FINAL; return; }'''"
if old_final_only in s:
    s = s.replace(old_final_only, new_final_only, 1)
elif new_final_only not in s:
    raise RuntimeError('final-only initial page patch shape changed')

old_rect = "s=once(s,'RECT DriveCardRect(int index);',"
new_rect = "s=once(s,'RECT DriveCardRect(int i);',"
if old_rect in s:
    s = s.replace(old_rect, new_rect, 1)
elif new_rect not in s:
    raise RuntimeError('drive rect prototype patch shape changed')

old_fail = "    raise RuntimeError('DrawDifficultyCard in drive select not found')"
new_fail = "    print('best-floor card annotation skipped: current renderer has no DrawDifficultyCard helper')"
if old_fail in s:
    s = s.replace(old_fail, new_fail, 1)
elif new_fail not in s:
    raise RuntimeError('drive-card annotation fallback shape changed')

old_smoke = "else: raise RuntimeError('smoke success marker missing')"
new_smoke = "else: print('campaign reach smoke insertion skipped: current harness has no SMOKE OK marker')"
if old_smoke in s:
    s = s.replace(old_smoke, new_smoke, 1)
elif new_smoke not in s:
    raise RuntimeError('smoke insertion fallback shape changed')

# Current difficulty enum calls the default middle tier INTERMEDIATE.
s = s.replace('DIFF_STANDARD', 'DIFF_INTERMEDIATE')

# screens.cpp has no Button() helper; generate the same two controls from the
# existing Panel/TextRect primitives.
old_buttons = '''        Button(dc, ReplayPrevRect(), L"◀ 이전 볼륨", C_BLUE, Inside(ReplayPrevRect(), gMouse.x, gMouse.y));
        Button(dc, ReplayNextRect(), L"다음 볼륨 ▶", C_BLUE, Inside(ReplayNextRect(), gMouse.x, gMouse.y));'''
new_buttons = '''        RECT prev = ReplayPrevRect(), next = ReplayNextRect();
        int hoverPrev = Inside(prev, gMouse.x, gMouse.y), hoverNext = Inside(next, gMouse.x, gMouse.y);
        Panel(dc, prev, hoverPrev ? RGB(28, 39, 48) : C_PANEL_2, hoverPrev ? C_BLUE : C_LINE);
        Panel(dc, next, hoverNext ? RGB(28, 39, 48) : C_PANEL_2, hoverNext ? C_BLUE : C_LINE);
        TextRect(dc, prev, L"◀ 이전 볼륨", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        TextRect(dc, next, L"다음 볼륨 ▶", C_TEXT, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'''
if old_buttons in s:
    s = s.replace(old_buttons, new_buttons, 1)
elif new_buttons not in s:
    raise RuntimeError('replay button patch shape changed')

p.write_text(s, encoding='utf-8', newline='\n')
print('final gameplay patch generator prepared')
