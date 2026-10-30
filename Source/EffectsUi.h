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
    void setTrackSession(std::uint64_t id);
    void finishNameEdit();
    void beginRename(int slot);
    int preferredHeight()const;
    void paint(juce::Graphics&)override;
    void resized()override;
private:
    friend class MainComponent;
    class Row;
    MainComponent& owner;
    int currentTrack()const;
    std::uint64_t trackSession=0;
    int dropPosition=-1;
    juce::TextButton add{"+"};
    ActionButton automation{"AUTOMATION"};
    std::vector<std::unique_ptr<Row>> rows;
};
class EffectChainWindow final : public juce::DocumentWindow, private juce::Timer {
public:
    explicit EffectChainWindow(MainComponent&);
    ~EffectChainWindow()override;
    void open(int track);
    void refresh();
    void closeButtonPressed()override;
    EffectChainView& chain(){return view;}
private:
    MainComponent& owner;
    EffectChainView view;
    std::uint64_t session=0;
    void timerCallback()override;
};
}
