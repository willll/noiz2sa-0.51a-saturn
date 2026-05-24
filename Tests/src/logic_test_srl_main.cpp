/* Tests/src/logic_test_srl_main.cpp
 *
 * SRL-aware startup wrapper for standalone logic tests (SH2 target only).
 *
 * - Wraps printf() -> SRL::Logger::LogInfo so all test printf() output
 *   appears in the mednafen emulator log (SRL_LOG_OUTPUT=EMULATOR).
 * - Provides int main() that emits ***UT_START*** then calls
 *   logic_test_main() (the renamed entry point in each test source file).
 *
 * Build system: the add_sh2_logic_test() CMake function includes this file
 * and passes -include logic_test_compat.h so the compiler replaces every
 * printf() call with logic_test_printf() below.
 */

#include <srl.hpp>
#include <srl_log.hpp>
#include <srl_memory.hpp>
#include <stdarg.h>
#include <string.h> /* strlen */

/* Forward-declare vsnprintf directly: dummy/stdio.h (on the SH2 include path)
 * only exposes a no-op printf macro and never includes the real <stdio.h>, so
 * we bypass that by declaring vsnprintf ourselves.  The implementation lives in
 * newlib and is always available at link time. */
extern "C" int vsnprintf(char *, __SIZE_TYPE__, const char *, va_list);

using namespace SRL::Logger;
using namespace SRL::Types;

/* Intercept all printf() calls and route through SRL Logger so output is
 * visible in mednafen's emulator log.  Called via the #define in
 * logic_test_compat.h which is force-included by the -include compile flag. */
extern "C" int logic_test_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Strip trailing newline(s) — SRL Logger adds its own line separator. */
    int len = (int)strlen(buf);
    while (len > 0 && buf[len - 1] == '\n')
        buf[--len] = '\0';

    if (len > 0)
        LogInfo("%s", buf);

    return len;
}

/* Defined in the individual test source file (renamed from main()). */
extern "C" int logic_test_main();

int main()
{
    SRL::Core::Initialize(HighColor(20, 10, 50));
    LogInfo("***UT_START***");
    return logic_test_main();
    /* Note: logic_test_main() emits ***UT_END*** via the wrapped printf. */
}
