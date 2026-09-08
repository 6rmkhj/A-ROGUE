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
new_seed = "s=once(s,'    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);',\n'''    PickDriveDifficulties(game, CampaignChoiceRandom(game->clearedMask) ^ 0x9E3779B9u);\n    if ((game->clearedMask & 0x3F) == 0x3F) SetReplayDrivePage(game, 0);''','replay difficulty reset')"
if old_seed in s:
    s = s.replace(old_seed, new_seed, 1)
elif new_seed not in s:
    raise RuntimeError('drive difficulty patch shape changed')

p.write_text(s, encoding='utf-8', newline='\n')
print('final gameplay patch generator prepared')
