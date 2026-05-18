#pragma once
/* Force-included header (-include compile flag) that redirects printf() to
 * SRL Logger for Saturn SH2 logic test targets.
 *
 * The dummy/stdio.h in SRL::Core's interface include path defines printf as
 * a no-op macro. This header: (1) triggers its #pragma once, (2) undefines
 * the no-op, (3) redirects printf to logic_test_printf (defined in
 * logic_test_srl_main.cpp which calls LogInfo). */
#include <stdio.h>   /* gets dummy/stdio.h -> pragma-once, printf = ((void)0) */
#undef printf
#ifdef __cplusplus
extern "C"
#endif
int logic_test_printf(const char *fmt, ...);
#define printf logic_test_printf
