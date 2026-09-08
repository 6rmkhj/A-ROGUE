from pathlib import Path
p = Path(__file__).with_name('fix_final_gameplay_issues.py')
s = p.read_text(encoding='utf-8')
old = "s=once(s,'    campaign->checksum = Get32(bytes + 16);','    campaign->checksum = Get32(bytes + 22);','init checksum offset')"
new = "old_checksum='    campaign->checksum = Get32(bytes + 16);'\nif old_checksum not in s: raise RuntimeError('init checksum offset missing')\ns=s.replace(old_checksum,'    campaign->checksum = Get32(bytes + 22);',1)"
if old in s:
    s = s.replace(old, new, 1)
elif new not in s:
    raise RuntimeError('checksum patch shape changed')
p.write_text(s, encoding='utf-8', newline='\n')
print('final gameplay patch generator prepared')
