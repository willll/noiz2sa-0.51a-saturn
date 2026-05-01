---
description: "Use when writing, editing, or reviewing C++ source files in src/. Covers naming conventions, Fxp math, SRL integration, global state, memory management, and entity patterns for the Sega Saturn target."
applyTo: "src/**/*.{cpp,cc,h}"
---

# Source Code Conventions (src/)

## Naming

| Category | Pattern | Example |
|----------|---------|---------|
| Classes | `PascalCase` | `Canvas`, `FoeCommand`, `LoadingScreen` |
| Structs | `snake_case` (typedef'd to `PascalCase`) | `struct foe` → `typedef struct foe Foe` |
| Functions | `camelCase` | `addFoe()`, `moveShip()`, `initBarragemanager()` |
| Cross-module globals | bare or `g_` prefix, `extern` in header | `extern int foeCnt;`, `extern RandomGenerator* g_random;` |
| Module-local globals | `static`, bare lowercase or `s` prefix | `static int sLiveActiveBullets = 0;` |
| Compile-time constants/macros | `SCREAMING_SNAKE_CASE` | `#define FOE_MAX 1024` |
| File-scope `constexpr` | `kCamelCase` | `static constexpr int kMaxActiveBullets = 160;` |

## Fixed-Point Math (Fxp)

- Use `Fxp` (from `SRL::Math::Types`) for BulletML direction/speed values and rank-based difficulty scaling.
- Use plain `int` for screen coordinates (stored as `int << 8`), frame counters, and legacy direction (0–1023 range where 1024 = 360°).
- **Never use `float` or `double`** — the SH-2 processor has no FPU.
- Convert between Fxp and legacy ints via the helpers in `foecommand.cc` (`fxpToLegacyInt`, `fxpToLegacyScaledInt`).

```cpp
// Correct: Fxp for BulletML rank
Fxp rank = Fxp(1) + Fxp(stage) * Fxp(0.1f);  // compile-time constant OK

// Correct: int for screen position
int x = ship.pos.x >> 8;  // sub-pixel to pixel
```

## SRL Namespace Imports

Import SRL namespaces at file scope (not inside functions or classes). Use the most specific import needed:

```cpp
using SRL::Math::Types::Fxp;          // preferred for single type
using namespace SRL::Logger;           // LogInfo, LogDebug, LogFatal
using namespace SRL::Input;            // Digital, pad state
using namespace SRL::Types;            // HighColor, etc.
```

## Global State

- All cross-module globals are declared in [`noiz2sa.h`](../noiz2sa.h) — **check there before adding new globals**.
- Define globals in the corresponding `.cpp` file; declare `extern` in the header.
- Module-private arrays/pools (e.g., entity pools) are `static` in the `.cc`/`.cpp` file — not exposed via headers.

## Memory Management

- Use manual `new`/`delete` — no `std::unique_ptr` or `std::shared_ptr`.
- For array allocations use `new[]` / `delete[]`.
- Null the pointer after deletion to prevent dangling references.
- Deleted copy constructor/assignment on classes that own heap resources:

```cpp
Canvas(const Canvas&) = delete;
Canvas& operator=(const Canvas&) = delete;
```

- **No C++ exceptions** — use `SRL::Logger::LogFatal()` + `SRL::System::Exit(1)` for unrecoverable errors.

## Error Handling

```cpp
if (!ptr) {
    SRL::Logger::LogFatal("[MODULE] Failed to allocate: %s", name);
    SRL::System::Exit(1);
}
```

Use `LogWarning()` for recoverable issues, `LogInfo()` for startup milestones, `LogDebug()` for detailed traces (compiled out in release builds).

## File Structure

- **New files**: use `#pragma once` and `.cpp` extension.
- **Legacy files** (`.cc`): use `#ifndef GUARD_H_` guards — preserve when editing, don't convert unless migrating the whole file.
- When migrating a `.cc` file to `.cpp`: switch to `#pragma once`, convert `typedef struct` patterns to `struct` with a `using` alias if appropriate, update the `CMakeLists.txt` source list.

## Entity Pattern

Entities (`Foe`, `Ship`, `Shot`) are plain C-style structs with free functions — not classes with methods:

```cpp
// Struct in .h
typedef struct { Vector pos; int cnt; } Ship;
extern Ship ship;

// Functions in .cpp — named after the entity
void moveShip();
void drawShip();
```

Do not add member functions or constructors to these legacy structs unless refactoring the entire entity to a class.

## Header Include Order

1. Paired header (`"foe.h"` in `foe.cc`)
2. Project headers (`"noiz2sa.h"`, `"vector.h"`, etc.)
3. SDL headers (`"SDL.h"`)
4. Standard library headers (`<cstdint>`, `<stdlib.h>`)
5. SRL headers (`<srl.hpp>`, `<srl_log.hpp>`)
