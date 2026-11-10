// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <set>
#include <map>
#include <tuple>
namespace fga {
std::string newVocalClipId(){
    std::random_device random;
    std::ostringstream text;
    for(int i=0;i<4;++i)text<<std::hex<<std::setfill('0')<<std::setw(8)<<(std::uint32_t)random();
    return text.str();
}
void enableVocalClip(Clip& clip,double duration){
    if(clip.vocal)return;
    clip.vocal.emplace();clip.vocal->id=newVocalClipId();clip.vocal->regionEnd=duration;
    reconcileVocalNotes(clip);
}
void reconcileVocalNotes(Clip& clip){
    if(!clip.vocal)return;
    auto& vocal=*clip.vocal;
    std::set<int> used;
    for(const auto& note:clip.notes)if(note.vocalId>0)vocal.nextNoteId=std::max(vocal.nextNoteId,note.vocalId+1);
    for(auto& note:clip.notes){
        if(note.vocalId<=0||!used.insert(note.vocalId).second){note.vocalId=vocal.nextNoteId++;used.insert(note.vocalId);}
    }
    // A deleted note cannot silently attach its lyric to a new note at the same index.
    std::set<int> assigned;
    for(auto& syllable:vocal.syllables){
        syllable.noteIds.erase(std::remove_if(syllable.noteIds.begin(),syllable.noteIds.end(),[&](int id){return !used.count(id)||!assigned.insert(id).second;}),syllable.noteIds.end());
    }
}
void retainVocalNoteIds(Clip& clip,const std::vector<Note>& previous){
    if(!clip.vocal)return;
    std::map<std::tuple<double,int,int>,int> ids;
    for(const auto& n:previous)ids[{n.start,n.pitch,n.lane}]=n.vocalId;
    for(auto& n:clip.notes){auto found=ids.find({n.start,n.pitch,n.lane});n.vocalId=found==ids.end()?0:found->second;}
    reconcileVocalNotes(clip);
}
bool assignVocalSyllable(Clip& clip,int syllableId,const std::vector<int>& noteIds){
    if(!clip.vocal)return false;
    auto& syllables=clip.vocal->syllables;
    auto target=std::find_if(syllables.begin(),syllables.end(),[&](const auto& s){return s.id==syllableId;});
    if(target==syllables.end())return false;
    std::set<int> ids;
    for(int id:noteIds)if(!ids.insert(id).second||std::none_of(clip.notes.begin(),clip.notes.end(),[&](const Note& n){return n.vocalId==id;}))return false;
    for(auto& syllable:syllables)syllable.noteIds.erase(std::remove_if(syllable.noteIds.begin(),syllable.noteIds.end(),[&](int id){return ids.count(id)>0;}),syllable.noteIds.end());
    target->noteIds=noteIds;return true;
}
void normaliseVocalClips(Project& project){
    std::set<std::string> ids;
    for(auto& track:project.tracks)for(auto& clip:track.clips){
        if(!clip.vocal)continue;
        if(track.vocalChops||!isManualTrack(track.kind)||clip.audioOffset>=0){clip.vocal.reset();for(auto& n:clip.notes)n.vocalId=0;continue;}
        auto& vocal=*clip.vocal;
        if(vocal.id.empty()||!ids.insert(vocal.id).second){vocal.id=newVocalClipId();ids.insert(vocal.id);}
        const double duration=project.clipLength((int)(&track-project.tracks.data()),(int)(&clip-track.clips.data()));
        vocal.regionStart=std::clamp(std::isfinite(vocal.regionStart)?vocal.regionStart:0.,0.,duration);
        vocal.regionEnd=std::clamp(std::isfinite(vocal.regionEnd)?vocal.regionEnd:duration,vocal.regionStart,duration);
        reconcileVocalNotes(clip);
    }
}
bool vocalBeatAllowed(const Project& project,int track,int index,double beat){
    if(track<0||track>=(int)project.tracks.size()||index<0||index>=(int)project.tracks[(size_t)track].clips.size()||!std::isfinite(beat))return false;
    const auto& c=project.tracks[(size_t)track].clips[(size_t)index];if(!c.vocal||beat<c.vocal->regionStart||beat>=c.vocal->regionEnd)return false;
    const double absolute=project.clipStart(track,index)+beat;
    if(absolute<0)return false;
    if(absolute>=project.sectionStart((int)project.sections.size()))return c.vocal->allowIntroOutro;
    const auto type=project.sections[(size_t)project.sectionAtBeat(absolute)].type;
    return c.vocal->allowIntroOutro||(type!=Section::intro&&type!=Section::outro);
}
}
