# 기믹 연출 애니메이션 구현 계획

보스 기믹 18종의 발동 연출을 화려하게 만들기 위한 **기술 설계**다. 어떤 그림을
그릴지가 아니라, 그 그림을 그릴 도구를 어떻게 만들지를 다룬다.

## 목적

지금 연출은 배너·충격 프레임·화면 찢김·충격파·파편이 있지만 움직임이 기계적이다.
원인은 연출 아이디어가 아니라 **애니메이션을 표현할 어휘가 없다는 것**이다.

---

## 진단

현재 구조는 **매 프레임이 경과 ms의 순수 함수**다. 애니메이션 기반으로는 옳다.
`GimmickFxElapsed()`가 시간을 주고, 각 연출이 그 값으로 그림을 계산하며, 16ms
타이머가 리페인트를 돌린다. 상태를 들고 다니지 않으므로 결정론이 깨지지 않는다.

문제는 이 위에 얹을 도구가 없다는 것이다.

| 없는 것 | 결과 |
|---|---|
| 이징·보간 어휘 | 전부 `p < 420 ? A : B` 손코딩. 선형이라 기계적으로 움직인다 |
| 이전 프레임 접근 | 잔상·트레일·모션블러·데이터모싱이 **원천적으로 불가** |
| 스프라이트 변형 | `DrawSpriteArt`가 정수 균일 배율이라 스쿼시/스트레치 불가 |
| 파티클 시간차 | 전부 `t=0`에 태어나 폭발이 납작하다 |
| 시간 정지 | 히트스톱 불가 |

### 스프라이트 제약 (확인함)

`src/render.cpp`의 `DrawSpriteArt`는 이렇게 배율을 정한다.

```c
int scale = (boxWidth < boxHeight ? boxWidth : boxHeight) / SPRITE_SIZE;
if (scale < 1) scale = 1;
```

**정수 균일 배율**이다. 박스를 찌그러뜨려도 스프라이트는 늘어나지 않고, 크기를
바꾸면 배율이 `7 → 6`처럼 툭툭 끊긴다. 스쿼시/스트레치는 지금 구조로 불가능하며
우회로가 필요하다 (부품 3).

---

## 부품 1 · 트랙과 이징

**코어.** 이후 모든 작업이 여기 얹힌다.

정수 `0~1000`만 쓴다. 기존 코드 규약대로 부동소수점을 들이지 않는다.

```c
// render.h
int  Track(int t, int fromMs, int toMs);   // 구간 진행도. 구간 밖은 0/1000으로 물린다
int  Ease(int p, int curve);
int  Lerp(int a, int b, int p);
RECT LerpRect(const RECT& a, const RECT& b, int p);

enum EaseCurve {
    EASE_LINEAR, EASE_OUT_CUBIC, EASE_IN_CUBIC,
    EASE_IN_OUT, EASE_OUT_BACK, EASE_OUT_ELASTIC, EASE_OUT_BOUNCE
};
```

정수 구현은 짧다.

```c
static int EaseOutCubic(int p) { int q = 1000 - p; return 1000 - q * q / 1000 * q / 1000; }
static int EaseInCubic(int p)  { return p * p / 1000 * p / 1000; }
static int EaseOutBack(int p)  { int q = 1000 - p; return 1000 - (q * q / 1000 * (2700 * q / 1000 - 1700)) / 1000; }
```

### 무엇이 달라지는가

지금:

```c
int close = act < 420 ? height * act / 420 : height;
```

바뀐 뒤:

```c
int close = Lerp(0, height, Ease(Track(t, 130, 320), EASE_OUT_BACK));
```

읽기도 쉽고, **커브 상수만 바꿔 느낌을 조정**할 수 있다. 18종을 각각 손보지 않고
튜닝된다. 구간을 명시하므로 예비 동작·본 동작·여운의 경계가 코드에 드러난다.

> **비용** 1.5시간 · **위험** 낮음 (순수 함수, 기존 동작 재현 가능)

---

## 부품 2 · 프레임 스냅샷

**가장 큰 해금.** 잔상 계열 전체가 여기서 열린다.

