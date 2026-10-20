// Fase Groove Guitar - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// App identity: the Fase Groove Guitar logo (icon + wordmark) and the credits
// artwork. Shared by the editor, the About page and the icon/README renders.

#include <juce_gui_basics/juce_gui_basics.h>

namespace Branding
{
    inline constexpr const char* appName   = "Fase Groove Guitar";
    inline constexpr const char* tagline   = "house guitar grooves for soulful, deep & funky house";
    inline constexpr const char* artist    = "Fabio 'Pitch' Serra";
    inline constexpr const char* licence   = "Free & open source - GNU AGPL v3";
    inline constexpr const char* sourceUrl = "https://github.com/wsx79/fasegrooveguitar";

    /** Donation pages (an empty URL hides its button). */
    inline constexpr const char* kofiUrl   = "https://ko-fi.com/fabios1979";
    inline constexpr const char* paypalUrl = "https://paypal.me/fabios79";

    /** Fabio 'Pitch' Serra's artist page on Spotify (empty hides the button). */
    inline constexpr const char* spotifyUrl = "https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP";

    /** The square app icon (a slanted guitar neck with a chord on it). */
    std::unique_ptr<juce::Drawable> createIcon();

    /** Fabio 'Pitch' Serra's FS PITCH logo (white on transparent). */
    juce::Image artistLogo();

    /** "FASE" + "GROOVE GUITAR", slanted like the FS logo, fitted inside `area`. */
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area);

    /** Icon on the left, wordmark on the right. */
    void drawFullLogo (juce::Graphics&, juce::Rectangle<float> area);
}

