// Copyright (C) 2026 Fabio 'Pitch' Serra, AGPL-3.0-or-later
#pragma once
#include "MainComponent.h"
#include "UiText.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
namespace fga {
class PianoStepGrid:public juce::Component,public juce::SettableTooltipClient {
public:
    explicit PianoStepGrid(MainComponent& m):owner(m){setSize(1100,190);setTooltip(tr("Click a step to cycle rest, note, accent, ghost, push and hold. Drag a velocity fader to change dynamics."));}
    void paint(juce::Graphics& g)override{
        auto s=pianoEngineSettings(owner.project,owner.selectedTrack,owner.clip().seed);auto pattern=sgk::GrooveEngine::basePattern(s);const bool bass=owner.selectedTrack==1;const auto& row=bass?pattern.leftHand:pattern.rightHand;
        const auto& levels=bass?s.userVelLeft:s.userVelRight;
        g.fillAll(Palette::panel);const float w=(getWidth()-80)/32.f;
        g.setFont(FamilyTheme::font(11));g.setColour(Palette::dim);g.drawText(tr(bass?"BASS":"PIANO"),4,25,72,30,juce::Justification::centredLeft);g.drawText(tr("VELOCITY"),4,94,72,30,juce::Justification::centredLeft);
        for(int i=0;i<32;++i){const char symbol=row.empty()?'.':row[(size_t)i%row.size()];auto box=juce::Rectangle<float>(80+i*w,28,w-3,32);g.setColour(i%8==0?Palette::control:Palette::field);g.fillRoundedRectangle(box,3);g.setColour(symbol=='.'?Palette::dim:Palette::accent);g.drawText(juce::String::charToString(symbol),box,juce::Justification::centred);g.setColour(Palette::dim);g.drawText(juce::String(i+1),80+(int)(i*w),5,(int)w,18,juce::Justification::centred);auto value=levels[(size_t)i];const float h=(value?value:owner.project.tracks[(size_t)owner.selectedTrack].engine.velocity)/127.f*70;g.setColour(value?Palette::accent2:Palette::dim.withAlpha(.5f));g.fillRect(80+i*w+4,145-h,w-11,h);g.drawText(value?juce::String(value):"–",80+(int)(i*w),150,(int)w,20,juce::Justification::centred);}
    }
    void mouseDown(const juce::MouseEvent& ev)override{
        const int i=index(ev.position.x);if(i<0)return;owner.checkpoint();auto& e=owner.project.tracks[(size_t)owner.selectedTrack].engine;
        if(e.pattern!=sgk::userPatternIndex())setPianoPattern(e,sgk::GrooveEngine::basePattern(pianoEngineSettings(owner.project,owner.selectedTrack,owner.clip().seed)));
        if(ev.y<70){auto& symbol=(owner.selectedTrack==1?e.userLeft:e.userRight)[(size_t)i];const std::string symbols=".xXg>=";auto pos=symbols.find(symbol);symbol=ev.mods.isRightButtonDown()?'.':symbols[(pos==std::string::npos?0:pos+1)%symbols.size()];}
        else setLevel(ev);
        owner.changed(false);repaint();
    }
    void mouseDrag(const juce::MouseEvent& ev)override{if(ev.getMouseDownY()>=70){setLevel(ev);owner.changed(false);repaint();}}
    void mouseDoubleClick(const juce::MouseEvent& ev)override{if(ev.y>=70){int i=index(ev.position.x);if(i>=0){owner.checkpoint();auto& e=owner.project.tracks[(size_t)owner.selectedTrack].engine;(owner.selectedTrack==1?e.userVelLeft:e.userVelRight)[(size_t)i]=0;owner.changed(false);repaint();}}}
private:
    int index(float x)const{return x<80||x>=getWidth()?-1:std::clamp((int)((x-80)*32/(getWidth()-80)),0,31);}
    void setLevel(const juce::MouseEvent& ev){int i=index(ev.position.x);if(i<0)return;auto& e=owner.project.tracks[(size_t)owner.selectedTrack].engine;(owner.selectedTrack==1?e.userVelLeft:e.userVelRight)[(size_t)i]=(unsigned char)std::clamp((int)((145-ev.y)*127/70),1,127);}
    MainComponent& owner;
};
}
