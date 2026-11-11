// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "NativeReturnEffects.h"
#include "DawTheme.h"
#include "Localisation.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fga {
namespace {
// These devices are 100% wet, for parallel return chains. No external dependencies.
class ReturnParameter final : public juce::HostedAudioProcessorParameter {
public:
    ReturnParameter(juce::String name,float initial,float low=0,float high=1):name(std::move(name)),initial(initial),low(low),high(high){setValue((initial-low)/(high-low));}
    float plain()const{return low+value.load(std::memory_order_relaxed)*(high-low);}
    float getValue()const override{return value.load(std::memory_order_relaxed);}
    void setValue(float v)override{value.store(std::isfinite(v)?std::clamp(v,0.f,1.f):(initial-low)/(high-low),std::memory_order_relaxed);}
    float getDefaultValue()const override{return (initial-low)/(high-low);}
    juce::String getName(int)const override{return name;}
    juce::String getLabel()const override{return name=="Delay (beats)"?"beats":"%";}
    juce::String getParameterID()const override{return name;}
    juce::String getText(float v,int)const override{return juce::String((low+v*(high-low))*(name=="Delay (beats)"?1.f:100.f),name=="Delay (beats)"?3:1);}
    float getValueForText(const juce::String& v)const override{return std::clamp((v.replaceCharacter(',','.').getFloatValue()*(name=="Delay (beats)"?1.f:.01f)-low)/(high-low),0.f,1.f);}
private:
    juce::String name;float initial,low,high;std::atomic<float> value{0};
};
class ReturnEffect final : public juce::AudioPluginInstance {
public:
    explicit ReturnEffect(bool delay):AudioPluginInstance(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),isDelay(delay){
        auto add=[&](const char* name,float initial,float low=0,float high=1){auto p=std::make_unique<ReturnParameter>(name,initial,low,high);parameters.push_back(p.get());addHostedParameter(std::move(p));};
        if(isDelay){add("Delay (beats)",1,.125f,4);add("Feedback",.35f,0,.9f);add("Crossfeed",.5f);add("Damping",.5f);}
        else{add("Room",.5f);add("Damping",.5f);}
    }
    const juce::String getName()const override{return isDelay?"Tempo Delay":"Reverb";}
    void fillInPluginDescription(juce::PluginDescription& d)const override{d=nativeReturnDescription(isDelay);}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainInputChannelSet()==b.getMainOutputChannelSet()&&(b.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void prepareToPlay(double sampleRate,int block)override{rate=sampleRate;reverb.setSampleRate(rate);if(isDelay){line.setSize(2,(int)std::ceil(rate*8)+block+4);time.reset(rate,.04);time.setCurrentAndTargetValue((float)(rate*.5));}reset();}
    void releaseResources()override{}
    void reset()override{reverb.reset();line.clear();write=0;damping.fill(0);delayPrimed=false;}
    void processBlock(juce::AudioBuffer<float>& block,juce::MidiBuffer&)override{
        const int count=block.getNumSamples();if(block.getNumChannels()<1)return;for(int c=0;c<std::min(2,block.getNumChannels());++c)for(int n=0;n<count;++n)if(!std::isfinite(block.getSample(c,n)))block.setSample(c,n,0);
        if(!isDelay){juce::Reverb::Parameters p;p.roomSize=parameters[0]->plain();p.damping=parameters[1]->plain();p.wetLevel=1;p.dryLevel=0;p.width=1;reverb.setParameters(p);if(block.getNumChannels()>1)reverb.processStereo(block.getWritePointer(0),block.getWritePointer(1),count);else reverb.processMono(block.getWritePointer(0),count);return;}
        const double bpm=tempo();
        const int size=line.getNumSamples();if(size<2){block.clear();return;}
        const float target=(float)std::clamp(rate*60/bpm*parameters[0]->plain(),1.,(double)size-2);if(!delayPrimed){time.setCurrentAndTargetValue(target);delayPrimed=true;}else time.setTargetValue(target);
        const float feedback=parameters[1]->plain(),cross=parameters[2]->plain(),coefficient=.03f+(1-parameters[3]->plain())*.97f;
        for(int i=0;i<count;++i){double read=std::fmod(write-(double)time.getNextValue(),(double)size);if(read<0)read+=size;const int a=(int)read,b=(a+1)%size;const float fraction=(float)(read-a);float wet[2];
            for(int c=0;c<2;++c){const auto* data=line.getReadPointer(c);wet[c]=data[a]+fraction*(data[b]-data[a]);damping[(size_t)c]+=(wet[c]-damping[(size_t)c])*coefficient;}
            for(int c=0;c<2;++c){const float input=block.getSample(std::min(c,block.getNumChannels()-1),i);line.setSample(c,write,(std::isfinite(input)?input:0)+feedback*((1-cross)*damping[(size_t)c]+cross*damping[(size_t)(1-c)]));}
            block.setSample(0,i,wet[0]);if(block.getNumChannels()>1)block.setSample(1,i,wet[1]);write=(write+1)%size;
        }
    }
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{if(!isDelay)return 10;const double feedback=parameters[1]->plain();return std::min(120.,parameters[0]->plain()*60/tempo()*(feedback>0?1+std::ceil(std::log(1e-5)/std::log(feedback)):1));}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
    std::function<void()> beforeEdit;
    void getStateInformation(juce::MemoryBlock& data)override{juce::XmlElement state("FgaReturn");state.setAttribute("version",1);state.setAttribute("delay",isDelay);for(size_t i=0;i<parameters.size();++i)state.setAttribute("p"+juce::String((int)i),(double)parameters[i]->getValue());copyXmlToBinary(state,data);}
    void setStateInformation(const void* data,int bytes)override{auto state=getXmlFromBinary(data,bytes);if(!state||!state->hasTagName("FgaReturn")||state->getIntAttribute("version")!=1||state->getBoolAttribute("delay")!=isDelay)return;for(size_t i=0;i<parameters.size();++i)if(state->hasAttribute("p"+juce::String((int)i)))parameters[i]->setValue((float)state->getDoubleAttribute("p"+juce::String((int)i)));}
private:
    double tempo()const{if(auto* head=getPlayHead())if(auto position=head->getPosition())if(auto value=position->getBpm())if(std::isfinite(*value)&&*value>0)return *value;return 120;}
    bool isDelay,delayPrimed=false;double rate=44100;std::vector<ReturnParameter*> parameters;juce::Reverb reverb;juce::AudioBuffer<float> line;juce::SmoothedValue<float> time;int write=0;std::array<float,2> damping{};
};
class ReturnEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit ReturnEditor(ReturnEffect& p):AudioProcessorEditor(p),processor(p){
        for(int i=0;i<p.getParameters().size();++i){auto* parameter=p.getParameters()[i];auto slider=std::make_unique<juce::Slider>();auto label=std::make_unique<juce::Label>();auto* control=slider.get();
            addAndMakeVisible(*slider);addAndMakeVisible(*label);label->setText(tr(parameter->getName(64)),juce::dontSendNotification);label->setJustificationType(juce::Justification::centred);label->setFont(Daw::font(Daw::body,true));
            slider->setComponentID("native-return-parameter-"+juce::String(i));slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,120,22);slider->setRange(0,1,0);slider->setScrollWheelEnabled(false);slider->setDoubleClickReturnValue(true,parameter->getDefaultValue());
            slider->textFromValueFunction=[parameter](double value){return parameter->getText((float)value,32)+" "+parameter->getLabel();};slider->valueFromTextFunction=[parameter](const juce::String& text){return parameter->getValueForText(text);};slider->setValue(parameter->getValue(),juce::dontSendNotification);
            slider->onDragStart=[this,parameter,i]{if(processor.beforeEdit)processor.beforeEdit();gestures[(size_t)i]=true;parameter->beginChangeGesture();};slider->onDragEnd=[this,parameter,i]{parameter->endChangeGesture();gestures[(size_t)i]=false;};
            slider->onValueChange=[this,parameter,control,i]{const bool discrete=!gestures[(size_t)i];if(discrete){if(processor.beforeEdit)processor.beforeEdit();parameter->beginChangeGesture();}parameter->setValueNotifyingHost((float)control->getValue());if(discrete)parameter->endChangeGesture();};
            sliders.push_back(std::move(slider));labels.push_back(std::move(label));
        }setSize(520,sliders.size()>2?370:230);startTimerHz(20);
    }
    void paint(juce::Graphics& g)override{g.fillAll(Daw::base);g.setColour(Daw::text);g.setFont(Daw::font(22,true));g.drawText(processor.getName(),16,10,getWidth()-32,30,juce::Justification::centredLeft);g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small));g.drawText(tr("100% wet - use the send and return levels to mix"),16,42,getWidth()-32,20,juce::Justification::centredLeft);}
    void resized()override{auto area=getLocalBounds().reduced(16).withTrimmedTop(56);const int rows=sliders.size()>2?2:1;for(size_t i=0;i<sliders.size();++i){auto cell=juce::Rectangle<int>(area.getX()+((int)i%2)*area.getWidth()/2,area.getY()+((int)i/2)*area.getHeight()/rows,area.getWidth()/2,area.getHeight()/rows).reduced(6,3);labels[i]->setBounds(cell.removeFromTop(22));sliders[i]->setBounds(cell);}}
