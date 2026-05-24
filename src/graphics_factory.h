#pragma once

#include <srl_bitmap.hpp>
#include <srl_cram.hpp>

#include "memory_factory.h"

inline SRL::Bitmap::TGA* createBitmapTga(const char* name)
{
  return createPooledObject<SRL::Bitmap::TGA>(name);
}

inline SRL::Bitmap::BitmapInfo* createBitmapInfo(
    uint16_t width,
    uint16_t height,
    SRL::Bitmap::Palette* palette)
{
  return createPooledObject<SRL::Bitmap::BitmapInfo>(width, height, palette);
}

inline SRL::CRAM::Palette* createCramPalette(
    SRL::CRAM::TextureColorMode colorMode,
    int32_t id)
{
  return createPooledObject<SRL::CRAM::Palette>(colorMode, id);
}

inline void destroyBitmapTga(SRL::Bitmap::TGA*& tga)
{
  destroyPooledObject(tga);
}

inline void destroyBitmapInfo(SRL::Bitmap::BitmapInfo*& bitmap)
{
  destroyPooledObject(bitmap);
}

inline void destroyCramPalette(SRL::CRAM::Palette*& palette)
{
  destroyPooledObject(palette);
}

inline void releaseGraphicsFactoryPools()
{
  releasePooledObjectPool<SRL::Bitmap::TGA>();
  releasePooledObjectPool<SRL::Bitmap::BitmapInfo>();
  releasePooledObjectPool<SRL::CRAM::Palette>();
}
