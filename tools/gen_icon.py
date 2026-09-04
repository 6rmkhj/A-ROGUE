# -*- coding: utf-8 -*-
# 앱 아이콘 생성기. 도트 그림을 코드로 들고 있다가 .ico(16·32·48px)로 뽑는다.
#
#   python tools/gen_icon.py            -> src/arogue.ico
#   python tools/gen_icon.py preview    -> build/icon_preview.png 도 함께 (확대 미리보기)
#
# 그림: 3.5인치 플로피가 드라이브 슬롯에 정면으로 반쯤 들어가는 순간. 셔터가 먼저
# 들어가므로 위쪽에 라벨, 아래쪽에 셔터가 보이고 슬롯 아래는 베젤에 가려진다.
# 32px와 16px는 따로 그렸고 48px는 16px의 3배(굵고 또렷하다).
import io, os, struct, sys, zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

PALETTE = {
    '.': None,
    'K': (10, 14, 20),      # 외곽선
    'B': (44, 56, 72),      # 플로피 몸체
    'b': (70, 86, 108),     # 몸체 하이라이트
    'S': (176, 188, 202),   # 셔터
    's': (112, 124, 138),   # 셔터 창(어두운 부분)
    'L': (234, 238, 234),   # 라벨
    'G': (82, 231, 174),    # 라벨 줄 (게임의 초록)
    'D': (98, 108, 120),    # 드라이브 베젤
    'e': (128, 138, 150),   # 베젤 윗면 하이라이트
    'd': (58, 66, 76),      # 베젤 음영 · 배출 버튼
    'T': (6, 8, 12),        # 슬롯 안쪽
    'R': (255, 92, 82),     # 접근 LED (게임의 빨강)
}

ART32 = [
    "................................",
    ".......KKKKKKKKKKKKKKKKKK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbLLLLLLLLLLLLBBK.......",
    ".......KBbLGGGGGGGGGLLBBK.......",
    ".......KBbLLLLLLLLLLLLBBK.......",
    ".......KBbLGGGGGGLLLLLBBK.......",
    ".......KBbLLLLLLLLLLLLBBK.......",
    ".......KBbLLLLLLLLLLLLBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbBBBBBBBBBBBBBBK.......",
    ".......KBbSSSSSSSSSSSSSBK.......",
    ".......KBbSSSssssssSSSSBK.......",
    ".......KBbSSSssssssSSSSBK.......",
    "eeeeeeeKBbSSSssssssSSSSBKeeeeeee",
    "DDDDDTTKBbSSSssssssSSSSBKTTDDDDD",
    "DDDDDTTKBbSSSSSSSSSSSSSBKTTDDDDD",
    "DDDDDddddddddddddddddddddddDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDdddRRDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDdddRRDDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "dddddddddddddddddddddddddddddddd",
    "................................",
]

ART16 = [
    "................",
    "...KKKKKKKKK....",
    "...KbLLLLLLBK...",
    "...KbLGGGGLBK...",
    "...KbLLLLLLBK...",
    "...KbBBBBBBBK...",
    "...KbBBBBBBBK...",
    "...KbSSssSSBK...",
    "eeeKbSSssSSBKeee",
    "DDTKbSSSSSSBKTDD",
    "DDdddddddddddddD",
    "DDDDDDDDDDDDDDDD",
    "DDDDDDDDDDDdRRDD",
    "DDDDDDDDDDDDDDDD",
    "dddddddddddddddd",
    "................",
]

def pixels(art, scale=1):
    h = len(art); w = len(art[0])
    for row in art: assert len(row) == w, (len(row), row)
    return [[PALETTE[art[y // scale][x // scale]] for x in range(w * scale)] for y in range(h * scale)]

def ico_image(px):
    h = len(px); w = len(px[0])
    xor = bytearray()
    for y in range(h - 1, -1, -1):          # BMP는 아래에서 위로
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
    imgs = {16: pixels(ART16), 32: pixels(ART32), 48: pixels(ART16, 3)}
    ico = os.path.join(ROOT, 'src', 'arogue.ico')
    write_ico(ico, [(n, n, ico_image(imgs[n])) for n in (16, 32, 48)])
    print('wrote', ico, os.path.getsize(ico), 'bytes')
    if len(sys.argv) > 1 and sys.argv[1] == 'preview':
        sizes = (16, 32, 48); S = 6; gap = 6
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
