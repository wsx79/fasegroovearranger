// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace fga {
juce::PluginDescription nativeReturnDescription(bool delay);
void configureNativeReturnEditor(juce::AudioPluginInstance*,std::function<void()>);
bool isNativeReturn(const juce::PluginDescription&);
std::unique_ptr<juce::AudioPluginInstance> createNativeReturn(const juce::PluginDescription&);
}
