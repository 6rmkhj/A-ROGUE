# -*- coding: utf-8 -*-
# 앱 아이콘 생성기. 도트 그림을 코드로 들고 있다가 .ico(16·32·48px)로 뽑는다.
#
#   python tools/gen_icon.py            -> src/arogue.ico
#   python tools/gen_icon.py preview    -> build/icon_preview.png 도 함께 (확대 미리보기)
#
# 그림: 3.5인치 플로피가 드라이브 슬롯에 셔터부터 반쯤 들어가는 순간. 살짝 기울어져
# "들어가는 중"으로 읽히고, 슬롯 안은 그늘져 있다. 빛은 왼쪽 위에서 온다.
#
# ASCII 격자 대신 32단위 설계 좌표에 도형으로 그리고 크기마다 다시 래스터한다.
# 그래야 16·32·48 전부에서 외곽선이 1px로 또렷하고 기울기가 크기에 맞게 나온다.
# 16px는 기울기 없이 플로피를 더 크게 잡는 별도 레이아웃을 쓴다 (작으면 형태가 우선).
import io, os, struct, sys, zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

# 팔레트 — 게임의 초록·빨강을 포인트로 쓴다
K   = (10, 14, 20)       # 외곽선
B   = (36, 46, 60)       # 플로피 몸체
BL  = (64, 78, 98)       # 몸체 하이라이트 (왼쪽·위)
BD  = (24, 32, 42)       # 몸체 그늘 (오른쪽)
L   = (236, 240, 236)    # 라벨
LS  = (204, 210, 208)    # 라벨 아래 그늘
G   = (82, 231, 174)     # 라벨 줄 (게임의 초록)
GD  = (48, 160, 120)     # 초록 줄 그늘
TX  = (150, 158, 164)    # 라벨의 글씨 줄
S   = (186, 196, 208)    # 셔터
SL  = (222, 230, 238)    # 셔터 하이라이트
SD  = (108, 120, 134)    # 셔터 창
D   = (94, 104, 116)     # 베젤 앞면
DL  = (138, 148, 160)    # 베젤 윗면 (빛)
DD  = (54, 62, 72)       # 베젤 그늘 · 배출 버튼
T   = (8, 10, 14)        # 슬롯 안쪽
TL  = (152, 162, 174)    # 슬롯 아래 입술
R   = (255, 92, 82)      # 접근 LED
RD  = (150, 50, 46)      # LED 테두리
RL  = (255, 160, 150)    # LED 하이라이트

# 크기별 레이아웃 (32단위). 작은 아이콘은 형태가 우선이라 플로피를 키우고 기울기를 뺀다.
LAYOUT = {
    'big':   dict(bezel=21, slot0=23, slot1=27, fx0=6.0, fx1=25.0, ftop=1, fbot=27, tilt=0.12, motion=True,  cut=2),
    'small': dict(bezel=22, slot0=24, slot1=28, fx0=5.0, fx1=27.0, ftop=2, fbot=28, tilt=0.0,  motion=False, cut=2),
}

class Canvas:
    def __init__(self, n):
        self.n = n; self.s = n / 32.0
        self.px = [[None] * n for _ in range(n)]
    def put(self, x, y, c):
        if 0 <= x < self.n and 0 <= y < self.n: self.px[y][x] = c
    def p(self, v): return int(round(v * self.s))
    def rect(self, x0, y0, x1, y1, c):
        for y in range(self.p(y0), self.p(y1)):
            for x in range(self.p(x0), self.p(x1)): self.put(x, y, c)
    def hline(self, x0, x1, y, c):
        py = self.p(y)
        for x in range(self.p(x0), self.p(x1)): self.put(x, py, c)
    def vline(self, x, y0, y1, c):
        px = self.p(x)
        for y in range(self.p(y0), self.p(y1)): self.put(px, y, c)

def draw_drive(cv, lo):
    b, s0, s1 = lo['bezel'], lo['slot0'], lo['slot1']
    cv.rect(0, b, 32, 32, D)
    cv.rect(0, b, 32, b + 2, DL)            # 윗면에 빛
    cv.rect(0, 30, 32, 32, DD)              # 바닥 그늘
    cv.vline(0, b, 32, DL)
    cv.vline(31, b + 2, 32, DD)
    cv.rect(3, s0, 29, s1, T)               # 슬롯 구멍
    cv.hline(3, 29, s0, K)
    cv.hline(3, 29, s1, TL)                 # 아래 입술은 밝다
    cv.rect(20, s1 + 2, 24, s1 + 4, DD)     # 배출 버튼
    cv.rect(25, s1 + 2, 28, s1 + 4, RD)     # 접근 LED
    cv.rect(26, s1 + 2, 28, s1 + 4, R)
    cv.hline(26, 28, s1 + 2, RL)

