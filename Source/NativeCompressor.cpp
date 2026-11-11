// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NativeCompressor.h"
#include "CompressorDSP.h"
#include "core/Project.h"
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
class SourceParameter final : public juce::HostedAudioProcessorParameter {
public:
    int source()const{return (int)std::round(value.load(std::memory_order_relaxed)*maxTracks)-1;}
    float getValue()const override{return value.load(std::memory_order_relaxed);}
    void setValue(float v)override{value.store(std::isfinite(v)?std::round(std::clamp(v,0.f,1.f)*maxTracks)/maxTracks:0,std::memory_order_relaxed);}
    float getDefaultValue()const override{return 0;}
    juce::String getName(int)const override{return "Sidechain source";}
    juce::String getLabel()const override{return {};}
    juce::String getParameterID()const override{return "sidechainSource";}
    bool isDiscrete()const override{return true;}
    bool isAutomatable()const override{return false;}
    int getNumSteps()const override{return maxTracks+1;}
    juce::String getText(float v,int)const override{const int source=(int)std::round(v*maxTracks)-1;return source<0?"Self":"Track "+juce::String(source+1);}
    float getValueForText(const juce::String& text)const override{return text.equalsIgnoreCase("Self")?0:std::clamp(text.getTrailingIntValue(),0,maxTracks)/(float)maxTracks;}
private:std::atomic<float> value{0};
};
class Compressor final : public juce::AudioPluginInstance {
public:
    Compressor():AudioPluginInstance(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)){
        auto source=std::make_unique<SourceParameter>();sourceParameter=source.get();
        for(int i=0;i<6;++i){auto parameter=std::make_unique<CompressorParameter>(i);parameters[(size_t)i]=parameter.get();addHostedParameter(std::move(parameter));}
        addHostedParameter(std::move(source));
    }
    CompressorDSP::Values values()const{CompressorDSP::Values values;for(int i=0;i<6;++i)values[(size_t)i]=parameters[(size_t)i]->plain();return values;}
    const juce::String getName()const override{return "Native Compressor";}
    void fillInPluginDescription(juce::PluginDescription& description)const override{description=nativeCompressorDescription();}
    bool isBusesLayoutSupported(const BusesLayout& layout)const override{return layout.getMainInputChannelSet()==layout.getMainOutputChannelSet()&&(layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()||layout.getMainOutputChannelSet()==juce::AudioChannelSet::mono());}
    void prepareToPlay(double rate,int)override{dsp.prepare(rate,values());inputWrite=detectorWrite=inputValid=detectorValid=0;reduction=0;}
    void releaseResources()override{}
    void reset()override{dsp.reset();inputWrite=detectorWrite=inputValid=detectorValid=0;reduction=0;}
    void processBlock(juce::AudioBuffer<float>& block,juce::MidiBuffer&)override{
        if(block.getNumChannels()<1)return;
        const int count=block.getNumSamples();
        if(inputDelay>0)for(int n=0;n<count;++n){for(int c=0;c<std::min(2,block.getNumChannels());++c){inputLine.setSample(c,inputWrite,block.getSample(c,n));block.setSample(c,n,inputValid>=inputDelay?inputLine.getSample(c,(inputWrite-inputDelay+inputLine.getNumSamples())%inputLine.getNumSamples()):0);}inputWrite=(inputWrite+1)%inputLine.getNumSamples();inputValid=std::min(inputValid+1,inputLine.getNumSamples());}
        dsp.configure(values());dsp.setBypassed(bypassed);
        if(detector&&detectorDelay>0){for(int n=0;n<count;++n){detectorLine[(size_t)detectorWrite]=detector[n];const float delayed=detectorValid>=detectorDelay?detectorLine[(size_t)((detectorWrite-detectorDelay+(int)detectorLine.size())%(int)detectorLine.size())]:0;detectorWrite=(detectorWrite+1)%(int)detectorLine.size();detectorValid=std::min(detectorValid+1,(int)detectorLine.size());dsp.process(block.getWritePointer(0)+n,block.getNumChannels()>1?block.getWritePointer(1)+n:nullptr,1,&delayed);}}
        else dsp.process(block.getWritePointer(0),block.getNumChannels()>1?block.getWritePointer(1):nullptr,count,detector);
        reduction.store((float)dsp.reductionDb(),std::memory_order_relaxed);
    }
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock& data)override{juce::XmlElement state("FgaCompressor");state.setAttribute("version",1);for(int i=0;i<6;++i)state.setAttribute(ids[i],(double)parameters[(size_t)i]->plain());state.setAttribute("sidechainSource",sourceParameter->source());copyXmlToBinary(state,data);}
    void setStateInformation(const void* data,int bytes)override{auto state=getXmlFromBinary(data,bytes);if(!state||!state->hasTagName("FgaCompressor")||state->getIntAttribute("version")!=1)return;for(int i=0;i<6;++i)if(state->hasAttribute(ids[i]))parameters[(size_t)i]->setValue(parameters[(size_t)i]->normalized((float)state->getDoubleAttribute(ids[i])));sourceParameter->setValue((std::clamp(state->getIntAttribute("sidechainSource",-1),-1,maxTracks-1)+1)/(float)maxTracks);}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
    const float* detector=nullptr; // audio-side borrowed buffer, reset by host before each process call
    SourceParameter* sourceParameter=nullptr;
    int routedSource=-1,inputDelay=0,detectorDelay=0,inputWrite=0,detectorWrite=0,inputValid=0,detectorValid=0;
    juce::AudioBuffer<float> inputLine;std::vector<float> detectorLine;
    static int boundedDelay(int value){return std::clamp(value,0,3*(1<<18));}
    bool routingChanged(int target,int source)const{return routedSource!=sourceParameter->source()||inputDelay!=(sourceParameter->source()<0?0:boundedDelay(source-target))||detectorDelay!=(sourceParameter->source()<0?0:boundedDelay(target-source));}
    void prepareRouting(int target,int source){
        if(!routingChanged(target,source))return;routedSource=sourceParameter->source();
        const int input=routedSource<0?0:boundedDelay(source-target),detection=routedSource<0?0:boundedDelay(target-source);
        // Keep the audio/detector history when only the source identity changes.
        // The new source enters the existing aligned timeline without a silence gap.
        if(input!=inputDelay){inputDelay=input;inputLine.setSize(input>0?2:0,input>0?input+1:0);inputWrite=inputValid=0;}
        if(detection!=detectorDelay){detectorDelay=detection;detectorLine.resize(detection>0?(size_t)detection+1:0);detectorWrite=detectorValid=0;}
        setLatencySamples(inputDelay);
    }

    juce::StringArray sourceNames;std::function<void()> beforeSourceEdit; // editor/message thread only
    std::atomic<bool> bypassed{false};std::atomic<float> reduction{0};
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
        auto area = getLocalBounds().toFloat().reduced(5.f,4.f);
        Daw::devicePanel(g,area,accent);area=area.reduced(8.f,6.f);
        auto title = area.removeFromTop(18.0f);
        g.setFont(Daw::caps(10.0f));
        g.setColour(accent);
        g.drawText(name.toUpperCase(), title.toNearestInt(), juce::Justification::centred);

        area.removeFromBottom(32.0f);
        const float margin = area.getHeight() < 120.0f ? 18.0f : 42.0f;
        const float diameter = std::max(30.0f, std::min(area.getWidth(), area.getHeight()) - margin);
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
        g.setColour(accent.withAlpha(.30f));
        g.drawEllipse(centre.x - radius + 3.0f, centre.y - radius + 3.0f, (radius - 3.0f)*2, (radius - 3.0f)*2, 3.f);
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
        Daw::devicePanel(g,bounds,ui::blue);

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
        Daw::devicePanel(g,bounds,ui::blue);

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
            slider.onDragStart = [this,&parameter] { if(!resetting&&processor.beforeSourceEdit)processor.beforeSourceEdit();parameterGesture=true;parameter.beginChangeGesture(); };
            slider.onDragEnd = [this,&parameter] { parameter.endChangeGesture();parameterGesture=false; };
            slider.onValueChange = [this,&parameter, &slider] { if(!parameterGesture&&!resetting&&processor.beforeSourceEdit)processor.beforeSourceEdit();parameter.setValueNotifyingHost(parameter.normalized((float)slider.getValue())); };
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
            if(processor.beforeSourceEdit)processor.beforeSourceEdit();
            resetting=true;
            for (int i = 0; i < 6; ++i)
            {
                auto& slider = sliders[(size_t)i];
                if (slider.onDragStart) slider.onDragStart();
                slider.setValue(CompressorDSP::defaults[(size_t)i], juce::sendNotificationSync);
                if (slider.onDragEnd) slider.onDragEnd();
            }
            processor.sourceParameter->beginChangeGesture();processor.sourceParameter->setValueNotifyingHost(0);processor.sourceParameter->endChangeGesture();resetting=false;
        };

        addAndMakeVisible(source);addAndMakeVisible(sourceLabel);
        sourceLabel.setText(ui::loc("SIDECHAIN SOURCE", "SORGENTE SIDECHAIN"),juce::dontSendNotification);
        sourceLabel.setFont(Daw::caps(9.5f));
        source.addItem(ui::loc("Self (internal)", "Segnale interno"),1);
        for(int i=0;i<processor.sourceNames.size();++i)source.addItem(processor.sourceNames[i],i+2);
        if(processor.sourceParameter->source()>=processor.sourceNames.size())source.addItem(ui::loc("Missing track", "Traccia assente"),processor.sourceParameter->source()+2);
        source.setSelectedId(processor.sourceParameter->source()+2,juce::dontSendNotification);
        source.setComponentID("native-compressor-source");source.setScrollWheelEnabled(false);
        source.setTooltip(ui::loc("Source before track inserts and fader, with latency alignment. Track mute disables the detector; group mute does not.", "Sorgente prima degli effetti e del volume, allineata per latenza. Il mute della traccia disattiva il detector; quello del gruppo no."));
        source.onChange=[this]{if(source.getSelectedId()<1)return;if(processor.beforeSourceEdit)processor.beforeSourceEdit();auto& p=*processor.sourceParameter;p.beginChangeGesture();p.setValueNotifyingHost((source.getSelectedId()-1)/(float)maxTracks);p.endChangeGesture();};
        lastValues = processor.values();
        lastBypassed = processor.bypassed;
        setResizable(true, true);
        setResizeLimits(720, 600, 1600, 1000);
        setSize(1040, 650);
        startTimerHz(30);
    }

    ~Editor() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Daw::base);

        Daw::deviceHeader(g,{18,8,430,50},ui::loc("COMPRESSOR", "COMPRESSORE"),"DYNAMICS",ui::blue);
        Daw::devicePanel(g,sourcePanel.toFloat(),Daw::returnA);

        const auto status = juce::Rectangle<int>(getWidth() - 250, 18, 150, 24);
        g.setColour(lastBypassed ? Daw::textFaint : ui::blue);
        g.fillEllipse((float)status.getX(), (float)status.getCentreY() - 3.0f, 7.0f, 7.0f);
        g.setColour(Daw::textDim);
        g.setFont(Daw::caps(9.0f));
        g.drawText(lastBypassed ? ui::loc("BYPASSED", "BYPASSATO") : ui::loc("ACTIVE", "ATTIVO"), status.withTrimmedLeft(13), juce::Justification::centredLeft);

        Daw::devicePanel(g,infoPanel.toFloat(),ui::blue);

        auto info = infoPanel.reduced(14, 10);
        g.setFont(Daw::caps(9.5f));
        g.setColour(Daw::textDim);
        g.drawText(ui::loc("SIGNAL", "SEGNALE"), info.removeFromTop(16).toNearestInt(), juce::Justification::centredLeft);
        g.setFont(Daw::font(Daw::body));
        auto row = [&](const juce::String& text) {
            const auto line = info.removeFromTop(22);
            g.setColour(ui::blue);
            g.fillEllipse((float)line.getX(), (float)line.getCentreY() - 2.0f, 4.0f, 4.0f);
            g.setColour(Daw::textDim);
            g.drawText(text, line.withTrimmedLeft(10), juce::Justification::centredLeft);
        };
        row(ui::loc("STEREO LINKED", "STEREO COLLEGATO"));
        row(processor.getLatencySamples()==0?ui::loc("ZERO LATENCY", "ZERO LATENZA"):ui::loc("ROUTING", "COMPENSAZIONE")+" "+juce::String(processor.getLatencySamples())+" smp");
        row(ui::loc("NO LOOKAHEAD", "SENZA LOOKAHEAD"));
        const double rate = processor.getSampleRate();
        info.removeFromTop(12);
        g.setColour(Daw::textFaint);
        g.drawText(ui::loc("Sample rate", "Frequenza") + ": " + (rate > 0.0 ? juce::String(rate / 1000.0, 1) + " kHz" : "--"), info.removeFromTop(22), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        auto header = area.removeFromTop(48);
        area.removeFromTop(10);sourcePanel=area.removeFromTop(42);auto sourceRow=sourcePanel.reduced(12,6);sourceLabel.setBounds(sourceRow.removeFromLeft(175));source.setBounds(sourceRow.removeFromLeft(std::min(400,sourceRow.getWidth())));
        area.removeFromTop(8);
        reset.setBounds(header.removeFromRight(86).reduced(0, 8));

        // The transfer curve and live reduction are the primary display;
        // tactile parameter tiles sit below, consistently at all window sizes.
        const int columns = area.getWidth() >= 780 ? 6 : 3;
        const int displayHeight = columns==6?std::clamp((int)(area.getHeight()*.52),220,310):180;
        auto bottom = area.removeFromTop(displayHeight);
        area.removeFromTop(12);
        knobPanel = area;

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

        if(processor.getLatencySamples()!=lastLatency){lastLatency=processor.getLatencySamples();repaint(infoPanel);}
        if(!source.isPopupActive())source.setSelectedId(processor.sourceParameter->source()+2,juce::dontSendNotification);
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
    int lastLatency=0;juce::ComboBox source;juce::Label sourceLabel;bool parameterGesture=false,resetting=false;
    std::array<juce::Slider, 6> sliders;
    std::array<std::unique_ptr<ui::Knob>, 6> knobs;
    std::array<std::function<juce::String(double)>, 6> formatters;
    ui::Curve curve{processor};
    ui::ReductionMeter reduction;
    ActionButton reset;
    juce::TooltipWindow tooltips{this, 700};
    juce::Rectangle<int> knobPanel, infoPanel, sourcePanel;
    CompressorDSP::Values lastValues{};
    bool lastBypassed = false;
    float grShown = 0.0f;
};

