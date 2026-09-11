# 코드 읽기 지도 — Unity 이전 준비용

작성 2026-09-09 · 대상: 이전 작업 전에 코드가 어떻게 도는지 파악하려는 사람

읽기용 번들은 `python tools/bundle.py` 로 만든다. 결과는 `build/read/` 에 나온다.
**번들은 읽기 전용이다.** 빌드는 계속 `src/` 의 원본을 쓴다.

---

## 1. 먼저 — 이 코드는 분할이 심하지 않다

실제로 세어 보면 `src/` 전체가 **18,139줄, .cpp 12개 + .h 14개**다.
문제는 파일이 많은 게 아니라 **두 파일에 42%가 몰려 있는 것**이다.

| 파일 | 줄 | 비고 |
|---|---:|---|
| `screens.cpp` | 4,826 | 모든 화면. 가장 큰 파일 |
| `game.cpp` | 2,919 | 전투·진행 규칙 전부 |
| `smoke.cpp` | 2,514 | 테스트. 게임 실행 파일에 안 들어감 |
| `main.cpp` | 1,740 | 창·입력·전역 상태 |
| `render.cpp` | 1,021 | 그리기 도구 |
| `sprites.h` | 995 | 도트 초상 |
| `data.h` | 799 | 원본 데이터 |
| 나머지 19개 | 3,325 | |

그래서 **전부를 파일 하나로 합치면 18,000줄짜리 버퍼**가 되고, 지금보다 읽기 어려워진다.
대신 셋으로 묶었다. **그 경계가 곧 이전 계획이기 때문이다.**

게임 실행 파일이 실제로 컴파일하는 것은 8개다 —
`campaign · main · screens · render · localization · audio · music · game`.
`smoke · balance · curve · wav` 는 별도 실행 파일이다.

---

## 2. 세 계층과 각각의 운명

| 번들 | 내용 | 줄 | 이전 시 |
|---|---|---:|---|
| `01_rules.cpp` | `data.h` `game.h` `game.cpp` `campaign.*` | 4,425 | **거의 1:1 포팅** |
| `02_view.cpp` | `render.*` `screens.cpp` `ui.h` `sprites.h` `fx_*` `*_style.h` | 7,925 | **버리고 다시 만든다** |
| `03_platform.cpp` | `main.cpp` `audio.*` `music.*` `localization.*` | 2,777 | **Unity가 대체** |
| `04_tools.cpp` | `smoke` `balance` `curve` `wav` | 3,226 | 참고 — 규칙을 화면 없이 돌리는 예시 |

---

## 3. 가장 중요한 발견

**규칙 계층은 그리기를 전혀 모른다.**

| 파일 | Win32 참조 |
|---|---|
| `data.h` | **0건** — `stdint.h` 하나만 |
| `game.h` | **0건** — `stdint.h` 하나만 |
| `game.cpp` | 61건인데 **전부 문자열·메모리 유틸리티**다 |
| `campaign.cpp` | 파일 입출력만 |

`game.cpp` 의 61건 내역:

```
wsprintfW  55    lstrcpyW   5    lstrlenW  2
ZeroMemory 14    lstrcpynW  4    lstrcatW  2
```

**`HDC` · `HWND` · `COLORREF` 는 0건이다.** 즉 규칙 계층은 화면과 완전히 분리돼 있고,
Win32 의존은 전부 기계적으로 치환된다.

| 지금 | C# |
|---|---|
| `wsprintfW` | `string.Format` / 보간 문자열 |
| `ZeroMemory` | `Array.Clear` / 구조체 재할당 |
| `lstrcpyW` · `lstrcatW` · `lstrlenW` | `string` 연산 |
| `CreateFileW` · `ReadFile` · `WriteFile` (campaign.cpp) | `File.*` + `Application.persistentDataPath` |

> **결론:** 이전 작업의 위험은 규칙에 있지 않다. `01_rules.cpp` 4,425줄은 옮기면 되는 코드이고,
> 실제 작업은 `02_view.cpp` 7,925줄을 **다시 만드는 것**이다.

---

## 4. 한 프레임이 도는 경로

