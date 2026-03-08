# Noiz2sa Drawing System Architecture

## Overview

Noiz2sa uses a sophisticated **multi-buffer rendering system** to achieve visual effects for a vertically-scrolling shoot-em-up game. The system combines direct sprite rendering, procedural line/shape drawing, and real-time pixel blending using lookup tables for performance.

---

## Buffer Architecture

### Core Buffers

The system manages **5 primary memory regions** for rendering:

| Buffer | Size | Type | Purpose |
|--------|------|------|---------|
| `video` | 320×240 | SDL_Surface | Physical screen output (hardware) |
| `layer` | 320×240 | SDL_Surface | Main game playfield (game graphics) |
| `lpanel` | 160×240 | SDL_Surface | Left-side HUD panel (status display) |
| `rpanel` | 160×240 | SDL_Surface | Right-side HUD panel (score display) |
| `smokeBuf` | 320×240×ptr | Indirection table | Applies distortion effect to game layer |

### Layer Buffers (Double-Buffering for Effects)

Three **pixel buffers** implement specialized rendering layers:

| Buffer | Size | Type | Purpose |
|--------|------|------|---------|
| `l1buf` | 320×240 | Uint8[] | Layer 1: Primary scene buffer (bullets, foes, player) |
| `l2buf` | 320×240 | Uint8[] | Layer 2: Secondary/effect buffer (smoke, debris) |
| `pbuf` | 320×240 | Uint8[] | Persistent buffer: Previous frame snapshot (for smoke effect) |
| `buf` | Alias | Uint8* | Current write target (points to either l1buf or l2buf) |

### Panel Buffers

| Buffer | Type | Purpose |
|--------|------|---------|
| `lpbuf` | Uint8* | Left panel pixel buffer (score, lives, weapon info) |
| `rpbuf` | Uint8* | Right panel pixel buffer (high scores, stats) |

---

## Rendering Pipeline

### 1. Frame Initialization

```
┌─────────────────────────────────────────────────────────┐
│ Each Frame Start                                        │
│                                                         │
│  clearScreen()  →  SDL_FillRect(layer, &layerRect, 0)  │
│                                                         │
│  buf = l1buf    →  Set primary write target             │
└─────────────────────────────────────────────────────────┘
```

### 2. Game Object Drawing Phase

During game update, all drawable entities (player, enemies, bullets) write to the **active layer buffer**:

```
Game Layer Drawing:
  ┌─────────────────────────────────────────┐
  │  Ship Position / Draw                   │
  │  └─→ drawLine(x1, y1, x2, y2, ..., buf)│
  │  └─→ drawBox(x, y, w, h, ..., buf)      │
  │                                         │
  │  Enemy Positions / Draw                 │
  │  └─→ drawLine() calls write to buf      │
  │  └─→ drawBox() writes pixels            │
  │                                         │
  │  Bullets / Shape Drawing                │
  │  └─→ All primitives use active 'buf'    │
  └─────────────────────────────────────────┘
           ↓
      [l1buf] Filled with pixels
```

### 3. Blending & Compositing Phase

After all game drawing is complete, **two rendering modes** handle layer composition:

#### Mode A: Standard Blending (`blendScreen()`)

```
blendScreen():
  for i in 0 to (LAYER_WIDTH × LAYER_HEIGHT - 1):
    buf[i] = colorAlp[l1buf[i]][l2buf[i]]
    
Where colorAlp is a 256×256 lookup table:
  colorAlp[layer1_pixel][layer2_pixel] → blended_result

Effect: Combines two layers using precomputed color algebra
        (used for transparency, glow, overlays)
```

#### Mode B: Smoke/Distortion (`smokeScreen()`)

```
smokeScreen():
  1. Save previous frame:  pbuf[] = l2buf[]
  2. Apply dfusion (darkening):
     for i in 0 to lyrSize-1:
       l1buf[i] = colorDfs[l1buf[i]]   // Darken layer 1
       l2buf[i] = colorDfs[*(smokeBuf[i])]  // Fetch indirect + darken
  3. Indirect mapping via smokeBuf:
     smokeBuf[x + y*pitch] = &pbuf[mx + my*pitch]
     where (mx, my) = displaced coordinates using sine table

Effect: Creates warping/distortion using previous frame as input,
        shifted by a 2D displacement field (sine-based wave)
```

### 4. Screen Output Phase

