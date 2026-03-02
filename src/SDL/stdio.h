#ifndef SDL_STDIO_WRAPPER_H
#define SDL_STDIO_WRAPPER_H

// Manual definitions to replace stddef.h (not available on Saturn)
#ifndef null_ptr
#define null_ptr ((void*)0)
#endif
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

// Manual va_list definition for Saturn (no stdarg.h available)
#ifndef _VA_LIST_DEFINED
#define _VA_LIST_DEFINED
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef long fpos_t;
struct __sFILE;
typedef struct __sFILE FILE;

extern FILE* stderr;
extern FILE* stdout;
extern FILE* stdin;

int clearerr(FILE* file);
int fclose(FILE* file);
int feof(FILE* file);
int ferror(FILE* file);
int fflush(FILE* file);
int fgetc(FILE* file);
int fgetpos(FILE* file, fpos_t* pos);
char* fgets(char* str, int size, FILE* file);
FILE* fopen(const char* filename, const char* mode);
int fprintf(FILE* file, const char* fmt, ...);
int fputc(int c, FILE* file);
int fputs(const char* str, FILE* file);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* file);
FILE* freopen(const char* filename, const char* mode, FILE* file);
int fscanf(FILE* file, const char* fmt, ...);
int fseek(FILE* file, long offset, int whence);
int fsetpos(FILE* file, const fpos_t* pos);
long ftell(FILE* file);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* file);
int getc(FILE* file);
int getchar(void);
int getw(FILE* file);
void perror(const char* str);
int printf(const char* fmt, ...);
int putc(int c, FILE* file);
int putchar(int c);
int putw(int w, FILE* file);
int puts(const char* str);
int remove(const char* filename);
int rename(const char* oldname, const char* newname);
void rewind(FILE* file);
int scanf(const char* fmt, ...);
void setbuf(FILE* file, char* buf);
int setvbuf(FILE* file, char* buf, int mode, size_t size);
int sprintf(char* buf, const char* fmt, ...);
int sscanf(const char* str, const char* fmt, ...);
FILE* tmpfile(void);
char* tmpnam(char* buf);
int ungetc(int c, FILE* file);
int vfprintf(FILE* file, const char* fmt, va_list args);
int vprintf(const char* fmt, va_list args);
int vsprintf(char* buf, const char* fmt, va_list args);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int vfscanf(FILE* file, const char* fmt, va_list args);
int vscanf(const char* fmt, va_list args);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);
int vsscanf(const char* str, const char* fmt, va_list args);

#ifdef __cplusplus
}
#endif

#ifndef EOF
#define EOF (-1)
#endif
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#endif
