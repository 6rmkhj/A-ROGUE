from pathlib import Path
p = Path('src/smoke.cpp')
s = p.read_text(encoding='utf-8')
start_marker = '    uint8_t bytes[21] = {};\n'
end_marker = '    if (!SaveCampaign(&state, file.path) || !SetFileAttributesW(file.path, FILE_ATTRIBUTE_READONLY))\n'
start = s.find(start_marker)
end = s.find(end_marker, start)
if start < 0 or end < 0:
    raise RuntimeError('campaign storage format test block not found')
replacement = r'''    uint8_t bytes[27] = {};
    HANDLE handle = CreateFileW(file.path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    DWORD read = 0;
    bool readOk = handle != INVALID_HANDLE_VALUE && ReadFile(handle, bytes, sizeof(bytes), &read, 0);
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    if (!readOk || read != 26 || memcmp(bytes, "AROG\x02\x00", 6)) return Fail("campaign stable 26-byte v2 format");

    // Every v2 byte, including the new best-floor progress and checksum, fails closed.
    for (int i = 0; i < 26; ++i) {
        bytes[i] ^= 0x80;
        if (!WriteCampaignFixture(file.path, bytes, 26)) return Fail("campaign v2 corruption fixture");
        loaded = state;
        if (LoadCampaign(&loaded, file.path) || memcmp(&loaded, &fresh, sizeof(fresh)))
            return Fail("corrupt v2 campaign must reset safely");
        bytes[i] ^= 0x80;
    }
    for (DWORD size = 0; size <= 27; ++size) {
        if (size == 26) continue;
        if (!WriteCampaignFixture(file.path, bytes, size)) return Fail("campaign v2 size fixture");
        loaded = state;
        if (LoadCampaign(&loaded, file.path) || memcmp(&loaded, &fresh, sizeof(fresh)))
            return Fail("truncated or oversized v2 campaign must reset safely");
    }

    // Recompute a valid v2 checksum while making individual fields invalid.
    const int invalidOffsets[] = {0, 4, 6, 12, 13, 16};
    const int invalidValues[]  = {2, 9, 2, 2, 2, 4};
    for (int i = 0; i < 6; ++i) {
        uint8_t invalid[26]; memcpy(invalid, bytes, 26);
        invalid[invalidOffsets[i]] = (uint8_t)invalidValues[i];
        uint32_t checksum = 2166136261u;
        for (int b = 0; b < 22; ++b) checksum = (checksum ^ invalid[b]) * 16777619u;
        for (int b = 0; b < 4; ++b) invalid[22 + b] = (uint8_t)(checksum >> (8 * b));
        if (!WriteCampaignFixture(file.path, invalid, 26)) return Fail("campaign v2 invalid field fixture");
        loaded = state;
        if (LoadCampaign(&loaded, file.path) || memcmp(&loaded, &fresh, sizeof(fresh)))
            return Fail("invalid v2 campaign fields must reset even with a valid checksum");
    }

    // Existing 20-byte v1 saves migrate without losing cleared/final/ending state.
    uint8_t v1[20] = {'A','R','O','G',1,0, 1,0,1,0,0,0, 1, 1,0,1, 0,0,0,0};
    uint32_t v1sum = 2166136261u;
    for (int i = 0; i < 16; ++i) v1sum = (v1sum ^ v1[i]) * 16777619u;
    for (int i = 0; i < 4; ++i) v1[16+i] = (uint8_t)(v1sum >> (8*i));
    if (!WriteCampaignFixture(file.path, v1, 20) || !LoadCampaign(&loaded, file.path))
        return Fail("valid v1 campaign must migrate");
    if (CampaignClearedMask(&loaded) != 0x05 || !loaded.finalCleared
        || !loaded.endingSeen[0] || loaded.endingSeen[1] || !loaded.endingSeen[2]
        || loaded.bestFloor[0] != 3 || loaded.bestFloor[2] != 3
        || loaded.bestFloor[1] != 0 || loaded.version != 2)
        return Fail("v1 campaign migration must preserve legacy state and seed best-floor records");

    // Restore the v2 snapshot before failed-write preservation checks below.
    if (!SaveCampaign(&before, file.path)) return Fail("restore v2 campaign fixture");
'''
s = s[:start] + replacement + s[end:]
p.write_text(s, encoding='utf-8', newline='\n')
print('campaign v2 storage regression coverage applied')