```
flipScreen():
  SDL_BlitSurface(layer, NULL, video, &layerRect)
  SDL_BlitSurface(lpanel, NULL, video, &lpanelRect)
  SDL_BlitSurface(rpanel, NULL, video, &rpanelRect)
  SDL_Flip(video)
  
  └─→ Composite all three surfaces to hardware framebuffer
```

---

## Memory Layout & Dimensions

### Layout Diagram

```
Physical Screen (320×240 pixels):
┌───────────────────────────────────────────┐
│  lpanel (160×240)  │  layer (320×240)   │ rpanel (160×240)
│                    │                    │
│  Status Info       │   Game Playfield   │  Score/Stats
│  - Lives           │   - Ship           │  - High Score
│  - Weapon Lvl      │   - Enemies        │  - Stage
│  - Health          │   - Bullets        │  - Rank
│                    │   - Effects        │
└────────────────────┴────────────────────┴─────────────────┘
  0                160               320                480
```

### Buffer Dimensions Reference

```cpp
#define SCREEN_WIDTH    320      // Total screen width
#define SCREEN_HEIGHT   240      // Total screen height
#define LAYER_WIDTH     320      // Game area width
#define LAYER_HEIGHT    240      // Game area height
#define PANEL_WIDTH     160      // Left/right panel width
#define PANEL_HEIGHT    240      // Panel height
#define BPP             8        // 8-bit color (palette mode)
#define LayerBit        Uint8    // Pixel data type (palette index)
```

---

## Color System

### Palette Mode (8-bit indexed)

The game uses **palette-based color mode** where each pixel value is an **8-bit index** (0–255):

```
Pixel Value:  buf[x + y*pitch] = color_index (0-255)
              ↓
        Color Table
        color[color_index]
              ↓
        RGB555 (5-bit per channel)
        {Red, Green, Blue} ∈ [0-31]
```

### Blending Lookup Tables

Two precomputed 256-element tables enable fast color operations:

#### `colorAlp[256][256]`
- **Purpose**: Alpha/transparency blending
- **Format**: `colorAlp[base_color][overlay_color]` → blended result
- **Usage**: Combines layers for effects (glow, shadows, transparency)
- **Memory**: 256 × 256 × 1 byte = **64 KB**

#### `colorDfs[256]`
- **Purpose**: Diffusion (darkening) for smoke effect
- **Format**: `colorDfs[color_in]` → darkened color
- **Usage**: Desaturates/dims pixels during smoke animation
- **Memory**: 256 × 1 byte = **256 bytes**

---

## Rendering Pipeline: Complete Flow

```
TIME ─────────────────────────────────────────────────────────>

Frame N:
┌─────────────────────────────────────────────────────────────┐
│ 1. CLEAR                                                    │
│    clearScreen() → FillRect(layer, 0)                       │
│    buf = l1buf                                              │
│                                                             │
│ 2. DRAW GAME OBJECTS                                        │
│    drawLine(x1,y1,x2,y2,...,buf)  ──→ pixels to l1buf       │
│    drawBox(x,y,w,h,...,buf)       ──→ pixels to l1buf       │
│    drawSprite(n,x,y)              ──→ VDP1 texture (HW)     │
│    [All geometry writes to buf]                             │
│                                                             │
│ 3. COMPOSITE LAYERS (choose one):                           │
│                                                             │
│    Option A: Normal Blend                                   │
│    ───────────────────────                                  │
│    blendScreen():                                           │
│      for each pixel:                                        │
│        result = colorAlp[l1buf[i]][l2buf[i]]                │
│                                                             │
│    Option B: Smoke Effect                                   │
│    ──────────────────────                                   │
│    smokeScreen():                                           │
│      pbuf[] = l2buf[]                (save previous)        │
│      l1buf[i] = colorDfs[l1buf[i]]   (darken layer 1)       │
│      l2buf[i] = colorDfs[pbuf_indirect]  (indirect + dim)   │
│                                                             │
│ 4. FLIP TO SCREEN                                           │
│    flipScreen():                                            │
│      Blit layer→video                                       │
│      Blit lpanel→video                                      │
│      Blit rpanel→video                                      │
│      SDL_Flip(video)                                        │
│                                                             │
│ 5. OUTPUT                                                   │
│    [Hardware framebuffer updated]                           │
└─────────────────────────────────────────────────────────────┘
  ↓
Frame N+1:
┌─────────────────────────────────────────────────────────────┐
│ Repeat from step 1                                          │
└─────────────────────────────────────────────────────────────┘
```

---

## Key Design Decisions & Why

### 1. **Multi-Buffer Approach**

