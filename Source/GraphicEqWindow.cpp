// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GraphicEqWindow.h"
#include "MainComponent.h"
#include "AudioEngine.h"
#include "core/EqResponse.h"
#include "core/EqEditing.h"
#include "EqAnalyzer.h"
#if FGA_SELF_TESTS
#include "ChannelStrip.h"
#endif
#include "Localisation.h"
#include "StudioTheme.h"
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>

namespace fga {
namespace {

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
    std::function<bool(std::vector<float>&, std::vector<float>&)> getSpectrum;
    std::function<double()> getSampleRate;
    std::function<void()> onSelectionChanged;
    std::function<void()> onWheelStart;

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

    void setEq(const EqSettings& settings, int trackIndex, const juce::String& trackName,std::uint64_t identity=0) {
        auto canonical=settings;canonical.ensureBands();
        const bool moved=track!=trackIndex||trackIdentity!=identity;
        if(moved||!(currentEq==canonical))++revision;
        if(moved){cancelEdit();selectedBand=-1;preBands.fill(0);postBands.fill(0);}
        currentEq = std::move(canonical);
        currentEq.ensureBands();
        track = trackIndex;trackIdentity=identity;
        name = trackName;
        if (selectedBand >= (int)currentEq.bands.size()) selectedBand = -1;
        repaint();
    }

    const EqSettings& getEq() const { return currentEq; }