`PaintGame`이 이미 `canvas` 비트맵을 만든다. 연출이 시작되는 시점의 캔버스를 별도
비트맵에 복사해 두고, 연출 동안 어긋나게·옅게 겹쳐 그린다.

```c
// render.h
void FxSnapshotCapture(HDC canvas);                          // 지금 화면을 붙잡는다
void FxSnapshotBlit(HDC dc, int dx, int dy, int keepPercent); // 어긋나게 겹친다
void FxSnapshotRelease();
```

구현은 `CreateCompatibleBitmap(1120 x 760)` 하나와 `BitBlt`가 전부다. 런타임 메모리
약 3.4MB이고 **실행 파일 크기에는 영향이 없다.**

### 열리는 것

- 모션 트레일 — 슬롯이 이동할 때 궤적이 남는다
- 데이터모싱 — 이전 프레임 조각이 남아 신호가 뭉개진다
- 되감기 잔상 — 복원 계열에서 보스의 직전 모습이 겹친다
- 화면 전체 고스팅

역전 계열의 "이전 순서가 잔상으로 남는다"가 여기서 나온다.

> **비용** 1.5시간 · **위험** 중간 (GDI 리소스 수명 관리 필요)

---

## 부품 3 · 스프라이트 변형

정수 균일 배율 제약을 우회한다. 스프라이트를 작은 오프스크린에 한 번 그리고
`StretchBlt`로 임의 사각형에 늘린다.

```c
// render.h
void DrawSpriteStretched(HDC dc, const RECT& box, int kind, int alive, int flash);
```

박스의 가로세로를 따로 조절할 수 있으므로 **스쿼시 앤 스트레치**가 들어온다.

- 보스가 맞을 때 가로로 눌린다
- 복원할 때 세로로 늘어났다 되돌아온다
- 압력이 찰 때 부푼다

디즈니 12원칙 중 게임에서 가장 효과가 큰 항목이고, 지금은 쓸 수가 없다.

> **비용** 1.5시간 · **위험** 중간 (`DrawPortrait` 호출부와 병행 유지 필요)

---

## 부품 4 · 시간차 방출 파티클

현재 `DrawFxShards`는 모든 파편이 `t=0`에 태어나 한 덩어리로 퍼진다. 인자 하나를
더해 층을 만든다.

```c
void DrawFxShards(HDC dc, int cx, int cy, int t, int life,
                  int count, int seed, COLORREF fam, int staggerMs);
```

```c
int born = i * staggerMs;
int age  = t - born;
if (age < 0) continue;                       // 아직 태어나지 않았다
int fall = age * age / 900;                  // 자기 수명 기준 중력 가속
```

> **비용** 40분 · **위험** 낮음

---

## 부품 5 · 히트스톱

**가장 값싸고 효과가 크다.** `GimmickFxElapsed()` 한 곳만 고치면 모든 트랙이 자동으로
얼어붙는다. 새 그리기 코드가 없다.

```c
int GimmickFxElapsed() {
    if (!gFxActive) return 0;
    int raw = (int)(GetTickCount() - gFxStart);
    int at = GimmickFxImpactAt(gFxKind), hold = 70;   // 착지 시점에서 70ms 정지
    if (raw < at) return raw;
    if (raw < at + hold) return at;                   // 시간이 멈춘다
    return raw - hold;
}
```

`GimmickFxDuration`에 `hold`만큼 더해 주면 뒤가 잘리지 않는다.

Vlambeer의 표현으로는 *"그게 중요했다고 말하는 아주 작은 마찰"*이다. 지금 연출에는
멈추는 순간이 없어 무게가 실리지 않는다.

> **비용** 30분 · **위험** 낮음

---

## 순서와 비용

| 단계 | 부품 | 비용 | 왜 이 순서인가 |
|---|---|---:|---|
| 1 | 5 히트스톱 | 30분 | 30분에 18종 전부가 올라간다 |
| 2 | 1 트랙·이징 | 1.5시간 | 기반. 이후 전부 여기 얹힌다 |
| 3 | 기존 18종 이관 | 2시간 | 코드가 줄고 커브 튜닝이 가능해진다 |
| 4 | 2 스냅샷 | 1.5시간 | 잔상 계열 전체 해금 |
| 5 | 4 파티클 시간차 | 40분 | |
| 6 | 3 스프라이트 변형 | 1.5시간 | 보스 반응 |