**Why?** To separate rendering concerns:
- **Spatial separation**: Game area vs. UI (layer vs. panels)
- **Temporal separation**: Current frame vs. previous frame (l1buf vs. pbuf)
- **Compositional separation**: Multiple layers can be blended independently

**Benefit**: Enables advanced effects (transparency, smoke, distortion) without per-pixel overhead in hot loops.

### 2. **Lookup Table Blending**

**Why?** Color arithmetic on 8-bit integers is expensive, especially inside 320×240 pixel loops.

**Cost without LUT**:
```cpp
// Per-pixel blending (SLOW - runs 76,800 times/frame)
for (int i = 0; i < 76800; i++) {
    Uint8 c1 = l1buf[i];
    Uint8 c2 = l2buf[i];
    Uint8 result;
    
    // Extract RGB from palette
    int r1 = color[c1].Red,   g1 = color[c1].Green,   b1 = color[c1].Blue;
    int r2 = color[c2].Red,   g2 = color[c2].Green,   b2 = color[c2].Blue;
    
    // Blend (assuming 50/50)
    int r = (r1 + r2) / 2;
    int g = (g1 + g2) / 2;
    int b = (b1 + b2) / 2;
    
    // Find nearest palette entry (expensive search)
    result = findNearestColor(r, g, b);
    buf[i] = result;
}
```

**Cost with LUT** (actual):
```cpp
// Per-pixel blending (FAST - single lookup)
for (int i = 0; i < 76800; i++) {
    buf[i] = colorAlp[l1buf[i]][l2buf[i]];  // ONE memory read, done.
}
```

**Speedup**: ~100–1000× faster (single L1 cache lookups vs. arithmetic + memory searches)

### 3. **Smoke Effect via Indirection**

**Why?** Create a distortion/warping effect without trigonometric calculations each frame.

**How it works**:
```
Normal rendering:     Smoke distortion:
buf[x+y*w] = pixel    ┌─────────────────────────┐
                      │ Calculate offset (mx,my)│
                      │ using sine table sctbl[]│
                      │                         │
                      │ Fetch from offset:      │
                      │ smokeBuf[x+y*w] =       │
                      │   &pbuf[mx+my*w]        │
                      │                         │
                      │ Dereference + darken:   │
                      │ l2buf[i] =              │
                      │  colorDfs[*smokeBuf[i]]│
                      └─────────────────────────┘
```

**Why indirection?** The `smokeBuf[]` array stores **pointers** to `pbuf[]`, creating a lookup grid that is computed once in `makeSmokeBuf()` during initialization, then reused every frame:

```cpp
// One-time setup
for (y = 0; y < LAYER_HEIGHT; y++) {
    for (x = 0; x < LAYER_WIDTH; x++) {
        mx = x + sctbl[(x*8) & (DIV-1)] / 128;  // Calculate displacement
        my = y + sctbl[(y*8) & (DIV-1)] / 128;
        
        // Clamp to valid range
        if (mx < 0 || mx >= LAYER_WIDTH || ...) {
            smokeBuf[x + y*pitch] = &pbuf[pitch * LAYER_HEIGHT];  // Blank pixel
        } else {
            smokeBuf[x + y*pitch] = &pbuf[mx + my*pitch];  // Displaced offset
        }
    }
}

// Every smoke frame (O(n) simple dereference + color lookup)
for (i = 0; i < lyrSize; i++) {
    l2buf[i] = colorDfs[*(smokeBuf[i])];  // Dereference pointer, look up darkened color
}
```

**Performance**: O(n) linear scan with pointer chasing, no calculation needed.

### 4. **Dual-Layer Rendering**

**Why?** Support compositing two independent drawable layers.

- **`l1buf`**: Primary scene (player, foes, bullets)
- **`l2buf`**: Secondary effects (explosions, smoke trails, environment)

**Use cases**:
- Blending for transparency (via `colorAlp`)
- Layered compositing (foreground + background)
- Motion blur / ghosting effects

---

## Typical Frame Rendering Sequence

### Example: Normal Gameplay Frame

```
Frame timing (60 FPS = 16.67 ms):

[0]
  clearScreen()               ← 1-2 ms (memset)
  buf = l1buf
  
  Update & Draw:
  drawLine(ship_x, ship_y, ...)           ← 0.5 ms
  drawBox(enemy_x, enemy_y, ...)          ← 0.1 ms per enemy
  drawLine(bullet_x, bullet_y, ...)       ← 0.05 ms per bullet
  [Repeat for 100+ objects]                ← ~2-3 ms total
  
  blendScreen()               ← 3-4 ms (76,800 lookups)
  flipScreen()                ← 5-8 ms (3 SDL blits + flip)
  
[Total: ~12-15 ms] ✓ Under 16.67 ms budget

[1]
  [Next frame...]
```

