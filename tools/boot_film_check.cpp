// Real-resource integration test. Its reference pixels come directly from the
// indexed JPEG, not from the production time-selection or rendering functions.
#include "../src/boot_film.h"
#include <wincodec.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <new>

static uint32_t ReadU32(const BYTE* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

struct FilmReference {
    const BYTE* bytes;
    DWORD size;
    uint32_t width, height, fps, count;
    IWICImagingFactory* factory;
    BYTE* pixels;
    bool ownsCom;
    FilmReference() : bytes(0), size(0), width(0), height(0), fps(0), count(0),
        factory(0), pixels(0), ownsCom(false) {}
    ~FilmReference() {
        delete[] pixels;
        if (factory) factory->Release();
        if (ownsCom) CoUninitialize();
    }
    bool Open() {
        HMODULE module = GetModuleHandleW(0);
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(201), RT_RCDATA);
        if (!resource) return false;
        size = SizeofResource(module, resource);
        HGLOBAL loaded = LoadResource(module, resource);
        bytes = loaded ? (const BYTE*)LockResource(loaded) : 0;
        if (!bytes || size < 24 || ReadU32(bytes) != 0x46565241u || ReadU32(bytes + 4) != 1) return false;
        width = ReadU32(bytes + 8); height = ReadU32(bytes + 12);
        fps = ReadU32(bytes + 16); count = ReadU32(bytes + 20);
        if (!width || width > 4096 || !height || height > 2160 || !fps || fps > 120
            || !count || count > 3600 || 24u + count * 8u > size) return false;
        HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
        ownsCom = SUCCEEDED(hr);
        hr = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) return false;
        pixels = new (std::nothrow) BYTE[(size_t)width * height * 4];
        return pixels != 0;
    }
    bool Decode(uint32_t index) {
        if (index >= count) return false;
        const BYTE* entry = bytes + 24 + index * 8;
        uint32_t offset = ReadU32(entry), length = ReadU32(entry + 4);
        if (offset < 24u + count * 8u || !length || (uint64_t)offset + length > size) return false;
        IWICStream* stream = 0;
        IWICBitmapDecoder* decoder = 0;
        IWICBitmapFrameDecode* frame = 0;
        IWICBitmapSource* source = 0;
        HRESULT hr = factory->CreateStream(&stream);
        if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(const_cast<BYTE*>(bytes + offset), length);
        if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream, 0, WICDecodeMetadataCacheOnLoad, &decoder);
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr)) hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGR, frame, &source);
        UINT actualWidth = 0, actualHeight = 0;
        if (SUCCEEDED(hr)) hr = source->GetSize(&actualWidth, &actualHeight);
        if (SUCCEEDED(hr) && (actualWidth != width || actualHeight != height)) hr = E_INVALIDARG;
        if (SUCCEEDED(hr)) hr = source->CopyPixels(0, width * 4, width * height * 4, pixels);
        if (source) source->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        return SUCCEEDED(hr);
    }
};

struct Canvas {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ oldBitmap;
    BYTE* bits;
    Canvas() : dc(0), bitmap(0), oldBitmap(0), bits(0) {}
    ~Canvas() {
        if (dc && oldBitmap) SelectObject(dc, oldBitmap);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }
    bool Open(int width, int height) {
        dc = CreateCompatibleDC(0);
        if (!dc) return false;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* address = 0;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &address, 0, 0);
        if (!bitmap) return false;
        bits = (BYTE*)address;
        oldBitmap = SelectObject(dc, bitmap);
        // Non-default mapping and brush origin expose leaked GDI state without
        // changing the source-to-destination pixel mapping of this oracle test.
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, width, height, 0);
        SetViewportExtEx(dc, width, height, 0);
        SetStretchBltMode(dc, COLORONCOLOR);
        SetBrushOrgEx(dc, 7, 11, 0);
        IntersectClipRect(dc, 0, 0, width, height);
        return true;
    }
};

static bool SameBgr(const BYTE* a, const BYTE* b, uint32_t pixels) {
    // The high byte in a BI_RGB DIB is reserved, not an alpha-channel contract.
    for (uint32_t i = 0; i < pixels; ++i)
        if (a[i * 4] != b[i * 4] || a[i * 4 + 1] != b[i * 4 + 1]
            || a[i * 4 + 2] != b[i * 4 + 2]) return false;
    return true;
}

