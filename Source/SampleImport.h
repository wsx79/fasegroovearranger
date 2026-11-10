// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
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
    std::atomic<double> progress{0};std::atomic<bool> ready{false};juce::WaitableEvent finish;
};
}
