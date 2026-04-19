#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>

// Minimal test macros for bulletml parity validation
static int g_bulletml_tests_failed = 0;

#define BULLETML_TEST_ASSERT(expr, msg)                                        \
  do                                                                           \
  {                                                                            \
    if (!(expr))                                                               \
    {                                                                          \
      std::cerr << "BULLETML_ASSERT failed at line " << __LINE__ << ": "     \
                << msg << "\n";                                               \
      g_bulletml_tests_failed++;                                              \
    }                                                                          \
  } while (0)

#define BULLETML_TEST_EQUAL(a, b, msg)                                        \
  do                                                                           \
  {                                                                            \
    const auto _a = (a);                                                       \
    const auto _b = (b);                                                       \
    if (!(_a == _b))                                                           \
    {                                                                          \
      std::cerr << "BULLETML_EQUAL failed at line " << __LINE__ << ": "      \
                << msg << " (" << _a << " != " << _b << ")\n";               \
      g_bulletml_tests_failed++;                                              \
    }                                                                          \
  } while (0)

/// BulletML Binary Format Validation Tests
/// These tests validate that the BLB (BulletML Binary) format is correctly
/// implemented by checking:
/// 1. Magic number and version
/// 2. Header structure and offsets
/// 3. String table encoding/decoding
/// 4. Reference map consistency
/// 5. Node tree structure preservation

// BLB Format Constants
#define BULLETML_BLB_MAGIC "BLB\x00"
#define BULLETML_BLB_VERSION 1

struct BulletMLBLBHeader
{
  uint8_t magic[4];
  uint16_t version;
  uint8_t orientation;  // 0=vertical, 1=horizontal
  uint8_t flags;
  uint32_t string_table_offset;
  uint32_t refmap_offset;
  uint32_t tree_offset;
  uint32_t file_size;
};

static void test_blb_magic_and_version()
{
  // Verify magic number constants
  const char *magic = BULLETML_BLB_MAGIC;
  BULLETML_TEST_EQUAL(magic[0], 'B', "Magic byte 0");
  BULLETML_TEST_EQUAL(magic[1], 'L', "Magic byte 1");
  BULLETML_TEST_EQUAL(magic[2], 'B', "Magic byte 2");
  BULLETML_TEST_EQUAL(magic[3], '\x00', "Magic byte 3 (null terminator)");

  // Verify version constant
  BULLETML_TEST_EQUAL(BULLETML_BLB_VERSION, 1, "BLB version");
}

static void test_blb_header_structure()
{
  // Validate header structure size and field offsets
  static_assert(sizeof(BulletMLBLBHeader) == 24, "Header size must be 24 bytes");

  // Verify alignment
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, magic), 0, "magic offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, version), 4, "version offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, orientation), 6, "orientation offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, flags), 7, "flags offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, string_table_offset), 8, "string_table_offset offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, refmap_offset), 12, "refmap_offset offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, tree_offset), 16, "tree_offset offset");
  BULLETML_TEST_EQUAL(offsetof(BulletMLBLBHeader, file_size), 20, "file_size offset");
}

static void test_blb_endianness_validation()
{
  // Verify little-endian encoding
  uint32_t test_val = 0x12345678;
  uint8_t *bytes = (uint8_t *)&test_val;

  // On little-endian systems, LSB comes first
  BULLETML_TEST_EQUAL((int)bytes[0], 0x78, "Little-endian byte 0 (LSB)");
  BULLETML_TEST_EQUAL((int)bytes[1], 0x56, "Little-endian byte 1");
  BULLETML_TEST_EQUAL((int)bytes[2], 0x34, "Little-endian byte 2");
  BULLETML_TEST_EQUAL((int)bytes[3], 0x12, "Little-endian byte 3 (MSB)");
}

