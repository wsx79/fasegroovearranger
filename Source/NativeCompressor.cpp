// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NativeCompressor.h"
#include "CompressorDSP.h"
#include "DawTheme.h"
#include "FamilyControls.h"
#include "Localisation.h"
#include <functional>
namespace fga {
static_assert(std::atomic<float>::is_always_lock_free,"Native compressor parameters/meters require lock-free float atomics");
namespace {
constexpr const char* ids[]{"threshold","ratio","knee","attack","release","makeup"};
constexpr const char* names[]{"Threshold","Ratio","Knee","Attack","Release","Makeup"};
constexpr const char* units[]{"dB",":1","dB","ms","ms","dB"};
class CompressorParameter final : public juce::HostedAudioProcessorParameter {
public:
    explicit CompressorParameter(int i):range(CompressorDSP::minimum[(size_t)i],CompressorDSP::maximum[(size_t)i]),index(i){value=range.convertTo0to1(CompressorDSP::defaults[(size_t)i]);}
    float plain()const{return range.convertFrom0to1(value.load(std::memory_order_relaxed));}
    float normalized(float v)const{return range.convertTo0to1(CompressorDSP::bounded(v,index));}
    float getValue()const override{return value.load(std::memory_order_relaxed);}
    void setValue(float v)override{value=std::isfinite(v)?std::clamp(v,0.f,1.f):getDefaultValue();}
    float getDefaultValue()const override{return normalized(CompressorDSP::defaults[(size_t)index]);}
    juce::String getName(int)const override{return names[index];}
    juce::String getLabel()const override{return units[index];}
    juce::String getParameterID()const override{return ids[index];}
    juce::String getText(float v,int)const override{return juce::String(range.convertFrom0to1(std::clamp(v,0.f,1.f)),1);}
    float getValueForText(const juce::String& text)const override{return normalized(text.getFloatValue());}
    juce::NormalisableRange<float> range;
private:int index;std::atomic<float> value{0};
};
class Compressor final : public juce::AudioPluginInstance {
public:
    Compressor():AudioPluginInstance(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)){
        for(int i=0;i<6;++i){auto parameter=std::make_unique<CompressorParameter>(i);parameters[(size_t)i]=parameter.get();addHostedParameter(std::move(parameter));}
    }
    CompressorDSP::Values values()const{CompressorDSP::Values values;for(int i=0;i<6;++i)values[(size_t)i]=parameters[(size_t)i]->plain();return values;}
    const juce::String getName()const override{return "Native Compressor";}
    void fillInPluginDescription(juce::PluginDescription& description)const override{description=nativeCompressorDescription();}
    bool isBusesLayoutSupported(const BusesLayout& layout)const override{return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono());}
    void prepareToPlay(double rate,int)override{dsp.prepare(rate,values());reduction=0;}
    void releaseResources()override{}
    void reset()override{dsp.reset();reduction=0;}
    void processBlock(juce::AudioBuffer<float>& block,juce::MidiBuffer&)override{
        if(block.getNumChannels()<1)return;
        dsp.configure(values());dsp.setBypassed(bypassed);
        dsp.process(block.getWritePointer(0),block.getNumChannels()>1?block.getWritePointer(1):nullptr,block.getNumSamples());
        reduction.store((float)dsp.reductionDb(),std::memory_order_relaxed);
    }
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& data)override{juce::XmlElement state("FgaCompressor");state.setAttribute("version",1);for(int i=0;i<6;++i)state.setAttribute(ids[i],(double)parameters[(size_t)i]->plain());copyXmlToBinary(state,data);}
    void setStateInformation(const void* data,int bytes)override{auto state=getXmlFromBinary(data,bytes);if(!state||!state->hasTagName("FgaCompressor")||state->getIntAttribute("version")!=1)return;for(int i=0;i<6;++i)if(state->hasAttribute(ids[i]))parameters[(size_t)i]->setValue(parameters[(size_t)i]->normalized((float)state->getDoubleAttribute(ids[i])));}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
    bool bypassed=false;std::atomic<float> reduction{0};
    std::array<CompressorParameter*,6> parameters{};
