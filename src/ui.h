#pragma once

#include <windows.h>
#include "game.h"
#include "render.h"
#include "fx_timing.h"

// 화면(screens.cpp)과 입력·창 관리(main.cpp)가 함께 쓰는 상태와 레이아웃.
// 좌표는 전부 render.h의 고정 캔버스(BASE_WIDTH x BASE_HEIGHT) 기준이다.

#define SETTINGS_SCALE_COUNT 5
static const int SCALE_OPTIONS[SETTINGS_SCALE_COUNT] = {75, 100, 125, 150, 200};

// 소리 크기 슬라이더. 손잡이 너비만큼 이동 구간이 줄어들므로 값↔좌표 변환을
// 한 곳에 모아 두고 그리기와 클릭 판정이 같은 식을 보게 한다.
#define VOL_HANDLE_W 16

// 카드 0~2는 설치할 면, 마지막 카드는 면 대신 체력을 얻는 섹터 복구다.
#define REWARD_CARD_COUNT 4
#define REWARD_REPAIR 3

#define COMBAT_CLEAR_MS 1900
#define NOISE_CHURN_MS 45      // 노이즈가 다시 섞이는 주기

// ---- 디렉터리 진입 연출 ----------------------------------------------------
// 예전에는 고른 카드가 잠기고(0.36초) 패널 한 장이 열려 경로 한 줄이 타이핑되면
// 끝이었다(1.46초). 층마다 두 번, 런에서 여섯 번 있는 "어디로 들어갈 것인가"의
// 대답이 글자 한 줄이었다는 뜻이다 - 진입한다고 적혀 있을 뿐 아무 데도 가지
// 않았다. 이제 판이 부모 디렉터리의 목록이 되고, 헤드가 고른 줄을 찾아 앉고,
// 그 줄이 문처럼 열리고, 시점이 그 문 안으로 파고들어 경로를 한 겹씩 지나친다.
//
//   잠금  고른 카드가 경로로 잠기고 탈락한 카드가 닫힌다
//   탐색  판이 목록으로 갈리고 헤드가 고른 줄로 내려앉는다
//   개방  걸쇠가 풀리고 그 줄이 문처럼 열린다
//   통과  문과 겹 셋을 지난다. 겹 하나가 경로 조각 하나다
//   안착  도착한 디렉터리의 이름·효과·비용·대상이 한 줄씩 선다
//   확정  작업 디렉터리가 박히고 전투판이 가운데부터 드러난다
//
// 그리기(screens.cpp)와 소리(main.cpp)가 같은 경계를 봐야 한다. 통과 구간은
// 문까지 넷으로 나뉘므로(DIR_TUNNEL_DEPTH + 1) 겹을 지나치는 순간과 그 소리가
// 같은 식에서 나온다.
//
// 길이는 읽는 시간이 정한다 (마운트가 쓰는 규칙과 같다). 처음 짰을 때는 2.36초
// 였는데, 목록이 다 찍히자마자 문이 열리고 겹의 이름이 0.14초 만에 지나가고
// 도착한 줄이 다 앉기도 전에 판이 걷혔다 - 글자를 세워 놓고 읽을 틈을 주지
// 않았다는 뜻이다. 그래서 세 구간에 읽는 시간을 세워 뒀다.
//
//   탐색  목록이 다 찍히고(0.28초) 헤드가 앉은 뒤(DIR_SEEK_TRAVEL_MS) 고른 줄이 선 채로 멎는다
//   통과  겹 하나에 0.20초. 한 겹의 이름이 읽을 만한 크기로 머무는 시간이 그만큼 길어진다
//   안착  줄이 0.09초 간격으로 빨리 앉고(0.39초에 끝난다) 나머지는 통째로 읽는 시간이다
//
// 전체 3.48초로 마운트(5.4초)보다 짧고, 어느 시점에나 클릭·아무 키로 건너뛸 수 있다.
#define DIR_LOCK_MS  360       // 고른 디렉터리 카드가 경로로 수렴하는 구간
#define DIR_SEEK_MS  700
#define DIR_SEEK_TRAVEL_MS 360 // 헤드가 목록을 훑는 시간. 나머지가 고른 줄을 읽는 시간이다
#define DIR_OPEN_MS  300
#define DIR_DIVE_MS  780
#define DIR_LAND_MS 1080
#define DIR_SEAL_MS  260
#define DIR_SEEK_AT  DIR_LOCK_MS
#define DIR_OPEN_AT  (DIR_SEEK_AT + DIR_SEEK_MS)
#define DIR_DIVE_AT  (DIR_OPEN_AT + DIR_OPEN_MS)
#define DIR_LAND_AT  (DIR_DIVE_AT + DIR_DIVE_MS)
#define DIR_SEAL_AT  (DIR_LAND_AT + DIR_LAND_MS)
#define DIR_ENTER_MS (DIR_SEAL_AT + DIR_SEAL_MS)
#define DIR_TUNNEL_DEPTH 3     // 문 안쪽에 서는 겹. 볼륨 · 상위 조각 · 고른 노드
#define DIR_DIVE_STEP_MS (DIR_DIVE_MS / (DIR_TUNNEL_DEPTH + 1))

