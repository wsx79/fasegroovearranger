// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyControls.h"
namespace fga {
class MainComponent;
class EffectChainView final : public juce::Component {
public:
    explicit EffectChainView(MainComponent&);
    ~EffectChainView() override;
    void refresh();
    void finishNameEdit();
    void beginRename(int slot);
    int preferredHeight()const;
    void paint(juce::Graphics&)override;
    void resized()override;
private:
    class Row;
    MainComponent& owner;
    int dropPosition=-1;
    juce::TextButton add{"+"};
    ActionButton automation{"AUTOMATION"};
    std::vector<std::unique_ptr<Row>> rows;
};
}
