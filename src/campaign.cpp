#include <windows.h>
#include <wchar.h>
#include "campaign.h"

static const uint32_t CAMPAIGN_MAGIC = 0x474F5241u; // On disk: AROG
static const uint16_t CAMPAIGN_VERSION = 2;
static const DWORD SAVE_V1_SIZE = 20;
static const DWORD SAVE_SIZE = 26;

// Explicit little-endian encoding keeps compiler padding out of the save format.
static void Put32(uint8_t* bytes, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[i] = (uint8_t)(value >> (i * 8));
}

static uint32_t Get32(const uint8_t* bytes) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= (uint32_t)bytes[i] << (i * 8);
    return value;
}

static uint32_t ChecksumN(const uint8_t* bytes, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static void Encode(const CampaignState* campaign, uint8_t* bytes) {
    Put32(bytes, CAMPAIGN_MAGIC);
    bytes[4] = (uint8_t)CAMPAIGN_VERSION;
    bytes[5] = (uint8_t)(CAMPAIGN_VERSION >> 8);
    for (int i = 0; i < 6; ++i) bytes[6 + i] = campaign->cleared[i];
    bytes[12] = campaign->finalCleared;
    for (int i = 0; i < 3; ++i) bytes[13 + i] = campaign->endingSeen[i];
    for (int i = 0; i < 6; ++i) bytes[16 + i] = campaign->bestFloor[i];
    Put32(bytes + 22, ChecksumN(bytes, 22));
}

void InitCampaign(CampaignState* campaign) {
    ZeroMemory(campaign, sizeof(*campaign));
    campaign->magic = CAMPAIGN_MAGIC;
    campaign->version = CAMPAIGN_VERSION;
    uint8_t bytes[SAVE_SIZE];
    Encode(campaign, bytes);
    campaign->checksum = Get32(bytes + 22);
}

uint8_t CampaignClearedMask(const CampaignState* campaign) {
    uint8_t mask = 0;
    for (int i = 0; i < 6; ++i) if (campaign->cleared[i]) mask |= (uint8_t)(1u << i);
    return mask;
}

bool RecordCampaignClears(CampaignState* campaign, uint8_t clearedMask) {
    bool changed = false;
    for (int i = 0; i < 6; ++i) if ((clearedMask & (1u << i)) && !campaign->cleared[i]) {
        campaign->cleared[i] = 1;
        changed = true;
    }
    return changed;
}

uint8_t CampaignSeenEndingMask(const CampaignState* campaign) {
    uint8_t mask = 0;
    for (int i = 0; i < 3; ++i) if (campaign->endingSeen[i]) mask |= (uint8_t)(1u << i);
    return mask;
}

bool RecordCampaignEnding(CampaignState* campaign, int ending) {
    if (ending < 0 || ending >= 3 || campaign->endingSeen[ending]) return false;
    campaign->endingSeen[ending] = 1;
    return true;
}

bool RecordCampaignReach(CampaignState* campaign, int drive, int floor) {
    if (!campaign || drive < 0 || drive >= 6) return false;
    int reached = floor + 1;
    if (reached < 1) reached = 1;
    if (reached > 3) reached = 3;
    if (campaign->bestFloor[drive] >= reached) return false;
    campaign->bestFloor[drive] = (uint8_t)reached;
    return true;
}

static bool BesideExecutable(wchar_t* path, const wchar_t* overridePath, const wchar_t* name) {
    if (overridePath) {
        DWORD length = GetFullPathNameW(overridePath, MAX_PATH, path, 0);
        return length > 0 && length < MAX_PATH;
    }
    DWORD length = GetModuleFileNameW(0, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || (slash + 1 - path) + lstrlenW(name) + 1 > MAX_PATH) return false;
    lstrcpyW(slash + 1, name);
    return true;
}

static bool SavePath(wchar_t* path, const wchar_t* overridePath) {
    return BesideExecutable(path, overridePath, L"AROGUE.SAV");
}

bool LoadCampaign(CampaignState* campaign, const wchar_t* overridePath) {
    InitCampaign(campaign);
    wchar_t path[MAX_PATH];
    if (!SavePath(path, overridePath)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    uint8_t bytes[SAVE_SIZE + 1] = {0};
    DWORD read = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes), &read, 0) != 0;
    CloseHandle(file);
    if (!ok || Get32(bytes) != CAMPAIGN_MAGIC) return false;
    uint16_t version = (uint16_t)(bytes[4] | ((uint16_t)bytes[5] << 8));
    if (version == 1) {
        if (read != SAVE_V1_SIZE || Get32(bytes + 16) != ChecksumN(bytes, 16)) return false;
        for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
        for (int i = 0; i < 6; ++i) { campaign->cleared[i] = bytes[6+i]; campaign->bestFloor[i] = bytes[6+i] ? 3 : 0; }
        campaign->finalCleared = bytes[12];
        for (int i = 0; i < 3; ++i) campaign->endingSeen[i] = bytes[13+i];
        campaign->version = CAMPAIGN_VERSION;
        return true;
    }
    if (version != CAMPAIGN_VERSION || read != SAVE_SIZE || Get32(bytes + 22) != ChecksumN(bytes, 22)) return false;
    for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
    for (int i = 0; i < 6; ++i) if (bytes[16+i] > 3) return false;
    for (int i = 0; i < 6; ++i) campaign->cleared[i] = bytes[6+i];
    campaign->finalCleared = bytes[12];
    for (int i = 0; i < 3; ++i) campaign->endingSeen[i] = bytes[13+i];
    for (int i = 0; i < 6; ++i) campaign->bestFloor[i] = bytes[16+i];
    campaign->checksum = Get32(bytes + 22);
    return true;
}

