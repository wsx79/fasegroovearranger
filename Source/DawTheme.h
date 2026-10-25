// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// The arranger's own DAW theme. It extends the family theme (FamilyTheme.h, shared with the
// plugins and left untouched): the same font and brand accent, with the denser, flatter
// surfaces of a multitrack workstation. Every colour, size and radius used by the arranger's
// workspace is defined here, once.
#include "FamilyTheme.h"
namespace Daw {
// Surfaces, from the darkest (the timeline well) to the lightest (a hovered control).
inline const juce::Colour base      {0xff111317};   // window background, gaps between panels
inline const juce::Colour well      {0xff15171b};   // timeline, fields, piano roll
inline const juce::Colour laneAlt   {0xff181a1f};   // every other track lane
inline const juce::Colour chrome    {0xff1b1e23};   // toolbars, ruler, status bar
inline const juce::Colour surface   {0xff202329};   // panels: track headers, inspector, mixer strips
inline const juce::Colour surfaceHi {0xff282c33};   // selected header, raised panel
inline const juce::Colour control   {0xff2b2f37};   // buttons, combo boxes
inline const juce::Colour controlHi {0xff353a43};   // hovered control
inline const juce::Colour lcd       {0xff0b0d0f};   // transport display
// Lines.
inline const juce::Colour line      {0xff2c3037};   // panel borders, separators
inline const juce::Colour lineStrong{0xff3c414b};   // control outlines, section edges
// Text: primary, secondary, disabled or faint.
inline const juce::Colour text      {0xffe4e6ea};
inline const juce::Colour textDim   {0xff9aa1ac};
inline const juce::Colour textFaint {0xff676e79};
// One accent (the family's amber) for selection, active state and the main actions.
inline const juce::Colour accent    {Palette::accent};
inline const juce::Colour onAccent  {0xff16130f};   // text and icons on the accent
// Functional colours: automation, transport and meters keep their usual meanings.
inline const juce::Colour automation{Palette::accent2};
inline const juce::Colour play      {0xff52d273};
inline const juce::Colour mute      {0xffe8a33d};
inline const juce::Colour solo      {0xff4cc3f0};
inline const juce::Colour meterLow  {0xff4fd17a};
inline const juce::Colour meterMid  {0xffe7c34a};
inline const juce::Colour meterHigh {0xffe85d5d};
inline const juce::Colour playhead  {0xfff2f3f5};
// Sizes: one font in three sizes, a 4 px grid and two radii.
constexpr float small=10.f,body=12.f,title=14.f,display=21.f;
constexpr float radius=3.f,panelRadius=5.f;
constexpr int unit=4,controlHeight=24,toolbarHeight=28;

inline juce::Font font(float size=body,bool bold=false){return FamilyTheme::font(size,bold);}
// Small caps captions: section titles, field labels, LCD legends.
inline juce::Font caps(float size=small){auto f=FamilyTheme::font(size,true);f.setExtraKerningFactor(.06f);return f;}
inline void drawCaps(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,juce::Colour c=textFaint,juce::Justification j=juce::Justification::centredLeft){g.setColour(c);g.setFont(caps());g.drawText(s.toUpperCase(),r,j,true);}

// Each track keeps the colour of its instrument family, with a different shade per drum lane
// so that neighbouring lanes stay distinguishable.
inline juce::Colour trackColour(int track){
    static const juce::uint32 colours[]{0xffffa94d,0xff4cc3f0,0xffff6b81,0xffffc65a,            // piano, bass, strings, guitar
        0xffff8a4d,0xff58b4f5,0xff6c9cff,0xff8a93ff,                                              // kick, snare, clap, rim
        0xff5fd68a,0xff45cfa5,0xff3ec5c5,                                                         // closed hat, open hat, ride
        0xffb69cff,0xffc690ff,0xffd98cf0,0xffe68acb,                                              // shaker, tambourine, conga, cowbell
        0xffff6b6b,0xffff9473};                                                                   // tom, crash
    return juce::Colour(colours[std::clamp(track,0,16)]);
}
// Section markers on the ruler, by the kind of section.
inline juce::Colour sectionColour(int kind){
    static const juce::uint32 colours[]{0xff6f8fd8,0xff4fb8b0,0xff9a86e0,0xffd9a441,0xffe8774d,0xff7d8a99};   // main, intro, breakdown, buildup, drop, outro
    return juce::Colour(colours[std::clamp(kind,0,5)]);
}
// A clip's body: the track colour sunk into the well, so the notes drawn on it stay readable.
inline juce::Colour clipBody(juce::Colour c,bool selected){return well.interpolatedWith(c,selected?.36f:.24f);}
// Meter colour for a linear peak level.
inline juce::Colour meterColour(float peak){return peak>=1.f?meterHigh:peak>.5f?meterMid:meterLow;}
// Meter position for a linear peak: 0 at -60 dB, 1 at +6 dB, as the faders.
inline float meterPosition(float peak){return peak<=.001f?0.f:std::clamp((20.f*std::log10(peak)+60.f)/66.f,0.f,1.f);}
}

