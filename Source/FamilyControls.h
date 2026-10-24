// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "FamilyTheme.h"
namespace fga {
class IconButton : public juce::Button
{
public:
    enum class Icon { none, play, pause, stop, rewind, dice, undo, redo, gear, close, fullScreen, exitFullScreen,
                      lock, unlock, left, right, song, folder, pencil, pointer, eraser };

    explicit IconButton (Icon i = Icon::none) : juce::Button ({}), icon (i) {}
    void setIcon (Icon i) { if (i != icon) { icon = i; repaint(); } }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);

    juce::Colour onColour { Palette::accent };
    juce::Colour iconColour;   // when set: the icon's own colour while off (the dice are amber)
    bool primary = false;      // filled with onColour even when off (the main action of a panel)

private:
    Icon icon;
};

}
