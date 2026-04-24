#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <srl.hpp>
#include <srl_timer.hpp>
#include <srl_vdp1.hpp>
#include <srl_scene2d.hpp>

// SDL type stubs for Saturn noiz2sa - Hybrid approach
// These stubs allow compilation now, can be replaced with SRL incrementally later
// Note: Most SDL types are already defined in gamepad.h

// Basic SDL types
typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;

// Forward declarations
struct SRL_Surface;
struct SDL_PixelFormat;

// SDL Rect type (defined early to avoid forward declaration issues)
typedef struct {
    int16_t x, y;
    uint16_t w, h;
} SDL_Rect;

// SDL Color - now using SRL HighColor
// Kept as typedef for compatibility but redirects to SRL::Types::HighColor
#include <srl_color.hpp>

// SDL PixelFormat
typedef struct SDL_PixelFormat {
    void* palette;
    uint8_t BitsPerPixel;
    uint8_t BytesPerPixel;
    uint32_t Rmask, Gmask, Bmask, Amask;
    uint8_t Rshift, Gshift, Bshift, Ashift;
} SDL_PixelFormat;

// SDL Surface type - C++ struct with constructor
struct SRL_Surface {
    int32_t textureIndex;
    int w, h;
    const void* pixels;
    bool dirty;
    bool preferRGB555;
    int16_t dirtyX1;
    int16_t dirtyY1;
    int16_t dirtyX2;
    int16_t dirtyY2;
    int16_t cachedPaletteBank;  // Track which palette was used for RGB555 expansion
    uint32_t cachedPaletteHash; // Simple hash to detect palette changes
    
    // Constructor for proper initialization
    SRL_Surface() : textureIndex(-1), w(0), h(0), pixels(nullptr), dirty(true), preferRGB555(false), dirtyX1(0), dirtyY1(0), dirtyX2(0), dirtyY2(0), cachedPaletteBank(INT16_MIN), cachedPaletteHash(0) {}
    
    // Constructor with dimensions
    SRL_Surface(int32_t textureIndex, int width, int height) : textureIndex(textureIndex), w(width), h(height), pixels(nullptr), dirty(true), preferRGB555(false), dirtyX1(0), dirtyY1(0), dirtyX2((int16_t)width), dirtyY2((int16_t)height), cachedPaletteBank(INT16_MIN), cachedPaletteHash(0) {}
};

// SDL Event type - needs 'type' field for the main event loop
typedef struct {
    uint32_t type;
    uint8_t padding[60];
} SDL_Event;

// SDL init flags
#define SDL_INIT_VIDEO 0x00000020
#define SDL_INIT_AUDIO 0x00000010
#define SDL_INIT_JOYSTICK 0x00000200

// SDL audio format constants
#define AUDIO_S16 0x8010  // Signed 16-bit samples

// SDL video flags
#define SDL_SWSURFACE 0x00000000
#define SDL_HWSURFACE 0x00000001
#define SDL_DOUBLEBUF 0x40000000
#define SDL_FULLSCREEN 0x80000000
#define SDL_HWPALETTE 0x20000000


// SDL timing - backed by SRL::Timer so the project uses a single timing source.
// SDL_GetTicks() and SDL_GetProfileMicros() measure elapsed time since SDL_Init().
static SRL::Tickstamp sdl_ticksStart;
static SRL::Tickstamp sdl_profilerStart;
static volatile int sdl_initialized = 0;
static volatile int16_t sdl_blitPaletteBank = 0;
static int16_t sdl_blitPaletteCacheBank = INT16_MIN;
static uint16_t sdl_blitPalette555[256] = { 0 };
static uint32_t sdl_blitCalls = 0;
static uint32_t sdl_blitUploads = 0;
static uint32_t sdl_blitUploadPixels = 0;
static uint32_t sdl_blitUploadUs = 0;
static uint32_t sdl_blitDrawUs = 0;

static inline uint32_t SDL_GetTicks(void);
static inline uint32_t SDL_GetProfileMicros(void);

static inline void SDL_CopyBytes(void* dst, const void* src, uint32_t byteCount)
{
    uint8_t* d8 = (uint8_t*)dst;
    const uint8_t* s8 = (const uint8_t*)src;

    while (byteCount-- > 0)
    {
        *d8++ = *s8++;
    }
}

