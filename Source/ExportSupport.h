// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/Theory.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
namespace fga {
inline juce::String exportTempo(const Project& project){
    auto value=juce::String(project.settings.bpm,3);while(value.endsWithChar('0'))value=value.dropLastCharacters(1);if(value.endsWithChar('.'))value=value.dropLastCharacters(1);return value;
}
inline juce::String exportKey(const Project& project){return juce::String::fromUTF8(sgk::spelledNote(project.settings.key,project.settings.key,project.settings.scale));}
inline juce::String exportSuffix(const Project& project){return "_"+exportTempo(project)+"bpm_"+juce::File::createLegalFileName(exportKey(project));}
inline juce::File exportNamedFile(const juce::File& chosen,const Project& project,const juce::String& extension){
    auto name=chosen.getFileNameWithoutExtension();const auto suffix=exportSuffix(project);if(!name.endsWithIgnoreCase(suffix))name+=suffix;return chosen.getSiblingFile(name+"."+extension);
}
inline juce::File exportStemFile(const juce::File& folder,const Project& project,int track,const juce::String& extension){
    return folder.getChildFile(juce::String(rowForTrack(track)+1).paddedLeft('0',2)+"-"+juce::File::createLegalFileName(juce::String::fromUTF8(project.tracks[(size_t)track].name.c_str()))+exportSuffix(project)+"."+extension);
}
inline juce::String exportTime(double seconds){
    const auto milliseconds=(juce::int64)std::llround(std::max(0.,seconds)*1000.);const auto hours=milliseconds/3600000;
    return (hours>0?juce::String(hours).paddedLeft('0',2)+":":juce::String())+juce::String(milliseconds/60000%60).paddedLeft('0',2)+":"+juce::String(milliseconds/1000%60).paddedLeft('0',2)+"."+juce::String(milliseconds%1000).paddedLeft('0',3);
}
inline juce::String exportArrangementText(const Project& project){
    juce::String result="Fase Groove Arranger\n"+juce::String::fromUTF8(project.name.c_str())+"\nBPM: "+exportTempo(project)+"\nKey / Tonalita: "+exportKey(project)+"\n\nArrangement / Arrangiamento (mm:ss.mmm)\n";
    const double secondsPerBeat=60./project.settings.bpm;
    for(int i=0;i<(int)project.sections.size();++i)result+=exportTime(project.sectionStart(i)*secondsPerBeat)+" - "+exportTime(project.sectionStart(i+1)*secondsPerBeat)+"  "+juce::String::fromUTF8(project.sections[(size_t)i].name.c_str())+"\n";
    result+="\nEnd / Fine: "+exportTime(project.length()*secondsPerBeat)+"\nAudio tails may extend beyond the arrangement. / Le code audio possono proseguire oltre la fine.\n";return result;
}
inline bool writeExportArrangement(const juce::File& folder,const Project& project){
    juce::TemporaryFile temp(folder.getChildFile("Arrangiamento"+exportSuffix(project)+".txt"));return temp.getFile().replaceWithText(exportArrangementText(project))&&temp.overwriteTargetFileWithTemporary();
}
}