juce::AudioProcessorEditor* Compressor::createEditor(){return new Editor(*this);}
}
juce::PluginDescription nativeCompressorDescription(){juce::PluginDescription d;d.name=d.descriptiveName="Native Compressor";d.pluginFormatName="FGA Native";d.category="Dynamics";d.manufacturerName="FaseGroove";d.version="2";d.fileOrIdentifier="fga:native-compressor";d.uniqueId=0x46474331;d.numInputChannels=d.numOutputChannels=2;return d;}
bool isNativeCompressor(const juce::PluginDescription& d){return d.pluginFormatName=="FGA Native"&&d.fileOrIdentifier=="fga:native-compressor"&&d.uniqueId==0x46474331&&!d.isInstrument;}
std::unique_ptr<juce::AudioPluginInstance> createNativeCompressor(){return std::make_unique<Compressor>();}
bool setNativeCompressorBypass(juce::AudioPluginInstance* plugin,bool bypassed)noexcept{if(auto* native=dynamic_cast<Compressor*>(plugin)){native->bypassed=bypassed;return true;}return false;}
int nativeCompressorDetectorSource(const juce::AudioPluginInstance* plugin)noexcept{if(auto* p=dynamic_cast<const Compressor*>(plugin))return p->routedSource;return -1;}
bool nativeCompressorRoutingChanged(const juce::AudioPluginInstance* plugin,int target,int source)noexcept{if(auto* p=dynamic_cast<const Compressor*>(plugin))return p->routingChanged(target,source);return false;}
void prepareNativeCompressorRouting(juce::AudioPluginInstance* plugin,int target,int source){if(auto* p=dynamic_cast<Compressor*>(plugin))p->prepareRouting(target,source);}
int nativeCompressorSource(const juce::AudioPluginInstance* plugin)noexcept{if(auto* p=dynamic_cast<const Compressor*>(plugin))return p->sourceParameter->source();return -1;}
void setNativeCompressorDetector(juce::AudioPluginInstance* plugin,const float* signal)noexcept{if(auto* p=dynamic_cast<Compressor*>(plugin))p->detector=signal;}
void configureNativeCompressorEditor(juce::AudioPluginInstance* plugin,const juce::StringArray& names,std::function<void()> beforeEdit){if(auto* p=dynamic_cast<Compressor*>(plugin)){p->sourceNames=names;p->beforeSourceEdit=std::move(beforeEdit);}}
float nativeCompressorReduction(const juce::AudioPluginInstance* plugin)noexcept{if(auto* native=dynamic_cast<const Compressor*>(plugin))return native->reduction.load(std::memory_order_relaxed);return 0;}
}
