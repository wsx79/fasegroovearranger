// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "AudioAnalysis.h"
#include "LoopTempo.h"
#include "Sampler.h"
namespace fga {
// Owns preparation and retirement. UI reads results only after ready.acquire and
// signals finish after transferring ownership; no worker touches DAW/UI objects.
class SampleImportJob final:public juce::Thread {
public:
    SampleImportJob(juce::File,SamplerSettings,int root,bool drum,double rate,SampleLoadLimits,bool autoSlice,bool preset);
    void run()override;
    juce::File file;SamplerSettings settings;int root;bool drum;double rate;SampleLoadLimits limits;bool autoSlice,preset;
    std::shared_ptr<const SampleAsset> asset;
    std::unique_ptr<SampleSynth> prepared;
    std::unique_ptr<juce::Synthesiser> retired;
    juce::Result result=juce::Result::fail("Sample not prepared.");
    bool analyse=false;TempoEstimate tempo;KeyEstimate key;   // imported audio: tempo and key, measured on this worker
    bool detectTempo=false;LoopTempoInfo embedded;            // a new sample: its own tempo (file data or name), else measured
    double chosenBpm(double minimumConfidence=.3)const{return embedded.bpm>0?embedded.bpm:tempo.bpm>0&&tempo.confidence>=minimumConfidence?tempo.bpm:0.;}
    std::atomic<double> progress{0};std::atomic<bool> ready{false};juce::WaitableEvent finish;
};
}
