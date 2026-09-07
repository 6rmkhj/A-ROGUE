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
