/*
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * Loading screen manager header.
 *
 * Centralises every aspect of the boot-time loading screen:
 * layout parameters, rendering, progress updates and teardown.
 * Modify LoadingLayout to adjust positions or colours without
 * touching any other game code.
 */

#ifndef NOIZ2SA_LOADING_SCREEN_H_
#define NOIZ2SA_LOADING_SCREEN_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Layout configuration
// ---------------------------------------------------------------------------

// All row and column values are character-cell coordinates as accepted by
// SRL::Debug::Print(col, row, text).
// barWidth must not exceed kLoadingScreenMaxBarWidth (62).
struct LoadingLayout
{
    // Background colour during loading (5-bit RGB for SRL::Types::HighColor).
    uint8_t bgR;
    uint8_t bgG;
    uint8_t bgB;

    // Background colour restored once loading is complete.
    uint8_t postBgR;
    uint8_t postBgG;
    uint8_t postBgB;

    // Left-most column for all text elements.
    int colLeft;

    // Row positions for each UI element.
    int rowTitle;    // Game title string ("NOIZ2SA")
    int rowPercent;  // "Loading... N%" line
    int rowBar;      // "[####----]" progress bar
    int rowStep;     // Current step description

    // Number of fill characters inside the progress bar brackets.
    int barWidth;
};

// ---------------------------------------------------------------------------
// LoadingScreen class
// ---------------------------------------------------------------------------

class LoadingScreen
{
public:
    // Maximum supported barWidth value.
    static constexpr int kMaxBarWidth = 62;
    static constexpr int kMaxHistoryLines = 4;
    static constexpr int kMaxStepText = 47;

    // Constructs with sensible defaults matching the original hard-coded values.
    LoadingScreen();

    // Constructs with a fully custom layout.
    explicit LoadingScreen(const LoadingLayout &layout);

    // Display the loading screen with current progress.
    // Clamps percent to [0, 100]. Substitutes "Loading" when step is nullptr.
    void Update(const char *step, int percent);

    // Clear the loading overlay and restore the post-load background colour.
    void Clear();

    // Read-only access to the active layout (useful for debugging or tests).
    const LoadingLayout &GetLayout() const { return _layout; }

private:
    void PushHistory(const char *step);
    void Render(const char *step, int percent);

    LoadingLayout _layout;
    int _historyCount;
    char _history[kMaxHistoryLines][kMaxStepText + 1];
};

// Global loading screen instance; defined in loading_screen.cpp.
extern LoadingScreen g_loadingScreen;

#endif // NOIZ2SA_LOADING_SCREEN_H_
