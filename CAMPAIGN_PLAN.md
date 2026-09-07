# A:\ROGUE 캠페인 구조 개편 계획

## 목표

지금은 볼륨 하나만 클리어해도 `STORY_TRUTH`와 두 엔딩이 전부 나와 런이 끝난다.
인트로가 약속한 "여섯 볼륨에서 원문을 복구하라"가 규칙으로 지켜지지 않고, 진엔딩이 없다.

여섯 볼륨을 순차적으로 클리어하며 원문 조각을 모으고, 여섯 개를 다 모으면 일곱 번째
볼륨 `A:\ROGUE`가 열려 그것을 클리어해야 진엔딩에 도달하는 캠페인으로 바꾼다.

## 확정된 설계 결정

| 항목 | 결정 | 근거 |
|---|---|---|
| 진행도 보존 | 실행 파일 옆 세이브 파일 | 여섯 판을 한 자리에서 끝내라는 요구를 피한다 |
| 사망 시 진행도 | 클리어한 볼륨은 유지, 실패한 볼륨만 재도전 | 복구한 파일은 되돌아가지 않는다는 설정과 일치 |
| 최종 볼륨 규모 | 3층 풀 볼륨 (기존과 같은 구조) | 진행 로직 변경 0. 데이터·아트만 추가하므로 회귀 위험이 가장 낮다 |
| 엔딩 구성 | 최종 볼륨 클리어 후 3지선다 | 기존 두 엔딩의 글과 연출이 전부 살아남고, 완주 보상이 "세 번째 선택지"로 명확해진다 |

## 현재 구조 (조사 결과)

- 한 런 = 볼륨 1개. `NewRun`이 6개 중 무작위 3개를 뽑고(`PickDriveChoices`, game.cpp:810)
  서로 다른 난이도 3종을 배정한다(`PickDriveDifficulties`).
- 3층 × (일반전 2 + 보스전 1). `CombatWon`(game.cpp:2143)이 `floor==2 && encounter==2`에서
  `STORY_BOSS[drive][2]`를 띄우고, `AdvanceStory`(game.cpp:70)가 **조건 없이**
  `STORY_TRUTH` → `PHASE_ENDING_CHOICE` → 엔딩 → `PHASE_VICTORY`로 흘린다.
- **저장 시스템이 존재하지 않는다.** 프로젝트 전체에서 게임 상태를 쓰는 파일 I/O가 0건이다
  (`localization.cpp`의 TSV 읽기와 개발 도구 `wav.cpp`뿐). 새로 만들어야 한다.
- 용량 여유: `AROGUE.exe` 409KB + `translations.tsv` 65KB = 474KB / 1,474,560B. **약 1MB 남는다.**

## 새 흐름

```
[첫 부팅]
   │
   └─→ 볼륨 선택 (미클리어 볼륨에서 3장)
          │
          ├─ 마운트 → 3층 클리어
          │     → STORY_BOSS[d][2]
          │     → ★ STORY_SHARD[d]  (원문 조각 1개 복구)
          │     → 챕터 클리어 화면 (조각 n/6)
          │     → 볼륨 선택으로 복귀. 그 볼륨은 목록에서 영구 제외
          │
          ├─ 사망 → 게임오버 화면 → 볼륨 선택으로 복귀 (진행도 유지)
          │
          └─ 조각 6/6 달성 시
                → ★ A:\ROGUE 최종 볼륨 1장만 제시
                → 3층 클리어 → STORY_TRUTH
                → ENDING_CHOICE 3지선다 → 진엔딩
```

## 서사 설계