// ---- 볼륨 마운트 연출 ------------------------------------------------------
// 예전에는 고른 카드가 잠기고 나면 패널 한 장이 열려 진행 막대를 채웠다. 런에서
// 가장 큰 결정 - 어느 볼륨으로 들어갈 것인가 - 의 대답이 막대 하나였다는 뜻이다.
// 이제 고른 볼륨은 실제로 물건이 된다. 나머지 카드가 판 밖으로 뜯겨 나가고, 남은
// 한 장이 뽑혀 나와 스핀들 위에 눕고, 회전수에 오른 그 원판을 액추에이터가 세
// 트랙에 걸쳐 읽는다. 읽히는 것은 새로 만든 값이 아니라 방금 카드에 적혀 있던
// 것들이다 - 손상 섹터 둘과 볼륨 법칙이 판 위의 자리로 나온다.
//
//   잠금   고른 카드만 남고 나머지는 좌우로 뜯겨 나간다
//   안착   남은 카드가 뽑혀 나와 화면 한가운데에서 눕는다 (원판이 된다)
//   기동   스핀들이 회전수에 오르고 트랙 고리가 열린다
//   판독   헤드가 바깥→가운데→안쪽 세 트랙을 읽는다. 손상 섹터가 여기서 드러난다
//   각인   다 읽은 판 한가운데에 볼륨 법칙이 박힌다
//   마운트 걸쇠가 물리고 카메라가 허브 속으로 들어가며 판이 화면을 삼킨다
//
// 구간 경계는 그리기(screens.cpp)와 소리(main.cpp)가 같은 값을 봐야 한다 -
// 헤드가 트랙에 닿는 순간과 그 소리가 어긋나면 기계가 아니라 애니메이션이 된다.
// 길이는 읽는 시간이 정한다. 한 트랙을 다 읽을 때마다 딱지가 한 장 붙는데,
// 다음 딱지가 붙기 전에 그 줄을 읽을 수 있어야 한다. 620ms가 한글 두 줄을
// 읽는 최소치였고, 각인 뒤에 620ms를 더 세워 둬 판 전체가 한 번 정지한 채
// 읽힌다. 전체가 5초를 넘지만 클릭·아무 키로 언제든 건너뛸 수 있고, 한 런에
// 한 번 나오는 장면이다 - 층 하강(DIVE)은 이 절반도 안 된다.
#define MOUNT_LOCK_MS  560     // 고른 카드가 잠기고 나머지가 뜯겨 나간다
#define MOUNT_SEAT_MS  400     // 카드가 뽑혀 나와 매체 자리에 눕는다
#define MOUNT_SPIN_MS  700     // 매체가 깨어난다 (판은 회전수에, 격자는 전원에)
#define MOUNT_READ_MS 1860     // 세 트랙 판독. 트랙마다 620ms
#define MOUNT_LAW_MS   560     // 볼륨 법칙 각인
#define MOUNT_HOLD_MS  900     // 다 읽은 매체가 멈춰 선다. 여기서 판 전체가 읽힌다
#define MOUNT_SEAL_MS  460     // 걸쇠 + 허브 속으로
#define MOUNT_SEAT_AT  MOUNT_LOCK_MS
#define MOUNT_SPIN_AT  (MOUNT_SEAT_AT + MOUNT_SEAT_MS)
#define MOUNT_READ_AT  (MOUNT_SPIN_AT + MOUNT_SPIN_MS)
#define MOUNT_LAW_AT   (MOUNT_READ_AT + MOUNT_READ_MS)
#define MOUNT_HOLD_AT  (MOUNT_LAW_AT + MOUNT_LAW_MS)
#define MOUNT_SEAL_AT  (MOUNT_HOLD_AT + MOUNT_HOLD_MS)
#define MOUNT_MS       (MOUNT_SEAL_AT + MOUNT_SEAL_MS)
#define MOUNT_TRACKS   3       // 바깥·가운데·안쪽
#define MOUNT_TRACK_MS (MOUNT_READ_MS / MOUNT_TRACKS)
#define MOUNT_SETTLE_MS 150    // 한 트랙에 옮겨 앉는 시간. 나머지가 한 바퀴 판독이다

// 층 하강은 같은 판에서 일어난다. 판은 이미 물려 돌고 있으므로 잠금도 기동도
// 없고, 헤드가 지금 트랙에서 한 칸 더 안쪽으로 파고드는 것만 남는다. 한 런에
// 두 번 더 나오는 장면이라 마운트보다 짧아야 한다.
#define DIVE_IN_MS    320      // 돌고 있는 매체로 카메라가 들어온다
#define DIVE_READ_MS  900      // 안쪽 트랙으로 파고들어 읽는다
#define DIVE_HOLD_MS  420      // 바뀐 용량 한도가 읽히는 시간
#define DIVE_SEAL_MS  420
#define DIVE_READ_AT  DIVE_IN_MS
#define DIVE_HOLD_AT  (DIVE_READ_AT + DIVE_READ_MS)
#define DIVE_SEAL_AT  (DIVE_HOLD_AT + DIVE_HOLD_MS)
#define DIVE_MS       (DIVE_SEAL_AT + DIVE_SEAL_MS)