static int CheckFrame(FilmReference& film, Canvas& canvas, int age, uint32_t expected, bool reduced) {
    if (!film.Decode(expected)) { printf("FAIL: reference JPEG %u cannot decode\n", expected); return 1; }
    RECT area = {0, 0, (LONG)film.width, (LONG)film.height};
    FillRect(canvas.dc, &area, (HBRUSH)GetStockObject(WHITE_BRUSH));
    HGDIOBJ bitmap = GetCurrentObject(canvas.dc, OBJ_BITMAP);
    HGDIOBJ brush = GetCurrentObject(canvas.dc, OBJ_BRUSH);
    HGDIOBJ pen = GetCurrentObject(canvas.dc, OBJ_PEN);
    int mapping = GetMapMode(canvas.dc), stretch = GetStretchBltMode(canvas.dc);
    POINT origin, viewportOrigin, windowOrigin;
    SIZE viewport, window;
    GetBrushOrgEx(canvas.dc, &origin); GetViewportOrgEx(canvas.dc, &viewportOrigin);
    GetWindowOrgEx(canvas.dc, &windowOrigin); GetViewportExtEx(canvas.dc, &viewport);
    GetWindowExtEx(canvas.dc, &window);
    RECT clip; int clipKind = GetClipBox(canvas.dc, &clip);
    if (!DrawBootFilm(canvas.dc, (int)film.width, (int)film.height, age, reduced)) {
        printf("FAIL: DrawBootFilm returned fallback at %d ms (frame %u)\n", age, expected); return 1;
    }
    GdiFlush();
    if (!SameBgr(canvas.bits, film.pixels, film.width * film.height)) {
        printf("FAIL: %d ms selected wrong pixels; expected frame %u, reduced=%d\n", age, expected, reduced); return 1;
    }
    POINT afterOrigin, afterViewportOrigin, afterWindowOrigin;
    SIZE afterViewport, afterWindow;
    RECT afterClip;
    GetBrushOrgEx(canvas.dc, &afterOrigin); GetViewportOrgEx(canvas.dc, &afterViewportOrigin);
    GetWindowOrgEx(canvas.dc, &afterWindowOrigin); GetViewportExtEx(canvas.dc, &afterViewport);
    GetWindowExtEx(canvas.dc, &afterWindow);
    if (GetCurrentObject(canvas.dc, OBJ_BITMAP) != bitmap || GetCurrentObject(canvas.dc, OBJ_BRUSH) != brush
        || GetCurrentObject(canvas.dc, OBJ_PEN) != pen || GetMapMode(canvas.dc) != mapping
        || GetStretchBltMode(canvas.dc) != stretch || origin.x != afterOrigin.x || origin.y != afterOrigin.y
        || viewportOrigin.x != afterViewportOrigin.x || viewportOrigin.y != afterViewportOrigin.y
        || windowOrigin.x != afterWindowOrigin.x || windowOrigin.y != afterWindowOrigin.y
        || viewport.cx != afterViewport.cx || viewport.cy != afterViewport.cy
        || window.cx != afterWindow.cx || window.cy != afterWindow.cy
        || GetClipBox(canvas.dc, &afterClip) != clipKind || !EqualRect(&clip, &afterClip)) {
        puts("FAIL: film renderer changed caller DC state"); return 1;
    }
    return 0;
}

static bool TimeBootDraw(Canvas& canvas, int age, const LARGE_INTEGER& frequency, double* ms) {
    LARGE_INTEGER begin, end;
    QueryPerformanceCounter(&begin);
    bool drawn = DrawBootFilm(canvas.dc, 1352, 760, age, false);
    BOOL flushed = GdiFlush();
    QueryPerformanceCounter(&end);
    *ms = (double)(end.QuadPart - begin.QuadPart) * 1000.0 / (double)frequency.QuadPart;
    return drawn && flushed;
}

