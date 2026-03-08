#pragma once

#include <srl.hpp>

using namespace SRL::Types;

/**
 * @brief Color palette management for Noiz2sa
 *
 * Manages a color palette for sprite and background rendering.
 * Provides methods for setting and retrieving colors, with bounds checking
 * and logging. The palette is initialized with a gradient of colors that
 * can be further customized.
 */
class MyPalette : public SRL::CRAM::Palette
{
public:

    /** @brief Construct a new Palette
     *
     * Allocates a new color palette with the specified number of entries.
     * @param count Number of colors in the palette
     */
    explicit MyPalette(const uint16_t id);

    /** @brief Set a palette entry
     *
     * Stores a color at the given palette index. Logs a fatal error if the
     * index is out of range.
     * @param index Palette index to set (0 to Count-1)
     * @param color Color value to move into the palette
     */
    void SetColor(uint16_t index, HighColor&& color);

    /** @brief Retrieve a color from the palette
     *
     * Returns the HighColor stored at the given index. If the index is out of
     * bounds a fatal log is emitted and black (0,0,0) is returned.
     * @param index Palette index to retrieve (0 to Count-1)
     * @return HighColor value at the index (or black on error)
     */
    HighColor GetColor(uint16_t index) const;

    /** @brief Initialize palette with a default gradient
     *
     * Fills the palette with a color gradient where each index produces
     * a unique color. The final entry is set to white. This provides a
     * basic starting palette for visualization purposes.
     */
    void Init();

    /** @brief Apply brightness adjustment to all colors
     *
     * Scales all colors in the palette by a brightness factor.
     * @param brightness Brightness multiplier (0-255, where 128 is normal)
     */
    void ApplyBrightness(uint8_t brightness);

    /** @brief Load palette into CRAM
     *
     * Attempts to find a free CRAM bank, upload the palette, and mark the
     * bank as used. This palette handler is used with VDP1::TryLoadTexture()
     * to automatically load palettes during texture initialization.
     * @param bitmap Bitmap info containing the palette to load
     * @return CRAM bank id on success, or -1 on failure
     */
    static int16_t LoadPalette(SRL::Bitmap::BitmapInfo* bitmap);

    static int32_t initPalette();

    static MyPalette * palette = nullptr;
};
