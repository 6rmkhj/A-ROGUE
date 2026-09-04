# -*- coding: utf-8 -*-
# BGM 악보 -> src/music.cpp 의 SONG 표 생성기.
#
#   python tools/gen_music.py
#
# 아래 SONGS 를 고치고 실행하면 music.cpp 안의 `static const MusicSong SONG[6] = { ... };`
# 블록만 제자리에서 다시 쓴다. 재생기 코드는 건드리지 않는다.
#
# 표기: 마디당 16토큰(X:\는 15개만 쓰이고 16번째는 무시). 숫자 = 반음, "." = 쉼표,
# "-" = 붙임(이전 음 유지). 반음 0 = A2(110Hz). 단조 음계: 0 2 3 5 7 8 10 12.
#   bass : 코드 근음 기준 오프셋
#   arp  : 코드 톤 번호 0 근음 1 3도 2 5도 3 옥타브 4 옥타브+3도 5 옥타브+5도
#   lead : tonic 기준 절대 반음, 섹션당 4마디(64토큰)
#   root/third : 8마디 코드 (A 4마디 + B 4마디). third 3 = 단조, 4 = 장조
#   drop : 이 스텝에서는 리드·아르페지오가 빠진다 (비트마스크)
import io, os, re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
TARGET = os.path.join(ROOT, 'src', 'music.cpp')

REST, TIE = -128, -127

def tokens(s, n):
    t = s.split()
    assert len(t) == n, (len(t), s)
    out = []
    for x in t:
        if x == '.': out.append(REST)
        elif x == '-': out.append(TIE)
        else:
            v = int(x); assert -60 <= v <= 60, x; out.append(v)
    return out

def bars(*ss):
    out = []
    for s in ss: out += tokens(s, 16)
    return out

def carr(vals):
    return '{' + ','.join(str(v) for v in vals) + '}'

m = 3; M = 4   # 단조 / 장조 3도

SONGS = []

# C:\ SYSTEM — A단조, 차분한 4비트. Am F C G / Dm Am F E
SONGS.append(dict(
    name='C:\\ SYSTEM', tonic=0, bpm=100, steps=16, duty=38, cut=150, drop=0x0000,
    root=[0, -4, 3, -2,   5, 0, -4, -5], third=[m, M, M, M,   m, m, M, M],
    bass=[tokens('0 . . . . . 0 . 7 . . . 0 . 5 .', 16),
          tokens('0 . . . . . 0 . 7 . . . 0 . 3 .', 16)],
    arp=[tokens('0 2 3 2 0 2 3 2 0 2 3 2 1 2 3 2', 16),
         tokens('0 2 3 5 3 2 0 2 0 2 3 5 3 2 1 2', 16)],
    lead=[bars('12 . . . 15 . 17 . 19 . . . . . 17 .',
               '15 . . . 17 . 15 . 12 . . . . . . .',
               '12 . . . 15 . 19 . 24 . . . 22 . 19 .',
               '17 . . . . . 19 . 14 . . . . . . .'),
          bars('17 . . . 20 . 22 . 24 . . . . . 22 .',
               '19 . . . 17 . 15 . 12 . . . . . . .',
               '20 . . . 24 . 20 . 17 . . . 15 . . .',
               '16 . . . . . 19 . 23 . . . . . . .')],
    kick=[0x1111, 0x1111], snare=[0x1010, 0x1010], hat=[0x5555, 0x5555],
))

# D:\ ARCHIVE — C단조, 느리고 성기게. Cm Cm Ab Fm / Eb Bb Cm Gm
SONGS.append(dict(
    name='D:\\ ARCHIVE', tonic=3, bpm=84, steps=16, duty=50, cut=90, drop=0x0000,
    root=[0, 0, -4, 5,   3, -2, 0, -5], third=[m, m, M, m,   M, M, m, m],
    bass=[tokens('0 . . . . . . . 0 . . 7 . . 5 .', 16),
          tokens('0 . . . . . . . 0 . . . 7 . . .', 16)],
    arp=[tokens('0 . 2 . 3 . 2 . 0 . 2 . 4 . 3 .', 16),
         tokens('0 . 2 . 3 . 4 . 3 . 2 . 0 . 2 .', 16)],
    lead=[bars('. . . . . . . . 15 . . . 17 . . .',
               '19 . . . . . . . 15 . . . 12 . . .',
               '. . . . 20 . . . 19 . . . 15 . . .',
               '17 . . . . . . . 20 . . . 17 . . .'),
          bars('22 . . . . . . . 19 . . . 15 . . .',
               '17 . . . . . . . 14 . . . 10 . . .',
               '12 . . . . . . . 15 . . . 19 . . .',
               '14 . . . . . . . 22 . . . 19 . . .')],
    kick=[0x0101, 0x0101], snare=[0x1000, 0x1000], hat=[0x4444, 0x4444],
))

