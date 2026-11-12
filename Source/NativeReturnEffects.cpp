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
        if(isDelay){add("Delay (beats)",1,.125f,4);add("Feedback",.35f,0,.9f);add("Crossfeed",.5f);add("Damping",.5f);add("Dry / Wet",1.f);}
        else{add("Room",.5f);add("Damping",.5f);add("Dry / Wet",1.f);}
    }
    const juce::String getName()const override{return isDelay?"Tempo Delay":"Reverb";}
    void fillInPluginDescription(juce::PluginDescription& d)const override{d=nativeReturnDescription(isDelay);}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainInputChannelSet()==b.getMainOutputChannelSet()&&(b.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
    void prepareToPlay(double sampleRate,int block)override{rate=sampleRate;reverb.setSampleRate(rate);mix.reset(rate,.02);feedbackAmount.reset(rate,.02);crossAmount.reset(rate,.02);dampCoeff.reset(rate,.02);roomAmount.reset(rate,.02);absorbAmount.reset(rate,.02);if(isDelay){line.setSize(2,(int)std::ceil(rate*8)+block+4);time.reset(rate,.04);time.setCurrentAndTargetValue((float)(rate*.5));}else dryKeep.setSize(2,std::max(block,1));reset();}
    void releaseResources()override{}
    void reset()override{reverb.reset();line.clear();write=0;damping.fill(0);delayPrimed=false;mix.setCurrentAndTargetValue(parameters.back()->plain());if(isDelay){feedbackAmount.setCurrentAndTargetValue(parameters[1]->plain());crossAmount.setCurrentAndTargetValue(parameters[2]->plain());dampCoeff.setCurrentAndTargetValue(.03f+(1.f-parameters[3]->plain())*.97f);}else{roomAmount.setCurrentAndTargetValue(parameters[0]->plain());absorbAmount.setCurrentAndTargetValue(parameters[1]->plain());}for(auto& level:inLevel)level.store(0,std::memory_order_relaxed);for(auto& level:outLevel)level.store(0,std::memory_order_relaxed);}
    void processBlock(juce::AudioBuffer<float>& block,juce::MidiBuffer&)override{
        const int count=block.getNumSamples();if(block.getNumChannels()<1)return;for(int c=0;c<std::min(2,block.getNumChannels());++c)for(int n=0;n<count;++n)if(!std::isfinite(block.getSample(c,n)))block.setSample(c,n,0);
        if(!isDelay){const int channels=std::min(2,block.getNumChannels());float inPeak[2]{};for(int n=0;n<count;++n)for(int c=0;c<channels;++c){const float sample=block.getSample(c,n);if(std::isfinite(sample))inPeak[c]=std::max(inPeak[c],std::abs(sample));}if(channels<2)inPeak[1]=inPeak[0];roomAmount.setTargetValue(parameters[0]->plain());absorbAmount.setTargetValue(parameters[1]->plain());mix.setTargetValue(parameters.back()->plain());const bool unchanged=mix.getCurrentValue()>=1.f&&parameters.back()->plain()>=1.f;auto wetBlock=[&](int offset,int n){juce::Reverb::Parameters rp;rp.roomSize=roomAmount.getNextValue();if(n>1)roomAmount.skip(n-1);rp.damping=absorbAmount.getNextValue();if(n>1)absorbAmount.skip(n-1);rp.wetLevel=1;rp.dryLevel=0;rp.width=1;reverb.setParameters(rp);if(block.getNumChannels()>1)reverb.processStereo(block.getWritePointer(0)+offset,block.getWritePointer(1)+offset,n);else reverb.processMono(block.getWritePointer(0)+offset,n);};auto renderWet=[&]{for(int offset=0;offset<count;){const int n=std::min(32,count-offset);wetBlock(offset,n);offset+=n;}};if(unchanged)renderWet();else if(dryKeep.getNumSamples()>=count){for(int c=0;c<channels;++c)dryKeep.copyFrom(c,0,block,c,0,count);renderWet();for(int n=0;n<count;++n){const float amount=mix.getNextValue();for(int c=0;c<channels;++c){const float dry=dryKeep.getSample(c,n),wet=block.getSample(c,n);block.setSample(c,n,amount>=1.f?wet:dry*(1.f-amount)+wet*amount);}}}else renderWet();float outPeak[2]{};for(int n=0;n<count;++n)for(int c=0;c<channels;++c){const float sample=block.getSample(c,n);if(std::isfinite(sample))outPeak[c]=std::max(outPeak[c],std::abs(sample));}if(channels<2)outPeak[1]=outPeak[0];for(int c=0;c<2;++c){inLevel[c].store(inPeak[c],std::memory_order_relaxed);outLevel[c].store(outPeak[c],std::memory_order_relaxed);}return;}
        const double bpm=tempo();
        const int size=line.getNumSamples();if(size<2){block.clear();return;}
        const float target=(float)std::clamp(rate*60/bpm*parameters[0]->plain(),1.,(double)size-2);if(!delayPrimed){time.setCurrentAndTargetValue(target);delayPrimed=true;}else time.setTargetValue(target);
        feedbackAmount.setTargetValue(parameters[1]->plain());crossAmount.setTargetValue(parameters[2]->plain());dampCoeff.setTargetValue(.03f+(1.f-parameters[3]->plain())*.97f);
        mix.setTargetValue(parameters.back()->plain());
        for(int i=0;i<count;++i){const float feedback=feedbackAmount.getNextValue(),cross=crossAmount.getNextValue(),coefficient=dampCoeff.getNextValue();double read=std::fmod(write-(double)time.getNextValue(),(double)size);if(read<0)read+=size;const int a=(int)read,b=(a+1)%size;const float fraction=(float)(read-a);float wet[2];
            for(int c=0;c<2;++c){const auto* data=line.getReadPointer(c);wet[c]=data[a]+fraction*(data[b]-data[a]);damping[(size_t)c]+=(wet[c]-damping[(size_t)c])*coefficient;}
            const float amount=mix.getNextValue();const float dry0=block.getSample(0,i);const float dry1=block.getNumChannels()>1?block.getSample(1,i):dry0;
            for(int c=0;c<2;++c){const float input=c==0?dry0:dry1;line.setSample(c,write,(std::isfinite(input)?input:0)+feedback*((1-cross)*damping[(size_t)c]+cross*damping[(size_t)(1-c)]));}
            const float out0=amount>=1.f?wet[0]:dry0*(1.f-amount)+wet[0]*amount;const float out1=amount>=1.f?wet[1]:dry1*(1.f-amount)+wet[1]*amount;
            block.setSample(0,i,out0);if(block.getNumChannels()>1)block.setSample(1,i,out1);write=(write+1)%size;
        }
    }
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{if(!isDelay)return 10;const double feedback=parameters[1]->plain();return std::min(120.,parameters[0]->plain()*60/tempo()*(feedback>0?1+std::ceil(std::log(1e-5)/std::log(feedback)):1));}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
    bool delayDevice()const{return isDelay;}
    double currentBpm()const{return tempo();}
    std::function<void()> beforeEdit;
    void getStateInformation(juce::MemoryBlock& data)override{juce::XmlElement state("FgaReturn");state.setAttribute("version",1);state.setAttribute("delay",isDelay);for(size_t i=0;i<parameters.size();++i)state.setAttribute("p"+juce::String((int)i),(double)parameters[i]->getValue());copyXmlToBinary(state,data);}
    void setStateInformation(const void* data,int bytes)override{auto state=getXmlFromBinary(data,bytes);if(!state||!state->hasTagName("FgaReturn")||state->getIntAttribute("version")!=1||state->getBoolAttribute("delay")!=isDelay)return;for(size_t i=0;i<parameters.size();++i)if(state->hasAttribute("p"+juce::String((int)i)))parameters[i]->setValue((float)state->getDoubleAttribute("p"+juce::String((int)i)));}
private:
    double tempo()const{if(auto* head=getPlayHead())if(auto position=head->getPosition())if(auto value=position->getBpm())if(std::isfinite(*value)&&*value>0)return *value;return 120;}
    bool isDelay,delayPrimed=false;double rate=44100;std::vector<ReturnParameter*> parameters;juce::Reverb reverb;juce::AudioBuffer<float> line,dryKeep;juce::SmoothedValue<float> time,mix{1.f},feedbackAmount,crossAmount,dampCoeff,roomAmount,absorbAmount;int write=0;std::array<float,2> damping{};
public:std::atomic<float> inLevel[2]{},outLevel[2]{};
};
class ReturnEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit ReturnEditor(ReturnEffect& p):AudioProcessorEditor(p),processor(p),delay(p.delayDevice()),colour(Daw::returnColour(delay?1:0)){
        setLookAndFeel(&theme);
        for(int i=0;i<p.getParameters().size();++i){
            auto* parameter=p.getParameters()[i];auto slider=std::make_unique<juce::Slider>();auto label=std::make_unique<juce::Label>();auto* control=slider.get();
            addAndMakeVisible(*slider);addAndMakeVisible(*label);label->setText(tr(parameter->getName(64)),juce::dontSendNotification);label->setJustificationType(juce::Justification::centred);label->setFont(Daw::font(11.5f));label->setColour(juce::Label::textColourId,Daw::textDim);label->setMinimumHorizontalScale(0.7f);
            slider->setComponentID("native-return-parameter-"+juce::String(i));slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,96,20);slider->setRange(0,1,0);slider->setScrollWheelEnabled(false);slider->setDoubleClickReturnValue(true,parameter->getDefaultValue());
            slider->setColour(juce::Slider::rotarySliderFillColourId,(!delay&&i==2)||(delay&&i==4)?juce::Colour(0xffc9d1dc):colour);slider->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);slider->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);slider->setColour(juce::Slider::textBoxTextColourId,Daw::text);
            slider->textFromValueFunction=[parameter](double value){return parameter->getText((float)value,32)+" "+parameter->getLabel();};slider->valueFromTextFunction=[parameter](const juce::String& text){return parameter->getValueForText(text);};slider->setValue(parameter->getValue(),juce::dontSendNotification);
            slider->onDragStart=[this,parameter,i]{if(processor.beforeEdit)processor.beforeEdit();gestures[(size_t)i]=true;parameter->beginChangeGesture();};slider->onDragEnd=[this,parameter,i]{parameter->endChangeGesture();gestures[(size_t)i]=false;};
            slider->onValueChange=[this,parameter,control,i]{const bool discrete=!gestures[(size_t)i];if(discrete){if(processor.beforeEdit)processor.beforeEdit();parameter->beginChangeGesture();}parameter->setValueNotifyingHost((float)control->getValue());if(discrete)parameter->endChangeGesture();repaint(diagram);};
            sliders.push_back(std::move(slider));labels.push_back(std::move(label));
        }
        setResizable(true,false);setResizeLimits(740,440,1200,760);setSize(820,500);startTimerHz(20);
    }
    ~ReturnEditor()override{stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base);
        Daw::deviceHeader(g,{20,12,440,50},delay?"Tempo Delay":"Reverb",delay?"Echo / Time":"Space / Stereo",colour);
        auto badge=juce::Rectangle<int>(getWidth()-150,24,128,26);
        const int mixIndex=delay?4:2;const juce::String badgeText=sliders.size()>(size_t)mixIndex?juce::String(sliders[(size_t)mixIndex]->getValue()*100.0,0)+"% WET":"100% WET";
        Daw::drawCaps(g,badgeText,badge,Daw::textDim,juce::Justification::centredRight);
        Daw::devicePanel(g,diagram.toFloat(),colour);
        if(!tiles.empty()){auto strip=tiles.front().getUnion(tiles.back()).expanded(4,0);Daw::devicePanel(g,strip.toFloat(),{});for(size_t i=1;i<tiles.size();++i){g.setColour(juce::Colour(0xff222529));g.drawVerticalLine(tiles[i].getX()-4,(float)strip.getY()+16,(float)strip.getBottom()-16);}}
        drawDiagram(g);
        if(!delay)drawReverbReadout(g);
        g.setFont(Daw::font(11));g.setColour(Daw::textDim);g.drawFittedText(tr("Shared effect. Balance it with the common A / B return level."),getLocalBounds().removeFromBottom(28).reduced(16,0),juce::Justification::centredLeft,1);
    }
    void resized()override{
        if(delay){auto body=getLocalBounds().reduced(16).withTrimmedTop(64).withTrimmedBottom(28);diagram=body.removeFromTop(std::max(200,body.getHeight()*64/100));body.removeFromTop(8);tiles.clear();auto row=body;const int n=(int)sliders.size();for(int i=0;i<n;++i){auto tile=row.removeFromLeft(row.getWidth()/std::max(1,n-i)).reduced(4,0);tiles.push_back(tile);auto controls=tile.reduced(4,10);labels[(size_t)i]->setBounds(controls.removeFromTop(18));sliders[(size_t)i]->setBounds(controls.withSizeKeepingCentre(std::min(controls.getWidth(),110),controls.getHeight()));}return;}
        auto body=getLocalBounds().reduced(16).withTrimmedTop(64).withTrimmedBottom(28);auto knobs=body.removeFromBottom(std::clamp(body.getHeight()*32/100,128,160));body.removeFromBottom(8);diagram=body.removeFromLeft(body.getWidth()*58/100);body.removeFromLeft(10);readout=body;tiles.clear();auto row=knobs;const int n=(int)sliders.size();for(int i=0;i<n;++i){auto tile=row.removeFromLeft(row.getWidth()/std::max(1,n-i)).reduced(4,0);tiles.push_back(tile);auto controls=tile.reduced(6,10);labels[(size_t)i]->setBounds(controls.removeFromTop(18));sliders[(size_t)i]->setBounds(controls.withSizeKeepingCentre(std::min(controls.getWidth(),110),controls.getHeight()));}
    }