def darker(c, pct): return tuple(v * pct // 100 for v in c)

def draw_floppy(cv, lo):
    s = cv.s
    top, bottom, x0, x1, tilt = lo['ftop'], lo['fbot'], lo['fx0'], lo['fx1'], lo['tilt']
    slot0 = lo['slot0']
    def shift(y): return (bottom - y) * tilt   # 아래(슬롯)를 축으로 위가 오른쪽으로 기운다
    py0, py1 = cv.p(top), cv.p(bottom)
    for py in range(py0, py1):
        y = py / s
        dx = shift(y) * s
        a = int(round(x0 * s + dx)); b = int(round(x1 * s + dx))
        for px in range(a, b):
            c = B
            if px == a or px == b - 1 or py == py0: c = K
            elif px == a + 1 or py == py0 + 1: c = BL
            elif px >= b - 2: c = BD
            # 라벨 (위쪽): 흰 종이, 초록 줄과 그 그늘, 글씨 줄, 아래 그늘
            ly0, ly1 = top + 2, top + 10
            if ly0 <= y < ly1 and a + 3 <= px < b - 3:
                c = L
                row = int(y) - top
                if row == 3 and a + 4 <= px < b - 5: c = G
                if row == 4 and a + 4 <= px < b - 5: c = GD
                if row == 6 and a + 4 <= px < b - 7: c = TX
                if row == 8 and a + 4 <= px < b - 9: c = TX
                if int(y) == ly1 - 1: c = LS
            # 셔터 (아래쪽 10단위): 금속, 위·왼쪽 하이라이트, 가운데 미닫이 창
            sy0 = bottom - 10
            if y >= sy0 and a + 2 <= px < b - 2:
                c = S
                if int(y) == sy0 or px == a + 2: c = SL
                if sy0 + 2 <= y < bottom - 1 and a + 6 <= px < b - 6: c = SD
                if sy0 + 2 <= y < sy0 + 3 and a + 6 <= px < b - 6: c = darker(SD, 80)
            # 슬롯 안으로 들어간 부분은 두 단계로 그늘진다
            if y >= slot0 - 1: c = darker(c, 60)
            if y >= slot0 + 1.5: c = darker(c, 45)
            cv.put(px, py, c)
    # 위 오른쪽 모서리의 사선 컷 (3.5인치 디스크의 그 모서리)
    cut = max(1, int(round(lo['cut'] * s)))
    right = int(round(x1 * s + shift(top) * s))
    for i in range(cut):
        for j in range(cut - i): cv.put(right - 1 - j, py0 + i, None)
        cv.put(right - 1 - (cut - i), py0 + i, K)
    # 쓰기 방지 탭 (위 왼쪽의 작은 홈)
    left = int(round(x0 * s + shift(top) * s))
    cv.put(left + 2, py0 + 1, BD)

def draw_motion(cv, lo):
    # 플로피 왼쪽에 짧은 선 두 개. 내려오는 중이라는 힌트다.
    x = lo['fx0'] + shift_at(lo, lo['ftop'] + 4)
    cv.hline(x - 4, x - 1, lo['ftop'] + 4, DL)
    cv.hline(x - 5, x - 1, lo['ftop'] + 7, DL)

def shift_at(lo, y): return (lo['fbot'] - y) * lo['tilt']

def render(n):
    lo = LAYOUT['small' if n <= 16 else 'big']
    cv = Canvas(n)
    draw_floppy(cv, lo)
    saved = [row[:] for row in cv.px]
    draw_drive(cv, lo)
    # 슬롯 창 안에서는 플로피가 다시 보인다 (베젤이 그 밑을 가린다)
    y0, y1 = cv.p(lo['slot0']) + 1, cv.p(lo['slot1'])
    xs0, xs1 = cv.p(3), cv.p(29)
    for py in range(y0, y1):
        for px in range(xs0, xs1):
            if saved[py][px] is not None: cv.px[py][px] = saved[py][px]
    if lo['motion']: draw_motion(cv, lo)
    return cv.px

def ico_image(px):
    h = len(px); w = len(px[0])
    xor = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            c = px[y][x]
            xor += bytes((0, 0, 0, 0)) if c is None else bytes((c[2], c[1], c[0], 255))
    mask = bytearray()
    rowBytes = ((w + 31) // 32) * 4
    for y in range(h - 1, -1, -1):
        bits = 0; row = bytearray()
        for x in range(w):
            bits = (bits << 1) | (1 if px[y][x] is None else 0)
            if x % 8 == 7: row.append(bits); bits = 0
        if w % 8: row.append(bits << (8 - w % 8))
        row += b'\x00' * (rowBytes - len(row))
        mask += row
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, len(xor) + len(mask), 0, 0, 0, 0)
    return header + xor + mask

def write_ico(path, images):
    out = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images); body = b''
    for w, h, data in images:
        out += struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, 1, 32, len(data), offset + len(body))
        body += data
    open(path, 'wb').write(out + body)

def write_png(path, px):
    h = len(px); w = len(px[0])
    raw = b''.join(b'\x00' + b''.join(bytes(c) for c in row) for row in px)
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)

def main():
    sizes = (16, 32, 48)
    imgs = {n: render(n) for n in sizes}
    ico = os.path.join(ROOT, 'src', 'arogue.ico')
    write_ico(ico, [(n, n, ico_image(imgs[n])) for n in sizes])
    print('wrote', ico, os.path.getsize(ico), 'bytes')
    if len(sys.argv) > 1 and sys.argv[1] == 'preview':
        S = 6; gap = 6
        dark, light = (8, 12, 17), (240, 240, 240)
        W = (sum(sizes) + gap * (len(sizes) + 1)) * S; H = (48 + gap * 2) * S * 2
        canvas = [[dark] * W for _ in range(H)]
        for line, bg in ((0, dark), (1, light)):
            oy = line * (48 + gap * 2) * S
            for y in range(oy, oy + (48 + gap * 2) * S):
                for x in range(W): canvas[y][x] = bg
            x = gap * S
            for n in sizes:
                px = imgs[n]; top = oy + (gap + 48 - n) * S
                for yy, row in enumerate(px):
                    for xx, c in enumerate(row):
                        if c is None: continue
                        for dy in range(S):
                            for dx in range(S): canvas[top + yy * S + dy][x + xx * S + dx] = c
                x += (n + gap) * S
        out = os.path.join(ROOT, 'build', 'icon_preview.png')
        write_png(out, canvas)
        print('wrote', out)

if __name__ == '__main__':
    main()