// Write through a sibling temporary file and rename over the target, so a
// failure at any point leaves the previous file intact.
static bool WriteFileAtomically(const wchar_t* path, const uint8_t* bytes, DWORD size) {
    wchar_t directory[MAX_PATH], temporary[MAX_PATH];
    lstrcpyW(directory, path);
    wchar_t* slash = wcsrchr(directory, L'\\');
    if (!slash) return false;
    slash[1] = 0;
    // Use the destination directory so replacement stays on the same volume.
    if (!GetTempFileNameW(directory, L"ARG", 0, temporary)) return false;
    HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, 0, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    DWORD written = 0;
    bool ok = false;
    if (file != INVALID_HANDLE_VALUE) {
        ok = WriteFile(file, bytes, size, &written, 0) && written == size;
        if (ok) ok = FlushFileBuffers(file) != 0;
        if (!CloseHandle(file)) ok = false;
    }
    if (ok) ok = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) { DeleteFileW(temporary); return false; }
    return true;
}

bool SaveCampaign(CampaignState* campaign, const wchar_t* overridePath) {
    uint8_t bytes[SAVE_SIZE];
    Encode(campaign, bytes);
    for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
    for (int i = 0; i < 6; ++i) if (bytes[16+i] > 3) return false;
    wchar_t path[MAX_PATH];
    if (!SavePath(path, overridePath)) return false;
    if (!WriteFileAtomically(path, bytes, SAVE_SIZE)) return false;
    campaign->magic = CAMPAIGN_MAGIC;
    campaign->version = CAMPAIGN_VERSION;
    campaign->checksum = Get32(bytes + 22);
    return true;
}

// ---- Preferences ----------------------------------------------------------
// Same shape as the campaign save: magic, version, payload, FNV checksum. The
// two files stay independent, so wiping progress keeps the environment and a
// damaged AROGUE.CFG never costs anyone their recovered shards.
static const uint32_t SETTINGS_MAGIC = 0x47464341u; // On disk: ACFG
static const uint16_t SETTINGS_VERSION = 2;
static const DWORD SETTINGS_V1_SIZE = 16;
static const DWORD SETTINGS_SIZE = 18;

static uint32_t SettingsChecksum(const uint8_t* bytes, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

void InitSettings(UserSettings* settings) {
    if (!settings) return;
    settings->language = 0;
    settings->scalePercent = 100;
    settings->fullscreen = 0;
    settings->fxLevel = 0;
    settings->musicEnabled = 1;
    settings->volume = 100;
    settings->bgmVolume = 100;
    settings->sfxVolume = 100;
}

bool LoadSettings(UserSettings* settings, const wchar_t* overridePath) {
    if (!settings) return false;
    InitSettings(settings);
    wchar_t path[MAX_PATH];
    if (!BesideExecutable(path, overridePath, L"AROGUE.CFG")) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    uint8_t bytes[SETTINGS_SIZE + 1] = {0};
    DWORD read = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes), &read, 0) != 0;
    CloseHandle(file);
    if (!ok || Get32(bytes) != SETTINGS_MAGIC) { InitSettings(settings); return false; }
    uint16_t version = bytes[4] | ((uint16_t)bytes[5] << 8);
    if (version == 1) {
        if (read != SETTINGS_V1_SIZE || Get32(bytes + 12) != SettingsChecksum(bytes, 12)) { InitSettings(settings); return false; }
    } else if (version == SETTINGS_VERSION) {
        if (read != SETTINGS_SIZE || Get32(bytes + 14) != SettingsChecksum(bytes, 14)) { InitSettings(settings); return false; }
    } else { InitSettings(settings); return false; }
    settings->language = bytes[6];
    settings->scalePercent = bytes[7];
    settings->fullscreen = bytes[8] ? 1 : 0;
    settings->fxLevel = bytes[9];
    settings->musicEnabled = bytes[10] ? 1 : 0;
    settings->volume = bytes[11] > 100 ? 100 : bytes[11];
    if (version >= 2) {
        settings->bgmVolume = bytes[12] > 100 ? 100 : bytes[12];
        settings->sfxVolume = bytes[13] > 100 ? 100 : bytes[13];
    }
    return true;
}

