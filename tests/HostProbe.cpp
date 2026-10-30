// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Test-only VST3. It is never linked into or distributed with the arranger.
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
class HostProbe final:public juce::AudioProcessor {
public:
    HostProbe():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true).withOutput("Part 2",juce::AudioChannelSet::stereo(),true)){
        addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID{"gain",1},"Gain",juce::NormalisableRange<float>(0,1),.25f));
    }
    const juce::String getName()const override{return "Arranger Host Probe";}
    void prepareToPlay(double rate,int)override{sampleRate=rate;phase.fill(0);velocity.fill(0);}
    void releaseResources()override{}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.outputBuses.size()==2&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainInputChannelSet().isDisabled()&&(b.outputBuses[1].isDisabled()||b.outputBuses[1]==juce::AudioChannelSet::stereo());}
    void processBlock(juce::AudioBuffer<float>& output,juce::MidiBuffer& midi)override{
        output.clear();auto* head=getPlayHead();if(!head)return;auto position=head->getPosition();
        if(!position||!position->getBpm()||!position->getIsPlaying())return;
        int cursor=0;
        auto renderTo=[&](int end){
            for(;cursor<end;++cursor){float value=0;for(int n=0;n<128;++n)if(velocity[(size_t)n]>0){
                value+=(float)std::sin(phase[(size_t)n])*velocity[(size_t)n]*gain->get()*.2f;
                phase[(size_t)n]+=juce::MathConstants<double>::twoPi*juce::MidiMessage::getMidiNoteInHertz(n)/sampleRate;
                if(phase[(size_t)n]>=juce::MathConstants<double>::twoPi)phase[(size_t)n]-=juce::MathConstants<double>::twoPi;
            }for(int ch=0;ch<output.getNumChannels();++ch)output.setSample(ch,cursor,value);}
        };
        for(auto event:midi){renderTo(std::clamp(event.samplePosition,0,output.getNumSamples()));auto m=event.getMessage();
            if(m.isNoteOn())velocity[(size_t)m.getNoteNumber()]=m.getFloatVelocity();
            if(m.isNoteOff())velocity[(size_t)m.getNoteNumber()]=0;
            if(m.isAllNotesOff()||m.isAllSoundOff())velocity.fill(0);
        }renderTo(output.getNumSamples());
    }
    bool acceptsMidi()const override{return true;}
    bool producesMidi()const override{return false;}
    bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 1;}
    int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Probe";}
    void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
    void getStateInformation(juce::MemoryBlock& dest)override{juce::MemoryOutputStream stream(dest,false);stream.writeFloat(gain->get());}
    void setStateInformation(const void* data,int size)override{if(size>=4){juce::MemoryInputStream stream(data,(size_t)size,false);gain->setValueNotifyingHost(stream.readFloat());}}
private:
    juce::AudioParameterFloat* gain=nullptr;
    double sampleRate=44100;
    std::array<double,128> phase{};
    std::array<float,128> velocity{};
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new HostProbe;}
