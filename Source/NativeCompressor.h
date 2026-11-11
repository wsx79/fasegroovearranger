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
float nativeCompressorReduction(const juce::AudioPluginInstance*)noexcept;
}