    int getSelectedBand() const { return selectedBand; }
    void selectBand(int index){cancelEdit();selectedBand=index>=0&&index<(int)currentEq.bands.size()?index:-1;if(onSelectionChanged)onSelectionChanged();repaint();}
    void setView(int mode,int speed,double range){analyzerMode=mode;spectrumSpeed=speed;displayRange=range;repaint();}
    void editOnce(const std::function<void()>& action){const auto before=currentEq;action();currentEq.syncLegacyFromBands();if(currentEq==before){repaint();return;}if(onDragStart)onDragStart();if(onSettingsChanged)onSettingsChanged();if(onDragEnd)onDragEnd();repaint();}
    void flatten(){editOnce([this]{currentEq.flatten();});}


#if FGA_SELF_TESTS
    bool verifyInteractions(juce::StringArray& checks){
        bool ok=true;auto expect=[&](bool passed,const char* label){checks.add(juce::String(passed?"PASS ":"FAIL ")+label);ok=passed&&ok;};
        int undos=0;onDragStart=[&]{++undos;};onSettingsChanged=[]{};onDragEnd=[]{};
        EqSettings authored;authored.enabled=true;authored.bands={{true,0,150,3,1},{true,0,1500,-4,2},{true,1,35,0,.707}};authored.syncLegacyFromBands();setEq(authored,0,"Test");
        auto event=[&](juce::Point<float> pos,int clicks=1){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pos,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,this,this,juce::Time::getCurrentTime(),pos,juce::Time::getCurrentTime(),clicks,false);};
        const int selectionUndo=undos;mouseDown(event(getNodePosition(0,getPlotArea())));mouseUp(event(getNodePosition(0,getPlotArea())));expect(undos==selectionUndo,"selecting a band does not add an empty Undo step");
        selectBand(1);const int before=undos;mouseDoubleClick(event(getNodePosition(0,getPlotArea()),2));
        expect(currentEq.bands.size()==3&&currentEq.bands[0].gain==0&&currentEq.bands[1].gain==-4&&undos==before+1,"double-click resets the clicked band, never deletes another one");
        selectBand(1);beginEdit(0);editor.setText("nonsense");const auto saved=currentEq;const int edits=undos;commitEdit();expect(currentEq==saved&&undos==edits&&editor.isVisible(),"invalid text keeps the band and Undo unchanged");
        editor.setText("2,5 kHz");commitEdit();expect(currentEq.bands[1].freq==2500&&undos==edits+1,"unit/decimal frequency entry updates exactly one band and Undo step");
        beginEdit(1);editor.setText("-6.5 dB");commitEdit();expect(currentEq.bands[1].gain==-6.5&&undos==edits+2,"valid entry is one undoable edit");
        selectBand(2);beginEdit(1);expect(editParam<0,"cut/notch gain editor is disabled");
        beginEdit(0);editor.setText("200");setEq(authored,1,"Another track");commitEdit();expect(currentEq==authored&&!editor.isVisible(),"track changes discard stale editor input");
        selectBand(0);const auto wheelEvent=event(getNodePosition(0,getPlotArea()));juce::MouseWheelDetails wheel;wheel.deltaY=.1f;const int scroll=undos;mouseWheelMove(wheelEvent,wheel);expect(undos==scroll+1,"Q wheel edit participates in Undo");
        for(auto size:std::initializer_list<juce::Point<int>>{{732,354},{932,514}}){setSize(size.x,size.y);const auto hud=getHudBounds(getPlotArea());const auto l=hudLayout(hud);expect(hud.contains(l.del)&&hud.contains(l.freq)&&hud.contains(l.q)&&getPlotArea().getBottom()<hud.getY(),"parameter strip fits the minimum and normal sizes without covering the graph");}
        onDragStart=nullptr;onSettingsChanged=nullptr;onDragEnd=nullptr;return ok;
    }
#endif

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
        dragBand = -1;
        if (onSettingsChanged) onSettingsChanged();
        if (onDragEnd) onDragEnd();
        if(onSelectionChanged)onSelectionChanged();
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
            if(onSelectionChanged)onSelectionChanged();
            repaint();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if(!key.getModifiers().isCommandDown()&&!key.getModifiers().isCtrlDown()){
            const auto letter=juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
            if(letter=='f'||letter=='g'||letter=='q'){beginEdit(letter=='f'?0:letter=='g'?1:2);return true;}
        }
        if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey)) {
            deleteSelectedBand();
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            selectBand(-1);
            return true;
        }
        return false;
    }

    void timerCallback() override {
        if(!isShowing())return;
        if(getSampleRate){const double rate=getSampleRate();if(std::isfinite(rate)&&rate>=8000)sampleRate=rate;}
        std::vector<float> pre,post;
        if(analyzerMode!=0&&getSpectrum)getSpectrum(pre,post);
        analyzer.analyse(pre,sampleRate,preBands,spectrumSpeed);
        analyzer.analyse(post,sampleRate,postBands,spectrumSpeed);
        repaint();
    }

    void beginEdit(int param) {
        if (selectedBand < 0 || selectedBand >= (int)currentEq.bands.size()) return;
        const auto& b = currentEq.bands[(size_t)selectedBand];
        if(param==1&&!eqHasGain(b.type))return;
        editor.setColour(juce::TextEditor::outlineColourId,Daw::lineStrong);
        editor.setColour(juce::TextEditor::focusedOutlineColourId,Daw::accent);
        editParam = param;
        const juce::String text = param == 0 ? juce::String(b.freq, b.freq < 100.0 ? 1 : 0)
                                : param == 1 ? juce::String(b.gain, 1)
                                             : juce::String(b.q, 2);
        editor.setInputRestrictions(24);
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
        if(editParam<0)return;
        if(selectedBand<0||selectedBand>=(int)currentEq.bands.size()){cancelEdit();return;}
        double value=0;
        if(!parseEqValue(editor.getText().toStdString(),editParam,value)){
            editor.setColour(juce::TextEditor::outlineColourId,Daw::meterHigh);
            editor.setColour(juce::TextEditor::focusedOutlineColourId,Daw::meterHigh);
            editor.setTooltip(tr("Enter a valid frequency, gain or Q within its range."));return;
        }
        const int parameter=editParam,index=selectedBand;
        cancelEdit();
        const auto& old=currentEq.bands[(size_t)index];
        if(value==(parameter==0?old.freq:parameter==1?old.gain:old.q))return;
        editOnce([&]{auto& b=currentEq.bands[(size_t)index];if(parameter==0)b.freq=value;else if(parameter==1)b.gain=value;else b.q=value;b.normalise();});
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

            const double dbTicks[] = {displayRange,displayRange*.75,displayRange*.5,displayRange*.25,0,-displayRange*.25,-displayRange*.5,-displayRange*.75,-displayRange};
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
        const double dbLabels[] = {displayRange,displayRange*.5,0,-displayRange*.5,-displayRange};
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
            if(f.hz>maxFrequency())continue;
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
            auto drawSpectrum = [&](const EqAnalyzer::Trace& values, juce::Colour fill, juce::Colour line) {
                if(*std::max_element(values.begin(),values.end())<.001f)return;
                juce::Path path;
                path.startNewSubPath(plotArea.getX(), plotArea.getBottom());
                for (int i = 0; i < EqAnalyzer::points; ++i) {
                    const float x = plotArea.getX() + plotArea.getWidth() * (float)i / (EqAnalyzer::points-1);
                    const float y = plotArea.getBottom() - values[(size_t)i] * plotArea.getHeight();
                    path.lineTo(x, y);
                }
                path.lineTo(plotArea.getRight(), plotArea.getBottom());
                path.closeSubPath();
                g.setColour(fill);
                g.fillPath(path);
                g.setColour(line);
                g.strokePath(path, juce::PathStrokeType(1.0f));
            };
            if(analyzerMode==1||analyzerMode==2)drawSpectrum(preBands, Daw::textFaint.withAlpha(0.08f), Daw::textFaint.withAlpha(0.22f));
            if (analyzerMode==1||analyzerMode==3)
                drawSpectrum(postBands, juce::Colour(0xff4cc3f0).withAlpha(0.10f), juce::Colour(0xff4cc3f0).withAlpha(0.45f));
        }

        std::vector<EqCoefficients> bandCoeffs;
        bandCoeffs.reserve(currentEq.bands.size());
        for (const auto& b : currentEq.bands) bandCoeffs.push_back(eqCoefficients(b, sampleRate));

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
                        const float y = yFromDb(std::clamp(bandCoeffs[(size_t)which].magnitudeDb(hz, sampleRate), -26.0, 26.0), plotArea);
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
                    for (const auto& coeff : bandCoeffs) totalDb += coeff.magnitudeDb(hz, sampleRate);
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

        g.setFont(Daw::smallFont(9));g.setColour(Daw::textFaint);
        g.drawText("dB",2,3,34,16,juce::Justification::centredRight);
        g.drawText("dBFS",(int)plotArea.getRight()+6,3,38,16,juce::Justification::centredLeft);
        for(int db:{0,-24,-48,-72,-96}){const float y=plotArea.getY()+plotArea.getHeight()*(-db/96.f);g.drawText(juce::String(db),(int)plotArea.getRight()+7,(int)y-7,34,14,juce::Justification::centredLeft);}
        if(analyzerMode==1||analyzerMode==2){g.setColour(Daw::textDim);g.drawText(tr("INPUT"),getWidth()-174,2,72,16,juce::Justification::centred);}if(analyzerMode==1||analyzerMode==3){g.setColour(juce::Colour(0xff4cc3f0));g.drawText(tr("OUTPUT"),getWidth()-100,2,72,16,juce::Justification::centred);}
        if(selectedBand>=0&&selectedBand<(int)currentEq.bands.size())drawBandHud(g,plotArea);
        else{const auto box=getHudBounds(plotArea);g.setColour(Daw::surface);g.fillRoundedRectangle(box,Daw::radius);g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::body));g.drawFittedText(tr("Select a band, or double-click the graph to add one."),box.toNearestInt().reduced(18),juce::Justification::centred,2);}

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
        gesture=false;

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
            selectBand(found);
            nodeStartFreq=currentEq.bands[(size_t)found].freq;nodeStartGain=currentEq.bands[(size_t)found].gain;
            dragBand = found;
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
            selectBand(-1);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            const auto& b=currentEq.bands[(size_t)selectedBand];
            const auto l = hudLayout(getHudBounds(getPlotArea()));
            if (l.freq.contains(e.position)) { beginEdit(0); return; }
            if (l.gain.contains(e.position)&&eqHasGain(b.type)) { beginEdit(1); return; }
            if (l.q.contains(e.position)) { beginEdit(2); return; }
        }
        const auto plotArea = getPlotArea();
        if(getHudBounds(plotArea).contains(e.position))return;
        const int hit=findBandAt(e.position,plotArea);
        if(hit>=0){selectBand(hit);if(eqHasGain(currentEq.bands[(size_t)hit].type)&&currentEq.bands[(size_t)hit].gain!=0)editOnce([&]{currentEq.bands[(size_t)hit].gain=0;});return;}
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
        if((dragBand>=0||dragHudParam>=0)&&e.getDistanceFromDragStart()>1&&!gesture){if(onDragStart)onDragStart();gesture=true;}
        if(!gesture)return;

        // Dragging HUD parameter slider
        if (dragHudParam >= 0 && selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)selectedBand];
            const float dx=(float)e.getDistanceFromDragStartX()*(e.mods.isShiftDown()?.1f:1.f);
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
            const double precision=e.mods.isShiftDown()?.1:1.;
            b.freq=std::clamp(nodeStartFreq*std::pow(maxFrequency()/20.,e.getDistanceFromDragStartX()*precision/plotArea.getWidth()),20.,20000.);
            if (b.type != (int)EqFilterType::LowCut && b.type != (int)EqFilterType::HighCut && b.type != (int)EqFilterType::Notch) {
                b.gain=std::clamp(nodeStartGain-e.getDistanceFromDragStartY()*precision*2*displayRange/plotArea.getHeight(),-24.,24.);
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
            if(gesture&&onDragEnd)onDragEnd();
            gesture=false;
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        int targetBand = (hoverBand >= 0) ? hoverBand : selectedBand;
        if (targetBand >= 0 && targetBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)targetBand];
            if(wheel.deltaY==0)return;
            if(onWheelStart)onWheelStart();else if(onDragStart)onDragStart();
            b.q=std::clamp(b.q*std::pow(2.,wheel.deltaY*2.),.1,18.);
            b.normalise();
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            if(onDragEnd)onDragEnd();
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
    bool gesture=false;
    int dragHudParam = -1; // 0: freq, 1: gain, 2: q
    double dragStartParamVal = 0.0;
    EqAnalyzer analyzer;
    EqAnalyzer::Trace preBands{},postBands{};
    double sampleRate=44100,displayRange=12,nodeStartFreq=1000,nodeStartGain=0;
    int analyzerMode=1,spectrumSpeed=1;
    std::uint64_t revision=0,trackIdentity=0;
    juce::TextEditor editor;
    int editParam = -1;

    juce::Rectangle<float> getPlotArea() const {
        const float padL=44, padR=48, padT=24, padB=136;
        return juce::Rectangle<float>(padL, padT, (float)getWidth() - padL - padR, (float)getHeight() - padT - padB);
    }

    double maxFrequency()const{return std::min(20000.,sampleRate*.499);}

    float xFromFreq(double hz, const juce::Rectangle<float>& r) const {
        hz=std::clamp(hz,20.,maxFrequency());
        const double norm=std::log10(hz/20.)/std::log10(maxFrequency()/20.);
        return r.getX() + (float)(norm * r.getWidth());
    }

    double freqFromX(float x, const juce::Rectangle<float>& r) const {
        const double norm = std::clamp((double)(x - r.getX()) / (double)r.getWidth(), 0.0, 1.0);
        return 20.*std::pow(maxFrequency()/20.,norm);
    }

    float yFromDb(double db, const juce::Rectangle<float>& r) const {
        const double minDb=-displayRange,maxDb=displayRange;
        const double norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return r.getY() + (float)((1.0 - norm) * r.getHeight());
    }

    double dbFromY(float y, const juce::Rectangle<float>& r) const {
        const double minDb=-displayRange,maxDb=displayRange;
        const double norm = 1.0 - std::clamp((double)(y - r.getY()) / (double)r.getHeight(), 0.0, 1.0);
        return minDb + norm * (maxDb - minDb);
    }

    juce::Point<float> getNodePosition(int index, const juce::Rectangle<float>& r) const {
        if (index < 0 || index >= (int)currentEq.bands.size()) return { 0, 0 };
        const auto& b = currentEq.bands[(size_t)index];
        const double db = (b.type == (int)EqFilterType::LowCut || b.type == (int)EqFilterType::HighCut || b.type == (int)EqFilterType::Notch) ? 0.0 : b.gain;
        return {xFromFreq(b.freq,r),yFromDb(std::clamp(db,-displayRange,displayRange),r)};
    }

    int findBandAt(juce::Point<float> pos,const juce::Rectangle<float>& r)const{
        int best=-1;float distance=15;
        for(int i=0;i<(int)currentEq.bands.size();++i){const float d=pos.getDistanceFrom(getNodePosition(i,r));if(d<distance||(d==distance&&i==selectedBand)){distance=d;best=i;}}
        return best;
    }

    struct HudLayout {
        juce::Rectangle<float> badge, pills[6], freq, gain, q, bypass, invert, del;
    };

    HudLayout hudLayout(const juce::Rectangle<float>& hud) const {
        HudLayout l;auto r=hud.reduced(10,8);auto top=r.removeFromTop(30);
        l.badge=top.removeFromLeft(82);top.removeFromLeft(8);
        for(auto& pill:l.pills)pill=top.removeFromLeft(44).reduced(2,1);
        r.removeFromTop(6);auto row=r.removeFromTop(38);const float gap=6;
        const float valueWidth=std::max(65.f,(row.getWidth()-3*58.f-5*gap)/3.f);
        l.freq=row.removeFromLeft(valueWidth);row.removeFromLeft(gap);
        l.gain=row.removeFromLeft(valueWidth);row.removeFromLeft(gap);
        l.q=row.removeFromLeft(valueWidth);row.removeFromLeft(gap);
        l.bypass=row.removeFromLeft(58);row.removeFromLeft(gap);
        l.invert=row.removeFromLeft(58);row.removeFromLeft(gap);l.del=row.removeFromLeft(58);
        return l;
    }
    juce::Rectangle<float> getHudBounds(const juce::Rectangle<float>&)const{return {14.f,(float)getHeight()-98.f,(float)getWidth()-28.f,90.f};}

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
        g.drawText(tr("BAND")+" "+juce::String(selectedBand+1), l.badge, juce::Justification::centred);

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
            g.setFont(Daw::caps(9.5f));
            g.setColour(Daw::textFaint);
            g.drawText(label, inner.removeFromTop(11.0f), juce::Justification::centredLeft);
            g.setFont(Daw::font(Daw::body, true));
            g.setColour(colour);
            g.drawText(value, inner, juce::Justification::centredLeft);
        };

        const bool gainDisabled = (b.type == (int)EqFilterType::LowCut || b.type == (int)EqFilterType::HighCut || b.type == (int)EqFilterType::Notch);
        valueBox(l.freq, "Hz", (b.freq >= 1000.0) ? juce::String(b.freq / 1000.0, 2) + " kHz" : juce::String((int)b.freq) + " Hz", Daw::text);
        valueBox(l.gain, "dB", gainDisabled ? juce::String(juce::String::fromUTF8("—")) : (b.gain > 0 ? "+" : "") + juce::String(b.gain, 1) + " dB", gainDisabled ? Daw::textFaint : Daw::text);
        valueBox(l.q, "Q", juce::String(b.q, 2), Daw::text);

        auto chip = [&](juce::Rectangle<float> box, const juce::String& text, bool active, juce::Colour activeColour) {
            g.setColour(active ? activeColour.withAlpha(0.22f) : Daw::control);
            g.fillRoundedRectangle(box, Daw::radius);
            g.setColour(active ? activeColour : Daw::textDim);
            g.setFont(Daw::caps(10.0f));
            g.drawText(text, box, juce::Justification::centred);
        };

        chip(l.bypass, b.enabled ? "ON" : "OFF", b.enabled, Daw::accent);
        g.setColour(Daw::control);g.fillRoundedRectangle(l.invert,Daw::radius);
        g.setColour(gainDisabled?Daw::textFaint:Daw::textDim);g.setFont(Daw::caps(10));g.drawText("+/-",l.invert,juce::Justification::centred);
        chip(l.del,tr("DELETE"),true,Daw::meterHigh);

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
            const juce::String tip=tr(tips[hoverPill]);
            const int tipW = 320, tipH = 26;
            juce::Rectangle<int> box((int)pill.getCentreX() - tipW / 2, (int)pill.getY() - tipH - 8, tipW, tipH);
            if (box.getY() < 4) box.setY((int)pill.getBottom()+8);
            box.setX(std::clamp(box.getX(),4,std::max(4,getWidth()-tipW-4)));
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
                editOnce([&]{b.type=i;b.normalise();});
                return;
            }
        }

        if (l.freq.contains(e.position)) {
            dragHudParam = 0; dragStartParamVal = b.freq;
            return;
        }
        if (l.gain.contains(e.position)&&eqHasGain(b.type)) {
            dragHudParam = 1; dragStartParamVal = b.gain;
            return;
        }
        if (l.q.contains(e.position)) {
            dragHudParam = 2; dragStartParamVal = b.q;
            return;
        }
        if (l.bypass.contains(e.position)) {
            editOnce([&]{b.enabled=!b.enabled;});return;
        }
        if(l.invert.contains(e.position)&&eqHasGain(b.type)){
            editOnce([&]{b.gain=-b.gain;});return;
        }
        if (l.del.contains(e.position)) {
            deleteSelectedBand();
            return;
        }
    }



    void showBandMenu(int index,juce::Point<int> screen){
        selectBand(index);juce::PopupMenu menu;
        menu.addSectionHeader(tr("BAND")+" "+juce::String(index+1));
        for(int i=0;i<6;++i)menu.addItem(10+i,tr(filterTypeNames[i]),true,currentEq.bands[(size_t)index].type==i);
        menu.addSeparator();menu.addItem(1,tr("Invert Gain"),eqHasGain(currentEq.bands[(size_t)index].type));
        menu.addItem(2,tr(currentEq.bands[(size_t)index].enabled?"Bypass Band":"Enable Band"));menu.addItem(3,tr("Delete Band"));
        const auto epoch=revision;juce::Component::SafePointer<EqDisplayComponent> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({screen.x,screen.y,1,1}),[safe,index,epoch](int result){
            if(!safe||safe->revision!=epoch||index>=(int)safe->currentEq.bands.size()||result==0)return;
            if(result==3){safe->deleteSelectedBand();return;}
            safe->editOnce([&]{auto& b=safe->currentEq.bands[(size_t)index];if(result>=10&&result<=15)b.type=result-10;else if(result==1&&eqHasGain(b.type))b.gain=-b.gain;else if(result==2)b.enabled=!b.enabled;b.normalise();});
        });
    }
    void showCanvasMenu(double hz,double db,juce::Point<int> screen){
        juce::PopupMenu menu;menu.addSectionHeader(tr("+ ADD BAND"));
        for(int i=0;i<6;++i)menu.addItem(10+i,tr(filterTypeNames[i]),currentEq.bands.size()<16);
        menu.addSeparator();menu.addItem(1,tr("RESET EQ"));
        const auto epoch=revision;juce::Component::SafePointer<EqDisplayComponent> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({screen.x,screen.y,1,1}),[safe,hz,db,epoch](int result){
            if(!safe||safe->revision!=epoch)return;
            if(result>=10&&result<=15)safe->addBandAt(hz,db,result-10);else if(result==1)safe->flatten();
        });
    }

};

} // namespace