static inline void SDL_ClearDirtyRect(SRL_Surface* surface)
{
    if (surface == nullptr)
    {
        return;
    }

    surface->dirty = false;
    surface->dirtyX1 = (int16_t)surface->w;
    surface->dirtyY1 = (int16_t)surface->h;
    surface->dirtyX2 = 0;
    surface->dirtyY2 = 0;
}

static inline void SDL_SetSurfaceDirty(SRL_Surface* surface)
{
    if (surface == nullptr)
    {
        return;
    }

    surface->dirty = true;
    surface->dirtyX1 = 0;
    surface->dirtyY1 = 0;
    surface->dirtyX2 = (int16_t)surface->w;
    surface->dirtyY2 = (int16_t)surface->h;
}

static inline void SDL_MarkDirtyRect(SRL_Surface* surface, int x, int y, int w, int h)
{
    if (surface == nullptr || w <= 0 || h <= 0)
    {
        return;
    }

    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x >= surface->w || y >= surface->h)
    {
        return;
    }

    if (x + w > surface->w)
    {
        w = surface->w - x;
    }
    if (y + h > surface->h)
    {
        h = surface->h - y;
    }
    if (w <= 0 || h <= 0)
    {
        return;
    }

    if (!surface->dirty)
    {
        surface->dirty = true;
        surface->dirtyX1 = (int16_t)x;
        surface->dirtyY1 = (int16_t)y;
        surface->dirtyX2 = (int16_t)(x + w);
        surface->dirtyY2 = (int16_t)(y + h);
        return;
    }

    if (x < surface->dirtyX1) surface->dirtyX1 = (int16_t)x;
    if (y < surface->dirtyY1) surface->dirtyY1 = (int16_t)y;
    if ((x + w) > surface->dirtyX2) surface->dirtyX2 = (int16_t)(x + w);
    if ((y + h) > surface->dirtyY2) surface->dirtyY2 = (int16_t)(y + h);
}

// SDL initialization and system
static inline int SDL_Init(uint32_t flags) { 
    (void)flags;

    // SRL::Core::Initialize() already calls SRL::Timer::Init().
    // We only snapshot the current timer so SDL-style elapsed APIs can measure
    // time relative to SDL_Init() without maintaining a separate timer stack.
    sdl_ticksStart = SRL::Timer::Capture();
    sdl_profilerStart = sdl_ticksStart;
    sdl_initialized = 1;
    
    return 0; 
}
static inline int SDL_InitSubSystem(uint32_t flags) { (void)flags; return 0; }
static inline const char* SDL_GetError(void) { return "SDL stub"; }

static inline void SDL_SetBlitPaletteBank(int16_t paletteBank)
{
    sdl_blitPaletteBank = paletteBank;
    sdl_blitPaletteCacheBank = INT16_MIN;
}

// Calculate a simple hash of palette data to detect changes
static inline uint32_t SDL_HashPalette()
{
    uint32_t hash = 5381;
    const uint16_t* palette_data = sdl_blitPalette555;
    for (int i = 0; i < 256; i++)
    {
        hash = ((hash << 5) + hash) ^ (palette_data[i] & 0xFF);
    }
    return hash;
}



static inline void SDL_ResetBlitStats()
{
    sdl_blitCalls = 0;
    sdl_blitUploads = 0;
    sdl_blitUploadPixels = 0;
    sdl_blitUploadUs = 0;
    sdl_blitDrawUs = 0;
}

static inline void SDL_GetBlitStats(
    uint32_t* calls,
    uint32_t* uploads,
    uint32_t* uploadPixels,
    uint32_t* uploadUs,
    uint32_t* drawUs)
{
    if (calls != nullptr) *calls = sdl_blitCalls;
    if (uploads != nullptr) *uploads = sdl_blitUploads;
    if (uploadPixels != nullptr) *uploadPixels = sdl_blitUploadPixels;
    if (uploadUs != nullptr) *uploadUs = sdl_blitUploadUs;
    if (drawUs != nullptr) *drawUs = sdl_blitDrawUs;
}

static inline bool SDL_RefreshBlitPaletteCache()
{
    if (sdl_blitPaletteCacheBank == sdl_blitPaletteBank)
    {
        return true;
    }

    SRL::CRAM::Palette blitPalette(
        SRL::CRAM::TextureColorMode::Paletted256,
        (uint16_t)(sdl_blitPaletteBank < 0 ? 0 : sdl_blitPaletteBank));
    SRL::Types::HighColor* paletteData = blitPalette.GetData();
    if (paletteData == nullptr)
    {
        return false;
    }

    for (uint32_t i = 0; i < 256; i++)
    {
        sdl_blitPalette555[i] = (uint16_t)paletteData[i];
    }

    sdl_blitPaletteCacheBank = sdl_blitPaletteBank;
    return true;
}

