// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "EqAnalyzer.h"
#include "LoudnessMeter.h"
#include <juce_audio_formats/juce_audio_formats.h>
namespace fga {
struct MixInput {juce::File file;std::uint64_t session=0;int role=0;}; // role: 0 mix, 1 kick, 2 bass, 3 other stem
struct MixMeasure {double peak=0,rms=0,correlation=0,lufs=-100;std::uint64_t fullScale=0;bool correlationValid=false;std::array<double,6> bandPower{};double truePeak=0;};
struct MixAdvice {juce::String text;std::uint64_t session=0;double trimDb=0;}; // 0 session = master; zero trim = listening advice only
struct MixReport {MixMeasure mix;std::vector<MixAdvice> suggestions;std::vector<MixMeasure> inputLevels;double previewStart=0;};
juce::Result analyseMixFiles(const std::vector<MixInput>&,MixReport&,const std::function<bool()>& cancelled,const std::function<void(double)>& progress = {});
}
