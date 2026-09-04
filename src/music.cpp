// 절차적 BGM. 외부 음원 없이 정수 연산만으로 실시간 합성한다.
//
// 구조는 "재생기 + 곡 데이터"다. 재생기는 16분음표 스텝 시퀀서이고, 스텝이 바뀌는
// 샘플에서만 음을 트리거한다. 그 뒤로는 엔벨로프·필터가 샘플마다 굴러간다.
//
// 곡은 드라이브당 하나. 8마디(A 4마디 + B 4마디) 코드 진행 위에 베이스 리프,
// 아르페지오, 리드 선율, 타악기 마스크가 얹힌다. 악보 표기와 생성기는
// tools/gen_music.py 에 있고 이 파일의 SONG 표는 거기서 제자리에 다시 쓴다.
// 곡을 고치려면 SONG 표를 손으로 만지지 말고 생성기의 악보를 고친다.
//
// 지켜야 할 것: 결정론(같은 입력이면 같은 출력), 게임 RNG 불가침(자기 noiseRng만
// 쓴다), 부동소수점 금지, 샘플 클럭은 프레임 수만큼 정확히 증가.
#include "music.h"

#define REST (-128)
#define TIE  (-127)

struct MusicSong {
    int8_t tonic;             // 반음, 0 = A2 (110Hz)
    uint8_t bpm, steps;       // steps: 마디의 스텝 수 (X:\는 15)
    uint8_t duty, cut;        // 리드 펄스 듀티 %, 리드·아르페지오 저역통과 기본값 (0~255)
    uint16_t drop;            // 이 스텝에서는 리드·아르페지오가 빠진다 (E:\ 접촉 불량)
    int8_t root[8], third[8]; // 마디별 코드: 근음(tonic 기준 반음), 3도(3 단조 / 4 장조)
    int8_t bass[2][16];       // 근음 기준 오프셋. 섹션 A/B
    int8_t arp[2][16];        // 코드 톤 번호: 0 근음 1 3도 2 5도 3 옥타브 4 옥타브+3도 5 옥타브+5도
    int8_t lead[2][64];       // tonic 기준 절대 반음. 섹션당 4마디
    uint16_t kick[2], snare[2], hat[2];
};