static inline int SDL_SetColors(SRL_Surface* surface, SRL::Types::HighColor* colors, int firstcolor, int ncolors) {
    (void)surface; (void)colors; (void)firstcolor; (void)ncolors; return 0;
}

static inline int SDL_BlitSurface(SRL_Surface* src, SDL_Rect* srcrect, SRL_Surface* dst, SDL_Rect* dstrect) {
    if (src == nullptr || dst == nullptr) {
        return -1;
    }

    sdl_blitCalls++;

    // Create a dynamic texture for software surfaces on first use.
    if (src->textureIndex < 0) {
        if (src->w <= 0 || src->h <= 0) {
            return -1;
        }

        const SRL::CRAM::TextureColorMode blitMode = src->preferRGB555
            ? SRL::CRAM::TextureColorMode::RGB555
            : SRL::CRAM::TextureColorMode::Paletted256;
        const uint16_t paletteId = src->preferRGB555
            ? 0
            : (uint16_t)(sdl_blitPaletteBank < 0 ? 0 : sdl_blitPaletteBank);

        src->textureIndex = SRL::VDP1::TryAllocateTexture(
            (uint16_t)src->w,
            (uint16_t)src->h,
            blitMode,
            paletteId);

        if (src->textureIndex < 0) {
            return -1;
        }
    }

    // Upload source pixels if this is a software-backed surface.
    if (src->pixels != nullptr && src->dirty) {
        const uint32_t uploadStart = SDL_GetProfileMicros();
        int uploadX = src->dirtyX1;
        int uploadY = src->dirtyY1;
        int uploadW = src->dirtyX2 - src->dirtyX1;
        int uploadH = src->dirtyY2 - src->dirtyY1;

        if (uploadW <= 0 || uploadH <= 0)
        {
            uploadX = 0;
            uploadY = 0;
            uploadW = src->w;
            uploadH = src->h;
        }

        const uint32_t pixelCount = (uint32_t)(uploadW * uploadH);

        void* dstData = SRL::VDP1::Textures[src->textureIndex].GetData();

        if (src->pixels == nullptr || dstData == nullptr) {
            return -1;
        }

        if (SRL::VDP1::Metadata[src->textureIndex].ColorMode == SRL::CRAM::TextureColorMode::RGB555)
        {
            if (!SDL_RefreshBlitPaletteCache())
            {
                return -1;
            }

            // Only expand pixels if palette changed or this is the first time
            const uint32_t newPaletteHash = SDL_HashPalette();
            const int16_t newPaletteBank = (int16_t)(sdl_blitPaletteBank < 0 ? 0 : sdl_blitPaletteBank);

            const uint8_t* src8 = (const uint8_t*)src->pixels;
            uint16_t* dst16 = (uint16_t*)dstData;
            for (int row = 0; row < uploadH; row++)
            {
                const uint8_t* srcRow = src8 + ((uploadY + row) * src->w) + uploadX;
                uint16_t* dstRow = dst16 + ((uploadY + row) * src->w) + uploadX;
                for (int col = 0; col < uploadW; col++)
                {
                    dstRow[col] = sdl_blitPalette555[srcRow[col]];
                }
            }

            src->cachedPaletteBank = newPaletteBank;
            src->cachedPaletteHash = newPaletteHash;
        }
        else
        {
            const uint8_t* src8 = (const uint8_t*)src->pixels;
            uint8_t* dst8 = (uint8_t*)dstData;

            if (uploadX == 0 && uploadY == 0 && uploadW == src->w && uploadH == src->h)
            {
                slDMACopy((void*)src8, dst8, (uint32_t)(src->w * src->h));
            }
            else
            {
                for (int row = 0; row < uploadH; row++)
                {
                    const uint8_t* srcRow = src8 + ((uploadY + row) * src->w) + uploadX;
                    uint8_t* dstRow = dst8 + ((uploadY + row) * src->w) + uploadX;
                    slDMACopy((void*)srcRow, dstRow, (uint32_t)uploadW);
                }
            }
            slDMAWait();
        }

        SDL_ClearDirtyRect(src);
        sdl_blitUploads++;
        sdl_blitUploadPixels += pixelCount;
        sdl_blitUploadUs += (SDL_GetProfileMicros() - uploadStart);
    }

    SDL_Rect resolvedRect;
    if (dstrect != nullptr) {
        resolvedRect = *dstrect;
    } else {
        resolvedRect.x = 0;
        resolvedRect.y = 0;
        resolvedRect.w = (uint16_t)src->w;
        resolvedRect.h = (uint16_t)src->h;
    }

    // Partial source rectangle support is not implemented in this wrapper yet.
    if (srcrect != nullptr) {
        if (srcrect->x != 0 || srcrect->y != 0 || srcrect->w != src->w || srcrect->h != src->h) {
            return -1;
        }
    }

    const int16_t sceneX = (int16_t)(resolvedRect.x - (dst->w / 2));
    const int16_t sceneY = (int16_t)(resolvedRect.y - (dst->h / 2));

    const SRL::Math::Types::Vector3D pos(
        SRL::Math::Types::Fxp::Convert(sceneX),
        SRL::Math::Types::Fxp::Convert(sceneY),
        500.0);

    SRL::CRAM::Palette paletteOverride;
    SRL::CRAM::Palette* paletteOverridePtr = nullptr;
    const uint16_t paletteId = (uint16_t)(sdl_blitPaletteBank < 0 ? 0 : sdl_blitPaletteBank);
    if (src->textureIndex >= 0 &&
        SRL::VDP1::Metadata[src->textureIndex].ColorMode == SRL::CRAM::TextureColorMode::Paletted256 &&
        SRL::VDP1::Metadata[src->textureIndex].PaletteId != paletteId) {
        paletteOverride = SRL::CRAM::Palette(SRL::CRAM::TextureColorMode::Paletted256, paletteId);
        paletteOverridePtr = &paletteOverride;
    }

    // In paletted modes, index 0xFF is a valid color for this game data.
    // Keep ECD disabled here so 0xFF does not terminate scanline drawing.
    const int32_t prevEndCode = SRL::Scene2D::GetEffect(SRL::Scene2D::SpriteEffect::EnableECD);
    SRL::Scene2D::SetEffect(SRL::Scene2D::SpriteEffect::EnableECD, 0);

    const uint32_t drawStart = SDL_GetProfileMicros();
    const int result = SRL::Scene2D::DrawSprite(
               (uint16_t)src->textureIndex,
               paletteOverridePtr,
               pos,
               SRL::Math::Types::Angle::Zero(),
               SRL::Math::Types::Vector2D(1.0, 1.0),
               SRL::Scene2D::ZoomPoint::UpperLeft) ? 0 : -1;
    const uint32_t drawEnd = SDL_GetProfileMicros();
    sdl_blitDrawUs += (drawEnd - drawStart);
    
    SRL::Scene2D::SetEffect(SRL::Scene2D::SpriteEffect::EnableECD, prevEndCode == 1 ? 1 : 0);
    return result;
}

