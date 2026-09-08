from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path): return (ROOT / path).read_text(encoding='utf-8')
def write(path, text): (ROOT / path).write_text(text, encoding='utf-8', newline='\n')
def once(text, old, new, label):
    n = text.count(old)
    if n != 1:
        raise RuntimeError(f'{label}: expected 1 occurrence, got {n}')
    return text.replace(old, new, 1)

def replace_function(text, signature, replacement):
    start = text.find(signature)
    if start < 0: raise RuntimeError(f'function not found: {signature}')
    brace = text.find('{', start)
    if brace < 0: raise RuntimeError(f'opening brace not found: {signature}')
    depth = 0
    i = brace
    while i < len(text):
        if text[i] == '{': depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[:start] + replacement.rstrip() + text[i+1:]
        i += 1
    raise RuntimeError(f'unclosed function: {signature}')

# #110 #111 #112 #116: save safety and capture cancellation.
p = 'src/main.cpp'; s = read(p)
s = once(s, 'int gSaveFailed;\nint gFxLevel = FX_FULL;',
'''int gSaveFailed;
// Settings and corrupt-input failures are tracked separately so a successful
// campaign write cannot hide a failed CFG write, and vice versa.
int gSettingsSaveFailed;
int gCampaignCorrupt;
int gFxLevel = FX_FULL;''', 'save state globals')

s = replace_function(s, 'static void PersistSettings()', r'''static void PersistSettings() {
    CaptureSettings();
    gSettingsSaveFailed = SaveSettings(&gSettings) ? 0 : 1;
}''')

insert_at = 'static void PersistCampaignProgress() {'
helper = r'''static int CampaignSaveExistsBesideExecutable() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(0, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 0;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) lstrcpyW(slash + 1, L"AROGUE.SAV");
    DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

'''
if helper.strip() not in s:
    s = once(s, insert_at, helper + insert_at, 'campaign save existence helper')

s = once(s,
'    if (changed) gSaveFailed = SaveCampaign(&gCampaign) ? 0 : 1;',
'    if (changed && !gCampaignCorrupt) gSaveFailed = SaveCampaign(&gCampaign) ? 0 : 1;',
'corrupt save write gate')

s = replace_function(s, 'static void ResetCampaignProgress()', r'''static void ResetCampaignProgress() {
    FinishDeath();
    FinishDirectoryEnter();
    FinishBossIntro();
    FinishUiFx();
    gUiFxPendingDescent = -1;
    gVictoryStart = 0;
    CampaignState previous = gCampaign;
    InitCampaign(&gCampaign);
    // Reset is the explicit authorization to replace a corrupt/old save. Do not
    // show 0/6 unless the replacement reached disk; roll memory back on failure.
    if (!SaveCampaign(&gCampaign)) {
        gCampaign = previous;
        gSaveFailed = 1;
        return;
    }
    gCampaignCorrupt = 0;
    gSaveFailed = 0;
    InitTitle(&gGame, 0, 0);
}''')

s = once(s, '    case WM_CAPTURECHANGED: gVolumeDragging = 0; return 0;',
             '    case WM_CAPTURECHANGED: gVolumeDragging = -1; return 0;',
             'capture changed sentinel')
s = once(s, '        SaveCampaign(&gCampaign);',
             '        if (!gCampaignCorrupt) SaveCampaign(&gCampaign);',
             'destroy corrupt save gate')
s = once(s, '    LoadCampaign(&gCampaign);\n    LoadCodex(gCodex, ENEMY_KIND_COUNT);',
'''    int hadCampaignSave = CampaignSaveExistsBesideExecutable();
    if (!LoadCampaign(&gCampaign) && hadCampaignSave) gCampaignCorrupt = 1;
    LoadCodex(gCodex, ENEMY_KIND_COUNT);''', 'corrupt load detection')
# Remove stale wording that contradicted the two-step reward flow (#117).
s = s.replace('// 보스 전리품: 카드 클릭 한 번으로 즉시 상주한다.',
              '// 보스 전리품: 첫 입력은 후보, 같은 카드를 다시 누르면 설치 확정이다.')