static const MusicSong SONG[6] = {
    { // C:\ SYSTEM
      0, 118, 16, 25, 200, 0x0000,
      {0,-4,3,-2,5,0,-4,-5}, {3,4,4,4,3,3,4,4},
      {{0,-128,-128,-128,-128,-128,0,-128,7,-128,-128,-128,0,-128,5,-128}, {0,-128,-128,-128,-128,-128,0,-128,7,-128,-128,-128,0,-128,3,-128}},
      {{0,2,3,2,0,2,3,2,0,2,3,2,1,2,3,2}, {0,2,3,5,3,2,0,2,0,2,3,5,3,2,1,2}},
      {{12,-128,-128,-128,15,-128,17,-128,19,-128,-128,-128,-128,-128,17,-128,15,-128,-128,-128,17,-128,15,-128,12,-128,-128,-128,-128,-128,-128,-128,12,-128,-128,-128,15,-128,19,-128,24,-128,-128,-128,22,-128,19,-128,17,-128,-128,-128,-128,-128,19,-128,14,-128,-128,-128,-128,-128,-128,-128},
       {17,-128,-128,-128,20,-128,22,-128,24,-128,-128,-128,-128,-128,22,-128,19,-128,-128,-128,17,-128,15,-128,12,-128,-128,-128,-128,-128,-128,-128,20,-128,-128,-128,24,-128,20,-128,17,-128,-128,-128,15,-128,-128,-128,16,-128,-128,-128,-128,-128,19,-128,23,-128,-128,-128,-128,-128,-128,-128}},
      {0x1111, 0x1111}, {0x1010, 0x1010}, {0x5555, 0x5555}
    },
    { // D:\ ARCHIVE
      3, 98, 16, 50, 130, 0x0000,
      {0,0,-4,5,3,-2,0,-5}, {3,3,4,3,4,4,3,3},
      {{0,-128,-128,-128,-128,-128,-128,-128,0,-128,-128,7,-128,-128,5,-128}, {0,-128,-128,-128,-128,-128,-128,-128,0,-128,-128,-128,7,-128,-128,-128}},
      {{0,-128,2,-128,3,-128,2,-128,0,-128,2,-128,4,-128,3,-128}, {0,-128,2,-128,3,-128,4,-128,3,-128,2,-128,0,-128,2,-128}},
      {{-128,-128,-128,-128,-128,-128,-128,-128,15,-128,-128,-128,17,-128,-128,-128,19,-128,-128,-128,-128,-128,-128,-128,15,-128,-128,-128,12,-128,-128,-128,-128,-128,-128,-128,20,-128,-128,-128,19,-128,-128,-128,15,-128,-128,-128,17,-128,-128,-128,-128,-128,-128,-128,20,-128,-128,-128,17,-128,-128,-128},
       {22,-128,-128,-128,-128,-128,-128,-128,19,-128,-128,-128,15,-128,-128,-128,17,-128,-128,-128,-128,-128,-128,-128,14,-128,-128,-128,10,-128,-128,-128,12,-128,-128,-128,-128,-128,-128,-128,15,-128,-128,-128,19,-128,-128,-128,14,-128,-128,-128,-128,-128,-128,-128,22,-128,-128,-128,19,-128,-128,-128}},
      {0x0101, 0x0101}, {0x1000, 0x1000}, {0x4444, 0x4444}
    },
    { // E:\ REMOVABLE
      5, 126, 16, 25, 215, 0x0408,
      {0,-2,-4,-2,5,0,-4,-5}, {3,4,4,4,3,3,4,4},
      {{0,-128,0,-128,-128,0,-128,-128,0,-128,-128,0,-128,7,-128,5}, {0,-128,0,-128,-128,0,-128,-128,0,-128,-128,0,-128,3,-128,5}},
      {{0,3,2,3,0,3,2,3,1,3,2,3,0,3,2,5}, {0,3,2,3,0,3,2,3,1,3,2,3,0,3,2,3}},
      {{12,-128,12,-128,-128,15,-128,-128,17,-128,-128,15,-128,-128,12,-128,14,-128,14,-128,-128,12,-128,-128,10,-128,-128,12,-128,-128,14,-128,15,-128,-128,-128,17,-128,-128,-128,20,-128,-128,-128,17,-128,15,-128,14,-128,-128,-128,12,-128,-128,-128,10,-128,-128,-128,-128,-128,-128,-128},
       {19,-128,19,-128,-128,17,-128,-128,15,-128,-128,17,-128,-128,19,-128,17,-128,-128,-128,15,-128,-128,-128,14,-128,-128,-128,12,-128,-128,-128,22,-128,-128,-128,20,-128,-128,-128,17,-128,-128,-128,15,-128,-128,-128,13,-128,-128,-128,-128,-128,16,-128,19,-128,-128,-128,-128,-128,-128,-128}},
      {0x1109, 0x1109}, {0x1010, 0x1010}, {0xAAAA, 0xAAAA}
    },
    { // N:\ NETWORK
      2, 142, 16, 12, 235, 0x0000,
      {0,5,-4,-2,3,-2,0,-5}, {3,3,4,4,4,4,3,4},
      {{0,-128,0,0,-128,0,-128,0,-128,0,0,-128,0,-128,7,-128}, {0,-128,0,0,-128,0,-128,0,-128,0,0,-128,0,-128,5,-128}},
      {{0,2,3,5,3,2,0,2,3,5,3,2,0,2,3,2}, {3,2,0,2,3,5,3,2,3,2,0,2,3,5,4,5}},
      {{12,-128,-128,15,-128,-128,19,-128,-128,-128,17,-128,15,-128,-128,-128,17,-128,-128,19,-128,-128,22,-128,-128,-128,19,-128,17,-128,-128,-128,20,-128,-128,22,-128,-128,24,-128,-128,-128,27,-128,24,-128,-128,-128,22,-128,-128,24,-128,-128,26,-128,-128,-128,24,-128,22,-128,-128,-128},
       {27,-128,-128,24,-128,-128,22,-128,-128,-128,24,-128,27,-128,-128,-128,26,-128,-128,24,-128,-128,22,-128,-128,-128,20,-128,22,-128,-128,-128,24,-128,-128,22,-128,-128,20,-128,-128,-128,19,-128,17,-128,-128,-128,19,-128,-128,-128,23,-128,-128,-128,26,-128,-128,-128,23,-128,-128,-128}},
      {0x1111, 0x1111}, {0x1010, 0x1010}, {0xFFFF, 0xFFFF}
    },
    { // R:\ RAMDISK
      -5, 162, 16, 12, 245, 0x0000,
      {0,3,5,8,10,8,7,0}, {3,4,3,4,4,4,4,3},
      {{0,-128,0,-128,0,-128,0,-128,0,-128,0,-128,0,-128,0,-128}, {0,0,-128,0,0,0,-128,0,0,0,-128,0,0,-128,7,-128}},
      {{0,2,3,4,3,2,0,2,3,4,5,4,3,2,0,2}, {5,4,3,2,3,4,5,4,3,2,0,2,3,4,3,2}},
      {{12,-128,14,-128,15,-128,-128,-128,19,-128,-128,-128,17,-128,15,-128,15,-128,17,-128,19,-128,-128,-128,22,-128,-128,-128,19,-128,17,-128,17,-128,19,-128,20,-128,-128,-128,24,-128,-128,-128,20,-128,19,-128,20,-128,22,-128,24,-128,-128,-128,27,-128,-128,-128,24,-128,22,-128},
       {26,-128,-128,-128,24,-128,-128,-128,22,-128,-128,-128,21,-128,-128,-128,20,-128,-128,-128,19,-128,-128,-128,17,-128,-128,-128,15,-128,-128,-128,16,-128,-128,-128,14,-128,-128,-128,11,-128,-128,-128,14,-128,-128,-128,12,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,12,-128,14,-128}},
      {0x1111, 0x1191}, {0x1010, 0x1010}, {0xFFFF, 0xFFFF}
    },
    { // X:\ QUARANTINE
      1, 112, 15, 50, 165, 0x0000,
      {0,1,0,-5,5,1,-2,0}, {3,4,3,3,3,4,4,3},
      {{0,-128,-128,0,-128,-128,0,-128,-128,0,-128,-128,0,-128,1,-128}, {0,-128,-128,0,-128,-128,0,-128,-128,0,-128,-128,7,-128,1,-128}},
      {{0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,-128}, {0,1,2,3,1,2,0,1,2,3,1,2,0,1,2,-128}},
      {{12,-128,-128,15,-128,-128,13,-128,-128,12,-128,-128,-128,-128,-128,-128,13,-128,-128,17,-128,-128,20,-128,-128,17,-128,-128,13,-128,-128,-128,12,-128,-128,15,-128,-128,17,-128,-128,19,-128,-128,15,-128,-128,-128,17,-128,-128,22,-128,-128,24,-128,-128,22,-128,-128,17,-128,-128,-128},
       {17,-128,-128,20,-128,-128,24,-128,-128,20,-128,-128,17,-128,-128,-128,13,-128,-128,17,-128,-128,20,-128,-128,13,-128,-128,25,-128,-128,-128,22,-128,-128,26,-128,-128,29,-128,-128,26,-128,-128,22,-128,-128,-128,12,-128,-128,13,-128,-128,12,-128,-128,-128,-128,-128,-128,-128,-128,-128}},
      {0x0249, 0x0249}, {0x1040, 0x1040}, {0x2492, 0x2492}
    }
};

