#pragma once

#include <cstdint>

#include <srl.hpp>

#include "canvas_palette.h"

class Canvas : public SRL::Bitmap::IBitmap
{
public:
    using Pixel = uint8_t;

    /**
     * @brief Creates a canvas backed by a mutable pixel buffer.
     * @param width Canvas width in pixels.
     * @param height Canvas height in pixels.
     * @param palette Palette used when rotating or loading bitmap data.
     */
    explicit Canvas(uint16_t width, uint16_t height, Palette& palette);
    /** @brief Releases the canvas backing storage. */
    ~Canvas() override;

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    /** @brief Returns the raw pixel buffer. */
    uint8_t* GetData() override;
    /** @brief Returns bitmap metadata for the canvas. */
    SRL::Bitmap::BitmapInfo GetInfo() const override;

    /** @brief Writes a pixel into the backing buffer. */
    void SetPixel(uint16_t x, uint16_t y, Pixel color);
    /** @brief Applies the current palette to the specified texture. */
    void RotatePalette(int32_t canvasTextureId);

    /** @brief Loads the palette from a bitmap into the Saturn CRAM. */
    static int16_t LoadPalette(SRL::Bitmap::BitmapInfo* bitmap);

private:
    uint16_t width_;
    uint16_t height_;
    Pixel* imageData_;
    SRL::Bitmap::BitmapInfo* bitmap_;
};
