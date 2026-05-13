#pragma once

#include <cstddef>

#include "bulletml_binary/bulletmlparser_blb.hpp"

/** @brief Creates a BulletML parser from embedded data. */
inline BulletMLParserBLB* createEmbeddedBulletMlParser(
    const char* name,
    const uint8_t* data,
    std::size_t size)
{
  return new BulletMLParserBLB(name, data, size);
}

/** @brief Creates a BulletML parser from a file path. */
inline BulletMLParserBLB* createFileBulletMlParser(const char* name)
{
  return new BulletMLParserBLB(name);
}
