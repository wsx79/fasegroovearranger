// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "DawTheme.h"

// Shared by Studio's three pages and Bus & Send. The workspace/family theme is unchanged.
namespace StudioStyle {
using Daw::font;
using Daw::caps;
inline const juce::Colour onAccent{0xff1b1c1f};
inline const juce::Colour base{0xff1b1c1f},surface{0xff232427},surfaceHi{0xff2c2d31};
inline const juce::Colour well{0xff1b1c1f},chrome{0xff232427},lcd{0xff17181b};
inline const juce::Colour control{0xff2c2d31},controlHi{0xff34353a},line{0xff34353a},lineStrong{0xff45464b};
inline const juce::Colour text{0xffc9cacd},textDim{0xff85868b},textFaint{0xff85868b},accent{0xfff5a142};
inline const juce::Colour automation=accent,solo=accent,mute=accent,meterHigh=accent;
constexpr float radius=4.f,panelRadius=6.f;
inline void drawCaps(juce::Graphics& g,const juce::String& value,juce::Rectangle<int> bounds,juce::Colour ink=textDim,juce::Justification align=juce::Justification::centredLeft){g.setFont(font(11));g.setColour(ink);g.drawText(value,bounds,align,true);}
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
    juce::Font getTextButtonFont(juce::TextButton&,int)override{return StudioStyle::font(11);}
    juce::Font getComboBoxFont(juce::ComboBox&)override{return StudioStyle::font(12);}
    juce::Label* createSliderTextBox(juce::Slider& s)override{auto* label=DawLookAndFeel::createSliderTextBox(s);label->setFont(StudioStyle::font(12));return label;}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& colour,bool hover,bool down)override{
        using namespace StudioStyle;const auto r=b.getLocalBounds().toFloat().reduced(.5f);auto fill=b.getToggleState()?b.findColour(juce::TextButton::buttonOnColourId):colour;
        if(hover||down)fill=fill.brighter(down?.04f:.08f);g.setColour(fill.withMultipliedAlpha(b.isEnabled()?1.f:.4f));g.fillRoundedRectangle(r,radius);
        g.setColour(b.hasKeyboardFocus(true)?accent:line);g.drawRoundedRectangle(r,radius,1);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool)override{
        using namespace StudioStyle;const auto fill=b.findColour(b.getToggleState()?juce::TextButton::buttonOnColourId:juce::TextButton::buttonColourId);g.setFont(font(11));g.setColour((fill.getPerceivedBrightness()>.45f?onAccent:text).withMultipliedAlpha(b.isEnabled()?1.f:.4f));g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(6,2),juce::Justification::centred,1,.8f);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool)override{
        using namespace StudioStyle;const auto r=b.getLocalBounds().toFloat().reduced(2);const bool on=b.getToggleState();g.beginTransparencyLayer(b.isEnabled()?1.f:.4f);g.setColour(on?accent:hover?controlHi:control);g.fillRoundedRectangle(r,radius);g.setColour(b.hasKeyboardFocus(true)?accent:line);g.drawRoundedRectangle(r,radius,1);g.setFont(font(11));g.setColour(on?onAccent:text);g.drawFittedText(b.getButtonText(),r.reduced(6,2).toNearestInt(),juce::Justification::centred,1,.8f);g.endTransparencyLayer();
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box)override{
        using namespace StudioStyle;auto r=juce::Rectangle<float>(0,0,(float)w,(float)h).reduced(.5f);g.setColour(box.findColour(juce::ComboBox::backgroundColourId));g.fillRoundedRectangle(r,radius);g.setColour(box.hasKeyboardFocus(true)?accent:line);g.drawRoundedRectangle(r,radius,1);const float x=w-12.f,y=h*.5f;juce::Path p;p.startNewSubPath(x-3,y-1.5f);p.lineTo(x,y+1.5f);p.lineTo(x+3,y-1.5f);g.setColour(textDim);g.strokePath(p,juce::PathStrokeType(1.2f));
    }
    void drawPopupMenuBackground(juce::Graphics& g,int w,int h)override{g.fillAll(StudioStyle::surface);g.setColour(StudioStyle::line);g.drawRect(0,0,w,h);}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& s)override{
        using namespace StudioStyle;auto r=juce::Rectangle<int>(x,y,w,h).toFloat().reduced(3);const float radius=std::min(r.getWidth(),r.getHeight())*.5f-1;const auto c=r.getCentre();const float angle=start+pos*(end-start),from=s.getMinimum()<0&&std::abs(s.getMinimum()+s.getMaximum())<1e-9?(start+end)*.5f:start;
        juce::Path rail;rail.addCentredArc(c.x,c.y,radius,radius,0,start,end,true);g.setColour(line);g.strokePath(rail,juce::PathStrokeType(2));juce::Path value;value.addCentredArc(c.x,c.y,radius,radius,0,std::min(from,angle),std::max(from,angle),true);g.setColour(accent.withMultipliedAlpha(s.isEnabled()?1.f:.35f));g.strokePath(value,juce::PathStrokeType(2));g.setColour(control);g.fillEllipse(juce::Rectangle<float>(radius*1.5f,radius*1.5f).withCentre(c));g.setColour(text.withMultipliedAlpha(s.isEnabled()?1.f:.35f));g.drawLine({c.getPointOnCircumference(radius*.3f,angle),c.getPointOnCircumference(radius*.75f,angle)},1.5f);
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float low,float high,juce::Slider::SliderStyle style,juce::Slider& s)override{
        using namespace StudioStyle;if(style==juce::Slider::LinearBar||style==juce::Slider::LinearBarVertical)return;
        if(style!=juce::Slider::LinearHorizontal&&style!=juce::Slider::LinearVertical){DawLookAndFeel::drawLinearSlider(g,x,y,w,h,pos,low,high,style,s);return;}
        const bool vertical=style==juce::Slider::LinearVertical;const float cx=x+w*.5f,cy=y+h*.5f;g.beginTransparencyLayer(s.isEnabled()?1.f:.4f);g.setColour(line);g.fillRect(vertical?juce::Rectangle<float>(cx-1,(float)y,2,(float)h):juce::Rectangle<float>((float)x,cy-1,(float)w,2));g.setColour(accent);g.fillRect(vertical?juce::Rectangle<float>(cx-1,pos,2,std::max(0.f,y+h-pos)):juce::Rectangle<float>((float)x,cy-1,std::max(0.f,pos-x),2));
        if(vertical){auto cap=juce::Rectangle<float>(28,12).withCentre({cx,pos});g.setColour(controlHi);g.fillRoundedRectangle(cap,2);g.setColour(lineStrong);g.drawRoundedRectangle(cap,2,1);g.setColour(text);g.fillRect(cx-10,pos-.5f,20.f,1.f);}else{g.setColour(text);g.fillEllipse(pos-4,cy-4,8,8);}g.endTransparencyLayer();
    }
};
