#pragma once

#include <srl.hpp>

using namespace SRL::Types;

class Palette : public SRL::Bitmap::Palette
{
public:
    /** @brief Creates a palette with the requested number of entries. */
    explicit Palette(size_t count);

    /** @brief Stores a colour at the specified index. */
    void SetColor(uint16_t index, HighColor&& color);
    /** @brief Returns the colour at the specified index. */
    HighColor GetColor(uint16_t index) const;
    /** @brief Initialises the palette contents. */
    void Init();
    /** @brief Applies a brightness adjustment to every colour. */
    void ApplyBrightness(uint8_t brightness);

    /** @brief Loads a palette from a bitmap into the Saturn palette format. */
    static int16_t LoadPalette(SRL::Bitmap::BitmapInfo* bitmap);
    /** @brief Initialises the global palette system. */
    static int32_t initPalette();

    static SRL::CRAM::Palette* palette;
};
