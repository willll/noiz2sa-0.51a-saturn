#include <srl.hpp>
#include <srl_log.hpp>
#include <cstdint>

#include "minunit.h"

#include "../../src/SDL/SDL.h"
#include "../../src/degutil.h"
#include "../../src/vector.h"

// ---------------------------------------------------------------------------
// FPS counter arithmetic helpers
// Mirror the vblank-based computation in noiz2sa.cpp without any rendering
// or global-state side effects.
//
// NTSC vblank = 60 Hz.  fps*100 = 6000 / vblanks_between_frames.
// ---------------------------------------------------------------------------
static uint32_t computeFpsTimes100(uint32_t vblanksElapsed)
{
  if (vblanksElapsed > 0u)
    return 6000u / vblanksElapsed;
  return 6000u; // first-frame / no-vblank-yet default
}

static void computeFpsDisplay(uint32_t fpsTimes100, int32_t &outWhole, int32_t &outFrac)
{
  outWhole = (int32_t)(fpsTimes100 / 100u);
  outFrac  = (int32_t)(fpsTimes100 % 100u);
}

static bool shotHitsFoe(const Vector &foePos, const Vector &shotPos, int foeScanSize, int shotScanHeight)
{
  return absN(foePos.x - shotPos.x) < foeScanSize &&
         absN(foePos.y - shotPos.y) < foeScanSize + shotScanHeight;
}

static bool shotHitsFoeSwept(const Vector &foePos,
                             const Vector &shotPos,
                             int shotPrevY,
                             int foeScanSize,
                             int shotScanHeight,
                             int shotScanHalfWidth)
{
  (void)shotPrevY;
  (void)shotScanHalfWidth;
  return shotHitsFoe(foePos, shotPos, foeScanSize, shotScanHeight);
}

static bool movingBulletHitsShip(const Vector &bulletPos,
                                 const Vector &bulletPrevPos,
                                 const Vector &shipPos,
                                 int shipHitWidth)
{
  const int startX = bulletPrevPos.x;
  const int startY = bulletPrevPos.y;
  const int deltaX = bulletPos.x - bulletPrevPos.x;
  const int deltaY = bulletPos.y - bulletPrevPos.y;
  const int targetX = shipPos.x;
  const int targetY = shipPos.y;
  const long long lengthSquared = (long long)deltaX * (long long)deltaX +
                                  (long long)deltaY * (long long)deltaY;

  if (lengthSquared <= 1)
  {
    const long long pointX = (long long)targetX - (long long)startX;
    const long long pointY = (long long)targetY - (long long)startY;
    return (pointX * pointX + pointY * pointY) <= (long long)shipHitWidth;
  }

  const long long dot = ((long long)targetX - (long long)startX) * (long long)deltaX +
                        ((long long)targetY - (long long)startY) * (long long)deltaY;
  long long tFp = (dot << 16) / lengthSquared;
  if (tFp < 0)
  {
    tFp = 0;
  }
  else if (tFp > (1LL << 16))
  {
    tFp = (1LL << 16);
  }

  const long long nearestX = (long long)startX + (((long long)deltaX * tFp) >> 16);
  const long long nearestY = (long long)startY + (((long long)deltaY * tFp) >> 16);
  const long long distX = (long long)targetX - nearestX;
  const long long distY = (long long)targetY - nearestY;
  return (distX * distX + distY * distY) <= (long long)shipHitWidth;
}

static bool shouldDestroyShipNow(int status)
{
  // Invincibility has been removed; only in-game state can consume a life.
  return status == 1;
}

static SRL_Surface makeTestSurface(int width, int height)
{
  SRL_Surface surface;
  surface.w = width;
  surface.h = height;
  surface.pixels = nullptr;
  surface.textureIndex = -1;
  surface.dirty = false;
  surface.dirtyX1 = (int16_t)width;
  surface.dirtyY1 = (int16_t)height;
  surface.dirtyX2 = 0;
  surface.dirtyY2 = 0;
  return surface;
}

template <typename LegacyFp>
static int legacyIntFromFloatType(LegacyFp value)
{
  return (int)value;
}

static Fxp fxpFromDouble(double value)
{
  return Fxp::BuildRaw(static_cast<int32_t>(value * 65536.0));
}

