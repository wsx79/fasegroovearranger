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
#include <juce_dsp/juce_dsp.h>

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
    juce::Colour(0xff7aa2f7), // slate blue
    juce::Colour(0xff7dcfff), // cool cyan
    juce::Colour(0xff9ece6a), // muted green
    juce::Colour(0xffe0af68), // sand
    juce::Colour(0xffbb9af7), // soft violet
    juce::Colour(0xff73daca), // teal
    juce::Colour(0xfff7768e), // dusty rose
    juce::Colour(0xffa9b1d6)  // steel
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

class EqDisplayComponent : public juce::Component, private juce::Timer {
public:
    std::function<void()> onSettingsChanged;
    std::function<void()> onDragStart;
    std::function<void()> onDragEnd;
    std::function<void(std::vector<float>&, std::vector<float>&)> getSpectrum;

    EqDisplayComponent() {
        setOpaque(true);
        setWantsKeyboardFocus(true);
        addChildComponent(editor);
        editor.setVisible(false);
        editor.setFont(Daw::font(Daw::body, true));
        editor.setJustification(juce::Justification::centred);
        editor.setColour(juce::TextEditor::backgroundColourId, Daw::well);
        editor.setColour(juce::TextEditor::textColourId, Daw::text);
        editor.setColour(juce::TextEditor::outlineColourId, Daw::lineStrong);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, Daw::accent);
        editor.setColour(juce::TextEditor::highlightColourId, Daw::accent.withAlpha(0.3f));
        editor.onReturnKey = [this] { commitEdit(); };
        editor.onFocusLost = [this] { commitEdit(); };
        editor.onEscapeKey = [this] { cancelEdit(); };
        startTimerHz(30);
    }

    ~EqDisplayComponent() override { stopTimer(); }

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

    void timerCallback() override {
        if (!getSpectrum) return;
        std::vector<float> pre, post;
        getSpectrum(pre, post);
        analyseSpectrum(pre, preBands);
        analyseSpectrum(post, postBands);
        repaint();
    }

    void analyseSpectrum(const std::vector<float>& samples, std::array<float, 96>& target) {
        if (samples.size() < 2048) return;
        constexpr int n = 2048;
        for (int i = 0; i < n; ++i)
            fftData[(size_t)i] = samples[samples.size() - (size_t)n + (size_t)i]
                * (0.5f - 0.5f * std::cos(2.0 * 3.141592653589793 * (double)i / (double)(n - 1)));
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        std::array<float, 96> bins{};
        bins.fill(-120.0f);
        for (int i = 1; i < n / 2; ++i) {
            const double hz = (double)i * 48000.0 / (double)n;
            if (hz < 20.0 || hz > 20000.0) continue;
            const double t = std::log(hz / 20.0) / std::log(1000.0);
            const int b = std::clamp((int)std::lround(t * 95.0), 0, 95);
            const float db = 20.0f * std::log10(std::max(1.0e-6f, fftData[(size_t)i] / (float)n));
            bins[(size_t)b] = std::max(bins[(size_t)b], db);
        }
        for (int b = 0; b < 96; ++b) {
            const float v = std::clamp((bins[(size_t)b] + 90.0f) / 90.0f, 0.0f, 1.0f);
            target[(size_t)b] = v > target[(size_t)b]
                ? target[(size_t)b] + (v - target[(size_t)b]) * 0.7f
                : target[(size_t)b] * 0.86f;
        }
    }

    void beginEdit(int param) {
        if (selectedBand < 0 || selectedBand >= (int)currentEq.bands.size()) return;
        const auto& b = currentEq.bands[(size_t)selectedBand];
        editParam = param;
        const juce::String text = param == 0 ? juce::String(b.freq, b.freq < 100.0 ? 1 : 0)
                                : param == 1 ? juce::String(b.gain, 1)
                                             : juce::String(b.q, 2);
        editor.setText(text, false);
        const auto l = hudLayout(getHudBounds(getPlotArea()));
        editor.setBounds((param == 0 ? l.freq : param == 1 ? l.gain : l.q).toNearestInt().reduced(3));
        editor.setVisible(true);
        editor.grabKeyboardFocus();
        editor.selectAll();
    }

    void cancelEdit() {
        editParam = -1;
        editor.setVisible(false);
        repaint();
    }

    void commitEdit() {
        if (editParam < 0) return;
        auto text = editor.getText().trim().toLowerCase();
        if (text.isEmpty()) { cancelEdit(); return; }
        double value = 0.0;
        if (editParam == 0) {
            value = text.endsWithChar('k') ? text.dropLastCharacters(1).getDoubleValue() * 1000.0 : text.getDoubleValue();
            value = std::clamp(value, 20.0, 20000.0);
        } else if (editParam == 1) {
            value = std::clamp(text.getDoubleValue(), -24.0, 24.0);
        } else {
            value = std::clamp(text.getDoubleValue(), 0.1, 18.0);
        }
        auto& b = currentEq.bands[(size_t)selectedBand];
        if (onDragStart) onDragStart();
        if (editParam == 0) b.freq = value;
        else if (editParam == 1) b.gain = value;
        else b.q = value;
        b.normalise();
        currentEq.syncLegacyFromBands();
        editParam = -1;
        editor.setVisible(false);
        if (onSettingsChanged) onSettingsChanged();
        if (onDragEnd) onDragEnd();
        repaint();
    }

    void resized() override {
        if (editor.isVisible()) cancelEdit();
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoverBand != -1 || hoverPill != -1) {
            hoverBand = hoverPill = -1;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(Daw::base);
        const auto plotArea = getPlotArea();

        g.setColour(Daw::lcd);
        g.fillRoundedRectangle(plotArea, 4.0f);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(plotArea, 4.0f, 1.0f);

        {
            const juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(plotArea.toNearestInt());

            static constexpr double dbTicks[] = { 24.0, 18.0, 12.0, 6.0, 0.0, -6.0, -12.0, -18.0, -24.0 };
            for (double db : dbTicks) {
                const float y = yFromDb(db, plotArea);
                const bool zero = std::abs(db) < 1e-4;
                g.setColour(zero ? Daw::lineStrong : Daw::line.withMultipliedAlpha(0.7f));
                g.drawHorizontalLine(juce::roundToInt(y), plotArea.getX(), plotArea.getRight());
            }

            static constexpr double majorFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
            static constexpr double minorFreqs[] = { 30, 40, 60, 70, 80, 90, 300, 400, 600, 700, 800, 900, 3000, 4000, 6000, 7000, 8000, 9000, 15000 };
            g.setColour(Daw::line.withMultipliedAlpha(0.45f));
            for (double hz : minorFreqs)
                g.drawVerticalLine(juce::roundToInt(xFromFreq(hz, plotArea)), plotArea.getY(), plotArea.getBottom());
            g.setColour(Daw::line.withMultipliedAlpha(0.9f));
            for (double hz : majorFreqs)
                g.drawVerticalLine(juce::roundToInt(xFromFreq(hz, plotArea)), plotArea.getY(), plotArea.getBottom());
        }

        g.setFont(Daw::smallFont(9.5f));
        static constexpr double dbLabels[] = { 24.0, 12.0, 0.0, -12.0, -24.0 };
        for (double db : dbLabels) {
            const float y = yFromDb(db, plotArea);
            g.setColour(std::abs(db) < 1e-4 ? Daw::textDim : Daw::textFaint);
            const juce::String t = (db > 0 ? "+" : "") + juce::String((int)db);
            g.drawText(t, 0, juce::roundToInt(y - 7.0f), juce::roundToInt(plotArea.getX()) - 8, 14, juce::Justification::centredRight);
        }

        static constexpr struct { double hz; const char* label; } freqLabels[] = {
            { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" },
            { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" }
        };
        for (const auto& f : freqLabels) {
            const float x = xFromFreq(f.hz, plotArea);
            g.setColour(Daw::textFaint);
            g.drawText(f.label, juce::roundToInt(x - 18.0f), juce::roundToInt(plotArea.getBottom() + 6.0f), 36, 14, juce::Justification::centred);
        }

        if (name.isNotEmpty()) {
            g.setColour(Daw::textFaint);
            g.setFont(Daw::caps(9.5f));
            g.drawText(name.toUpperCase(), juce::roundToInt(plotArea.getX() + 8.0f), juce::roundToInt(plotArea.getY() + 6.0f),
                       juce::roundToInt(plotArea.getWidth() - 16.0f), 14, juce::Justification::centredLeft);
        }

        {
            const juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(plotArea.toNearestInt());
            auto drawSpectrum = [&](const std::array<float, 96>& values, juce::Colour fill, juce::Colour line) {
                juce::Path path;
                path.startNewSubPath(plotArea.getX(), plotArea.getBottom());
                for (int i = 0; i < 96; ++i) {
                    const float x = plotArea.getX() + plotArea.getWidth() * (float)i / 95.0f;
                    const float y = plotArea.getBottom() - values[(size_t)i] * plotArea.getHeight() * 0.92f;
                    path.lineTo(x, y);
                }
                path.lineTo(plotArea.getRight(), plotArea.getBottom());
                path.closeSubPath();
                g.setColour(fill);
                g.fillPath(path);
                g.setColour(line);
                g.strokePath(path, juce::PathStrokeType(1.0f));
            };
            drawSpectrum(preBands, Daw::textFaint.withAlpha(0.08f), Daw::textFaint.withAlpha(0.22f));
            if (currentEq.enabled)
                drawSpectrum(postBands, juce::Colour(0xff4cc3f0).withAlpha(0.10f), juce::Colour(0xff4cc3f0).withAlpha(0.45f));
        }

        std::vector<BiquadCoeffs> bandCoeffs;
        bandCoeffs.reserve(currentEq.bands.size());
        for (const auto& b : currentEq.bands) bandCoeffs.push_back(computeBandCoeffs(b, 48000.0));

        const int steps = std::max(2, (int)plotArea.getWidth());
        const float yZero = yFromDb(0.0, plotArea);

        {
            const juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(plotArea.toNearestInt());

            if (currentEq.enabled && (selectedBand >= 0 || hoverBand >= 0)) {
                const int which = selectedBand >= 0 ? selectedBand : hoverBand;
                if (which >= 0 && which < (int)currentEq.bands.size() && currentEq.bands[(size_t)which].enabled) {
                    juce::Path bandPath;
                    for (int i = 0; i < steps; ++i) {
                        const float x = plotArea.getX() + (float)i * (plotArea.getWidth() / (float)(steps - 1));
                        const double hz = freqFromX(x, plotArea);
                        const float y = yFromDb(std::clamp(bandCoeffs[(size_t)which].magnitudeDb(hz, 48000.0), -26.0, 26.0), plotArea);
                        if (i == 0) bandPath.startNewSubPath(x, y);
                        else bandPath.lineTo(x, y);
                    }
                    g.setColour(getBandColour((size_t)which).withAlpha(0.55f));
                    g.strokePath(bandPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved));
                }
            }

            juce::Path curvePath, fillPath;
            for (int i = 0; i < steps; ++i) {
                const float x = plotArea.getX() + (float)i * (plotArea.getWidth() / (float)(steps - 1));
                const double hz = freqFromX(x, plotArea);
                double totalDb = 0.0;
                if (currentEq.enabled)
                    for (const auto& coeff : bandCoeffs) totalDb += coeff.magnitudeDb(hz, 48000.0);
                const float y = yFromDb(std::clamp(totalDb, -26.0, 26.0), plotArea);
                if (i == 0) {
                    curvePath.startNewSubPath(x, y);
                    fillPath.startNewSubPath(x, yZero);
                } else {
                    curvePath.lineTo(x, y);
                }
                fillPath.lineTo(x, y);
            }
            fillPath.lineTo(plotArea.getRight(), yZero);
            fillPath.closeSubPath();

            const auto curveColour = currentEq.enabled ? Daw::accent : Daw::textFaint;
            g.setColour(curveColour.withMultipliedAlpha(0.12f));
            g.fillPath(fillPath);
            g.setColour(curveColour.withAlpha(currentEq.enabled ? 0.95f : 0.6f));
            g.strokePath(curvePath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        for (size_t i = 0; i < currentEq.bands.size(); ++i) {
            const auto& b = currentEq.bands[i];
            const auto pt = getNodePosition((int)i, plotArea);
            const bool isHovered = hoverBand == (int)i;
            const bool isSelected = selectedBand == (int)i;
            const bool isDragging = dragBand == (int)i;
            const auto col = getBandColour(i);

            if (isDragging || isSelected || isHovered) {
                g.setColour(Daw::lineStrong.withAlpha(isDragging ? 0.8f : 0.5f));
                g.drawVerticalLine(juce::roundToInt(pt.x), plotArea.getY(), plotArea.getBottom());
                g.drawHorizontalLine(juce::roundToInt(pt.y), plotArea.getX(), plotArea.getRight());
            }

            const float radius = isSelected ? 9.0f : 8.0f;
            g.setColour(Daw::base);
            g.fillEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(b.enabled ? col : col.withAlpha(0.35f));
            g.drawEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f, isSelected ? 2.0f : 1.5f);
            g.setFont(Daw::font(9.0f, true));
            g.setColour(b.enabled ? (isSelected ? Daw::text : col) : Daw::textFaint);
            g.drawText(juce::String((int)i + 1), juce::roundToInt(pt.x - radius), juce::roundToInt(pt.y - radius),
                       juce::roundToInt(radius * 2.0f), juce::roundToInt(radius * 2.0f), juce::Justification::centred);
        }

        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) drawBandHud(g, plotArea);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        const int found = findBandAt(e.position, plotArea);
        int pill = -1;
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            const auto l = hudLayout(getHudBounds(plotArea));
            for (int i = 0; i < 6; ++i)
                if (l.pills[i].contains(e.position)) { pill = i; break; }
        }
        if (hoverBand != found || hoverPill != pill) {
            hoverBand = found;
            hoverPill = pill;
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
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            const auto l = hudLayout(getHudBounds(getPlotArea()));
            if (l.freq.contains(e.position)) { beginEdit(0); return; }
            if (l.gain.contains(e.position)) { beginEdit(1); return; }
            if (l.q.contains(e.position)) { beginEdit(2); return; }
        }
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
    int hoverPill = -1;
    int dragBand = -1;
    int selectedBand = -1;

    // HUD parameter dragging
    int dragHudParam = -1; // 0: freq, 1: gain, 2: q
    double dragStartParamVal = 0.0;
    juce::dsp::FFT fft{11};
    std::array<float, 4096> fftData{};
    std::array<float, 96> preBands{}, postBands{};
    juce::TextEditor editor;
    int editParam = -1;

    juce::Rectangle<float> getPlotArea() const {
        const float padL = 44.0f, padR = 16.0f, padT = 18.0f, padB = 34.0f;
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

    struct HudLayout {
        juce::Rectangle<float> badge, pills[6], freq, gain, q, bypass, invert, del;
    };

    HudLayout hudLayout(const juce::Rectangle<float>& hud) const {
        HudLayout l;
        auto r = hud.reduced(6.0f, 6.0f);
        l.badge = r.removeFromLeft(52.0f).reduced(1.0f);
        r.removeFromLeft(6.0f);
        for (auto& pill : l.pills) pill = r.removeFromLeft(38.0f).reduced(1.0f, 4.0f);
        r.removeFromLeft(8.0f);
        l.freq = r.removeFromLeft(78.0f).reduced(2.0f);
        r.removeFromLeft(6.0f);
        l.gain = r.removeFromLeft(66.0f).reduced(2.0f);
        r.removeFromLeft(6.0f);
        l.q = r.removeFromLeft(56.0f).reduced(2.0f);
        r.removeFromLeft(8.0f);
        l.bypass = r.removeFromLeft(30.0f).reduced(2.0f, 4.0f);
        r.removeFromLeft(4.0f);
        l.invert = r.removeFromLeft(30.0f).reduced(2.0f, 4.0f);
        r.removeFromLeft(4.0f);
        l.del = r.removeFromLeft(30.0f).reduced(2.0f, 4.0f);
        return l;
    }

    juce::Rectangle<float> getHudBounds(const juce::Rectangle<float>& plotArea) const {
        const float hudW = std::min(624.0f, plotArea.getWidth() - 20.0f);
        const float hudH = 50.0f;
        const float hudX = plotArea.getX() + (plotArea.getWidth() - hudW) * 0.5f;
        const float hudY = plotArea.getBottom() - hudH - 8.0f;
        return { hudX, hudY, hudW, hudH };
    }

    static void drawFilterIcon(juce::Graphics& g, juce::Rectangle<float> r, int type, juce::Colour colour) {
        const float x0 = r.getX(), x1 = r.getRight(), yTop = r.getY(), yBottom = r.getBottom();
        const float mid = r.getCentreY();
        juce::Path p;
        auto pt = [&](float x, float y) { return juce::Point<float>(x0 + (x1 - x0) * x, y); };
        switch (type) {
            case (int)EqFilterType::LowCut:
                p.startNewSubPath(pt(0.0f, yBottom)); p.lineTo(pt(0.45f, yBottom)); p.lineTo(pt(0.75f, yTop)); p.lineTo(pt(1.0f, yTop)); break;
            case (int)EqFilterType::LowShelf:
                p.startNewSubPath(pt(0.0f, yBottom)); p.lineTo(pt(0.38f, yBottom)); p.lineTo(pt(0.62f, yTop)); p.lineTo(pt(1.0f, yTop)); break;
            case (int)EqFilterType::HighShelf:
                p.startNewSubPath(pt(0.0f, yTop)); p.lineTo(pt(0.38f, yTop)); p.lineTo(pt(0.62f, yBottom)); p.lineTo(pt(1.0f, yBottom)); break;
            case (int)EqFilterType::HighCut:
                p.startNewSubPath(pt(0.0f, yTop)); p.lineTo(pt(0.25f, yTop)); p.lineTo(pt(0.55f, yBottom)); p.lineTo(pt(1.0f, yBottom)); break;
            case (int)EqFilterType::Notch:
                p.startNewSubPath(pt(0.0f, yTop)); p.lineTo(pt(0.4f, yTop)); p.lineTo(pt(0.5f, yBottom)); p.lineTo(pt(0.6f, yTop)); p.lineTo(pt(1.0f, yTop)); break;
            default:
                p.startNewSubPath(pt(0.0f, mid)); p.quadraticTo(pt(0.5f, yTop - 1.0f), pt(1.0f, mid)); break;
        }
        g.setColour(colour);
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawBandHud(juce::Graphics& g, const juce::Rectangle<float>& plotArea) {
        auto hud = getHudBounds(plotArea);
        auto l = hudLayout(hud);
        const auto& b = currentEq.bands[(size_t)selectedBand];

        g.setColour(Daw::chrome);
        g.fillRoundedRectangle(hud, Daw::panelRadius);
        g.setColour(Daw::lineStrong);
        g.drawRoundedRectangle(hud, Daw::panelRadius, 1.0f);

        g.setFont(Daw::caps(9.0f));
        g.setColour(getBandColour(selectedBand));
        g.drawText("BAND " + juce::String(selectedBand + 1), l.badge, juce::Justification::centred);

        for (int i = 0; i < 6; ++i) {
            const bool active = (b.type == i);
            g.setColour(active ? Daw::accent : Daw::control);
            g.fillRoundedRectangle(l.pills[i], Daw::radius);
            drawFilterIcon(g, l.pills[i].reduced(7.0f, 5.0f), i, active ? Daw::onAccent : Daw::textDim);
        }

        auto valueBox = [&](juce::Rectangle<float> box, const char* label, const juce::String& value, juce::Colour colour) {
            g.setColour(Daw::well);
            g.fillRoundedRectangle(box, Daw::radius);
            auto inner = box.reduced(4.0f, 2.0f);
            g.setFont(Daw::caps(7.5f));
            g.setColour(Daw::textFaint);
            g.drawText(label, inner.removeFromTop(11.0f), juce::Justification::centredLeft);
            g.setFont(Daw::font(Daw::body, true));
            g.setColour(colour);
            g.drawText(value, inner, juce::Justification::centredLeft);
        };

        const bool gainDisabled = (b.type == (int)EqFilterType::LowCut || b.type == (int)EqFilterType::HighCut || b.type == (int)EqFilterType::Notch);
        valueBox(l.freq, "FREQ", (b.freq >= 1000.0) ? juce::String(b.freq / 1000.0, 2) + " kHz" : juce::String((int)b.freq) + " Hz", Daw::text);
        valueBox(l.gain, "GAIN", gainDisabled ? juce::String("—") : (b.gain > 0 ? "+" : "") + juce::String(b.gain, 1) + " dB", gainDisabled ? Daw::textFaint : Daw::text);
        valueBox(l.q, "Q", juce::String(b.q, 2), Daw::text);

        auto chip = [&](juce::Rectangle<float> box, const juce::String& text, bool active, juce::Colour activeColour) {
            g.setColour(active ? activeColour.withAlpha(0.22f) : Daw::control);
            g.fillRoundedRectangle(box, Daw::radius);
            g.setColour(active ? activeColour : Daw::textDim);
            g.setFont(Daw::caps(8.0f));
            g.drawText(text, box, juce::Justification::centred);
        };

        chip(l.bypass, b.enabled ? "ON" : "OFF", b.enabled, Daw::accent);
        chip(l.invert, "INV", false, Daw::textDim);
        chip(l.del, "DEL", true, Daw::meterHigh);

        if (hoverPill >= 0 && hoverPill < 6) {
            static const char* tips[] = {
                "Bell: boost or cut around the frequency",
                "Low cut: removes frequencies below the cutoff",
                "Low shelf: boosts or cuts the low end",
                "High shelf: boosts or cuts the high end",
                "High cut: removes frequencies above the cutoff",
                "Notch: removes a narrow band of frequencies"
            };
            const auto& pill = l.pills[hoverPill];
            const juce::String tip(tips[hoverPill]);
            const int tipW = 320, tipH = 26;
            juce::Rectangle<int> box((int)pill.getCentreX() - tipW / 2, (int)pill.getY() - tipH - 8, tipW, tipH);
            if (box.getY() < 4) box.setY((int)pill.getBottom() + 8);
            g.setColour(Daw::chrome);
            g.fillRoundedRectangle(box.toFloat(), Daw::radius);
            g.setColour(Daw::lineStrong);
            g.drawRoundedRectangle(box.toFloat(), Daw::radius, 1.0f);
            g.setFont(Daw::font(Daw::small));
            g.setColour(Daw::text);
            g.drawFittedText(tip, box.reduced(8, 0), 1, 0.8f);
        }
    }

    void handleHudMouseDown(const juce::MouseEvent& e, const juce::Rectangle<float>& hud) {
        auto l = hudLayout(hud);
        auto& b = currentEq.bands[(size_t)selectedBand];

        for (int i = 0; i < 6; ++i) {
            if (l.pills[i].contains(e.position)) {
                b.type = i;
                b.normalise();
                currentEq.syncLegacyFromBands();
                if (onSettingsChanged) onSettingsChanged();
                repaint();
                return;
            }
        }

        if (l.freq.contains(e.position)) {
            dragHudParam = 0; dragStartParamVal = b.freq;
            if (onDragStart) onDragStart();
            return;
        }
        if (l.gain.contains(e.position)) {
            dragHudParam = 1; dragStartParamVal = b.gain;
            if (onDragStart) onDragStart();
            return;
        }
        if (l.q.contains(e.position)) {
            dragHudParam = 2; dragStartParamVal = b.q;
            if (onDragStart) onDragStart();
            return;
        }
        if (l.bypass.contains(e.position)) {
            b.enabled = !b.enabled;
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
            return;
        }
        if (l.invert.contains(e.position)) {
            b.gain = -b.gain;
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
            return;
        }
        if (l.del.contains(e.position)) {
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

        eqDisplay.getSpectrum = [this](std::vector<float>& pre, std::vector<float>& post) {
            owner.audio.readSpectrum(currentTrack, pre, post, 2048);
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
        owner.audio.setSpectrumTrack(currentTrack);
        updating = false;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(Daw::base);

        g.setColour(Daw::accent);
        g.fillRoundedRectangle(16.0f, 14.0f, 3.0f, 16.0f, 1.5f);

        g.setColour(Daw::text);
        g.setFont(Daw::font(Daw::title, true));
        g.drawText(tr("GRAPHIC EQ"), 28, 8, 240, 26, juce::Justification::centredLeft);

        g.setFont(Daw::font(Daw::small, false));
        g.setColour(Daw::textFaint);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(14);
        auto topBar = r.removeFromTop(34);
        r.removeFromTop(10);

        topBar.removeFromLeft(180); // Title spacing
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
    : DocumentWindow("Graphic Equalizer", Daw::base, juce::DocumentWindow::closeButton),
      owner(m) {
    setLookAndFeel(&m.theme);
    setUsingNativeTitleBar(true);
    panel = new Panel(m);
    setContentOwned(panel, false);
    setResizable(true, false);
    // Fit the editor inside the display's usable area (DPI and small screens included).
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = display ? display->userArea : juce::Rectangle<int>(0, 0, 1280, 800);
    const int maxW = juce::jmax(760, area.getWidth() - 32);
    const int maxH = juce::jmax(460, area.getHeight() - 48);
    setResizeLimits(760, 460, maxW, maxH);
    setSize(juce::jmin(900, area.getWidth() - 80), juce::jmin(560, area.getHeight() - 100));
    setAlwaysOnTop(true);
}

GraphicEqWindow::~GraphicEqWindow() {
    clearContentComponent();
    setLookAndFeel(nullptr);
}

void GraphicEqWindow::open(int track) {
    panel->setTrack(track);
    if (!isVisible()) {
        // Open at a safe offset; centring a scaled top-level window can push it off-screen.
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto area = display ? display->userArea : juce::Rectangle<int>(0, 0, 1280, 800);
        setTopLeftPosition(area.getX() + 40, area.getY() + 40);
    }
    setAlwaysOnTop(true);
    setVisible(true);
    toFront(true);
}

void GraphicEqWindow::refresh() {
    if (panel) panel->refresh();
}

void GraphicEqWindow::resized() {
    DocumentWindow::resized();
    if (panel) panel->setBounds(getLocalBounds());
}

void GraphicEqWindow::closeButtonPressed() {
    owner.audio.setSpectrumTrack(-1);
    owner.dismissAuxiliaryWindow(*this);
}

} // namespace fga
