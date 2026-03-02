#include <dirent.h>

// Manual definitions to replace stddef.h (not available on Saturn)
#ifndef null_ptr
#define null_ptr ((void*)0)
#endif
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

static DIR g_dir_stub = {0};

DIR* opendir(const char* name) {
    (void)name;
    return &g_dir_stub;
}

struct dirent* readdir(DIR* dirp) {
    (void)dirp;
    return null_ptr;
}

int closedir(DIR* dirp) {
    (void)dirp;
    return 0;
}

void rewinddir(DIR* dirp) {
    (void)dirp;
}
