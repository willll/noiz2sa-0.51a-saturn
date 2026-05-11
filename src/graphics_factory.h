#pragma once

#include <srl_bitmap.hpp>
#include <srl_cram.hpp>

#include "memory_factory.h"

inline SRL::Bitmap::TGA* createBitmapTga(const char* name)
{
  return createObject<SRL::Bitmap::TGA>(name);
}

inline SRL::Bitmap::BitmapInfo* createBitmapInfo(
    uint16_t width,
    uint16_t height,
    SRL::Bitmap::Palette* palette)
{
  return createObject<SRL::Bitmap::BitmapInfo>(width, height, palette);
}

inline SRL::CRAM::Palette* createCramPalette(
    SRL::CRAM::TextureColorMode colorMode,
    int32_t id)
{
  return createObject<SRL::CRAM::Palette>(colorMode, id);
}