write(p, s)

# Expose independent warning states to the renderer.
p = 'src/ui.h'; s = read(p)
s = once(s, 'extern int gSaveFailed;',
'''extern int gSaveFailed;
extern int gSettingsSaveFailed;
extern int gCampaignCorrupt;''', 'ui save warning externs')
write(p, s)

# #114 multi-monitor scaling, #110/#111 warnings, #117 title copy.
p = 'src/screens.cpp'; s = read(p)
s = replace_function(s, 'void ApplyWindowedScale(int percent)', r'''void ApplyWindowedScale(int percent) {
    gWindowedScale = percent;
    if (gFullscreen) ApplyFullscreen(0);
    RECT desired = {0, 0, BASE_WIDTH * percent / 100, BASE_HEIGHT * percent / 100};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int width = desired.right - desired.left, height = desired.bottom - desired.top;

    MONITORINFO info = {}; info.cbSize = sizeof(info);
    HMONITOR monitor = MonitorFromWindow(gWindow, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(monitor, &info)) {
        info.rcWork = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    }
    int workW = info.rcWork.right - info.rcWork.left;
    int workH = info.rcWork.bottom - info.rcWork.top;
    if (width > workW) width = workW;
    if (height > workH) height = workH;
    int x = info.rcWork.left + (workW - width) / 2;
    int y = info.rcWork.top + (workH - height) / 2;
    SetWindowPos(gWindow, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED);
}''')
s = once(s,
'''    if (gSaveFailed) {
        RECT warn = MakeRect(BASE_WIDTH / 2 - 300, 2, BASE_WIDTH / 2 + 300, 22);
        Panel(canvas, warn, RGB(48, 12, 12), C_RED);
        TextRect(canvas, warn, L"진행도를 저장하지 못했습니다 · AROGUE.exe가 있는 폴더에 쓸 수 있는지 확인하십시오",
            C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }''',
'''    if (gCampaignCorrupt || gSaveFailed || gSettingsSaveFailed) {
        RECT warn = MakeRect(BASE_WIDTH / 2 - 390, 2, BASE_WIDTH / 2 + 390, 22);
        Panel(canvas, warn, RGB(48, 12, 12), C_RED);
        const wchar_t* warning = gCampaignCorrupt
            ? L"세이브 검증 실패 · 원본 AROGUE.SAV는 보호 중입니다 · 설정의 진행도 초기화로 새로 시작할 수 있습니다"
            : gSettingsSaveFailed
                ? L"설정을 저장하지 못했습니다 · AROGUE.CFG를 쓸 수 있는지 확인하십시오"
                : L"진행 데이터를 저장하지 못했습니다 · 실행 폴더에 쓸 수 있는지 확인하십시오";
        TextRect(canvas, warn, warning, C_RED, gFontSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }''', 'save warning banner')
s = once(s,
'L"마우스 또는 1·2·3으로 주사위 선택  /  슬롯 클릭으로 배치  /  스페이스 키로 실행  /  취소 키로 보상 건너뛰기"',
'L"1·2·3 주사위 선택 / 슬롯 클릭 배치 / Space 실행 / 보상 Esc: 선택 취소 · 선택 없음에서 두 번 눌러 포기"',
'title control hint')
write(p, s)

# #115 translation manifest/completeness validation.
p = 'src/localization.cpp'; s = read(p)
s = once(s, 'static int gLocalizedBufferIndex;',
'''static int gLocalizedBufferIndex;
static int gTranslationManifestValid;''', 'translation manifest state')
s = once(s, '    gTranslations.clear();', '    gTranslations.clear();\n    gTranslationManifestValid = 0;', 'translation reset')
s = once(s, "        if (line.empty() || line[0] == '#') continue;",
'''        if (line == "# AROGUE_TRANSLATIONS_V2") { gTranslationManifestValid = 1; continue; }
        if (line.empty() || line[0] == '#') continue;''', 'translation marker parsing')
