#pragma once

#include <stdint.h>

// Campaign lifetime is independent of GameState and NewRun's memory reset.
struct CampaignState {
    uint32_t magic;
    uint16_t version;
    uint8_t cleared[6];
    uint8_t finalCleared;
    uint8_t endingSeen[3];
    uint32_t checksum;
};

void InitCampaign(CampaignState* campaign);
uint8_t CampaignClearedMask(const CampaignState* campaign);
// Merge completed regular volumes without changing final-volume/ending flags.
bool RecordCampaignClears(CampaignState* campaign, uint8_t clearedMask);
// Bit i is set when ending i has been committed at least once.
uint8_t CampaignSeenEndingMask(const CampaignState* campaign);
// Out-of-range endings are ignored and report no change.
bool RecordCampaignEnding(CampaignState* campaign, int ending);

// Null path means AROGUE.SAV beside the executable, never the working directory.
// Load failure resets to a fresh campaign. Failures are silent and return false.
// Save refreshes metadata on success, and preserves the old file on failure.
bool LoadCampaign(CampaignState* campaign, const wchar_t* path = 0);
bool SaveCampaign(CampaignState* campaign, const wchar_t* path = 0);

// ---------------------------------------------------------------------------
// Preferences: language, window scale, effect level, music and volume. Kept out
// of CampaignState because "진행도 초기화" must wipe progress without resetting
// the player's environment, and because a corrupt preferences file must never
// cost anyone their recovered shards.
// ---------------------------------------------------------------------------
struct UserSettings {
    uint8_t language;      // UiLanguage enum
    uint8_t scalePercent;  // one of ui.h's SCALE_OPTIONS; 0 means "leave as built"
    uint8_t fullscreen;
    uint8_t fxLevel;       // FxLevel enum
    uint8_t musicEnabled;
    uint8_t volume;        // 0-100
};

// Defaults match a fresh install: Korean, 100%, windowed, full effects, BGM on.
void InitSettings(UserSettings* settings);
// Null path means AROGUE.CFG beside the executable. A missing, short, or
// corrupt file leaves defaults in place and returns false; out-of-range values
// are clamped rather than rejected so one bad field cannot drop the rest.
bool LoadSettings(UserSettings* settings, const wchar_t* path = 0);
bool SaveSettings(const UserSettings* settings, const wchar_t* path = 0);
