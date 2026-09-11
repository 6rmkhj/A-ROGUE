#!/usr/bin/env python3
"""소스 전체를 파일 하나로 합친다. 읽기 전용.

    python tools/bundle.py

결과: build/read/abs.cpp

의존 순서대로 헤더를 먼저 깔고 구현을 잇는다. 시스템 include 는 맨 위로 모으고
지역 include 는 주석 처리한다. 맨 앞에 목차와 전역 색인을 붙인다.

빌드는 계속 src/ 의 원본을 쓴다. 이 파일을 고치지 말 것.
"""

import os
import re
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
OUT = os.path.join(ROOT, "build", "read")

# 의존 순서. 헤더가 먼저, 그다음 구현, 마지막이 별도 실행 파일용 도구.
ORDER = [
    ("data.h",           "원본 데이터 — 면·적·보스·드라이브·기믹·스토리"),
    ("game.h",           "규칙 계층 공개 API 와 GameState"),
    ("campaign.h",       "세이브 구조"),
    ("sprites.h",        "코드로 그리는 16x16 도트 초상"),
    ("render.h",         "그리기 도구와 애니메이션 어휘"),
    ("fx_timing.h",      "연출 타이밍 상수"),
    ("ui.h",             "전역 상태 extern 선언과 레이아웃 RECT"),
    ("fx_draw.h",        "연출 프리미티브"),
    ("scene_style.h",    "화면별 배경 양식"),
    ("combat_style.h",   "전투 연출 양식"),
    ("presentation.h",   "위 둘을 묶는 헤더"),
    ("localization.h",   "번역표 인터페이스"),
    ("audio.h",          "효과음 인터페이스"),
    ("music.h",          "BGM 인터페이스"),

    ("game.cpp",         "전투·진행·보상·기믹 규칙. 그리기를 모른다"),
    ("campaign.cpp",     "세이브 파일 입출력"),
    ("localization.cpp", "translations.tsv 로더"),
    ("render.cpp",       "그리기 도구 구현"),
    ("screens.cpp",      "모든 화면. 가장 큰 파일"),
    ("audio.cpp",        "메모리에서 합성하는 효과음"),
    ("music.cpp",        "4채널 절차적 BGM"),
    ("main.cpp",         "WinMain · 메시지 루프 · 입력 · 전역 상태 소유"),

    ("smoke.cpp",        "[별도 실행 파일] 결정론적 스모크 테스트"),
    ("balance.cpp",      "[별도 실행 파일] 밸런스 검사"),
    ("curve.cpp",        "[별도 실행 파일] 성장 곡선 검사"),
    ("wav.cpp",          "[별도 실행 파일] BGM WAV 출력"),
]

BAR = "=" * 92
SYS_INC = re.compile(r'^\s*#include\s*<[^>]+>')
LOC_INC = re.compile(r'^\s*#include\s*"[^"]+"')
PRAGMA = re.compile(r'^\s*#pragma\s+once')

# 전역 색인용 — 이 프로젝트의 명명 규칙(g + 대문자)을 따르는 이름
GLOBAL = re.compile(r'\bg[A-Z][A-Za-z0-9_]*\b')


def main():
    files = []
    for name, desc in ORDER:
        path = os.path.join(SRC, name)
        if not os.path.exists(path):
            print("  없음: %s" % name)
            continue
        with open(path, encoding="utf-8-sig") as f:
            files.append((name, desc, f.read().split("\n")))

    # 1) 시스템 include 를 모은다
    sysinc = OrderedDict()
    for _, _, lines in files:
        for ln in lines:
            if SYS_INC.match(ln):
                sysinc[ln.strip()] = True

    body = []
    n = [1]

    def emit(text=""):
        body.append(text)
        n[0] += text.count("\n") + 1

    emit("// " + BAR)
    emit("// A:\\ROGUE — 전체 소스 병합본")
    emit("//")
    emit("// 읽기 전용이다. 컴파일 대상이 아니다 (아래 도구 4종이 각자 main 을 갖는다).")
    emit("// 빌드는 계속 src/ 의 원본을 쓴다. 고칠 일이 있으면 src/ 를 고칠 것.")
    emit("// tools/bundle.py 가 생성한다.")
    emit("// " + BAR)
    emit()
    toc_slot = len(body)
    emit()
    idx_slot = len(body)
    emit()
    emit()
    emit("// ---- 시스템 헤더 (전 파일에서 모음) " + "-" * 54)
    for inc in sysinc:
        emit(inc)
    emit()

    toc = []
    for name, desc, lines in files:
        emit()
        emit("// " + BAR)
        emit("// %s" % name)
        emit("// %s" % desc)
        emit("// " + BAR)
        emit()
        head = n[0]                      # 원본 1행이 놓일 줄
        toc.append((name, desc, head, len(lines)))
        for ln in lines:
            if SYS_INC.match(ln) or LOC_INC.match(ln) or PRAGMA.match(ln):
                emit("//[merged] " + ln.rstrip())
            else:
                emit(ln)

    # 2) 목차
    t = ["// ---- 목차 " + "-" * 80, "//",
         "//   파일              시작 줄     원본 줄수   원본 N행 → 병합본 줄",
         "//   " + "-" * 68]
    for name, desc, head, count in toc:
        t.append("//   %-16s %7d %10d      N + %d" % (name, head, count, head - 1))
    t.append("//")
    t.append("//   병합본 L행의 원본 줄 = L - (위 오프셋)")
    body[toc_slot] = "\n".join(t)

    # 3) 전역 색인 — 정의된 줄과 등장 횟수
    text = "\n".join(body)
    all_lines = text.split("\n")
    counts = {}
    first = {}
    for i, ln in enumerate(all_lines, 1):
        if ln.startswith("//"):
            continue
        for m in GLOBAL.findall(ln):
            counts[m] = counts.get(m, 0) + 1
            if m not in first:
                first[m] = i
    ranked = sorted(counts.items(), key=lambda kv: -kv[1])
    g = ["// ---- 전역 색인 " + "-" * 75, "//",
         "//   이 프로젝트는 전역에 g + 대문자로 이름을 붙인다. 등장 순.",
         "//   " + "-" * 68]
    for name, c in ranked[:60]:
        g.append("//   %-26s %5d회   첫 등장 %6d행" % (name, c, first[name]))
    if len(ranked) > 60:
        g.append("//   ... 그 밖에 %d개" % (len(ranked) - 60))
    g.append("//")
    g.append("//   에디터에서 이름을 그대로 검색하면 정의와 모든 사용처가 한 버퍼에 나온다.")
    body[idx_slot] = "\n".join(g)

    os.makedirs(OUT, exist_ok=True)
    out = os.path.join(OUT, "abs.cpp")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(body))

    total = len("\n".join(body).split("\n"))
    print("build/read/abs.cpp  %d행  %.0f KB  (파일 %d개, 전역 %d개)"
          % (total, os.path.getsize(out) / 1024.0, len(files), len(ranked)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
