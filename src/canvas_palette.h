#pragma once

#include <srl.hpp>

using namespace SRL::Types;

class Palette : public SRL::Bitmap::Palette
{
public:
    explicit Palette(size_t count);

    void SetColor(uint16_t index, HighColor&& color);
    HighColor GetColor(uint16_t index) const;
    void Init();
    void ApplyBrightness(uint8_t brightness);

    static int16_t LoadPalette(SRL::Bitmap::BitmapInfo* bitmap);
    static int32_t initPalette();

    static SRL::CRAM::Palette* palette;
};
