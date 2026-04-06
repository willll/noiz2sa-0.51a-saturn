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

namespace
{
static constexpr int kDefaultColLeft = 1;
static constexpr int kDefaultRowTitle = 1;
static constexpr int kDefaultRowPercent = 3;
static constexpr int kDefaultRowBar = 4;
static constexpr int kDefaultRowStep = 6;
static constexpr int kDefaultBarWidth = 20;

static constexpr int kDefaultBgR = 20;
static constexpr int kDefaultBgG = 10;
static constexpr int kDefaultBgB = 50;

static constexpr int kCreditsRowStart = 9;
static constexpr int kCreditsMaxCols = 34;

static const char *kCreditLine0 = "Copyright 2002 Kenta Cho. All rights reserved.";
static const char *kCreditLine1 = "Ported to SEGA Saturn by:";
static const char *kCreditLine2 = "https://github.com/willll/";
static const char *kCreditLine3 = "Using:";
static const char *kCreditLine4 = "https://github.com/ReyeMe/SaturnRingLib";
static const char *kCreditLine5 = "https://github.com/bimmerlabs/Ponesound-SRL";
static const char *kCreditLine6 = "https://github.com/ponut64/SCSP_poneSound";

static int printChunkedLine(int col, int row, const char *text, int maxCols)
{
    if (text == nullptr || text[0] == '\0')
    {
        return row;
    }

    const int cols = (maxCols > 0 && maxCols <= 63) ? maxCols : 34;
    char chunk[64];
    const size_t textLen = strlen(text);

    for (size_t offset = 0; offset < textLen; offset += (size_t)cols)
    {
        const size_t remain = textLen - offset;
        const size_t take = (remain < (size_t)cols) ? remain : (size_t)cols;
        memcpy(chunk, text + offset, take);
        chunk[take] = '\0';
        SRL::Debug::Print(col, row, chunk);
        row++;
    }

    return row;
}

static void ensureRuntimeDefaults(LoadingLayout &layout)
{
    // Some SH2 runtime paths may not run global C++ constructors reliably.
    // Keep rendering stable by applying sane defaults on first use.
    if (layout.colLeft == 0 && layout.rowTitle == 0 &&
        layout.rowPercent == 0 && layout.rowBar == 0 && layout.rowStep == 0)
    {
        layout.colLeft = kDefaultColLeft;
        layout.rowTitle = kDefaultRowTitle;
        layout.rowPercent = kDefaultRowPercent;
        layout.rowBar = kDefaultRowBar;
        layout.rowStep = kDefaultRowStep;
    }

    if (layout.barWidth <= 0 || layout.barWidth > LoadingScreen::kMaxBarWidth)
    {
        layout.barWidth = kDefaultBarWidth;
    }

    if (layout.bgR == 0 && layout.bgG == 0 && layout.bgB == 0)
    {
        layout.bgR = kDefaultBgR;
        layout.bgG = kDefaultBgG;
        layout.bgB = kDefaultBgB;
    }

    if (layout.postBgR == 0 && layout.postBgG == 0 && layout.postBgB == 0)
    {
        layout.postBgR = NOIZ2SA_POST_LOAD_BG_R;
        layout.postBgG = NOIZ2SA_POST_LOAD_BG_G;
        layout.postBgB = NOIZ2SA_POST_LOAD_BG_B;
    }
}
} // namespace

static int firstDigitIndex(const char *text)
{
    if (text == nullptr)
    {
        return -1;
    }

    for (int i = 0; text[i] != '\0'; i++)
    {
        if (text[i] >= '0' && text[i] <= '9')
        {
            return i;
        }
    }

    return -1;
}

static bool sameProgressFamily(const char *lhs, const char *rhs)
{
    if (lhs == nullptr || rhs == nullptr)
    {
        return false;
    }

    const int lhsDigit = firstDigitIndex(lhs);
    const int rhsDigit = firstDigitIndex(rhs);
    if (lhsDigit <= 0 || rhsDigit <= 0)
    {
        return false;
    }

    // Compare message stem up to the first digit. This collapses
    // lines like "Loading ZAKO 9/32" and "Loading ZAKO 10/32".
    if (lhsDigit != rhsDigit)
    {
        return false;
    }

    return strncmp(lhs, rhs, (size_t)lhsDigit) == 0;
}

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

    _historyCount = 0;
    for (int i = 0; i < kMaxHistoryLines; i++)
    {
        _history[i][0] = '\0';
    }
}

LoadingScreen::LoadingScreen(const LoadingLayout &layout)
    : _layout(layout)
{
    _historyCount = 0;
    for (int i = 0; i < kMaxHistoryLines; i++)
    {
        _history[i][0] = '\0';
    }
}

void LoadingScreen::PushHistory(const char *step)
{
    if (step == nullptr || step[0] == '\0')
    {
        return;
    }

    if (_historyCount > 0 && strcmp(_history[_historyCount - 1], step) == 0)
    {
        return;
    }

    if (_historyCount > 0 && sameProgressFamily(_history[_historyCount - 1], step))
    {
        snprintf(_history[_historyCount - 1], kMaxStepText + 1, "%s", step);
        return;
    }

    if (_historyCount < kMaxHistoryLines)
    {
        snprintf(_history[_historyCount], kMaxStepText + 1, "%s", step);
        _historyCount++;
        return;
    }

    for (int i = 1; i < kMaxHistoryLines; i++)
    {
        snprintf(_history[i - 1], kMaxStepText + 1, "%s", _history[i]);
    }
    snprintf(_history[kMaxHistoryLines - 1], kMaxStepText + 1, "%s", step);
}

// ---------------------------------------------------------------------------
// LoadingScreen::Update
// ---------------------------------------------------------------------------

void LoadingScreen::Update(const char *step, int percent)
{
    ensureRuntimeDefaults(_layout);

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
    ensureRuntimeDefaults(_layout);

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
    ensureRuntimeDefaults(_layout);

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
    char stepLine[kMaxStepText + 1];

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
    int creditsRow = kCreditsRowStart;
    creditsRow = printChunkedLine(_layout.colLeft, creditsRow, kCreditLine0, kCreditsMaxCols);
    creditsRow++;
    creditsRow = printChunkedLine(_layout.colLeft, creditsRow, kCreditLine1, kCreditsMaxCols);
    creditsRow = printChunkedLine(_layout.colLeft, creditsRow, kCreditLine2, kCreditsMaxCols);
    creditsRow++;
    creditsRow = printChunkedLine(_layout.colLeft, creditsRow, kCreditLine3, kCreditsMaxCols);
    creditsRow = printChunkedLine(_layout.colLeft, creditsRow, kCreditLine4, kCreditsMaxCols);
    creditsRow = printChunkedLine(_layout.colLeft, creditsRow, kCreditLine5, kCreditsMaxCols);
    printChunkedLine(_layout.colLeft, creditsRow, kCreditLine6, kCreditsMaxCols);
    SRL::Core::Synchronize();
}

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

LoadingScreen g_loadingScreen;
