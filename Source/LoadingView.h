// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "DawTheme.h"
namespace fga {
// Shared presentation for startup and asynchronous project/plugin restoration.
class LoadingView final : public juce::Component {
public:
    LoadingView():bar(progress){addAndMakeVisible(bar);bar.setPercentageDisplay(true);setWantsKeyboardFocus(true);}
    void show(juce::String text,double value=-1){status=std::move(text);progress=value;setVisible(true);toFront(false);repaint();}
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base.withAlpha(.9f));auto r=getLocalBounds().withSizeKeepingCentre(480,184);
        g.setColour(Daw::surfaceHi);g.fillRoundedRectangle(r.toFloat(),10.f);
        g.setColour(Daw::accent);g.drawRoundedRectangle(r.toFloat().reduced(.5f),10.f,1.f);
        const auto logo=juce::ImageCache::getFromMemory(BinaryData::arranger_png,BinaryData::arranger_pngSize);
        g.drawImageWithin(logo,r.getX()+24,r.getY()+24,56,56,juce::RectanglePlacement::centred);
        g.setColour(Daw::text);g.setFont(Daw::font(20,true));g.drawText("FASE GROOVE ARRANGER",r.getX()+96,r.getY()+28,360,28,juce::Justification::centredLeft);
        g.setFont(Daw::font(14));g.drawText(status,r.getX()+96,r.getY()+62,360,28,juce::Justification::centredLeft);
    }
    void resized()override{auto r=getLocalBounds().withSizeKeepingCentre(480,184);bar.setBounds(r.getX()+24,r.getY()+124,432,22);}
    bool keyPressed(const juce::KeyPress&)override{return true;}
private:
    double progress=-1;juce::ProgressBar bar;juce::String status;
};
}