// 마운트(볼륨 선택)와 하강(층 이동)은 같은 함수가 그리고 같은 타이머가 운다.
// 구간 이름이 같은 자리를 가리키도록 여기서 한 번에 풀어 준다 - 그림·소리·
// 타이머가 이 표 하나만 보므로 길이를 바꿔도 셋이 같이 움직인다.
struct MountBeats { int seatAt, spinAt, readAt, lawAt, holdAt, sealAt, total, tracks; };
inline MountBeats MountBeatsFor(int mount) {
    MountBeats b;
    if (mount) {
        b.seatAt = MOUNT_SEAT_AT; b.spinAt = MOUNT_SPIN_AT; b.readAt = MOUNT_READ_AT;
        b.lawAt = MOUNT_LAW_AT; b.holdAt = MOUNT_HOLD_AT; b.sealAt = MOUNT_SEAL_AT;
        b.total = MOUNT_MS; b.tracks = MOUNT_TRACKS;
    } else {
        // 하강에는 각인이 없다. 법칙은 마운트에서 이미 박혔고 여기서는 트랙만 옮긴다.
        // lawAt은 그래서 "판독이 끝나는 자리"라는 뜻만 남는다.
        b.seatAt = 0; b.spinAt = 0; b.readAt = DIVE_READ_AT;
        b.lawAt = DIVE_HOLD_AT; b.holdAt = DIVE_HOLD_AT; b.sealAt = DIVE_SEAL_AT;
        b.total = DIVE_MS; b.tracks = 1;
    }
    return b;
}
// 한 트랙에 주어지는 시간. 앞의 MOUNT_SETTLE_MS가 옮겨 앉기, 나머지가 판독이다.
inline int MountTrackMs(const MountBeats& b) { return (b.lawAt - b.readAt) / (b.tracks > 0 ? b.tracks : 1); }

// ---- 볼륨 매체 -------------------------------------------------------------
// 일곱 볼륨이 전부 같은 원판이면 어느 볼륨에 들어왔는지가 색으로만 남는다.
// 볼륨마다 실제로 다른 물건이 돌아야 한다 - 데이터에 이미 적혀 있는 성격이
// 그대로 물건의 형태가 된다.
//
//   C:\ SYSTEM      플래터 3장 스택   전원부가 안정적인 기본형. 피벗 암이 읽는다
//   D:\ ARCHIVE     오픈릴 테이프     넓지만 느린 창고. 릴 둘 사이를 띠가 지나간다
//   E:\ REMOVABLE   광 디스크         직선 레일 위의 슬레드. 접촉이 끊겨 판독이 튄다
//   N:\ NETWORK     원격 링크         실체가 없다. 끊긴 고리를 패킷이 채운다
//   R:\ RAMDISK     셀 격자           도는 것이 없다. 행을 훑고 읽은 자리는 증발한다
//   X:\ QUARANTINE  격리 캐비닛       우리에 물린 판. 잠금쇠를 풀어야 돈다
//   A:\ ROGUE       플로피 한 장      복구 도구 자신의 기록. 헤드가 양쪽에서 둘이다
//
// 그리기(screens.cpp)와 소리(main.cpp)가 이 표를 같이 본다 - 눈에 보이는 물건과
// 귀에 들리는 소리가 어긋나면 형태를 갈라 둔 보람이 없다.
enum MediaKind { MEDIA_STACK, MEDIA_TAPE, MEDIA_OPTICAL, MEDIA_LINK, MEDIA_CELL, MEDIA_CAGE, MEDIA_FLOPPY };
inline int DriveMedia(int drive) {
    static const unsigned char TABLE[DRIVE_COUNT] = {
        MEDIA_STACK, MEDIA_TAPE, MEDIA_OPTICAL, MEDIA_LINK, MEDIA_CELL, MEDIA_CAGE, MEDIA_FLOPPY};
    return TABLE[drive < 0 || drive >= DRIVE_COUNT ? 0 : drive];
}
// 트랙이 고리인가 (아니면 가로줄인가). 테이프와 셀 격자만 줄이다.
inline int MediaIsDisc(int media) { return media != MEDIA_TAPE && media != MEDIA_CELL; }
// 돌아가는 축이 있는가. 없으면 스핀들 소리도 쓰지 않는다.
inline int MediaSpindle(int media) { return media != MEDIA_CELL && media != MEDIA_LINK; }
inline const wchar_t* MediaName(int media) {
    static const wchar_t* const NAMES[] = {L"플래터 스택", L"오픈릴 테이프", L"광 디스크",
                                           L"원격 링크", L"셀 격자", L"격리 캐비닛", L"플로피"};
    return NAMES[media < 0 || media > MEDIA_FLOPPY ? 0 : media];
}

// 연출 타이머의 주기. 60fps를 노리고 16으로 두면 안 된다 - WM_TIMER는 시스템
// 틱(기본 15.6ms) 경계에서만 깨어나므로, 16ms짜리는 매번 다음 틱까지 밀려 두
// 틱에 한 번씩 온다. 실제로 재 보니 삽입 연출이 3.5초에 131프레임(37fps)이었고,
// 같은 연출을 8ms로 걸었더니 234프레임(66fps)이 나왔다. 틱마다 한 번 깨어나되
// 실제 속도는 그리기가 정한다 (WM_TIMER는 밀린 만큼 쌓이지 않는다).
#define FX_TIMER_MS 8

// 새 게임 삽입 · 볼륨 마운트 · 층 하강이 흐르는 속도(100 = 설계한 길이 그대로).
// 구간 길이를 하나씩 늘리면 그 안에 박힌 짧은 오프셋(섬광 120ms, 큐 +240ms 등)이
// 제자리에 남아 구간 끝에 빈 정지 화면이 생긴다. 그래서 시계를 늦춘다 - 그림·소리·
// 타이머가 모두 이 함수로 경과 시간을 읽으므로 셋이 같은 비율로 느려진다.
#define SCENE_PACE_PCT 130
constexpr inline int ScenePace(int realMs) { return realMs * 100 / SCENE_PACE_PCT; }

