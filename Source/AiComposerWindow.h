// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyControls.h"
namespace fga {
class MainComponent;
class AiComposerPanel;
class AiComposerWindow : public juce::DocumentWindow {
public:
    explicit AiComposerWindow(MainComponent&);
    ~AiComposerWindow()override;
    void closeButtonPressed()override;
    void show();
    bool verify(const juce::File&);
private:
    std::unique_ptr<AiComposerPanel> panel;
};
}
