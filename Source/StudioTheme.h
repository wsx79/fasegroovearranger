// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "DawTheme.h"

// Studio and Bus & Send use the workspace tokens. No second palette.
namespace StudioStyle {
using Daw::font;
using Daw::caps;
using Daw::onAccent;
using Daw::base;
using Daw::surface;
using Daw::surfaceHi;
using Daw::well;
using Daw::chrome;
using Daw::lcd;
using Daw::control;
using Daw::controlHi;
using Daw::line;
using Daw::lineStrong;
using Daw::text;
using Daw::textDim;
using Daw::textFaint;
using Daw::accent;
using Daw::automation;
using Daw::solo;
using Daw::mute;
using Daw::meterHigh;
using Daw::radius;
using Daw::panelRadius;
inline void drawCaps(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> bounds,juce::Colour ink=textDim,juce::Justification align=juce::Justification::centredLeft){Daw::drawCaps(g,value,bounds,ink,align);}
}

class StudioLookAndFeel : public DawLookAndFeel {
public:
    StudioLookAndFeel(){
        using namespace StudioStyle;
        setColour(juce::ResizableWindow::backgroundColourId,base);
        setColour(juce::TextButton::buttonColourId,control);setColour(juce::TextButton::buttonOnColourId,accent);
        setColour(juce::TextButton::textColourOffId,text);setColour(juce::TextButton::textColourOnId,onAccent);
        setColour(juce::ComboBox::backgroundColourId,control);setColour(juce::ComboBox::textColourId,text);setColour(juce::ComboBox::outlineColourId,line);
        setColour(juce::PopupMenu::backgroundColourId,surface);setColour(juce::PopupMenu::textColourId,text);setColour(juce::PopupMenu::highlightedBackgroundColourId,accent);setColour(juce::PopupMenu::highlightedTextColourId,onAccent);
        setColour(juce::Label::textColourId,text);setColour(juce::Slider::textBoxTextColourId,text);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        setColour(juce::Slider::backgroundColourId,line);setColour(juce::Slider::trackColourId,accent);setColour(juce::Slider::thumbColourId,text);
        setColour(juce::TextEditor::backgroundColourId,well);setColour(juce::TextEditor::textColourId,text);setColour(juce::TextEditor::outlineColourId,line);setColour(juce::TextEditor::focusedOutlineColourId,accent);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int)override{return StudioStyle::font(Daw::small);}
    juce::Font getComboBoxFont(juce::ComboBox&)override{return StudioStyle::font(12);}
    juce::Label* createSliderTextBox(juce::Slider& s)override{auto* label=DawLookAndFeel::createSliderTextBox(s);label->setFont(StudioStyle::font(12));return label;}
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool)override{
        using namespace StudioStyle;const auto r=b.getLocalBounds().toFloat().reduced(2);const bool on=b.getToggleState();const bool down=b.isDown();g.beginTransparencyLayer(b.isEnabled()?1.f:.4f);auto fill=on?accent:hover?controlHi:control;if(down)fill=fill.darker(.18f);Daw::fillElevatedSurface(g,r,fill,radius);g.setColour(down?accent:line);g.drawRoundedRectangle(r,radius,1);g.setFont(font(11));g.setColour(on?onAccent:text);g.drawFittedText(b.getButtonText(),r.reduced(6,2).toNearestInt(),juce::Justification::centred,1,.8f);g.endTransparencyLayer();
    }
    void drawPopupMenuBackground(juce::Graphics& g,int w,int h)override{g.fillAll(StudioStyle::surface);g.setColour(StudioStyle::line);g.drawRect(0,0,w,h);}
};
