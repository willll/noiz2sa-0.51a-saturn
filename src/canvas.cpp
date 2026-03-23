#include "canvas.h"

#include <algorithm>

#include <srl_log.hpp>

using namespace SRL::Logger;
using namespace SRL::Types;

Canvas::Canvas(uint16_t width, uint16_t height, Palette& palette)
    : width_(width),
      height_(height),
      imageData_(new Pixel[width * height]),
      bitmap_(new SRL::Bitmap::BitmapInfo(width, height, &palette))
{
}

Canvas::~Canvas()
{
    delete[] imageData_;
    delete bitmap_;
}

uint8_t* Canvas::GetData()
{
    return imageData_;
}

SRL::Bitmap::BitmapInfo Canvas::GetInfo() const
{
    return *bitmap_;
}

void Canvas::SetPixel(uint16_t x, uint16_t y, Pixel color)
{
    if (x < width_ && y < height_)
    {
        imageData_[y * width_ + x] = color;
    }
}

void Canvas::RotatePalette(int32_t canvasTextureId)
{
    SRL::CRAM::Palette palette(bitmap_->ColorMode, SRL::VDP1::Metadata[canvasTextureId].PaletteId);
    int16_t size = palette.GetSize();
    HighColor* data = palette.GetData();

    if (size <= 1 || data == nullptr)
    {
        return;
    }

    std::rotate(data, data + 1, data + size);
}

int16_t Canvas::LoadPalette(SRL::Bitmap::BitmapInfo* bitmap)
{
    int32_t id = SRL::CRAM::GetFreeBank(bitmap->ColorMode);

    Log::LogPrint<LogLevels::INFO>("palette (%d) ColorMode : %d", id, bitmap->ColorMode);

    if (id >= 0)
    {
        SRL::CRAM::Palette palette(bitmap->ColorMode, id);

        if (palette.Load((HighColor*)bitmap->Palette->Colors, bitmap->Palette->Count) >= 0)
        {
            SRL::CRAM::SetBankUsedState(id, bitmap->ColorMode, true);
            return id;
        }

        Log::LogPrint<LogLevels::FATAL>("palette load failure");
    }
    else
    {
        Log::LogPrint<LogLevels::FATAL>("palette GetFreeBank failure");
    }

    return -1;
}
