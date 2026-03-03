#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

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
struct SDL_Surface;
struct SDL_PixelFormat;

// SDL Rect type (defined early to avoid forward declaration issues)
typedef struct {
    int16_t x, y;
    uint16_t w, h;
} SDL_Rect;

// SDL Color
typedef struct {
    uint8_t r, g, b, unused;
} SDL_Color;

// SDL PixelFormat
typedef struct SDL_PixelFormat {
    void* palette;
    uint8_t BitsPerPixel;
    uint8_t BytesPerPixel;
    uint32_t Rmask, Gmask, Bmask, Amask;
    uint8_t Rshift, Gshift, Bshift, Ashift;
} SDL_PixelFormat;

// SDL Surface type - complete version
typedef struct SDL_Surface {
    uint32_t flags;
    SDL_PixelFormat* format;
    int w, h;
    uint16_t pitch;
    void* pixels;
    int offset;
    void* hwdata;
    SDL_Rect clip_rect;
    uint32_t unused1;
    uint32_t locked;
    void* map;
    unsigned int format_version;
    int refcount;
} SDL_Surface;

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

// SDL color key and RLE flags
#define SDL_SRCCOLORKEY 0x00001000
#define SDL_RLEACCEL 0x00004000

// SDL event types
#define SDL_QUIT 12
#define SDL_PRESSED 1
#define SDL_RELEASED 0

// SDL cursor visibility
#define SDL_DISABLE 0
#define SDL_ENABLE 1

// SDL keysym codes
#define SDLK_ESCAPE 256
#define SDLK_p 'p'
#define SDLK_z 'z'
#define SDLK_x 'x'
#define SDLK_UP 273
#define SDLK_DOWN 274
#define SDLK_RIGHT 275
#define SDLK_LEFT 276
#define SDLK_KP8 273
#define SDLK_KP2 274
#define SDLK_KP6 275
#define SDLK_KP4 276

// SDL initialization and system
static inline int SDL_Init(uint32_t flags) { (void)flags; return 0; }
static inline int SDL_InitSubSystem(uint32_t flags) { (void)flags; return 0; }
static inline void SDL_Quit(void) {}
static inline const char* SDL_GetError(void) { return "SDL stub"; }

// SDL video functions
static inline SDL_Surface* SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags) {
    (void)width; (void)height; (void)bpp; (void)flags;
    static SDL_Surface stub_surface = {0};
    static SDL_PixelFormat stub_format = {0};
    stub_surface.format = &stub_format;
    stub_surface.w = width;
    stub_surface.h = height;
    stub_format.BitsPerPixel = bpp;
    stub_format.BytesPerPixel = bpp / 8;
    return &stub_surface;
}

static inline SDL_Surface* SDL_CreateRGBSurface(uint32_t flags, int width, int height, int depth, 
                                                 uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
    (void)flags; (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;
    static SDL_Surface stub_surface = {0};
    static SDL_PixelFormat stub_format = {0};
    stub_surface.format = &stub_format;
    stub_surface.w = width;
    stub_surface.h = height;
    stub_format.BitsPerPixel = depth;
    stub_format.BytesPerPixel = depth / 8;
    return &stub_surface;
}

static inline SDL_Surface* SDL_LoadBMP(const char* file) { (void)file; return nullptr; }
static inline SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, SDL_PixelFormat* fmt, uint32_t flags) {
    (void)fmt; (void)flags; return src;
}

static inline int SDL_SetColors(SDL_Surface* surface, SDL_Color* colors, int firstcolor, int ncolors) {
    (void)surface; (void)colors; (void)firstcolor; (void)ncolors; return 1;
}

static inline int SDL_SetColorKey(SDL_Surface* surface, uint32_t flag, uint32_t key) {
    (void)surface; (void)flag; (void)key; return 0;
}

static inline int SDL_BlitSurface(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
    (void)src; (void)srcrect; (void)dst; (void)dstrect; return 0;
}

static inline int SDL_FillRect(SDL_Surface* dst, SDL_Rect* dstrect, uint32_t color) {
    (void)dst; (void)dstrect; (void)color; return 0;
}

static inline int SDL_Flip(SDL_Surface* screen) { (void)screen; return 0; }
static inline void SDL_WM_SetCaption(const char* title, const char* icon) { (void)title; (void)icon; }
static inline int SDL_ShowCursor(int toggle) { (void)toggle; return 0; }

// SDL event and input functions
static inline int SDL_PollEvent(SDL_Event* event) { (void)event; return 0; }
static inline const uint8_t* SDL_GetKeyState(int* numkeys) { if(numkeys) *numkeys=0; return nullptr; }
static inline uint32_t SDL_GetTicks(void) { return 0; }
static inline void SDL_Delay(uint32_t ms) { (void)ms; }

// SDL joystick/controller functions
typedef struct _SDL_Joystick SDL_Joystick;
typedef struct _SDL_GameController SDL_GameController;
typedef int SDL_bool;

#define SDL_FALSE 0
#define SDL_TRUE  1

// SDL joystick hat positions
#define SDL_HAT_CENTERED  0x00
#define SDL_HAT_UP        0x01
#define SDL_HAT_RIGHT     0x02
#define SDL_HAT_DOWN      0x04
#define SDL_HAT_LEFT      0x08

static inline int SDL_NumJoysticks(void) { return 0; }
static inline SDL_Joystick* SDL_JoystickOpen(int device_index) { (void)device_index; return nullptr; }
static inline void SDL_JoystickClose(SDL_Joystick* joystick) { (void)joystick; }
static inline void SDL_JoystickUpdate(void) { }
static inline SDL_bool SDL_JoystickOpened(int device_index) { (void)device_index; return SDL_FALSE; }
static inline const char* SDL_JoystickName(SDL_Joystick* joystick) { (void)joystick; return ""; }
static inline uint8_t SDL_JoystickGetButton(SDL_Joystick* joystick, int button) { (void)joystick; (void)button; return 0; }
static inline int16_t SDL_JoystickGetAxis(SDL_Joystick* joystick, int axis) { (void)joystick; (void)axis; return 0; }
static inline uint8_t SDL_JoystickGetHat(SDL_Joystick* joystick, int hat) { (void)joystick; (void)hat; return SDL_HAT_CENTERED; }
static inline void SDL_QuitSubSystem(uint32_t flags) { (void)flags; }

// SDL_GameControllerOpen is defined in gamepad.cpp, declared in gamepad.h
// Declare it here for files that include SDL.h before gamepad.h
SDL_GameController* SDL_GameControllerOpen(int joystick_index);

#endif // SDL_WRAPPER_H
