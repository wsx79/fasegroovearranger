// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
namespace fga {
class MainComponent;
class TrackInspector : public juce::Component {
public:
    explicit TrackInspector(MainComponent&);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MainComponent& owner;
    bool updating=false,gainGesture=false,panGesture=false;
    juce::Label name,source,section;
    juce::Slider gain,pan;
    juce::TextButton instrument{"INSTRUMENT PANEL"},editor{"EDIT MIDI"},mute{"M"},solo{"S"},lockTrack{"LOCK TRACK"},lockClip{"LOCK CLIP"},internal{"BUILT-IN"},sample{"LOAD SAMPLE"},plugin{"LOAD VST3 / AU"},pluginEditor{"OPEN VST"},regenerate{"REGENERATE CLIP"},loop{"LOOP SECTION"};
};
class MixerStrip : public juce::Component {
public:
    MixerStrip(MainComponent&,int track);
    void refresh();
    void tick();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    MainComponent& owner;
    int track;
    float peak=0;
    bool updating=false,gainGesture=false,panGesture=false;
    juce::Label name;
    juce::Slider gain,pan;
    juce::TextButton mute{"M"},solo{"S"},instrument{"INSTRUMENT"},clip{"CLIP"};
};
class DawMixer : public juce::Component,public juce::SettableTooltipClient {
public:
    explicit DawMixer(MainComponent&);
    void refresh();
    void tick();
    void reveal(int track);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MainComponent& owner;
    int channelWidth=76,resizeOrigin=0,resizeHeight=0;
    bool resizingMixer=false;
    juce::Viewport port;
    juce::Component channels;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    MixerStrip master;
};
}