// ---- 타이틀 진입 연출 ------------------------------------------------------
// 전원이 들어오고 18개 섹터가 타 들어가면 그 불이 지나간 자리에서 서명 → 제목 →
// 안내 → 버튼 → 진행도가 차례로 드러난다. 구간 경계는 그리기(screens.cpp)와
// 소리(main.cpp)가 같은 값을 봐야 하므로 여기 모아 둔다 - 제목이 앉는 순간과
// 그 소리가 어긋나면 조립되는 것이 아니라 느리게 그려지는 것으로 읽힌다.
#define TITLE_SIGN_AT   110    // 부팅 서명이 찍힌다
#define TITLE_LOGO_AT   170    // 제목이 판에서 밀려 올라온다
#define TITLE_BLURB_AT  620    // 안내 세 줄이 한 줄씩 찍힌다
#define TITLE_START_AT  900    // 시작 버튼이 열린다
#define TITLE_SHARD_AT  1090   // 복구 진행도와 조각 칸이 선다
#define TITLE_HINT_AT   1280   // 맨 아래 조작 안내
#define TITLE_SETTLE_AT 1560   // 여기부터는 헤드가 계속 판을 읽는 유휴 상태다

// ---- 새 게임 삽입 연출 -----------------------------------------------------
// 새 게임을 누르면 지금 화면이 먼저 갈라지고, 소용돌이에 감겨 빨려 들어가 플로피
// 한 장이 되고, 그 디스크가 공중에서 한 바퀴 뒤집힌 뒤 책상 위 컴퓨터의 3.5인치
// 드라이브에 꽂힌다. 구간 경계는 그리기와 소리·흔들림이 같은 값을 봐야 하므로
// 여기 모아 둔다.
#define BOOT_GLITCH_MS 460     // 판이 띠로 어긋나고 고리가 조여 온다
#define BOOT_SUCK_MS   860     // 화면이 세 바퀴 돌며 디스크 라벨로 빨려 들어간다
#define BOOT_FLIP_MS   470     // 클로즈업 안에서 한 바퀴 뒤집힌다
#define BOOT_FLY_MS    400     // 카메라가 물러나며 책상·기계·바닥이 드러난다
#define BOOT_PUSH_MS   380     // 슬롯 안으로 밀려 들어간다 (중간에 한 번 걸린다)
#define BOOT_SEEK_MS   660     // 드라이브가 읽는다 (점등·헤드 이동·신호 전송)
#define BOOT_ZOOM_MS   520     // 기계 전체가 덮쳐 오고 그 화면 속으로 들어간다
// 붕괴 직전의 예비 동작. 여기서부터 화면이 과전압으로 하얗게 부풀었다가 찢어진다.
// 무너지는 구간 안에 무너지기 전이 있어야 첫 소리가 허공에서 나지 않는다.
#define BOOT_SURGE_AT  (BOOT_GLITCH_MS - 190)
// 디스크가 물린 뒤 브라운관이 켜지는 시간. 가로 한 줄이 세로로 열린다.
#define BOOT_POWER_MS  300
#define BOOT_SUCK_AT   BOOT_GLITCH_MS
#define BOOT_FLIP_AT   (BOOT_SUCK_AT + BOOT_SUCK_MS)   // 디스크 한 장이 완성되는 순간
#define BOOT_FLY_AT    (BOOT_FLIP_AT + BOOT_FLIP_MS)
#define BOOT_PUSH_AT   (BOOT_FLY_AT + BOOT_FLY_MS)
#define BOOT_CLUNK_AT  (BOOT_PUSH_AT + BOOT_PUSH_MS)   // 다 들어가 철컥 물리는 순간
#define BOOT_SEEK_END  (BOOT_CLUNK_AT + BOOT_SEEK_MS)
#define BOOT_INSERT_MS (BOOT_SEEK_END + BOOT_ZOOM_MS)

// ---- 보스 조우 연출 --------------------------------------------------------
// 일반전 앞에는 디렉터리 2택과 진입 연출이 있지만 보스 구역에는 둘 다 없다.
// 두 번째 일반전의 보상을 고르면 판이 곧장 보스전으로 갈렸다 - 이 게임에서
// 가장 큰 사건이 카드 한 장 바뀌는 것으로 끝났다는 뜻이다. 디렉터리 화면이
// 내내 "LOCKED DESTINATION ...\<BOSS>"로 가리켜 온 그 자리를 여기서 연다.
//   경보   잠긴 목적지의 마지막 조각이 실제 코드로 풀린다
//   게이트 그 경로를 막고 있던 철문이 좌우로 갈라진다
//   강림   열린 틈에서 보스가 걸어 나와 바닥을 딛는다 (충격파·흔들림)
//   명패   코드·수치·기믹이 박히고 명패가 걷히며 전투판이 열린다
// 구간 경계는 그리기와 소리·흔들림이 같은 값을 봐야 하므로 여기 모아 둔다.
#define BOSS_ALERT_MS  680     // 경보가 올라오고 목적지가 판독된다
#define BOSS_GATE_MS   760     // 잠금이 풀리고 문짝이 갈라진다
#define BOSS_RISE_MS   860     // 보스가 앞으로 나와 바닥을 딛는다
#define BOSS_NAME_MS   800     // 명패가 박히고 기믹 도장이 찍힌다
#define BOSS_HAND_MS   420     // 명패가 좌우로 걷히며 전투판을 내보낸다
#define BOSS_GATE_AT   BOSS_ALERT_MS
#define BOSS_RISE_AT   (BOSS_GATE_AT + BOSS_GATE_MS)
#define BOSS_LAND_AT   (BOSS_RISE_AT + 520)            // 발이 바닥에 닿는 순간
#define BOSS_NAME_AT   (BOSS_RISE_AT + BOSS_RISE_MS)
#define BOSS_HAND_AT   (BOSS_NAME_AT + BOSS_NAME_MS)
#define BOSS_INTRO_MS  (BOSS_HAND_AT + BOSS_HAND_MS)

