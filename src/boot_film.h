#pragma once

#include <windows.h>

// Draw a silent, time-indexed film into the caller's logical canvas. The UI
// thread owns this cache; false leaves the caller free to draw the native intro.
// Both motion modes use the same shake-free footage; live overlays set intensity.
bool DrawBootFilm(HDC dc, int width, int height, int tMs, bool reduced);
void DestroyBootFilm();