# E:\ REMOVABLE — D단조, 접촉 불량. 3·10 스텝은 항상 빠진다. Dm C Bb C / Gm Dm Bb A
SONGS.append(dict(
    name='E:\\ REMOVABLE', tonic=5, bpm=108, steps=16, duty=30, cut=170, drop=0x0408,
    root=[0, -2, -4, -2,   5, 0, -4, -5], third=[m, M, M, M,   m, m, M, M],
    bass=[tokens('0 . 0 . . 0 . . 0 . . 0 . 7 . 5', 16),
          tokens('0 . 0 . . 0 . . 0 . . 0 . 3 . 5', 16)],
    arp=[tokens('0 3 2 3 0 3 2 3 1 3 2 3 0 3 2 5', 16),
         tokens('0 3 2 3 0 3 2 3 1 3 2 3 0 3 2 3', 16)],
    lead=[bars('12 . 12 . . 15 . . 17 . . 15 . . 12 .',
               '14 . 14 . . 12 . . 10 . . 12 . . 14 .',
               '15 . . . 17 . . . 20 . . . 17 . 15 .',
               '14 . . . 12 . . . 10 . . . . . . .'),
          bars('19 . 19 . . 17 . . 15 . . 17 . . 19 .',
               '17 . . . 15 . . . 14 . . . 12 . . .',
               '22 . . . 20 . . . 17 . . . 15 . . .',
               '13 . . . . . 16 . 19 . . . . . . .')],
    kick=[0x1109, 0x1109], snare=[0x1010, 0x1010], hat=[0xAAAA, 0xAAAA],
))

# N:\ NETWORK — B단조, 빠른 아르페지오. Bm Em G A / D A Bm F#
SONGS.append(dict(
    name='N:\\ NETWORK', tonic=2, bpm=124, steps=16, duty=25, cut=200, drop=0x0000,
    root=[0, 5, -4, -2,   3, -2, 0, -5], third=[m, m, M, M,   M, M, m, M],
    bass=[tokens('0 . 0 0 . 0 . 0 . 0 0 . 0 . 7 .', 16),
          tokens('0 . 0 0 . 0 . 0 . 0 0 . 0 . 5 .', 16)],
    arp=[tokens('0 2 3 5 3 2 0 2 3 5 3 2 0 2 3 2', 16),
         tokens('3 2 0 2 3 5 3 2 3 2 0 2 3 5 4 5', 16)],
    lead=[bars('12 . . 15 . . 19 . . . 17 . 15 . . .',
               '17 . . 19 . . 22 . . . 19 . 17 . . .',
               '20 . . 22 . . 24 . . . 27 . 24 . . .',
               '22 . . 24 . . 26 . . . 24 . 22 . . .'),
          bars('27 . . 24 . . 22 . . . 24 . 27 . . .',
               '26 . . 24 . . 22 . . . 20 . 22 . . .',
               '24 . . 22 . . 20 . . . 19 . 17 . . .',
               '19 . . . 23 . . . 26 . . . 23 . . .')],
    kick=[0x1111, 0x1111], snare=[0x1010, 0x1010], hat=[0xFFFF, 0xFFFF],
))