// ---- 피격·위독·정지 연출 --------------------------------------------------
#define CRITICAL_HP 10         // 이 체력 이하부터 화면이 노이즈에 잠식된다
#define STRIKE_MS 440          // 적이 달려들었다가 제자리로 돌아오는 시간
#define STRIKE_POP_MS 720      // 피해 숫자가 떠오르다 사라지는 시간
#define PLAYER_HIT_MS 420      // 피격 테두리 섬광
#define SHAKE_MS 300           // 화면 흔들림

// ---- 사망 연출 (DEATH-01) ---------------------------------------------------
// 화면 전체를 잡음으로 덮지 않는다. 이번 런의 기억 열 줄이 한 줄씩 오염되어
// 부서지고, 마지막으로 실행체의 이름이 부서지는 순간 화면이 한 번 꺼진다.
// 어둠 속에서 십칠의 말이 찍히고 나면 그 마지막 프레임이 곧 사망 화면이다.
// 구간 경계는 그리기와 소리·흔들림이 같은 값을 봐야 하므로 여기 모아 둔다.
#define DEATH_LINES          10
#define DEATH_CMD_AT         700    // 십칠이 명령을 친다 (0~1000에는 기억이 들어온다)
#define DEATH_ROT_AT         1400   // 첫 줄의 오염이 시작된다
#define DEATH_SPREAD_MS      360    // 한 줄에서 오염이 양끝까지 번지는 최대 시간
#define DEATH_ROT_HOLD_MS    200    // 오염된 글자가 쪼개지기까지
#define DEATH_FALL_MS        420    // 쪼개진 조각이 떨어져 사라지기까지
#define DEATH_LAST_GAP_MS    350    // 마지막 줄 앞에서만 쉰다
#define DEATH_NAME_AT        4500   // 실행체 이름 가운데부터 오염이 번진다
#define DEATH_NAME_CRACK_AT  4640   // 이름에 금이 간다
#define DEATH_NAME_SPLIT_AT  4800   // 이름이 위아래로 갈라진다
#define DEATH_NAME_BREAK_AT  4960   // 오염된 글자부터 부서진다
#define DEATH_CUT_AT         5300   // 조각이 공중에 떠 있을 때 화면이 끊긴다
#define DEATH_DARK_AT        5800   // 어둠 속에서 대사가 찍힌다 (여기서부터 건너뛸 수 있다)
#define DEATH_MS             7800

// ---- 연출 강도 -------------------------------------------------------------
// 줄어드는 것은 흔들림·파편·전역 글리치 같은 장식뿐이다. 슬롯 잠금과 다음 잠금
// 대상, 오프라인 주사위, 격리·삭제 대상 면, 해결 순서와 역전 예고, 압력 게이지,
// HP 잔상과 실제 피해 숫자는 어떤 모드에서도 숨기지 않는다.
enum FxLevel { FX_FULL = 0, FX_REDUCED, FX_OFF, FX_LEVEL_COUNT };
extern int gFxLevel;
int FxScale(int amount);   // 장식 강도를 현재 모드로 줄인다 (REDUCED 50%, OFF 0)
int FxDecorOn();           // 움직이는 장식을 그려도 되는가

// ---- 직접 조작 피드백 ------------------------------------------------------
// 클릭 결과는 게임 상태에 즉시 반영하고, 그 직전/직후 위치만 짧게 기록해 화면이
// 경과 시간의 순수 함수로 재생한다. 보상은 처리 직후 화면이 바뀌므로 마지막 보상
// 화면 스냅샷을 잠깐 유지하고, 주사위 배치·정리는 현재 화면 위에서 바로 재생한다.
enum UiFxKind {
    UIFX_NONE = 0,
    UIFX_DIE_PLACE,
    UIFX_DIE_MOVE,
    UIFX_DIE_REMOVE,
    UIFX_REWARD_FACE,
    UIFX_REWARD_TSR,
    UIFX_REWARD_REPAIR,
    UIFX_PRUNE_DELETE,
    UIFX_PRUNE_RESTORE
};

struct UiFxState {
    int kind;
    DWORD start;
    int die, face;
    int displacedDie;
    int fromSlot, toSlot;
    int rewardIndex;
    int valueBefore, valueAfter;
    Face shownFace;
};

extern UiFxState gUiFx;
int UiFxElapsed();
int UiFxSnapshotActive();
void CaptureUiFxSnapshot();
void DrawUiInteractionFx(HDC dc);

