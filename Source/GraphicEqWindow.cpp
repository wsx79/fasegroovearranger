// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GraphicEqWindow.h"
#include "MainComponent.h"
#include "Localisation.h"
#include "StudioTheme.h"
#include <cmath>

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

static void computeFilters(const EqSettings& eq, double rate, std::array<BiquadCoeffs, 4>& filters) {
    rate = std::max(8000.0, rate);
    for (int i = 0; i < 4; ++i) {
        const double hz = std::clamp(i == 0 ? eq.highPass : i == 1 ? eq.lowFreq : i == 2 ? eq.midFreq : eq.highFreq, 20.0, rate * 0.45);
        const double qVal = i == 2 ? std::clamp(eq.midQ, 0.1, 10.0) : 0.70710678118;
        const double w = 6.283185307179586 * hz / rate;
        const double c = std::cos(w), s = std::sin(w), alpha = s / (2.0 * qVal);
        auto& f = filters[(size_t)i];
        if (i == 0) {
            f.setCoeffs((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            continue;
        }
        const double db = std::clamp(i == 1 ? eq.low : i == 2 ? eq.mid : eq.high, -18.0, 18.0);
        const double A = std::pow(10.0, db / 40.0);
        const double beta = 2.0 * std::sqrt(A) * alpha;
        if (i == 2) {
            f.setCoeffs(1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
        } else if (i == 1) {
            f.setCoeffs(A * ((A + 1.0) - (A - 1.0) * c + beta),
                        2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                        A * ((A + 1.0) - (A - 1.0) * c - beta),
                        (A + 1.0) + (A - 1.0) * c + beta,
                        -2.0 * ((A - 1.0) + (A + 1.0) * c),
                        (A + 1.0) + (A - 1.0) * c - beta);
        } else {
            f.setCoeffs(A * ((A + 1.0) + (A - 1.0) * c + beta),
                        -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                        A * ((A + 1.0) + (A - 1.0) * c - beta),
                        (A + 1.0) - (A - 1.0) * c + beta,
                        2.0 * ((A - 1.0) - (A + 1.0) * c),
                        (A + 1.0) - (A - 1.0) * c - beta);
        }
    }
}

class EqDisplayComponent : public juce::Component {
public:
    std::function<void()> onSettingsChanged;
    std::function<void()> onDragStart;
    std::function<void()> onDragEnd;

    EqDisplayComponent() {
        setOpaque(true);
    }

    void setEq(const EqSettings& settings, int trackIndex, const juce::String& trackName) {
        currentEq = settings;
        track = trackIndex;
        name = trackName;
        repaint();
    }

    const EqSettings& getEq() const { return currentEq; }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff11161d)); // FabFilter dark background

        const float padL = 36.0f, padR = 24.0f, padT = 24.0f, padB = 30.0f;
        const auto plotArea = juce::Rectangle<float>(padL, padT, bounds.getWidth() - padL - padR, bounds.getHeight() - padT - padB);

        // Draw grid
        g.setColour(juce::Colour(0xff1f2836));
        g.drawRect(plotArea, 1.0f);

        // dB horizontal grid lines
        static constexpr double dbTicks[] = { 18.0, 12.0, 6.0, 0.0, -6.0, -12.0, -18.0 };
        g.setFont(Daw::font(Daw::small, false));
        for (double db : dbTicks) {
            const float y = yFromDb(db, plotArea);
            const bool isZero = std::abs(db) < 1e-4;
            g.setColour(isZero ? juce::Colour(0xff3f4f66) : juce::Colour(0xff1a2330));
            g.drawHorizontalLine(std::lround(y), plotArea.getX(), plotArea.getRight());
            g.setColour(isZero ? Daw::text : Daw::textDim);
            g.drawText((db > 0 ? "+" : "") + juce::String((int)db) + " dB", 0, std::lround(y - 7.0f), (int)padL - 6, 14, juce::Justification::centredRight);
        }

        // Frequency vertical grid lines
        static constexpr double freqTicks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (double hz : freqTicks) {
            const float x = xFromFreq(hz, plotArea);
            g.setColour(juce::Colour(0xff1a2330));
            g.drawVerticalLine(std::lround(x), plotArea.getY(), plotArea.getBottom());
            g.setColour(Daw::textDim);
            juce::String label = hz >= 1000 ? juce::String((int)(hz / 1000)) + "k" : juce::String((int)hz);
            g.drawText(label, std::lround(x - 20.0f), std::lround(plotArea.getBottom() + 6.0f), 40, 16, juce::Justification::centred);
        }

        std::array<BiquadCoeffs, 4> filters;
        computeFilters(currentEq, 48000.0, filters);

        // Draw frequency response curve
        const int steps = (int)plotArea.getWidth();
        if (steps > 10) {
            juce::Path curvePath;
            juce::Path fillPath;
            const float yZero = yFromDb(0.0, plotArea);
            fillPath.startNewSubPath(plotArea.getX(), yZero);

            for (int i = 0; i < steps; ++i) {
                const float x = plotArea.getX() + (float)i;
                const double hz = freqFromX(x, plotArea);
                double totalDb = 0.0;
                if (currentEq.enabled) {
                    for (int f = 0; f < 4; ++f) {
                        totalDb += filters[(size_t)f].magnitudeDb(hz, 48000.0);
                    }
                }
                const float y = yFromDb(std::clamp(totalDb, -24.0, 24.0), plotArea);
                if (i == 0) {
                    curvePath.startNewSubPath(x, y);
                    fillPath.lineTo(x, y);
                } else {
                    curvePath.lineTo(x, y);
                    fillPath.lineTo(x, y);
                }
            }
            fillPath.lineTo(plotArea.getRight(), yZero);
            fillPath.closeSubPath();

            // Gradient fill under the curve (Pro-Q neon turquoise/cyan glow)
            juce::Colour neonCyan(0xff00e5ff);
            juce::Colour glowColour = neonCyan.withAlpha(currentEq.enabled ? 0.22f : 0.05f);
            juce::Colour baseColour = neonCyan.withAlpha(0.0f);
            juce::ColourGradient fillGrad(glowColour, 0.0f, plotArea.getY() + 10.0f, baseColour, 0.0f, plotArea.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);

            // Curve stroke
            g.setColour(currentEq.enabled ? neonCyan : Daw::textDim);
            g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw nodes
        static const juce::Colour nodeColours[] = {
            juce::Colour(0xffff9900), // HP (Amber)
            juce::Colour(0xff4488ff), // Low Shelf (Blue)
            juce::Colour(0xff00e5ff), // Mid Bell (Cyan)
            juce::Colour(0xffff44aa)  // High Shelf (Magenta)
        };
        static const char* nodeNames[] = { "HP", "LOW", "MID", "HIGH" };

        for (int i = 0; i < 4; ++i) {
            const auto pt = getNodePosition(i, plotArea);
            const bool hovered = (hoverNode == i);
            const bool active = (dragNode == i);
            const auto col = nodeColours[(size_t)i];

            // Outer glow
            if (hovered || active) {
                g.setColour(col.withAlpha(0.35f));
                g.fillEllipse(pt.x - 16.0f, pt.y - 16.0f, 32.0f, 32.0f);
            }

            // Node core
            g.setColour(juce::Colour(0xff151b24));
            g.fillEllipse(pt.x - 10.0f, pt.y - 10.0f, 20.0f, 20.0f);
            g.setColour(currentEq.enabled ? col : col.withAlpha(0.4f));
            g.drawEllipse(pt.x - 10.0f, pt.y - 10.0f, 20.0f, 20.0f, 2.0f);

            // Inner label
            g.setFont(Daw::font(Daw::small, true));
            g.setColour(currentEq.enabled ? Daw::text : Daw::textDim);
            g.drawText(nodeNames[i], std::lround(pt.x - 14.0f), std::lround(pt.y - 8.0f), 28, 16, juce::Justification::centred);
        }

        // Floating info badge on drag/hover
        const int infoNode = dragNode >= 0 ? dragNode : hoverNode;
        if (infoNode >= 0) {
            const auto pt = getNodePosition(infoNode, plotArea);
            juce::String tipText;
            if (infoNode == 0) {
                tipText = "HP: " + juce::String((int)currentEq.highPass) + " Hz";
            } else if (infoNode == 1) {
                tipText = juce::String("Low: ") + (currentEq.low > 0 ? "+" : "") + juce::String(currentEq.low, 1) + " dB @ " + juce::String((int)currentEq.lowFreq) + " Hz";
            } else if (infoNode == 2) {
                tipText = juce::String("Mid: ") + (currentEq.mid > 0 ? "+" : "") + juce::String(currentEq.mid, 1) + " dB @ " + juce::String((int)currentEq.midFreq) + " Hz (Q=" + juce::String(currentEq.midQ, 2) + ")";
            } else {
                tipText = juce::String("High: ") + (currentEq.high > 0 ? "+" : "") + juce::String(currentEq.high, 1) + " dB @ " + juce::String((int)currentEq.highFreq) + " Hz";
            }

            const int badgeW = 200, badgeH = 26;
            int badgeX = std::clamp((int)pt.x - badgeW / 2, (int)plotArea.getX() + 4, (int)plotArea.getRight() - badgeW - 4);
            int badgeY = pt.y > plotArea.getY() + 45.0f ? (int)pt.y - 40 : (int)pt.y + 18;

            g.setColour(juce::Colour(0xee0e131a));
            g.fillRoundedRectangle((float)badgeX, (float)badgeY, (float)badgeW, (float)badgeH, 4.0f);
            g.setColour(nodeColours[(size_t)infoNode]);
            g.drawRoundedRectangle((float)badgeX, (float)badgeY, (float)badgeW, (float)badgeH, 4.0f, 1.0f);
            g.setColour(Daw::text);
            g.setFont(Daw::font(Daw::body, true));
            g.drawText(tipText, badgeX, badgeY, badgeW, badgeH, juce::Justification::centred);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        int found = -1;
        for (int i = 0; i < 4; ++i) {
            const auto pt = getNodePosition(i, plotArea);
            if (e.position.getDistanceFrom(pt) <= 15.0f) {
                found = i;
                break;
            }
        }
        if (hoverNode != found) {
            hoverNode = found;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        dragNode = -1;
        for (int i = 0; i < 4; ++i) {
            const auto pt = getNodePosition(i, plotArea);
            if (e.position.getDistanceFrom(pt) <= 16.0f) {
                dragNode = i;
                break;
            }
        }
        if (dragNode >= 0) {
            if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2) {
                // Reset node gain
                if (dragNode == 0) currentEq.highPass = 20.0;
                else if (dragNode == 1) { currentEq.low = 0.0; currentEq.lowFreq = 120.0; }
                else if (dragNode == 2) { currentEq.mid = 0.0; currentEq.midFreq = 1000.0; currentEq.midQ = 0.707; }
                else if (dragNode == 3) { currentEq.high = 0.0; currentEq.highFreq = 6000.0; }
                if (onSettingsChanged) onSettingsChanged();
                repaint();
                return;
            }
            if (onDragStart) onDragStart();
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragNode < 0) return;
        const auto plotArea = getPlotArea();
        const double hz = std::clamp(freqFromX(e.position.x, plotArea), 20.0, 20000.0);
        const double db = std::clamp(dbFromY(e.position.y, plotArea), -18.0, 18.0);

        if (dragNode == 0) {
            currentEq.highPass = std::clamp(hz, 20.0, 400.0);
        } else if (dragNode == 1) {
            currentEq.low = db;
            currentEq.lowFreq = std::clamp(hz, 30.0, 800.0);
        } else if (dragNode == 2) {
            currentEq.mid = db;
            currentEq.midFreq = std::clamp(hz, 200.0, 8000.0);
        } else if (dragNode == 3) {
            currentEq.high = db;
            currentEq.highFreq = std::clamp(hz, 1500.0, 18000.0);
        }
        currentEq.normalise();
        if (onSettingsChanged) onSettingsChanged();
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (dragNode >= 0) {
            dragNode = -1;
            if (onDragEnd) onDragEnd();
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        if (hoverNode == 2 || dragNode == 2) { // Mid band Q control
            const double delta = wheel.deltaY * 0.5;
            currentEq.midQ = std::clamp(currentEq.midQ + delta, 0.2, 8.0);
            if (onSettingsChanged) onSettingsChanged();
            repaint();
        }
    }

private:
    EqSettings currentEq;
    int track = 0;
    juce::String name;
    int hoverNode = -1;
    int dragNode = -1;

    juce::Rectangle<float> getPlotArea() const {
        const float padL = 36.0f, padR = 24.0f, padT = 24.0f, padB = 30.0f;
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
        const double minDb = -20.0, maxDb = 20.0;
        const double norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return r.getY() + (float)((1.0 - norm) * r.getHeight());
    }

    static double dbFromY(float y, const juce::Rectangle<float>& r) {
        const double minDb = -20.0, maxDb = 20.0;
        const double norm = 1.0 - std::clamp((double)(y - r.getY()) / (double)r.getHeight(), 0.0, 1.0);
        return minDb + norm * (maxDb - minDb);
    }

    juce::Point<float> getNodePosition(int index, const juce::Rectangle<float>& r) const {
        if (index == 0) {
            return { xFromFreq(currentEq.highPass, r), yFromDb(0.0, r) };
        } else if (index == 1) {
            return { xFromFreq(currentEq.lowFreq, r), yFromDb(currentEq.low, r) };
        } else if (index == 2) {
            return { xFromFreq(currentEq.midFreq, r), yFromDb(currentEq.mid, r) };
        } else {
            return { xFromFreq(currentEq.highFreq, r), yFromDb(currentEq.high, r) };
        }
    }
};

} // namespace

class GraphicEqWindow::Panel : public juce::Component {
public:
    explicit Panel(MainComponent& m) : owner(m) {
        addAndMakeVisible(eqDisplay);
        addAndMakeVisible(trackSelector);
        addAndMakeVisible(enableButton);
        addAndMakeVisible(resetButton);

        Localisation::bindTip(enableButton, "Toggle EQ on or off for this track.");
        Localisation::bindTip(resetButton, "Reset EQ bands to flat (0 dB).");

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
            q.highPass = 20.0;
            q.low = 0.0; q.mid = 0.0; q.high = 0.0;
            q.lowFreq = 120.0; q.midFreq = 1000.0; q.highFreq = 6000.0; q.midQ = 0.707;
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
        const auto& t = owner.project.tracks[(size_t)currentTrack];

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
        g.fillAll(StudioStyle::base);
        g.setColour(StudioStyle::text);
        g.setFont(StudioStyle::font(15, true));
        g.drawText(tr("GRAPHIC PARAMETRIC EQUALIZER"), 16, 12, 320, 24, juce::Justification::centredLeft);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(16);
        auto topBar = r.removeFromTop(32);
        r.removeFromTop(10);

        topBar.removeFromLeft(280); // Title spacing
        trackSelector.setBounds(topBar.removeFromLeft(180));
        topBar.removeFromLeft(12);
        enableButton.setBounds(topBar.removeFromLeft(130));
        topBar.removeFromLeft(12);
        resetButton.setBounds(topBar.removeFromLeft(80));

        eqDisplay.setBounds(r);
    }

private:
    MainComponent& owner;
    EqDisplayComponent eqDisplay;
    juce::ComboBox trackSelector;
    juce::ToggleButton enableButton;
    juce::TextButton resetButton;
    int currentTrack = 0;
    bool updating = false;

    bool validTrack() const {
        return currentTrack >= 0 && currentTrack < (int)owner.project.tracks.size();
    }
};

GraphicEqWindow::GraphicEqWindow(MainComponent& m)
    : DocumentWindow("FASE GROOVE - PARAMETRIC EQ", StudioStyle::base, juce::DocumentWindow::closeButton),
      owner(m) {
    setLookAndFeel(&m.theme);
    setUsingNativeTitleBar(true);
    panel = new Panel(m);
    setContentOwned(panel, true);
    setResizable(true, false);
    setResizeLimits(700, 420, 2560, 1440);
    setSize(860, 520);
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
