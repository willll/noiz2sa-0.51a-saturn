#ifndef SDL_MIXER_WRAPPER_H
#define SDL_MIXER_WRAPPER_H

// SDL_mixer stubs for Saturn noiz2sa
// On Saturn, audio is handled via Saturn native sound system, not SDL_mixer

typedef struct { int dummy; } Mix_Music;
typedef struct { int dummy; } Mix_Chunk;

// Minimal SDL_mixer function stubs
static inline int Mix_OpenAudio(int frequency, unsigned short format, int channels, int chunksize) { 
    (void)frequency; (void)format; (void)channels; (void)chunksize; return 0; 
}
static inline void Mix_CloseAudio(void) {}
static inline int Mix_QuerySpec(int* frequency, unsigned short* format, int* channels) { 
    (void)frequency; (void)format; (void)channels; return 0; 
}
static inline Mix_Music* Mix_LoadMUS(const char* file) { (void)file; return null_ptr; }
static inline void Mix_FreeMusic(Mix_Music* music) { (void)music; }
static inline int Mix_PlayMusic(Mix_Music* music, int loops) { (void)music; (void)loops; return -1; }
static inline int Mix_PlayingMusic(void) { return 0; }
static inline int Mix_FadeOutMusic(int ms) { (void)ms; return 0; }
static inline void Mix_HaltMusic(void) {}
static inline Mix_Chunk* Mix_LoadWAV(const char* file) { (void)file; return null_ptr; }
static inline void Mix_FreeChunk(Mix_Chunk* chunk) { (void)chunk; }
static inline int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops) { (void)channel; (void)chunk; (void)loops; return -1; }

#endif // SDL_MIXER_WRAPPER_H