// ---- 공유 상태 (main.cpp가 소유한다) --------------------------------------
extern GameState gGame;
extern HWND gWindow;
extern POINT gMouse;
extern int gGuideOpen, gSettingsOpen, gDeckOpen, gFullscreen;
extern uint8_t gPruneTsrPending[TSR_COUNT];
// 가이드는 주제별 다섯 쪽이다. 앞의 넷은 공통 규칙, 마지막 쪽은 마운트한 드라이브의 로스터.
#define GUIDE_PAGE_COUNT 5
#define GUIDE_PAGE_DRIVE (GUIDE_PAGE_COUNT - 1)
extern int gGuidePage;   // 0 시작하기 · 1 슬롯과 면 · 2 적과 위험 · 3 덱·보상·조작 · 4 드라이브 정보
extern int gRestartArmed; // 설정 화면의 "다시 시작" 버튼: 0=대기, 1=한 번 더 누르면 확정
extern int gCampaignResetArmed; // "진행도 초기화" 버튼. 런이 아니라 세이브를 지우므로 확정을 따로 받는다
// 보상 포기 버튼. 되돌릴 수 없는 손실이라 "다시 시작"과 같은 두 번 누르기를 쓴다.
// 보상 화면을 벗어나거나 카드를 새로 고르면 0으로 풀린다.
extern int gRewardSkipArmed;
// 마지막 캠페인 세이브가 실패했으면 1. 한 번 서면 그 실행 동안 유지되고,
// 다음 저장이 성공하면 다시 0으로 내려간다.
extern int gSaveFailed;
extern int gSettingsSaveFailed;
extern int gCampaignCorrupt;
int CampaignBestFloor(int drive);

// 되돌릴 수 없는 선택을 고르는 단계와 확정하는 단계로 나눈다. 각 값은 아직
// 확정하지 않은 후보의 번호이고, -1은 "고른 것 없음"이다. 첫 입력은 후보를
// 세우고 카드에 확정 문구를 띄우기만 하며, 같은 후보에 한 번 더 와야 실제로
// 적용된다. 다른 후보를 누르면 그쪽으로 옮겨 가고 취소 키로 풀린다.
extern int gDirectoryArmed;   // 디렉터리 카드
extern int gTsrArmed;         // 보스 전리품 카드
extern int gFaceSwapArmed;    // 보상 면을 덮어쓸 기존 면 (die * 6 + face)
extern int gEndingArmed;      // 최종 명령 카드

// 주사위 판독 연출
extern int gReadActive, gRolled;
// 전투 종료·턴 계산·볼륨 진입 연출
extern int gCombatClearActive, gClearedFloor, gClearedEncounter;
extern DWORD gCombatClearStart;
extern int gTurnTraceActive;
extern DWORD gTurnTraceStart;
extern int gDescentActive, gDescentToFloor;
extern DWORD gDescentStart;
extern int gDescentChoiceIndex; // 최초 마운트 때 고른 카드 (층 하강이면 -1)
// 디렉터리 진입: 고른 경로 조각이 타이핑되는 짧은 오버레이
extern int gDirEnterActive, gDirEnterKind, gDirEnterChoiceIndex;
extern DWORD gDirEnterStart;
// 보스 조우: 잠긴 목적지가 열리고 보스가 걸어 나오는 동안. 판은 이미 보스전
// 상태다 (StartCombat이 먼저 끝나 있다) - 그래서 언제 건너뛰어도 결과가 같다.
extern int gBossIntroActive;
extern DWORD gBossIntroStart;
// 새 게임: 화면이 디스크로 빨려 들어가 드라이브에 꽂힐 때까지. 이 연출이 도는
// 동안 판은 아직 누르기 직전 그대로다 (런은 연출이 끝날 때 만들어진다).
extern int gBootActive;
extern DWORD gBootStart;
// 체력 0 이후의 사망 연출. 끝나면 그 마지막 프레임이 사망 화면으로 남는다.
extern int gDeathActive;
extern DWORD gDeathStart;
int DeathElapsed();          // 사망 연출 경과 ms (연출이 끝났으면 DEATH_MS)
int DeathLineAt(int line);   // 기억 한 줄의 오염이 시작되는 ms. 줄 간격이 가속한다
void DrawDeathScene(HDC dc, int width, int height, int t);

// ---- 연출 질의 (main.cpp가 계산하고 화면이 읽는다) ------------------------
int DieNoise(int die);
int DieSettled(int die);
int DieSettleFlash(int die);
int NoiseStep(int die);
int EnemyBob(int index);
void SyncIdleAnimation();
int SceneElapsed(); // visible phase/page entrance, presentation state only
// 이번 도착이 화면 자체가 바뀐 것인가. 0이면 같은 화면에서 쪽만 넘긴 것이라
// 도착 연출이 브라운관을 다시 열지 않는다.
int SceneArrivalMajor();
int UiFocusElapsed(); // ms on the current actionable hover target, -1 when absent
// 가이드 2페이지에 아직 미판독 칸이 남아 있는가. 남아 있으면 가이드가 열려 있는
// 동안에도 리페인트를 계속 돌려야 노이즈가 멈추지 않는다.
int GuideNoiseActive();

// 계산 재생에서 지금까지 드러난 줄 수 (0 = 아직 없음)
int TurnTraceShown();
const DieState* DisplayDie(int index); // 재생 중에는 실행 직전의 주사위·배치를 보존
const EnemyState* DisplayEnemyAction(int index); // 실행한 의도와 카운터를 재생 끝까지 보존
int DisplayTurn();

