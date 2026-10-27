// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "DawTheme.h"
#include "FamilyControls.h"
namespace fga {
class MainComponent;
// The drag handle on the inspector's right edge, or (forBrowser) on the sample browser's left edge.
class InspectorResizer : public juce::Component,public juce::SettableTooltipClient {
public:
    explicit InspectorResizer(MainComponent&,bool forBrowser=false);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    MainComponent& owner;
    const bool browser;
    float dragOrigin=0;
    int initialWidth=248;
};
// Selected track's mix, sound source and clip actions. It keeps no pointer to a
// track, clip or plugin: every refresh and every action reads the current selection.
class TrackInspector : public juce::Component {
public:
    explicit TrackInspector(MainComponent&);
    void refresh();
    void finishNameEdit(){if(name.isBeingEdited())name.hideEditor(false);}
    int preferredHeight() const { return contentHeight; }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MainComponent& owner;
    bool updating=false,gainGesture=false,panGesture=false;
    int contentHeight=0;
    std::vector<std::pair<juce::String,juce::Rectangle<int>>> captions;
    std::vector<juce::Rectangle<int>> cards;
    juce::Label name,source,section;
    juce::Slider gain,pan;
    ActionButton instrument{"INSTRUMENT PANEL"},editor{"EDIT MIDI"},mute{"M"},solo{"S"},lockTrack{"LOCK TRACK"},lockClip{"LOCK CLIP"},internal{"BUILT-IN"},sample{"LOAD SAMPLE"},plugin{"LOAD VST3 / AU"},pluginEditor{"OPEN VST"},samplerEditor{"OPEN SAMPLER"},deleteTrack{"DELETE MIDI TRACK"},regenerate{"REGENERATE CLIP"},loop{"LOOP CLIP"};
    int layout(int width);
    bool validTrack() const;
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
    float peak=0,drawnLevel=-1;
    std::array<float,2> sides{},holds{},drawnSides{-1.f,-1.f},drawnHolds{-1.f,-1.f};
    std::array<int,2> holdTicks{};
    int drawnColour=-1;
    float faderY(double db) const;
    juce::Rectangle<int> meterBounds() const;
    bool updating=false,gainGesture=false,panGesture=false;
    juce::Label name;
    juce::Slider gain,pan;
    juce::TextButton mute{"M"},solo{"S"},instrument{"INSTRUMENT"},clip{"CLIP"},sendA{"A"},sendB{"B"},output{"M"};
    std::array<double,2> sendMemory{.25,.25};
};
class DawMixer : public juce::Component,public juce::SettableTooltipClient {
public:
    explicit DawMixer(MainComponent&);
    void refresh();
    void tick();
    void reveal(int track);
    bool verifySends(const juce::File&);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MainComponent& owner;
    int channelWidth=76,resizeHeight=0;
    float resizeOrigin=0;
    bool resizingMixer=false;
    juce::Viewport port;
    juce::Component channels;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    MixerStrip master;
    std::array<float,2> compactPeaks{},compactHolds{};
    std::array<int,2> compactHoldTicks{};
    ActionButton collapseButton{"COLLAPSE MIXER"},routingButton{"BUS & SEND"};
};
}