// 성부별 엔벨로프. attack은 샘플당 증가량(65536 = 최대), 감쇠·릴리스는 시프트 양.
struct VoiceShape { int attack, decayShift, sustain, releaseShift; };
static const VoiceShape SHAPE[MUSIC_CHANNELS] = {
    {2048, 9, 42000, 6},   // 베이스: 빠른 어택, 길게 유지
    {1200, 8, 34000, 6},   // 리드
    {4096, 6, 6000, 5},    // 아르페지오: 짧게 톡톡 튄다
    {48, 12, 65536, 9}     // 코드: 60ms 어택, 계속 울린다
};

// ---- 8비트 칩 특성 ----------------------------------------------------------
// NES 삼각파는 4비트 계단이다. 매끈한 삼각파를 16단으로 깎으면 그 거친 저음이 난다.
static int Steps4(int v) { return (v / 4096) * 4096; }
// NES 노이즈 채널은 1비트 LFSR이다. 크기가 없는 부호만 남기면 훨씬 거칠고 건조하다.
static int Bit(int noise) { return noise >= 0 ? 24000 : -24000; }

static int NoteHz(int semitone) {
    static const int ratio[12] = {1024,1085,1149,1218,1290,1367,1448,1534,1625,1722,1825,1933};
    int octave = semitone / 12, note = semitone % 12;
    if (note < 0) { note += 12; --octave; }
    int hz = 110 * ratio[note] / 1024;
    while (octave > 0) { hz *= 2; --octave; }
    while (octave < 0) { hz /= 2; ++octave; }
    return hz < 20 ? 20 : hz;
}
static uint32_t PhaseStep(int hz) { return (uint32_t)((uint64_t)hz * 0x100000000ull / MUSIC_RATE); }
static int Scale(int v, int gain) { return (int)((int64_t)v * gain / 65536); }
static int Clamp16(int v) { return v < 0 ? 0 : v > 65536 ? 65536 : v; }