// ---- 기믹 발동 연출 --------------------------------------------------------
// 계산 재생이 끝나 새 턴 화면이 드러나는 순간 시작된다. 규칙은 이미 game.cpp에서
// 확정된 뒤이므로 여기서는 보여 주는 방식만 정한다.
// C:\ 3층 파쇄에서 초상이 칸 윗변을 누르는 순간. 세 시간표와 연출, 소리 박자가
// 모두 이 값 하나를 본다.
#define SHRED_IMPACT 1000
int GimmickFxKind();        // 재생 중인 기믹 (GIMMICK_NONE = 없음)
int GimmickFxElapsed();     // 시작으로부터 경과 ms (히트스톱 동안은 멈춰 있다)
int GimmickFxRawElapsed();  // 히트스톱을 풀지 않은 실제 경과 ms. 멈춘 동안에도 도는 빛에 쓴다
int GimmickFxA();           // 대상 1 (슬롯·주사위·면 또는 수치)
int GimmickFxB();           // 대상 2
int GimmickFxDuration(int kind, int b);   // 그 기믹 연출의 총 길이 ms
int GimmickFxImpactAt(int kind, int b);   // 히트스톱이 걸리는 시점 ms (0 = 없음)
void DrawGimmickFx(HDC dc);
// 철문이 아직 안 내려왔으면 1. 잠금 표시를 그때까지 미루는 데 쓴다.
int GimmickLockPending(int slot);
// 소환된 카드가 아직 격리막 안에 있으면 1. 계산 재생 중과 연출의 임팩트 전까지는
// 카드를 그리지도, 클릭하지도, 세지도 않는다. 규칙은 이미 소환을 끝냈지만 화면에서는
// 막이 깨지는 순간에 나타나야 연출이 사건이 된다.
int GimmickSummonPending(int enemyIndex);

// ---- 전투 시각 이벤트 재생 -------------------------------------------------
// game.cpp가 남긴 CombatFxEvent를 계산 줄 번호에 맞춰 되짚는다.
//   eventStart = gTurnTraceStart + FxTraceAt(gGame, traceLine)
// 화면은 CombatFxElapsed만 읽어 모든 위치·강도를 경과 시간의 순수 함수로 낸다.
int CombatFxPlaying();          // 지금 이벤트가 흐르고 있는가
int CombatFxElapsed(int index); // 그 이벤트 시작 이후 ms (아직 안 왔으면 -1)
int CombatFxLeadElapsed(int index, int leadMs); // 결과 전 신호 이동만 미리 재생
// 재생 중 화면에 보일 내 체력. 아직 닿지 않은 타격의 결과를 미리 보여 주지 않는다.
int PlayerDisplayHp();
// 소리와 적의 달려들기를 그 사건의 줄에 맞춰 한 번씩 발동한다.
void SyncCombatFx();
int EnemyStrikeDrop(int index);     // 플레이어 쪽(아래)으로 파고드는 픽셀
int EnemyStrikeShift(int index);    // 달려들 때의 좌우 흔들림
int EnemyStrikePop(int index);      // 피해 숫자 표시 강도 1000 → 0
int EnemyStrikeDamage(int index);   // 그때 들어온 피해 (0 = 방어도가 전부 막음)

// 피격 반응: 화면 흔들림과 테두리 섬광
int PlayerHitFlash();
int PlayerHitBlocked();
int ScreenShakeX();
int ScreenShakeY();

// 화면 노이즈. 살아 있는 동안에는 항상 가장자리에만 머문다.
//   체력 2~CRITICAL_HP : 체력이 줄수록 띠가 두꺼워지고 짙어진다
//   체력 1             : 버티는 시간만큼 띠가 더 두꺼워지고 짙어진다 (중앙은 그대로)
//   체력 0             : 띠는 걷히고 사망 연출이 글자 단위로 이어받는다
//
// 세기는 세 갈래로 나눠 넘긴다. 하나로 합치면 "언제나 같은 세기로 지직거리는"
// 평면이 된다. 바닥(Level)은 낮게 깔고, 심박(Pulse)이 규칙적으로 밀어 올리고,
// 파열(Surge)이 불규칙하게 크게 무너뜨린다. 한 프레임 안에서는 셋 다 시간의
// 함수라 값이 흔들리므로, 그리는 쪽은 프레임마다 한 번만 읽어 돌려 쓸 것.
int AmbientNoiseLevel();   // 테두리 띠 밀도 (0 = 위독 연출 없음)
int AmbientNoiseBand();    // 테두리 띠 두께(px)
int AmbientNoisePulse();   // 심박 0~1000. 규칙적으로 두 번 뛰고 쉰다
int AmbientNoiseSurge();   // 파열 0~1000. 불규칙하게 찾아온다 (평소 0)
// 파열 순간에 띠 안에서 가로로 어긋나는 줄. 있으면 1과 함께 자리를 채워 준다.
// skew는 아래로 갈수록 더 벌어지는 몫이라 그대로 DrawSignalSlip에 넘기면 된다.
#define AMBIENT_SLIP_MAX 3
int AmbientSlip(int index, int* y, int* height, int* shift, int* skew);
void SyncLastGasp();       // 체력 1이 된 시각을 잡아 둔다 (띠가 자라는 기준)
int NoiseFrameStep();

