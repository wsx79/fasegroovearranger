// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "AudioTake.h"
#include <functional>
namespace fga {
struct GuideVocalPlan {
    juce::MemoryBlock midi;
    double start=0,duration=0;
    std::vector<std::pair<double,double>> audible; // seconds relative to the rendered take
};
juce::Result guideVocalPlan(const Project&,int track,int clip,GuideVocalPlan&);
// Dormant render boundary. No files, processes or audio are produced without a future backend.
juce::Result renderGuideVocal(const Project&,int track,int clip,AudioTake&,
    const std::function<bool()>& cancelled,const std::function<void(double)>& progress);
}
