// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
#include "PluginCatalog.h"
namespace fga {
class PluginSettings : public juce::Component,private juce::Timer,private juce::ListBoxModel {
public:
    explicit PluginSettings(PluginCatalog&);
    void show();
    void refreshText();
    void paint(juce::Graphics&)override;
    void resized()override;
    bool verifyLayout()const;
private:
    PluginCatalog& catalog;
    juce::TextButton close,add,remove,rescan,retry,cancel;
    juce::Label title,standardLabel,customLabel,status,hint;
    juce::TextEditor standard;
    juce::ListBox list;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray folders;
    juce::Rectangle<int> card()const{return getLocalBounds().withSizeKeepingCentre(620,620);}
    void timerCallback()override;
    int getNumRows()override{return folders.size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool)override;
};
}
