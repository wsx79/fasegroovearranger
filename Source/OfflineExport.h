// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
namespace fga {
enum class AudioExportMode { transparent, loud, legacy };
inline constexpr double minExportCeilingDb=-12.,maxExportCeilingDb=-1.;
// Candidate is exercised by offline QA; never presented as ready without its quality gate.
inline constexpr bool offlineLoudReady=false;
struct AudioExportOptions {
    double sampleRate=0;int bitDepth=24;bool loudnessGuard=false;
    AudioExportMode mode=AudioExportMode::transparent;double ceilingDb=-1.;
    AudioExportOptions()=default;
    // Preserve the historic positional API, including guard on/off.
    AudioExportOptions(double rate,int bits,bool guard=false):sampleRate(rate),bitDepth(bits),loudnessGuard(guard),mode(AudioExportMode::legacy){}
    AudioExportOptions(double rate,int bits,AudioExportMode selected,double ceiling=-1.):sampleRate(rate),bitDepth(bits),mode(selected),ceilingDb(ceiling){}
};
struct OfflineExportStats {double inputPeak=0,outputPeak=0,gain=1;juce::int64 frames=0;};
// Read-only worker QA; shares the exact existing 16x export estimator. No encoding/state changes.
juce::Result measureOfflineTruePeak(juce::AudioFormatReader&,double& linearPeak,
    const std::function<bool()>& cancelled,const std::function<void(double)>& progress);
// Worker only. Source is stereo float WAV; output remains a private staging file until success.
juce::Result finishOfflineExport(const juce::File& source,const juce::File& output,int bits,
    AudioExportMode mode,double ceilingDb,const std::function<bool()>& cancelled,
    const std::function<void(double)>& progress,OfflineExportStats* stats=nullptr);
}