s = once(s, 'int TranslationsLoaded() { return gTranslations.empty() ? 0 : 1; }',
r'''int TranslationsLoaded() {
    // A one-row or stale table must not enable English and then fall back to
    // Korean line-by-line. The version marker catches stale packages; a floor
    // on populated entries catches truncated files while remaining tolerant of
    // newly added optional copy.
    if (!gTranslationManifestValid || gTranslations.size() < 100) return 0;
    for (size_t i = 0; i < gTranslations.size(); ++i)
        if (gTranslations[i].english.empty()) return 0;
    return 1;
}''', 'translation completeness')
write(p, s)

# Translation tables: schema marker + current title instruction.
for p in ('translations.tsv', 'build/translations.tsv'):
    s = read(p)
    if not s.startswith('# AROGUE_TRANSLATIONS_V2\n'):
        s = '# AROGUE_TRANSLATIONS_V2\n' + s
    old = '마우스 또는 1·2·3으로 주사위 선택  /  슬롯 클릭으로 배치  /  스페이스 키로 실행  /  취소 키로 보상 건너뛰기\tSelect a die with the mouse or 1/2/3  /  Click a slot to place  /  Space to execute  /  Esc to skip rewards'
    new = '1·2·3 주사위 선택 / 슬롯 클릭 배치 / Space 실행 / 보상 Esc: 선택 취소 · 선택 없음에서 두 번 눌러 포기\t1/2/3 select a die / click a slot / Space executes / reward Esc cancels selection; press twice with none selected to skip'
    s = once(s, old, new, p + ' title translation')
    write(p, s)

# #117/#118 documentation reflects confirmation and three-channel audio.
p = 'README.md'; s = read(p)
s = once(s,
'- 소리 크기 슬라이더 (0~100%): 믹서 단계에서 걸리는 마스터 볼륨이라 울리는 중인 소리에도 바로 적용된다. 끌거나 좌우 방향키로 조절하고, 기본값은 50%',
'- 오디오 설정: Master / BGM / SFX를 각각 0~100%로 독립 조절하며 새 설정의 기본값은 모두 100%다. BGM on/off는 음악 재생 자체를 켜고 끄고, BGM 볼륨은 켜진 음악의 크기만 바꾼다. 모든 값은 `AROGUE.CFG`에 저장되어 재실행 후 유지된다.',
'README audio settings')
s = once(s,
'| 보상 | 카드 클릭 / `1` `2` `3` | 보상 면 선택 (보스 전리품은 클릭 즉시 상주) |',
'| 보상 | 카드 클릭 / `1` `2` `3` | 일반 면 선택. 보스 전리품은 첫 입력으로 후보 지정 → 같은 카드 다시 입력해 설치 확정 |',
'README boss reward confirmation')
# Document the safe skip rule next to reward controls.
needle = '| 보상 | 기존 면 클릭 | 선택한 보상 면으로 교체 |'
if needle in s and '선택 없음에서 다시 `Esc`' not in s:
    s = s.replace(needle, needle + '\n| 보상 | `Esc` | 선택/후보 취소. 선택이 없는 상태에서 다시 `Esc`를 눌러야 보상 포기 확정 |', 1)
# Append concise confirmation matrix so directory/final choices cannot drift silently.
if '### 되돌릴 수 없는 선택의 확정 규칙' not in s:
    s += r'''

### 되돌릴 수 없는 선택의 확정 규칙

- **보스 전리품:** 카드 선택 → 같은 카드 다시 입력해 설치 확정
- **디렉터리:** 첫 입력으로 후보 지정 → 같은 선택지를 다시 입력해 확정
- **최종 명령:** 카드 선택 → `Enter`/`Space` 또는 확정 동작으로 실행
- **보상 포기:** 현재 선택을 `Esc`로 먼저 취소하고, 선택이 없는 상태에서 `Esc`를 다시 입력해 포기 확정
'''
write(p, s)

print('bug/save/docs issue patch applied')