class GraphicEqWindow::Panel : public juce::Component {
public:
    explicit Panel(MainComponent& m):owner(m){
        for(juce::Component* c:std::initializer_list<juce::Component*>{&eqDisplay,&trackSelector,&bandSelector,&addBandButton,&enableButton,&resetButton,&analyzerMode,&speed,&range})addAndMakeVisible(c);
        Localisation::bindTip(addBandButton,"Add a new parametric band to the EQ.");
        Localisation::bindTip(enableButton,"Toggle EQ on or off for this track.");
        Localisation::bindTip(resetButton,"Flatten gains and bypass cut/notch filters. Undo restores the EQ.");
        eqDisplay.getSampleRate=[this]{return owner.audio.spectrumSampleRate();};
        eqDisplay.getSpectrum=[this](std::vector<float>& pre,std::vector<float>& post){
            const auto stamp=owner.audio.spectrumRevision();
            if(stamp==lastSpectrumRevision)return false;
            lastSpectrumRevision=stamp;if(stamp-spectrumGate<(std::uint32_t)EqAnalyzer::size)return false;
            owner.audio.readSpectrum(currentTrack,pre,post,EqAnalyzer::size,true);return true;
        };
        addBandButton.onClick=[this]{if(validTrack())eqDisplay.addBandAt(1000,0);};
        enableButton.onClick=[this]{if(updating||!validTrack())return;owner.checkpointMix();owner.project.tracks[(size_t)currentTrack].eq.enabled=enableButton.getToggleState();owner.mixChanged();};
        resetButton.onClick=[this]{if(validTrack())eqDisplay.flatten();};
        trackSelector.onChange=[this]{if(updating||trackSelector.getSelectedId()<1)return;currentTrack=trackSelector.getSelectedId()-1;owner.select(currentTrack,owner.selectedSection);refresh();};
        bandSelector.onChange=[this]{if(!updating)eqDisplay.selectBand(bandSelector.getSelectedId()-2);};
        eqDisplay.onSelectionChanged=[this]{refresh();};
        eqDisplay.onDragStart=[this]{if(validTrack())owner.checkpointMix();};
        eqDisplay.onSettingsChanged=[this]{if(updating||!validTrack())return;owner.project.tracks[(size_t)currentTrack].eq=eqDisplay.getEq();owner.mixChanged();};
        eqDisplay.onDragEnd=[this]{refresh();};
        eqDisplay.onWheelStart=[this]{if(validTrack())owner.checkpointMix(&eqDisplay);};
        for(auto* combo:{&analyzerMode,&speed,&range})combo->onChange=[this]{if(updating)return;Localisation::setSetting("eqAnalyzerMode",analyzerMode.getSelectedId()-1);Localisation::setSetting("eqAnalyzerSpeed",speed.getSelectedId()-1);Localisation::setSetting("eqDisplayRange",range.getSelectedId()-1);updateView();};
        analyzerMode.setSelectedId(std::clamp(Localisation::setting("eqAnalyzerMode","1").getIntValue(),0,3)+1,juce::dontSendNotification);
        speed.setSelectedId(std::clamp(Localisation::setting("eqAnalyzerSpeed","1").getIntValue(),0,2)+1,juce::dontSendNotification);
        range.setSelectedId(std::clamp(Localisation::setting("eqDisplayRange","1").getIntValue(),0,2)+1,juce::dontSendNotification);
    }
    void setTrack(int track){meterTrack=-2;currentTrack=std::clamp(track>=0?track:owner.selectedTrack,0,(int)owner.project.tracks.size()-1);refresh();}
    void refresh(){
        if(owner.project.tracks.empty())return;updating=true;
        currentTrack=std::clamp(currentTrack,0,(int)owner.project.tracks.size()-1);
        const auto& track=owner.project.tracks[(size_t)currentTrack];
        trackSelector.clear(juce::dontSendNotification);for(size_t i=0;i<owner.project.tracks.size();++i)trackSelector.addItem(juce::String::fromUTF8(owner.project.tracks[i].name.c_str()),(int)i+1);
        trackSelector.setSelectedId(currentTrack+1,juce::dontSendNotification);
        eqDisplay.setEq(track.eq,currentTrack,juce::String::fromUTF8(track.name.c_str()),track.sessionId);
        bandSelector.clear(juce::dontSendNotification);bandSelector.addItem(tr("SELECT BAND"),1);
        const auto& bands=eqDisplay.getEq().bands;
        for(size_t i=0;i<bands.size();++i){const auto& b=bands[i];bandSelector.addItem(juce::String((int)i+1)+juce::String::fromUTF8(" · ")+tr(filterTypeNames[b.type])+juce::String::fromUTF8(" · ")+(b.freq>=1000?juce::String(b.freq/1000,2)+"k":juce::String((int)b.freq))+(!b.enabled?" ["+tr("OFF")+"]":""),(int)i+2);}
        bandSelector.setSelectedId(eqDisplay.getSelectedBand()+2,juce::dontSendNotification);
        addBandButton.setButtonText(tr("+ ADD BAND"));addBandButton.setEnabled(bands.size()<16);
        enableButton.setButtonText(tr(track.eq.enabled?"EQ ACTIVE":"EQ BYPASSED"));enableButton.setToggleState(track.eq.enabled,juce::dontSendNotification);
        resetButton.setButtonText(tr("RESET EQ"));
        auto populate=[](juce::ComboBox& c,std::initializer_list<juce::String> texts,int fallback){const int id=c.getSelectedId()>0?c.getSelectedId():fallback;c.clear(juce::dontSendNotification);int i=1;for(const auto& t:texts)c.addItem(t,i++);c.setSelectedId(id,juce::dontSendNotification);};
        populate(analyzerMode,{tr("ANALYZER OFF"),tr("INPUT / OUTPUT"),tr("INPUT ONLY"),tr("OUTPUT ONLY")},std::clamp(Localisation::setting("eqAnalyzerMode","1").getIntValue(),0,3)+1);
        populate(speed,{tr("RESPONSE: FAST"),tr("RESPONSE: MEDIUM"),tr("RESPONSE: SLOW")},std::clamp(Localisation::setting("eqAnalyzerSpeed","1").getIntValue(),0,2)+1);
        populate(range,{juce::String::fromUTF8("±6 dB"),juce::String::fromUTF8("±12 dB"),juce::String::fromUTF8("±24 dB")},std::clamp(Localisation::setting("eqDisplayRange","1").getIntValue(),0,2)+1);
        updating=false;updateView();repaint();
    }
    void updateView(){
        const int mode=std::max(0,analyzerMode.getSelectedId()-1),selectedRange=std::max(0,range.getSelectedId()-1);
        eqDisplay.setView(mode,std::max(0,speed.getSelectedId()-1),selectedRange==0?6:selectedRange==1?12:24);
        const int wanted=mode==0?-1:currentTrack;
        if(wanted!=meterTrack){owner.audio.setSpectrumTrack(wanted);meterTrack=wanted;spectrumGate=lastSpectrumRevision=owner.audio.spectrumRevision();}
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base);g.setColour(Daw::accent);g.fillRoundedRectangle(15,16,3,18,1.5f);
        g.setColour(Daw::text);g.setFont(Daw::font(Daw::body,true));g.drawText(tr("PARAMETRIC EQ"),27,10,148,29,juce::Justification::centredLeft);
        g.setColour(Daw::line);g.drawHorizontalLine(eqDisplay.getY()-8,14.f,(float)getWidth()-14);
        g.setColour(Daw::textFaint);g.setFont(Daw::smallFont(9));
        g.drawFittedText(tr(juce::String::fromUTF8("Double-click: add band · Wheel: Q · Shift: fine adjustment")),getLocalBounds().removeFromBottom(22).reduced(18,1).withTrimmedRight(118),juce::Justification::centredLeft,1);
        g.drawText(juce::String(owner.audio.spectrumSampleRate()/1000.,1)+juce::String::fromUTF8(" kHz · ")+juce::String((int)eqDisplay.getEq().bands.size())+"/16",getWidth()-140,getHeight()-22,122,20,juce::Justification::centredRight);
    }
    void resized()override{
        auto r=getLocalBounds().reduced(14);auto top=r.removeFromTop(34);
        if(getWidth()>=740){
            resetButton.setBounds(top.removeFromRight(72));top.removeFromRight(8);enableButton.setBounds(top.removeFromRight(132));top.removeFromRight(8);addBandButton.setBounds(top.removeFromRight(114));top.removeFromRight(12);top.removeFromLeft(150);trackSelector.setBounds(top);
        }else{
            top.removeFromLeft(150);trackSelector.setBounds(top);r.removeFromTop(8);auto actions=r.removeFromTop(30);
            addBandButton.setBounds(actions.removeFromLeft(114));actions.removeFromLeft(8);enableButton.setBounds(actions.removeFromLeft(132));actions.removeFromLeft(8);resetButton.setBounds(actions.removeFromLeft(96));
        }
        r.removeFromTop(10);auto settings=r.removeFromTop(30);
        if(getWidth()<680){bandSelector.setBounds(settings);r.removeFromTop(6);settings=r.removeFromTop(30);}
        range.setBounds(settings.removeFromRight(92));settings.removeFromRight(8);speed.setBounds(settings.removeFromRight(144));settings.removeFromRight(8);
        if(getWidth()<680)analyzerMode.setBounds(settings);else{analyzerMode.setBounds(settings.removeFromRight(174));settings.removeFromRight(10);bandSelector.setBounds(settings);}
        r.removeFromTop(13);r.removeFromBottom(12);eqDisplay.setBounds(r);
    }