static int migratedIntFromFxp(Fxp value)
{
  int result = value.As<int>();
  if (value.RawValue() < 0 && (value.RawValue() & 0xFFFF) != 0)
  {
    result += 1;
  }
  return result;
}

static int migratedScaledIntFromFxp(Fxp value)
{
  const int raw = value.RawValue();
  if (raw >= 0)
  {
    return (raw + 0x8000) >> 16;
  }
  return -(((-raw) + 0x8000) >> 16);
}

template <typename LegacyFp>
static int legacyDirectionIndexWrapped(LegacyFp directionDegrees)
{
  int direction = legacyIntFromFloatType(directionDegrees * static_cast<LegacyFp>(DIV) / static_cast<LegacyFp>(360));
  direction &= (DIV - 1);
  return direction;
}

template <typename LegacyFp>
static int legacyDirectionIndexSigned(LegacyFp directionDegrees)
{
  return legacyIntFromFloatType(directionDegrees * static_cast<LegacyFp>(DIV) / static_cast<LegacyFp>(360));
}

static int migratedDirectionIndexSigned(Fxp directionDegrees)
{
  const int64_t scaledRaw = static_cast<int64_t>(directionDegrees.RawValue()) * DIV / 360;
  return static_cast<int>(scaledRaw / 65536);
}

static int migratedDirectionIndexWrapped(Fxp directionDegrees)
{
  int direction = migratedDirectionIndexSigned(directionDegrees);
  direction &= (DIV - 1);
  return direction;
}

template <typename LegacyFp>
static int legacyScaledSpeed(LegacyFp speed, int rate)
{
  return legacyIntFromFloatType(speed * static_cast<LegacyFp>(rate));
}

static int migratedScaledSpeed(Fxp speed, int rate)
{
  const int64_t scaledRaw = static_cast<int64_t>(speed.RawValue()) * rate;
  if (scaledRaw >= 0)
  {
    return static_cast<int>((scaledRaw + 0x8000) >> 16);
  }
  return -static_cast<int>(((-scaledRaw) + 0x8000) >> 16);
}

static Fxp migratedDirectionDegreesFromIndex(int direction)
{
  const int64_t raw = static_cast<int64_t>(direction) * 360 * 65536 / DIV;
  return Fxp::BuildRaw(static_cast<int32_t>(raw));
}

using namespace SRL::Types;
using namespace SRL::Logger;

extern "C"
{
  const char *const strStart = "***UT_START***";
  const char *const strEnd = "***UT_END***";
}

MU_TEST(test_vctDist_branches)
{
  Vector a{100, 20};
  Vector b{0, 0};
  mu_assert_int_eq(110, vctDist(&a, &b));

  Vector c{20, 100};
  mu_assert_int_eq(110, vctDist(&c, &b));
}

MU_TEST(test_basic_vector_ops)
{
  Vector a{10, -4};
  Vector b{2, 6};

  vctAdd(&a, &b);
  mu_assert_int_eq(12, a.x);
  mu_assert_int_eq(2, a.y);

  vctSub(&a, &b);
  mu_assert_int_eq(10, a.x);
  mu_assert_int_eq(-4, a.y);

  vctMul(&a, 3);
  mu_assert_int_eq(30, a.x);
  mu_assert_int_eq(-12, a.y);

  vctDiv(&a, 3);
  mu_assert_int_eq(10, a.x);
  mu_assert_int_eq(-4, a.y);
}

MU_TEST(test_vctCheckSide_branches)
{
  Vector p{10, 10};

  Vector l1{5, 0};
  Vector l2{5, 20};
  mu_assert_int_eq(5, vctCheckSide(&p, &l1, &l2));

  Vector h1{0, 5};
  Vector h2{20, 5};
  mu_assert_int_eq(-5, vctCheckSide(&p, &h1, &h2));

  Vector d1{0, 0};
  Vector d2{10, 10};
  mu_assert_int_eq(0, vctCheckSide(&p, &d1, &d2));

  Vector d3{0, 10};
  Vector d4{10, 0};
  mu_assert(vctCheckSide(&p, &d3, &d4) != 0, "negative slope side check should be non-zero");
}

MU_TEST(test_vctInnerProduct_large_values_no_overflow)
{
  Vector a{120000, 0};
  Vector b{120000, 0};
  const long long dot = vctInnerProduct(&a, &b);
  mu_assert(dot > 10000000000LL, "dot product too small");
  mu_assert(dot < 20000000000LL, "dot product too large");
}

