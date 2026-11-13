// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// The arranger's own DAW theme. It extends the family theme (FamilyTheme.h, shared with the
// plugins and left untouched): the same font and brand accent, with the denser, flatter
// surfaces of a multitrack workstation. Every colour, size and radius used by the arranger's
// workspace is defined here, once.
#include "FamilyTheme.h"
#include <cmath>
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
// Interactive small parts (scrollbar thumbs, slider grooves): they must stay visible on the
// wells, so they sit above 3:1 instead of borrowing lineStrong.
inline const juce::Colour lineActive{0xff5f6774};
// Text: primary, secondary, disabled or faint. Faint is kept above 4.5:1 on every
// surface it is used on: it carries captions and secondary labels, which must stay legible
// at 10 px.
inline const juce::Colour text      {0xffe4e6ea};
inline const juce::Colour textDim   {0xff9aa1ac};
inline const juce::Colour textFaint {0xff868e9a};
// One accent (the family's amber) for selection, active state and the main actions.
inline const juce::Colour accent    {Palette::accent};
inline const juce::Colour onAccent  {0xff16130f};   // text and icons on the accent
// Functional colours stay off the track palette: piano/accent are amber, bass and
// accent2 are cyan, guitar is gold. Mute, solo and automation each have their own hue.
inline const juce::Colour returnA   {0xff52d4c8};
inline const juce::Colour returnB   {0xffb494ff};
inline juce::Colour returnColour(int index){return index==0?returnA:returnB;}
inline const juce::Colour automation{0xffff5ec8};   // magenta, not bass cyan or cowbell pink
inline const juce::Colour play      {0xff52d273};
inline const juce::Colour mute      {0xff8d9070};   // olive grey, not piano amber
inline const juce::Colour solo      {0xfff5e04a};   // lemon, not guitar gold or bass cyan
inline const juce::Colour meterLow  {0xff4fd17a};
inline const juce::Colour meterMid  {0xffe7c34a};
inline const juce::Colour meterHigh {0xffe85d5d};
inline const juce::Colour playhead  {0xfff2f3f5};
// Sizes: one font in three sizes, a 4 px grid and two radii.
constexpr float small=10.5f,body=12.f,title=14.f,display=21.f;
constexpr float radius=3.f,panelRadius=5.f;
constexpr int unit=4,controlHeight=24,toolbarHeight=28;

inline juce::Font font(float size=body,bool bold=false){return FamilyTheme::font(size,bold);}
inline juce::Font smallFont(float size=10.f){return FamilyTheme::font(size,true);}
inline juce::Colour bgPanel(){return surface;}
// One physical pixel, so a 1 px rule stays crisp when the desktop scale is not 1.
inline float hairline(){const float s=juce::Desktop::getInstance().getGlobalScaleFactor();return s>0.f?1.f/s:1.f;}
// Small caps captions: section titles, field labels, LCD legends.
inline juce::Font caps(float size=small){auto f=FamilyTheme::font(size,true);f.setExtraKerningFactor(.06f);return f;}
inline void drawCaps(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,juce::Colour c=textFaint,juce::Justification j=juce::Justification::centredLeft){g.setColour(c);g.setFont(caps());g.drawFittedText(s.toUpperCase(),r,j,1,.72f);}