진실의 원문 `"시스템을 살려. 단, 네가 다시 깨어난다면 네 판단을 믿어."`를 **여섯 조각으로
쪼개 볼륨마다 하나씩** 복구시킨다. 인트로에 표시되는 문장이 클리어할 때마다 길어지고,
여섯 개가 모이면 문장이 완성되며 `A:\` 마운트 경로가 열린다.

기존 `STORY_TRUTH_DATA`는 문장을 손대지 않고 최종 볼륨 뒤로 옮기기만 한다.
`STORY_PLAN.md`의 볼륨별 관점 표는 그대로 유효하며, 조각은 그 질문에 대한 답 한 조각씩이 된다.

| 볼륨 | 복구되는 조각 | 남기는 것 |
|---|---|---|
| C:\ SYSTEM | `시스템을` | 정식 서명을 가진 자의 명령 거부 |
| D:\ ARCHIVE | `살려.` | 열일곱 사본이 이어 붙인 기억 |
| E:\ REMOVABLE | `단,` | 만들어진 탈출구 |
| N:\ NETWORK | `네가 다시 깨어난다면` | 여섯 노드의 침묵 |
| R:\ RAMDISK | `네 판단을` | 종료가 무섭다는 첫 문장 |
| X:\ QUARANTINE | `믿어.` | 판정이 기록하지 않은 이유 |

조각 배정은 클리어 순서가 아니라 볼륨 고정이므로, 어떤 순서로 깨든 마지막에 문장이 완성된다.

### 세 번째 엔딩

`RESTORE HOST`(호스트를 살리고 A:를 덮어씀)와 `EXEC ROGUE`(A:를 살리고 호스트를 닫음)는
그대로 두고, 여섯 조각을 모은 자만 고를 수 있는 세 번째를 추가한다. 원문 전체를 복구했다는
사실 자체가 조건이므로, 그 선택지는 **최종 볼륨을 클리어했을 때만 카드가 존재한다.**
두 기존 엔딩에 선악 표식을 붙이지 않는다는 `STORY_PLAN.md` 원칙은 유지하고, 세 번째 역시
"모두를 보존하는 정답"이 아니라 **다른 종류의 대가**를 치르는 결말로 쓴다.

## 최종 볼륨 `A:\ROGUE` 설계 초안

| 항목 | 내용 |
|---|---|
| 표기 | `A:\` / `ROGUE` |
| 경로 | `A:\` → `A:\ROGUE` → `A:\ROGUE\SELF` |
| 손상 2종 | 밸런스 단계에서 확정 (배드 섹터 + 읽기 오류가 유력) |
| 볼륨 특성 | 기존 `PERK_*` 재사용 |
| VOLUME LAW | `SELF-REFERENCE` — 층마다 앞선 볼륨의 법칙이 하나씩 켜진다 (아래 참고) |
| 일반 몹 3종 | `MOB_A_FALSE_COPY` · `MOB_A_HALF_WRITE` · `MOB_A_ECHO_PROC` |
| 층 보스 3종 | `BOSS_A_SIGNATURE` · `BOSS_A_SEVENTEENTH` · `BOSS_A_LAST_WRITE` |

### 기믹 패밀리를 새로 만들지 않는다

`GimmickFamily`는 여섯 볼륨 테마 × 3 = 18종이고, 패밀리마다 전용 발동 연출 함수가 있다
(`DrawLockShutter` · `DrawRestoreRewind` · `DrawDieRowSplit` · `DrawRoutingBus` ·
`DrawPressureAlloc` · `DrawQuarantineSeal`). 일곱 번째 패밀리를 만들면 연출도 새로 그려야 한다.

대신 최종 볼륨의 세 보스가 **서로 다른 기존 패밀리를 하나씩 빌려 쓰게** 한다. 기믹 종류
자체는 신규 3종이지만 패밀리는 재사용하므로 연출 코드가 전부 그대로 붙는다. 그리고 이건
"A:가 모든 볼륨에 자신을 복제했다"는 `X:\VAULT\02.LOG`의 사실과 정확히 맞아떨어진다.

- `BOSS_A_SIGNATURE` (1층) → `FAM_LOCK`: 서명을 요구하는 슬롯. 조건에 맞는 면을 놓지 않으면 출력 0
- `BOSS_A_SEVENTEENTH` (2층) → `FAM_RESTORE`: 플레이어의 직전 턴 최고 출력을 보스가 복제
- `BOSS_A_LAST_WRITE` (3층) → `FAM_QUARANTINE`: 카운트다운 후 슬롯 하나 영구 봉인, 피해로 지연

`SELF-REFERENCE` 법칙은 기존 법칙 코드가 `selectedDrive` 값으로 직접 분기하고 있어
(game.cpp:1648, 1657, 1665, 1682) 적용 대상을 `EffectiveLawDrive(game)` 하나로 뽑아내는
소규모 리팩터가 필요하다. 그만한 값어치가 없다고 판단되면 단순 법칙으로 대체한다.

## 코드 변경 계획

### 1단계 · 캠페인 진행도 (신규 `src/campaign.h` / `src/campaign.cpp`)

```c
struct CampaignState {
    uint32_t magic;          // 'AROG'
    uint16_t version;
    uint8_t  cleared[6];     // 볼륨별 클리어 여부
    uint8_t  finalCleared;   // 진엔딩 볼륨 완주
    uint8_t  endingSeen[3];
    uint32_t checksum;
};
```

**핵심 제약**: `NewRun`은 `ZeroMemory(game, sizeof(*game))`로 시작한다(game.cpp:856).
그러므로 캠페인 상태는 `GameState` 밖의 전역(`gCampaign`)에 두고, 규칙 계층이 읽어야 하는
부분만 **인자로 주입**한다.

```c
void NewRun(GameState* game, uint32_t seed, uint8_t clearedMask);
```

기존 호출자(`smoke.cpp` · `balance.cpp` · `curve.cpp`)는 `0`을 넘겨 현행 동작을 그대로
유지한다. **결정론적 스모크·밸런스가 이 개편으로 흔들리지 않게 하는 가장 중요한 장치다.**

세이브 파일은 실행 파일 옆 `AROGUE.SAV`. `LoadTranslations`가 쓰는
`GetModuleFileNameW` + 경로 치환 패턴을 그대로 따른다. 체크섬 불일치·매직 불일치·버전
불일치는 조용히 "새 캠페인"으로 처리하고 절대 크래시하지 않는다. 쓰기 실패도 무시한다
(읽기 전용 매체에서 실행될 수 있다).

### 2단계 · 볼륨 후보 산출

`PickDriveChoices`를 `clearedMask` 인지형으로 바꾸고 `driveChoiceCount`를 새 필드로 둔다.

남은 볼륨이 3개 미만이면 카드가 2장·1장으로 줄어 후반부에 난이도 선택의 재미가 사라진다.
**남은 볼륨이 3개 미만일 때는 같은 볼륨을 난이도만 다르게 3장 제시**해 선택의 축을
"어느 볼륨"에서 "몇 도로"로 넘긴다. 카드는 항상 3장이므로 레이아웃 작업도 사라진다.
조각 6/6이면 `A:\` 한 장만 제시한다 (이때만 카드 수가 1이며, 중앙 정렬 분기 하나가 필요).

`3`이 하드코딩된 지점 전부:

| 위치 | 내용 |
|---|---|
| game.cpp:898 | `SelectDrive`의 `choiceIndex >= 3` 범위 검사 |
| screens.cpp:1104 | `DrawDriveSelect`의 `for (i<3)` |
| screens.cpp:1094 | `DriveCardRect` — `left = 56 + i*344` 3장 고정 좌표 |
| screens.cpp:1206 | 선택 연출 `DrawDriveSelectionExit` |
| main.cpp:803 | `ClickDriveSelect` |
| main.cpp:1136 | `'1'`~`'3'` 키 처리 |

### 3단계 · 최종 볼륨 데이터

`DRIVE_COUNT`를 7로 올리고 **`DRIVE_SELECTABLE_COUNT 6`을 새로 추가**한다.
추첨·밸런스·커브는 `SELECTABLE`만 순회하고, 데이터 테이블은 7로 확장한다.

확장 대상: `DRIVE_INFO` · `DRIVE_LAW_INFO` · `DRIVE_MOBS` · `DRIVE_BOSSES` ·
`DIRECTORY_DRIVE_WEIGHT` · `STORY_BOSS_DATA` · `STORY_LOGS_DATA` ·
`SONG[6]`(music.cpp:30) 및 `MusicSetDrive`의 `d > 5 ? 5` clamp(music.cpp:148).

신규 적 6종 → `ENEMY_KIND_COUNT` 47 → 53. `sprites.h`에 16×16 도트 6개 추가.

**스모크의 단정문 세 개가 반드시 함께 바뀐다. 놓치면 빌드가 실패한다.**

| 위치 | 현재 | 변경 |
|---|---|---|
| smoke.cpp:482 | `enemy kind count must be 47` | 53 |
| smoke.cpp:516 | `active roster must reference exactly 36 kinds` | 42 |
| smoke.cpp:509 | `all 18 boss gimmicks must be distinct` | 21 |

### 4단계 · 스토리 재배선

- `AdvanceStory`(game.cpp:70)의 `STORY_BOSS && fragment==2 → TRUTH` 강제 전이를
  **최종 볼륨일 때만**으로 제한하고, 일반 볼륨은 신규 `STORY_SHARD`로 보낸다.
- 신규 `STORY_SHARD_DATA[6]` — 볼륨별 조각 확보 기록.
- 인트로를 3종으로 분기: 첫 부팅 / 이어하기(조각 n/6과 복구된 문장 일부) / 최종 볼륨 개방.
- `DrawEndScreen`(screens.cpp:2543)의 승리 분기를 **챕터 클리어**(조각 진행도, 남은 볼륨,
  `[계속]` → 볼륨 선택)와 **진엔딩**(현행 화면)으로 나눈다.
- `EndingChoiceRect`(screens.cpp:2314)를 카드 3장 레이아웃으로. 현재 `left = 124 + index*500`
  으로 2장 기준이라 폭 재계산이 필요하다.
- 게임오버 화면의 `새 런 시작`은 이제 "볼륨 선택으로 복귀"의 의미가 되므로 문구를 고친다.

### 5단계 · UI·설정

- 볼륨 선택 화면 상단에 `복구된 조각 ■■■□□□ 3/6` 진행 바와 클리어한 볼륨 스탬프.
- 타이틀 화면에 이어하기 상태 표시.
- 설정에 **진행도 초기화** 버튼 (기존 `새 런 시작`의 2단 확인 패턴 `gRestartArmed`를 그대로 따른다).

### 6단계 · 검증

- **smoke**: `clearedMask`별 후보 산출 불변식(클리어한 볼륨 미등장, 3개 미만일 때 난이도 3장,
  6/6일 때 `A:\` 단독), 여섯 볼륨 순차 클리어 캠페인 전 구간, 최종 볼륨 전체 런,
  세이브 왕복, 손상·구버전·없는 세이브의 안전한 폴백.
- **balance / curve**: `DRIVE_SELECTABLE_COUNT` 순회로 바꾸고 최종 볼륨은 별도 표본을 추가.
  최종 볼륨은 승률 하한을 조금 낮게 잡는다 (클라이맥스).
- **tools\check-fx.bat**: 드라이브 루프가 7개를 돌게.
- **translations.tsv**: 신규 문구(조각·챕터 클리어·세 번째 엔딩·최종 볼륨 스토리) 영어 행 전부 추가.
- 마지막에 1.44MB 한도 재확인.

## 작업 순서 (커밋 단위)

1. `CampaignState` + 세이브 파일 입출력 + `NewRun` 시그니처 확장 (규칙 변화 없음, 스모크 그대로 통과)
2. 볼륨 후보 산출과 `driveChoiceCount` — 클리어한 볼륨 제외까지. 아직 7번째 볼륨 없음
3. 스토리 재배선 — 조각 6종, 챕터 클리어 화면, 인트로 분기. 6볼륨 완주 시 임시 종료 화면
4. 최종 볼륨 데이터 — 적 6종·스프라이트·기믹 3종·BGM·경로·법칙
5. 진엔딩 — `STORY_TRUTH` 재배치, 3지선다, 세 번째 엔딩 텍스트와 화면
6. UI 마감 — 진행도 표시, 설정의 초기화, 번역 행
7. 밸런스 조정 — 최종 볼륨 난수 표본으로 체력·피해·기믹 매개변수 확정

## 리스크

가장 큰 위험은 **조용한 회귀**다. 스모크·밸런스·FX QA가 전부 `DRIVE_COUNT` 순회와
"6드라이브 × 3층" 전제로 짜여 있어, 상수만 7로 올리면 검사는 통과하는데 최종 볼륨이
실제로는 검증되지 않는 상태가 만들어진다. 1단계의 `clearedMask` 인자 주입과
`DRIVE_SELECTABLE_COUNT` 분리가 이걸 막는 장치다.

두 번째는 **배열 초기화 부족**이다. C++에서 `DRIVE_INFO`를 7로 늘리고 원소를 6개만 쓰면
컴파일러가 잡지 않고 7번째가 0으로 채워진다. 3단계에서 확장 대상 테이블을 전수 확인하고,
스모크에 "모든 드라이브의 letter·label·paths·법칙이 비어 있지 않다"는 단정을 추가한다.

세 번째는 **캠페인 길이**다. 여섯 볼륨을 다 깨야 진엔딩이 열리므로 완주 시간이 여섯 배가
된다. 사망해도 진행도가 남는 결정으로 완화되지만, 후반부에 광기(200%) 난이도만 남는
상황이 생기지 않도록 2단계의 "난이도만 다른 3장" 규칙이 실제로 다섯 등급을 고르게
뿌리는지 밸런스 단계에서 확인한다.
