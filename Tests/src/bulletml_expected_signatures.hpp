#ifndef BULLETML_EXPECTED_SIGNATURES_HPP_
#define BULLETML_EXPECTED_SIGNATURES_HPP_

#include <cstdint>

struct BulletMLExpectedSignature {
  const char* path;
  uint32_t node_count;
  uint32_t top_actions;
  uint32_t signature;
  uint8_t is_horizontal;
};

static const BulletMLExpectedSignature kBulletMLExpectedSignatures[] = {
  {"BOSS/B000.BLB", 36u, 2u, 0xA17DE288u, 0u},
  {"BOSS/B001.BLB", 23u, 1u, 0x11717DEEu, 0u},
  {"BOSS/B002.BLB", 27u, 2u, 0x84C6E195u, 0u},
  {"BOSS/B003.BLB", 58u, 1u, 0x1B145573u, 0u},
  {"BOSS/B004.BLB", 39u, 1u, 0xF876BA65u, 0u},
  {"BOSS/B005.BLB", 14u, 1u, 0x8135479Eu, 0u},
  {"BOSS/B006.BLB", 58u, 1u, 0x9BAFBDB2u, 0u},
  {"BOSS/B007.BLB", 33u, 1u, 0x1E1EB07Fu, 0u},
  {"BOSS/B008.BLB", 27u, 1u, 0x8D26CE47u, 0u},
  {"BOSS/B009.BLB", 30u, 1u, 0xFDE5A9FAu, 0u},
  {"BOSS/B010.BLB", 32u, 1u, 0xC74480A3u, 0u},
  {"BOSS/B011.BLB", 22u, 1u, 0x756613B4u, 0u},
  {"BOSS/B012.BLB", 27u, 1u, 0xCDD7D4E5u, 0u},
  {"BOSS/B013.BLB", 11u, 1u, 0x8F62C6DFu, 0u},
  {"BOSS/B014.BLB", 54u, 2u, 0x62195DB3u, 0u},
  {"BOSS/B015.BLB", 88u, 2u, 0x026E539Cu, 0u},
  {"BOSS/B016.BLB", 33u, 3u, 0xE3D17DBBu, 0u},
  {"BOSS/B017.BLB", 55u, 1u, 0xC129F93Au, 0u},
  {"BOSS/B018.BLB", 32u, 2u, 0x7C3FBE14u, 0u},
  {"BOSS/B019.BLB", 62u, 1u, 0x99EDB3E9u, 0u},
  {"BOSS/B020.BLB", 33u, 2u, 0x85E33EE8u, 0u},
  {"BOSS/B021.BLB", 25u, 2u, 0xACB20EC2u, 0u},
  {"BOSS/B022.BLB", 37u, 1u, 0x27BAC94Cu, 0u},
  {"BOSS/B023.BLB", 80u, 1u, 0x31C12CCAu, 0u},
  {"BOSS/B024.BLB", 22u, 1u, 0x64CA4ECAu, 0u},
  {"BOSS/B025.BLB", 29u, 3u, 0xE313992Cu, 0u},
  {"BOSS/B026.BLB", 45u, 1u, 0x4BFAC7ABu, 0u},
  {"BOSS/B027.BLB", 50u, 1u, 0x089C5102u, 0u},
  {"BOSS/B028.BLB", 26u, 1u, 0x6664F1BAu, 0u},
  {"BOSS/B029.BLB", 43u, 2u, 0xC0BC5FB3u, 0u},
  {"BOSS/B030.BLB", 27u, 1u, 0xE40B3884u, 0u},
  {"BOSS/B031.BLB", 34u, 1u, 0x16D81BE3u, 0u},
  {"MIDDLE/M000.BLB", 40u, 4u, 0x5E28B35Eu, 0u},
  {"MIDDLE/M001.BLB", 43u, 2u, 0xFE2824A6u, 0u},
  {"MIDDLE/M002.BLB", 49u, 2u, 0xEA69D872u, 0u},
  {"MIDDLE/M003.BLB", 41u, 2u, 0x5C45F560u, 0u},
  {"MIDDLE/M004.BLB", 61u, 2u, 0x7AF257D8u, 0u},
  {"MIDDLE/M005.BLB", 24u, 2u, 0xCFF4D214u, 0u},
  {"MIDDLE/M006.BLB", 20u, 1u, 0x20D11B87u, 0u},
  {"MIDDLE/M007.BLB", 24u, 2u, 0x9E228916u, 0u},
  {"MIDDLE/M008.BLB", 33u, 2u, 0x71FF510Fu, 0u},
  {"MIDDLE/M009.BLB", 41u, 2u, 0x2E2057D4u, 0u},
  {"MIDDLE/M010.BLB", 67u, 2u, 0xC8E137BFu, 0u},
  {"MIDDLE/M011.BLB", 36u, 2u, 0xEAC24060u, 0u},
  {"MIDDLE/M012.BLB", 29u, 2u, 0xFFB786D3u, 0u},
  {"MIDDLE/M013.BLB", 19u, 2u, 0x32BA51D4u, 0u},
  {"MIDDLE/M014.BLB", 42u, 3u, 0x3788747Au, 0u},
  {"MIDDLE/M015.BLB", 38u, 3u, 0x19538B86u, 0u},
  {"MIDDLE/M016.BLB", 30u, 2u, 0x3DA1C2A3u, 0u},
  {"MIDDLE/M017.BLB", 24u, 2u, 0x6EBBEAADu, 0u},
  {"MIDDLE/M018.BLB", 31u, 2u, 0xEB9121A3u, 0u},
  {"MIDDLE/M019.BLB", 33u, 3u, 0x158C4028u, 0u},
  {"MIDDLE/M020.BLB", 38u, 2u, 0x1DDB1E18u, 0u},
  {"MIDDLE/M021.BLB", 32u, 2u, 0x3E5EF21Cu, 0u},
  {"MIDDLE/M022.BLB", 33u, 2u, 0x608C194Du, 0u},
  {"ZAKO/Z000.BLB", 42u, 3u, 0x667C09F0u, 0u},
  {"ZAKO/Z001.BLB", 25u, 2u, 0xCB21E0D4u, 0u},
  {"ZAKO/Z002.BLB", 61u, 3u, 0x4BACFEF7u, 0u},
  {"ZAKO/Z003.BLB", 18u, 1u, 0x82C5BA42u, 0u},
  {"ZAKO/Z004.BLB", 23u, 2u, 0xF362A1A7u, 0u},
  {"ZAKO/Z005.BLB", 33u, 2u, 0x9930D5B6u, 0u},
  {"ZAKO/Z006.BLB", 25u, 2u, 0x567D9F1Fu, 0u},
  {"ZAKO/Z007.BLB", 30u, 2u, 0xA7DC7225u, 0u},
  {"ZAKO/Z008.BLB", 22u, 2u, 0x60BC8918u, 0u},
  {"ZAKO/Z009.BLB", 25u, 2u, 0x3CE8105Cu, 0u},
  {"ZAKO/Z010.BLB", 38u, 2u, 0xED30A316u, 0u},
  {"ZAKO/Z011.BLB", 28u, 3u, 0x0F4E9C43u, 0u},
  {"ZAKO/Z012.BLB", 50u, 2u, 0xAFA3F679u, 0u},
  {"ZAKO/Z013.BLB", 30u, 2u, 0xED164B6Au, 0u},
  {"ZAKO/Z014.BLB", 20u, 2u, 0x7163A0B6u, 0u},
  {"ZAKO/Z015.BLB", 14u, 2u, 0x7F24E763u, 0u},
  {"ZAKO/Z016.BLB", 39u, 2u, 0x0F3B5695u, 0u},
  {"ZAKO/Z017.BLB", 38u, 2u, 0x2B9B8F5Eu, 0u},
};

static const uint32_t kBulletMLExpectedSignatureCount =
    sizeof(kBulletMLExpectedSignatures) / sizeof(kBulletMLExpectedSignatures[0]);

#endif