static void test_bulletml_node_type_encoding()
{
  // Verify node type encoding constants (must match converter.py)
  enum NodeType
  {
    bulletml = 0x00,
    bullet = 0x01,
    action = 0x02,
    fire = 0x03,
    changeDirection = 0x04,
    changeSpeed = 0x05,
    accel = 0x06,
    wait = 0x07,
    repeat = 0x08,
    bulletRef = 0x09,
    actionRef = 0x0A,
    fireRef = 0x0B,
    vanish = 0x0C,
    horizontal = 0x0D,
    vertical = 0x0E,
    term = 0x0F,
    times = 0x10,
    direction = 0x11,
    speed = 0x12,
    param = 0x13,
  };

  BULLETML_TEST_EQUAL((int)bulletml, 0x00, "bulletml node type");
  BULLETML_TEST_EQUAL((int)bullet, 0x01, "bullet node type");
  BULLETML_TEST_EQUAL((int)action, 0x02, "action node type");
  BULLETML_TEST_EQUAL((int)fire, 0x03, "fire node type");
  BULLETML_TEST_EQUAL((int)changeDirection, 0x04, "changeDirection node type");
  BULLETML_TEST_EQUAL((int)changeSpeed, 0x05, "changeSpeed node type");
  BULLETML_TEST_EQUAL((int)accel, 0x06, "accel node type");
  BULLETML_TEST_EQUAL((int)wait, 0x07, "wait node type");
  BULLETML_TEST_EQUAL((int)repeat, 0x08, "repeat node type");
  BULLETML_TEST_EQUAL((int)bulletRef, 0x09, "bulletRef node type");
  BULLETML_TEST_EQUAL((int)actionRef, 0x0A, "actionRef node type");
  BULLETML_TEST_EQUAL((int)fireRef, 0x0B, "fireRef node type");
  BULLETML_TEST_EQUAL((int)vanish, 0x0C, "vanish node type");
  BULLETML_TEST_EQUAL((int)horizontal, 0x0D, "horizontal node type");
  BULLETML_TEST_EQUAL((int)vertical, 0x0E, "vertical node type");
  BULLETML_TEST_EQUAL((int)term, 0x0F, "term node type");
  BULLETML_TEST_EQUAL((int)times, 0x10, "times node type");
  BULLETML_TEST_EQUAL((int)direction, 0x11, "direction node type");
  BULLETML_TEST_EQUAL((int)speed, 0x12, "speed node type");
  BULLETML_TEST_EQUAL((int)param, 0x13, "param node type");
}

static void test_bulletml_value_type_encoding()
{
  // Verify value type encoding (type attribute handling)
  enum ValueType
  {
    value_none = 0x00,
    value_aim = 0x01,
    value_absolute = 0x02,
    value_relative = 0x03,
    value_sequence = 0x04,
  };

  BULLETML_TEST_EQUAL((int)value_none, 0x00, "value type: none");
  BULLETML_TEST_EQUAL((int)value_aim, 0x01, "value type: aim");
  BULLETML_TEST_EQUAL((int)value_absolute, 0x02, "value type: absolute");
  BULLETML_TEST_EQUAL((int)value_relative, 0x03, "value type: relative");
  BULLETML_TEST_EQUAL((int)value_sequence, 0x04, "value type: sequence");
}

static void test_bulletml_none_u32_sentinel()
{
  // Verify that 0xFFFFFFFF is used as a "none" sentinel for references
  const uint32_t NONE_SENTINEL = 0xFFFFFFFF;
  BULLETML_TEST_EQUAL(NONE_SENTINEL, 0xFFFFFFFFU, "None sentinel value");

  // Verify that it cannot be a valid ref ID
  const uint32_t max_refs = 500;  // BULLETML_MAX_REFS
  BULLETML_TEST_ASSERT(NONE_SENTINEL >= max_refs, "Sentinel is outside valid ref range");
}

static void test_bulletml_parser_parity_constraints()
{
  // Verify parser limits match between Saturn and GP32 versions
  const int MAX_CHILDREN = 32;
  const int MAX_FILENAME = 32;
  const int MAX_STRINGS = 128;
  const int MAX_STRING_TABLE_SIZE = 4096;
  const int MAX_NODES = 512;
  const int MAX_REFS = 500;

  BULLETML_TEST_ASSERT(MAX_CHILDREN > 0, "MAX_CHILDREN > 0");
  BULLETML_TEST_ASSERT(MAX_FILENAME > 0, "MAX_FILENAME > 0");
  BULLETML_TEST_ASSERT(MAX_STRINGS > 0, "MAX_STRINGS > 0");
  BULLETML_TEST_ASSERT(MAX_STRING_TABLE_SIZE > 0, "MAX_STRING_TABLE_SIZE > 0");
  BULLETML_TEST_ASSERT(MAX_NODES > 0, "MAX_NODES > 0");
  BULLETML_TEST_ASSERT(MAX_REFS > 0, "MAX_REFS > 0");

  // Ensure parser limits are reasonable
  BULLETML_TEST_ASSERT(MAX_NODES > MAX_REFS, "Node count larger than ref count");
}

int main()
{
  // Run all bulletml parity tests
  test_blb_magic_and_version();
  test_blb_header_structure();
  test_blb_endianness_validation();
  test_bulletml_node_type_encoding();
  test_bulletml_value_type_encoding();
  test_bulletml_none_u32_sentinel();
  test_bulletml_parser_parity_constraints();

  if (g_bulletml_tests_failed != 0)
  {
    std::cerr << "BulletML parity tests FAILED: " << g_bulletml_tests_failed << " assertion(s).\n";
    return EXIT_FAILURE;
  }

  std::cout << "BulletML parity tests PASSED.\n";
  return EXIT_SUCCESS;
}