private:CompressorDSP dsp;
};
namespace ui {
const juce::Colour blue(0xff4d8dff);
const juce::Colour blueDim(0xff2f5ea8);
const juce::Colour knobColours[6] = {
    juce::Colour(0xff4d8dff), // threshold: blue
    juce::Colour(0xff38c6c0), // ratio: teal
    juce::Colour(0xff9b8cff), // knee: violet
    juce::Colour(0xffffb54a), // attack: amber
    juce::Colour(0xffff7a6b), // release: coral
    juce::Colour(0xff5ec8e8)  // makeup: cyan
};
static juce::String loc(const char* en, const char* it)
{
    return Localisation::current() == Localisation::Language::italian ? juce::String::fromUTF8(it) : juce::String(en);
}
constexpr float arcStart = -2.35619449f; // -135 degrees
constexpr float arcSweep = 4.71238898f;  // 270 degrees

static juce::String dbText(double v, bool sign = false)
{
    return (sign && v > 0.0 ? "+" : "") + juce::String(v, 1) + " dB";
}

static juce::String ratioText(double v) { return juce::String(v, 2) + " : 1"; }

static juce::String timeText(double v) { return juce::String(v, v < 100.0 ? 1 : 0) + " ms"; }

class Knob final : public juce::Component, public juce::SettableTooltipClient
{
public:
    Knob(juce::Slider& s, juce::String title, juce::String tip, double resetValue,
         std::function<juce::String(double)> formatter, juce::Colour accentColour)
        : slider(s), name(std::move(title)), defaultValue(resetValue), format(std::move(formatter)), accent(accentColour)
    {
        addAndMakeVisible(value);
        value.setJustificationType(juce::Justification::centred);
        value.setFont(Daw::font(Daw::body, true));
        value.setColour(juce::Label::backgroundColourId, Daw::lcd);
        value.setColour(juce::Label::textColourId, Daw::text);
        value.setColour(juce::Label::outlineColourId, Daw::line);
        value.setColour(juce::Label::textWhenEditingColourId, Daw::text);
        value.setEditable(false, true, false);
        value.onTextChange = [this] {
            auto typed = value.getText().trim().replaceCharacter(',', '.');
            if (typed.isEmpty()) { refresh(); return; }
            const double next = typed.getDoubleValue();
            if (!std::isfinite(next)) { refresh(); return; }
            gesture([this, next] { slider.setValue(next, juce::sendNotificationSync); });
            refresh();
        };
        setTooltip(std::move(tip));
    }

    void refresh()
    {
        const auto text = format(slider.getValue());
        bool changed = false;
        if (!value.isBeingEdited() && value.getText() != text)
        {
            value.setText(text, juce::dontSendNotification);
            changed = true;
        }
        if (std::abs(slider.getValue() - lastShown) > 1.0e-6)
        {
            lastShown = slider.getValue();
            changed = true;
        }
        if (changed) repaint();
    }

