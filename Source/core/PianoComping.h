// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Adapted unchanged musical logic from Fase Groove Piano PluginProcessor at
// 026f0451d683f25520409f8dd273ff5a4496cad0; JUCE helpers replaced by std equivalents.
#pragma once
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
namespace fga {
struct PianoComping {
struct CompingWindow {double start=0;int figure=-1;};
struct CompingHit {double offset,length;float level;};
struct CompingFigure {int count;std::array<CompingHit,3> hits;};
struct CompingSpan {double on=0,off=0;float level=1;int id=0;};
static constexpr int numCompingFigures=8;
static int compingWindows(std::uint32_t,std::array<CompingWindow,8>&) noexcept;
static const CompingFigure& compingFigure(int) noexcept;
static bool compingKeeps(double,std::uint32_t) noexcept;
static int compingHits(std::uint32_t,std::array<CompingSpan,24>&) noexcept;
static void compingAnswers(double,double,std::uint32_t,const sgk::GrooveSettings&,std::vector<sgk::NoteEvent>&);
static void compingAnswers(double,double,std::uint32_t,const sgk::GrooveSettings&,const std::array<sgk::VoicedChord,sgk::maxProgressionLength>&,std::vector<sgk::NoteEvent>&);
static std::vector<sgk::NoteEvent> comping(const std::vector<sgk::NoteEvent>&,std::uint32_t,const sgk::GrooveSettings&,double);
};
}
