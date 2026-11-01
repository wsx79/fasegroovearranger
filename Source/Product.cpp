// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

// What the shared module (ThirdParty/fasegroove-common) needs to know about this product.
// The arranger compiles part of it (theme, logo, circle of fifths, updater, translations).

#include "ProductInfo.h"
#include "ProductSettings.h"
#include "Branding.h"
#include "Localisation.h"

#include <BinaryData.h>

namespace
{
    constexpr const char* name = "Fase Groove Arranger";
    bool selfTestPreferences=false;

    // The family's texts (after the shared CommonTranslations.inc), then the arranger's own.
    // clang-format off
    const Localisation::Entry ownTexts[] {
#include "Translations.inc"
#include "ArrangerTranslations.inc"
    };
    // clang-format on
}

const FaseGroove::ProductInfo& FaseGroove::product()
{
    static const ProductInfo info { name, "fasegroovearranger", "FaseGrooveArranger", "GROOVE ARRANGER",
                                    BinaryData::arranger_svg, BinaryData::arranger_svgSize,
                                    BinaryData::fs_pitch_logo_png, BinaryData::fs_pitch_logo_pngSize,
                                    true,                               // only the app updates itself
                                    "FaseGrooveArranger.install.ini",   // the setup, in any folder
                                    true,                               // reopened once it has quit
                                    true };                             // translations found in any case (see ProductInfo.h)
    if(selfTestPreferences){static const auto testInfo=[] {auto copy=info;copy.productName="Fase Groove Arranger Self-test";return copy;}();return testInfo;}
    return info;
}

void FaseGroove::useSelfTestPreferences(){
    selfTestPreferences=true;
    // Refresh the shared settings cache after selecting the separate test file.
    Localisation::setSetting("selfTest",true);
}

namespace Branding
{
    const char* const appName   = name;
    const char* const tagline   = "Piano + Drums + Guitar + Lead - one ecosystem";
    const char* const sourceUrl = "https://github.com/wsx79/fasegroovearranger";
}

Localisation::Table Localisation::productTranslations()
{
    return { ownTexts, sizeof (ownTexts) / sizeof (ownTexts[0]) };
}
