// Fase Groove Piano - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Localisation.h"

#include <atomic>
#include <unordered_map>

namespace Localisation
{
namespace
{
    struct Entry { const char* en; const char* it; const char* es; const char* fr; const char* de; const char* pt; };

    // clang-format off
    const Entry table[] {
#include "Translations.inc"
#include "ArrangerTranslations.inc"
    };
    // clang-format on

    std::atomic<int> currentLanguage { 0 };

    const std::unordered_map<std::string, const Entry*>& index()
    {
        static const auto map = []
        {
            std::unordered_map<std::string, const Entry*> m;
            for (const auto& e : table)
                m[juce::String::fromUTF8(e.en).toLowerCase().toStdString()] = &e;
            for (const auto& e : table) m.emplace(juce::String::fromUTF8(e.it).toLowerCase().toStdString(), &e);
            return m;
        }();
        return map;
    }

    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Fase Groove Arranger";
        o.folderName = "Fase Groove Arranger";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (o);
    }
}

const char* languageName (int language) noexcept
{
    static const char* const names[numLanguages] { "English", "Italiano", "Español", "Français", "Deutsch", "Português" };
    return names[juce::jlimit (0, numLanguages - 1, language)];
}

Language current() noexcept
{
    return (Language) currentLanguage.load();
}

namespace
{
    /** JUCE's own texts (the Standalone window's Options menu and audio settings use TRANS). */
    void applyToJuce()
    {
        static const char* const juceTexts[] {
            "Audio/MIDI Settings...", "Audio/MIDI Settings", "Save current state...", "Load a saved state...",
            "Reset to default state", "Save current state", "Load a saved state", "Error whilst saving",
            "Error whilst loading", "Couldn't write to the specified file!", "Couldn't read from the specified file!",
            "Audio device type:", "Device:", "Output:", "Input:", "Sample rate:", "Audio buffer size:",
            "Active MIDI inputs:", "MIDI Output:", "Active output channels:", "Active input channels:", "Test",
            "Plays a test tone", "Control Panel", "Reset Device", "Show advanced settings...",
            "No MIDI inputs available", "none", "OK"
        };

        if (currentLanguage.load() == 0)
        {
            juce::LocalisedStrings::setCurrentMappings (nullptr);
            return;
        }

        juce::String text ("language: Fase Groove Piano\n");
        for (auto* key : juceTexts)
            text << "\"" << juce::String (key).replace ("\"", "\\\"") << "\" = \""
                 << tr (key).replace ("\"", "\\\"") << "\"\n";

        juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (text, false));
    }
}

bool tipsEnabled()
{
    if (auto settings = openSettings())
        return settings->getBoolValue ("tips", true);
    return true;
}

void setTipsEnabled (bool enabled)
{
    if (auto settings = openSettings())
    {
        settings->setValue ("tips", enabled);
        settings->saveIfNeeded();
    }
}

juce::String setting (const char* key, const juce::String& fallback)
{
    if (auto settings = openSettings())
        return settings->getValue (key, fallback);
    return fallback;
}

void setSetting (const char* key, const juce::var& value)
{
    if (auto settings = openSettings())
    {
        settings->setValue (key, value);
        settings->saveIfNeeded();
    }
}

void setCurrent (Language language)
{
    currentLanguage = juce::jlimit (0, numLanguages - 1, (int) language);
    applyToJuce();

    if (auto settings = openSettings())
    {
        settings->setValue ("language", currentLanguage.load());
        settings->saveIfNeeded();
    }
}

void loadSaved()
{
    if (auto settings = openSettings(); settings != nullptr && settings->containsKey ("language"))
    {
        currentLanguage = juce::jlimit (0, numLanguages - 1, settings->getIntValue ("language"));
        applyToJuce();
        return;
    }

    // First run: follow the computer's language.
    const auto system = juce::SystemStats::getUserLanguage().toLowerCase();
    static const char* const codes[numLanguages] { "en", "it", "es", "fr", "de", "pt" };
    for (int i = 0; i < numLanguages; ++i)
        if (system.startsWith (codes[i]))
            currentLanguage = i;
    applyToJuce();
}

juce::String upper (const juce::String& text)
{
    juce::String out;
    for (auto c : text)
    {
        if (c >= 0xe0 && c <= 0xfe && c != 0xf7)
            c -= 0x20;   // Latin-1: à -> À, é -> É, ñ -> Ñ ...
        else
            c = juce::CharacterFunctions::toUpperCase (c);
        out += juce::String::charToString (c);
    }
    return out;
}

juce::String tr (const juce::String& english)
{
    const int lang = currentLanguage.load();

    const auto& map = index();
    const auto it = map.find (english.toLowerCase().toStdString());
    if (it == map.end()){
        auto split=english.lastIndexOfChar(' ');if(split>0){auto prefix=english.substring(0,split),suffix=english.substring(split+1);if(suffix.containsOnly("0123456789ABCD")&&map.find(prefix.toLowerCase().toStdString())!=map.end())return tr(prefix)+" "+suffix;}
        return english;
    }

    const Entry& e = *it->second;
    const char* text = lang == 0 ? e.en : lang == 1 ? e.it : lang == 2 ? e.es : lang == 3 ? e.fr : lang == 4 ? e.de : e.pt;
    auto translated = juce::String::fromUTF8 (text);
    return english == english.toUpperCase() ? translated.toUpperCase() : translated;
}
}
