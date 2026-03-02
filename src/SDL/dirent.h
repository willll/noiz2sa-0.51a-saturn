#ifndef SDL_DIRENT_WRAPPER_H
#define SDL_DIRENT_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DIR {
    int dummy;
} DIR;

struct dirent {
    char d_name[256];
};

DIR* opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);
void rewinddir(DIR* dirp);

#ifdef __cplusplus
}
#endif

#endif