static int Triangle(uint32_t phase) { int p = (int)(phase >> 16) & 65535; return p < 32768 ? p * 2 - 32768 : 98303 - p * 2; }
static int Pulse(uint32_t phase, int duty) { return ((int)(phase >> 16) & 65535) < duty * 655 ? 22000 : -22000; }
// 포물선 근사 사인. 킥 드럼 몸통에 쓴다. 표가 필요 없고 정수만 쓴다.
static int Para(uint32_t phase) {
    int p = (int)(phase >> 16) & 65535, h = p & 32767;
    int y = (h * (32768 - h)) >> 13;
    return p < 32768 ? y : -y;
}
// 1극 저역통과. cut이 낮을수록 어둡다. 효과음 쪽과 같은 식이다.
static int LowPass(int* lp, int v, int cut) { *lp += (v - *lp) * cut / 256; return *lp; }

static int ChordTone(int idx, int third) {
    static const int8_t TONE[6] = {0, 0, 7, 12, 12, 19};
    if (idx < 0 || idx > 5) return 0;
    return TONE[idx] + ((idx == 1 || idx == 4) ? third : 0);
}

static void Trigger(MusicChannelState* v, int note) { v->note = note; v->env = 0; v->stage = 0; v->gate = 1; v->held = 0; }

static void Envelope(MusicChannelState* v, const VoiceShape* s) {
    if (!v->gate) { v->env -= v->env >> s->releaseShift; if (v->env < 16) v->env = 0; return; }
    if (v->stage == 0) { v->env += s->attack; if (v->env >= 65536) { v->env = 65536; v->stage = 1; } return; }
    if (v->env > s->sustain) v->env -= (v->env - s->sustain) >> s->decayShift;
    else if (v->env < s->sustain) v->env += (s->sustain - v->env) >> s->decayShift;
}

