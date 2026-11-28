// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Messages made of a fixed text and a value ("Missing sample: " + a path, "... limit (" + 64 + " MiB)."):
// translated where they are shown, part by part, so the worker threads that build them never call tr()
// (its cache belongs to the message thread). Anything else is translated whole, as tr() does.
#include "Localisation.h"
#include <cstring>
#include <utility>
namespace fga {
inline juce::String trComposed(const juce::String& text){
    static const std::pair<const char*,const char*> forms[]{
        {"Missing sample: ",""},{"Sample loaded: ",""},{"Cannot copy sample: ",""},{"Sample copy failed: ",""},{"Sample import failed: ",""},
        {"Sample file exceeds the configured file limit ("," MiB)."},{"Sample exceeds the configured duration limit ("," seconds)."},
        {"Sample exceeds the configured decoded-memory limit ("," MiB)."},
        {"Sample exceeds the total decoded-memory budget ("," MiB); the previous sample remains reserved during replacement."},
        {"Exported, but the mix peaked at "," dB and was limited to -0.01 dBFS."},{"The service returned HTTP ",""}};
    for(const auto& [pre,post]:forms){const int a=(int)std::strlen(pre),b=(int)std::strlen(post);
        if(text.length()>=a+b&&text.startsWith(pre)&&text.endsWith(post))
            return Localisation::tr(pre)+text.substring(a,text.length()-b)+(b?Localisation::tr(post):juce::String());}
    return Localisation::tr(text);
}
}
