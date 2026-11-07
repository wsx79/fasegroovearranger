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
juce::Result resolveProjectMedia(Project&,const juce::File& project);
void relativiseProjectMedia(juce::var&,const juce::File& project);
juce::Result collectProjectMedia(const Project&,const juce::File& target,Project& collected,
    std::function<bool()> cancelled={},std::function<void(double)> progress={});
struct RelinkResult {int found=0;juce::StringArray missing,unverified;};
juce::Result relinkProjectMedia(Project&,const juce::File& folder,RelinkResult&,std::function<bool()> cancelled={});
struct SamplerPreset {SamplerSettings settings;int root=60;juce::File file;juce::String hash;};
juce::Result saveSamplerPreset(const SamplerPreset&,const juce::File&);
juce::Result loadSamplerPreset(const juce::File&,SamplerPreset&);
struct RecoveryOffer { juce::File snapshot; juce::String originalPath; juce::int64 millis=0; };
juce::File recoveryDirectory();
void openRecoverySession(const juce::File& directory);
void closeRecoverySession(const juce::File& directory);
void clearRecoverySnapshots(const juce::File& directory);
juce::Result storeRecoverySnapshot(const Project&,const juce::String& originalPath,const juce::File& directory);
bool pendingRecovery(const juce::File& directory,RecoveryOffer&);
struct MissingResource { juce::String track,kind,path; };
void listMissingResources(const Project&,const juce::File& project,std::vector<MissingResource>&);
struct PortableReport { juce::File project; juce::StringArray externalPlugins; };
juce::Result packagePortableProject(const Project&,const juce::File& emptyFolder,PortableReport&);
}