void MusicInit(MusicState* m) {
    if (!m) return;
    *m = {};
    m->noiseRng = 0xA11CE55u; m->enabled = 1; m->stepCount = SONG[0].steps; m->bpm = SONG[0].bpm;
    m->driveFade = 65536; m->sceneFade = 65536; m->currentScene = MUSIC_SCENE_TITLE;
    m->pendingStep = 1;   // 첫 샘플에서 0번 스텝을 트리거한다
}
void MusicSetDrive(MusicState* m, int d) { if (m) m->targetDrive = d < 0 ? 0 : d > 5 ? 5 : d; }
void MusicSetScene(MusicState* m, int s) { if (m) m->scene = s; }
void MusicSetIntensity(MusicState* m, int v) { if (m) m->targetIntensity = v < 0 ? 0 : v > 3 ? 3 : v; }
void MusicSetCritical(MusicState* m, int v) { if (m) m->critical = v != 0; }
void MusicSetEnabled(MusicState* m, int v) { if (m) m->enabled = v != 0; }
void MusicSetEnding(MusicState* m, int v) { if (m) m->ending = v != 0; }

// 스텝이 바뀐 샘플에서 한 번. 이번 스텝의 음과 타악기를 건다.
static void TriggerStep(MusicState* m, const MusicSong* s, int crit) {
    int step = m->step, sec = (m->bar >> 2) & 1, chord = sec * 4 + (m->bar & 3);
    int root = s->tonic + s->root[chord], third = s->third[chord];
    int dropped = (s->drop >> step) & 1;
    int v;
    v = s->bass[sec][step];
    if (v == REST) m->channel[0].gate = 0;
    else if (v != TIE) Trigger(&m->channel[0], root + v - (crit > 32768 ? 12 : 0));   // 위독: 한 옥타브 아래
    v = dropped ? REST : s->lead[sec][(m->bar & 3) * 16 + step];
    if (v == REST) m->channel[1].gate = 0;
    else if (v != TIE) Trigger(&m->channel[1], s->tonic + v);
    v = dropped ? REST : s->arp[sec][step];
    if (v == REST) m->channel[2].gate = 0;
    else if (v != TIE) Trigger(&m->channel[2], root + ChordTone(v, third));
    if (step == 0) {
        // 코드 성부는 마디마다 근음을 잡고 계속 울린다. 3도·5도는 렌더에서 60Hz로 번갈아 낸다.
        // 채널 하나로 화음을 내는 C64/NES의 고속 아르페지오다. 음이 같으면 건드리지 않는다.
        MusicChannelState* c = &m->channel[3];
        int want = root + 12;
        if (c->note != want || !c->gate) { c->note = want; c->gate = 1; c->stage = 0; }
        m->chordThird = third;
    }
    int snareMask = s->snare[sec];
    if ((m->bar & 3) == 3) snareMask |= 0xC000;   // 4마디마다 끝에 작은 채움
    if ((s->kick[sec] >> step) & 1) { m->drum.kickEnv = 65536; m->drum.kickPhase = 0; }
    if ((snareMask >> step) & 1) { m->drum.snareEnv = 65536; m->drum.bodyPhase = 0; }
    if ((s->hat[sec] >> step) & 1) m->drum.hatEnv = 65536;
}