#if FGA_SELF_TESTS
    bool writeEvidence(const juce::File& folder);
#endif
private:
    MainComponent& owner;EqDisplayComponent eqDisplay;
    juce::ComboBox trackSelector,bandSelector,analyzerMode,speed,range;
    juce::TextButton addBandButton,resetButton;juce::ToggleButton enableButton;
    int currentTrack=0,meterTrack=-2;bool updating=false;
    std::uint32_t lastSpectrumRevision=0,spectrumGate=0;
    bool validTrack()const{return currentTrack>=0&&currentTrack<(int)owner.project.tracks.size();}
};

GraphicEqWindow::GraphicEqWindow(MainComponent& m)
    : DocumentWindow(tr("PARAMETRIC EQ"), Daw::base, juce::DocumentWindow::closeButton),
      owner(m) {
    setLookAndFeel(&m.theme);
    setUsingNativeTitleBar(true);
    panel = new Panel(m);
    setContentOwned(panel, false);
    setResizable(true, false);
    // Fit the editor inside the display's usable area (DPI and small screens included).
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = display ? display->userArea : juce::Rectangle<int>(0, 0, 1280, 800);
    const int maxW=juce::jmax(480,area.getWidth()-32);
    const int maxH=juce::jmax(360,area.getHeight()-48);
    setResizeLimits(std::min(760,maxW),std::min(520,maxH),maxW,maxH);
    setSize(std::min(1020,maxW),std::min(680,maxH));
    setAlwaysOnTop(true);
}