class DawLookAndFeel : public FamilyLookAndFeel {
public:
    DawLookAndFeel(){
        setColour(juce::ResizableWindow::backgroundColourId,Daw::base);
        setColour(juce::TextButton::buttonColourId,Daw::control);
        setColour(juce::TextButton::buttonOnColourId,Daw::accent);
        setColour(juce::TextButton::textColourOffId,Daw::text);
        setColour(juce::TextButton::textColourOnId,Daw::onAccent);
        setColour(juce::ComboBox::backgroundColourId,Daw::control);
        setColour(juce::ComboBox::outlineColourId,Daw::line);
        setColour(juce::PopupMenu::backgroundColourId,Daw::surfaceHi);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,Daw::accent.withAlpha(.9f));
        setColour(juce::PopupMenu::highlightedTextColourId,Daw::onAccent);
        setColour(juce::Slider::trackColourId,Daw::accent);
        setColour(juce::Slider::backgroundColourId,Daw::well);
        setColour(juce::Slider::thumbColourId,Daw::text);
        setColour(juce::TextEditor::backgroundColourId,Daw::well);
        setColour(juce::TextEditor::outlineColourId,Daw::line);
        setColour(juce::ListBox::backgroundColourId,Daw::well);
        setColour(juce::ScrollBar::thumbColourId,Daw::lineStrong);
        setColour(juce::CaretComponent::caretColourId,Daw::accent);
    }
    // Flat buttons: a 3 px radius, a hairline edge and a 1 px top light; on = accent.
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& background,bool highlighted,bool down) override{
        const auto r=b.getLocalBounds().toFloat().reduced(.5f);const bool on=b.getToggleState();
        auto fill=on?b.findColour(juce::TextButton::buttonOnColourId):background;
        if(fill==Palette::control||fill==Palette::background)fill=Daw::control;
        if(!b.isEnabled())fill=fill.withMultipliedAlpha(.45f);else if(down)fill=fill.darker(.18f);else if(highlighted)fill=fill==Daw::control?Daw::controlHi:fill.brighter(.08f);
        juce::Path shape;shape.addRoundedRectangle(r.getX(),r.getY(),r.getWidth(),r.getHeight(),Daw::radius,Daw::radius,!b.isConnectedOnLeft(),!b.isConnectedOnRight(),!b.isConnectedOnLeft(),!b.isConnectedOnRight());
        g.setColour(fill);g.fillPath(shape);
        if(!down&&b.isEnabled()){g.setColour(juce::Colours::white.withAlpha(fill.getPerceivedBrightness()>.4f?.18f:.045f));g.fillRect(r.getX()+Daw::radius,r.getY()+.5f,r.getWidth()-2*Daw::radius,1.f);}
        g.setColour(b.hasKeyboardFocus(true)?Daw::accent.withAlpha(.8f):fill.getPerceivedBrightness()>.4f?fill.darker(.3f):juce::Colours::black.withAlpha(.45f));
        g.strokePath(shape,juce::PathStrokeType(1.f));
    }
    juce::Font getTextButtonFont(juce::TextButton&,int height) override{return Daw::font(juce::jlimit(9.5f,11.5f,height*.42f),true);}
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override{
        const bool on=b.getToggleState();const auto fill=b.findColour(on?juce::TextButton::buttonOnColourId:juce::TextButton::buttonColourId);
        auto ink=fill.getPerceivedBrightness()>.4f?Daw::onAccent:Daw::text;
        g.setColour(ink.withMultipliedAlpha(b.isEnabled()?1.f:.45f));auto f=getTextButtonFont(b,b.getHeight());f.setExtraKerningFactor(.03f);g.setFont(f);
        g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(6,2),juce::Justification::centred,1,.75f);
    }
    void drawComboBox(juce::Graphics& g,int width,int height,bool,int,int,int,int,juce::ComboBox& box) override{
        const auto r=juce::Rectangle<float>(0,0,(float)width,(float)height).reduced(.5f);
        const auto fill=box.findColour(juce::ComboBox::backgroundColourId);const bool flat=fill.isTransparent();
        if(!flat){g.setColour(box.isMouseOver(true)&&box.isEnabled()&&fill==Daw::control?Daw::controlHi:fill);g.fillRoundedRectangle(r,Daw::radius);
            g.setColour(box.hasKeyboardFocus(true)?Daw::accent.withAlpha(.8f):juce::Colours::black.withAlpha(.45f));g.drawRoundedRectangle(r,Daw::radius,1.f);}
        const float cx=(float)width-10.f,cy=height*.5f;juce::Path chevron;chevron.startNewSubPath(cx-3.f,cy-1.5f);chevron.lineTo(cx,cy+1.5f);chevron.lineTo(cx+3.f,cy-1.5f);
        g.setColour(box.isEnabled()?Daw::textDim:Daw::textFaint.withAlpha(.5f));g.strokePath(chevron,juce::PathStrokeType(1.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
    juce::Font getComboBoxFont(juce::ComboBox& box) override{const float display=(float)box.getProperties().getWithDefault("dawDisplay",0.f);return display>0?Daw::font(display,true):Daw::font(juce::jlimit(10.5f,13.f,box.getHeight()*.46f));}
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override{label.setBounds(3,1,box.getWidth()-20,box.getHeight()-2);label.setFont(getComboBoxFont(box));}
    void drawPopupMenuBackground(juce::Graphics& g,int width,int height) override{g.fillAll(Daw::surfaceHi);g.setColour(Daw::lineStrong);g.drawRect(0,0,width,height,1);}
    juce::Font getPopupMenuFont() override{return Daw::font(13.f);}
    // Linear sliders: a thin groove with the value in the accent and a small round thumb;
    // vertical sliders are mixer faders with a flat cap; bar styles draw nothing (a value
    // display, as the transport tempo).
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,juce::Slider::SliderStyle style,juce::Slider& s) override{
        if(style==juce::Slider::LinearBar||style==juce::Slider::LinearBarVertical)return;
        const auto value=s.findColour(juce::Slider::thumbColourId)==Daw::text?Daw::accent:s.findColour(juce::Slider::thumbColourId);
        const float alpha=s.isEnabled()?1.f:.4f;
        if(style==juce::Slider::LinearVertical){
            const float cx=x+width*.5f;const auto groove=juce::Rectangle<float>(cx-2.f,(float)y,4.f,(float)height);
            g.setColour(juce::Colours::black.withAlpha(.6f*alpha));g.fillRoundedRectangle(groove,2.f);
            g.setColour(Daw::line.withAlpha(alpha));g.drawRoundedRectangle(groove.expanded(.5f),2.f,1.f);
            const auto cap=juce::Rectangle<float>(std::min(30.f,(float)width-4.f),14.f).withCentre({cx,pos});
            g.setColour(juce::Colours::black.withAlpha(.4f));g.fillRoundedRectangle(cap.translated(0,1.5f),2.f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff50565f).withMultipliedAlpha(alpha),cx,cap.getY(),juce::Colour(0xff30343b).withMultipliedAlpha(alpha),cx,cap.getBottom(),false));g.fillRoundedRectangle(cap,2.f);
            g.setColour(juce::Colours::black.withAlpha(.5f));g.drawRoundedRectangle(cap.reduced(.5f),2.f,1.f);
            g.setColour(value==Daw::accent?Daw::text.withAlpha(.9f*alpha):value.withMultipliedAlpha(alpha));g.fillRect(cap.getX()+4.f,pos-.75f,cap.getWidth()-8.f,1.5f);
            return;
        }
        const bool horizontal=style!=juce::Slider::LinearVertical;
        if(!horizontal)return;
        const float cy=y+height*.5f;const auto groove=juce::Rectangle<float>((float)x,cy-1.5f,(float)width,3.f);
        g.setColour(juce::Colours::black.withAlpha(.55f*alpha));g.fillRoundedRectangle(groove,1.5f);
        const float origin=s.getMinimum()<0&&s.getMaximum()>0&&std::abs(s.getMinimum()+s.getMaximum())<1e-9?(float)s.getPositionOfValue(0):(float)x;
        g.setColour(value.withMultipliedAlpha(alpha));g.fillRoundedRectangle(juce::Rectangle<float>(std::min(origin,pos),cy-1.5f,std::abs(pos-origin),3.f),1.5f);
        const auto thumb=juce::Rectangle<float>(10.f,10.f).withCentre({pos,cy});
        g.setColour(juce::Colours::black.withAlpha(.4f));g.fillEllipse(thumb.translated(0,1.f));
        g.setColour((s.isMouseOverOrDragging()?Daw::text:Daw::text.withAlpha(.85f)).withMultipliedAlpha(alpha));g.fillEllipse(thumb);
    }
    int getSliderThumbRadius(juce::Slider& s) override{return s.getSliderStyle()==juce::Slider::LinearVertical?7:5;}
    juce::Label* createSliderTextBox(juce::Slider& s) override{
        auto* l=FamilyLookAndFeel::createSliderTextBox(s);l->setFont(Daw::font(s.getSliderStyle()==juce::Slider::LinearBarVertical?Daw::display:11.f,s.getSliderStyle()==juce::Slider::LinearBarVertical));
        l->setColour(juce::Label::outlineWhenEditingColourId,Daw::accent);l->setColour(juce::TextEditor::highlightColourId,Daw::accent.withAlpha(.4f));return l;}
    // Knobs: a thin track, the value arc from the left (or from the centre for a bipolar
    // range such as pan) and a flat cap with a pointer.
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float startAngle,float endAngle,juce::Slider& s) override{
        const auto bounds=juce::Rectangle<int>(x,y,width,height).toFloat().reduced(2.f);
        const float radius=std::min(bounds.getWidth(),bounds.getHeight())*.5f;const auto centre=bounds.getCentre();
        const float angle=startAngle+pos*(endAngle-startAngle);const float lineW=juce::jlimit(1.8f,3.f,radius*.11f),arcR=radius-lineW*.5f;
        const bool bipolar=s.getMinimum()<0&&std::abs(s.getMinimum()+s.getMaximum())<1e-9;const float from=bipolar?(startAngle+endAngle)*.5f:startAngle;
        const auto fill=s.findColour(juce::Slider::rotarySliderFillColourId);const auto ink=s.isColourSpecified(juce::Slider::rotarySliderFillColourId)?fill:Daw::accent;
        juce::Path track;track.addCentredArc(centre.x,centre.y,arcR,arcR,0,startAngle,endAngle,true);
        g.setColour(juce::Colours::black.withAlpha(.55f));g.strokePath(track,juce::PathStrokeType(lineW,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        juce::Path value;value.addCentredArc(centre.x,centre.y,arcR,arcR,0,std::min(from,angle),std::max(from,angle),true);
        g.setColour(ink.withMultipliedAlpha(s.isEnabled()?1.f:.4f));g.strokePath(value,juce::PathStrokeType(lineW,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        const float capR=arcR-lineW-2.f;const auto cap=juce::Rectangle<float>(capR*2,capR*2).withCentre(centre);
        g.setColour(juce::Colours::black.withAlpha(.35f));g.fillEllipse(cap.translated(0,1.f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3f48),centre.x,cap.getY(),juce::Colour(0xff25292f),centre.x,cap.getBottom(),false));g.fillEllipse(cap);
        g.setColour(juce::Colours::black.withAlpha(.5f));g.drawEllipse(cap.reduced(.5f),1.f);
        g.setColour(Daw::text.withMultipliedAlpha(s.isEnabled()?1.f:.4f));g.drawLine({centre.getPointOnCircumference(capR*.3f,angle),centre.getPointOnCircumference(capR*.9f,angle)},std::max(1.5f,lineW*.8f));
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool highlighted,bool) override{
        g.beginTransparencyLayer(b.isEnabled()?1.f:.45f);auto r=b.getLocalBounds().toFloat();const bool on=b.getToggleState();
        auto pill=r.removeFromLeft(30.f).withSizeKeepingCentre(26.f,14.f);
        g.setColour(on?Daw::accent:Daw::well);g.fillRoundedRectangle(pill,7.f);
        g.setColour(on?Daw::accent.darker(.25f):highlighted?Daw::lineStrong.brighter(.2f):Daw::lineStrong);g.drawRoundedRectangle(pill.reduced(.5f),7.f,1.f);
        const float d=10.f;g.setColour(on?Daw::onAccent:Daw::textDim);g.fillEllipse(on?pill.getRight()-d-2.f:pill.getX()+2.f,pill.getCentreY()-d*.5f,d,d);
        g.setColour(Daw::text);g.setFont(Daw::font(Daw::body));g.drawText(b.getButtonText(),r.withTrimmedLeft(5.f),juce::Justification::centredLeft);
        g.endTransparencyLayer();
    }
    void drawScrollbar(juce::Graphics& g,juce::ScrollBar&,int x,int y,int width,int height,bool vertical,int thumbStart,int thumbSize,bool over,bool down) override{
        g.setColour(Daw::well);g.fillRect(x,y,width,height);
        auto thumb=vertical?juce::Rectangle<float>((float)x,(float)thumbStart,(float)width,(float)thumbSize):juce::Rectangle<float>((float)thumbStart,(float)y,(float)thumbSize,(float)height);
        g.setColour(down?Daw::accent.withAlpha(.8f):over?Daw::lineStrong.brighter(.35f):Daw::lineStrong);g.fillRoundedRectangle(thumb.reduced(2.5f),3.f);
    }
    int getDefaultScrollbarWidth() override{return 10;}
    void drawTooltip(juce::Graphics& g,const juce::String& s,int width,int height) override{
        const auto r=juce::Rectangle<float>(0,0,(float)width,(float)height);g.setColour(Daw::surfaceHi);g.fillRoundedRectangle(r,Daw::radius);
        g.setColour(Daw::lineStrong);g.drawRoundedRectangle(r.reduced(.5f),Daw::radius,1.f);
        g.setColour(Daw::text);g.setFont(FamilyTheme::font(13.f));g.drawFittedText(s,r.reduced(10.f,6.f).toNearestInt(),juce::Justification::centredLeft,8,1.f);
    }
};