void MusicRender(MusicState* m, int32_t* out, int frames) {
    if (!m || !out || frames <= 0) return;
    for (int i = 0; i < frames; ++i) {
        // drive/scene은 sample clock을 건드리지 않고 0.4초 fade-out, 0.7초 fade-in한다.
        if (m->currentDrive != m->targetDrive) {
            m->driveFade -= 7;
            if (m->driveFade <= 0) { m->driveFade = 0; m->currentDrive = m->targetDrive; }
        } else if (m->driveFade < 65536) { m->driveFade += 4; if (m->driveFade > 65536) m->driveFade = 65536; }
        if (m->currentScene != m->scene) {
            m->sceneFade -= 7;
            if (m->sceneFade <= 0) { m->sceneFade = 0; m->currentScene = m->scene; }
        } else if (m->sceneFade < 65536) { m->sceneFade += 4; if (m->sceneFade > 65536) m->sceneFade = 65536; }
        const MusicSong* s = &SONG[m->currentDrive];
        m->bpm = s->bpm; m->stepCount = s->steps;
        m->stepClock += (uint32_t)(m->bpm * 4);
        if (m->stepClock >= MUSIC_RATE * 60u) {
            m->stepClock -= MUSIC_RATE * 60u;
            if (++m->step >= m->stepCount) { m->step = 0; ++m->bar; }
            m->pendingStep = 1;
        }
        ++m->sampleClock;
        // 노이즈는 어느 층이 켜져 있든 매 샘플 돌린다. 그래야 편곡이 바뀌어도 잡음 열이 같다.
        m->noiseRng = m->noiseRng * 1664525u + 1013904223u;
        int noise = (int)(m->noiseRng >> 16) - 32768;
        int prevNoise = m->drum.noisePrev;   // 하이햇이 차분(고역)을 만들 때 쓴다
        m->drum.noisePrev = noise;
        (void)prevNoise;
        if (!m->enabled || m->currentScene == MUSIC_SCENE_GAMEOVER) continue;

        // ---- 층 게인. 볼륨이 아니라 악기가 들어오는 순서다 ----
        //   playGain  베이스·패드 (연주 씬이면 항상)
        //   g1        킥·스네어·하이햇  (intensity 1: 일반전)
        //   g2        리드              (2: 보스전)
        //   g3        아르페지오, 필터 개방 (3: 보스 체력 절반)
        int wantIntensity = m->currentScene == MUSIC_SCENE_VICTORY ? 2 : m->targetIntensity;
        int targetGain = wantIntensity * 65536;
        if (m->intensityGain < targetGain) { m->intensityGain += 6; if (m->intensityGain > targetGain) m->intensityGain = targetGain; }
        else if (m->intensityGain > targetGain) { m->intensityGain -= 6; if (m->intensityGain < targetGain) m->intensityGain = targetGain; }
        m->intensity = (m->intensityGain + 32768) / 65536;
        int wantPlay = (m->currentScene == MUSIC_SCENE_PLAY || m->currentScene == MUSIC_SCENE_VICTORY) ? 65536 : 0;
        if (m->playGain < wantPlay) { m->playGain += 8; if (m->playGain > wantPlay) m->playGain = wantPlay; }
        else if (m->playGain > wantPlay) { m->playGain -= 8; if (m->playGain < wantPlay) m->playGain = wantPlay; }
        int wantCrit = m->critical ? 65536 : 0;
        if (m->criticalGain < wantCrit) { m->criticalGain += 4; if (m->criticalGain > wantCrit) m->criticalGain = wantCrit; }
        else if (m->criticalGain > wantCrit) { m->criticalGain -= 4; if (m->criticalGain < wantCrit) m->criticalGain = wantCrit; }
        int g1 = Clamp16(m->intensityGain), g2 = Clamp16(m->intensityGain - 65536), g3 = Clamp16(m->intensityGain - 131072);
        int crit = m->criticalGain, play = m->playGain;

        if (m->pendingStep) { m->pendingStep = 0; TriggerStep(m, s, crit); }

        // 필터는 위험할수록 열리고, 위독하면 어두워진다. EXEC ROGUE 엔딩은 어둡게 끝난다.
        int cut = s->cut + (255 - s->cut) * g3 / 65536;
        if (m->currentScene == MUSIC_SCENE_VICTORY && m->ending) cut = cut * 2 / 5;
        cut -= (cut - 24) * crit / 65536;

        int sample = 0;
        // 베이스: 4비트 계단 삼각파. NES의 그 저음이다.
        { MusicChannelState* c = &m->channel[0]; Envelope(c, &SHAPE[0]); c->phase += PhaseStep(NoteHz(c->note));
          if (c->env) sample += Scale(Scale(Steps4(Triangle(c->phase)), c->env), play) * 60 / 100; }
        // 리드: 얇은 펄스 + 저역통과. 한 음을 150ms 넘게 끌면 비브라토가 들어온다. 위독하면 사라진다.
        { MusicChannelState* c = &m->channel[1]; Envelope(c, &SHAPE[1]);
          uint32_t inc = PhaseStep(NoteHz(c->note));
          if (c->gate && ++c->held > 3300) {
              // 6Hz 삼각 LFO, 폭 약 +-25센트. 정수 위상 하나로 만든다.
              int lfo = Triangle((uint32_t)(c->held - 3300) * 17476u);   // 6Hz: 2^32 / 22050 * 6
              inc += (uint32_t)((int64_t)inc * lfo / (32768 * 64));
          }
          c->phase += inc;
          int v = LowPass(&c->lp, Pulse(c->phase, s->duty), cut);
          if (c->env) sample += Scale(Scale(Scale(v, c->env), g2), 65536 - crit) * 46 / 100; }
        // 아르페지오: 좁은 펄스, 짧은 음.
        { MusicChannelState* c = &m->channel[2]; Envelope(c, &SHAPE[2]); c->phase += PhaseStep(NoteHz(c->note));
          int v = LowPass(&c->lp, Pulse(c->phase, 25), cut);
          if (c->env) sample += Scale(Scale(v, c->env), g3) * 30 / 100; }
        // 코드: 근음·3도·5도를 60Hz로 번갈아 내는 고속 아르페지오. 한 채널로 화음이 난다.
        { MusicChannelState* c = &m->channel[3]; Envelope(c, &SHAPE[3]);
          int which = (int)((m->sampleClock / 367u) % 3u);   // 367샘플 = 약 60Hz
          int tone = c->note + (which == 1 ? m->chordThird : which == 2 ? 7 : 0);
          c->phase += PhaseStep(NoteHz(tone));
          int v = LowPass(&c->lp, Pulse(c->phase, 50), 70);
          if (c->env) sample += Scale(Scale(v, c->env), play) * 11 / 100; }
        // 킥: 150Hz에서 45Hz로 떨어지는 사인. 엔벨로프가 피치도 끌고 내려간다.
        if (m->drum.kickEnv > 0) {
            int hz = 45 + 105 * m->drum.kickEnv / 65536;
            m->drum.kickPhase += PhaseStep(hz);
            sample += Scale(Scale(Steps4(Para(m->drum.kickPhase)), m->drum.kickEnv), g1) * 62 / 100;
            m->drum.kickEnv -= m->drum.kickEnv / 256; if (m->drum.kickEnv < 64) m->drum.kickEnv = 0;
        }
        // 스네어: 1비트 노이즈에 185Hz 몸통을 잠깐 얹는다. 몸통은 제곱으로 더 빨리 죽는다.
        if (m->drum.snareEnv > 0) {
            int e = m->drum.snareEnv;
            m->drum.bodyPhase += PhaseStep(185);
            int body = Scale(Scale(Steps4(Triangle(m->drum.bodyPhase)), e), e);
            sample += Scale(Scale(Bit(noise), e) * 30 / 100 + body * 30 / 100, g1);
            m->drum.snareEnv -= e / 512; if (m->drum.snareEnv < 64) m->drum.snareEnv = 0;
        }
        // 하이햇: 1비트 노이즈의 차분(고역), 20ms. 짧고 바삭하다.
        if (m->drum.hatEnv > 0) {
            int tick = (Bit(noise) - Bit(prevNoise)) / 2;
            sample += Scale(Scale(tick, m->drum.hatEnv), g1) * 10 / 100;
            m->drum.hatEnv -= m->drum.hatEnv / 64; if (m->drum.hatEnv < 64) m->drum.hatEnv = 0;
        }
        // 위독: 바닥에 잡음이 깔린다.
        if (crit) sample += Scale(noise, crit) * 6 / 100;

        sample = (int)((int64_t)sample * m->driveFade / 65536 * m->sceneFade / 65536);
        out[i] += sample;
    }
}
