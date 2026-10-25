// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// IconButton copied from Drums 7f1def6; uses the shared family font.
#include "FamilyControls.h"
namespace fga {
namespace
{
    /** Icons are drawn in a unit square, then fitted to the area. */
    juce::Path unitIcon (IconButton::Icon icon, float& strokeWidth, bool& filled)
    {
        using I = IconButton::Icon;
        juce::Path p;
        strokeWidth = 0.09f;
        filled = false;
        switch (icon)
        {
            case I::play:   p.addTriangle (0.28f, 0.18f, 0.28f, 0.82f, 0.82f, 0.5f); filled = true; break;
            case I::pause:  p.addRoundedRectangle (0.24f, 0.2f, 0.17f, 0.6f, 0.03f); p.addRoundedRectangle (0.59f, 0.2f, 0.17f, 0.6f, 0.03f); filled = true; break;
            case I::stop:   p.addRoundedRectangle (0.24f, 0.24f, 0.52f, 0.52f, 0.05f); filled = true; break;
            case I::rewind: p.addRectangle (0.2f, 0.22f, 0.11f, 0.56f); p.addTriangle (0.8f, 0.22f, 0.8f, 0.78f, 0.33f, 0.5f); filled = true; break;
            case I::dice:
                p.addRoundedRectangle (0.14f, 0.14f, 0.72f, 0.72f, 0.14f);
                for (auto pip : { juce::Point<float> (0.33f, 0.33f), { 0.67f, 0.33f }, { 0.5f, 0.5f }, { 0.33f, 0.67f }, { 0.67f, 0.67f } })
                    p.addEllipse (pip.x - 0.065f, pip.y - 0.065f, 0.13f, 0.13f);
                strokeWidth = 0.08f;
                break;
            case I::undo:
            case I::redo:
            {
                juce::Path a;
                a.startNewSubPath (0.16f, 0.36f);
                a.lineTo (0.6f, 0.36f);
                a.addCentredArc (0.6f, 0.57f, 0.21f, 0.21f, 0.0f, 0.0f, juce::MathConstants<float>::pi, false);
                a.lineTo (0.34f, 0.78f);
                a.startNewSubPath (0.34f, 0.18f); a.lineTo (0.16f, 0.36f); a.lineTo (0.34f, 0.54f);
                if (icon == I::redo)
                    a.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (1.0f, 0.0f));
                p = a;
                break;
            }
            case I::gear:
            {
                const int teeth = 8;
                const float outer = 0.46f, inner = 0.34f;
                for (int k = 0; k < teeth * 4; ++k)
                {
                    const float a = ((float) k / (float) (teeth * 4)) * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
                    const float r = (k % 4 == 0 || k % 4 == 1) ? outer : inner;
                    const juce::Point<float> pt (0.5f + r * std::cos (a), 0.5f + r * std::sin (a));
                    if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                }
                p.closeSubPath();
                p.addEllipse (0.36f, 0.36f, 0.28f, 0.28f);
                p.setUsingNonZeroWinding (false);
                filled = true;
                break;
            }
            case I::close:
                p.startNewSubPath (0.24f, 0.24f); p.lineTo (0.76f, 0.76f);
                p.startNewSubPath (0.76f, 0.24f); p.lineTo (0.24f, 0.76f);
                strokeWidth = 0.1f;
                break;
            case I::fullScreen:
            case I::exitFullScreen:
            {
                const bool in = icon == I::exitFullScreen;
                for (int c = 0; c < 4; ++c)
                {
                    const float x = c % 2 == 0 ? 0.16f : 0.84f, y = c < 2 ? 0.16f : 0.84f;
                    const float dx = c % 2 == 0 ? 0.24f : -0.24f, dy = c < 2 ? 0.24f : -0.24f;
                    if (! in) { p.startNewSubPath (x + dx, y); p.lineTo (x, y); p.lineTo (x, y + dy); }
                    else      { p.startNewSubPath (x + dx, y); p.lineTo (x + dx, y + dy); p.lineTo (x, y + dy); }
                }
                break;
            }
            case I::lock:
            case I::unlock:
                p.addRoundedRectangle (0.2f, 0.46f, 0.6f, 0.42f, 0.06f);
                p.startNewSubPath (0.32f, 0.46f);
                if (icon == I::lock)
                {
                    p.lineTo (0.32f, 0.32f);
                    p.addCentredArc (0.5f, 0.32f, 0.18f, 0.18f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
                    p.lineTo (0.68f, 0.46f);
                }
                else
                {
                    p.lineTo (0.32f, 0.3f);
                    p.addCentredArc (0.5f, 0.3f, 0.18f, 0.18f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
                    p.lineTo (0.68f, 0.34f);
                }
                strokeWidth = 0.09f;
                break;
            case I::left:  p.addTriangle (0.7f, 0.2f, 0.7f, 0.8f, 0.26f, 0.5f); filled = true; break;
            case I::right: p.addTriangle (0.3f, 0.2f, 0.3f, 0.8f, 0.74f, 0.5f); filled = true; break;
            case I::song:
                // The song timeline: blocks of different lengths.
                p.addRoundedRectangle (0.06f, 0.36f, 0.2f, 0.28f, 0.04f);
                p.addRoundedRectangle (0.3f, 0.36f, 0.36f, 0.28f, 0.04f);
                p.addRoundedRectangle (0.7f, 0.36f, 0.24f, 0.28f, 0.04f);
                filled = true;
                break;
            case I::folder:
                p.startNewSubPath (0.1f, 0.24f); p.lineTo (0.4f, 0.24f); p.lineTo (0.48f, 0.34f); p.lineTo (0.9f, 0.34f);
                p.lineTo (0.9f, 0.8f); p.lineTo (0.1f, 0.8f); p.closeSubPath();
                strokeWidth = 0.08f;
                break;
            case I::pencil:
            {
                // Pencil at 45 degrees: body, ferrule line and tip.
                p.startNewSubPath (0.68f, 0.14f); p.lineTo (0.86f, 0.32f); p.lineTo (0.36f, 0.82f);
                p.lineTo (0.14f, 0.86f); p.lineTo (0.18f, 0.64f); p.closeSubPath();
                p.startNewSubPath (0.58f, 0.24f); p.lineTo (0.76f, 0.42f);
                p.startNewSubPath (0.18f, 0.64f); p.lineTo (0.36f, 0.82f);
                strokeWidth = 0.08f;
                break;
            }
            case I::pointer:
                // Arrow cursor.
                p.startNewSubPath (0.26f, 0.12f); p.lineTo (0.26f, 0.78f); p.lineTo (0.42f, 0.63f);
                p.lineTo (0.53f, 0.88f); p.lineTo (0.64f, 0.83f); p.lineTo (0.53f, 0.59f);
                p.lineTo (0.74f, 0.58f); p.closeSubPath();
                strokeWidth = 0.075f;
                break;
            case I::eraser:
            {
                // Tilted eraser block over a base line; the lower half is the rubber.
                p.startNewSubPath (0.56f, 0.14f); p.lineTo (0.88f, 0.46f); p.lineTo (0.5f, 0.84f);
                p.lineTo (0.3f, 0.84f); p.lineTo (0.14f, 0.68f); p.closeSubPath();
                p.startNewSubPath (0.36f, 0.34f); p.lineTo (0.68f, 0.66f);
                p.startNewSubPath (0.5f, 0.88f); p.lineTo (0.9f, 0.88f);
                strokeWidth = 0.08f;
                break;
            }
            case I::loop:
            {
                // Two arrows chasing each other around a rounded rectangle.
                p.startNewSubPath (0.3f, 0.7f); p.lineTo (0.24f, 0.7f);
                p.quadraticTo (0.12f, 0.7f, 0.12f, 0.58f); p.lineTo (0.12f, 0.42f); p.quadraticTo (0.12f, 0.3f, 0.24f, 0.3f); p.lineTo (0.7f, 0.3f);
                p.startNewSubPath (0.6f, 0.2f); p.lineTo (0.72f, 0.3f); p.lineTo (0.6f, 0.4f);
                p.startNewSubPath (0.7f, 0.3f); p.lineTo (0.76f, 0.3f); p.quadraticTo (0.88f, 0.3f, 0.88f, 0.42f); p.lineTo (0.88f, 0.58f); p.quadraticTo (0.88f, 0.7f, 0.76f, 0.7f); p.lineTo (0.3f, 0.7f);
                p.startNewSubPath (0.4f, 0.6f); p.lineTo (0.28f, 0.7f); p.lineTo (0.4f, 0.8f);
                strokeWidth = 0.075f;
                break;
            }
            case I::metronome:
                p.startNewSubPath (0.4f, 0.14f); p.lineTo (0.6f, 0.14f); p.lineTo (0.8f, 0.86f); p.lineTo (0.2f, 0.86f); p.closeSubPath();
                p.startNewSubPath (0.27f, 0.66f); p.lineTo (0.73f, 0.66f);
                p.startNewSubPath (0.5f, 0.66f); p.lineTo (0.76f, 0.24f);
                strokeWidth = 0.075f;
                break;
            case I::newFile:
            case I::save:
            case I::saveAs:
            case I::exportFile:
                if (icon == I::newFile)
                {
                    // A page with a folded corner and a plus.
                    p.startNewSubPath (0.24f, 0.1f); p.lineTo (0.58f, 0.1f); p.lineTo (0.78f, 0.3f); p.lineTo (0.78f, 0.9f); p.lineTo (0.24f, 0.9f); p.closeSubPath();
                    p.startNewSubPath (0.58f, 0.1f); p.lineTo (0.58f, 0.3f); p.lineTo (0.78f, 0.3f);
                    p.startNewSubPath (0.51f, 0.48f); p.lineTo (0.51f, 0.76f); p.startNewSubPath (0.37f, 0.62f); p.lineTo (0.65f, 0.62f);
                }
                else if (icon == I::exportFile)
                {
                    // An arrow leaving a tray.
                    p.startNewSubPath (0.16f, 0.56f); p.lineTo (0.16f, 0.86f); p.lineTo (0.84f, 0.86f); p.lineTo (0.84f, 0.56f);
                    p.startNewSubPath (0.5f, 0.66f); p.lineTo (0.5f, 0.12f);
                    p.startNewSubPath (0.3f, 0.32f); p.lineTo (0.5f, 0.12f); p.lineTo (0.7f, 0.32f);
                }
                else
                {
                    // A floppy disk; SAVE AS adds a pencil stroke.
                    p.startNewSubPath (0.14f, 0.14f); p.lineTo (0.7f, 0.14f); p.lineTo (0.86f, 0.3f); p.lineTo (0.86f, 0.86f); p.lineTo (0.14f, 0.86f); p.closeSubPath();
                    p.startNewSubPath (0.3f, 0.14f); p.lineTo (0.3f, 0.34f); p.lineTo (0.62f, 0.34f); p.lineTo (0.62f, 0.14f);
                    p.addRoundedRectangle (0.3f, 0.54f, 0.4f, 0.32f, 0.03f);
                    if (icon == I::saveAs) { p.startNewSubPath (0.62f, 0.98f); p.lineTo (0.98f, 0.62f); }
                }
                strokeWidth = 0.075f;
                break;
            case I::speaker:
                p.startNewSubPath (0.12f, 0.38f); p.lineTo (0.3f, 0.38f); p.lineTo (0.52f, 0.18f); p.lineTo (0.52f, 0.82f); p.lineTo (0.3f, 0.62f); p.lineTo (0.12f, 0.62f); p.closeSubPath();
                p.addCentredArc (0.52f, 0.5f, 0.18f, 0.18f, 0.0f, 0.75f, 2.39f, true);
                p.addCentredArc (0.52f, 0.5f, 0.34f, 0.34f, 0.0f, 0.75f, 2.39f, true);
                strokeWidth = 0.075f;
                break;
            case I::help:
                p.addEllipse (0.12f, 0.12f, 0.76f, 0.76f);
                p.startNewSubPath (0.38f, 0.4f);
                p.cubicTo (0.38f, 0.26f, 0.62f, 0.26f, 0.62f, 0.4f);
                p.cubicTo (0.62f, 0.5f, 0.5f, 0.5f, 0.5f, 0.6f);
                p.addEllipse (0.47f, 0.68f, 0.06f, 0.06f);
                strokeWidth = 0.075f;
                break;
            case I::mixer:
                for (int k = 0; k < 3; ++k)
                {
                    const float x = 0.22f + 0.28f * (float) k, knob = k == 0 ? 0.62f : k == 1 ? 0.36f : 0.54f;
                    p.startNewSubPath (x, 0.14f); p.lineTo (x, knob - 0.08f);
                    p.startNewSubPath (x, knob + 0.08f); p.lineTo (x, 0.86f);
                    p.addRoundedRectangle (x - 0.1f, knob - 0.08f, 0.2f, 0.16f, 0.03f);
                }
                strokeWidth = 0.075f;
                break;
            case I::inspector:
            case I::browser:
            {
                p.addRoundedRectangle (0.1f, 0.18f, 0.8f, 0.64f, 0.06f);
                const float x = icon == I::inspector ? 0.38f : 0.62f;
                p.startNewSubPath (x, 0.18f); p.lineTo (x, 0.82f);
                const float from = icon == I::inspector ? 0.18f : 0.7f;
                for (float y : { 0.34f, 0.46f, 0.58f }) { p.startNewSubPath (from, y); p.lineTo (from + 0.12f, y); }
                strokeWidth = 0.075f;
                break;
            }
            case I::zoomSelection:
                p.addEllipse (0.12f, 0.12f, 0.52f, 0.52f);
                p.startNewSubPath (0.57f, 0.57f); p.lineTo (0.86f, 0.86f);
                p.startNewSubPath (0.27f, 0.38f); p.lineTo (0.49f, 0.38f);
                p.startNewSubPath (0.38f, 0.27f); p.lineTo (0.38f, 0.49f);
                strokeWidth = 0.085f;
                break;
            case I::fitWidth:
                p.startNewSubPath (0.1f, 0.2f); p.lineTo (0.1f, 0.8f);
                p.startNewSubPath (0.9f, 0.2f); p.lineTo (0.9f, 0.8f);
                p.startNewSubPath (0.24f, 0.5f); p.lineTo (0.76f, 0.5f);
                p.startNewSubPath (0.36f, 0.38f); p.lineTo (0.24f, 0.5f); p.lineTo (0.36f, 0.62f);
                p.startNewSubPath (0.64f, 0.38f); p.lineTo (0.76f, 0.5f); p.lineTo (0.64f, 0.62f);
                strokeWidth = 0.08f;
                break;
            case I::follow:
                // The playhead with an arrow pulling the view along.
                p.startNewSubPath (0.62f, 0.12f); p.lineTo (0.62f, 0.88f);
                p.startNewSubPath (0.12f, 0.5f); p.lineTo (0.46f, 0.5f);
                p.startNewSubPath (0.34f, 0.36f); p.lineTo (0.48f, 0.5f); p.lineTo (0.34f, 0.64f);
                p.startNewSubPath (0.76f, 0.3f); p.lineTo (0.9f, 0.5f); p.lineTo (0.76f, 0.7f);
                strokeWidth = 0.08f;
                break;
            case I::automation:
                p.startNewSubPath (0.1f, 0.72f); p.lineTo (0.34f, 0.72f); p.lineTo (0.62f, 0.28f); p.lineTo (0.9f, 0.28f);
                p.addEllipse (0.27f, 0.65f, 0.14f, 0.14f);
                p.addEllipse (0.55f, 0.21f, 0.14f, 0.14f);
                strokeWidth = 0.075f;
                break;
            case I::loopSelection:
                p.startNewSubPath (0.26f, 0.16f); p.lineTo (0.12f, 0.16f); p.lineTo (0.12f, 0.84f); p.lineTo (0.26f, 0.84f);
                p.startNewSubPath (0.74f, 0.16f); p.lineTo (0.88f, 0.16f); p.lineTo (0.88f, 0.84f); p.lineTo (0.74f, 0.84f);
                p.addRoundedRectangle (0.3f, 0.38f, 0.4f, 0.24f, 0.04f);
                strokeWidth = 0.08f;
                break;
            case I::lead:
                p.addRoundedRectangle (0.08f, 0.64f, 0.2f, 0.1f, 0.025f);
                p.addRoundedRectangle (0.3f, 0.46f, 0.2f, 0.1f, 0.025f);
                p.addRoundedRectangle (0.52f, 0.26f, 0.2f, 0.1f, 0.025f);
                p.addRoundedRectangle (0.74f, 0.46f, 0.18f, 0.1f, 0.025f);
                strokeWidth = 0.07f;
                break;
            case I::piano:
                p.addRoundedRectangle (0.1f, 0.16f, 0.8f, 0.68f, 0.05f);
                for (float x : { 0.3f, 0.5f, 0.7f }) { p.startNewSubPath (x, 0.5f); p.lineTo (x, 0.84f); }
                for (float x : { 0.3f, 0.5f, 0.7f }) p.addRectangle (x - 0.055f, 0.16f, 0.11f, 0.34f);
                strokeWidth = 0.07f;
                break;
            case I::bass:
            case I::guitar:
            {
                // A guitar body seen at an angle; the bass has a longer neck.
                const float neck = icon == I::bass ? 0.06f : 0.18f;
                p.addEllipse (0.12f, 0.5f, 0.34f, 0.34f);
                p.addEllipse (0.24f, 0.36f, 0.28f, 0.28f);
                p.startNewSubPath (0.46f, 0.54f); p.lineTo (0.88f - neck * 0.3f, neck + 0.04f);
                p.startNewSubPath (0.8f - neck * 0.3f, neck); p.lineTo (0.94f - neck * 0.3f, neck + 0.12f);
                strokeWidth = 0.075f;
                break;
            }
            case I::strings:
                // A violin scroll and f-holes, reduced to a bow over strings.
                for (float x : { 0.34f, 0.46f, 0.58f, 0.7f }) { p.startNewSubPath (x, 0.12f); p.lineTo (x, 0.88f); }
                p.startNewSubPath (0.12f, 0.74f); p.lineTo (0.88f, 0.3f);
                strokeWidth = 0.07f;
                break;
            case I::drum:
                p.addEllipse (0.14f, 0.24f, 0.72f, 0.24f);
                p.startNewSubPath (0.14f, 0.36f); p.lineTo (0.14f, 0.72f);
                p.quadraticTo (0.5f, 0.92f, 0.86f, 0.72f); p.lineTo (0.86f, 0.36f);
                p.startNewSubPath (0.3f, 0.06f); p.lineTo (0.5f, 0.32f);
                strokeWidth = 0.075f;
                break;
            case I::none:
            default: break;
        }
        return p;
    }
}

void IconButton::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour colour)
{
    if (icon == Icon::none)
        return;
    float stroke = 0.09f;
    bool filled = false;
    auto p = unitIcon (icon, stroke, filled);
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const auto box = r.withSizeKeepingCentre (s, s);
    p.applyTransform (juce::AffineTransform::scale (s).translated (box.getX(), box.getY()));
    g.setColour (colour);
    if (filled)
        g.fillPath (p);
    else
        g.strokePath (p, juce::PathStrokeType (juce::jmax (1.2f, stroke * s), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const bool filled = (on && ! quiet) || primary;
    const bool flatLeft = isConnectedOnLeft(), flatRight = isConnectedOnRight();

    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), Daw::radius, Daw::radius, ! flatLeft, ! flatRight, ! flatLeft, ! flatRight);
    if (quiet)
    {
        // Toolbar icons: no body until hovered; a toggled icon sits on a tint of its colour.
        if (on)                     { g.setColour (onColour.withAlpha (down ? 0.3f : highlighted ? 0.26f : 0.2f)); g.fillPath (shape); }
        else if (down)              { g.setColour (Daw::control.darker (0.2f)); g.fillPath (shape); }
        else if (highlighted && isEnabled()) { g.setColour (Daw::controlHi); g.fillPath (shape); }
    }
    else
    {
        auto fillColour = filled ? onColour : Daw::control;
        if (down)             fillColour = fillColour.darker (0.18f);
        else if (highlighted) fillColour = filled ? fillColour.brighter (0.08f) : Daw::controlHi;
        if (! isEnabled())    fillColour = fillColour.withMultipliedAlpha (0.45f);
        g.setColour (fillColour);
        g.fillPath (shape);
        if (! down && isEnabled())
        {
            g.setColour (juce::Colours::white.withAlpha (filled ? 0.18f : 0.045f));
            g.fillRect (r.getX() + Daw::radius, r.getY() + 0.5f, r.getWidth() - 2.0f * Daw::radius, 1.0f);
        }
        g.setColour (hasKeyboardFocus (true) ? Daw::accent.withAlpha (0.8f) : filled ? fillColour.darker (0.3f) : juce::Colours::black.withAlpha (0.45f));
        g.strokePath (shape, juce::PathStrokeType (1.0f));
    }

    auto ink = filled ? Daw::onAccent : quiet && on ? onColour : Daw::text.withAlpha (quiet ? 0.82f : 0.92f);
    if (quiet && highlighted && ! on) ink = Daw::text;
    if (! isEnabled())
        ink = ink.withMultipliedAlpha (0.35f);
    const auto iconInk = ! filled && ! on && ! iconColour.isTransparent() ? iconColour.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f) : ink;
    const float iconSize = juce::jmin (r.getHeight() * 0.58f, 16.0f);
    const auto label = getButtonText();

    auto content = r.reduced (6.0f, 0.0f);
    if (label.isEmpty())
    {
        drawIcon (g, icon, content.withSizeKeepingCentre (iconSize, iconSize), iconInk);
        return;
    }

    auto f = FamilyTheme::font (juce::jlimit (9.5f, 11.5f, r.getHeight() * 0.42f), true);
    f.setExtraKerningFactor (0.03f);
    const float textW = juce::GlyphArrangement::getStringWidth (f, label);
    const float iconsW = icon == Icon::none ? 0.0f : iconSize + 6.0f;
    auto area = content.withSizeKeepingCentre (juce::jmin (content.getWidth(), iconsW + textW), content.getHeight());
    if (icon != Icon::none)
    {
        drawIcon (g, icon, area.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize), iconInk);
        area.removeFromLeft (6.0f);
    }
    g.setColour (ink);
    g.setFont (f);
    g.drawFittedText (label, area.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
}

}