bool SaveSettings(const UserSettings* settings, const wchar_t* overridePath) {
    if (!settings) return false;
    wchar_t path[MAX_PATH];
    if (!BesideExecutable(path, overridePath, L"AROGUE.CFG")) return false;
    uint8_t bytes[SETTINGS_SIZE] = {0};
    Put32(bytes, SETTINGS_MAGIC);
    bytes[4] = (uint8_t)SETTINGS_VERSION;
    bytes[5] = (uint8_t)(SETTINGS_VERSION >> 8);
    bytes[6] = settings->language;
    bytes[7] = settings->scalePercent;
    bytes[8] = settings->fullscreen ? 1 : 0;
    bytes[9] = settings->fxLevel;
    bytes[10] = settings->musicEnabled ? 1 : 0;
    bytes[11] = settings->volume > 100 ? 100 : settings->volume;
    bytes[12] = settings->bgmVolume > 100 ? 100 : settings->bgmVolume;
    bytes[13] = settings->sfxVolume > 100 ? 100 : settings->sfxVolume;
    Put32(bytes + 14, SettingsChecksum(bytes, 14));
    return WriteFileAtomically(path, bytes, SETTINGS_SIZE);
}


// ---- Persistent codex -----------------------------------------------------
// Fixed 32-byte bitset supports up to 256 enemy kinds without tying this file
// to game.h. Resetting AROGUE.SAV deliberately leaves AROGUE.CDX intact.
static const uint32_t CODEX_MAGIC = 0x58444341u; // On disk: ACDX
static const uint16_t CODEX_VERSION = 1;
static const DWORD CODEX_SIZE = 44;

static uint32_t CodexChecksum(const uint8_t* bytes) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 40; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

bool LoadCodex(uint8_t* scanned, int count, const wchar_t* overridePath) {
    if (!scanned || count < 0 || count > 256) return false;
    ZeroMemory(scanned, count);
    wchar_t path[MAX_PATH];
    if (!BesideExecutable(path, overridePath, L"AROGUE.CDX")) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    uint8_t bytes[CODEX_SIZE + 1] = {0}; DWORD got = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes), &got, 0) != 0;
    CloseHandle(file);
    uint16_t version = bytes[4] | ((uint16_t)bytes[5] << 8);
    uint16_t savedCount = bytes[6] | ((uint16_t)bytes[7] << 8);
    if (!ok || got != CODEX_SIZE || Get32(bytes) != CODEX_MAGIC || version != CODEX_VERSION
        || savedCount != count || Get32(bytes + 40) != CodexChecksum(bytes)) return false;
    for (int i = 0; i < count; ++i) scanned[i] = (uint8_t)((bytes[8 + i / 8] >> (i & 7)) & 1u);
    return true;
}

bool SaveCodex(const uint8_t* scanned, int count, const wchar_t* overridePath) {
    if (!scanned || count < 0 || count > 256) return false;
    uint8_t bytes[CODEX_SIZE] = {0};
    Put32(bytes, CODEX_MAGIC); bytes[4] = (uint8_t)CODEX_VERSION; bytes[5] = (uint8_t)(CODEX_VERSION >> 8);
    bytes[6] = (uint8_t)count; bytes[7] = (uint8_t)(count >> 8);
    for (int i = 0; i < count; ++i) if (scanned[i]) bytes[8 + i / 8] |= (uint8_t)(1u << (i & 7));
    Put32(bytes + 40, CodexChecksum(bytes));
    wchar_t path[MAX_PATH];
    return BesideExecutable(path, overridePath, L"AROGUE.CDX") && WriteFileAtomically(path, bytes, CODEX_SIZE);
}
