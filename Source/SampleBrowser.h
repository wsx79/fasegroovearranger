// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyControls.h"
#include "SampleLibrary.h"
#include <atomic>
#include <memory>
namespace fga {
class AudioEngine;
// The arranger's sample browser, docked beside the arrangement: saved folders and favourites,
// breadcrumb with back/forward/up, search, type filter and sorting, a virtualised file list,
// file info, preview, and loading into a track's sampler (double-click, Enter, LOAD or drag
// onto a track). Folders are listed and audio headers read on a background thread; nothing
// runs while the browser is idle.
class SampleBrowser : public juce::Component, private juce::Timer {
public:
    explicit SampleBrowser(AudioEngine&);
    ~SampleBrowser() override;
    std::function<void(const juce::File&)> onLoad;       // into the selected track's sampler
    std::function<void(const juce::String&,bool)> onMessage;
    std::function<juce::String()> targetName;             // the selected track, for LOAD's tip
    static constexpr const char* dragPrefix="fga-sample:";
    void refreshText();
    void refreshTarget();
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;
    // Model-level access for the smoke test.
    void navigate(const juce::File&,bool remember=true);
    juce::File currentFolder() const {return folder;}
    bool addFolder(const juce::File&);
    void removeFolder(const juce::String& path);
    void toggleFavourite(const juce::String& path);
    const samples::Preferences& preferences() const {return prefs;}
    bool waitForListing(int milliseconds); // smoke test only: pumps the message loop
    int visibleRows() const {return (int)rows.size();}
    juce::File fileAt(int row) const;
    void selectRow(int row);
    void setSearch(const juce::String&);
    void setFilter(int);
    void setSort(int,bool ascending);
    void back();
    void forward();
    void up();
    bool canBack() const {return history.canBack();}
    bool canForward() const {return history.canForward();}
    void preview(const juce::File&);
    void stopPreview();
    bool loadSelected();
    juce::String infoText() const {return info;}
    bool infoIsError() const {return infoError;}
    juce::StringArray crumbNames() const;
    bool verifyLayout() const;
private:
    struct Shared;           // state the background threads keep alive
    class Places;            // saved folders and favourites
    class Files;             // the virtualised list
    class Crumbs;            // clickable breadcrumb
    AudioEngine& engine;
    std::shared_ptr<Shared> shared;
    samples::Preferences prefs;
    samples::History history;
    juce::File folder;
    std::vector<samples::Entry> entries;
    std::vector<int> rows;
    bool reading=false,available=true;
    juce::String listingError,info;
    bool infoError=false;
    juce::File previewFile;
    std::unique_ptr<Places> places;
    std::unique_ptr<Files> files;
    std::unique_ptr<Crumbs> crumbs;
    juce::ListBox placeList,fileList;
    juce::TextEditor search;
    juce::ComboBox filterBox,sortBox;
    juce::TextButton addButton,orderButton,autoButton,loadButton,upButton;
    IconButton backButton{IconButton::Icon::left},forwardButton{IconButton::Icon::right},playButton{IconButton::Icon::play};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> infoArea,headerArea;
    bool quiet=false;
    double previewDeadline=0;
    std::unique_ptr<juce::ThreadPool> pool; // listing, audio info and preview decoding
    void savePreferences();
    void rebuildRows(bool keepSelection=true);
    void listingArrived(samples::Listing);
    void infoArrived();
    void selectionChanged();
    void open(int row);
    void showFile(const juce::File&);
    void chooseFolder(const juce::String& replacing={});
    void showPlaceMenu(const juce::String& path);
    void updateButtons();
    void checkPlaces();
    void timerCallback() override;
    double lengthOf(const samples::Entry&) const;
    friend class Places;
    friend class Files;
    friend class Crumbs;
};
}