합계 약 **8시간**.

**1번과 2번이 핵심이다.** 트랙·이징을 먼저 깔지 않으면 3단계 이후가 다시 손코딩이
된다. 시간이 부족하면 1·2·3까지만 해도 체감이 크게 달라진다.

---

## 만들지 않을 것

씬 그래프, 트윈 매니저, 동적 할당 기반 파티클 풀은 만들지 않는다.

지금의 **"프레임 = 시간의 순수 함수"** 규약이 결정론과 스모크 안정성의 근거다.
상태를 들고 다니는 애니메이션 시스템은 그 규약을 깬다. 트랙 함수는 순수 함수이므로
이 규약을 지킨다.

프레임 스냅샷(부품 2)만이 유일하게 상태를 가지는데, 이것은 그리기 전용 버퍼이고
게임 상태가 아니므로 규약 밖이다.

---

## 검증 기준

모든 작업은 표시 전용이다. 다음이 유지되어야 한다.

```
PASS: roster, sprites, spawn matrix, 18 gimmick scenarios, directory routing, 48 complete runs
BALANCE: 933/1800 total heuristic wins, 8.06 average combats
```

**밸런스 표본이 933에서 움직이면 규칙에 손을 댄 것이다.** 즉시 되돌린다.

> 기준선은 952 → 933으로 한 번 바뀌었다. SANDBOX.BREACH를 소환 기믹으로
> 교체한 것(이슈 #32)이 유일한 원인이고, 그때 drive 0~4는 한 판도 움직이지
> 않았다. 자세한 내용은 [SPAWN_PLAN.md](SPAWN_PLAN.md).

---

## 부록 · 선행 버그

애니메이션 작업 전에 고쳐야 한다. 안 고치면 아무리 다듬어도 김이 빠진다.

### 증상

잠금 기믹에서 턴이 지나기 전에 이미 슬롯이 잠겨 보이고, 턴이 지난 뒤에야 잠금
애니메이션이 나온다. 연출이 사건이 아니라 사후 보고가 된다.

### 원인

```
ExecuteCombatTurn
 └ EndTurn
    └ (턴 해결) → turn++ → BeginTurn → GimmickTurnBegin
                                        └ lockedSlot[i] = 1      ← 여기서 이미 잠긴다
 └ BeginTurnTrace                                                 ← 계산 재생 시작
      DrawCombat이 뒤에서 계속 그려지므로 DrawSlot이 붉은 "잠김"을 이미 표시
 └ (클릭) → FinishTurnTrace
    └ BeginGimmickFx                                              ← 이제야 셔터 애니메이션
```

`GimmickTurnBegin` 계열 **9종 전부**(잠금 3 · 오프라인 3 · 역전 3)에 같은 문제가 있다.
`GimmickTurnEnd` 계열 9종(복원 · 압력 · 격리)은 계산 재생이 직접 서술하므로 무관하다.

### 수정 방향

규칙은 건드리지 않는다. 상태는 그대로 두고 **보여주는 시점만** 미룬다.

```c
int GimmickRevealPending(int kind, int target);
// 계산 재생 중이고 이번 턴 firedFx가 TurnBegin 계열이거나,
// 연출이 진행 중이고 아직 착지 지점을 지나지 않았으면 1
```

착지 지점은 계열마다 다르다 — 잠금은 셔터가 닿는 지점, 오프라인은 노이즈가 덮는
지점, 역전은 자리바꿈이 끝나는 지점.

세 곳에서 읽는다.

| 위치 | 지금 | 바뀔 것 |
|---|---|---|
| `DrawSlot` | `SlotLockedThisTurn`이면 즉시 붉은 패널 | 착지 전이면 평소대로 |
| `DrawDie` | `die->offline`이면 "오프라인" | 착지 전이면 평소대로 |
| 사이드바·안내 줄 | `ResolveOrderReversed`면 붉게 | 착지 전이면 평소 순서 |

배치 거부는 규칙 쪽 `AssignDieToSlot`이 그대로 막으므로 안전하다.

> **비용** 1~2시간 · **위험** 낮음 (표시 전용)