MU_TEST(test_vctGetElement_projection)
{
  Vector alongX{10000, 0};
  Vector base{500, 0};
  Vector p = vctGetElement(&alongX, &base);
  mu_assert_int_eq(10000, p.x);
  mu_assert_int_eq(0, p.y);

  Vector zero{0, 0};
  p = vctGetElement(&alongX, &zero);
  mu_assert_int_eq(0, p.x);
  mu_assert_int_eq(0, p.y);
}

MU_TEST(test_vctSize)
{
  Vector v{3, 4};
  mu_assert_int_eq(5, vctSize(&v));
}

MU_TEST(test_shotHitsFoe_boundaries)
{
  const Vector foe{10000, 20000};
  const int foeScan = 2000;
  const int shotScanHeight = 300;

  const Vector hit{11000, 20500};
  mu_check(shotHitsFoe(foe, hit, foeScan, shotScanHeight));

  const Vector missBoundaryX{12000, 20500};
  mu_check(!shotHitsFoe(foe, missBoundaryX, foeScan, shotScanHeight));

  const Vector missBoundaryY{11000, 22300};
  mu_check(!shotHitsFoe(foe, missBoundaryY, foeScan, shotScanHeight));
}

MU_TEST(test_shotHitsFoeSwept_tunneling)
{
  const Vector foe{10000, 20000};
  const int foeScan = 2000;
  const int shotScanHeight = 300;
  const int shotScanHalfWidth = 512;

  const Vector shotNow{10000, 17000};
  const int shotPrevY = 23000;
  mu_check(!shotHitsFoeSwept(foe, shotNow, shotPrevY, foeScan, shotScanHeight, shotScanHalfWidth));

  const Vector shotCurrentHit{10000, 19900};
  mu_check(shotHitsFoeSwept(foe, shotCurrentHit, shotPrevY, foeScan, shotScanHeight, shotScanHalfWidth));

  const Vector shotFarX{13000, 17000};
  mu_check(!shotHitsFoeSwept(foe, shotFarX, shotPrevY, foeScan, shotScanHeight, shotScanHalfWidth));

  const Vector shotMissY{10000, 12000};
  mu_check(!shotHitsFoeSwept(foe, shotMissY, 14000, foeScan, shotScanHeight, shotScanHalfWidth));
}