// ---- 관리자 터미널 (디버그) ------------------------------------------------
// `(백틱)으로 열고 닫는다. 보스까지 가는 데 걸리는 시간을 줄이려고 넣은 개발용
// 창이라 규칙에는 관여하지 않는다. 명령이 부르는 것은 전부 정규 규칙 함수다.
// 커서를 깜빡이지 않으므로 리페인트를 따로 돌릴 필요가 없다.
//
// 배포 빌드에서는 열리지 않는다. AROGUE_DEV로 빌드했거나 실행 인자에 -dev가
// 있을 때만 gDevMode가 서고, 그때만 백틱이 먹는다. 일반 플레이어가 실수로
// 승리 명령을 눌러 캠페인 기록을 망치는 길을 아예 없앤다.
extern int gDevMode;
#define TERM_LOG_LINES 10
#define TERM_LOG_CAP   72
#define TERM_INPUT_MAX 40
extern int gTermOpen;
extern wchar_t gTermLog[TERM_LOG_LINES][TERM_LOG_CAP];
extern int gTermLogCount;
extern wchar_t gTermInput[TERM_INPUT_MAX + 1];
extern int gTermInputLen;
void DrawTerminal(HDC dc, int width, int height);

// 새 게임 삽입 연출. 붙잡아 둔 판을 돌려 얹으므로 캔버스의 실제 픽셀 크기가 필요하다.
void DrawBootInsert(HDC dc, int width, int height, int deviceW, int deviceH);
void DrawBossIntro(HDC dc, int width, int height);

// ---- 16:9 레이아웃 ---------------------------------------------------------
// 캔버스가 1120에서 1352로 넓어졌다. 전투판(적·슬롯·주사위)은 예전 좌표를 그대로
// 쓰고, 새로 생긴 오른쪽 폭은 전투 정보 사이드바가 쓴다. 1120 폭 기준으로 짜인
// 비전투 화면은 LEGACY_X만큼 밀어 가운데에 둔다.
static const int LEGACY_WIDTH = 1120;
static const int LEGACY_X = (BASE_WIDTH - LEGACY_WIDTH) / 2;
static const int COMBAT_MAIN_RIGHT = 930;   // 전투판과 조작 버튼이 쓰는 오른쪽 끝
static const int SIDEBAR_LEFT = 952, SIDEBAR_RIGHT = 1330;
static const int SIDEBAR_TOP = 94, SIDEBAR_BOTTOM = 738;

// ---- 레이아웃 (그리기와 클릭 판정이 같은 사각형을 봐야 한다) --------------
RECT GuideButtonRect(int width);
RECT GuideCloseRect(int width);
RECT GuidePrevRect(int width, int height);
RECT GuideNextRect(int width, int height);
RECT GuideTabRect(int page);
RECT SettingsButtonRect(int width);
RECT SettingsCloseRect(int width);
RECT DeckButtonRect(int width);
RECT DeckCloseRect(int width);
RECT ScaleOptionRect(int index);
RECT LanguageOptionRect(int index);
enum AudioVolumeChannel { AUDIO_VOLUME_MASTER = 0, AUDIO_VOLUME_BGM, AUDIO_VOLUME_SFX, AUDIO_VOLUME_COUNT };
RECT VolumeSliderRect(int channel);
RECT VolumeHandleRect(int channel, int volume);
int VolumeFromX(int channel, int x);          // 슬라이더 위 x좌표를 0~100으로
RECT FullscreenToggleRect();
RECT BgmToggleRect();
RECT RestartButtonRect();
RECT CampaignResetRect();
RECT FxLevelRect(int index);
RECT StartButtonRect(int width, int height);
RECT DriveCardRect(int index);
RECT ReplayPrevRect();
RECT ReplayNextRect();
RECT DirectoryChoiceRect(int i);
RECT EnemyRect(int i);
RECT SlotRect(int i);
RECT DieRect(int i);
RECT EndTurnRect();
RECT ReadButtonRect();
RECT RewardRect(int i, int width);
RECT FaceGridRect(int die, int face);
RECT ContinueRect(int width, int height);
// 스토리 화면의 [다음]. 패널 아무 곳이나 눌러 넘어가지 않도록 진행 입력을
// 이 버튼 하나로 좁힌다 (엔터·스페이스는 그대로 받는다).
RECT StoryNextRect(int width, int height);
RECT EndingChoiceRect(int index);
// 최종 명령의 확정 버튼. 카드 선택과 실행을 갈라 놓는다.
RECT EndingConfirmRect();
RECT EndingRestartRect();
RECT KeybButtonRect();
RECT TurnTraceTickerRect();
RECT TurnTracePanelRect();
// 전투 정보 사이드바. 위에서부터 대상 → 예상 → 시스템 → 기록이고, 계산 재생
// 중에는 예상부터 기록까지가 TurnTracePanelRect 하나로 합쳐진다. 전부 표시
// 전용이라 클릭을 받지 않는다.
RECT CombatSidebarRect();
RECT TargetInfoRect();
RECT ForecastRect();
RECT SystemInfoRect();
RECT CombatHistoryRect();
RECT PruneTsrRect(int i);

int DieForSlotUI(int slot);
int CanRepairSector();
int VictoryElapsed();

// ---- 화면 -----------------------------------------------------------------
void ApplyFullscreen(int enable);
void ApplyWindowedScale(int percent);
// 지금 적용된 창 배율(%). 설정 저장이 화면 상태가 아니라 값을 읽어야 한다.
int WindowedScale();
void PaintGame(HWND window);
// 페인트 계측. 터미널 perf 명령이 읽는다.
int PaintLastMs();
int PaintMaxMs();
int PaintCount();
