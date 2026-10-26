// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include <juce_core/juce_core.h>
namespace fga {
juce::var samplerJson(const SamplerSettings&);
juce::Result readSampler(const juce::var&,SamplerSettings&);
juce::var busesJson(const std::array<BusSettings,4>&);
juce::Result readBuses(const juce::var&,std::array<BusSettings,4>&);
bool extendedProject(const Project&);
juce::File sampleFile(const Track&,const juce::File& project={});
void resolveProjectMedia(Project&,const juce::File& project);
void relativiseProjectMedia(juce::var&,const juce::File& project);
juce::Result collectProjectMedia(const Project&,const juce::File& target,Project& collected,
    std::function<bool()> cancelled={},std::function<void(double)> progress={});
struct RelinkResult {int found=0;juce::StringArray missing;};
juce::Result relinkProjectMedia(Project&,const juce::File& folder,RelinkResult&,std::function<bool()> cancelled={});
struct SamplerPreset {SamplerSettings settings;int root=60;juce::File file;juce::String hash;};
juce::Result saveSamplerPreset(const SamplerPreset&,const juce::File&);
juce::Result loadSamplerPreset(const juce::File&,SamplerPreset&);
}
