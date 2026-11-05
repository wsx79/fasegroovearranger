// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace workspace_detail {
inline juce::Colour colour(int t){return Daw::trackColour(t);}
inline juce::Colour guitarColour(int lane){return lane<=1?Palette::accent:lane<=4?Palette::accent2:lane<=6?Palette::percussion:lane<=8?Palette::cymbals:Palette::toms;}
inline juce::String utf(const std::string& s){return juce::String::fromUTF8(s.c_str());}
// Editor notifications invalidate one instance. Explicit saves also refresh opaque state.
inline void captureProjectStates(AudioEngine& audio,Project& project,bool fresh){if(fresh)audio.invalidatePluginStates();audio.capturePluginStates(project);}
inline void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size=12,juce::Colour c=Daw::text){g.setColour(c);g.setFont(Daw::font(size));g.drawText(tr(s),r,juce::Justification::centredLeft);}
constexpr int trackHeight=40,headerWidth=216,rulerHeight=60;
// Header controls come off the right edge in steps of Daw::unit, so hit-testing and
// painting share one rectangle. The automation slot still covers x=140.
struct HeaderChrome { juce::Rectangle<int> name,source,meter,led,lock,automation,mute,solo,gear; };
inline HeaderChrome headerChrome(int hx,int y,int h){
    HeaderChrome c;auto band=juce::Rectangle<int>(hx,y,headerWidth,h).withTrimmedRight(Daw::unit);const int side=5*Daw::unit;
    c.gear=band.removeFromRight(side);band.removeFromRight(Daw::unit);c.solo=band.removeFromRight(side);band.removeFromRight(Daw::unit);c.mute=band.removeFromRight(side);band.removeFromRight(Daw::unit);c.automation=band.removeFromRight(side);
    c.lock={c.automation.getX()-4*Daw::unit-2,y+Daw::unit,4*Daw::unit,4*Daw::unit};
    c.name={hx+9*Daw::unit,y+Daw::unit,std::max(24,c.lock.getX()-(hx+9*Daw::unit)-2),16};
    c.source={c.name.getX(),y+5*Daw::unit,c.name.getWidth(),12};
    // The meter and clip light stop before the automation button, so a click on A is not the clip reset.
    const int meterRight=c.automation.getX()-Daw::unit;
    c.led={meterRight-2*Daw::unit,y+h-2*Daw::unit-2,2*Daw::unit,6};
    c.meter={c.name.getX(),c.led.getY(),std::max(6*Daw::unit,c.led.getX()-c.name.getX()-Daw::unit),6};
    return c;
}
inline juce::ModifierKeys physicalMouseModifiers(){
    auto result=juce::ModifierKeys::getCurrentModifiersRealtime();
   #if JUCE_MAC
    // JUCE refreshes Cocoa keyboard modifiers but retains the last mouse event's
    // buttons. A missed mouse-up therefore needs the actual session button state.
    result=result.withoutMouseButtons();
    const int flags[]{juce::ModifierKeys::leftButtonModifier,juce::ModifierKeys::rightButtonModifier,juce::ModifierKeys::middleButtonModifier,juce::ModifierKeys::backButtonModifier,juce::ModifierKeys::forwardButtonModifier};
    for(int button=0;button<5;++button)if(CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState,static_cast<CGMouseButton>(button)))result=result.withFlags(flags[button]);
   #endif
    return result;
}
}

using namespace workspace_detail;
