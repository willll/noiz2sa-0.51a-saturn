// Minimal stdio implementation for Saturn
// These stubs satisfy the C++ standard library requirements

#include "stdio.h"
#include <string.h>

// Minimalist implementations of stdarg formatting functions
int printf(const char* fmt, ...) {
    (void)fmt;
    return 0;
}

int sprintf(char* buf, const char* fmt, ...) {
    (void)buf;
    (void)fmt;
    return 0;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    (void)buf;
    (void)size;
    (void)fmt;
    return 0;
}

int vprintf(const char* fmt, va_list args) {
    (void)fmt;
    (void)args;
    return 0;
}

int vsprintf(char* buf, const char* fmt, va_list args) {
    (void)buf;
    (void)fmt;
    (void)args;
    return 0;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    (void)buf;
    (void)size;
    (void)fmt;
    (void)args;
    return 0;
}

// Exit function (stub for Saturn)
void exit(int status) {
    (void)status;
    // On Saturn, we can't actually exit, just return
    // The game would typically reset or loop
}
