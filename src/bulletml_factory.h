#pragma once

#include <cstddef>

#include "memory_factory.h"
#include "bulletml_binary/bulletmlparser_blb.hpp"

/** @brief Creates a BulletML parser from embedded data. */
inline BulletMLParserBLB* createEmbeddedBulletMlParser(
    const char* name,
    const uint8_t* data,
    std::size_t size)
{
  return createPooledObject<BulletMLParserBLB>(name, data, size);
}

/** @brief Creates a BulletML parser from a file path. */
inline BulletMLParserBLB* createFileBulletMlParser(const char* name)
{
  return createPooledObject<BulletMLParserBLB>(name);
}

inline void destroyBulletMlParser(BulletMLParserBLB*& parser)
{
  destroyPooledObject(parser);
}

inline void releaseBulletMlParserPool()
{
  releasePooledObjectPool<BulletMLParserBLB>();
}
