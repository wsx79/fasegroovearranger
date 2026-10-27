// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "DawTheme.h"
namespace fga {
class IconButton : public juce::Button
{
public:
    enum class Icon { none, play, pause, stop, rewind, dice, undo, redo, gear, close, fullScreen, exitFullScreen,
                      lock, unlock, left, right, song, folder, pencil, pointer, eraser,
                      loop, metronome, newFile, save, saveAs, exportFile, speaker, help, mixer, inspector, browser,
                      zoomSelection, fitWidth, follow, automation, loopSelection, piano, bass, strings, guitar, drum, lead, sparkles, studio, zoomIn, zoomOut };

    explicit IconButton (Icon i = Icon::none) : juce::Button ({}), icon (i) {}
    void setIcon (Icon i) { if (i != icon) { icon = i; repaint(); } }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);

    juce::Colour onColour { Palette::accent };
    juce::Colour iconColour;   // when set: the icon's own colour while off (the dice are amber)
    bool primary = false;      // filled with onColour even when off (the main action of a panel)
    bool quiet = false;        // toolbar style: no body until hovered; on = tinted with onColour

private:
    Icon icon;
};

// Labelled actions keep TextButton behaviour and localisation, with a shared vector glyph.
class ActionButton : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    void setIcon(IconButton::Icon next){glyph=next;repaint();}
    void paintButton(juce::Graphics&,bool,bool) override;
private:
    IconButton::Icon glyph=IconButton::Icon::none;
};
// Transparent target over the readout; global Space remains the transport shortcut.
class DisplayButton : public juce::Button {
public:
    DisplayButton():juce::Button("position-format"){setWantsKeyboardFocus(false);setMouseCursor(juce::MouseCursor::PointingHandCursor);}
    void paintButton(juce::Graphics& g,bool over,bool down)override{
        if(over||down||hasKeyboardFocus(true)){g.setColour(Daw::accent.withAlpha(down?.6f:.3f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f),4.f,1.f);}
    }
};
}
