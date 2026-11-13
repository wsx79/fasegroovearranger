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
    void finishRecording();
    void refreshSampleState();
    void open(int tab=0);
    bool verify(const juce::File& folder);
    // Self-tests drive the real controls: the track the Studio edits, a knob moved through its
    // own callback (Undo checkpoint, commit, audio clip sync) and the TRIM SILENCE button.
    int editedTrack();
    bool setControlForTests(const juce::String& caption,double value);
    void trimSilenceForTests();
private:
    class Panel;
    MainComponent& owner;
    Panel* panel=nullptr;
    juce::TextButton dismissButton{"X"};
    void resized()override;
    void resetDefaultSize();
};
}
