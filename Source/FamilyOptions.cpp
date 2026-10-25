// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// OptionsOverlay from Fase Groove Guitar e571f8d: same tabs, settings and credits geometry.
#include "FamilyOptions.h"
#include "Branding.h"
#include "Localisation.h"
#include "Updater.h"
namespace fga {
OptionsOverlay::OptionsOverlay() : artist (Branding::artistLogo())
{
    setInterceptsMouseClicks (true, true);

    auto styleTab = [this] (juce::TextButton& b)
    {
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, Palette::background);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::accent);
        b.setColour (juce::TextButton::textColourOffId, Palette::text);
        b.setColour (juce::TextButton::textColourOnId, Palette::background);
        addAndMakeVisible (b);
    };
    styleTab (settingsTab);
    styleTab (aboutTab);
    styleTab (closeButton);
    settingsTab.onClick = [this] { selectTab (false); };
    aboutTab.onClick    = [this] { selectTab (true); };
    closeButton.onClick = [this] { setVisible (false); };

    auto setUpLink = [this] (juce::TextButton& b, const char* url, juce::Colour colour, juce::Colour text)
    {
        b.setColour (juce::TextButton::buttonColourId, colour);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.onClick = [url] { juce::URL (url).launchInDefaultBrowser(); };
        addChildComponent (b);
    };
    setUpLink (sourceButton, Branding::sourceUrl, Palette::background, Palette::text);
    setUpLink (kofiButton, Branding::kofiUrl, Palette::accent, Palette::background);
    setUpLink (paypalButton, Branding::paypalUrl, Palette::accent2, Palette::background);
    setUpLink (spotifyButton, Branding::spotifyUrl, juce::Colour (0xff1db954), Palette::background);

    // Settings tab
    for (auto* l : { &languageLabel, &sizeLabel })
    {
        l->setFont (FamilyTheme::font (11.0f, true));
        l->setColour (juce::Label::textColourId, Palette::dim);
        addChildComponent (l);
    }

    for (int i = 0; i < Localisation::numLanguages; ++i)
        languageBox.addItem (juce::String::fromUTF8 (Localisation::languageName (i)), i + 1);
    languageBox.onChange = [this]
    {
        Localisation::setCurrent ((Localisation::Language) (languageBox.getSelectedItemIndex()));
        refreshText();
        if (onLanguageChanged) onLanguageChanged();
    };
    addChildComponent (languageBox);

    for (int percent : { 60, 75, 90, 100, 110, 125, 150, 175, 200 })
        sizeBox.addItem (juce::String (percent) + " %", percent);
    sizeBox.onChange = [this]
    {
        if (onScaleChosen != nullptr && sizeBox.getSelectedId() > 0)
            onScaleChosen (sizeBox.getSelectedId() / 100.0f);
    };
    addChildComponent (sizeBox);

    tipsToggle.onClick = [this] { if (onTipsChanged != nullptr) onTipsChanged (tipsToggle.getToggleState()); };
    addChildComponent (tipsToggle);
    updatesToggle.onClick = [this] { Updater::setAutoCheckEnabled (updatesToggle.getToggleState()); };
    addChildComponent (updatesToggle);
    checkUpdatesButton.onClick = [this] { if (onCheckUpdates != nullptr) onCheckUpdates(); };
    addChildComponent (checkUpdatesButton);

    pluginButton.onClick=[this]{if(onPluginSettings)onPluginSettings();};
    pluginButton.setTooltip(tr("Choose plugin folders and scan installed instruments."));
    audioButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
    guideButton.onClick = [this] { if (onDocument) onDocument(0); };
    privacyButton.onClick = [this] { if (onDocument) onDocument(1); };
    licencesButton.onClick = [this] { if (onDocument) onDocument(2); };
    for (auto* b : { &pluginButton, &audioButton, &guideButton, &privacyButton, &licencesButton }) addChildComponent(b);
    refreshText();
    selectTab (false);
}