// Shared material for native devices; graphs and controls retain their own
// semantics. No simulated meters or DSP-dependent resources on the UI thread.
inline void devicePanel(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour){
    // Flat module surface: one fill, one hairline. Colour belongs to data, not to chrome.
    r=r.reduced(.5f);g.setColour(juce::Colour(0xff16181b));g.fillRoundedRectangle(r,6.f);
    g.setColour(juce::Colour(0xff25282d));g.drawRoundedRectangle(r,6.f,1.f);
}
// Display window for graphs and meters.
inline void deviceScreen(juce::Graphics& g,juce::Rectangle<float> r){
    g.setColour(juce::Colour(0xff0d0e10));g.fillRoundedRectangle(r,4.f);
    g.setColour(juce::Colour(0xff222429));g.drawRoundedRectangle(r.reduced(.5f),4.f,1.f);
}
inline void deviceHeader(juce::Graphics& g,juce::Rectangle<int> r,const juce::String& titleText,const juce::String& category,juce::Colour){
    auto f=font(16,true);g.setFont(f);g.setColour(text);
    const int w=juce::GlyphArrangement::getStringWidthInt(f,titleText);g.drawText(titleText,r.withWidth(w+2),juce::Justification::centredLeft);
    r.removeFromLeft(w+12);g.setFont(font(11.5f));g.setColour(textFaint);g.drawText(category,r,juce::Justification::centredLeft);
}
// One knob for every native device: flat cap, thin value ring, white indicator.
inline void drawKnob(juce::Graphics& g,juce::Rectangle<float> bounds,float norm,juce::Colour ink,bool bipolar,bool enabled,bool active){
    const float size=std::min(bounds.getWidth(),bounds.getHeight());if(size<8.f)return;
    const auto c=bounds.getCentre();const float outer=size*.5f;
    constexpr float start=juce::MathConstants<float>::pi*1.25f,end=juce::MathConstants<float>::pi*2.75f;
    const float angle=start+std::clamp(norm,0.f,1.f)*(end-start),from=bipolar?(start+end)*.5f:start;
    const float alpha=enabled?1.f:.35f,arcW=outer>30.f?2.5f:2.f,arcR=outer-arcW*.5f-.5f,body=arcR-arcW-3.5f;
    juce::Path track;track.addCentredArc(c.x,c.y,arcR,arcR,0,start,end,true);
    g.setColour(juce::Colour(0xff2a2d33).withMultipliedAlpha(alpha));g.strokePath(track,juce::PathStrokeType(arcW,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    if(std::abs(angle-from)>.01f){juce::Path v;v.addCentredArc(c.x,c.y,arcR,arcR,0,std::min(from,angle),std::max(from,angle),true);
        g.setColour(ink.withMultipliedAlpha(alpha));g.strokePath(v,juce::PathStrokeType(arcW,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
    // 3D cap, as the hardware keys: soft drop shadow, a bevelled skirt lit from above and a
    // slightly concave face (darker at the top), with a small specular highlight.
    const auto cap=juce::Rectangle<float>(body*2,body*2).withCentre(c);
    g.setColour(juce::Colours::black.withAlpha(.22f*alpha));g.fillEllipse(cap.expanded(1.f).translated(0,2.5f));
    g.setColour(juce::Colours::black.withAlpha(.45f*alpha));g.fillEllipse(cap.translated(0,1.5f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(active?0xff40454e:0xff3a3e46).withMultipliedAlpha(alpha),c.x,cap.getY(),juce::Colour(0xff1d1f24).withMultipliedAlpha(alpha),c.x,cap.getBottom(),false));g.fillEllipse(cap);
    const auto face=cap.reduced(std::max(1.5f,body*.16f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26292e).withMultipliedAlpha(alpha),c.x,face.getY(),juce::Colour(active?0xff363a42:0xff31353c).withMultipliedAlpha(alpha),c.x,face.getBottom(),false));g.fillEllipse(face);
    g.setColour(juce::Colours::black.withAlpha(.35f*alpha));g.drawEllipse(face,.8f);
    {juce::Path rim;rim.addCentredArc(c.x,c.y,body-.6f,body-.6f,0,-1.1f,1.1f,true);g.setColour(juce::Colours::white.withAlpha(.16f*alpha));g.strokePath(rim,juce::PathStrokeType(1.f));}
    g.setColour(juce::Colour(0xff101114).withMultipliedAlpha(alpha));g.drawEllipse(cap,1.f);
    g.setColour(juce::Colour(0xfff2f3f5).withMultipliedAlpha(alpha));
    g.drawLine({c.getPointOnCircumference(body*.45f,angle),c.getPointOnCircumference(body*.88f,angle)},outer>30.f?2.2f:1.8f);
}
// Vertical LED-style level meter (peak hold optional), scale -60..+6 dB.
inline void drawLedMeter(juce::Graphics& g,juce::Rectangle<float> r,float levelNorm){
    g.setColour(juce::Colour(0xff1d1f23));g.fillRoundedRectangle(r,2.f);
    levelNorm=std::clamp(levelNorm,0.f,1.f);if(levelNorm<=0.f)return;
    const auto lit=r.reduced(1.f).withTrimmedTop(r.reduced(1.f).getHeight()*(1.f-levelNorm));
    g.setGradientFill(juce::ColourGradient(meterHigh,r.getX(),r.getY(),meterLow,r.getX(),r.getBottom(),false));
    {juce::Graphics::ScopedSaveState s(g);g.reduceClipRegion(lit.toNearestInt());g.fillRoundedRectangle(r.reduced(1.f),1.5f);}
}

// Each track keeps the colour of its instrument family, with a different shade per drum lane
// so that neighbouring lanes stay distinguishable.
inline juce::Colour trackColour(int track){
    static const juce::uint32 colours[]{0xffffa94d,0xff4cc3f0,0xffff6b81,0xffffc65a,            // piano, bass, strings, guitar
        0xffff8a4d,0xff58b4f5,0xff6c9cff,0xff8a93ff,                                              // kick, snare, clap, rim
        0xff5fd68a,0xff45cfa5,0xff3ec5c5,                                                         // closed hat, open hat, ride
        0xffb69cff,0xffc690ff,0xffd98cf0,0xffe68acb,                                              // shaker, tambourine, conga, cowbell
        0xffff6b6b,0xffff9473,0xff64d6ca};                                                                   // tom, crash, lead
    // Factory tracks 0–17 keep this palette. Added MIDI tracks get a stable hue so
    // neighbouring manual parts stay distinct from Lead and from each other.
    if(track<18)return juce::Colour(colours[std::clamp(track,0,17)]);
    const float hue=std::fmod(0.07f+(float)(track-18)*0.6180339887f,1.f);
    return juce::Colour::fromHSV(hue,0.62f,0.93f,1.f);
}
// Section markers on the ruler, by the kind of section.
inline juce::Colour sectionColour(int kind){
    static const juce::uint32 colours[]{0xff6f8fd8,0xff4fb8b0,0xff9a86e0,0xffd9a441,0xffe8774d,0xff7d8a99};   // main, intro, breakdown, buildup, drop, outro
    return juce::Colour(colours[std::clamp(kind,0,5)]);
}
// A clip's body: the track colour sunk into the well, so the notes drawn on it stay readable.
inline juce::Colour clipBody(juce::Colour c,bool selected){return well.interpolatedWith(c,selected?.52f:.24f);}
// Meter colour for a linear peak level.
inline juce::Colour meterColour(float peak){return peak>=1.f?meterHigh:peak>.5f?meterMid:meterLow;}
// Meter position for a linear peak: 0 at -60 dB, 1 at +6 dB, as the faders.
inline float meterPosition(float peak){return peak<=.001f?0.f:std::clamp((20.f*std::log10(peak)+60.f)/66.f,0.f,1.f);}
// Subtle vertical micro-gradient + 1px 8% top highlight for modern studio hardware depth.
inline void fillElevatedSurface(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour baseCol,float cornerRadius=0.f){
    g.setGradientFill(juce::ColourGradient(baseCol.brighter(.11f),r.getX(),r.getY(),baseCol.darker(.09f),r.getX(),r.getBottom(),false));
    if(cornerRadius>0.f)g.fillRoundedRectangle(r,cornerRadius);else g.fillRect(r);
    g.setColour(juce::Colours::white.withAlpha(.11f));
    g.fillRect(r.getX()+cornerRadius,r.getY(),std::max(0.f,r.getWidth()-2.f*cornerRadius),1.f);
    g.setColour(juce::Colours::black.withAlpha(.35f));
    g.fillRect(r.getX()+cornerRadius,r.getBottom()-1.f,std::max(0.f,r.getWidth()-2.f*cornerRadius),1.f);
}
// Hardware key shared by every button: a raised graphite key that sinks when pressed.
// selected = latched (amber frame, lit from inside); primary = main action (amber rim on top,
// no flat yellow fill); body = a custom key colour (mute, solo, sends), transparent = graphite.
struct KeyLook {bool hover=false,down=false,selected=false,primary=false;juce::Colour body{};bool joinLeft=false,joinRight=false;};
inline bool isNeutralKey(juce::Colour c){return c.isTransparent()||c==control||c==controlHi||c==surfaceHi||c==Palette::control||c==Palette::background;}
inline juce::Path keyShape(juce::Rectangle<float> r,float corner,const KeyLook& k){
    juce::Path p;p.addRoundedRectangle(r.getX(),r.getY(),r.getWidth(),r.getHeight(),corner,corner,!k.joinLeft,!k.joinRight,!k.joinLeft,!k.joinRight);return p;}
inline void drawKey(juce::Graphics& g,juce::Rectangle<float> bounds,const KeyLook& k,float corner=radius){
    const bool pressed=k.down||k.selected,custom=!isNeutralKey(k.body);
    auto r=bounds.withTrimmedBottom(1.f);const auto shape=keyShape(r,corner,k);
    juce::Colour top,bot;
    if(custom){top=pressed?k.body.darker(.28f):k.body.brighter(.12f);bot=pressed?k.body.darker(.06f):k.body.darker(.2f);}
    else{top=pressed?juce::Colour(26,28,33):juce::Colour(52,56,64);bot=juce::Colour(34,37,43);}
    if(k.hover&&!custom){const float a=k.selected?.18f:.10f,b=k.selected?.12f:.06f;top=top.interpolatedWith(accent,a);bot=bot.interpolatedWith(accent,b);}
    else if(k.hover){top=top.brighter(.08f);bot=bot.brighter(.06f);}
    if(!pressed){g.setColour(juce::Colours::black.withAlpha(.38f));g.fillPath(keyShape(r.translated(0,1.f),corner,k));}
    g.setGradientFill(juce::ColourGradient(top,r.getX(),r.getY(),bot,r.getX(),r.getBottom(),false));g.fillPath(shape);
    const float inset=k.joinLeft&&k.joinRight?0.f:corner;
    if(!pressed){
        g.setColour(custom?juce::Colours::white.withAlpha(.2f):juce::Colour(92,97,107));g.fillRect(r.getX()+inset,r.getY()+1.f,r.getWidth()-2*inset,1.f);
        g.setColour(juce::Colour(14,15,18).withAlpha(.9f));g.fillRect(r.getX()+inset,r.getBottom()-1.5f,r.getWidth()-2*inset,1.f);
    }else{
        juce::Graphics::ScopedSaveState s(g);g.reduceClipRegion(shape);
        g.setColour(juce::Colours::black.withAlpha(.38f));g.fillRect(r.getX(),r.getY(),r.getWidth(),1.f);
        g.setColour(juce::Colours::black.withAlpha(.18f));g.fillRect(r.getX(),r.getY()+1.f,r.getWidth(),1.f);
    }
    if(k.selected&&!custom){
        juce::Graphics::ScopedSaveState s(g);g.reduceClipRegion(shape);
        const auto c=r.getCentre();juce::ColourGradient glow(accent.withAlpha(0.f),c.x,c.y,accent.withAlpha(k.hover?.22f:.13f),r.getX(),r.getY(),true);
        g.setGradientFill(glow);g.fillRect(r);
        g.setColour(accent.darker(.15f));g.strokePath(keyShape(r.reduced(1.5f),std::max(1.f,corner-1.5f),k),juce::PathStrokeType(1.f));
    }
    if(k.primary){
        const auto bar=juce::Rectangle<float>(r.getX()+6.f,r.getY()+(pressed?3.f:2.f),std::max(0.f,r.getWidth()-12.f),2.f);
        g.setColour(accent.withAlpha(.22f));g.fillRoundedRectangle(bar.expanded(1.f,1.5f),2.f);
        g.setColour(accent);g.fillRoundedRectangle(bar,1.f);
    }
    g.setColour(pressed?juce::Colour(10,11,13):juce::Colour(16,17,20));g.strokePath(shape,juce::PathStrokeType(1.f));
}
// Label colour on a key.
inline juce::Colour keyInk(const KeyLook& k){
    if(k.selected&&isNeutralKey(k.body))return accent.interpolatedWith(juce::Colours::white,.25f);
    if(!isNeutralKey(k.body)&&k.body.getPerceivedBrightness()>.55f)return onAccent;
    return text;
}
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
        setColour(juce::AlertWindow::backgroundColourId,Daw::surfaceHi);
        setColour(juce::AlertWindow::textColourId,Daw::text);
        setColour(juce::AlertWindow::outlineColourId,Daw::lineStrong);
    }
    // Dialogs (save / discard, messages, rename): a raised panel in the theme, the title in
    // bold under an accent bar (no icon: the title says what it is).
    void drawAlertBox(juce::Graphics& g,juce::AlertWindow& w,const juce::Rectangle<int>& textArea,juce::TextLayout& text) override{
        const auto r=w.getLocalBounds().toFloat();
        g.setColour(Daw::surfaceHi);g.fillRoundedRectangle(r,6.f);
        g.setColour(Daw::lineStrong);g.drawRoundedRectangle(r.reduced(.5f),6.f,1.f);
        g.setColour(Daw::accent);g.fillRoundedRectangle(r.getX()+1.f,r.getY()+1.f,r.getWidth()-2.f,3.f,1.5f);
        g.setColour(Daw::text);text.draw(g,textArea.toFloat());
    }
    int getAlertBoxWindowFlags() override{return juce::ComponentPeer::windowAppearsOnTaskbar;}
    juce::Font getAlertWindowTitleFont() override{return Daw::font(Daw::title,true);}
    juce::Font getAlertWindowMessageFont() override{return Daw::font(Daw::body);}
    juce::Font getAlertWindowFont() override{return Daw::font(Daw::body);}
    int getAlertWindowButtonHeight() override{return 30;}
    // Hardware keys (Daw::drawKey). A button whose colour is the accent is a primary action;
    // a toggle whose on-colour is the accent is latched (amber frame); other colours stay keys of that colour.
    static Daw::KeyLook keyLookFor(juce::Button& b,const juce::Colour& background,bool highlighted,bool down){
        Daw::KeyLook k;const bool on=b.getToggleState();const auto onColour=b.findColour(juce::TextButton::buttonOnColourId);
        k.hover=highlighted&&b.isEnabled();k.down=down;k.joinLeft=b.isConnectedOnLeft();k.joinRight=b.isConnectedOnRight();
        k.primary=(background==Daw::accent||(bool)b.getProperties().getWithDefault("dawFeatured",false))&&!on;
        if(on){if(onColour==Daw::accent||onColour.getHue()==Daw::accent.getHue())k.selected=true;else k.body=onColour;}
        else if(!k.primary)k.body=background;
        return k;
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& background,bool highlighted,bool down) override{
        g.beginTransparencyLayer(b.isEnabled()?1.f:.45f);Daw::drawKey(g,b.getLocalBounds().toFloat().reduced(.5f),keyLookFor(b,background,highlighted,down));g.endTransparencyLayer();
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override{return Daw::font(Daw::small,true);}
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override{
        if((bool)b.getProperties().getWithDefault("fgaIconOnly",false)){auto r=b.getLocalBounds().toFloat().reduced(6.f);juce::Path pencil;pencil.startNewSubPath(r.getX()+2.f,r.getBottom()-2.f);pencil.lineTo(r.getRight()-3.f,r.getY()+3.f);g.setColour(b.isEnabled()?Daw::text:Daw::textFaint);g.strokePath(pencil,juce::PathStrokeType(1.5f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));return;}
        const auto k=keyLookFor(b,b.findColour(juce::TextButton::buttonColourId),false,b.isDown());const auto ink=Daw::keyInk(k);
        g.setColour(ink.withMultipliedAlpha(b.isEnabled()?1.f:.78f));auto f=getTextButtonFont(b,b.getHeight());f.setExtraKerningFactor(.03f);g.setFont(f);
        g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(4,1).withTrimmedBottom(1).translated(0,k.down||k.selected?1:0),juce::Justification::centred,1,.5f);
    }
    void drawComboBox(juce::Graphics& g,int width,int height,bool,int,int,int,int,juce::ComboBox& box) override{
        const auto r=juce::Rectangle<float>(0,0,(float)width,(float)height).reduced(.5f);
        const auto fill=box.findColour(juce::ComboBox::backgroundColourId);const bool flat=fill.isTransparent();
        if(!flat){Daw::fillElevatedSurface(g,r,box.isMouseOver(true)&&box.isEnabled()&&fill==Daw::control?Daw::controlHi:fill,Daw::radius);
            g.setColour(box.hasKeyboardFocus(true)?Daw::accent.withAlpha(.8f):juce::Colours::black.withAlpha(.45f));g.drawRoundedRectangle(r,Daw::radius,1.f);}
        const float cx=(float)width-10.f,cy=height*.5f;juce::Path chevron;chevron.startNewSubPath(cx-3.f,cy-1.5f);chevron.lineTo(cx,cy+1.5f);chevron.lineTo(cx+3.f,cy-1.5f);
        g.setColour(box.isEnabled()?Daw::textDim:Daw::textFaint.withAlpha(.5f));g.strokePath(chevron,juce::PathStrokeType(1.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
    juce::Font getComboBoxFont(juce::ComboBox& box) override{const float display=(float)box.getProperties().getWithDefault("dawDisplay",0.f);return display>0?Daw::font(display,true):Daw::font(Daw::body);}
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override{if((bool)box.getProperties().getWithDefault("transportReadout",false)){label.setBorderSize(juce::BorderSize<int>(0));label.setBounds(4,1,box.getWidth()-20,box.getHeight()-2);}else label.setBounds(3,1,box.getWidth()-20,box.getHeight()-2);label.setFont(getComboBoxFont(box));}
    void drawPopupMenuBackground(juce::Graphics& g,int width,int height) override{g.fillAll(Daw::surfaceHi);g.setColour(Daw::lineStrong);g.drawRect(0,0,width,height,1);}
    juce::Font getPopupMenuFont() override{return Daw::font(Daw::body);}
    // Linear sliders: a thin groove with the value in the accent and a small round thumb;
    // vertical sliders are mixer faders with a flat cap; bar styles draw nothing (a value
    // display, as the transport tempo).
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float minPos,float maxPos,juce::Slider::SliderStyle style,juce::Slider& s) override{
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
            g.setColour(juce::Colours::white.withAlpha(.15f*alpha));g.fillRect(cap.getX()+2.f,cap.getY()+1.f,cap.getWidth()-4.f,1.f);g.setColour(value==Daw::accent?Daw::accent.withAlpha(.95f*alpha):value.withMultipliedAlpha(alpha));g.fillRect(cap.getX()+3.f,pos-1.f,cap.getWidth()-6.f,2.f);
            return;
        }
        const bool horizontal=style!=juce::Slider::LinearVertical;
        if(!horizontal)return;
        const float cy=y+height*.5f;const auto groove=juce::Rectangle<float>((float)x,cy-1.5f,(float)width,3.f);
        g.setColour(juce::Colours::black.withAlpha(.55f*alpha));g.fillRoundedRectangle(groove,1.5f);
        const bool range=style==juce::Slider::TwoValueHorizontal;
        // A two-value slider has no centre value: JUCE supplies the two thumb
        // positions separately. Drawing `pos` here leaves a stationary ghost thumb.
        const float origin=range?minPos:s.getMinimum()<0&&s.getMaximum()>0&&std::abs(s.getMinimum()+s.getMaximum())<1e-9?(float)s.getPositionOfValue(0):(float)x;
        const float end=range?maxPos:pos;
        g.setColour(value.withMultipliedAlpha(alpha));g.fillRoundedRectangle(juce::Rectangle<float>(std::min(origin,end),cy-1.5f,std::abs(end-origin),3.f),1.5f);
        const auto drawThumb=[&](float position){
            const auto thumb=juce::Rectangle<float>(10.f,10.f).withCentre({position,cy});
            g.setColour(juce::Colours::black.withAlpha(.4f));g.fillEllipse(thumb.translated(0,1.f));
            g.setColour((s.isMouseOverOrDragging()?Daw::text:Daw::text.withAlpha(.85f)).withMultipliedAlpha(alpha));g.fillEllipse(thumb);
        };
        if(range){drawThumb(minPos);drawThumb(maxPos);}else drawThumb(pos);
    }
    // Knobs and faders never follow the mouse wheel: the wheel scrolls the view under them
    // (JUCE forwards it to the parent), values change only by clicking and dragging.
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& s) override{s.setScrollWheelEnabled(false);return FamilyLookAndFeel::getSliderLayout(s);}
    int getSliderThumbRadius(juce::Slider& s) override{return s.getSliderStyle()==juce::Slider::LinearVertical?7:5;}
    juce::Label* createSliderTextBox(juce::Slider& s) override{
        auto* l=FamilyLookAndFeel::createSliderTextBox(s);l->setFont(Daw::font(s.getSliderStyle()==juce::Slider::LinearBarVertical?Daw::display:11.f,s.getSliderStyle()==juce::Slider::LinearBarVertical));
        if(s.getSliderStyle()==juce::Slider::LinearBarVertical){l->setBorderSize(juce::BorderSize<int>(0));l->setJustificationType(juce::Justification::centredLeft);}
        l->setColour(juce::Label::outlineWhenEditingColourId,Daw::accent);l->setColour(juce::TextEditor::highlightColourId,Daw::accent.withAlpha(.4f));return l;}
    // Knobs: a thin track, the value arc from the left (or from the centre for a bipolar
    // range such as pan) and a flat cap with a pointer.
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,juce::Slider& s) override{
        const bool bipolar=s.getMinimum()<0&&std::abs(s.getMinimum()+s.getMaximum())<1e-9;
        const auto fill=s.findColour(juce::Slider::rotarySliderFillColourId);const auto ink=s.isColourSpecified(juce::Slider::rotarySliderFillColourId)?fill:Daw::accent;
        Daw::drawKnob(g,juce::Rectangle<int>(x,y,width,height).toFloat().reduced(2.f),pos,ink,bipolar,s.isEnabled(),s.isMouseOverOrDragging());
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
        g.setColour(down?Daw::accent.withAlpha(.8f):over?Daw::lineActive.brighter(.3f):Daw::lineActive);g.fillRoundedRectangle(thumb.reduced(2.5f),3.f);
    }
    int getDefaultScrollbarWidth() override{return 10;}
    void drawTooltip(juce::Graphics& g,const juce::String& s,int width,int height) override{
        const auto r=juce::Rectangle<float>(0,0,(float)width,(float)height);g.setColour(Daw::surfaceHi);g.fillRoundedRectangle(r,Daw::radius);
        g.setColour(Daw::lineStrong);g.drawRoundedRectangle(r.reduced(.5f),Daw::radius,1.f);
        g.setColour(Daw::text);g.setFont(FamilyTheme::font(13.f));g.drawFittedText(s,r.reduced(10.f,6.f).toNearestInt(),juce::Justification::centredLeft,8,1.f);
    }
};
