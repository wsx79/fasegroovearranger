// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
#include "Updater.h"
namespace fga {
class OptionsOverlay : public juce::Component
{
public:
    OptionsOverlay();

    std::function<void (float)> onScaleChosen;
    std::function<void (bool)> onTipsChanged;
    std::function<void()> onCheckUpdates, onLanguageChanged;
    std::function<void(int)> onDocument;
    std::function<void()> onAudioSettings,onPluginSettings;
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
    juce::TextButton pluginButton, audioButton, guideButton, privacyButton, licencesButton;
    juce::Label languageLabel, sizeLabel;
    juce::ComboBox languageBox, sizeBox;
    juce::ToggleButton tipsToggle, updatesToggle;
};

class UpdateOverlay : public juce::Component
{
public:
    UpdateOverlay();
    void show (const Updater::Result&);
    void refreshText();
    bool verifyLayout()const;
    std::function<void(const Updater::Result&)> onInstall;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<float> card() const { return getLocalBounds().toFloat().withSizeKeepingCentre (640.0f, 560.0f); }
    Updater::Result result;
    juce::TextEditor notes;
    juce::TextButton installButton, laterButton, skipButton;
};

}