private:
    void drawDiagram(juce::Graphics& g){
        if(delay){drawDelay(g);return;}
        auto box=diagram.reduced(16);auto title=box.removeFromTop(18);Daw::drawCaps(g,tr("DECAY ENVELOPE"),title,Daw::textDim);
        box.removeFromTop(6);auto footer=box.removeFromBottom(18);
        auto plot=box.toFloat();Daw::deviceScreen(g,plot);plot=plot.reduced(10.f,8.f);plot.removeFromBottom(14.f);
        const float room=(float)sliders[0]->getValue(),damp=(float)sliders[1]->getValue();
        const float seconds=.4f+room*3.6f,shown=4.f;
        // Grid: time in seconds, level in dB.
        g.setFont(Daw::font(9.5f));
        for(int t=0;t<=4;++t){const float x=plot.getX()+plot.getWidth()*t/shown;g.setColour(juce::Colours::white.withAlpha(t==0?.12f:.05f));g.drawVerticalLine((int)x,plot.getY(),plot.getBottom());g.setColour(Daw::textFaint);g.drawText(juce::String(t)+" s",(int)x-14,(int)plot.getBottom()+2,28,12,juce::Justification::centred);}
        for(int d=0;d<=3;++d){const float y=plot.getY()+plot.getHeight()*d/3.f;g.setColour(juce::Colours::white.withAlpha(.05f));g.drawHorizontalLine((int)y,plot.getX(),plot.getRight());}
        auto envelope=[&](float hfLoss){juce::Path p;p.startNewSubPath(plot.getX(),plot.getBottom());const float pre=plot.getX()+plot.getWidth()*.02f;p.lineTo(pre,plot.getBottom());
            for(int i=0;i<=160;++i){const float t=shown*i/160.f;const float level=std::exp(-6.9f*t/(seconds*(1.f-hfLoss)));const float x=pre+(plot.getRight()-pre)*i/160.f;p.lineTo(x,plot.getBottom()-plot.getHeight()*.92f*level);}
            p.lineTo(plot.getRight(),plot.getBottom());p.closeSubPath();return p;};
        const auto full=envelope(0.f),high=envelope(.75f*damp);
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(.32f),plot.getX(),plot.getY(),colour.withAlpha(.03f),plot.getX(),plot.getBottom(),false));g.fillPath(full);
        g.setColour(colour);g.strokePath(full,juce::PathStrokeType(1.6f));
        g.setColour(Daw::text.withAlpha(.55f));const float dash[]{3.f,3.f};juce::Path dashed;juce::PathStrokeType(1.2f).createDashedStroke(dashed,high,dash,2);g.fillPath(dashed);
        auto legend=plot.toNearestInt().removeFromTop(14).withTrimmedRight(4);g.setFont(Daw::caps(9));g.setColour(Daw::textDim);g.drawText(tr("HF TAIL")+"  - -",legend,juce::Justification::centredRight);
        g.setColour(colour);g.drawText("RT60 ~ "+juce::String(seconds,1)+" s",legend.withTrimmedLeft(4),juce::Justification::centredLeft);
        g.setFont(Daw::font(10));g.setColour(Daw::textFaint);g.drawText(tr("Parameter schematic, not a measured response"),footer,juce::Justification::centredLeft);
    }
    void drawDelay(juce::Graphics& g){
        auto box=diagram.reduced(16).toFloat();
        const double beats=.125+sliders[0]->getValue()*3.875,bpm=processor.currentBpm();
        const float feedback=(float)sliders[1]->getValue()*.9f,cross=(float)sliders[2]->getValue(),damp=(float)sliders[3]->getValue();
        const float ms=(float)(beats*60000/bpm);
        auto caption=box.removeFromBottom(34);
        g.setColour(Daw::text);g.setFont(Daw::font(22,true));g.drawText(juce::String(ms,1)+" ms",box.removeFromTop(28).toNearestInt(),juce::Justification::centredLeft);
        g.setColour(colour);g.setFont(Daw::font(13,true));
        g.drawText(juce::String(beats,3)+" beats    "+juce::String(bpm,1)+" BPM    feedback "+juce::String(feedback*100.f,0)+"%",box.removeFromTop(20).toNearestInt(),juce::Justification::centredLeft);
        box.removeFromTop(6);const float span=4.f;
        Daw::deviceScreen(g,box);box=box.reduced(10.f,4.f);
        auto lane=[&](juce::Rectangle<float> row,bool right){
            g.setColour(juce::Colours::white.withAlpha(.1f));g.drawLine(row.getX(),row.getBottom()-8,row.getRight(),row.getBottom()-8,Daw::hairline());
            g.setColour(Daw::textDim);g.setFont(Daw::caps(10));g.drawText(right?"R":"L",(int)row.getX(),(int)row.getY(),16,16,juce::Justification::centredLeft);
            for(int n=0;n<=4;++n){const float at=row.getX()+24+(row.getWidth()-36)*(n/span);g.setColour(juce::Colours::white.withAlpha(.05f));g.drawVerticalLine((int)at,row.getY(),row.getBottom()-8);
                if(!right){g.setColour(Daw::textFaint);g.setFont(Daw::font(10.5f));g.drawText(juce::String(n),(int)at-8,(int)row.getBottom()-6,16,12,juce::Justification::centred);}}
            for(int n=0;n<8;++n){const float amp=std::pow(feedback,(float)n);if(amp<.04f)break;const float atBeat=(float)(n*beats);if(atBeat>span)break;const float mixAmt=n%2?cross:1.f-cross;const float height=(right?amp*mixAmt:amp)*(row.getHeight()-14);const float tone=std::pow(1.f-damp*.85f,(float)n);
                const float at=row.getX()+24+(row.getWidth()-36)*(atBeat/span);auto bar=juce::Rectangle<float>(at-1.5f,row.getBottom()-8-height,3,std::max(0.f,height));
                const auto ink=colour.interpolatedWith(Daw::textDim,1.f-tone);
                g.setColour(ink.withAlpha(.18f));g.fillRect(bar.expanded(3.f,0).withTrimmedTop(bar.getHeight()*.15f));
                g.setColour(ink.withAlpha(.35f+.65f*amp));g.fillRect(bar);g.fillEllipse(at-3.5f,bar.getY()-3.5f,7,7);}
        };
        auto top=box.removeFromTop(box.getHeight()*.5f);lane(top.reduced(0,4),false);lane(box.reduced(0,4),true);
        g.setColour(Daw::textDim);g.setFont(Daw::font(11));g.drawFittedText(tr("Tap diagram follows time, feedback and stereo exchange."),caption.removeFromTop(16).toNearestInt(),juce::Justification::centredLeft,1);
        g.setColour(Daw::textFaint);g.setFont(Daw::font(10));g.drawText(tr("Parameter schematic, not a measured response"),caption.toNearestInt(),juce::Justification::centredLeft);
    }
    void drawReverbReadout(juce::Graphics& g){
        if(readout.isEmpty())return;Daw::devicePanel(g,readout.toFloat(),colour);auto card=readout.reduced(12,10).toFloat();
        g.setColour(Daw::textDim);g.setFont(Daw::caps(10.5f));g.drawText("SPACE",card.removeFromTop(16).toNearestInt(),juce::Justification::centredLeft);
        auto line=[&](const juce::String& k,const juce::String& v){auto row=card.removeFromTop(34);g.setColour(Daw::textFaint);g.setFont(Daw::caps(10));g.drawText(k,row.removeFromTop(14).toNearestInt(),juce::Justification::centredLeft);g.setColour(colour.interpolatedWith(Daw::text,.5f));g.setFont(Daw::font(17.f,true));g.drawText(v,row.toNearestInt(),juce::Justification::centredLeft);};
        line("ROOM",juce::String(sliders[0]->getValue()*100.0,0)+"%");line("DAMPING",juce::String(sliders[1]->getValue()*100.0,0)+"%");
        auto meter=[&](juce::Rectangle<float> r,const juce::String& name,float linear){const float db=linear>1.e-8f?20.f*std::log10(linear):-48.f;g.setColour(Daw::textDim);g.setFont(Daw::caps(10));g.drawText(name,r.removeFromTop(14).toNearestInt(),juce::Justification::centred);g.setColour(Daw::text);g.setFont(Daw::font(12.f,true));g.drawText(linear>1.e-8f?juce::String(db,1):"--",r.removeFromBottom(16).toNearestInt(),juce::Justification::centred);auto bar=r.withSizeKeepingCentre(12.f,std::max(8.f,r.getHeight()-4));Daw::drawLedMeter(g,bar,std::clamp((db+48.f)/48.f,0.f,1.f));};
        auto meters=card.removeFromTop(std::min(130.f,card.getHeight()));meter(meters.removeFromLeft(meters.getWidth()*.5f),"IN",std::max(inShown[0],inShown[1]));meter(meters,"OUT",std::max(outShown[0],outShown[1]));
    }
    void timerCallback()override{
        bool changed=false;
        for(size_t i=0;i<sliders.size();++i){bool editing=gestures[i];for(auto* child:sliders[i]->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))editing=editing||label->isBeingEdited();const auto next=processor.getParameters()[(int)i]->getValue();if(!editing&&std::abs(sliders[i]->getValue()-next)>1.e-7){sliders[i]->setValue(next,juce::dontSendNotification);changed=true;}}
        const auto bpm=processor.currentBpm();if(delay&&std::abs(bpm-lastBpm)>.001){lastBpm=bpm;changed=true;}
        if(!delay){auto follow=[](float& shown,float target){shown=target>shown?target:shown*.86f;};for(int c=0;c<2;++c){follow(inShown[c],processor.inLevel[c].load(std::memory_order_relaxed));follow(outShown[c],processor.outLevel[c].load(std::memory_order_relaxed));}repaint(readout);}
        if(changed)repaint(diagram);
    }
    ReturnEffect& processor;bool delay;juce::Colour colour;DawLookAndFeel theme;double lastBpm=0;
    juce::Rectangle<int> diagram,readout;float inShown[2]{},outShown[2]{};std::vector<juce::Rectangle<int>> tiles;
    std::vector<std::unique_ptr<juce::Slider>> sliders;std::vector<std::unique_ptr<juce::Label>> labels;std::array<bool,5> gestures{};
};
juce::AudioProcessorEditor* ReturnEffect::createEditor(){return new ReturnEditor(*this);}
}
juce::PluginDescription nativeReturnDescription(bool delay){juce::PluginDescription d;d.name=d.descriptiveName=delay?"Tempo Delay":"Reverb";d.pluginFormatName="FGA Native";d.category=delay?"Delay":"Reverb";d.manufacturerName="FaseGroove";d.version="1";d.fileOrIdentifier=delay?"fga:native-delay":"fga:native-reverb";d.uniqueId=delay?0x46474431:0x46475231;d.numInputChannels=d.numOutputChannels=2;return d;}
void configureNativeReturnEditor(juce::AudioPluginInstance* plugin,std::function<void()> beforeEdit){if(auto* p=dynamic_cast<ReturnEffect*>(plugin))p->beforeEdit=std::move(beforeEdit);}
bool isNativeReturn(const juce::PluginDescription& d){return d.pluginFormatName=="FGA Native"&&!d.isInstrument&&((d.fileOrIdentifier=="fga:native-reverb"&&d.uniqueId==0x46475231)||(d.fileOrIdentifier=="fga:native-delay"&&d.uniqueId==0x46474431));}
std::unique_ptr<juce::AudioPluginInstance> createNativeReturn(const juce::PluginDescription& d){return isNativeReturn(d)?std::make_unique<ReturnEffect>(d.fileOrIdentifier=="fga:native-delay"):nullptr;}
}
