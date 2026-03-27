#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <srl.hpp>
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

// Saturn hardware timer support - included after SRL to avoid conflicts
#include <sega_tim.h>

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
    
    // Constructor for proper initialization
    SRL_Surface() : textureIndex(-1), w(0), h(0), pixels(nullptr), dirty(true), preferRGB555(false) {}
    
    // Constructor with dimensions
    SRL_Surface(int32_t textureIndex, int width, int height) : textureIndex(textureIndex), w(width), h(height), pixels(nullptr), dirty(true), preferRGB555(false) {}
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


// SDL timing - Track elapsed time since SDL_Init for SDL_GetTicks()
// Uses Saturn's Free-Running Timer (FRT) hardware for accurate microsecond timing
static volatile uint16_t sdl_previousRawCount = 0;
static volatile uint32_t sdl_elapsedMicros = 0;
static volatile int sdl_initialized = 0;
static volatile int16_t sdl_blitPaletteBank = 0;
static int16_t sdl_blitPaletteCacheBank = INT16_MIN;
static uint16_t sdl_blitPalette555[256] = { 0 };
static uint32_t sdl_blitCalls = 0;
static uint32_t sdl_blitUploads = 0;
static uint32_t sdl_blitUploadPixels = 0;
static uint32_t sdl_blitUploadMs = 0;
static uint32_t sdl_blitDrawMs = 0;

static inline uint32_t SDL_GetTicks(void);

static inline void SDL_CopyBytes(void* dst, const void* src, uint32_t byteCount)
{
    uint8_t* d8 = (uint8_t*)dst;
    const uint8_t* s8 = (const uint8_t*)src;

    while (byteCount-- > 0)
    {
        *d8++ = *s8++;
    }
}

// SDL initialization and system
static inline int SDL_Init(uint32_t flags) { 
    (void)flags;
    
    // Initialize the FRT once and keep it running continuously.
    // Use divider 128 so 16-bit counter wraps much less frequently,
    // keeping SDL_GetTicks stable even when frame rate is low.
    TIM_FRT_INIT(TIM_CKS_128);
    TIM_FRT_SET_16(0);
    
    sdl_previousRawCount = 0;
    sdl_elapsedMicros = 0;
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

static inline void SDL_ResetBlitStats()
{
    sdl_blitCalls = 0;
    sdl_blitUploads = 0;
    sdl_blitUploadPixels = 0;
    sdl_blitUploadMs = 0;
    sdl_blitDrawMs = 0;
}

static inline void SDL_GetBlitStats(
    uint32_t* calls,
    uint32_t* uploads,
    uint32_t* uploadPixels,
    uint32_t* uploadMs,
    uint32_t* drawMs)
{
    if (calls != nullptr) *calls = sdl_blitCalls;
    if (uploads != nullptr) *uploads = sdl_blitUploads;
    if (uploadPixels != nullptr) *uploadPixels = sdl_blitUploadPixels;
    if (uploadMs != nullptr) *uploadMs = sdl_blitUploadMs;
    if (drawMs != nullptr) *drawMs = sdl_blitDrawMs;
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
        const uint32_t uploadStart = SDL_GetTicks();

        const uint32_t pixelCount = (uint32_t)(src->w * src->h);
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

            const uint8_t* src8 = (const uint8_t*)src->pixels;
            uint16_t* dst16 = (uint16_t*)dstData;
            for (uint32_t i = 0; i < pixelCount; i++)
            {
                dst16[i] = sdl_blitPalette555[src8[i]];
            }
        }
        else
        {
            SDL_CopyBytes(dstData, src->pixels, pixelCount);
        }

        src->dirty = false;
        sdl_blitUploads++;
        sdl_blitUploadPixels += pixelCount;
        sdl_blitUploadMs += (SDL_GetTicks() - uploadStart);
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

    const uint32_t drawStart = SDL_GetTicks();
    const int result = SRL::Scene2D::DrawSprite(
               (uint16_t)src->textureIndex,
               paletteOverridePtr,
               pos,
               SRL::Math::Types::Angle::Zero(),
               SRL::Math::Types::Vector2D(1.0, 1.0),
               SRL::Scene2D::ZoomPoint::UpperLeft) ? 0 : -1;
    SRL::Scene2D::SetEffect(SRL::Scene2D::SpriteEffect::EnableECD, prevEndCode == 1 ? 1 : 0);
    sdl_blitDrawMs += (SDL_GetTicks() - drawStart);
    return result;
}

static inline int SDL_FillRect(SRL_Surface* dst, SDL_Rect* dstrect, uint32_t color) {
    (void)dst; (void)dstrect; (void)color; return 0;
}

static inline int SDL_Flip(SRL_Surface* screen) { (void)screen; return 0; }

// SDL event and input functions - Saturn implementation
// Hardware timer-based timing using the FRT (Free-Running Timer)

static inline uint32_t SDL_GetTicks(void) {
    if (!sdl_initialized) {
        return 0;
    }

    const uint16_t currentRawCount = TIM_FRT_GET_16();
    const uint16_t deltaCount = (uint16_t)(currentRawCount - sdl_previousRawCount);
    sdl_previousRawCount = currentRawCount;
    sdl_elapsedMicros += TIM_FRT_CNT_TO_MCR(deltaCount);
    return sdl_elapsedMicros / 1000u;
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
