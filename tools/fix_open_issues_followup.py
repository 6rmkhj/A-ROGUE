from pathlib import Path

p = Path(__file__).resolve().parents[1] / "src" / "game.cpp"
s = p.read_text(encoding="utf-8")
old = '''static uint32_t CampaignChoiceRandom(uint8_t clearedMask) {
    uint32_t x = 0xA341316Cu ^ ((uint32_t)clearedMask * 0x9E3779B9u);'''
new = '''static uint32_t CampaignChoiceRandom(uint8_t clearedMask) {
    clearedMask &= 0x3Fu;
    uint32_t x = 0xA341316Cu ^ ((uint32_t)clearedMask * 0x9E3779B9u);'''
if s.count(old) != 1:
    raise RuntimeError("CampaignChoiceRandom normalization target not found exactly once")
p.write_text(s.replace(old, new, 1), encoding="utf-8", newline="\n")
print("campaign mask normalized")
