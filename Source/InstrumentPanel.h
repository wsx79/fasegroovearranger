// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
#include "core/Project.h"
namespace fga {
class MainComponent;
// Adaptation of the family plugin instrument strips: DAW owns harmony/transport.
class InstrumentPanel : public juce::Component {
public:
    explicit InstrumentPanel(MainComponent&);
    void rebuild();
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MainComponent& owner;
    struct Control { std::unique_ptr<juce::Component> component; juce::String caption; bool knob=false; };
    std::vector<Control> controls;
    std::vector<std::pair<juce::String,int>> headings;
    void heading(const juce::String&);
    void knob(const juce::String&,double,double,double,double,std::function<void(EngineOptions&,double)>,const juce::String& suffix={});
    void choice(const juce::String&,const juce::StringArray&,int,std::function<void(EngineOptions&,int)>);
    void toggle(const juce::String&,bool,std::function<void(EngineOptions&,bool)>);
    void apply(std::function<void(EngineOptions&)>);
};
void drawFamilyLogo(juce::Graphics&,juce::Rectangle<float>);
}
