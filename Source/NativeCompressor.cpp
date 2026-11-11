// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NativeCompressor.h"
#include "CompressorDSP.h"
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
class Editor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit Editor(Compressor& processor):AudioProcessorEditor(processor),processor(processor){
        for(int i=0;i<6;++i){auto& slider=sliders[(size_t)i];auto& label=labels[(size_t)i];auto& p=*processor.parameters[(size_t)i];
            addAndMakeVisible(slider);addAndMakeVisible(label);label.setText(names[i],juce::dontSendNotification);
            slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,92,24);slider.setRange(p.range.start,p.range.end,.1);slider.setValue(p.plain(),juce::dontSendNotification);slider.setTextValueSuffix(" "+juce::String(units[i]));
            slider.setComponentID("native-compressor-"+juce::String(ids[i]));
            slider.onDragStart=[&p]{p.beginChangeGesture();};slider.onDragEnd=[&p]{p.endChangeGesture();};slider.onValueChange=[&p,&slider]{p.setValueNotifyingHost(p.normalized((float)slider.getValue()));};
        }
        addAndMakeVisible(meter);meter.setComponentID("native-compressor-reduction");setSize(460,260);startTimerHz(20);
    }
    void paint(juce::Graphics& g)override{g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));}
    void resized()override{auto area=getLocalBounds().reduced(12);for(int i=0;i<6;++i){auto row=area.removeFromTop(34);labels[(size_t)i].setBounds(row.removeFromLeft(90));sliders[(size_t)i].setBounds(row);}meter.setBounds(area);}
private:
    void timerCallback()override{for(int i=0;i<6;++i)if(!sliders[(size_t)i].isMouseButtonDown())sliders[(size_t)i].setValue(processor.parameters[(size_t)i]->plain(),juce::dontSendNotification);meter.setText("Gain reduction: "+juce::String(processor.reduction.load(std::memory_order_relaxed),1)+" dB",juce::dontSendNotification);}
    Compressor& processor;std::array<juce::Slider,6> sliders;std::array<juce::Label,6> labels;juce::Label meter;
};
juce::AudioProcessorEditor* Compressor::createEditor(){return new Editor(*this);}
}
juce::PluginDescription nativeCompressorDescription(){juce::PluginDescription d;d.name=d.descriptiveName="Native Compressor";d.pluginFormatName="FGA Native";d.category="Dynamics";d.manufacturerName="FaseGroove";d.version="1";d.fileOrIdentifier="fga:native-compressor";d.uniqueId=0x46474331;d.numInputChannels=d.numOutputChannels=2;return d;}
bool isNativeCompressor(const juce::PluginDescription& d){return d.pluginFormatName=="FGA Native"&&d.fileOrIdentifier=="fga:native-compressor"&&d.uniqueId==0x46474331&&!d.isInstrument;}
std::unique_ptr<juce::AudioPluginInstance> createNativeCompressor(){return std::make_unique<Compressor>();}
bool setNativeCompressorBypass(juce::AudioPluginInstance* plugin,bool bypassed)noexcept{if(auto* native=dynamic_cast<Compressor*>(plugin)){native->bypassed=bypassed;return true;}return false;}
float nativeCompressorReduction(const juce::AudioPluginInstance* plugin)noexcept{if(auto* native=dynamic_cast<const Compressor*>(plugin))return native->reduction.load(std::memory_order_relaxed);return 0;}
}
