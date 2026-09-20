#include "boot_film.h"

#include <wincodec.h>
#include <stdint.h>
#include <new>

// ARVF v1 is deliberately independent of C++ structure packing. Every integer
// is unsigned little-endian: magic 'ARVF', version, width, height, fps, count;
// then count pairs of absolute JPEG byte offset and byte length. The executable
// keeps compressed resource 201 mapped, and this cache owns one decoded frame.
static const uint32_t FILM_MAGIC = 0x46565241u;
static const uint32_t FILM_HEADER_SIZE = 24;
static const uint32_t FILM_INDEX_SIZE = 8;
static const uint32_t FILM_MAX_BYTES = 128u * 1024u * 1024u;
static const uint32_t FILM_MAX_PIXELS = 4096u * 2160u;

static const BYTE* gFilmData;
static uint32_t gFilmWidth, gFilmHeight, gFilmFps, gFilmCount;
static uint32_t gFilmCacheWidth, gFilmCacheHeight;
static uint32_t gFilmFailedWidth, gFilmFailedHeight;
static BYTE* gFilmPixels;
static IWICImagingFactory* gFilmFactory;
static bool gFilmTried, gFilmOwnsCom, gFilmFrozen;
static int gFilmFrame = -1;

static uint32_t FilmU32(const BYTE* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void ReleaseFilmStorage() {
    if (gFilmFactory) { gFilmFactory->Release(); gFilmFactory = 0; }
    delete[] gFilmPixels; gFilmPixels = 0;
    if (gFilmOwnsCom) { CoUninitialize(); gFilmOwnsCom = false; }
    gFilmData = 0;
    gFilmWidth = gFilmHeight = gFilmFps = gFilmCount = 0;
    gFilmCacheWidth = gFilmCacheHeight = gFilmFailedWidth = gFilmFailedHeight = 0;
    gFilmFrame = -1;
    gFilmFrozen = false;
}

void DestroyBootFilm() {
    ReleaseFilmStorage();
    gFilmTried = false;
}

static bool PrepareBootFilm() {
    if (gFilmTried) return gFilmData && gFilmFactory;
    gFilmTried = true;
    HMODULE module = GetModuleHandleW(0);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(201), RT_RCDATA);
    if (!resource) return false;
    DWORD bytes = SizeofResource(module, resource);
    if (bytes < FILM_HEADER_SIZE || bytes > FILM_MAX_BYTES) return false;
    HGLOBAL loaded = LoadResource(module, resource);
    const BYTE* data = loaded ? (const BYTE*)LockResource(loaded) : 0;
    if (!data || FilmU32(data) != FILM_MAGIC || FilmU32(data + 4) != 1) return false;
    uint32_t width = FilmU32(data + 8), height = FilmU32(data + 12);
    uint32_t fps = FilmU32(data + 16), count = FilmU32(data + 20);
    if (!width || width > 4096 || !height || height > 2160
        || (uint64_t)width * height > FILM_MAX_PIXELS
        || !fps || fps > 120 || !count || count > 3600) return false;
    uint64_t tableEnd = FILM_HEADER_SIZE + (uint64_t)count * FILM_INDEX_SIZE;
    if (tableEnd > bytes) return false;
    uint64_t previousEnd = tableEnd;
    for (uint32_t i = 0; i < count; ++i) {
        const BYTE* entry = data + FILM_HEADER_SIZE + i * FILM_INDEX_SIZE;
        uint32_t offset = FilmU32(entry), size = FilmU32(entry + 4);
        uint64_t end = (uint64_t)offset + size;
        if (offset < previousEnd || size < 4 || end > bytes) return false;
        // WIC also checks the entire bitstream. These markers reject wrong
        // formats without invoking a decoder on arbitrary resource contents.
        if (data[offset] != 0xff || data[offset + 1] != 0xd8
            || data[end - 2] != 0xff || data[end - 1] != 0xd9) return false;
        previousEnd = end;
    }
    HRESULT com = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return false;
    gFilmOwnsCom = SUCCEEDED(com);
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&gFilmFactory));
    if (FAILED(hr)) { ReleaseFilmStorage(); return false; }
    gFilmData = data;
    gFilmWidth = width; gFilmHeight = height; gFilmFps = fps; gFilmCount = count;
    return true;
}

static bool DecodeFilmFrame(int index, BYTE* pixels, uint32_t outputWidth, uint32_t outputHeight) {
    const BYTE* entry = gFilmData + FILM_HEADER_SIZE + (uint32_t)index * FILM_INDEX_SIZE;
    uint32_t offset = FilmU32(entry), size = FilmU32(entry + 4);
    IWICStream* stream = 0;
    IWICBitmapDecoder* decoder = 0;
    IWICBitmapFrameDecode* frame = 0;
    IWICFormatConverter* converter = 0;
    IWICBitmapScaler* scaler = 0;
    HRESULT hr = gFilmFactory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(const_cast<BYTE*>(gFilmData + offset), size);
    if (SUCCEEDED(hr)) hr = gFilmFactory->CreateDecoderFromStream(stream, 0, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);
    if (SUCCEEDED(hr) && (width != gFilmWidth || height != gFilmHeight)) hr = E_INVALIDARG;
    if (SUCCEEDED(hr)) hr = gFilmFactory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGR,
        WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
    IWICBitmapSource* source = converter;
    if (SUCCEEDED(hr) && (outputWidth != gFilmWidth || outputHeight != gFilmHeight)) {
        hr = gFilmFactory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) hr = scaler->Initialize(converter, outputWidth, outputHeight,
            WICBitmapInterpolationModeHighQualityCubic);
        if (SUCCEEDED(hr)) source = scaler;
    }
    if (SUCCEEDED(hr)) hr = source->CopyPixels(0, outputWidth * 4,
        outputWidth * outputHeight * 4, pixels);
    if (scaler) scaler->Release();
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    return SUCCEEDED(hr);
}

