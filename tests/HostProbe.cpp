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
    void prepareToPlay(double rate,int)override{prepared=true;sampleRate=rate;phase.fill(0);velocity.fill(0);}
    void releaseResources()override{prepared=false;}
    bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.outputBuses.size()==2&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainInputChannelSet().isDisabled()&&(b.outputBuses[1].isDisabled()||b.outputBuses[1]==juce::AudioChannelSet::stereo());}
    void processBlock(juce::AudioBuffer<float>& output,juce::MidiBuffer& midi)override{
        output.clear();if(badStateOrder)return;auto* head=getPlayHead();if(!head)return;auto position=head->getPosition();
        if(!position||!position->getBpm()||!position->getIsPlaying())return;
        int cursor=0;
        auto renderTo=[&](int end){
            for(;cursor<end;++cursor){float value=0;for(int n=0;n<128;++n)if(velocity[(size_t)n]>0){
                value+=(float)std::sin(phase[(size_t)n])*velocity[(size_t)n]*gain->get()*.2f;
                phase[(size_t)n]+=juce::MathConstants<double>::twoPi*juce::MidiMessage::getMidiNoteInHertz(n)*std::pow(2.,program.load()==1?1.:program.load()==2?-1.:0.)/sampleRate;
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
    int getNumPrograms()override{return 3;}
    int getCurrentProgram()override{return program.load();}
    void setCurrentProgram(int value)override{program=std::clamp(value,0,2);}
    const juce::String getProgramName(int index)override{const char* names[]{"Probe","Bright","Soft"};return names[std::clamp(index,0,2)];}
    void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}
    juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
    void getStateInformation(juce::MemoryBlock& dest)override{juce::MemoryOutputStream stream(dest,false);stream.writeFloat(gain->get());}
    // Model instruments that require inactive processing during native-state restore.
    void setStateInformation(const void* data,int size)override{badStateOrder=prepared;if(size>=4){juce::MemoryInputStream stream(data,(size_t)size,false);gain->setValueNotifyingHost(stream.readFloat());}}
private:
    juce::AudioParameterFloat* gain=nullptr;
    std::atomic<int> program{0}; // intentionally not encoded in the native float chunk
    bool prepared=false,badStateOrder=false;
    double sampleRate=44100;
    std::array<double,128> phase{};
    std::array<float,128> velocity{};
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new HostProbe;}
