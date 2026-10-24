// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

// What the shared module (ThirdParty/fasegroove-common) needs to know about this product.
// The arranger compiles only part of it (theme, logo, circle of fifths): its updater and its
// translations are still its own (see fasegroove/MODULO_COMUNE.md, items 13 and 14).

#include "ProductInfo.h"
#include "Branding.h"

#include <BinaryData.h>

namespace
{
    constexpr const char* name = "Fase Groove Arranger";
}

const FaseGroove::ProductInfo& FaseGroove::product()
{
    static const ProductInfo info { name, "fasegroovearranger", "FaseGrooveArranger", "GROOVE ARRANGER",
                                    BinaryData::arranger_svg, BinaryData::arranger_svgSize,
                                    BinaryData::fs_pitch_logo_png, BinaryData::fs_pitch_logo_pngSize };
    return info;
}

namespace Branding
{
    const char* const appName   = name;
    const char* const tagline   = "Piano + Drums + Guitar - one ecosystem";
    const char* const sourceUrl = "https://github.com/wsx79/fasegroovearranger";
}