bool DrawBootFilm(HDC dc, int width, int height, int tMs, bool reduced) {
    (void)reduced; // The master has no camera shake or flashes in either mode.
    if (!dc || width <= 0 || height <= 0) return false;
    POINT edges[2] = {{0, 0}, {width, height}};
    if (!LPtoDP(dc, edges, 2)) return false;
    int64_t spanX = (int64_t)edges[1].x - edges[0].x;
    int64_t spanY = (int64_t)edges[1].y - edges[0].y;
    if (spanX < 0) spanX = -spanX;
    if (spanY < 0) spanY = -spanY;
    if (!spanX || spanX > 4096 || !spanY || spanY > 2160
        || spanX * spanY > FILM_MAX_PIXELS || !PrepareBootFilm()) return false;
    uint32_t outputWidth = (uint32_t)spanX, outputHeight = (uint32_t)spanY;
    uint64_t frame = (uint64_t)(tMs > 0 ? tMs : 0) * gFilmFps / 1000;
    if (frame >= gFilmCount) frame = gFilmCount - 1;
    bool resize = !gFilmPixels || outputWidth != gFilmCacheWidth || outputHeight != gFilmCacheHeight;
    if (resize && (outputWidth != gFilmFailedWidth || outputHeight != gFilmFailedHeight)) {
        // Cache physical pixels, not logical canvas pixels: repeated paints of
        // the same 30 fps film frame now cost only a 1:1 copy, even at 2x scale.
        // A resized allocation replaces the old cache only after successful
        // decoding, so allocation/scale failures retain the last good picture.
        BYTE* next = new (std::nothrow) BYTE[(size_t)outputWidth * outputHeight * 4];
        int nextFrame = gFilmFrozen && gFilmFrame >= 0 ? gFilmFrame : (int)frame;
        bool ready = next && DecodeFilmFrame(nextFrame, next, outputWidth, outputHeight);
        if (!ready && next && gFilmFrame >= 0 && nextFrame != gFilmFrame) {
            nextFrame = gFilmFrame;
            ready = DecodeFilmFrame(nextFrame, next, outputWidth, outputHeight);
            if (ready) gFilmFrozen = true;
        }
        if (ready) {
            delete[] gFilmPixels;
            gFilmPixels = next;
            gFilmCacheWidth = outputWidth; gFilmCacheHeight = outputHeight;
            gFilmFailedWidth = gFilmFailedHeight = 0;
            gFilmFrame = nextFrame;
        } else {
            delete[] next;
            if (gFilmFrame < 0) { ReleaseFilmStorage(); return false; }
            gFilmFrozen = true;
            // Avoid retrying an unavailable allocation every paint. A different
            // viewport may still retry the last good frame at a smaller size.
            gFilmFailedWidth = outputWidth; gFilmFailedHeight = outputHeight;
        }
    } else if (!gFilmFrozen && (int)frame != gFilmFrame) {
        if (DecodeFilmFrame((int)frame, gFilmPixels, gFilmCacheWidth, gFilmCacheHeight)) gFilmFrame = (int)frame;
        else {
            // CopyPixels may have partly written the cache before failing.
            // Restore the last known-good JPEG into this same allocation, then
            // hold it while the live exit shutter closes. No second full frame
            // is allocated, and a damaged frame is never exposed to the player.
            if (gFilmFrame < 0 || !DecodeFilmFrame(gFilmFrame, gFilmPixels, gFilmCacheWidth, gFilmCacheHeight)) {
                ReleaseFilmStorage();
                return false;
            }
            gFilmFrozen = true;
        }
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = (LONG)gFilmCacheWidth;
    info.bmiHeader.biHeight = -(LONG)gFilmCacheHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    int saved = SaveDC(dc);
    if (!saved) return false;
    // COLORONCOLOR does no resampling when the cached and physical extents are
    // identical. WIC already produced the high-quality cubic pixels. Only a
    // failed resize uses HALFTONE to preserve the prior valid cache gracefully.
    SetStretchBltMode(dc, outputWidth == gFilmCacheWidth && outputHeight == gFilmCacheHeight ? COLORONCOLOR : HALFTONE);
    SetBrushOrgEx(dc, 0, 0, 0);
    int drawn = StretchDIBits(dc, 0, 0, width, height, 0, 0,
        (int)gFilmCacheWidth, (int)gFilmCacheHeight, gFilmPixels, &info, DIB_RGB_COLORS, SRCCOPY);
    RestoreDC(dc, saved);
    return drawn != 0 && drawn != GDI_ERROR;
}