private:
    void timerCallback()override{for(size_t i=0;i<sliders.size();++i){bool editing=gestures[i];for(auto* child:sliders[i]->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))editing=editing||label->isBeingEdited();if(!editing)sliders[i]->setValue(processor.getParameters()[(int)i]->getValue(),juce::dontSendNotification);}}
    ReturnEffect& processor;std::vector<std::unique_ptr<juce::Slider>> sliders;std::vector<std::unique_ptr<juce::Label>> labels;std::array<bool,4> gestures{};
};
juce::AudioProcessorEditor* ReturnEffect::createEditor(){return new ReturnEditor(*this);}
}
juce::PluginDescription nativeReturnDescription(bool delay){juce::PluginDescription d;d.name=d.descriptiveName=delay?"Tempo Delay":"Reverb";d.pluginFormatName="FGA Native";d.category=delay?"Delay":"Reverb";d.manufacturerName="FaseGroove";d.version="1";d.fileOrIdentifier=delay?"fga:native-delay":"fga:native-reverb";d.uniqueId=delay?0x46474431:0x46475231;d.numInputChannels=d.numOutputChannels=2;return d;}
void configureNativeReturnEditor(juce::AudioPluginInstance* plugin,std::function<void()> beforeEdit){if(auto* p=dynamic_cast<ReturnEffect*>(plugin))p->beforeEdit=std::move(beforeEdit);}
bool isNativeReturn(const juce::PluginDescription& d){return d.pluginFormatName=="FGA Native"&&!d.isInstrument&&((d.fileOrIdentifier=="fga:native-reverb"&&d.uniqueId==0x46475231)||(d.fileOrIdentifier=="fga:native-delay"&&d.uniqueId==0x46474431));}
std::unique_ptr<juce::AudioPluginInstance> createNativeReturn(const juce::PluginDescription& d){return isNativeReturn(d)?std::make_unique<ReturnEffect>(d.fileOrIdentifier=="fga:native-delay"):nullptr;}
}
