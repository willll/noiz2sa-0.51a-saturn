/**
 * Minimal newlib syscall stubs for Saturn
 * These are required by newlib but not provided by the Saturn hardware
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Stub implementations */

void _exit(int code) {
    (void)code;
    while(1); /* Hang */
}

int _close(int file) {
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st) {
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

void *_sbrk(int incr) {
    extern char end; /* Defined by linker */
    static char *heap_end = 0;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &end;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    return (void *)prev_heap_end;
}

int _write(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    return len; /* Pretend everything was written */
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}
