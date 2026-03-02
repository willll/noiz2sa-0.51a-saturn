#ifndef SDL_STDIO_WRAPPER_H
#define SDL_STDIO_WRAPPER_H

// Manual definitions to replace stddef.h (not available on Saturn)
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

// Additional declarations for Saturn compatibility
extern "C" void exit(int status);

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