static inline int SDL_FillRect(SRL_Surface* dst, SDL_Rect* dstrect, uint32_t color) {
    (void)dst; (void)dstrect; (void)color; return 0;
}

static inline int SDL_Flip(SRL_Surface* screen) { (void)screen; return 0; }

// SDL event and input functions - Saturn implementation
// Time conversion uses SRL::Timer snapshots instead of direct FRT access.

static inline uint32_t SDL_GetTicks(void) {
    if (!sdl_initialized) {
        return 0;
    }

    const SRL::Tickstamp elapsed = SRL::Timer::Capture() - sdl_ticksStart;
    const uint32_t secondsRaw = (uint32_t)elapsed.ToSeconds().RawValue();
    return (uint32_t)(((uint64_t)secondsRaw * 1000ull) >> 16);
}

static inline uint32_t SDL_GetProfileMicros(void) {
    if (!sdl_initialized) {
        return 0;
    }

    const SRL::Tickstamp elapsed = SRL::Timer::Capture() - sdl_profilerStart;
    const uint32_t secondsRaw = (uint32_t)elapsed.ToSeconds().RawValue();
    return (uint32_t)(((uint64_t)secondsRaw * 1000000ull) >> 16);
}

static inline void SDL_Delay(uint32_t ms) {
    if (ms == 0 || !sdl_initialized) {
        return;
    }

    const uint32_t start = SDL_GetTicks();
    while ((SDL_GetTicks() - start) < ms) {
    }
}

// SDL_GameControllerOpen is defined in gamepad.cpp, declared in gamepad.h
// Declare it here for files that include SDL.h before gamepad.h

#endif // SDL_WRAPPER_H
