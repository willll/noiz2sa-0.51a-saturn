#pragma once

#include <cstdint>

#include <srl.hpp>

#include "canvas_palette.h"

class Canvas : public SRL::Bitmap::IBitmap
{
public:
    using Pixel = uint8_t;

    explicit Canvas(uint16_t width, uint16_t height, Palette& palette);
    ~Canvas() override;

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    uint8_t* GetData() override;
    SRL::Bitmap::BitmapInfo GetInfo() const override;

    void SetPixel(uint16_t x, uint16_t y, Pixel color);
    void RotatePalette(int32_t canvasTextureId);

    static int16_t LoadPalette(SRL::Bitmap::BitmapInfo* bitmap);

private:
    uint16_t width_;
    uint16_t height_;
    Pixel* imageData_;
    SRL::Bitmap::BitmapInfo* bitmap_;
};