    bool isDragging() const { return dragging; }
    bool isEditing() const { return value.isBeingEdited(); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto title = area.removeFromTop(18.0f);
        g.setFont(Daw::caps(10.0f));
        g.setColour(Daw::textDim);
        g.drawText(name.toUpperCase(), title.toNearestInt(), juce::Justification::centred);

        area.removeFromBottom(32.0f);
        const float diameter = std::max(30.0f, std::min(area.getWidth(), area.getHeight()) - 42.0f);
        const auto centre = area.getCentre();
        const float radius = diameter * 0.5f;
        const float arcRadius = radius - 3.0f;
        const double norm = std::clamp((slider.getValue() - slider.getMinimum()) /
                                           std::max(1.0e-9, slider.getMaximum() - slider.getMinimum()), 0.0, 1.0);

        g.setColour(Daw::lineStrong);
        for (int i = 0; i <= 10; ++i)
        {
            const float angle = arcStart + arcSweep * (float)i / 10.0f;
            const float from = arcRadius + 4.0f, to = arcRadius + (i % 5 == 0 ? 11.0f : 8.0f);
            g.drawLine(centre.x + std::sin(angle) * from, centre.y - std::cos(angle) * from,
                       centre.x + std::sin(angle) * to, centre.y - std::cos(angle) * to, 1.0f);
        }

        juce::Path track, filled;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, arcStart, arcStart + arcSweep, true);
        g.setColour(Daw::line);
        g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (norm > 0.001)
        {
            filled.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 arcStart, arcStart + arcSweep * (float)norm, true);
            g.setColour(accent);
            g.strokePath(filled, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        juce::ColourGradient body(juce::Colour(0xff3b4049), centre.x - radius * 0.6f, centre.y - radius * 0.7f,
                                  juce::Colour(0xff16191e), centre.x + radius * 0.7f, centre.y + radius * 0.8f, true);
        g.setGradientFill(body);
        g.fillEllipse(centre.x - radius + 6.0f, centre.y - radius + 6.0f, (radius - 6.0f) * 2.0f, (radius - 6.0f) * 2.0f);
        g.setColour(Daw::lineStrong);
        g.drawEllipse(centre.x - radius + 6.0f, centre.y - radius + 6.0f, (radius - 6.0f) * 2.0f, (radius - 6.0f) * 2.0f, 1.0f);

        const float pointerAngle = arcStart + arcSweep * (float)norm;
        g.setColour(dragging ? accent : Daw::text);
        g.drawLine(centre.x + std::sin(pointerAngle) * radius * 0.20f,
                   centre.y - std::cos(pointerAngle) * radius * 0.20f,
                   centre.x + std::sin(pointerAngle) * radius * 0.68f,
                   centre.y - std::cos(pointerAngle) * radius * 0.68f, 2.0f);
    }

    void resized() override { value.setBounds(getLocalBounds().removeFromBottom(28).reduced(12, 3)); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        dragging = true;
        dragStart = slider.getValue();
        if (slider.onDragStart) slider.onDragStart();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;
        const double range = slider.getMaximum() - slider.getMinimum();
        const double fine = e.mods.isShiftDown() ? 0.2 : 1.0;
        const double next = dragStart + (double)(-e.getDistanceFromDragStartY()) / 180.0 * range * fine;
        slider.setValue(next, juce::sendNotificationSync);
        refresh();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (!dragging) return;
        dragging = false;
        if (slider.onDragEnd) slider.onDragEnd();
        refresh();
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        gesture([this] { slider.setValue(defaultValue, juce::sendNotificationSync); });
        refresh();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const double range = slider.getMaximum() - slider.getMinimum();
        const double next = slider.getValue() + (double)wheel.deltaY * range * 0.02;
        gesture([this, next] { slider.setValue(next, juce::sendNotificationSync); });
        refresh();
    }

private:
    template <typename Fn> void gesture(Fn&& fn)
    {
        if (slider.onDragStart) slider.onDragStart();
        fn();
        if (slider.onDragEnd) slider.onDragEnd();
    }