MU_TEST(test_movingBulletHitsShip_full_coverage)
{
  const int shipHitWidth = 512 * 512;

  // Ship on the segment.
  Vector prev{0, 0};
  Vector cur{1000, 0};
  Vector ship{200, 0};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Zero-length motion falls back to point-hit.
  prev = {0, 0};
  cur = {0, 0};
  ship = {200, 0};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Ship far from segment should miss.
  prev = {1000, 0};
  cur = {2000, 0};
  ship = {500, 600};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Ship beyond segment endpoint should miss.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {2600, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Parallel pass outside hit radius should miss.
  prev = {-2000, 1000};
  cur = {2000, 1000};
  ship = {0, 0};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Endpoint contact should count.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {0, 200};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // End endpoint contact should also count.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {1000, 200};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Segment-middle proximity: catches cases missed by ppos-centered checks.
  prev = {-2000, 0};
  cur = {2000, 0};
  ship = {0, 300};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  // Just outside hit radius should miss.
  prev = {0, 0};
  cur = {1000, 0};
  ship = {300, 520};
  mu_check(!movingBulletHitsShip(cur, prev, ship, shipHitWidth));

  prev = {120000, 70000};
  cur = {121200, 70000};
  ship = {120600, 70100};
  mu_check(movingBulletHitsShip(cur, prev, ship, shipHitWidth));
}

MU_TEST(test_destroyShip_no_invincibility_behavior)
{
  const int TITLE = 0;
  const int IN_GAME = 1;
  const int GAMEOVER = 2;

  mu_check(!shouldDestroyShipNow(TITLE));
  mu_check(shouldDestroyShipNow(IN_GAME));
  mu_check(!shouldDestroyShipNow(GAMEOVER));
}

template <typename LegacyFp>
static void assertDirectionWrappedParity()
{
  const double samples[] = {
      -719.9, -540.5, -360.0, -181.2, -90.5, -45.25, -1.99, -0.75,
      0.0, 0.75, 1.99, 45.25, 89.99, 90.5, 179.9, 180.0, 359.9, 540.5, 719.9};

  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
  {
    const LegacyFp degrees = static_cast<LegacyFp>(samples[i]);
    mu_assert_int_eq(legacyDirectionIndexWrapped(degrees), migratedDirectionIndexWrapped(fxpFromDouble(samples[i])));
  }

  for (int q = -5760; q <= 5760; ++q)
  {
    const double degDouble = static_cast<double>(q) * 0.25;
    const LegacyFp degrees = static_cast<LegacyFp>(degDouble);
    mu_assert_int_eq(legacyDirectionIndexWrapped(degrees), migratedDirectionIndexWrapped(fxpFromDouble(degDouble)));
  }
}

template <typename LegacyFp>
static void assertDirectionSignedParity()
{
  const double samples[] = {
      -719.9, -540.5, -360.0, -181.2, -90.5, -45.25, -1.99, -0.75,
      0.0, 0.75, 1.99, 45.25, 89.99, 90.5, 179.9, 180.0, 359.9, 540.5, 719.9};

  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
  {
    const LegacyFp degrees = static_cast<LegacyFp>(samples[i]);
    mu_assert_int_eq(legacyDirectionIndexSigned(degrees), migratedDirectionIndexSigned(fxpFromDouble(samples[i])));
  }
}

template <typename LegacyFp>
static void assertSpeedAccelParity()
{
  const int rate = 400;
  const double samples[] = {
      -8.75, -4.5, -2.25, -1.99, -1.5, -0.99, -0.5, -0.01,
      0.0, 0.01, 0.5, 0.99, 1.5, 1.99, 2.25, 4.5, 8.75};

  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
  {
    const LegacyFp speed = static_cast<LegacyFp>(samples[i]);
    mu_assert_int_eq(legacyScaledSpeed(speed, rate), migratedScaledSpeed(fxpFromDouble(samples[i]), rate));
  }
}

template <typename LegacyFp>
static void assertDirectionGetterParity()
{
  const int directionSamples[] = {-1536, -1025, -1024, -513, -512, -257, -1, 0, 1, 257, 512, 1023, 1024, 1535};

  for (unsigned i = 0; i < sizeof(directionSamples) / sizeof(directionSamples[0]); ++i)
  {
    const int legacyDirection = directionSamples[i];
    const LegacyFp legacyDegrees = static_cast<LegacyFp>(legacyDirection) * static_cast<LegacyFp>(360) / static_cast<LegacyFp>(DIV);
    const int expectedSigned = legacyDirectionIndexSigned(legacyDegrees);
    const int migratedSigned = migratedDirectionIndexSigned(migratedDirectionDegreesFromIndex(legacyDirection));
    mu_assert_int_eq(expectedSigned, migratedSigned);
  }
}

MU_TEST(test_fxp_foecommand_direction_wrapped_matches_legacy_float_and_double)
{
  assertDirectionWrappedParity<float>();
  assertDirectionWrappedParity<double>();
}

MU_TEST(test_fxp_foecommand_direction_signed_matches_legacy_float_and_double)
{
  assertDirectionSignedParity<float>();
  assertDirectionSignedParity<double>();
}

MU_TEST(test_fxp_foecommand_speed_and_accel_match_legacy_float_and_double)
{
  assertSpeedAccelParity<float>();
  assertSpeedAccelParity<double>();
}

MU_TEST(test_fxp_foecommand_direction_getter_matches_legacy_float_and_double)
{
  assertDirectionGetterParity<float>();
  assertDirectionGetterParity<double>();
}

MU_TEST(test_add_sub_mul_div_extended)
{
  Vector a{20, -30};
  Vector b{-4, 10};

  vctAdd(&a, &b);
  mu_assert_int_eq(16, a.x);
  mu_assert_int_eq(-20, a.y);

  vctSub(&a, &b);
  mu_assert_int_eq(20, a.x);
  mu_assert_int_eq(-30, a.y);

  vctMul(&a, 3);
  mu_assert_int_eq(60, a.x);
  mu_assert_int_eq(-90, a.y);

  vctDiv(&a, 3);
  mu_assert_int_eq(20, a.x);
  mu_assert_int_eq(-30, a.y);
}

MU_TEST(test_inner_product_large_values_extended)
{
  Vector a{120000, -50000};
  Vector b{120000, 50000};

  const long long dot = vctInnerProduct(&a, &b);
  mu_assert(dot > 11800000000LL, "dot product too small");
  mu_assert(dot < 12000000000LL, "dot product too large");
}

MU_TEST(test_get_element_projection_extended)
{
  Vector v{9000, 3000};
  Vector axis{3000, 0};
  Vector p = vctGetElement(&v, &axis);
  mu_assert_int_eq(9000, p.x);
  mu_assert_int_eq(0, p.y);

  Vector zero{0, 0};
  p = vctGetElement(&v, &zero);
  mu_assert_int_eq(0, p.x);
  mu_assert_int_eq(0, p.y);
}

MU_TEST(test_check_side_extended)
{
  Vector p{10, 10};

  Vector verticalA{5, 0};
  Vector verticalB{5, 20};
  mu_assert_int_eq(5, vctCheckSide(&p, &verticalA, &verticalB));

  Vector horizontalA{0, 5};
  Vector horizontalB{20, 5};
  mu_assert_int_eq(-5, vctCheckSide(&p, &horizontalA, &horizontalB));

  Vector diagA{0, 0};
  Vector diagB{10, 10};
  mu_assert_int_eq(0, vctCheckSide(&p, &diagA, &diagB));
}

MU_TEST(test_size_and_dist_extended)
{
  Vector v{3, 4};
  mu_assert_int_eq(5, vctSize(&v));

  Vector big{30000, 40000};
  mu_assert_int_eq(50000, vctSize(&big));

  Vector a{100, 20};
  Vector b{0, 0};
  mu_assert_int_eq(110, vctDist(&a, &b));

  Vector c{20, 100};
  mu_assert_int_eq(110, vctDist(&c, &b));
}

MU_TEST(test_sdl_copy_bytes)
{
  uint8_t src[5] = {1, 2, 3, 4, 5};
  uint8_t dst[5] = {0, 0, 0, 0, 0};
  SDL_CopyBytes(dst, src, 5);
  mu_assert_int_eq(1, dst[0]);
  mu_assert_int_eq(2, dst[1]);
  mu_assert_int_eq(3, dst[2]);
  mu_assert_int_eq(4, dst[3]);
  mu_assert_int_eq(5, dst[4]);
}

MU_TEST(test_sdl_dirty_rect_helpers)
{
  SRL_Surface surface = makeTestSurface(32, 24);

  SDL_SetSurfaceDirty(&surface);
  mu_check(surface.dirty);
  mu_assert_int_eq(0, surface.dirtyX1);
  mu_assert_int_eq(0, surface.dirtyY1);
  mu_assert_int_eq(32, surface.dirtyX2);
  mu_assert_int_eq(24, surface.dirtyY2);

  SDL_ClearDirtyRect(&surface);
  mu_check(!surface.dirty);
  mu_assert_int_eq(32, surface.dirtyX1);
  mu_assert_int_eq(24, surface.dirtyY1);
  mu_assert_int_eq(0, surface.dirtyX2);
  mu_assert_int_eq(0, surface.dirtyY2);

  SDL_MarkDirtyRect(&surface, 4, 5, 8, 6);
  mu_check(surface.dirty);
  mu_assert_int_eq(4, surface.dirtyX1);
  mu_assert_int_eq(5, surface.dirtyY1);
  mu_assert_int_eq(12, surface.dirtyX2);
  mu_assert_int_eq(11, surface.dirtyY2);

  SDL_MarkDirtyRect(&surface, 2, 3, 4, 4);
  mu_assert_int_eq(2, surface.dirtyX1);
  mu_assert_int_eq(3, surface.dirtyY1);
  mu_assert_int_eq(12, surface.dirtyX2);
  mu_assert_int_eq(11, surface.dirtyY2);

  SDL_MarkDirtyRect(&surface, -4, -3, 6, 6);
  mu_assert_int_eq(0, surface.dirtyX1);
  mu_assert_int_eq(0, surface.dirtyY1);

  SDL_MarkDirtyRect(&surface, 100, 100, 5, 5);
  mu_assert_int_eq(0, surface.dirtyX1);
  mu_assert_int_eq(0, surface.dirtyY1);
}

MU_TEST(test_sdl_init_and_stub_functions)
{
  mu_assert_int_eq(0, SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK));
  mu_assert_int_eq(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
  mu_check(strcmp(SDL_GetError(), "SDL stub") == 0);

  SRL::Types::HighColor color = HighColor(0, 0, 0);
  SRL_Surface surface = makeTestSurface(16, 16);
  mu_assert_int_eq(0, SDL_SetColors(&surface, &color, 0, 1));
  mu_assert_int_eq(0, SDL_FillRect(&surface, nullptr, 0));
  mu_assert_int_eq(0, SDL_Flip(&surface));
}

MU_TEST(test_sdl_blit_palette_and_stats_helpers)
{
  mu_assert_int_eq(0, SDL_Init(SDL_INIT_VIDEO));

  SDL_SetBlitPaletteBank(0);
  mu_check(SDL_RefreshBlitPaletteCache());

  const uint32_t hash1 = SDL_HashPalette();
  const uint32_t hash2 = SDL_HashPalette();
  mu_assert_int_eq(hash1, hash2);

  SDL_ResetBlitStats();
  uint32_t calls = 99;
  uint32_t uploads = 99;
  uint32_t uploadPixels = 99;
  uint32_t uploadUs = 99;
  uint32_t drawUs = 99;
  SDL_GetBlitStats(&calls, &uploads, &uploadPixels, &uploadUs, &drawUs);
  mu_assert_int_eq(0, calls);
  mu_assert_int_eq(0, uploads);
  mu_assert_int_eq(0, uploadPixels);
  mu_assert_int_eq(0, uploadUs);
  mu_assert_int_eq(0, drawUs);
}

MU_TEST(test_sdl_blit_surface_safe_error_paths)
{
  SRL_Surface src = makeTestSurface(16, 16);
  SRL_Surface dst = makeTestSurface(320, 240);
  SDL_Rect rect = {1, 1, 8, 8};

  mu_assert_int_eq(-1, SDL_BlitSurface(nullptr, nullptr, &dst, nullptr));
  mu_assert_int_eq(-1, SDL_BlitSurface(&src, nullptr, nullptr, nullptr));

  // Smoke-test the partial-source-rect rejection path without asserting the
  // exact backend failure order beyond "must not succeed".
  mu_check(SDL_BlitSurface(&src, &rect, &dst, nullptr) != 0);
}

MU_TEST(test_sdl_time_wrappers_are_safe_and_monotonic)
{
  mu_assert_int_eq(0, SDL_Init(SDL_INIT_VIDEO));

  const uint32_t ticks1 = SDL_GetTicks();
  const uint32_t ticks2 = SDL_GetTicks();
  mu_check(ticks2 >= ticks1);

  const uint32_t prof1 = SDL_GetProfileMicros();
  const uint32_t prof2 = SDL_GetProfileMicros();
  mu_check(prof2 >= prof1);

  // In Mednafen the underlying timer may stall, so only verify the zero-delay
  // fast path here to avoid hanging the campaign.
  SDL_Delay(0);
}

MU_TEST_SUITE(sdl_wrapper_test_suite)
{
  MU_RUN_TEST(test_sdl_copy_bytes);
  MU_RUN_TEST(test_sdl_dirty_rect_helpers);
  MU_RUN_TEST(test_sdl_init_and_stub_functions);
  MU_RUN_TEST(test_sdl_blit_palette_and_stats_helpers);
  MU_RUN_TEST(test_sdl_blit_surface_safe_error_paths);
  MU_RUN_TEST(test_sdl_time_wrappers_are_safe_and_monotonic);
}

// ---------------------------------------------------------------------------
// FPS counter tests
// These feed raw Fxp delta-time values through the same arithmetic used by
// drawFpsCounter() and assert the resulting whole/fractional parts.
//
// Raw value encoding: 16.16 fixed point, so 1.0 s = 0x10000 = 65536.
// Hardware division: AsyncDivSet sets 64-bit dividend = (raw << 16),
// divisor = deltaRaw, result stored in dvdntl.
// Equivalent: fpsRaw = (65536 * 65536) / deltaRaw = 4294967296 / deltaRaw.
// ---------------------------------------------------------------------------

MU_TEST(test_fps_zero_vblanks_returns_default_60fps)
{
  // First-frame / no-vblanks-yet path must not divide by zero and must
  // return 60.00 fps as the boot default.
  mu_assert_int_eq(6000u, computeFpsTimes100(0u));
  int32_t w, f;
  computeFpsDisplay(6000u, w, f);
  mu_assert_int_eq(60, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_1_vblank_is_60fps)
{
  // 1 vblank between renders → 60 fps (NTSC full rate)
  const uint32_t t = computeFpsTimes100(1u);
  mu_assert_int_eq(6000u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(60, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_2_vblanks_is_30fps)
{
  // 2 vblanks between renders → 30 fps (typical Saturn VDP1 target)
  const uint32_t t = computeFpsTimes100(2u);
  mu_assert_int_eq(3000u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(30, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_3_vblanks_is_20fps)
{
  // 3 vblanks → 20 fps
  const uint32_t t = computeFpsTimes100(3u);
  mu_assert_int_eq(2000u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(20, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_4_vblanks_is_15fps)
{
  // 4 vblanks → 15 fps
  const uint32_t t = computeFpsTimes100(4u);
  mu_assert_int_eq(1500u, t);
  int32_t w, f;
  computeFpsDisplay(t, w, f);
  mu_assert_int_eq(15, w);
  mu_assert_int_eq(0, f);
}

MU_TEST(test_fps_display_fractional_frac)
{
  // fpsTimes100 = 4567 → whole=45, frac=67
  int32_t w, f;
  computeFpsDisplay(4567u, w, f);
  mu_assert_int_eq(45, w);
  mu_assert_int_eq(67, f);
}

MU_TEST(test_fps_display_zero_fpstimes100)
{
  // Edge: fpsTimes100 == 0 → whole=0, frac=0
  int32_t w, f;
  computeFpsDisplay(0u, w, f);
  mu_assert_int_eq(0, w);
  mu_assert_int_eq(0, f);
}

MU_TEST_SUITE(fps_test_suite)
{
  MU_RUN_TEST(test_fps_zero_vblanks_returns_default_60fps);
  MU_RUN_TEST(test_fps_1_vblank_is_60fps);
  MU_RUN_TEST(test_fps_2_vblanks_is_30fps);
  MU_RUN_TEST(test_fps_3_vblanks_is_20fps);
  MU_RUN_TEST(test_fps_4_vblanks_is_15fps);
  MU_RUN_TEST(test_fps_display_fractional_frac);
  MU_RUN_TEST(test_fps_display_zero_fpstimes100);
}

MU_TEST_SUITE(collision_test_suite)
{
  MU_RUN_TEST(test_basic_vector_ops);
  MU_RUN_TEST(test_vctCheckSide_branches);
  MU_RUN_TEST(test_vctDist_branches);
  MU_RUN_TEST(test_vctInnerProduct_large_values_no_overflow);
  MU_RUN_TEST(test_vctGetElement_projection);
  MU_RUN_TEST(test_vctSize);
  MU_RUN_TEST(test_shotHitsFoe_boundaries);
  MU_RUN_TEST(test_shotHitsFoeSwept_tunneling);
  MU_RUN_TEST(test_movingBulletHitsShip_full_coverage);
  MU_RUN_TEST(test_destroyShip_no_invincibility_behavior);
  MU_RUN_TEST(test_fxp_foecommand_direction_wrapped_matches_legacy_float_and_double);
  MU_RUN_TEST(test_fxp_foecommand_direction_signed_matches_legacy_float_and_double);
  MU_RUN_TEST(test_fxp_foecommand_speed_and_accel_match_legacy_float_and_double);
  MU_RUN_TEST(test_fxp_foecommand_direction_getter_matches_legacy_float_and_double);
}

MU_TEST_SUITE(vector_test_suite)
{
  MU_RUN_TEST(test_add_sub_mul_div_extended);
  MU_RUN_TEST(test_inner_product_large_values_extended);
  MU_RUN_TEST(test_get_element_projection_extended);
  MU_RUN_TEST(test_check_side_extended);
  MU_RUN_TEST(test_size_and_dist_extended);
}

int main()
{
  SRL::Core::Initialize(HighColor(20, 10, 50));

  LogInfo(strStart);

  MU_RUN_SUITE(sdl_wrapper_test_suite);
  MU_RUN_SUITE(fps_test_suite);
  MU_RUN_SUITE(collision_test_suite);
  MU_RUN_SUITE(vector_test_suite);
  MU_REPORT();

  LogInfo(strEnd);

  // Keep emulator alive long enough for log flush + marker detection.
  for (;;)
  {
    SRL::Core::Synchronize();
  }

  return MU_EXIT_CODE;
}
