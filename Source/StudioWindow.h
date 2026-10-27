// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace fga {
class MainComponent;
class StudioWindow : public juce::DocumentWindow {
public:
    StudioWindow(MainComponent&);
    ~StudioWindow() override;
    void closeButtonPressed()override;
    void open(int tab=0);
    bool verify(const juce::File& folder);
private:
    class Panel;
    MainComponent& owner;
    Panel* panel=nullptr;
};
}