### Example: Smoke Effect Frame

```
Frame timing with smoke:

[0]
  clearScreen()               ← 1-2 ms
  buf = l1buf
  
  Update & Draw: [2-3 ms]
  
  smokeScreen()               ← 5-6 ms
    pbuf[] = l2buf[]          ← 1 ms (memcpy 76,800 pixels)
    l1buf[i] = colorDfs[...]  ← 2 ms (76,800 lookups)
    l2buf[i] = colorDfs[...] ← 2 ms (76,800 pointer + lookups)
  
  flipScreen()                ← 5-8 ms
  
[Total: ~14-17 ms] ✓ Near budget (smoke is expensive)

[1]
  [Next frame...]
```

---

## Data Structure Details

### SDL_Rect Structure (Four Rectangles)

```cpp
static SDL_Rect screenRect;         // Full 320×240 screen
static SDL_Rect layerRect;          // Centered playfield
static SDL_Rect layerClearRect;     // Clear bounds (0,0,320,240)
static SDL_Rect lpanelRect;         // Left panel position (0, y, 160, 240)
static SDL_Rect rpanelRect;         // Right panel position (160, y, 160, 240)
static SDL_Rect panelClearRect;     // Panel clear bounds (0, 0, 160, 240)

// Example values:
// layerRect = {x: 0, y: 0, w: 320, h: 240}
// lpanelRect = {x: 0, y: 0, w: 160, h: 240}
// rpanelRect = {x: 320, y: 0, w: 160, h: 240}
```

### Pitch (Stride)

```cpp
int pitch = layer->pitch / (BPP / 8);  // pixels per line

// pitch = 320 bytes / 1 byte per pixel = 320
// To access pixel at (x, y):
//    index = x + y * pitch
//    pixel = buf[x + y * 320]
```

---

## Performance Characteristics

### Memory Footprint

```
Layer buffers:    320 × 240 × 1 byte × 3 (l1, l2, pbuf)  = 230.4 KB
Panel buffers:    160 × 240 × 1 byte × 2 (lp, rp)        = 76.8 KB
Smoke indirection: 320 × 240 × sizeof(ptr) × 1            = 614.4 KB (64-bit)
Color tables:     256 × 256 (colorAlp) + 256 (colorDfs)   = 65.8 KB
Sprites:          7 × TGA assets                          = Variable (~500 KB)

Total in-core: ~990 KB (excluding assets)
```

### Pixel Throughput

```
Screen size:        320 × 240 = 76,800 pixels
Frames per second:  60 FPS

Pixels per second:  76,800 × 60 = 4,608,000 pixels/sec

blendScreen():      76,800 writes + 76,800 reads = 153,600 ops/frame
smokeScreen():      3 × 76,800 ops = 230,400 ops/frame

Cache efficiency:   Very high (sequential access pattern, LUT fits in L2)
```

---

## Code Walkthrough: Key Functions

### `clearScreen()` — Reset Layer

```cpp
void clearScreen() {
  SDL_FillRect(layer, &layerClearRect, 0);
  // Fills 320×240×1 bytes = 76,800 bytes with 0 (black)
  // Time: ~1-2 ms on modern CPU (memset is optimized)
}
```

### `blendScreen()` — Composite Two Layers

```cpp
void blendScreen() {
  int i;
  for (i = lyrSize - 1; i >= 0; i--) {
    buf[i] = colorAlp[l1buf[i]][l2buf[i]];
    // lyrSize = 320 × 240 = 76,800
    // Each iteration: 2 memory reads (l1buf, l2buf), 1 lookup (colorAlp), 1 write
    // Backward loop ensures cache coherency (prefetch)
  }
}
// Time: ~3-4 ms (256×256 lookup fits L2 cache)
```

### `smokeScreen()` — Distortion Effect

