// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
#include "core/Project.h"
#include "CircleOfFifths.h"
namespace fga {
class MainComponent;
// Adaptation of the family plugin instrument strips: DAW owns harmony/transport.
class InstrumentPanel : public juce::Component {
public:
    explicit InstrumentPanel(MainComponent&);
    void rebuild();
    void refreshFromProject();
    juce::String layoutProblems()const;
    int sectionPosition(const juce::String& name)const{for(size_t i=0;i<headings.size()&&i<headingY.size();++i)if(headings[i].first==name)return headingY[i];return 0;}
    juce::StringArray sectionNames()const{juce::StringArray names;for(const auto& h:headings)names.add(h.first);return names;}

    void paint(juce::Graphics&) override;
    void resized() override;
private:
    MainComponent& owner;
    struct Control { std::unique_ptr<juce::Component> component; juce::String caption; bool knob=false; };
    std::vector<Control> controls;
    std::vector<std::pair<juce::String,int>> headings;
    std::vector<int> headingY;
    struct Card { juce::String title; juce::Rectangle<int> bounds; };
    std::vector<Card> cards;
    CircleOfFifths circle;
    int guitarLane=0;
    void updateCircle();
    void heading(const juce::String&);
    void knob(const juce::String&,double,double,double,double,std::function<void(EngineOptions&,double)>,const juce::String& suffix={});
    void choice(const juce::String&,const juce::StringArray&,int,std::function<void(EngineOptions&,int)>);
    void sharedChoice(const juce::String&,const juce::StringArray&,int,std::function<void(int)>);
    void action(const juce::String&,std::function<void()>);
    void toggle(const juce::String&,bool,std::function<void(EngineOptions&,bool)>);
    void apply(std::function<void(EngineOptions&)>);
};
void drawFamilyLogo(juce::Graphics&,juce::Rectangle<float>);
}
