// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Project.h"
namespace sgk {struct Chord;}
namespace fga {
sgk::Chord chordAtBeat(const Settings&,double);
void applyElectronicHarmony(Settings&,std::uint32_t seed=0);
void applyElectronicFeel(Settings&);
void addElectronicToneAutomation(Project&);
// Returns false when a deliberately requested legacy instrument should render.
// previewBeats limits work without changing where the complete section ends.
// harmonySection binds a loop source (which may start before beat zero) to the
// harmony of its visible section. The default resolves the section from begin.
bool generateElectronicClip(const Project&,Clip&,int track,double begin,double beats,Section,bool requested,double previewBeats=0,int harmonySection=-1);
// Select against the notes actually heard, ignoring velocity and timing jitter.
std::uint32_t freshElectronicMotif(const Project&,const Clip&,int track,double begin,double beats,Section,std::uint32_t seed);
}
