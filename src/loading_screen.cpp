/*
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Loading screen manager implementation.
 *
 * Renders a simple text-mode progress display using SRL debug print
 * utilities.  All layout parameters come from the LoadingLayout struct
 * so they can be tuned without editing this file.
 */

#include "loading_screen.h"

#include <srl.hpp>
#include <srl_string.hpp>

// ---------------------------------------------------------------------------
// LoadingScreen — construction
// ---------------------------------------------------------------------------

LoadingScreen::LoadingScreen()
{
    // Loading backdrop: deep blue.
    _layout.bgR = 20;
    _layout.bgG = 10;
    _layout.bgB = 50;

    // Post-load background: driven by CMake compile definitions so they can
    // differ per build configuration without touching source code.
    _layout.postBgR = NOIZ2SA_POST_LOAD_BG_R;
    _layout.postBgG = NOIZ2SA_POST_LOAD_BG_G;
    _layout.postBgB = NOIZ2SA_POST_LOAD_BG_B;

    // Text layout — character-cell coordinates.
    _layout.colLeft    = 1;
    _layout.rowTitle   = 1;
    _layout.rowPercent = 3;
    _layout.rowBar     = 4;
    _layout.rowStep    = 6;

    // Progress bar fill width (number of '#'/'–' characters).
    _layout.barWidth = 20;
}

LoadingScreen::LoadingScreen(const LoadingLayout &layout)
    : _layout(layout)
{
}

// ---------------------------------------------------------------------------
// LoadingScreen::Update
// ---------------------------------------------------------------------------

void LoadingScreen::Update(const char *step, int percent)
{
    const char *displayStep = (step != nullptr) ? step : "Loading";

    int displayPercent = percent;
    if (displayPercent < 0)   displayPercent = 0;
    if (displayPercent > 100) displayPercent = 100;

    Render(displayStep, displayPercent);
}

// ---------------------------------------------------------------------------
// LoadingScreen::Clear
// ---------------------------------------------------------------------------

void LoadingScreen::Clear()
{
    SRL::Debug::PrintClearScreen();
    SRL::Debug::PrintColorRestore();

    // Restore the gameplay background colour configured at compile time.
    SRL::VDP2::SetBackColor(
        SRL::Types::HighColor(_layout.postBgR, _layout.postBgG, _layout.postBgB));
}

// ---------------------------------------------------------------------------
// LoadingScreen::Render  (private)
// ---------------------------------------------------------------------------

void LoadingScreen::Render(const char *step, int percent)
{
    SRL::VDP2::SetBackColor(
        SRL::Types::HighColor(_layout.bgR, _layout.bgG, _layout.bgB));

    // Clamp barWidth to the supported range so buffer sizes are always safe.
    const int width = (_layout.barWidth > 0 && _layout.barWidth <= kMaxBarWidth)
                      ? _layout.barWidth
                      : 20;

    // kMaxBarWidth + 1 keeps all bar-related buffers in bounds for any valid width.
    char bar[kMaxBarWidth + 1];
    char percentLine[32];
    char barLine[kMaxBarWidth + 4]; // '[' + bar + ']' + '\0'
    char stepLine[48];

    const int filled = (percent * width) / 100;
    for (int i = 0; i < width; i++)
    {
        bar[i] = (i < filled) ? '#' : '-';
    }
    bar[width] = '\0';

    snprintf(percentLine, sizeof(percentLine), "Loading... %d%%", percent);
    snprintf(barLine,     sizeof(barLine),     "[%s]",            bar);

    if (step != nullptr)
    {
        snprintf(stepLine, sizeof(stepLine), "%s", step);
    }
    else
    {
        stepLine[0] = '\0';
    }

    SRL::Debug::PrintColorSet(1);
    SRL::Debug::PrintClearScreen();
    SRL::Debug::Print(_layout.colLeft, _layout.rowTitle,   "NOIZ2SA");
    SRL::Debug::Print(_layout.colLeft, _layout.rowPercent, percentLine);
    SRL::Debug::Print(_layout.colLeft, _layout.rowBar,     barLine);
    SRL::Debug::Print(_layout.colLeft, _layout.rowStep,    stepLine);
    SRL::Core::Synchronize();
}

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

LoadingScreen g_loadingScreen;
