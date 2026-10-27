// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace fga {
class MainComponent;
class MixerRoutingWindow : public juce::DocumentWindow {
public:
    explicit MixerRoutingWindow(MainComponent&);
    ~MixerRoutingWindow()override;
    void open();
    void closeButtonPressed()override;
    bool verify(const juce::File&);
private:
    class Panel;Panel* panel=nullptr;
};
}
