"""Merge reviewed story/UI translations without duplicate keys or stale overrides."""
from pathlib import Path
import argparse

root = Path(__file__).resolve().parents[1]
target = root / 'translations.tsv'
rows = {}
for source in (target, root / 'narrative_translations.tsv', root / 'docs/narrative_ui_translations.tsv'):
    for line in source.read_text(encoding='utf-8-sig').splitlines():
        if not line or line.startswith('#') or line == '한국어\tEnglish':
            continue
        if '\t' not in line:
            raise ValueError(f'{source}: malformed translation row')
        key, value = line.split('\t', 1)
        rows[key] = value
text = '# AROGUE_TRANSLATIONS_V2\n# Korean source<TAB>English; escaped newlines and printf tokens are preserved.\n'
text += ''.join(f'{key}\t{value}\n' for key, value in rows.items())
parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
if args.check:
    if target.read_text(encoding='utf-8-sig') != text:
        raise SystemExit('translations.tsv needs merging')
else:
    target.write_text(text, encoding='utf-8', newline='\n')
print(f'{len(rows)} unique translations verified' if args.check else f'{len(rows)} unique translations merged')