    juce::Slider& slider;
    juce::String name;
    double defaultValue;
    std::function<juce::String(double)> format;
    juce::Label value;
    bool dragging = false;
    double dragStart = 0.0;
    double lastShown = -1.0e9;
    juce::Colour accent;
};

class Curve final : public juce::Component
{
public:
    explicit Curve(Compressor& p) : processor(p) { setInterceptsMouseClicks(false, false); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(Daw::surface);
        g.fillRoundedRectangle(bounds, Daw::panelRadius);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(bounds, Daw::panelRadius, 1.0f);

        auto title = bounds.reduced(14.0f, 8.0f).removeFromTop(16.0f);
        g.setFont(Daw::caps(9.5f));
        g.setColour(Daw::textDim);
        g.drawText(ui::loc("COMPRESSION CURVE", "CURVA DI COMPRESSIONE"), title.toNearestInt(), juce::Justification::centredLeft);

        auto plot = bounds.reduced(40.0f, 30.0f);
        plot.removeFromRight(16.0f);
        plot.removeFromBottom(28.0f);
        auto xFrom = [&](double db) { return plot.getX() + (float)((db + 60.0) / 60.0) * plot.getWidth(); };
        auto yFrom = [&](double db) { return plot.getY() + (float)((0.0 - db) / 60.0) * plot.getHeight(); };

        g.setFont(Daw::smallFont(9.0f));
        for (int db = -60; db <= 0; db += 12)
        {
            const float x = xFrom(db), y = yFrom(db);
            g.setColour(Daw::line.withAlpha(0.7f));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
            g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
            g.setColour(Daw::textFaint);
            g.drawText(juce::String(db), juce::roundToInt(x - 16.0f), juce::roundToInt(plot.getBottom() + 4.0f), 32, 12, juce::Justification::centred);
            g.drawText(juce::String(db), 0, juce::roundToInt(y - 6.0f), juce::roundToInt(plot.getX() - 7.0f), 12, juce::Justification::centredRight);
        }

        const float dash[] = { 4.0f, 4.0f };
        g.setColour(Daw::textFaint.withAlpha(0.55f));
        g.drawDashedLine(juce::Line<float>(plot.getX(), plot.getBottom(), plot.getRight(), plot.getY()), dash, 2, 1.0f);

        const auto v = processor.values();
        const double threshold = v[0], ratio = std::max(1.0, (double)v[1]), knee = std::max(0.0, (double)v[2]);
        juce::Path curve, fill;
        fill.startNewSubPath(plot.getX(), plot.getBottom());
        for (int i = 0; i <= 240; ++i)
        {
            const double input = -60.0 + 60.0 * (double)i / 240.0;
            const double reduction = CompressorDSP::reduction(input, threshold, ratio, knee);
            const double output = input - reduction;
            const float x = xFrom(input), y = yFrom(std::clamp(output, -60.0, 0.0));
            if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
            fill.lineTo(x, y);
        }
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.closeSubPath();
        g.setColour(ui::blue.withAlpha(0.10f));
        g.fillPath(fill);
        g.setColour(ui::blue);
        g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));

        if (threshold >= -60.0 && threshold <= 0.0)
        {
            const double atThreshold = threshold - CompressorDSP::reduction(threshold, threshold, ratio, knee);
            g.setColour(Daw::text.withAlpha(0.55f));
            g.drawVerticalLine(juce::roundToInt(xFrom(threshold)), plot.getY(), plot.getBottom());
            g.setColour(Daw::text);
            g.fillEllipse(xFrom(threshold) - 3.0f, yFrom(atThreshold) - 3.0f, 6.0f, 6.0f);
        }
    }

private:
    Compressor& processor;
};

