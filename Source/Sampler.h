// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/StudioSettings.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
namespace fga {
struct SampleLoadLimits {
    std::int64_t fileBytes=512ll*1024*1024,decodedBytes=maxDecodedSampleBytes,totalBytes=2048ll*1024*1024;
    double seconds=maxSampleSeconds;
    void normalise();
};
std::int64_t sampleMemoryInUse(); // decoded file PCM + waveform peaks, including pending imports
struct SampleAsset {
    SampleAsset()=default;
    ~SampleAsset();
    SampleAsset(const SampleAsset&)=delete;
    SampleAsset& operator=(const SampleAsset&)=delete;
    std::int64_t reservedBytes=0;
    juce::File file;
    juce::String hash;
    juce::AudioBuffer<float> audio;
    double rate=44100;
    int frames=0;
    std::vector<std::array<float,2>> peaks;
    double seconds()const{return frames/rate;}
};
juce::Result readSampleAsset(const juce::File&,std::shared_ptr<const SampleAsset>&,
    SampleLoadLimits limits={},const std::function<bool()>& cancelled={},const std::function<void(double)>& progress={});
double snapSlicePoint(const SampleAsset&,double normalised,const SamplerSettings&,double projectBpm);
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
    // Internal, fixed-size event carried in the sampler MIDI stream so audio
    // regions and ordinary notes share exactly the same sample clock.
    static void addRegionEvent(juce::MidiBuffer&,int sample,int note,int velocity,double offset,double duration,double fadeIn=0,double fadeOut=0,bool reverse=false);
    static bool isRegionEvent(const unsigned char*,int size);
    void renderNextBlock(juce::AudioBuffer<float>&,const juce::MidiBuffer&,int start,int count);
    double regionOffset=-1,regionDuration=0,regionFadeIn=0,regionFadeOut=0;bool regionReverse=false; // only set while dispatching a region trigger
    int latency=0;
protected:
    juce::SynthesiserVoice* findFreeVoice(juce::SynthesiserSound*,int midiChannel,int midiNoteNumber,bool stealIfNoneAvailable)const override;
    void handleMidiEvent(const juce::MidiMessage&)override;
private:
    bool handleRegion(const unsigned char*,int size);
    std::shared_ptr<const SampleAsset> data;
    SamplerSettings fallback;
    const SamplerSettings* parameters=&fallback;
    int rootNote=60;bool drumTrack=false;double bpm=120;
};
}
