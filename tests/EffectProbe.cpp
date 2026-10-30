// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Test-only native VST3: affine transform makes order and bypass measurable.
#include <juce_audio_utils/juce_audio_utils.h>
class EffectProbe final : public juce::AudioProcessor {
public:
    EffectProbe():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)){
        addParameter(gain=new juce::AudioParameterFloat("gain","Gain",0.f,2.f,1.f));addParameter(offset=new juce::AudioParameterFloat("offset","Offset",0.f,.1f,0.f));
    }
    const juce::String getName()const override{return "Arranger Effect Probe";}
    void prepareToPlay(double,int)override{}void releaseResources()override{}
    bool isBusesLayoutSupported(const BusesLayout& layout)const override{return layout.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{for(int c=0;c<buffer.getNumChannels();++c)for(int i=0;i<buffer.getNumSamples();++i)buffer.setSample(c,i,buffer.getSample(c,i)*gain->get()+offset->get());midi.clear();}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
    void getStateInformation(juce::MemoryBlock& data)override{const float values[]{gain->get(),offset->get()};data.replaceWith(values,sizeof(values));}
    void setStateInformation(const void* data,int size)override{if(size!=(int)(sizeof(float)*2))return;float values[2];std::memcpy(values,data,sizeof(values));gain->setValueNotifyingHost(gain->convertTo0to1(values[0]));offset->setValueNotifyingHost(offset->convertTo0to1(values[1]));}
private:juce::AudioParameterFloat *gain=nullptr,*offset=nullptr;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new EffectProbe;}
