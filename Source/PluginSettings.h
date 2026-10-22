// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
#include "PluginCatalog.h"
namespace fga {
class PluginSettings : public juce::Component, private juce::Timer {
public:
    explicit PluginSettings(PluginCatalog&);
    void show();
    void refreshText();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool verifyLayout() const;
private:
    PluginCatalog& catalog;
    juce::TextButton close,choose,clear,rescan,retry,cancel;
    juce::Label title,standardLabel,customLabel,folderPath,status,hint;
    juce::TextEditor standard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> card() const { return getLocalBounds().withSizeKeepingCentre(620,560); }
    void timerCallback() override;
    void showFolder();
};
}
