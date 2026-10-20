// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
namespace fga {
class OptionsOverlay : public juce::Component
{
public:
    OptionsOverlay();

    std::function<void (float)> onScaleChosen;
    std::function<void (bool)> onTipsChanged;
    std::function<void()> onCheckUpdates, onLanguageChanged;
    std::function<void(int)> onDocument;
    std::function<void()> onAudioSettings;
    bool verifyLayout() const;

    juce::TextButton checkUpdatesButton;

    void show (bool aboutTab, float currentScale);
    void refreshText();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> card() const { return getLocalBounds().toFloat().withSizeKeepingCentre (620.0f, 620.0f); }
    void selectTab (bool about);

    bool showingAbout = false;
    juce::Image artist;
    juce::TextButton settingsTab, aboutTab, closeButton;
    juce::TextButton sourceButton, kofiButton, paypalButton, spotifyButton;
    juce::TextButton audioButton, guideButton, privacyButton, licencesButton;
    juce::Label languageLabel, sizeLabel;
    juce::ComboBox languageBox, sizeBox;
    juce::ToggleButton tipsToggle, updatesToggle;
};

}
