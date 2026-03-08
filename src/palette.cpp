#include "palette.h"
#include "clrtbl.h"

#include <srl_log.hpp>
#include <srl_cram.hpp>
#include <srl_memory.hpp>

MyPalette* MyPalette::palette = nullptr;

MyPalette::MyPalette(const uint16_t id)
    : SRL::CRAM::Palette(SRL::CRAM::TextureColorMode::Paletted256, id)
{
    SRL::Logger::LogDebug("[Palette] Palette created with ID %d", id);
}

void MyPalette::SetColor(uint16_t index, HighColor&& color)
{
    if (index < 256)
    {
        this->GetData()[index] = std::move(color);
        SRL::Logger::LogTrace("[Palette] SetColor[%d] = 0x%04X", index, color);
    }
    else
    {
        SRL::Logger::LogFatal("[Palette] SetColor: index(%d) out of bounds", index);
    }
}

HighColor MyPalette::GetColor(uint16_t index) const
{
    if (index < 256)
    {
        // SRL::CRAM::Palette exposes only a non-const GetData(); read access is safe here.
        return const_cast<MyPalette*>(this)->GetData()[index];
    }
    else
    {
        SRL::Logger::LogFatal("[Palette] GetColor: index(%d) out of bounds", index);
        return HighColor(0, 0, 0);
    }
}

void MyPalette::Init()
{
    // Fill with gradient: each component cycles at different rates for interesting colors
    for (uint16_t i = 0; i < 256 - 1; ++i)
    {
        SetColor(i, HighColor::FromRGB555(i, (i * 2) % 256, (i * 4) % 256));
    }

    // Final entry is white for highlighting
    SetColor(256 - 1, HighColor::FromRGB555(255, 255, 255));

    SRL::Logger::LogDebug("[Palette] Palette initialization complete");
}

void MyPalette::ApplyBrightness(uint8_t brightness)
{
    SRL::Logger::LogDebug("[Palette] Applying brightness %d", brightness);

    for (uint16_t i = 0; i < 256; ++i)
    {
        HighColor color = this->GetColor(i);

        // Extract RGB components (5-bit each for RGB555)
        uint8_t red = (color >> 10) & 0x1F;
        uint8_t green = (color >> 5) & 0x1F;
        uint8_t blue = color & 0x1F;

        // Apply brightness scaling
        red = (red * brightness) / 256;
        green = (green * brightness) / 256;
        blue = (blue * brightness) / 256;

        // Clamp to 5-bit range
        if (red > 31) red = 31;
        if (green > 31) green = 31;
        if (blue > 31) blue = 31;

        // Reconstruct color with new brightness
        this->SetColor(i, HighColor::FromRGB555(red, green, blue));
    }

    SRL::Logger::LogTrace("[Palette] Brightness adjustment complete");
}

int16_t MyPalette::LoadPalette(SRL::Bitmap::BitmapInfo* bitmap)
{
    if (bitmap == nullptr || bitmap->Palette == nullptr)
    {
        SRL::Logger::LogFatal("[Palette::LoadPalette] Null bitmap or palette pointer");
        return -1;
    }

    return initPalette();
}

int32_t MyPalette::initPalette()
{
    if (palette)
        return palette->GetId(); // Already initialized

     // Get free CRAM bank
    int32_t id = SRL::CRAM::GetFreeBank(SRL::CRAM::TextureColorMode::Paletted256);

    if (id >= 0)
    {
        palette = new MyPalette(id);

        for (int i = 0; i < 256; i++)
        {
            palette->SetColor(i, std::move(color[i]));
        }

        // Mark bank as in use
        SRL::CRAM::SetBankUsedState(id, SRL::CRAM::TextureColorMode::Paletted256, true);
    }

    return id;
}