class ReductionMeter final : public juce::Component
{
public:
    void setReduction(float db)
    {
        if (std::abs(db - shown) < 0.01f) return;
        shown = db;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(Daw::surface);
        g.fillRoundedRectangle(bounds, Daw::panelRadius);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(bounds, Daw::panelRadius, 1.0f);

        auto area = bounds.reduced(14.0f, 10.0f);
        g.setFont(Daw::caps(9.5f));
        g.setColour(Daw::textDim);
        g.drawText(ui::loc("GAIN REDUCTION", "RIDUZIONE"), area.removeFromTop(16.0f).toNearestInt(), juce::Justification::centred);

        g.setFont(Daw::font(Daw::title, true));
        g.setColour(shown > 0.05f ? ui::blue : Daw::textFaint);
        g.drawText("-" + juce::String(shown, 1) + " dB", area.removeFromTop(30.0f).toNearestInt(), juce::Justification::centred);

        auto meter = area.reduced(18.0f, 4.0f);
        const int labelWidth = 34;
        auto scale = meter.removeFromRight((float)labelWidth);
        const float barWidth = std::min(34.0f, meter.getWidth());
        auto bar = meter.withSizeKeepingCentre(barWidth, meter.getHeight());

        g.setColour(Daw::lcd);
        g.fillRoundedRectangle(bar, 3.0f);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(bar, 3.0f, 1.0f);

        const double span = 30.0;
        const float filled = (float)std::clamp((double)shown / span, 0.0, 1.0);
        if (filled > 0.001f)
        {
            auto lit = bar.withHeight(bar.getHeight() * filled);
            juce::ColourGradient grad(juce::Colour(0xffffb347), bar.getX(), bar.getY(),
                                      juce::Colour(0xffff5a4a), bar.getX(), bar.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(lit, 3.0f);
        }

        g.setFont(Daw::smallFont(8.5f));
        for (int db = 0; db <= 30; db += 6)
        {
            const float y = bar.getY() + bar.getHeight() * (float)db / (float)span;
            g.setColour(Daw::line.withAlpha(0.8f));
            g.drawHorizontalLine(juce::roundToInt(y), bar.getX(), bar.getRight());
            g.setColour(Daw::textFaint);
            g.drawText("-" + juce::String(db), juce::roundToInt(scale.getX() + 4.0f), juce::roundToInt(y - 6.0f), labelWidth - 4, 12, juce::Justification::centredLeft);
        }
    }

private:
    float shown = 0.0f;
};

} // namespace ui