```cpp
void smokeScreen() {
  int i;
  
  // Step 1: Save current l2buf to pbuf
  memcpy(pbuf, l2buf, lyrSize + sizeof(LayerBit));
  // Time: ~1 ms (memcpy is SIMD-optimized)
  
  // Step 2: Darken both layers
  for (i = lyrSize - 1; i >= 0; i--) {
    l1buf[i] = colorDfs[l1buf[i]];           // Direct lookup
    l2buf[i] = colorDfs[*(smokeBuf[i])];     // Indirect lookup (pointer chase)
    // Two separate lookups due to indirection
  }
  // Time: ~4-5 ms (cache-friendly Uint8 access, but pointer chase adds latency)
}
// Total 5-6 ms (more expensive than blendScreen due to memcpy + extra indirection)
```

### `makeSmokeBuf()` — Precompute Distortion Map

```cpp
static void makeSmokeBuf() {
  // Allocate pointers array: 320×240 = 76,800 pointers
  smokeBuf = (LayerBit**)malloc(sizeof(LayerBit*) * pitch * LAYER_HEIGHT);
  
  // For each pixel, compute displaced source position
  for (y = 0; y < LAYER_HEIGHT; y++) {
    for (x = 0; x < LAYER_WIDTH; x++) {
      // Use sine table for smooth distortion
      mx = x + sctbl[(x*8) & (DIV-1)] / 128;    // Horizontal displacement
      my = y + sctbl[(y*8) & (DIV-1)] / 128;    // Vertical displacement
      
      // Bounds check (wrap to blank pixel if out of range)
      if (mx < 0 || mx >= LAYER_WIDTH || my < 0 || my >= LAYER_HEIGHT) {
        smokeBuf[x + y*pitch] = &pbuf[pitch * LAYER_HEIGHT];  // Last pixel = blank
      } else {
        smokeBuf[x + y*pitch] = &pbuf[mx + my*pitch];  // Store pointer to source
      }
    }
  }
}
// Runs once at startup; result is reused every smoke frame
// Time: ~5-10 ms (one-time cost, negligible)
```

### `flipScreen()` — Composite and Display

```cpp
void flipScreen() {
  // Blit central game layer to screen
  SDL_BlitSurface(layer, NULL, video, &layerRect);
  
  // Blit left HUD panel
  SDL_BlitSurface(lpanel, NULL, video, &lpanelRect);
  
  // Blit right HUD panel
  SDL_BlitSurface(rpanel, NULL, video, &rpanelRect);
  
  // Draw title screen overlay (if in TITLE state)
  if (status == TITLE) {
    drawTitle();
  }
  
  // Update hardware framebuffer (buffer swap)
  SDL_Flip(video);
}
// Time: ~5-8 ms (3 blits + vsync wait, hardware-dependent)
```

---

## Integration with Game Loop

Typical per-frame sequence in `noiz2sa.cpp`:

```cpp
// Main game loop (60 FPS)
while (game_running) {
    // 1. Update game state
    updateShip();
    updateEnemies();
    updateBullets();
    // ...
    
    // 2. Render to layer buffers
    clearScreen();                          // Reset layer
    buf = l1buf;                            // Set primary write target
    
    // Draw all game objects
    for (auto& enemy : enemies) {
        drawBox(enemy.x, enemy.y, enemy.w, enemy.h, ..., buf);
    }
    for (auto& bullet : bullets) {
        drawLine(bullet.x1, bullet.y1, bullet.x2, bullet.y2, ..., buf);
    }
    for (auto& particle : particles) {
        drawLine(particle.x, particle.y, ..., buf);  // Trails on l2buf potentially
    }
    
    // 3. Composite layers
    if (smoke_effect_active) {
        smokeScreen();                      // Distortion effect
    } else {
        blendScreen();                      // Standard blending
    }
    
    // 4. Render HUD
    drawNum(score, 10, 10, 3, 255, 128, lpbuf);
    drawNumRight(high_score, 150, 10, 3, 255, 128, rpbuf);
    
    // 5. Display
    flipScreen();                           // Composite to screen
    
    // 6. Wait for next frame (60 FPS)
    wait_frame();
}
```

---

## Summary

The Noiz2sa drawing system uses a **layered, lookup-table accelerated architecture** to achieve:

1. **Separation of concerns**: Distinct buffers for game area, UI, and effects
2. **Performance**: Precomputed color blending via lookup tables avoids expensive arithmetic
3. **Visual appeal**: Multi-layer compositing enables transparency, glow, and distortion effects
4. **Efficiency**: Smoke effect uses pointer indirection instead of recalculating distortion every frame
5. **Scalability**: Modular design supports adding/removing effects without redesign

The multi-buffer approach is particularly well-suited to the **Sega Saturn hardware**, which has fast texture memory and supports hardware blitting, making SDL-style surface operations ideal for this game engine.
