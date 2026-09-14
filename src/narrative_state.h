#pragma once
#include <stdint.h>

// Shared, versioned campaign story progress. Runtime clocks never belong here.
#define NARRATIVE_NAME_MAX 16
struct NarrativeProgress {
    wchar_t playerName[NARRATIVE_NAME_MAX + 1];
    uint8_t introSeen;
    uint8_t tutorialSeen;
    uint8_t milestoneSeen; // bits 0..5: completed-volume relationship conversations
    uint8_t shardSeen;     // bits 0..5: acknowledged recovered-volume evidence
    uint32_t bossSeen;     // drive * 3 + floor, 21 bits
    uint32_t logsSeen;     // drive * 3 + floor, 21 bits
};
