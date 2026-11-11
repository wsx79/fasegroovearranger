// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace fga {
juce::PluginDescription nativeCompressorDescription();
bool isNativeCompressor(const juce::PluginDescription&);
std::unique_ptr<juce::AudioPluginInstance> createNativeCompressor();
// Audio side only: native bypass ramps through the existing insert processor.
bool setNativeCompressorBypass(juce::AudioPluginInstance*,bool)noexcept;
int nativeCompressorDetectorSource(const juce::AudioPluginInstance*)noexcept;
bool nativeCompressorRoutingChanged(const juce::AudioPluginInstance*,int targetLatency,int sourceLatency)noexcept;
void prepareNativeCompressorRouting(juce::AudioPluginInstance*,int targetLatency,int sourceLatency);
int nativeCompressorSource(const juce::AudioPluginInstance*)noexcept;
void setNativeCompressorDetector(juce::AudioPluginInstance*,const float*)noexcept;
void configureNativeCompressorEditor(juce::AudioPluginInstance*,const juce::StringArray&,std::function<void()>);
float nativeCompressorReduction(const juce::AudioPluginInstance*)noexcept;
}
