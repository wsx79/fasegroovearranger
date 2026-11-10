// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <string>
#include <vector>
namespace fga {
// IDs are scoped to the vocal clip; an empty assignment is a deliberate rest.
struct VocalSyllable {
    int id=0;
    std::string text,phonemes;
    std::vector<int> noteIds;
    bool operator==(const VocalSyllable& o)const{return id==o.id&&text==o.text&&phonemes==o.phonemes&&noteIds==o.noteIds;}
};
struct VocalClip {
    std::string id,lyrics,voice,modelPath; // voice is empty or qixuan-candidate; never an activation
    int nextNoteId=1,nextSyllableId=1;
    double regionStart=0,regionEnd=0; // clip-relative beats, half-open
    bool allowIntroOutro=false;
    std::vector<VocalSyllable> syllables;
    bool operator==(const VocalClip& o)const{return id==o.id&&lyrics==o.lyrics&&voice==o.voice&&modelPath==o.modelPath&&nextNoteId==o.nextNoteId&&nextSyllableId==o.nextSyllableId&&regionStart==o.regionStart&&regionEnd==o.regionEnd&&allowIntroOutro==o.allowIntroOutro&&syllables==o.syllables;}
};
struct Clip;
struct Project;
std::string newVocalClipId();
void enableVocalClip(Clip&,double duration);
void reconcileVocalNotes(Clip&);
void retainVocalNoteIds(Clip&,const std::vector<struct Note>&);
void normaliseVocalClips(Project&);
bool assignVocalSyllable(Clip&,int syllableId,const std::vector<int>& noteIds);
bool vocalBeatAllowed(const Project&,int track,int clip,double relativeBeat);
}
