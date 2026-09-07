#include <windows.h>
#include <wchar.h>
#include "campaign.h"

static const uint32_t CAMPAIGN_MAGIC = 0x474F5241u; // On disk: AROG
static const uint16_t CAMPAIGN_VERSION = 1;
static const DWORD SAVE_SIZE = 20;

// Explicit little-endian encoding keeps compiler padding out of the save format.
static void Put32(uint8_t* bytes, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[i] = (uint8_t)(value >> (i * 8));
}

static uint32_t Get32(const uint8_t* bytes) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= (uint32_t)bytes[i] << (i * 8);
    return value;
}

static uint32_t Checksum(const uint8_t* bytes) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 16; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static void Encode(const CampaignState* campaign, uint8_t* bytes) {
    Put32(bytes, CAMPAIGN_MAGIC);
    bytes[4] = (uint8_t)CAMPAIGN_VERSION;
    bytes[5] = (uint8_t)(CAMPAIGN_VERSION >> 8);
    for (int i = 0; i < 6; ++i) bytes[6 + i] = campaign->cleared[i];
    bytes[12] = campaign->finalCleared;
    for (int i = 0; i < 3; ++i) bytes[13 + i] = campaign->endingSeen[i];
    Put32(bytes + 16, Checksum(bytes));
}

void InitCampaign(CampaignState* campaign) {
    ZeroMemory(campaign, sizeof(*campaign));
    campaign->magic = CAMPAIGN_MAGIC;
    campaign->version = CAMPAIGN_VERSION;
    uint8_t bytes[SAVE_SIZE];
    Encode(campaign, bytes);
    campaign->checksum = Get32(bytes + 16);
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

static bool SavePath(wchar_t* path, const wchar_t* overridePath) {
    if (overridePath) {
        DWORD length = GetFullPathNameW(overridePath, MAX_PATH, path, 0);
        return length > 0 && length < MAX_PATH;
    }
    DWORD length = GetModuleFileNameW(0, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || (slash + 1 - path) + 11 > MAX_PATH) return false;
    lstrcpyW(slash + 1, L"AROGUE.SAV");
    return true;
}

bool LoadCampaign(CampaignState* campaign, const wchar_t* overridePath) {
    InitCampaign(campaign);
    wchar_t path[MAX_PATH];
    if (!SavePath(path, overridePath)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    uint8_t bytes[SAVE_SIZE + 1];
    DWORD read = 0;
    bool ok = ReadFile(file, bytes, sizeof(bytes), &read, 0) != 0;
    CloseHandle(file);
    if (!ok || read != SAVE_SIZE || Get32(bytes) != CAMPAIGN_MAGIC
        || (bytes[4] | ((uint16_t)bytes[5] << 8)) != CAMPAIGN_VERSION
        || Get32(bytes + 16) != Checksum(bytes)) return false;
    for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
    for (int i = 0; i < 6; ++i) campaign->cleared[i] = bytes[6 + i];
    campaign->finalCleared = bytes[12];
    for (int i = 0; i < 3; ++i) campaign->endingSeen[i] = bytes[13 + i];
    campaign->checksum = Get32(bytes + 16);
    return true;
}

bool SaveCampaign(CampaignState* campaign, const wchar_t* overridePath) {
    uint8_t bytes[SAVE_SIZE];
    Encode(campaign, bytes);
    for (int i = 6; i < 16; ++i) if (bytes[i] > 1) return false;
    wchar_t path[MAX_PATH], directory[MAX_PATH], temporary[MAX_PATH];
    if (!SavePath(path, overridePath)) return false;
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
        ok = WriteFile(file, bytes, SAVE_SIZE, &written, 0) && written == SAVE_SIZE;
        if (ok) ok = FlushFileBuffers(file) != 0;
        if (!CloseHandle(file)) ok = false;
    }
    if (ok) ok = MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) { DeleteFileW(temporary); return false; }
    campaign->magic = CAMPAIGN_MAGIC;
    campaign->version = CAMPAIGN_VERSION;
    campaign->checksum = Get32(bytes + 16);
    return true;
}
