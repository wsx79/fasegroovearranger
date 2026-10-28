// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/StudioSettings.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
namespace fga {
struct SampleAsset {
    juce::File file;
    juce::String hash;
    juce::AudioBuffer<float> audio;
    double rate=44100;
    int frames=0;
    std::vector<std::array<float,2>> peaks;
    double seconds()const{return frames/rate;}
};
juce::Result readSampleAsset(const juce::File&,std::shared_ptr<const SampleAsset>&);
double snapSamplePoint(const SampleAsset&,double normalised);
std::vector<double> detectSampleSlices(const SampleAsset&,double start,double end,double sensitivity,int limit=128);
class SampleSynth : public juce::Synthesiser {
public:
    SampleSynth(std::shared_ptr<const SampleAsset>,const SamplerSettings&,int root,bool drum);
    ~SampleSynth()override;
    bool configure(const SamplerSettings&,int root,double bpm); // audio thread, copies a fixed-size settings snapshot (no allocation)
    const SamplerSettings& settings()const{return *parameters;}
    const SampleAsset& asset()const{return *data;}
    std::shared_ptr<const SampleAsset> sharedAsset()const{return data;}
    int root()const{return rootNote;}
    double tempo()const{return bpm;}
    int mode()const{return parameters->mode<0?(drumTrack?1:0):parameters->mode;}
    int latencySamples()const{return parameters->sync?latency:0;}
    void setCurrentPlaybackSampleRate(double)override;
    void noteOn(int channel,int note,float velocity)override;
    int latency=0;
private:
    std::shared_ptr<const SampleAsset> data;
    SamplerSettings fallback;
    const SamplerSettings* parameters=&fallback;
    int rootNote=60;bool drumTrack=false;double bpm=120;
};
}
