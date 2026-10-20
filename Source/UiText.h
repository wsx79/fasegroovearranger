// Copyright (C) 2026 Fabio 'Pitch' Serra, AGPL-3.0-or-later
#pragma once
#include "Localisation.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace fga {
juce::String controlTip(const juce::String& key);
void translateControls(juce::Component&);
}
