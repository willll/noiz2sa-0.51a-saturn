#include "canvas_palette.h"

#include "clrtbl.h"

#include <srl_cram.hpp>
#include <srl_log.hpp>

SRL::CRAM::Palette* Palette::palette = nullptr;

Palette::Palette(size_t count)
    : SRL::Bitmap::Palette(count)
{
}

void Palette::SetColor(uint16_t index, HighColor&& color)
{
    if (index < Count)
    {
        Colors[index] = std::move(color);
    }
    else
    {
        SRL::Logger::LogFatal("[CanvasPalette] index(%d) out of bound", index);
    }
}

HighColor Palette::GetColor(uint16_t index) const
{
    if (index < Count)
    {
        return Colors[index];
    }

    SRL::Logger::LogFatal("[CanvasPalette] index(%d) out of bound", index);
    return HighColor(0, 0, 0);
}

void Palette::Init()
{
    if (Count == 0)
    {
        return;
    }

    for (uint16_t i = 0; i < Count - 1; ++i)
    {
        SetColor(i, HighColor::FromRGB555(i, (i * 2) % 256, (i * 4) % 256));
    }

    SetColor(Count - 1, HighColor::FromRGB555(255, 255, 255));
}

void Palette::ApplyBrightness(uint8_t brightness)
{
    if (Count == 0)
    {
        return;
    }

    for (uint16_t i = 0; i < Count; ++i)
    {
        HighColor colorValue = GetColor(i);

        uint8_t red = (colorValue >> 10) & 0x1F;
        uint8_t green = (colorValue >> 5) & 0x1F;
        uint8_t blue = colorValue & 0x1F;

        red = (red * brightness) / 256;
        green = (green * brightness) / 256;
        blue = (blue * brightness) / 256;

        if (red > 31) red = 31;
        if (green > 31) green = 31;
        if (blue > 31) blue = 31;

        SetColor(i, HighColor::FromRGB555(red, green, blue));
    }
}

int16_t Palette::LoadPalette(SRL::Bitmap::BitmapInfo* bitmap)
{
    if (bitmap == nullptr || bitmap->Palette == nullptr)
    {
        SRL::Logger::LogFatal("[Palette::LoadPalette] Null bitmap or palette pointer");
        return -1;
    }

    return initPalette();
}

int32_t Palette::initPalette()
{
    if (palette)
    {
        return palette->GetId();
    }

    int32_t id = SRL::CRAM::GetFreeBank(SRL::CRAM::TextureColorMode::Paletted256);
    if (id < 0)
    {
        return id;
    }

    palette = new SRL::CRAM::Palette(SRL::CRAM::TextureColorMode::Paletted256, id);
    if (palette == nullptr)
    {
        return -1;
    }

    SRL::Types::HighColor* data = palette->GetData();
    if (data == nullptr)
    {
        return -1;
    }

    for (int i = 0; i < 256; ++i)
    {
        data[i] = color[i];
    }

    SRL::CRAM::SetBankUsedState(id, SRL::CRAM::TextureColorMode::Paletted256, true);
    return id;
}
