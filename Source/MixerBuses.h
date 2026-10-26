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
    void configure(const std::array<BusSettings,4>&,double bpm,double master,bool snap);
    void begin(int count,bool stem=false);
    void add(int channel,int sample,float value,int group,const std::array<double,2>& sends);
    void finish(float* const* output,int channels,int count,const float* automation);
    float level(int bus)const{return peaks[(size_t)std::clamp(bus,0,3)].load();}
private:
    const std::array<BusSettings,4>* settings=nullptr;
    std::array<BusSettings,4> fallback=defaultBuses();
    std::array<juce::AudioBuffer<float>,2> groups,returns;
    juce::AudioBuffer<float> dry,delay;
    std::array<std::vector<float>,4> groupGain;
    std::array<std::array<juce::SmoothedValue<float>,2>,4> gains;
    juce::SmoothedValue<float> masterGain,delaySamples;
    juce::Reverb reverb;
    double rate=44100,bpm=120;
    int write=0;
    std::array<float,2> damping{};
    bool groupSolo=false,returnSolo=false,stem=false;
    std::array<std::atomic<float>,4> peaks{};
};
}
