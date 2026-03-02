// Minimal stdio implementation for Saturn
// These stubs satisfy the C++ standard library requirements

#include "stdio.h"
#include <string.h>

// Basic FILE implementation (opaque, only for pointer passing)
struct __sFILE {
    int dummy;
};

static struct __sFILE __file_stub = {0};

FILE* stderr = (FILE*)&__file_stub;
FILE* stdout = (FILE*)&__file_stub;
FILE* stdin = (FILE*)&__file_stub;

int clearerr(FILE* file) {
    (void)file;
    return 0;
}

int feof(FILE* file) {
    (void)file;
    return 0;
}

int ferror(FILE* file) {
    (void)file;
    return 0;
}

int fflush(FILE* file) {
    (void)file;
    return 0;
}

// Minimalist implementations of stdio functions
int fprintf(FILE* file, const char* fmt, ...) {
    (void)file;
    (void)fmt;
    return 0;
}

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

int vfprintf(FILE* file, const char* fmt, va_list args) {
    (void)file;
    (void)fmt;
    (void)args;
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

FILE* fopen(const char* filename, const char* mode) {
    (void)filename;
    (void)mode;
    return NULL;
}

int fclose(FILE* file) {
    (void)file;
    return 0;
}

FILE* freopen(const char* filename, const char* mode, FILE* file) {
    (void)filename;
    (void)mode;
    (void)file;
    return NULL;
}

int fgetc(FILE* file) {
    (void)file;
    return EOF;
}

int fputc(int c, FILE* file) {
    (void)c;
    (void)file;
    return EOF;
}

char* fgets(char* str, int size, FILE* file) {
    (void)str;
    (void)size;
    (void)file;
    return NULL;
}

int fputs(const char* str, FILE* file) {
    (void)str;
    (void)file;
    return 0;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* file) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)file;
    return 0;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* file) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)file;
    return 0;
}

int fseek(FILE* file, long offset, int whence) {
    (void)file;
    (void)offset;
    (void)whence;
    return -1;
}

long ftell(FILE* file) {
    (void)file;
    return -1;
}

int fgetpos(FILE* file, fpos_t* pos) {
    (void)file;
    (void)pos;
    return -1;
}

int fsetpos(FILE* file, const fpos_t* pos) {
    (void)file;
    (void)pos;
    return -1;
}

void rewind(FILE* file) {
    (void)file;
}

int fscanf(FILE* file, const char* fmt, ...) {
    (void)file;
    (void)fmt;
    return 0;
}

int scanf(const char* fmt, ...) {
    (void)fmt;
    return 0;
}

int sscanf(const char* str, const char* fmt, ...) {
    (void)str;
    (void)fmt;
    return 0;
}

int vfscanf(FILE* file, const char* fmt, va_list args) {
    (void)file;
    (void)fmt;
    (void)args;
    return 0;
}

int vscanf(const char* fmt, va_list args) {
    (void)fmt;
    (void)args;
    return 0;
}

int vsscanf(const char* str, const char* fmt, va_list args) {
    (void)str;
    (void)fmt;
    (void)args;
    return 0;
}

int getc(FILE* file) {
    (void)file;
    return EOF;
}

int getchar(void) {
    return EOF;
}

int getw(FILE* file) {
    (void)file;
    return EOF;
}

int putc(int c, FILE* file) {
    (void)c;
    (void)file;
    return EOF;
}

int putchar(int c) {
    (void)c;
    return EOF;
}

int putw(int w, FILE* file) {
    (void)w;
    (void)file;
    return EOF;
}

int puts(const char* str) {
    (void)str;
    return 0;
}

int ungetc(int c, FILE* file) {
    (void)c;
    (void)file;
    return EOF;
}

void setbuf(FILE* file, char* buf) {
    (void)file;
    (void)buf;
}

int setvbuf(FILE* file, char* buf, int mode, size_t size) {
    (void)file;
    (void)buf;
    (void)mode;
    (void)size;
    return 0;
}

FILE* tmpfile(void) {
    return NULL;
}

char* tmpnam(char* buf) {
    if (buf) buf[0] = '\0';
    return NULL;
}

int remove(const char* filename) {
    (void)filename;
    return -1;
}

int rename(const char* oldname, const char* newname) {
    (void)oldname;
    (void)newname;
    return -1;
}

void perror(const char* str) {
    (void)str;
}