```
WM_PAINT                                  main.cpp
  └ PaintGame(hwnd)                       screens.cpp 4720행 근처
      ├ Fill(canvas, C_BG)
      ├ DrawHeader                        상단 68px
      ├ 화면 분기 ─ gGame.phase
      │   PHASE_TITLE          → DrawTitle
      │   PHASE_STORY          → DrawStory
      │   PHASE_DRIVE_SELECT   → DrawDriveSelect
      │   PHASE_DIRECTORY      → DrawDirectorySelect
      │   PHASE_COMBAT         → DrawCombat
      │   PHASE_REWARD         → DrawReward
      │   PHASE_PRUNE          → DrawPrune
      │   PHASE_ENDING_CHOICE  → DrawEndingChoice
      │   PHASE_GAMEOVER       → DrawEndScreen(0)
      │   PHASE_VICTORY        → DrawEndScreen(1)
      ├ 오버레이 (하나만)
      │   gTurnTraceActive     → DrawTurnCalculation    ← 턴 계산 재생
      │   gDescentActive       → DrawDescent
      │   gDirEnterActive      → DrawDirectoryEnter
      │   gCombatClearActive   → DrawCombatClear
      │   gBossIntroActive     → DrawBossIntro
      │   gDeckOpen/gSettingsOpen/gGuideOpen → 각 패널
      └ 캔버스(1120×760)를 창 크기로 확대
```

**모든 좌표는 고정 캔버스 1120×760 기준**이고 마지막에 한 번 확대된다(`render.h`).
Unity로 가면 이 확대가 `CanvasScaler` 로 바뀐다.

---

## 5. 한 턴이 도는 경로 — 여기가 이 코드의 핵심 설계

```
입력            main.cpp   슬롯 클릭 / 스페이스
  ↓
규칙            game.cpp   AssignDieToSlot() · EndTurn()
                           상태를 즉시 확정하고,
                           그 과정에서 일어난 사건을 CombatFxEvent 배열에 기록
  ↓
재생            ui.h       gTurnTraceStart 이후 경과 ms로 그 사건들을 되짚는다
                screens.cpp DrawTurnCalculation + 신호·충격·피해 숫자
```

**규칙은 먼저 끝나 있고, 화면은 나중에 그것을 재생한다.**
그래서 재생 도중 언제 건너뛰어도 결과가 같다.

여기에 딸린 규약이 하나 더 있다 — **모든 프레임은 경과 ms의 순수 함수다.**
프레임마다 누적하는 상태가 없어서 같은 시각이면 같은 그림이 나오고,
`tools/check-fx.bat` 의 픽셀 재현성 검사가 그걸 검증한다.

> **이전 시 주의.** Unity에서 `Time.deltaTime` 누적으로 바꾸면 이 규약과 검사 자산을 함께 잃는다.
> 시작 시각 기준의 순수 함수로 유지해야 옮길 수 있다.

---

## 6. 상태를 누가 소유하는가

`main.cpp` 가 전부 소유하고, `screens.cpp` 는 읽기만 한다.

| 전역 | 소유 | 내용 |
|---|---|---|
| `gGame` | main.cpp:12 | **게임 상태 전부.** `GameState` 하나에 다 들어 있다 |
| `gCampaign` | main.cpp:13 | 세이브 대상 진행도 |
| `gSettings` | main.cpp:16 | 배율·볼륨·언어·연출 강도 |
| `gWindow` `gMouse` | main.cpp | 창과 커서 |
| `gTurnTraceActive` `gDescentActive` `gBossIntroActive` `gDeathActive` … | main.cpp | 연출 진행 플래그 |
| `gGuideOpen` `gSettingsOpen` `gDeckOpen` | main.cpp | 패널 열림 |
| `gDirectoryArmed` `gTsrArmed` `gFaceSwapArmed` `gEndingArmed` | main.cpp | 되돌릴 수 없는 선택의 **후보 단계** |

`ui.h` 가 이 전역들의 `extern` 선언과 레이아웃 `RECT` 함수를 함께 들고 있다 —
**그리기와 클릭 판정이 같은 사각형을 보게 하려고** 한곳에 모아 둔 것이다.

> **이전 시 주의.** 전역 20여 개가 화면과 입력을 잇고 있다. Unity에서는 이걸 그대로 옮기지 말고
> `GameState` 는 순수 데이터로, 나머지 UI 플래그는 화면 쪽 상태로 갈라야 한다.

---

## 7. 읽는 순서

시간이 없으면 **1~3번만** 봐도 이전 계획을 세울 수 있다.