static int CompareTiming(const void* left, const void* right) {
    double a = *(const double*)left, b = *(const double*)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static bool CheckScaledFrame(FilmReference& film, int scale, int age, uint32_t index, bool reduced) {
    const int width = 1352 * scale, height = 760 * scale;
    Canvas actual, expected;
    if (!actual.Open(width, height) || !expected.Open(width, height) || !film.Decode(index)) return false;
    SetWindowExtEx(actual.dc, 1352, 760, 0);
    SetViewportExtEx(actual.dc, width, height, 0);
    // Scale already-decoded reference pixels independently of the production
    // decoder pipeline. Matching output proves that cached physical pixels are
    // copied 1:1, rather than scaled a second time by the anisotropic canvas DC.
    IWICBitmap* bitmap = 0;
    IWICBitmapScaler* scaler = 0;
    HRESULT hr = film.factory->CreateBitmapFromMemory(film.width, film.height,
        GUID_WICPixelFormat32bppBGR, film.width * 4, film.width * film.height * 4, film.pixels, &bitmap);
    if (SUCCEEDED(hr)) hr = film.factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(bitmap, width, height, WICBitmapInterpolationModeHighQualityCubic);
    if (SUCCEEDED(hr)) hr = scaler->CopyPixels(0, width * 4, width * height * 4, expected.bits);
    if (scaler) scaler->Release();
    if (bitmap) bitmap->Release();
    if (FAILED(hr) || !DrawBootFilm(actual.dc, 1352, 760, age, reduced)) return false;
    GdiFlush();
    SIZE window, viewport;
    POINT brush;
    GetWindowExtEx(actual.dc, &window); GetViewportExtEx(actual.dc, &viewport);
    GetBrushOrgEx(actual.dc, &brush);
    return SameBgr(actual.bits, expected.bits, width * height)
        && window.cx == 1352 && window.cy == 760 && viewport.cx == width && viewport.cy == height
        && GetMapMode(actual.dc) == MM_ANISOTROPIC && GetStretchBltMode(actual.dc) == COLORONCOLOR
        && brush.x == 7 && brush.y == 11;
}

int main() {
    Canvas playback;
    LARGE_INTEGER frequency;
    if (!playback.Open(1352, 760) || !QueryPerformanceFrequency(&frequency)) {
        puts("FAIL: performance canvas or clock initialization"); return 10;
    }
    // This runs before the reference WIC factory exists: include the production
    // factory initialization, resource validation, allocation, first decode and
    // scaled GDI draw in the cold sample. No oracle work is timed.
    double coldMs = 0;
    if (!TimeBootDraw(playback, 0, frequency, &coldMs)) {
        puts("FAIL: cold embedded film draw returned fallback"); return 11;
    }
    DestroyBootFilm();
    FilmReference film;
    if (!film.Open()) { puts("FAIL: embedded ARVF resource 201 is missing, invalid, or WIC is unavailable"); return 1; }
    Canvas canvas;
    if (!canvas.Open((int)film.width, (int)film.height)) { puts("FAIL: test canvas allocation"); return 2; }
    if (CheckFrame(film, canvas, 0, 0, false)) return 3;
    DestroyBootFilm();
    DWORD baseline = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    int checks = 0;
    // Exercise every packaged JPEG and both sides of each timestamp boundary.
    // This also detects a broken later frame that would otherwise silently hold
    // an earlier valid cache through the remainder of the intro.
    for (uint32_t i = 0; i < film.count; ++i) {
        int at = (int)(((uint64_t)i * 1000 + film.fps - 1) / film.fps);
        if (CheckFrame(film, canvas, at, i, false)) return 4;
        ++checks;
        if (i && CheckFrame(film, canvas, at - 1, i - 1, false)) return 5;
        if (i) ++checks;
        if (CheckFrame(film, canvas, at, i, false)) return 6;
        ++checks;
    }
    uint32_t samples[] = {0, film.count / 2, film.count - 1};
    for (int cycle = 0; cycle < 3; ++cycle) {
        DestroyBootFilm();
        for (int mode = 0; mode < 2; ++mode) {
            for (int s = 0; s < 3; ++s) {
                uint32_t i = samples[s];
                int at = (int)(((uint64_t)i * 1000 + film.fps - 1) / film.fps);
                if (CheckFrame(film, canvas, at, i, mode != 0)) return 7;
                ++checks;
            }
            int afterEnd = (int)(((uint64_t)film.count * 1000 + film.fps - 1) / film.fps);
            if (CheckFrame(film, canvas, -100, 0, mode != 0)
                || CheckFrame(film, canvas, afterEnd, film.count - 1, mode != 0)
                || CheckFrame(film, canvas, INT_MAX, film.count - 1, mode != 0)) return 8;
            checks += 3;
        }
        DestroyBootFilm();
        if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) != baseline) {
            puts("FAIL: film reinitialization leaked GDI objects"); return 9;
        }
    }
    for (int mode = 0; mode < 2; ++mode) {
        uint32_t frame = film.count / 2;
        int at = (int)(((uint64_t)frame * 1000 + film.fps - 1) / film.fps);
        // Do not destroy the film between these draws: its frame number stays
        // fixed while viewport dimensions expand, shrink, then return to native.
        if (CheckFrame(film, canvas, at, frame, mode != 0)
            || !CheckScaledFrame(film, 1, at, frame, mode != 0)
            || !CheckScaledFrame(film, 2, at, frame, mode != 0)
            || !CheckScaledFrame(film, 1, at, frame, mode != 0)
            || CheckFrame(film, canvas, at, frame, mode != 0)) {
            puts("FAIL: viewport resize invalidation or high-quality scaled pixels"); return 15;
        }
        checks += 5;
    }
    DestroyBootFilm();
    if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) != baseline) return 16;
    // Keep all reference decoding and pixel assertions outside this hot pass.
    // Prime the final frame so frame zero also needs a real decode, as do all
    // subsequent frames; cached repeats cannot flatter the playback timings.
    int lastAge = (int)(((uint64_t)(film.count - 1) * 1000 + film.fps - 1) / film.fps);
    if (!DrawBootFilm(playback.dc, 1352, 760, lastAge, false) || !GdiFlush()) return 12;
    double timings[3600], sum = 0;
    for (uint32_t i = 0; i < film.count; ++i) {
        int at = (int)(((uint64_t)i * 1000 + film.fps - 1) / film.fps);
        if (!TimeBootDraw(playback, at, frequency, &timings[i])) return 13;
        sum += timings[i];
    }
    DestroyBootFilm();
    if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) != baseline) {
        puts("FAIL: timed playback leaked GDI objects"); return 14;
    }
    qsort(timings, film.count, sizeof(timings[0]), CompareTiming);
    uint32_t p95Index = (film.count * 95 + 99) / 100 - 1;
    printf("PASS: %u embedded JPEG frames, %ux%u at %u fps; %d pixel checks, frame boundaries, "
        "FULL/REDUCED, end clamp, 1x/2x viewport resizing, DC preservation, destroy/reinitialize, no GDI leaks\n",
        film.count, film.width, film.height, film.fps, checks);
    printf("TIMING (1352x760, DrawBootFilm + GdiFlush only): cold %.3f ms; "
        "%u sequential hot frames mean %.3f ms, p95 %.3f ms, max %.3f ms (informational)\n",
        coldMs, film.count, sum / film.count, timings[p95Index], timings[film.count - 1]);
    {
        Canvas large;
        if (!large.Open(2704, 1520)) return 17;
        SetWindowExtEx(large.dc, 1352, 760, 0);
        SetViewportExtEx(large.dc, 2704, 1520, 0);
        if (!DrawBootFilm(large.dc, 1352, 760, lastAge, false) || !GdiFlush()) return 18;
        sum = 0;
        for (uint32_t i = 0; i < film.count; ++i) {
            int at = (int)(((uint64_t)i * 1000 + film.fps - 1) / film.fps);
            if (!TimeBootDraw(large, at, frequency, &timings[i])) return 19;
            sum += timings[i];
        }
        qsort(timings, film.count, sizeof(timings[0]), CompareTiming);
        printf("TIMING (2704x1520 physical, 2x viewport): %u sequential hot frames "
            "mean %.3f ms, p95 %.3f ms, max %.3f ms (informational)\n",
            film.count, sum / film.count, timings[p95Index], timings[film.count - 1]);
        double cachedSum = 0;
        for (int i = 0; i < 120; ++i) {
            if (!TimeBootDraw(large, lastAge, frequency, &timings[i])) return 20;
            cachedSum += timings[i];
        }
        qsort(timings, 120, sizeof(timings[0]), CompareTiming);
        printf("TIMING (2704x1520 cached repeats): mean %.3f ms, p95 %.3f ms, max %.3f ms (informational)\n",
            cachedSum / 120, timings[113], timings[119]);
        DestroyBootFilm();
    }
    if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) != baseline) return 21;
    return 0;
}