# R:\ RAMDISK — E단조(낮은 E2), 가장 빠르게. 상승 Em G Am C / 붕괴 D C B Em
SONGS.append(dict(
    name='R:\\ RAMDISK', tonic=-5, bpm=140, steps=16, duty=33, cut=210, drop=0x0000,
    root=[0, 3, 5, 8,   10, 8, 7, 0], third=[m, M, m, M,   M, M, M, m],
    bass=[tokens('0 . 0 . 0 . 0 . 0 . 0 . 0 . 0 .', 16),
          tokens('0 0 . 0 0 0 . 0 0 0 . 0 0 . 7 .', 16)],
    arp=[tokens('0 2 3 4 3 2 0 2 3 4 5 4 3 2 0 2', 16),
         tokens('5 4 3 2 3 4 5 4 3 2 0 2 3 4 3 2', 16)],
    lead=[bars('12 . 14 . 15 . . . 19 . . . 17 . 15 .',
               '15 . 17 . 19 . . . 22 . . . 19 . 17 .',
               '17 . 19 . 20 . . . 24 . . . 20 . 19 .',
               '20 . 22 . 24 . . . 27 . . . 24 . 22 .'),
          bars('26 . . . 24 . . . 22 . . . 21 . . .',
               '20 . . . 19 . . . 17 . . . 15 . . .',
               '16 . . . 14 . . . 11 . . . 14 . . .',
               '12 . . . . . . . . . . . 12 . 14 .')],
    kick=[0x1111, 0x1191], snare=[0x1010, 0x1010], hat=[0xFFFF, 0xFFFF],
))

# X:\ QUARANTINE — Bb단조, 15스텝 엇박, 반음 충돌. Bbm B Bbm Fm / Ebm B Ab Bbm
SONGS.append(dict(
    name='X:\\ QUARANTINE', tonic=1, bpm=96, steps=15, duty=45, cut=120, drop=0x0000,
    root=[0, 1, 0, -5,   5, 1, -2, 0], third=[m, M, m, m,   m, M, M, m],
    bass=[tokens('0 . . 0 . . 0 . . 0 . . 0 . 1 .', 16),
          tokens('0 . . 0 . . 0 . . 0 . . 7 . 1 .', 16)],
    arp=[tokens('0 1 2 0 1 2 0 1 2 0 1 2 0 1 2 .', 16),
         tokens('0 1 2 3 1 2 0 1 2 3 1 2 0 1 2 .', 16)],
    lead=[bars('12 . . 15 . . 13 . . 12 . . . . . .',
               '13 . . 17 . . 20 . . 17 . . 13 . . .',
               '12 . . 15 . . 17 . . 19 . . 15 . . .',
               '17 . . 22 . . 24 . . 22 . . 17 . . .'),
          bars('17 . . 20 . . 24 . . 20 . . 17 . . .',
               '13 . . 17 . . 20 . . 13 . . 25 . . .',
               '22 . . 26 . . 29 . . 26 . . 22 . . .',
               '12 . . 13 . . 12 . . . . . . . . .')],
    kick=[0x0249, 0x0249], snare=[0x1040, 0x1040], hat=[0x2492, 0x2492],
))

def song_init(s):
    assert len(s['root']) == 8 and len(s['third']) == 8
    for k in ('bass', 'arp'): assert len(s[k]) == 2 and all(len(v) == 16 for v in s[k])
    assert len(s['lead']) == 2 and all(len(v) == 64 for v in s['lead'])
    return ('    { // ' + s['name'] + '\n'
            f"      {s['tonic']}, {s['bpm']}, {s['steps']}, {s['duty']}, {s['cut']}, 0x{s['drop']:04X},\n"
            f"      {carr(s['root'])}, {carr(s['third'])},\n"
            f"      {{{carr(s['bass'][0])}, {carr(s['bass'][1])}}},\n"
            f"      {{{carr(s['arp'][0])}, {carr(s['arp'][1])}}},\n"
            f"      {{{carr(s['lead'][0])},\n       {carr(s['lead'][1])}}},\n"
            f"      {{0x{s['kick'][0]:04X}, 0x{s['kick'][1]:04X}}}, {{0x{s['snare'][0]:04X}, 0x{s['snare'][1]:04X}}}, {{0x{s['hat'][0]:04X}, 0x{s['hat'][1]:04X}}}\n"
            '    }')

def main():
    src = io.open(TARGET, 'rb').read().decode('utf-8').replace('\r\n', '\n')
    head = 'static const MusicSong SONG[6] = {\n'
    i = src.index(head) + len(head)
    j = src.index('\n};\n', i)
    data = ',\n'.join(song_init(s) for s in SONGS)
    out = src[:i] + data + src[j:]
    changed = out != src
    io.open(TARGET, 'wb').write(out.replace('\n', '\r\n').encode('utf-8'))
    print('src/music.cpp SONG table', 'updated' if changed else 'unchanged')

if __name__ == '__main__':
    main()
