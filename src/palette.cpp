#include "palette.h"
#include <srl_log.hpp>
#include <srl_cram.hpp>

Palette::Palette(size_t count)
    : SRL::Bitmap::Palette(count)
{
    SRL::Logger::LogDebug("[Palette] Palette created with %zu colors", count);
}

void Palette::SetColor(uint16_t index, HighColor&& color)
{
    if (index < Count)
    {
        Colors[index] = std::move(color);
        SRL::Logger::LogTrace("[Palette] SetColor[%d] = 0x%04X", index, color);
    }
    else
    {
        SRL::Logger::LogFatal("[Palette] SetColor: index(%d) out of bounds (max=%zu)", index, Count);
    }
}

HighColor Palette::GetColor(uint16_t index) const
{
    if (index < Count)
    {
        return Colors[index];
    }
    else
    {
        SRL::Logger::LogFatal("[Palette] GetColor: index(%d) out of bounds (max=%zu)", index, Count);
        return HighColor(0, 0, 0);
    }
}

void Palette::Init()
{
    SRL::Logger::LogDebug("[Palette] Initializing palette with gradient (%zu colors)", Count);

    // Fill with gradient: each component cycles at different rates for interesting colors
    for (uint16_t i = 0; i < Count - 1; ++i)
    {
        SetColor(i, HighColor::FromRGB555(i, (i * 2) % 256, (i * 4) % 256));
    }

    // Final entry is white for highlighting
    SetColor(Count - 1, HighColor::FromRGB555(255, 255, 255));

    SRL::Logger::LogDebug("[Palette] Palette initialization complete");
}

void Palette::ApplyBrightness(uint8_t brightness)
{
    if (Colors == nullptr)
    {
        SRL::Logger::LogWarning("[Palette] ApplyBrightness: null color array");
        return;
    }

    SRL::Logger::LogDebug("[Palette] Applying brightness %d to %zu colors", brightness, Count);

    for (uint16_t i = 0; i < Count; ++i)
    {
        HighColor color = Colors[i];

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
        Colors[i] = HighColor::FromRGB555(red << 3, green << 3, blue << 3);
    }

    SRL::Logger::LogTrace("[Palette] Brightness adjustment complete");
}

int16_t Palette::LoadPalette(SRL::Bitmap::BitmapInfo* bitmap)
{
    if (bitmap == nullptr || bitmap->Palette == nullptr)
    {
        SRL::Logger::LogFatal("[Palette::LoadPalette] Null bitmap or palette pointer");
        return -1;
    }

    // Get free CRAM bank for this color mode
    int32_t id = SRL::CRAM::GetFreeBank(bitmap->ColorMode);

    SRL::Logger::LogDebug("[Palette::LoadPalette] Found CRAM bank: %d (ColorMode: %d)", 
                         id, static_cast<int>(bitmap->ColorMode));

    if (id >= 0)
    {
        // Create CRAM palette entry
        SRL::CRAM::Palette cramPalette(bitmap->ColorMode, id);

        // Load colors into CRAM
        if (cramPalette.Load((HighColor*)bitmap->Palette->Colors, bitmap->Palette->Count) >= 0)
        {
            // Mark bank as in use
            SRL::CRAM::SetBankUsedState(id, bitmap->ColorMode, true);
            SRL::Logger::LogDebug("[Palette::LoadPalette] Palette loaded successfully at CRAM bank %d (%zu colors)", 
                                 id, bitmap->Palette->Count);
            return id;
        }
        else
        {
            SRL::Logger::LogFatal("[Palette::LoadPalette] Failed to load palette into CRAM bank %d", id);
        }
    }
    else
    {
        SRL::Logger::LogFatal("[Palette::LoadPalette] No free CRAM bank available for ColorMode %d", 
                             static_cast<int>(bitmap->ColorMode));
    }

    return -1;
}