void OptionsOverlay::show (bool about, float currentScale)
{
    languageBox.setSelectedItemIndex ((int) Localisation::current(), juce::dontSendNotification);

    int closest = 100;
    for (int i = 0; i < sizeBox.getNumItems(); ++i)
        if (std::abs (sizeBox.getItemId (i) - currentScale * 100.0f) < std::abs (closest - currentScale * 100.0f))
            closest = sizeBox.getItemId (i);
    sizeBox.setSelectedId (closest, juce::dontSendNotification);

    tipsToggle.setToggleState (Localisation::tipsEnabled(), juce::dontSendNotification);
    updatesToggle.setToggleState (Updater::autoCheckEnabled(), juce::dontSendNotification);
    refreshText();
    selectTab (about);
    setVisible (true);
    toFront (false);
}

void OptionsOverlay::refreshText()
{
    pluginButton.setButtonText(tr("PLUGINS..."));pluginButton.setTooltip(tr("Choose plugin folders and scan installed instruments."));
    audioButton.setButtonText(tr("Audio/MIDI Settings"));
    guideButton.setButtonText(tr("GUIDE"));
    privacyButton.setButtonText(tr("Privacy and permissions"));
    licencesButton.setButtonText(tr("Licences and attributions"));
    settingsTab.setButtonText (tr ("SETTINGS"));
    aboutTab.setButtonText (tr ("ABOUT"));
    closeButton.setButtonText (tr ("CLOSE"));
    sourceButton.setButtonText (tr ("SOURCE CODE"));
    kofiButton.setButtonText (tr ("DONATE: KO-FI"));
    paypalButton.setButtonText (tr ("DONATE: PAYPAL"));
    spotifyButton.setButtonText (tr ("SPOTIFY"));
    spotifyButton.setTooltip (tr ("Listen to my productions on Spotify"));
    languageLabel.setText (Localisation::upper (tr ("Language")), juce::dontSendNotification);
    sizeLabel.setText (Localisation::upper (tr ("Interface size")), juce::dontSendNotification);
    tipsToggle.setButtonText (tr ("Show tips when the mouse rests on a control"));
    updatesToggle.setButtonText (tr ("Check for updates automatically (GitHub)"));
    if (checkUpdatesButton.isEnabled())
        checkUpdatesButton.setButtonText (tr ("CHECK NOW"));
    repaint();
}

void OptionsOverlay::selectTab (bool about)
{
    showingAbout = about;
    settingsTab.setToggleState (! about, juce::dontSendNotification);
    aboutTab.setToggleState (about, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &languageLabel, &languageBox, &sizeLabel, &sizeBox, &tipsToggle,
                                                             &updatesToggle, &checkUpdatesButton, &pluginButton, &audioButton, &guideButton, &privacyButton, &licencesButton })
        c->setVisible (! about);

    auto showLink = [about] (juce::Component& b, const char* url) { b.setVisible (about && juce::String (url).isNotEmpty()); };
    showLink (sourceButton, Branding::sourceUrl);
    showLink (kofiButton, Branding::kofiUrl);
    showLink (paypalButton, Branding::paypalUrl);
    showLink (spotifyButton, Branding::spotifyUrl);

    resized();
    repaint();
}

void OptionsOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (! card().contains (e.position))
        setVisible (false);
}