GraphicEqWindow::~GraphicEqWindow() {
    owner.audio.setSpectrumTrack(-1);
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
    setName(tr("PARAMETRIC EQ"));
    if (panel) panel->refresh();
}

void GraphicEqWindow::resized() {
    DocumentWindow::resized();

}

void GraphicEqWindow::closeButtonPressed() {
    owner.audio.setSpectrumTrack(-1);
    owner.dismissAuxiliaryWindow(*this);
}

#if FGA_SELF_TESTS
bool GraphicEqWindow::Panel::writeEvidence(const juce::File& folder){
    juce::StringArray checks;bool ok=true;auto expect=[&](bool passed,const char* label){checks.add(juce::String(passed?"PASS ":"FAIL ")+label);ok=passed&&ok;};
    EqDisplayComponent test;addAndMakeVisible(test);test.setBounds(eqDisplay.getBounds());ok=test.verifyInteractions(checks)&&ok;removeChildComponent(&test);
    for(double rate:{44100.,48000.,96000.}){
        EqAnalyzer analyzer;EqAnalyzer::Trace trace{};std::vector<float> sine(EqAnalyzer::size*2);
        const int bin=(int)std::lround(1000.*EqAnalyzer::size/rate);const double hz=bin*rate/EqAnalyzer::size;
        for(int i=0;i<EqAnalyzer::size;++i){sine[(size_t)i]=(float)std::sin(juce::MathConstants<double>::twoPi*bin*i/EqAnalyzer::size);sine[(size_t)i+EqAnalyzer::size]=-sine[(size_t)i];}
        for(int frame=0;frame<20;++frame)analyzer.analyse(sine,rate,trace,0);
        const auto peak=std::max_element(trace.begin(),trace.end());const double measuredHz=20.*std::pow(1000.,std::distance(trace.begin(),peak)/(double)(EqAnalyzer::points-1));
        expect(std::abs(measuredHz/hz-1)<.1&&std::abs((*peak-1)*96)<1,"spectrum follows the actual sample rate and retains opposite-phase stereo energy");
        for(int frame=0;frame<80;++frame)analyzer.analyse({},rate,trace,0);expect(*std::max_element(trace.begin(),trace.end())<.01,"stopped/missing input decays instead of freezing an old spectrum");
        for(int shape=0;shape<6;++shape){
            EqSettings eq;eq.enabled=true;eq.bands={{true,shape,700,4,1.2},{true,0,1200,-3,.9},{true,0,3200,2,2},{true,0,5200,-1,1}};eq.syncLegacyFromBands();
            ChannelStrip strip;strip.configure(eq,rate);const int frames=(int)rate;juce::AudioBuffer<float> audio(2,frames);
            for(int i=0;i<frames;++i)for(int ch=0;ch<2;++ch)audio.setSample(ch,i,(float)(.1*std::sin(juce::MathConstants<double>::twoPi*1000*i/rate)));
            const float dry=audio.getRMSLevel(0,frames/2,frames/2);strip.process(audio,frames);const double measured=20*std::log10(audio.getRMSLevel(0,frames/2,frames/2)/dry);double response=0;for(const auto& band:eq.bands)response+=eqCoefficients(band,rate).magnitudeDb(1000,rate);
            expect(std::abs(measured-response)<.15,"all six graph filter shapes match the processed audio, including four custom bands");
        }
    }
    setTrack(0);owner.undoStack.clear();owner.redoStack.clear();
    const auto original=owner.project.tracks[0].eq;const auto midiBefore=midiFile(owner.project);
    eqDisplay.addBandAt(2500,3);const auto added=owner.project.tracks[0].eq;
    expect(added.bands.size()==eqDisplay.getEq().bands.size()&&owner.undoStack.size()==1,"adding a band records exactly one real mix Undo");
    owner.undo(false);expect(owner.project.tracks[0].eq==original,"real Undo restores the previous EQ without regeneration");
    owner.undo(true);expect(owner.project.tracks[0].eq==added,"real Redo restores the added band");
    eqDisplay.flatten();owner.undo(false);expect(owner.project.tracks[0].eq==added&&midiFile(owner.project)==midiBefore,"reset/Undo preserves the curve and the song MIDI");
    owner.undoStack.clear();owner.redoStack.clear();
    auto& eq=owner.project.tracks[0].eq;eq.enabled=true;
    eq.bands={{true,1,35,0,.707},{true,2,110,2,.707},{true,0,420,-3.5,1.4},{true,0,2400,2.8,1.1},{true,3,9000,1.5,.707}};eq.syncLegacyFromBands();setTrack(0);eqDisplay.selectBand(2);
    const auto language=Localisation::current();
    for(auto lang:{Localisation::Language::italian,Localisation::Language::english}){
        Localisation::setCurrent(lang);refresh();
        for(auto size:std::initializer_list<juce::Point<int>>{{760,520},{1020,680}}){setSize(size.x,size.y);const auto image=createComponentSnapshot(getLocalBounds());const auto name=juce::String(lang==Localisation::Language::italian?"eq-it-":"eq-en-")+juce::String(size.x)+".png";if(auto stream=folder.getChildFile(name).createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(image,*stream);}}
    }
    Localisation::setCurrent(language);folder.getChildFile("eq-checks.txt").replaceWithText(checks.joinIntoString("\n"));return ok;
}
bool GraphicEqWindow::writeUiEvidence(const juce::File& folder){
    folder.createDirectory();const auto before=owner.project.tracks[0].eq;
    owner.openGraphicEq(0);auto* live=owner.graphicEqWindow.get();
    const bool readOnly=owner.project.tracks[0].eq==before;
    const bool ok=live&&live->panel->writeEvidence(folder);
    folder.getChildFile("eq-checks.txt").appendText(juce::String(readOnly?"\nPASS ":"\nFAIL ")+"opening the editor does not mutate stored settings");
    return readOnly&&ok;
}

#endif

} // namespace fga
