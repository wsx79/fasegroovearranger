// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "MixAnalysis.h"
#include "OfflineExport.h"
namespace fga {
struct MasteringTargets {
    bool useLufs=false,expectVocal=false,refineComparison=false;double lufs=-14,ceilingDb=-1,startA=0,startB=0;
    int expectedRate=0,expectedBits=0;
};
struct MasteringInput {juce::File file;bool vocal=false;};
struct MasteringFile {
    juce::File file;MixMeasure mix;double rate=0,truePeak=0;juce::int64 frames=0,bytes=0;
    int bits=0,channels=0;bool vocal=false;juce::Time modified;
};
struct MasteringReport {
    std::vector<MasteringFile> files; // mix, then stems; reference is separate
    MasteringFile reference;bool hasReference=false,qaPassed=true,comparisonReady=false;
    juce::StringArray qa,suggestions;juce::String comparisonNote;
    juce::AudioBuffer<float> a,b;double comparisonRate=0,matchedLufs=-100,gainA=1,gainB=1;
};
// Local WAV-only worker functions. 15-minute analysis; A/B contains at most 10 seconds per side.
juce::Result analyseMastering(const std::vector<MasteringInput>&,const juce::File& reference,
    const MasteringTargets&,MasteringReport&,const std::function<bool()>& cancelled,
    const std::function<void(double)>& progress);
bool masteringFilesUnchanged(const MasteringReport&); // worker only (filesystem)
}
