#include <dirent.h>
#include <stddef.h>

static DIR g_dir_stub = {0};

DIR* opendir(const char* name) {
    (void)name;
    return &g_dir_stub;
}

struct dirent* readdir(DIR* dirp) {
    (void)dirp;
    return NULL;
}

int closedir(DIR* dirp) {
    (void)dirp;
    return 0;
}

void rewinddir(DIR* dirp) {
    (void)dirp;
}