class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor(Compressor& p) : AudioProcessorEditor(p), processor(p)
    {
        for (int i = 0; i < 6; ++i)
        {
            auto& slider = sliders[(size_t)i];
            auto& parameter = *processor.parameters[(size_t)i];
            addChildComponent(slider);
            slider.setRange(parameter.range.start, parameter.range.end, 0.1);
            slider.setValue(parameter.plain(), juce::dontSendNotification);
            slider.setComponentID("native-compressor-" + juce::String(ids[i]));
            slider.onDragStart = [&parameter] { parameter.beginChangeGesture(); };
            slider.onDragEnd = [&parameter] { parameter.endChangeGesture(); };
            slider.onValueChange = [&parameter, &slider] { parameter.setValueNotifyingHost(parameter.normalized((float)slider.getValue())); };
        }

        formatters[0] = [](double v) { return ui::dbText(v); };
        formatters[1] = [](double v) { return ui::ratioText(v); };
        formatters[2] = [](double v) { return ui::dbText(v); };
        formatters[3] = [](double v) { return ui::timeText(v); };
        formatters[4] = [](double v) { return ui::timeText(v); };
        formatters[5] = [](double v) { return ui::dbText(v, true); };

        const std::array<juce::String, 6> tips{
            ui::loc("Threshold: level where compression starts. Drag to adjust, double-click to reset, double-click the value to type.",
                    "Soglia: livello in cui inizia la compressione. Trascina per regolare, doppio clic per ripristinare, doppio clic sul valore per digitarlo."),
            ui::loc("Ratio: how strongly signals above the threshold are reduced.",
                    "Rapporto: quanto vengono ridotti i segnali sopra la soglia."),
            ui::loc("Knee: softens the transition around the threshold.",
                    "Ginocchio: ammorbidisce la transizione attorno alla soglia."),
            ui::loc("Attack: how quickly compression reacts.",
                    "Attacco: rapidità con cui la compressione reagisce."),
            ui::loc("Release: how quickly compression returns to unity.",
                    "Rilascio: rapidità con cui la compressione torna a unità."),
            ui::loc("Makeup: output gain added after compression.",
                    "Guadagno: guadagno di uscita aggiunto dopo la compressione.")};
        for (int i = 0; i < 6; ++i)
        {
            knobs[(size_t)i] = std::make_unique<ui::Knob>(sliders[(size_t)i], names[i], tips[(size_t)i],
                                                          CompressorDSP::defaults[(size_t)i], formatters[(size_t)i],
                                                          ui::knobColours[i]);
            addAndMakeVisible(*knobs[(size_t)i]);
        }

        addAndMakeVisible(curve);
        addAndMakeVisible(reduction);
        addAndMakeVisible(reset);
        reset.setButtonText(ui::loc("RESET", "RIPRISTINA"));
        reset.setTooltip(ui::loc("Restore all compressor parameters to their default values.",
                                 "Ripristina tutti i parametri del compressore ai valori predefiniti."));
        reset.onClick = [this] {
            for (int i = 0; i < 6; ++i)
            {
                auto& slider = sliders[(size_t)i];
                if (slider.onDragStart) slider.onDragStart();
                slider.setValue(CompressorDSP::defaults[(size_t)i], juce::sendNotificationSync);
                if (slider.onDragEnd) slider.onDragEnd();
            }
        };

        lastValues = processor.values();
        lastBypassed = processor.bypassed;
        setResizable(true, true);
        setResizeLimits(720, 480, 1600, 1000);
        setSize(1000, 600);
        startTimerHz(30);
    }

    ~Editor() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Daw::base);

        g.setColour(Daw::text);
        g.setFont(Daw::font(24.0f, true));
        g.drawText(ui::loc("COMPRESSOR", "COMPRESSORE"), 16, 8, 360, 30, juce::Justification::centredLeft);
        g.setColour(Daw::textDim);
        g.setFont(Daw::caps(9.5f));
        g.drawText(ui::loc("FGA NATIVE  /  DYNAMICS", "FGA NATIVE  /  DINAMICA"), 18, 38, 360, 14, juce::Justification::centredLeft);

        const auto status = juce::Rectangle<int>(getWidth() - 250, 18, 150, 24);
        g.setColour(lastBypassed ? Daw::textFaint : ui::blue);
        g.fillEllipse((float)status.getX(), (float)status.getCentreY() - 3.0f, 7.0f, 7.0f);
        g.setColour(Daw::textDim);
        g.setFont(Daw::caps(9.0f));
        g.drawText(lastBypassed ? ui::loc("BYPASSED", "BYPASSATO") : ui::loc("ACTIVE", "ATTIVO"), status.withTrimmedLeft(13), juce::Justification::centredLeft);

        g.setColour(Daw::surface);
        g.fillRoundedRectangle(knobPanel.toFloat(), Daw::panelRadius);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(knobPanel.toFloat(), Daw::panelRadius, 1.0f);

        g.setColour(Daw::surface);
        g.fillRoundedRectangle(infoPanel.toFloat(), Daw::panelRadius);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(infoPanel.toFloat(), Daw::panelRadius, 1.0f);

        auto info = infoPanel.reduced(14, 10);
        g.setFont(Daw::caps(9.5f));
        g.setColour(Daw::textDim);
        g.drawText(ui::loc("SIGNAL", "SEGNALE"), info.removeFromTop(16).toNearestInt(), juce::Justification::centredLeft);
        g.setFont(Daw::font(Daw::body));
        auto row = [&](const juce::String& text) {
            g.setColour(ui::blue);
            g.fillEllipse((float)info.getX(), (float)info.getCentreY() - 2.0f, 4.0f, 4.0f);
            g.setColour(Daw::textDim);
            g.drawText(text, info.withTrimmedLeft(10), juce::Justification::centredLeft);
            info.removeFromTop(22);
        };
        row(ui::loc("STEREO LINKED", "STEREO COLLEGATO"));
        row(ui::loc("ZERO LATENCY", "ZERO LATENZA"));
        row(ui::loc("NO LOOKAHEAD", "SENZA LOOKAHEAD"));
        const double rate = processor.getSampleRate();
        g.setColour(Daw::textFaint);
        g.drawText(rate > 0.0 ? juce::String(rate / 1000.0, 1) + " kHz" : "--", info, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        auto header = area.removeFromTop(46);
        area.removeFromTop(8);
        reset.setBounds(header.removeFromRight(86).reduced(0, 8));

        const int bottomHeight = std::clamp((int)(area.getHeight() * 0.46), 210, 300);
        auto bottom = area.removeFromBottom(bottomHeight);
        area.removeFromBottom(10);
        knobPanel = area;

        const int columns = area.getWidth() >= 780 ? 6 : 3;
        const int rows = (6 + columns - 1) / columns;
        const int cellW = area.getWidth() / columns;
        const int cellH = area.getHeight() / rows;
        for (int i = 0; i < 6; ++i)
            knobs[(size_t)i]->setBounds(area.getX() + (i % columns) * cellW,
                                        area.getY() + (i / columns) * cellH, cellW, cellH);

        auto left = bottom;
        const int grWidth = std::clamp(bottom.getWidth() / 5, 150, 210);
        const int infoWidth = std::clamp(bottom.getWidth() / 6, 150, 220);
        left.removeFromRight(grWidth + infoWidth + 20);
        curve.setBounds(left);
        bottom.removeFromLeft(left.getWidth() + 10);
        reduction.setBounds(bottom.removeFromLeft(grWidth));
        bottom.removeFromLeft(10);
        infoPanel = bottom;
    }

