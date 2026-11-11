// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/StudioSettings.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
namespace fga {
class MixerBuses {
public:
    void prepare(double rate,int block);
    void reset();
    void configure(const std::array<BusSettings,4>&,double bpm,double master,bool snap,bool editableReturns=false);
    void begin(int count,bool stem=false);
    juce::AudioBuffer<float>& returnBuffer(int bus){return returns[(size_t)bus];}
    bool needsReturnDelay(int samples)const{return samples>0&&returnDelay.getNumSamples()<=samples;}
    void prepareReturnDelay(int samples){if(needsReturnDelay(samples)){int size=2;while(size<=samples)size*=2;returnDelay.setSize(2,size);returnDelay.clear();returnWrite=0;returnValid=0;}}
    void setReturnLatency(int samples){const int next=needsReturnDelay(samples)?0:samples;if(next!=returnLatency){returnWrite=0;returnValid=0;returnLatency=next;}}

    void add(int channel,int sample,float value,int group,const std::array<double,2>& sends);
    bool detects(int track)const{return track==(*settings)[0].sidechainSource||track==(*settings)[1].sidechainSource;}
    void source(int track,int sample,float value){
        if(sample<0||sample>=(int)sourceSignal[0].size()||!std::isfinite(value))return;
        for(int b=0;b<2;++b)if(track==(*settings)[(size_t)b].sidechainSource){auto& v=sourceSignal[(size_t)b][(size_t)sample];v=std::max(v,std::abs(value));}
    }
    void kick(int sample,float value){source(4,sample,value);}
    float reduction(int bus)const{return reductions[(size_t)std::clamp(bus,0,1)].load(std::memory_order_relaxed);}
    void finish(float* const* output,int channels,int count,const float* automation);
    float level(int bus)const{return peaks[(size_t)std::clamp(bus,0,3)].load();}
private:
    const std::array<BusSettings,4>* settings=nullptr;
    std::array<BusSettings,4> fallback=defaultBuses();
    std::array<juce::AudioBuffer<float>,2> groups,returns;
    juce::AudioBuffer<float> dry,delay,returnDelay;
    int returnLatency=0,returnWrite=0,returnValid=0;bool editableReturns=false;
    std::array<std::vector<float>,4> groupGain;
    std::array<std::array<juce::SmoothedValue<float>,2>,4> gains;
    juce::SmoothedValue<float> masterGain,delaySamples;
    juce::Reverb reverb;
    double rate=44100,bpm=120;
    std::array<std::vector<float>,2> sourceSignal;
    std::array<float,2> duck{},attack{},release{};
    std::array<juce::SmoothedValue<float>,2> depths,editOffsets;
    struct DetectorControls {int source=4;double attackMs=0,releaseMs=180,curve=1;};
    std::array<DetectorControls,2> detectorControls{};
    std::array<float,2> lastReduction{};
    std::array<bool,2> editPending{};
    std::array<std::atomic<float>,2> reductions{};
    int write=0;
    std::array<float,2> damping{};
    bool groupSolo=false,returnSolo=false,stem=false;
    std::array<std::atomic<float>,4> peaks{};
};
}
