// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fga {
constexpr size_t maxProjectNotes=2000000;
constexpr juce::int64 maxProjectBytes=256ll*1024*1024,maxMidiBytes=64ll*1024*1024;
juce::Result readBoundedJson(const juce::File&,juce::var&,juce::int64 maxBytes=maxProjectBytes);
juce::var projectJson(const Project&);
juce::Result projectFromJson(const juce::var&, Project&);
juce::Result saveProject(const Project&, const juce::File&);
juce::Result loadProject(const juce::File&, Project&);
juce::Result importMidi(const juce::File&, Clip&, double beats,size_t maxNotes=maxProjectNotes);
}
