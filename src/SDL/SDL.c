// SDL wrapper implementation for Saturn noiz2sa
// Minimal SDL function stubs - hybrid approach for incremental refactoring

#include "SDL.h"

// Minimal FILE stub for stderr
FILE _stderr_stub = {0};
FILE* stderr = &_stderr_stub;

// Global stub surfaces that can be reused
SDL_Surface global_screen = {0};
SDL_PixelFormat global_format = {0};

// SDL stub initialization
void __attribute__((constructor)) sdl_stub_init() {
    global_screen.format = &global_format;
}