void OptionsOverlay::resized()
{
    auto inner = card().toNearestInt().reduced (24, 18);

    auto tabs = inner.removeFromTop (30);
    closeButton.setBounds (tabs.removeFromRight (100));
    settingsTab.setBounds (tabs.removeFromLeft (150));
    tabs.removeFromLeft (8);
    aboutTab.setBounds (tabs.removeFromLeft (150));

    // Settings tab
    auto settings = inner.withTrimmedTop (40).reduced (40, 0);
    languageLabel.setBounds (settings.removeFromTop (18));
    languageBox.setBounds (settings.removeFromTop (30).withWidth (260));
    settings.removeFromTop (22);
    sizeLabel.setBounds (settings.removeFromTop (18));
    sizeBox.setBounds (settings.removeFromTop (30).withWidth (260));
    settings.removeFromTop (22);
    tipsToggle.setBounds (settings.removeFromTop (26).withWidth (460));
    settings.removeFromTop (10);
    {
        auto row = settings.removeFromTop (26);
        updatesToggle.setBounds (row.removeFromLeft (380));
        checkUpdatesButton.setBounds (row.removeFromLeft (130).reduced (0, 1));
    }

    settings.removeFromTop (16);
    auto extra = settings.removeFromTop(28);
    audioButton.setBounds(extra.removeFromLeft(260));
    extra.removeFromLeft(8);
    guideButton.setBounds(extra.removeFromLeft(130));
    extra.removeFromLeft(8);pluginButton.setBounds(extra.removeFromLeft(86));
    settings.removeFromTop(8);
    extra = settings.removeFromTop(28);
    privacyButton.setBounds(extra.removeFromLeft(260));
    extra.removeFromLeft(8);
    licencesButton.setBounds(extra.removeFromLeft(180));

    // About tab: link buttons above the footer
    auto row = inner.withTrimmedBottom (26).removeFromBottom (30);
    juce::Array<juce::Component*> buttons;
    for (auto* b : { &sourceButton, &kofiButton, &paypalButton, &spotifyButton })
        if (b->isVisible())
            buttons.add (b);

    const int w = 126, gap = 10;
    row = row.withSizeKeepingCentre (juce::jmax (1, buttons.size() * w + (buttons.size() - 1) * gap), row.getHeight());
    for (auto* b : buttons)
    {
        b->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gap);
    }
}

void OptionsOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));

    auto card = this->card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (card, 16.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (card, 10.0f, 1.0f);

    auto r = card.reduced (36.0f, 18.0f);
    r.removeFromTop (46.0f);   // tabs
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");

    if (! showingAbout)
    {
        // Under the shared settings: the standalone arranger shortcuts.
        auto keys = juce::Rectangle<float> (r.getX() + 28.0f, r.getY() + 336.0f, r.getWidth() - 28.0f, 220.0f);
        g.setColour (Palette::dim);
        g.setFont (FamilyTheme::font (11.0f, true));
        g.drawText (Localisation::upper (tr ("Shortcuts")), keys.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setColour (Palette::text.withAlpha (0.85f));
        g.setFont (FamilyTheme::font (12.0f));
        for (const auto* line : { "Space: play / pause      Ctrl+Z: undo      Ctrl+Shift+Z / Ctrl+Y: redo",
                                  "Track name: instrument settings. Clip: MIDI editor. Escape: arrangement.",
                                  "MIDI editor: draw, move, resize; Alt-drag changes velocity.",
                                  "Ctrl/Cmd+S: save      Ctrl/Cmd+C / V: copy / paste notes",
                                  "Drop a .fga project, MIDI file, sample or instrument on the window." })
            g.drawFittedText (tr (line), keys.removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

        g.setColour (Palette::dim);
        g.setFont (FamilyTheme::font (12.0f));
        g.drawText (tr ("Settings are saved for all your projects."),
                    juce::Rectangle<float> (r.getX() + 28.0f, r.getY() + 500.0f, r.getWidth() - 28.0f, 18.0f), juce::Justification::centredLeft);
        g.setFont (FamilyTheme::font (11.0f));
        g.drawText ("Fase Groove Arranger " + juce::String (JUCE_APPLICATION_VERSION_STRING) + dot + tr ("Built with JUCE"),
                    r.removeFromBottom (16.0f), juce::Justification::centred);
        return;
    }

    Branding::drawFullLogo (g, r.removeFromTop (70.0f).withSizeKeepingCentre (340.0f, 70.0f));

    g.setColour (Palette::dim);
    g.setFont (FamilyTheme::font (13.0f));
    g.drawText (tr ("Version") + " " + juce::String (JUCE_APPLICATION_VERSION_STRING) + dot + tr (Branding::tagline),
                r.removeFromTop (26.0f), juce::Justification::centred);

    r.removeFromTop (8.0f);
    g.setColour (Palette::panelEdge);
    g.fillRect (r.removeFromTop (1.0f));
    r.removeFromTop (12.0f);

    auto caption = [&g] (const juce::String& text, juce::Rectangle<float> area)
    {
        g.setColour (Palette::accent);
        g.setFont (FamilyTheme::font (11.0f, true));
        g.drawText (text, area, juce::Justification::centred);
    };

    caption (tr ("CREATED BY"), r.removeFromTop (16.0f));
    r.removeFromTop (6.0f);
    if (artist.isValid())
        g.drawImageWithin (artist, (int) r.getX(), (int) r.getY(), (int) r.getWidth(), 110,
                           juce::RectanglePlacement::centred);
    r.removeFromTop (114.0f);

    g.setColour (Palette::text);
    g.setFont (FamilyTheme::font (14.0f));
    g.drawText (tr ("DJ & Producer"), r.removeFromTop (20.0f), juce::Justification::centred);
    g.setColour (juce::Colour (0xff1db954));
    g.setFont (FamilyTheme::font (12.0f, true));
    g.drawText (tr ("Listen to my productions on Spotify"), r.removeFromTop (18.0f), juce::Justification::centred);

    r.removeFromTop (12.0f);
    caption (tr ("CODE"), r.removeFromTop (16.0f));
    r.removeFromTop (4.0f);
    g.setColour (Palette::text);
    g.setFont (FamilyTheme::font (16.0f, true));
    g.drawText ("Claude (Anthropic) / Codex (OpenAI)", r.removeFromTop (22.0f), juce::Justification::centred);
    g.setColour (Palette::dim);
    g.setFont (FamilyTheme::font (12.0f));
    g.drawText (tr ("AI coding assistant"), r.removeFromTop (16.0f), juce::Justification::centred);

    g.setFont (FamilyTheme::font (11.0f));
    g.drawText (tr ("Built with JUCE") + dot + "VST is a registered trademark of Steinberg Media Technologies GmbH",
                r.removeFromBottom (16.0f), juce::Justification::centred);

    r.removeFromBottom (48.0f);   // link buttons
    g.setColour (Palette::dim);
    g.setFont (FamilyTheme::font (12.0f));
    g.drawText (juce::String (Branding::sourceUrl).fromFirstOccurrenceOf ("://", false, false),
                r.removeFromBottom (18.0f), juce::Justification::centred);
    g.setColour (Palette::text);
    g.setFont (FamilyTheme::font (12.0f, true));
    g.drawText (tr (Branding::licence), r.removeFromBottom (18.0f), juce::Justification::centred);
}


bool OptionsOverlay::verifyLayout() const
{
    const auto c=card().toNearestInt();
    return c.contains(settingsTab.getBounds()) && settingsTab.getY()==closeButton.getY()
        && settingsTab.getRight()+8==aboutTab.getX()
        && languageBox.getWidth()==260 && sizeBox.getX()==languageBox.getX()
        && sizeBox.getY()>languageBox.getBottom() && updatesToggle.getY()==checkUpdatesButton.getY()-1
        && sourceButton.getY()==spotifyButton.getY() && sourceButton.getWidth()==126;
}

namespace {
    juce::String plain (juce::String text)
    {
        text = text.replace ("\r\n", "\n").replace ("**", "").replace ("*", "").replace ("`", "");
        juce::StringArray raw, lines;
        raw.addLines (text);
        for (auto& line : raw)
        {
            // Changelog lines are wrapped at ~100 columns: join them back (the text box wraps).
            if (line.startsWith ("  ") && line.trim().isNotEmpty() && ! lines.isEmpty() && lines[lines.size() - 1].isNotEmpty())
                lines.getReference (lines.size() - 1) << " " << line.trim();
            else
                lines.add (line);
        }
        for (auto& line : lines)
        {
            if (line.startsWith ("### ")) line = Localisation::upper (line.substring (4));
            else if (line.startsWith ("## ")) line = line.substring (3);
            else if (line.startsWith ("- ")) line = juce::String::fromUTF8 ("\xe2\x80\xa2 ") + line.substring (2);
        }
        return lines.joinIntoString ("\n").trim();
    }
}

UpdateOverlay::UpdateOverlay()
{
    notes.setMultiLine (true, true);
    notes.setReadOnly (true);
    notes.setScrollbarsShown (true);
    notes.setIndents (10, 8);
    notes.setCaretVisible (false);
    notes.setFont (FamilyTheme::font (14.0f));
    notes.setColour (juce::TextEditor::backgroundColourId, Palette::background);
    notes.setColour (juce::TextEditor::textColourId, Palette::text);
    notes.setColour (juce::TextEditor::outlineColourId, Palette::accent.withAlpha (0.3f));
    addAndMakeVisible (notes);

    installButton.setColour (juce::TextButton::buttonColourId, Palette::accent);
    installButton.setColour (juce::TextButton::textColourOffId, Palette::background);
    for (auto* b : { &installButton, &laterButton, &skipButton })
        addAndMakeVisible (b);

    installButton.onClick = [this]
    {
        if(onInstall)onInstall(result);
    };
    laterButton.onClick = [this] { setVisible (false); };
    skipButton.onClick = [this]
    {
        Updater::skipVersion(result.version);
        setVisible (false);
    };
    refreshText();
}

void UpdateOverlay::show (const Updater::Result& r)
{
    result = r;
    notes.setText (plain(r.notes), false);
    notes.moveCaretToTop (false);
    refreshText();
    setVisible (true);
    toFront (false);
}

void UpdateOverlay::refreshText()
{
    installButton.setButtonText (tr ("INSTALL NOW"));
    laterButton.setButtonText (tr ("LATER"));
    skipButton.setButtonText (tr ("SKIP THIS VERSION"));
    repaint();
}

void UpdateOverlay::resized()
{
    auto inner = card().toNearestInt().reduced (28, 22);
    inner.removeFromTop (92);
    auto buttons = inner.removeFromBottom (34);
    inner.removeFromBottom (44);
    notes.setBounds (inner);

    const int w = 170, gap = 12;
    buttons = buttons.withSizeKeepingCentre (3 * w + 2 * gap, buttons.getHeight());
    for (auto* b : { &installButton, &laterButton, &skipButton })
    {
        b->setBounds (buttons.removeFromLeft (w));
        buttons.removeFromLeft (gap);
    }
}

void UpdateOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));

    auto c = card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (c, 10.0f, 1.0f);

    auto r = c.reduced (28.0f, 22.0f);
    g.setColour (Palette::accent);
    g.setFont (FamilyTheme::font (22.0f, true));
    g.drawText (tr ("New version") + " " + result.version + " " + tr ("downloaded"), r.removeFromTop (30.0f), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (FamilyTheme::font (13.0f));
    g.drawText (tr ("You have") + " " + juce::String (JUCE_APPLICATION_VERSION_STRING) + ". " + tr ("What's new:"),
                r.removeFromTop (26.0f), juce::Justification::centredLeft);

    const char* how = result.kind==Updater::InstallKind::installer
        ? "INSTALL NOW updates the app with the setup and reopens it. Your projects and preferences stay."
        : result.kind==Updater::InstallKind::portable
        ? "INSTALL NOW replaces this copy and reopens it. Save your project first."
        : "INSTALL NOW shows the downloaded package. Replace the app in its current folder.";
    auto bottom = c.reduced (28.0f, 22.0f).removeFromBottom (34.0f + 44.0f).removeFromTop (44.0f);
    g.drawFittedText (tr (how), bottom.toNearestInt(), juce::Justification::centredLeft, 2);
    g.setFont (FamilyTheme::font (11.0f));
    g.drawText (result.file.getFullPathName(), r.removeFromTop (20.0f), juce::Justification::centredLeft);
}

bool UpdateOverlay::verifyLayout()const{
    return card().toNearestInt().contains(notes.getBounds()) && installButton.getWidth()==170
        && installButton.getRight()+12==laterButton.getX() && laterButton.getRight()+12==skipButton.getX()
        && notes.getBottom()<installButton.getY();
}

}
