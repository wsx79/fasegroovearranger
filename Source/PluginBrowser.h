// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
#include "PluginCatalog.h"
namespace fga {
// Same browser as the FaseGroove groove menu: categories on the left, items on the right, search,
// favourites and recents. Manufacturers replace genres. FaseGroove engines stay the first group.
class PluginBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    explicit PluginBrowser(PluginCatalog&);
    void show(int engineFamily,const juce::String& currentKey);
    std::function<void(const PluginCatalog::BrowserRow&)> onChoose;
    std::function<void()> onRescan,onSettings,onOpenFile;
    void refreshText();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;
    bool verifyLayout() const;
private:
    struct Column : juce::Component { juce::OwnedArray<juce::TextButton> buttons; };
    static constexpr int favourites=0,recent=1,firstCategory=2,starWidth=30;
    PluginCatalog& catalog;
    Column column;
    juce::Viewport categories;
    juce::TextEditor search;
    juce::TextButton closeButton,rescanButton,settingsButton,openButton;
    juce::ListBox list;
    std::vector<PluginCatalog::BrowserRow> entries;
    juce::StringArray favouriteKeys;
    juce::String currentKey,listStamp;
    int paintedChecked=-1;
    int category=firstCategory,engineFamily=0;
    bool quiet=false;
    juce::Rectangle<float> card() const { return getLocalBounds().toFloat().withSizeKeepingCentre(920.f,640.f); }
    juce::StringArray loadList(const char*) const;
    void saveList(const char*,const juce::StringArray&) const;
    bool isFavourite(const PluginCatalog::BrowserRow&) const;
    void toggleFavourite(int row);
    void selectCategory(int);
    void rebuildList();
    void choose(int row);
    void timerCallback() override;
    int getNumRows() override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void selectedRowsChanged(int) override;
    void listBoxItemClicked(int,const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    void returnKeyPressed(int) override;
};
}
