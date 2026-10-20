// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace fga {
// Native, offline reader shared by the guide and reference documents.
std::unique_ptr<juce::Component> createGuidePage(bool english=false);
std::unique_ptr<juce::Component> createReferencePage(const juce::String& title,const juce::String& markdown);
bool verifyGuidePages(const juce::File& folder);
}
