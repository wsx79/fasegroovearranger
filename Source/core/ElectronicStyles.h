// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Project.h"
namespace sgk {struct Chord;}
namespace fga {
sgk::Chord chordAtBeat(const Settings&,double);
void applyElectronicHarmony(Settings&,std::uint32_t seed=0);
void applyElectronicFeel(Settings&);
// Returns false when a deliberately requested legacy instrument should render.
bool generateElectronicClip(const Project&,Clip&,int track,double begin,double beats,Section,bool requested);
}
