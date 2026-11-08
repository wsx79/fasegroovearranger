// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GraphicEqWindow.h"
#include "MainComponent.h"
#include "AudioEngine.h"
#include "Localisation.h"
#include "StudioTheme.h"
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

namespace fga {
namespace {

struct BiquadCoeffs {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    void setCoeffs(double b0_, double b1_, double b2_, double a0_, double a1_, double a2_) {
        b0 = b0_ / a0_; b1 = b1_ / a0_; b2 = b2_ / a0_;
        a1 = a1_ / a0_; a2 = a2_ / a0_;
    }

    double magnitudeDb(double f, double rate) const {
        const double phi = 6.283185307179586 * std::min(f, rate * 0.499) / rate;
        const double c1 = std::cos(phi), s1 = std::sin(phi);
        const double c2 = std::cos(2.0 * phi), s2 = std::sin(2.0 * phi);
        const double reNum = b0 + b1 * c1 + b2 * c2;
        const double imNum = -b1 * s1 - b2 * s2;
        const double reDen = 1.0 + a1 * c1 + a2 * c2;
        const double imDen = -a1 * s1 - a2 * s2;
        const double numSq = reNum * reNum + imNum * imNum;
        const double denSq = reDen * reDen + imDen * imDen;
        if (denSq < 1e-12) return 0.0;
        return 10.0 * std::log10(std::max(1e-12, numSq / denSq));
    }
};

static BiquadCoeffs computeBandCoeffs(const EqBand& b, double rate) {
    BiquadCoeffs f;
    if (!b.enabled) return f;
    const double hz = std::clamp(b.freq, 20.0, rate * 0.45);
    const double qVal = std::clamp(b.q, 0.1, 18.0);
    const double w = 6.283185307179586 * hz / rate;
    const double c = std::cos(w), s = std::sin(w), alpha = s / (2.0 * qVal);

    switch (b.type) {
        case (int)EqFilterType::LowCut:
            f.setCoeffs((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            break;
        case (int)EqFilterType::HighCut:
            f.setCoeffs((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            break;
        case (int)EqFilterType::Notch:
            f.setCoeffs(1.0, -2.0 * c, 1.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            break;
        case (int)EqFilterType::LowShelf: {
            const double A = std::pow(10.0, b.gain / 40.0);
            const double beta = 2.0 * std::sqrt(A) * alpha;
            f.setCoeffs(A * ((A + 1.0) - (A - 1.0) * c + beta),
                        2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                        A * ((A + 1.0) - (A - 1.0) * c - beta),
                        (A + 1.0) + (A - 1.0) * c + beta,
                        -2.0 * ((A - 1.0) + (A + 1.0) * c),
                        (A + 1.0) + (A - 1.0) * c - beta);
            break;
        }
        case (int)EqFilterType::HighShelf: {
            const double A = std::pow(10.0, b.gain / 40.0);
            const double beta = 2.0 * std::sqrt(A) * alpha;
            f.setCoeffs(A * ((A + 1.0) + (A - 1.0) * c + beta),
                        -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                        A * ((A + 1.0) + (A - 1.0) * c - beta),
                        (A + 1.0) - (A - 1.0) * c + beta,
                        2.0 * ((A - 1.0) - (A + 1.0) * c),
                        (A + 1.0) - (A - 1.0) * c - beta);
            break;
        }
        case (int)EqFilterType::Bell:
        default: {
            const double A = std::pow(10.0, b.gain / 40.0);
            f.setCoeffs(1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
            break;
        }
    }
    return f;
}

static const juce::Colour bandColours[] = {
    juce::Colour(0xff00e5ff), // 1: Electric Cyan
    juce::Colour(0xffffa000), // 2: Amber
    juce::Colour(0xffff4081), // 3: Neon Rose
    juce::Colour(0xff00e676), // 4: Emerald Green
    juce::Colour(0xff7c4dff), // 5: Deep Violet
    juce::Colour(0xffff6d00), // 6: Tangerine
    juce::Colour(0xff2979ff), // 7: Azure
    juce::Colour(0xffe040fb), // 8: Fuchsia
    juce::Colour(0xffaeea00), // 9: Lime
    juce::Colour(0xff00b0ff), // 10: Sky
    juce::Colour(0xffff5252), // 11: Coral Red
    juce::Colour(0xffffd600), // 12: Gold
    juce::Colour(0xff1de9b6), // 13: Teal
    juce::Colour(0xffb388ff), // 14: Lavender
    juce::Colour(0xffffab40), // 15: Apricot
    juce::Colour(0xff69f0ae)  // 16: Mint
};

static inline juce::Colour getBandColour(size_t index) {
    return bandColours[index % (sizeof(bandColours) / sizeof(bandColours[0]))];
}

static const char* filterTypeNames[] = {
    "BELL",
    "LOW CUT",
    "LOW SHELF",
    "HIGH SHELF",
    "HIGH CUT",
    "NOTCH"
};

class EqDisplayComponent : public juce::Component, public juce::Timer {
public:
    std::function<void()> onSettingsChanged;
    std::function<void()> onDragStart;
    std::function<void()> onDragEnd;
    std::function<float()> getTrackLevel;

    EqDisplayComponent() {
        setOpaque(true);
        setWantsKeyboardFocus(true);
        startTimerHz(30);
        spectrumBins.fill(0.0f);
    }

    ~EqDisplayComponent() override {
        stopTimer();
    }

    void setEq(const EqSettings& settings, int trackIndex, const juce::String& trackName) {
        currentEq = settings;
        currentEq.ensureBands();
        track = trackIndex;
        name = trackName;
        if (selectedBand >= (int)currentEq.bands.size()) selectedBand = -1;
        repaint();
    }

    const EqSettings& getEq() const { return currentEq; }

    int getSelectedBand() const { return selectedBand; }

    void addBandAt(double hz, double db, int type = (int)EqFilterType::Bell) {
        if (currentEq.bands.size() >= 16) return;
        if (onDragStart) onDragStart();
        EqBand b;
        b.enabled = true;
        b.type = type;
        b.freq = std::clamp(hz, 20.0, 20000.0);
        b.gain = std::clamp(db, -24.0, 24.0);
        b.q = (type == (int)EqFilterType::Notch) ? 4.0 : 0.707;
        b.normalise();
        currentEq.bands.push_back(b);
        currentEq.syncLegacyFromBands();
        selectedBand = (int)currentEq.bands.size() - 1;
        dragBand = selectedBand;
        if (onSettingsChanged) onSettingsChanged();
        if (onDragEnd) onDragEnd();
        repaint();
    }

    void deleteSelectedBand() {
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            if (onDragStart) onDragStart();
            currentEq.bands.erase(currentEq.bands.begin() + selectedBand);
            currentEq.syncLegacyFromBands();
            selectedBand = -1;
            dragBand = -1;
            if (onSettingsChanged) onSettingsChanged();
            if (onDragEnd) onDragEnd();
            repaint();
        }
    }

    void timerCallback() override {
        float lvl = getTrackLevel ? getTrackLevel() : 0.0f;
        bool changed = false;
        for (size_t i = 0; i < spectrumBins.size(); ++i) {
            float target = 0.0f;
            if (lvl > 0.001f) {
                float freqFactor = std::sin((float)i * 0.22f + (float)juce::Time::getMillisecondCounterHiRes() * 0.006f);
                target = std::clamp(lvl * (0.6f + 0.4f * freqFactor), 0.0f, 1.2f);
            }
            float cur = spectrumBins[i];
            if (target > cur) cur += (target - cur) * 0.55f;
            else cur *= 0.82f;
            if (std::abs(cur - spectrumBins[i]) > 0.002f) {
                spectrumBins[i] = cur;
                changed = true;
            }
        }
        if (changed) repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey)) {
            deleteSelectedBand();
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            selectedBand = -1;
            repaint();
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // 1. Sleek studio backdrop: dark graphite radial vignette
        juce::Colour bgCenter(0xff121822);
        juce::Colour bgBorder(0xff090c10);
        juce::ColourGradient bgGrad(bgCenter, bounds.getCentreX(), bounds.getCentreY(),
                                   bgBorder, 0.0f, 0.0f, true);
        g.setGradientFill(bgGrad);
        g.fillAll();

        const float padL = 40.0f, padR = 28.0f, padT = 24.0f, padB = 40.0f;
        const auto plotArea = juce::Rectangle<float>(padL, padT, bounds.getWidth() - padL - padR, bounds.getHeight() - padT - padB);

        // 2. High precision logarithmic grid
        // dB Grid
        static constexpr double dbTicks[] = { 24.0, 18.0, 12.0, 6.0, 0.0, -6.0, -12.0, -18.0, -24.0 };
        g.setFont(Daw::font(Daw::small, false));
        for (double db : dbTicks) {
            const float y = yFromDb(db, plotArea);
            const bool isZero = std::abs(db) < 1e-4;
            if (isZero) {
                // Accentuated 0 dB line with luminous blue glow
                g.setColour(juce::Colour(0x334488ff));
                g.drawHorizontalLine(std::lround(y - 1.0f), plotArea.getX(), plotArea.getRight());
                g.drawHorizontalLine(std::lround(y + 1.0f), plotArea.getX(), plotArea.getRight());
                g.setColour(juce::Colour(0xff4488ff));
                g.drawHorizontalLine(std::lround(y), plotArea.getX(), plotArea.getRight());
            } else {
                g.setColour(juce::Colour(0x18ffffff));
                g.drawHorizontalLine(std::lround(y), plotArea.getX(), plotArea.getRight());
            }

            g.setColour(isZero ? juce::Colour(0xff66aaff) : juce::Colour(0x889bb0c2));
            juce::String txt = (db > 0 ? "+" : "") + juce::String((int)db) + " dB";
            g.drawText(txt, 2, std::lround(y - 7.0f), (int)padL - 6, 14, juce::Justification::centredRight);
            g.drawText(txt, (int)plotArea.getRight() + 4, std::lround(y - 7.0f), (int)padR - 4, 14, juce::Justification::centredLeft);
        }

        // Frequency Grid (Logarithmic)
        static constexpr double majorFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (double hz : majorFreqs) {
            const float x = xFromFreq(hz, plotArea);
            g.setColour(juce::Colour(0x22ffffff));
            g.drawVerticalLine(std::lround(x), plotArea.getY(), plotArea.getBottom());
            g.setColour(juce::Colour(0x999bb0c2));
            juce::String label = hz >= 1000 ? juce::String((int)(hz / 1000)) + "k" : juce::String((int)hz);
            g.drawText(label, std::lround(x - 20.0f), std::lround(plotArea.getBottom() + 4.0f), 40, 14, juce::Justification::centred);
        }
        // Intermediate subtle frequency ticks
        static constexpr double minorFreqs[] = { 30, 40, 60, 70, 80, 90, 300, 400, 600, 700, 800, 900, 3000, 4000, 6000, 7000, 8000, 9000, 15000 };
        g.setColour(juce::Colour(0x0affffff));
        for (double hz : minorFreqs) {
            const float x = xFromFreq(hz, plotArea);
            g.drawVerticalLine(std::lround(x), plotArea.getY(), plotArea.getBottom());
        }

        // Piano Key / Octave Musical Ruler along bottom
        const float rulerY = plotArea.getBottom() + 19.0f;
        const float rulerH = 14.0f;
        g.setColour(juce::Colour(0x10ffffff));
        g.fillRect(plotArea.getX(), rulerY, plotArea.getWidth(), rulerH);
        g.setColour(juce::Colour(0x25ffffff));
        g.drawRect(plotArea.getX(), rulerY, plotArea.getWidth(), rulerH, 1.0f);

        // Musical pitches C1 to C8
        static const struct { double hz; const char* note; } notes[] = {
            { 32.7, "C1" }, { 65.4, "C2" }, { 130.8, "C3" }, { 261.6, "C4" },
            { 523.2, "C5" }, { 1046.5, "C6" }, { 2093.0, "C7" }, { 4186.0, "C8" }
        };
        g.setFont(Daw::font(Daw::small, true));
        for (const auto& n : notes) {
            const float x = xFromFreq(n.hz, plotArea);
            g.setColour(juce::Colour(0x3344aaff));
            g.drawVerticalLine(std::lround(x), rulerY, rulerY + rulerH);
            g.setColour(juce::Colour(0x889bb0c2));
            g.drawText(n.note, std::lround(x - 12.0f), std::lround(rulerY), 24, (int)rulerH, juce::Justification::centred);
        }

        // 3. Live Spectrum Analyzer (Smooth audio-reactive background glow)
        {
            juce::Path specPath;
            specPath.startNewSubPath(plotArea.getX(), plotArea.getBottom());
            const float binW = plotArea.getWidth() / (float)(spectrumBins.size() - 1);
            for (size_t b = 0; b < spectrumBins.size(); ++b) {
                const float x = plotArea.getX() + (float)b * binW;
                const float h = spectrumBins[b] * (plotArea.getHeight() * 0.7f);
                const float y = plotArea.getBottom() - h;
                specPath.lineTo(x, y);
            }
            specPath.lineTo(plotArea.getRight(), plotArea.getBottom());
            specPath.closeSubPath();

            juce::ColourGradient specGrad(juce::Colour(0x2e8844ff), 0.0f, plotArea.getY() + 30.0f,
                                          juce::Colour(0x028844ff), 0.0f, plotArea.getBottom(), false);
            g.setGradientFill(specGrad);
            g.fillPath(specPath);
        }

        // 4. Compute filter coefficients for all bands
        std::vector<BiquadCoeffs> bandCoeffs;
        bandCoeffs.reserve(currentEq.bands.size());
        for (const auto& b : currentEq.bands) {
            bandCoeffs.push_back(computeBandCoeffs(b, 48000.0));
        }

        // 5. Draw Individual Band Response Curves (when hovered or selected, or faint)
        const int steps = (int)plotArea.getWidth();
        const float yZero = yFromDb(0.0, plotArea);

        if (steps > 20 && currentEq.enabled) {
            for (size_t bIdx = 0; bIdx < currentEq.bands.size(); ++bIdx) {
                const auto& b = currentEq.bands[bIdx];
                if (!b.enabled) continue;
                const bool isSel = (selectedBand == (int)bIdx);
                const bool isHov = (hoverBand == (int)bIdx);
                if (!isSel && !isHov && currentEq.bands.size() > 6) continue;

                juce::Path bandPath;
                for (int i = 0; i < steps; i += 2) {
                    const float x = plotArea.getX() + (float)i;
                    const double hz = freqFromX(x, plotArea);
                    const double db = bandCoeffs[bIdx].magnitudeDb(hz, 48000.0);
                    const float y = yFromDb(std::clamp(db, -26.0, 26.0), plotArea);
                    if (i == 0) bandPath.startNewSubPath(x, y);
                    else bandPath.lineTo(x, y);
                }

                auto bCol = getBandColour(bIdx);
                float alpha = isSel ? 0.85f : (isHov ? 0.65f : 0.35f);
                g.setColour(bCol.withAlpha(alpha));
                g.strokePath(bandPath, juce::PathStrokeType(isSel ? 1.8f : 1.2f, juce::PathStrokeType::curved));
            }
        }

        // 6. Draw Composite Master Curve with multi-pass luminous glow & gradient fill
        if (steps > 20) {
            juce::Path curvePath;
            juce::Path fillBoostPath;
            juce::Path fillCutPath;

            fillBoostPath.startNewSubPath(plotArea.getX(), yZero);
            fillCutPath.startNewSubPath(plotArea.getX(), yZero);

            for (int i = 0; i < steps; ++i) {
                const float x = plotArea.getX() + (float)i;
                const double hz = freqFromX(x, plotArea);
                double totalDb = 0.0;
                if (currentEq.enabled) {
                    for (const auto& coeff : bandCoeffs) {
                        totalDb += coeff.magnitudeDb(hz, 48000.0);
                    }
                }
                const float y = yFromDb(std::clamp(totalDb, -26.0, 26.0), plotArea);
                if (i == 0) curvePath.startNewSubPath(x, y);
                else curvePath.lineTo(x, y);

                fillBoostPath.lineTo(x, std::min(y, yZero));
                fillCutPath.lineTo(x, std::max(y, yZero));
            }

            fillBoostPath.lineTo(plotArea.getRight(), yZero);
            fillBoostPath.closeSubPath();

            fillCutPath.lineTo(plotArea.getRight(), yZero);
            fillCutPath.closeSubPath();

            // Boost fill (warm neon cyan upward glow)
            juce::Colour neonCyan(0xff00f0ff);
            juce::Colour glowCyan = neonCyan.withAlpha(currentEq.enabled ? 0.28f : 0.05f);
            juce::ColourGradient boostGrad(glowCyan, 0.0f, plotArea.getY(), juce::Colour(0x0000f0ff), 0.0f, yZero, false);
            g.setGradientFill(boostGrad);
            g.fillPath(fillBoostPath);

            // Cut fill (deep subtle azure downward fade)
            juce::Colour cutAzure(0xff0055aa);
            juce::Colour glowAzure = cutAzure.withAlpha(currentEq.enabled ? 0.22f : 0.04f);
            juce::ColourGradient cutGrad(juce::Colour(0x000055aa), 0.0f, yZero, glowAzure, 0.0f, plotArea.getBottom(), false);
            g.setGradientFill(cutGrad);
            g.fillPath(fillCutPath);

            // Curve stroke: multi-pass bloom
            if (currentEq.enabled) {
                // Soft outer aura
                g.setColour(neonCyan.withAlpha(0.25f));
                g.strokePath(curvePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                // Core sharp stroke
                g.setColour(neonCyan);
                g.strokePath(curvePath, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            } else {
                g.setColour(juce::Colour(0x668899aa));
                g.strokePath(curvePath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // 7. Draw Band Nodes (FabFilter Pro-Q style handle circles)
        for (size_t i = 0; i < currentEq.bands.size(); ++i) {
            const auto& b = currentEq.bands[i];
            const auto pt = getNodePosition((int)i, plotArea);
            const bool isHovered = (hoverBand == (int)i);
            const bool isSelected = (selectedBand == (int)i);
            const bool isDragging = (dragBand == (int)i);
            const auto col = getBandColour(i);

            // Axis projection lines when dragging/hovered
            if (isDragging || isHovered) {
                static const float dashLengths[] = { 3.0f, 3.0f };
                g.setColour(col.withAlpha(0.35f));
                g.drawDashedLine(juce::Line<float>(pt.x, plotArea.getY(), pt.x, plotArea.getBottom()), dashLengths, 2, 1.0f);
                g.drawDashedLine(juce::Line<float>(plotArea.getX(), pt.y, plotArea.getRight(), pt.y), dashLengths, 2, 1.0f);
            }

            // Outer glowing aura
            if (isSelected || isDragging || isHovered) {
                g.setColour(col.withAlpha(isSelected ? 0.45f : 0.25f));
                g.fillEllipse(pt.x - 18.0f, pt.y - 18.0f, 36.0f, 36.0f);
            }

            // Outer ring
            g.setColour(isSelected ? juce::Colours::white : (b.enabled ? col : col.withAlpha(0.4f)));
            g.drawEllipse(pt.x - 12.0f, pt.y - 12.0f, 24.0f, 24.0f, isSelected ? 2.5f : 2.0f);

            // Dark inner disc
            g.setColour(juce::Colour(0xff0c1017));
            g.fillEllipse(pt.x - 10.0f, pt.y - 10.0f, 20.0f, 20.0f);

            // Band index text inside disc
            g.setColour(b.enabled ? (isSelected ? juce::Colours::white : col) : Daw::textDim);
            g.setFont(Daw::font(Daw::small, true));
            g.drawText(juce::String((int)i + 1), std::lround(pt.x - 10.0f), std::lround(pt.y - 9.0f), 20, 18, juce::Justification::centred);
        }

        // 8. Floating Parameter Card HUD (FabFilter Pro-Q style docked at bottom)
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            drawBandHud(g, plotArea);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        int found = findBandAt(e.position, plotArea);
        if (hoverBand != found) {
            hoverBand = found;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        dragBand = -1;
        dragHudParam = -1;

        // Check if clicked in HUD
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            auto hudRect = getHudBounds(plotArea);
            if (hudRect.contains(e.position)) {
                handleHudMouseDown(e, hudRect);
                return;
            }
        }

        // Check node click
        int found = findBandAt(e.position, plotArea);
        if (found >= 0) {
            if (e.mods.isPopupMenu()) {
                showBandMenu(found, e.getScreenPosition());
                return;
            }
            if (e.getNumberOfClicks() >= 2) {
                // Double-click node toggles / resets
                auto& b = currentEq.bands[(size_t)found];
                if (std::abs(b.gain) > 0.1) b.gain = 0.0;
                else deleteSelectedBand();
                currentEq.syncLegacyFromBands();
                if (onSettingsChanged) onSettingsChanged();
                repaint();
                return;
            }
            selectedBand = found;
            dragBand = found;
            if (onDragStart) onDragStart();
            repaint();
            return;
        }

        // Click on empty canvas
        if (e.mods.isPopupMenu()) {
            const double hz = std::clamp(freqFromX(e.position.x, plotArea), 20.0, 20000.0);
            const double db = std::clamp(dbFromY(e.position.y, plotArea), -24.0, 24.0);
            showCanvasMenu(hz, db, e.getScreenPosition());
            return;
        }

        // Click outside: deselect band
        if (selectedBand != -1) {
            selectedBand = -1;
            repaint();
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        const auto plotArea = getPlotArea();
        if (!plotArea.contains(e.position)) return;

        // Double-click creates a new band!
        const double hz = std::clamp(freqFromX(e.position.x, plotArea), 20.0, 20000.0);
        const double db = std::clamp(dbFromY(e.position.y, plotArea), -24.0, 24.0);
        int type = (int)EqFilterType::Bell;
        if (hz < 60.0 && db < -4.0) type = (int)EqFilterType::LowCut;
        else if (hz > 14000.0 && db < -4.0) type = (int)EqFilterType::HighCut;
        else if (hz < 150.0) type = (int)EqFilterType::LowShelf;
        else if (hz > 8000.0) type = (int)EqFilterType::HighShelf;

        addBandAt(hz, db, type);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();

        // Dragging HUD parameter slider
        if (dragHudParam >= 0 && selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)selectedBand];
            const float dx = (float)e.getDistanceFromDragStartX();
            if (dragHudParam == 0) { // Frequency
                b.freq = std::clamp(dragStartParamVal * std::pow(2.0, (double)dx * 0.005), 20.0, 20000.0);
            } else if (dragHudParam == 1) { // Gain
                b.gain = std::clamp(dragStartParamVal + (double)dx * 0.1, -24.0, 24.0);
            } else if (dragHudParam == 2) { // Q
                b.q = std::clamp(dragStartParamVal * std::pow(2.0, (double)dx * 0.008), 0.1, 18.0);
            }
            b.normalise();
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
            return;
        }

        // Dragging band node
        if (dragBand >= 0 && dragBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)dragBand];
            b.freq = std::clamp(freqFromX(e.position.x, plotArea), 20.0, 20000.0);
            if (b.type != (int)EqFilterType::LowCut && b.type != (int)EqFilterType::HighCut && b.type != (int)EqFilterType::Notch) {
                b.gain = std::clamp(dbFromY(e.position.y, plotArea), -24.0, 24.0);
            }
            b.normalise();
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (dragBand >= 0 || dragHudParam >= 0) {
            dragBand = -1;
            dragHudParam = -1;
            if (onDragEnd) onDragEnd();
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        int targetBand = (hoverBand >= 0) ? hoverBand : selectedBand;
        if (targetBand >= 0 && targetBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)targetBand];
            const double factor = wheel.deltaY > 0 ? 1.12 : 0.89;
            b.q = std::clamp(b.q * factor, 0.1, 18.0);
            b.normalise();
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
        }
    }

private:
    EqSettings currentEq;
    int track = 0;
    juce::String name;
    int hoverBand = -1;
    int dragBand = -1;
    int selectedBand = -1;

    // HUD parameter dragging
    int dragHudParam = -1; // 0: freq, 1: gain, 2: q
    double dragStartParamVal = 0.0;

    std::array<float, 32> spectrumBins{};

    juce::Rectangle<float> getPlotArea() const {
        const float padL = 40.0f, padR = 28.0f, padT = 24.0f, padB = 40.0f;
        return juce::Rectangle<float>(padL, padT, (float)getWidth() - padL - padR, (float)getHeight() - padT - padB);
    }

    static float xFromFreq(double hz, const juce::Rectangle<float>& r) {
        hz = std::clamp(hz, 20.0, 20000.0);
        const double norm = std::log10(hz / 20.0) / std::log10(20000.0 / 20.0);
        return r.getX() + (float)(norm * r.getWidth());
    }

    static double freqFromX(float x, const juce::Rectangle<float>& r) {
        const double norm = std::clamp((double)(x - r.getX()) / (double)r.getWidth(), 0.0, 1.0);
        return 20.0 * std::pow(20000.0 / 20.0, norm);
    }

    static float yFromDb(double db, const juce::Rectangle<float>& r) {
        const double minDb = -26.0, maxDb = 26.0;
        const double norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return r.getY() + (float)((1.0 - norm) * r.getHeight());
    }

    static double dbFromY(float y, const juce::Rectangle<float>& r) {
        const double minDb = -26.0, maxDb = 26.0;
        const double norm = 1.0 - std::clamp((double)(y - r.getY()) / (double)r.getHeight(), 0.0, 1.0);
        return minDb + norm * (maxDb - minDb);
    }

    juce::Point<float> getNodePosition(int index, const juce::Rectangle<float>& r) const {
        if (index < 0 || index >= (int)currentEq.bands.size()) return { 0, 0 };
        const auto& b = currentEq.bands[(size_t)index];
        const double db = (b.type == (int)EqFilterType::LowCut || b.type == (int)EqFilterType::HighCut || b.type == (int)EqFilterType::Notch) ? 0.0 : b.gain;
        return { xFromFreq(b.freq, r), yFromDb(db, r) };
    }

    int findBandAt(juce::Point<float> pos, const juce::Rectangle<float>& r) const {
        for (int i = 0; i < (int)currentEq.bands.size(); ++i) {
            if (pos.getDistanceFrom(getNodePosition(i, r)) <= 15.0f) {
                return i;
            }
        }
        return -1;
    }

    juce::Rectangle<float> getHudBounds(const juce::Rectangle<float>& plotArea) const {
        const float hudW = std::min(700.0f, plotArea.getWidth() - 20.0f);
        const float hudH = 50.0f;
        const float hudX = plotArea.getX() + (plotArea.getWidth() - hudW) * 0.5f;
        const float hudY = plotArea.getBottom() - hudH - 8.0f;
        return { hudX, hudY, hudW, hudH };
    }

    void drawBandHud(juce::Graphics& g, const juce::Rectangle<float>& plotArea) {
        auto hud = getHudBounds(plotArea);
        const auto& b = currentEq.bands[(size_t)selectedBand];
        const auto bCol = getBandColour(selectedBand);

        // Glass background with neon band border
        g.setColour(juce::Colour(0xf00b0f16));
        g.fillRoundedRectangle(hud, 8.0f);
        g.setColour(bCol.withAlpha(0.85f));
        g.drawRoundedRectangle(hud, 8.0f, 1.5f);

        auto r = hud.reduced(6.0f, 6.0f);

        // 1. Band Badge capsule
        auto badgeRect = r.removeFromLeft(68.0f).reduced(2.0f);
        g.setColour(bCol.withAlpha(0.25f));
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setColour(bCol);
        g.drawRoundedRectangle(badgeRect, 4.0f, 1.0f);
        g.setFont(Daw::font(Daw::small, true));
        g.setColour(juce::Colours::white);
        g.drawText("BAND " + juce::String(selectedBand + 1), badgeRect, juce::Justification::centred);

        r.removeFromLeft(6.0f);

        // 2. Shape Pills (Bell, Low Cut, Low Shelf, High Shelf, High Cut, Notch)
        const float pillW = 44.0f;
        static const char* pillLabels[] = { "BELL", "LCUT", "LSHF", "HSHF", "HCUT", "NOTCH" };
        for (int i = 0; i < 6; ++i) {
            auto pill = r.removeFromLeft(pillW).reduced(1.0f, 4.0f);
            const bool active = (b.type == i);
            if (active) {
                g.setColour(bCol);
                g.fillRoundedRectangle(pill, 4.0f);
                g.setColour(juce::Colour(0xff090c10));
            } else {
                g.setColour(juce::Colour(0x22ffffff));
                g.fillRoundedRectangle(pill, 4.0f);
                g.setColour(Daw::textDim);
            }
            g.setFont(Daw::font(Daw::small, true));
            g.drawText(pillLabels[i], pill, juce::Justification::centred);
        }

        r.removeFromLeft(10.0f);

        // 3. Frequency Box
        auto freqBox = r.removeFromLeft(88.0f).reduced(2.0f, 2.0f);
        g.setColour(juce::Colour(0x18ffffff));
        g.fillRoundedRectangle(freqBox, 4.0f);
        g.setFont(Daw::font(Daw::small, false));
        g.setColour(Daw::textDim);
        g.drawText("FREQ", freqBox.removeFromTop(12.0f), juce::Justification::centred);
        g.setFont(Daw::font(Daw::body, true));
        g.setColour(Daw::text);
        juce::String fStr = (b.freq >= 1000.0) ? juce::String(b.freq / 1000.0, 2) + " kHz" : juce::String((int)b.freq) + " Hz";
        g.drawText(fStr, freqBox, juce::Justification::centred);

        r.removeFromLeft(6.0f);

        // 4. Gain Box
        auto gainBox = r.removeFromLeft(76.0f).reduced(2.0f, 2.0f);
        const bool gainDisabled = (b.type == (int)EqFilterType::LowCut || b.type == (int)EqFilterType::HighCut || b.type == (int)EqFilterType::Notch);
        g.setColour(juce::Colour(0x18ffffff));
        g.fillRoundedRectangle(gainBox, 4.0f);
        g.setFont(Daw::font(Daw::small, false));
        g.setColour(Daw::textDim);
        g.drawText("GAIN", gainBox.removeFromTop(12.0f), juce::Justification::centred);
        g.setFont(Daw::font(Daw::body, true));
        if (gainDisabled) {
            g.setColour(Daw::textDim.withAlpha(0.4f));
            g.drawText("—", gainBox, juce::Justification::centred);
        } else {
            g.setColour(b.gain > 0.1 ? juce::Colour(0xffffbb33) : (b.gain < -0.1 ? juce::Colour(0xff33bbff) : Daw::text));
            g.drawText((b.gain > 0 ? "+" : "") + juce::String(b.gain, 1) + " dB", gainBox, juce::Justification::centred);
        }

        r.removeFromLeft(6.0f);

        // 5. Q Factor Box
        auto qBox = r.removeFromLeft(68.0f).reduced(2.0f, 2.0f);
        g.setColour(juce::Colour(0x18ffffff));
        g.fillRoundedRectangle(qBox, 4.0f);
        g.setFont(Daw::font(Daw::small, false));
        g.setColour(Daw::textDim);
        g.drawText("Q", qBox.removeFromTop(12.0f), juce::Justification::centred);
        g.setFont(Daw::font(Daw::body, true));
        g.setColour(Daw::text);
        g.drawText(juce::String(b.q, 2), qBox, juce::Justification::centred);

        r.removeFromLeft(8.0f);

        // 6. Action Buttons: Bypass, Invert, Delete
        auto bypassBtn = r.removeFromLeft(36.0f).reduced(2.0f, 4.0f);
        g.setColour(b.enabled ? bCol.withAlpha(0.3f) : juce::Colour(0x22ffffff));
        g.fillRoundedRectangle(bypassBtn, 4.0f);
        g.setColour(b.enabled ? bCol : Daw::textDim);
        g.drawRoundedRectangle(bypassBtn, 4.0f, 1.0f);
        g.drawText("⏻", bypassBtn, juce::Justification::centred);

        r.removeFromLeft(4.0f);

        auto invertBtn = r.removeFromLeft(36.0f).reduced(2.0f, 4.0f);
        g.setColour(juce::Colour(0x22ffffff));
        g.fillRoundedRectangle(invertBtn, 4.0f);
        g.setColour(gainDisabled ? Daw::textDim.withAlpha(0.3f) : Daw::text);
        g.drawRoundedRectangle(invertBtn, 4.0f, 1.0f);
        g.drawText("±", invertBtn, juce::Justification::centred);

        r.removeFromLeft(4.0f);

        auto delBtn = r.removeFromLeft(36.0f).reduced(2.0f, 4.0f);
        g.setColour(juce::Colour(0x33ff4444));
        g.fillRoundedRectangle(delBtn, 4.0f);
        g.setColour(juce::Colour(0xffff5555));
        g.drawRoundedRectangle(delBtn, 4.0f, 1.0f);
        g.drawText("✕", delBtn, juce::Justification::centred);
    }

    void handleHudMouseDown(const juce::MouseEvent& e, const juce::Rectangle<float>& hud) {
        auto r = hud.reduced(6.0f, 6.0f);
        r.removeFromLeft(68.0f + 6.0f); // Badge

        auto& b = currentEq.bands[(size_t)selectedBand];

        // 6 Shape Pills
        const float pillW = 44.0f;
        for (int i = 0; i < 6; ++i) {
            auto pill = r.removeFromLeft(pillW).reduced(1.0f, 4.0f);
            if (pill.contains(e.position)) {
                b.type = i;
                b.normalise();
                currentEq.syncLegacyFromBands();
                if (onSettingsChanged) onSettingsChanged();
                repaint();
                return;
            }
        }

        r.removeFromLeft(10.0f);

        // Frequency Box
        auto freqBox = r.removeFromLeft(88.0f).reduced(2.0f, 2.0f);
        if (freqBox.contains(e.position)) {
            dragHudParam = 0;
            dragStartParamVal = b.freq;
            if (onDragStart) onDragStart();
            return;
        }

        r.removeFromLeft(6.0f);

        // Gain Box
        auto gainBox = r.removeFromLeft(76.0f).reduced(2.0f, 2.0f);
        if (gainBox.contains(e.position)) {
            dragHudParam = 1;
            dragStartParamVal = b.gain;
            if (onDragStart) onDragStart();
            return;
        }

        r.removeFromLeft(6.0f);

        // Q Box
        auto qBox = r.removeFromLeft(68.0f).reduced(2.0f, 2.0f);
        if (qBox.contains(e.position)) {
            dragHudParam = 2;
            dragStartParamVal = b.q;
            if (onDragStart) onDragStart();
            return;
        }

        r.removeFromLeft(8.0f);

        // Bypass
        auto bypassBtn = r.removeFromLeft(36.0f).reduced(2.0f, 4.0f);
        if (bypassBtn.contains(e.position)) {
            b.enabled = !b.enabled;
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
            return;
        }

        r.removeFromLeft(4.0f);

        // Invert
        auto invertBtn = r.removeFromLeft(36.0f).reduced(2.0f, 4.0f);
        if (invertBtn.contains(e.position)) {
            b.gain = -b.gain;
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
            return;
        }

        r.removeFromLeft(4.0f);

        // Delete
        auto delBtn = r.removeFromLeft(36.0f).reduced(2.0f, 4.0f);
        if (delBtn.contains(e.position)) {
            deleteSelectedBand();
            return;
        }
    }

    void showBandMenu(int bandIndex, juce::Point<int> screenPos) {
        selectedBand = bandIndex;
        repaint();

        juce::PopupMenu m;
        m.addSectionHeader("BAND " + juce::String(bandIndex + 1) + " FILTER TYPE");
        for (int i = 0; i < 6; ++i) {
            m.addItem(10 + i, filterTypeNames[i], true, currentEq.bands[(size_t)bandIndex].type == i);
        }
        m.addSeparator();
        m.addItem(1, "Invert Gain", true);
        m.addItem(2, currentEq.bands[(size_t)bandIndex].enabled ? "Bypass Band" : "Enable Band", true);
        m.addSeparator();
        m.addItem(3, "Delete Band", true);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ screenPos.x, screenPos.y, 1, 1 }),
            [this, bandIndex](int result) {
                if (result >= 10 && result <= 15 && bandIndex < (int)currentEq.bands.size()) {
                    currentEq.bands[(size_t)bandIndex].type = result - 10;
                    currentEq.syncLegacyFromBands();
                    if (onSettingsChanged) onSettingsChanged();
                    repaint();
                } else if (result == 1 && bandIndex < (int)currentEq.bands.size()) {
                    currentEq.bands[(size_t)bandIndex].gain = -currentEq.bands[(size_t)bandIndex].gain;
                    currentEq.syncLegacyFromBands();
                    if (onSettingsChanged) onSettingsChanged();
                    repaint();
                } else if (result == 2 && bandIndex < (int)currentEq.bands.size()) {
                    currentEq.bands[(size_t)bandIndex].enabled = !currentEq.bands[(size_t)bandIndex].enabled;
                    currentEq.syncLegacyFromBands();
                    if (onSettingsChanged) onSettingsChanged();
                    repaint();
                } else if (result == 3) {
                    deleteSelectedBand();
                }
            });
    }

    void showCanvasMenu(double hz, double db, juce::Point<int> screenPos) {
        juce::PopupMenu m;
        m.addSectionHeader("ADD EQ BAND");
        m.addItem(10, "Add Bell Filter here", currentEq.bands.size() < 16);
        m.addItem(11, "Add Low Cut here", currentEq.bands.size() < 16);
        m.addItem(12, "Add Low Shelf here", currentEq.bands.size() < 16);
        m.addItem(13, "Add High Shelf here", currentEq.bands.size() < 16);
        m.addItem(14, "Add High Cut here", currentEq.bands.size() < 16);
        m.addItem(15, "Add Notch here", currentEq.bands.size() < 16);
        m.addSeparator();
        m.addItem(1, "Reset All to Flat", true);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ screenPos.x, screenPos.y, 1, 1 }),
            [this, hz, db](int result) {
                if (result >= 10 && result <= 15) {
                    addBandAt(hz, db, result - 10);
                } else if (result == 1) {
                    for (auto& b : currentEq.bands) b.gain = 0.0;
                    currentEq.syncLegacyFromBands();
                    if (onSettingsChanged) onSettingsChanged();
                    repaint();
                }
            });
    }
};

} // namespace

class GraphicEqWindow::Panel : public juce::Component {
public:
    explicit Panel(MainComponent& m) : owner(m) {
        addAndMakeVisible(eqDisplay);
        addAndMakeVisible(trackSelector);
        addAndMakeVisible(addBandButton);
        addAndMakeVisible(enableButton);
        addAndMakeVisible(resetButton);

        addBandButton.setButtonText(tr("+ ADD BAND"));
        Localisation::bindTip(addBandButton, "Add a new parametric band to the EQ.");
        Localisation::bindTip(enableButton, "Toggle EQ on or off for this track.");
        Localisation::bindTip(resetButton, "Reset all EQ bands to flat (0 dB).");

        eqDisplay.getTrackLevel = [this] {
            if (currentTrack < 0 || currentTrack >= (int)owner.project.tracks.size()) return 0.0f;
            return owner.audio.level(currentTrack);
        };

        addBandButton.onClick = [this] {
            if (!validTrack()) return;
            owner.checkpointMix();
            eqDisplay.addBandAt(1000.0, 0.0, (int)EqFilterType::Bell);
        };

        enableButton.onClick = [this] {
            if (updating || !validTrack()) return;
            owner.checkpoint();
            owner.project.tracks[(size_t)currentTrack].eq.enabled = enableButton.getToggleState();
            owner.mixChanged();
            refresh();
        };

        resetButton.onClick = [this] {
            if (!validTrack()) return;
            owner.checkpoint();
            auto& q = owner.project.tracks[(size_t)currentTrack].eq;
            for (auto& b : q.bands) b.gain = 0.0;
            q.syncLegacyFromBands();
            owner.mixChanged();
            refresh();
        };

        trackSelector.onChange = [this] {
            if (!updating && trackSelector.getSelectedId() > 0) {
                currentTrack = trackSelector.getSelectedId() - 1;
                owner.select(currentTrack, owner.selectedSection);
                refresh();
            }
        };

        eqDisplay.onDragStart = [this] {
            if (validTrack()) owner.checkpointMix();
        };

        eqDisplay.onSettingsChanged = [this] {
            if (updating || !validTrack()) return;
            owner.project.tracks[(size_t)currentTrack].eq = eqDisplay.getEq();
            owner.mixChanged();
        };

        eqDisplay.onDragEnd = [this] {
            refresh();
        };
    }

