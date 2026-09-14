"""Check the new narrative's authored localization and source text hygiene."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
keys = set()
for line in (root / 'translations.tsv').read_text(encoding='utf-8-sig').splitlines():
    if line and not line.startswith('#') and '\t' in line:
        key = line.split('\t', 1)[0]
        keys.add(re.sub(r'\\([\\ntr])', lambda m: {'\\': '\\', 'n': '\n', 't': '\t', 'r': '\r'}[m[1]], key))
missing = set()
for name in ('src/narrative_data.h', 'src/narrative_visuals.inl'):
    text = (root / name).read_text(encoding='utf-8')
    for match in re.finditer(r'L("(?:[^"\\]|\\.)*")', text):
        value = json.loads(match.group(1))
        key = value.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t').replace('\r', '\\r')
        if re.search('[가-힣]', value) and value not in keys:
            missing.add(key)
if missing:
    print('Missing narrative translations:')
    print('\n'.join(sorted(missing)))
    raise SystemExit(1)
print('PASS: every Korean narrative card and cinematic UI literal has a translation')