1. **`01_rules.cpp` 의 `data.h` `game.h`** (1,200줄) — 이 게임이 무엇으로 이루어졌는지가 전부 여기 있다.
   면·적·보스·기믹·드라이브·스토리 데이터와 `GameState` 구조.
2. **`04_tools.cpp` 의 `smoke.cpp`** — 규칙 계층을 화면 없이 돌리는 실제 코드다.
   **Unity에서 규칙만 먼저 살릴 때 그대로 참고할 수 있다.**
3. **`01_rules.cpp` 의 `game.cpp` 중 `EndTurn`** — 한 턴이 실제로 어떻게 해결되는지.
4. `03_platform.cpp` 의 `main.cpp` — 입력이 규칙으로 어떻게 들어가는지.
5. `02_view.cpp` 의 `screens.cpp` `DrawCombat` — 화면이 상태를 어떻게 읽는지.
   전부 읽을 필요 없다. **어차피 다시 만든다.**

---

## 8. 규칙 계층의 진입점 (game.h)

이 목록이 그대로 C# 인터페이스가 된다.

| 묶음 | 함수 |
|---|---|
| 런 시작 | `InitTitle` `NewRun` `SelectDrive` `SetSeenEndings` |
| 경로 | `BeginDirectorySelection` `SelectDirectoryChoice` `DirectoryChoiceCount` `DirectoryNodeAllowed` |
| 전투 | `StartCombat` `AssignDieToSlot` `UnassignDie` `SelectEnemy` `EndTurn` `KeybReroll` |
| 미리보기 | `PreviewTurn` → `TurnPreview` |
| 보상·정리 | `SelectReward` `InstallSelectedReward` `InstallTsr` `RepairSector` `SkipReward` `PruneFace` `ConfirmPrune` `CanUndoPrunedFace` `UninstallTsr` |
| 스토리 | `BeginStory` `AdvanceStory` `CurrentStoryFragment` `SelectEnding` `CommittedEnding` |
| 조회 | `FaceCost` `FacePower` `DeckBytes` `TsrBytes` `UsedBytes` `EffectiveCapacity` `SectorRepairAmount` `RecoveredShardCount` `EffectiveLawDrive` |
| 개발용 | `DebugWinCombat` `DebugWinDrive` `DebugJumpToBoss` `ConfigureDriveForTest` |

---

## 9. 번들 사용법

```bash
python tools/bundle.py
```

각 번들 맨 위에 목차가 있고, **원본 줄 번호로 되돌리는 오프셋**이 적혀 있다.

```
data.h    번들     9행부터   원본  800줄   (원본 N행 = 번들 14+N행)
game.cpp  번들  1224행부터   원본 2920줄   (원본 N행 = 번들 1229+N행)
```

번들에서 `game.cpp` 의 어떤 줄을 1500행에서 찾았다면 원본은 `1500 - 1229 = 271행`이다.

지역 `#include "..."` 는 순서를 번들이 이미 보장하므로 주석 처리하고 표시만 남겼다.

`build/read/` 는 파생물이다. 추적하고 싶지 않으면 `.gitignore` 에 `/build/read/` 한 줄을 넣으면 되고,
팀원과 공유하려면 그대로 커밋해도 된다.

---

## 10. 이전 작업 순서 제안

1. **`01_rules.cpp` 를 C#으로 옮긴다.** 화면 없이. `04_tools.cpp` 의 `smoke.cpp` 를 같이 옮겨
   **콘솔에서 규칙이 도는 것부터 확인한다.** 이게 되면 이전의 절반이 끝난다.
2. **`campaign.cpp` 의 세이브를 옮긴다.** 바이트 직렬화라 구조체 크기에 의존하므로 형식을 새로 정한다.
   구버전 세이브 변환 정책을 여기서 정해야 한다.
3. **화면을 새로 만든다.** `02_view.cpp` 는 참고 자료로만 본다. 좌표를 그대로 옮기지 않는다 —
   그 여백 대부분이 GDI에서 계산을 피하려고 굳힌 값이다.
4. **연출을 다시 만든다.** 단, §5의 "경과 ms의 순수 함수" 규약은 유지한다.
5. `audio.cpp` `music.cpp` 는 절차적 합성이라 로직 자체는 옮길 수 있지만,
   Unity의 오디오로 바꾸는 편이 싸다. **판단 필요.**