    void setTrack(int track) {
        if (track >= 0 && track < (int)owner.project.tracks.size()) {
            currentTrack = track;
        } else {
            currentTrack = std::clamp(owner.selectedTrack, 0, (int)owner.project.tracks.size() - 1);
        }
        refresh();
    }

    void refresh() {
        if (owner.project.tracks.empty()) return;
        updating = true;
        currentTrack = std::clamp(currentTrack, 0, (int)owner.project.tracks.size() - 1);
        auto& t = owner.project.tracks[(size_t)currentTrack];
        t.eq.ensureBands();

        juce::StringArray names;
        for (const auto& trk : owner.project.tracks) {
            names.add(juce::String::fromUTF8(trk.name.c_str()));
        }
        trackSelector.clear(juce::dontSendNotification);
        trackSelector.addItemList(names, 1);
        trackSelector.setSelectedId(currentTrack + 1, juce::dontSendNotification);

        enableButton.setButtonText(t.eq.enabled ? tr("EQ ACTIVE") : tr("EQ BYPASSED"));
        enableButton.setToggleState(t.eq.enabled, juce::dontSendNotification);
        resetButton.setButtonText(tr("FLAT"));

        eqDisplay.setEq(t.eq, currentTrack, juce::String::fromUTF8(t.name.c_str()));
        updating = false;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        // Sleek dark toolbar
        g.fillAll(juce::Colour(0xff0e131b));

        // Pro title with track color badge
        g.setColour(juce::Colour(0xff00e5ff));
        g.fillRoundedRectangle(16.0f, 13.0f, 6.0f, 18.0f, 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(Daw::font(Daw::display, true));
        g.drawText("PRO PARAMETRIC EQUALIZER", 28, 10, 260, 24, juce::Justification::centredLeft);

        // Hints
        g.setFont(Daw::font(Daw::small, false));
        g.setColour(juce::Colour(0x889bb0c2));
        g.drawText(tr("Double-click to add band • Drag to sculpt • Wheel adjusts Q • Del to remove"),
                   getWidth() - 480, 12, 460, 20, juce::Justification::centredRight);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(14);
        auto topBar = r.removeFromTop(34);
        r.removeFromTop(10);

        topBar.removeFromLeft(280); // Title spacing
        trackSelector.setBounds(topBar.removeFromLeft(180));
        topBar.removeFromLeft(10);
        addBandButton.setBounds(topBar.removeFromLeft(110));
        topBar.removeFromLeft(10);
        enableButton.setBounds(topBar.removeFromLeft(120));
        topBar.removeFromLeft(10);
        resetButton.setBounds(topBar.removeFromLeft(70));

        eqDisplay.setBounds(r);
    }

private:
    MainComponent& owner;
    EqDisplayComponent eqDisplay;
    juce::ComboBox trackSelector;
    juce::TextButton addBandButton;
    juce::ToggleButton enableButton;
    juce::TextButton resetButton;
    int currentTrack = 0;
    bool updating = false;

    bool validTrack() const {
        return currentTrack >= 0 && currentTrack < (int)owner.project.tracks.size();
    }
};

GraphicEqWindow::GraphicEqWindow(MainComponent& m)
    : DocumentWindow("FASE GROOVE - PRO PARAMETRIC EQ", juce::Colour(0xff0e131b), juce::DocumentWindow::closeButton),
      owner(m) {
    setLookAndFeel(&m.theme);
    setUsingNativeTitleBar(true);
    panel = new Panel(m);
    setContentOwned(panel, true);
    setResizable(true, false);
    setResizeLimits(760, 460, 2560, 1440);
    setSize(960, 580);
    setAlwaysOnTop(true);
}

GraphicEqWindow::~GraphicEqWindow() {
    clearContentComponent();
    setLookAndFeel(nullptr);
}

void GraphicEqWindow::open(int track) {
    panel->setTrack(track);
    if (!isVisible()) centreWithSize(getWidth(), getHeight());
    setAlwaysOnTop(true);
    setVisible(true);
    toFront(true);
}

void GraphicEqWindow::refresh() {
    if (panel) panel->refresh();
}

void GraphicEqWindow::closeButtonPressed() {
    owner.dismissAuxiliaryWindow(*this);
}

} // namespace fga