private:
    void timerCallback() override
    {
        for (int i = 0; i < 6; ++i)
        {
            auto& slider = sliders[(size_t)i];
            auto& knob = *knobs[(size_t)i];
            if (!slider.isMouseButtonDown() && !knob.isDragging() && !knob.isEditing())
                slider.setValue(processor.parameters[(size_t)i]->plain(), juce::dontSendNotification);
            knob.refresh();
        }

        const auto values = processor.values();
        if (values != lastValues)
        {
            lastValues = values;
            curve.repaint();
        }

        const float target = std::clamp(processor.reduction.load(std::memory_order_relaxed), 0.0f, 30.0f);
        grShown = target > grShown ? target : grShown * 0.90f;
        reduction.setReduction(grShown);

        if (processor.bypassed != lastBypassed)
        {
            lastBypassed = processor.bypassed;
            repaint();
        }
    }

    Compressor& processor;
    std::array<juce::Slider, 6> sliders;
    std::array<std::unique_ptr<ui::Knob>, 6> knobs;
    std::array<std::function<juce::String(double)>, 6> formatters;
    ui::Curve curve{processor};
    ui::ReductionMeter reduction;
    ActionButton reset;
    juce::TooltipWindow tooltips{this, 700};
    juce::Rectangle<int> knobPanel, infoPanel;
    CompressorDSP::Values lastValues{};
    bool lastBypassed = false;
    float grShown = 0.0f;
};

juce::AudioProcessorEditor* Compressor::createEditor(){return new Editor(*this);}
}
juce::PluginDescription nativeCompressorDescription(){juce::PluginDescription d;d.name=d.descriptiveName="Native Compressor";d.pluginFormatName="FGA Native";d.category="Dynamics";d.manufacturerName="FaseGroove";d.version="1";d.fileOrIdentifier="fga:native-compressor";d.uniqueId=0x46474331;d.numInputChannels=d.numOutputChannels=2;return d;}
bool isNativeCompressor(const juce::PluginDescription& d){return d.pluginFormatName=="FGA Native"&&d.fileOrIdentifier=="fga:native-compressor"&&d.uniqueId==0x46474331&&!d.isInstrument;}
std::unique_ptr<juce::AudioPluginInstance> createNativeCompressor(){return std::make_unique<Compressor>();}
bool setNativeCompressorBypass(juce::AudioPluginInstance* plugin,bool bypassed)noexcept{if(auto* native=dynamic_cast<Compressor*>(plugin)){native->bypassed=bypassed;return true;}return false;}
float nativeCompressorReduction(const juce::AudioPluginInstance* plugin)noexcept{if(auto* native=dynamic_cast<const Compressor*>(plugin))return native->reduction.load(std::memory_order_relaxed);return 0;}
}
