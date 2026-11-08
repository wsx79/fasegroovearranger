// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace fga {
class MainComponent;

class GraphicEqWindow : public juce::DocumentWindow {
public:
    explicit GraphicEqWindow(MainComponent&);
    ~GraphicEqWindow() override;
    void open(int track = -1);
    void refresh();
    void closeButtonPressed() override;
private:
    MainComponent& owner;
    class Panel;
    Panel* panel = nullptr;
};
}
