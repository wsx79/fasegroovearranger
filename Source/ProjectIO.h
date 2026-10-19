// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fga {
juce::var projectJson(const Project&);
juce::Result projectFromJson(const juce::var&, Project&);
juce::Result saveProject(const Project&, const juce::File&);
juce::Result loadProject(const juce::File&, Project&);
juce::Result importMidi(const juce::File&, Clip&, double beats);
}
