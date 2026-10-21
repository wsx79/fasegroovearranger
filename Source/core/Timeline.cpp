// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include <cmath>
namespace fga {
namespace {
constexpr double tick=1./960.;
Clip slice(const Clip& source,double offset,double duration,double position){
    Clip part=source;part.notes.clear();part.position=position;part.span=duration;
    for(auto n:source.notes){const double from=std::max(offset,n.start),to=std::min(offset+duration,n.start+n.length);if(to-from<tick)continue;n.start=from-offset;n.length=to-from;part.notes.push_back(n);}
    tidyNotes(part,duration);part.loopLength=duration;part.loopSource=part.notes;return part;
}
void sortClips(Track& t){std::stable_sort(t.clips.begin(),t.clips.end(),[](const Clip& a,const Clip& b){return a.position<b.position;});}
}
double Project::clipStart(int track,int clip)const{
    if(track<0||track>=(int)tracks.size()||clip<0||clip>=(int)tracks[(size_t)track].clips.size())return 0;
    return tracks[(size_t)track].freeTimeline?tracks[(size_t)track].clips[(size_t)clip].position:sectionStart(clip);
}
int Project::sectionAtBeat(double beat)const{int index=0;for(size_t i=1;i<sections.size();++i)if(beat>=sectionStart((int)i))index=(int)i;return index;}
int Project::clipAtBeat(int track,double beat)const{
    if(track<0||track>=(int)tracks.size())return -1;
    double occupied=0;const auto& clips=tracks[(size_t)track].clips;
    for(size_t i=0;i<clips.size();++i){const double start=clipStart(track,(int)i),end=start+clipLength(track,(int)i),from=std::max(start,occupied);occupied=std::max(occupied,end);if(beat>=from&&beat<end)return (int)i;}return -1;
}
int Project::detachTrack(int track,int selected){
    if(track<0||track>=(int)tracks.size())return -1;auto& t=tracks[(size_t)track];if(t.freeTimeline)return selected;
    std::vector<Clip> clips;double occupied=0;int result=-1;
    for(size_t i=0;i<t.clips.size();++i){const double start=clipStart(track,(int)i),duration=clipLength(track,(int)i),from=std::max(start,occupied);occupied=std::max(occupied,start+duration);if(from>=start+duration)continue;
        auto clip=t.clips[i];clip.role=(int)sections[i].type;if(from>start)clip=slice(clip,from-start,start+duration-from,from);else{clip.position=start;clip.span=duration;}
        if((int)i==selected)result=(int)clips.size();clips.push_back(std::move(clip));}
    t.clips=std::move(clips);t.freeTimeline=true;return result;
}
int Project::splitClip(int track,int index,double beat){
    if(track<0||track>=(int)tracks.size()||index<0||index>=(int)tracks[(size_t)track].clips.size()||!std::isfinite(beat))return -1;
    const double start=clipStart(track,index),end=start+clipLength(track,index);beat=std::round(beat*960)/960.;
    if(beat-start<tick||end-beat<tick)return -1;
    index=detachTrack(track,index);if(index<0)return -1;auto& t=tracks[(size_t)track];const auto original=t.clips[(size_t)index];
    const auto left=slice(original,0,beat-original.position,original.position),right=slice(original,beat-original.position,original.position+original.span-beat,beat);
    if(t.clips.size()>=4096)return -1;t.clips[(size_t)index]=left;t.clips.insert(t.clips.begin()+index+1,right);return index+1;
}
int Project::placeClip(int track,Clip clip,double beat){
    if(track<0||track>=(int)tracks.size()||!std::isfinite(beat)||!std::isfinite(clip.span)||clip.span<tick||clip.span>512)return -1;
    beat=std::clamp(std::round(beat*960)/960.,0.,131072.);detachTrack(track);auto& t=tracks[(size_t)track];const double end=beat+clip.span;
    std::vector<Clip> keep;size_t notes=clip.notes.size()+clip.loopSource.size();
    for(const auto& old:t.clips){const double tail=old.position+old.span;if(tail<=beat||old.position>=end)keep.push_back(old);else{
        if(beat-old.position>=tick)keep.push_back(slice(old,0,beat-old.position,old.position));
        if(tail-end>=tick)keep.push_back(slice(old,end-old.position,tail-end,end));}}
    if(keep.size()>=4096)return -1;
    for(const auto& c:keep)notes+=c.notes.size()+c.loopSource.size();
    for(size_t i=0;i<tracks.size();++i)if((int)i!=track)for(const auto& c:tracks[i].clips)notes+=c.notes.size()+c.loopSource.size();
    if(notes>2000000)return -1;
    clip.position=beat;tidyNotes(clip,clip.span);keep.push_back(std::move(clip));t.clips=std::move(keep);sortClips(t);normalise();return clipAtBeat(track,beat);
}
int Project::moveClip(int track,int index,int targetTrack,double beat,bool copy){
    if(track<0||track>=(int)tracks.size()||targetTrack<0||targetTrack>=(int)tracks.size()||index<0||index>=(int)tracks[(size_t)track].clips.size()||!std::isfinite(beat))return -1;
    Project edited=*this;index=edited.detachTrack(track,index);if(index<0)return -1;auto& source=edited.tracks[(size_t)track];auto clip=source.clips[(size_t)index];if(!copy)source.clips.erase(source.clips.begin()+index);
    const int placed=edited.placeClip(targetTrack,std::move(clip),beat);if(placed<0)return -1;*this=std::move(edited);return placed;
}
bool Project::deleteClip(int track,int index){
    if(track<0||track>=(int)tracks.size()||index<0||index>=(int)tracks[(size_t)track].clips.size())return false;
    index=detachTrack(track,index);if(index<0)return false;auto& clips=tracks[(size_t)track].clips;clips.erase(clips.begin()+index);return true;
}
void Project::resizeGuide(int index,int bars){if(index<0||index>=(int)sections.size())return;for(int t=0;t<(int)tracks.size();++t)detachTrack(t);sections[(size_t)index].bars=std::clamp(bars,1,128);normalise();}
void Project::insertGuide(int index,SongSection guide){if(sections.size()>=64)return;for(int t=0;t<(int)tracks.size();++t)detachTrack(t);index=std::clamp(index,0,(int)sections.size());sections.insert(sections.begin()+index,std::move(guide));normalise();}
void Project::removeGuide(int index){if(sections.size()<2||index<0||index>=(int)sections.size())return;for(int t=0;t<(int)tracks.size();++t)detachTrack(t);sections.erase(sections.begin()+index);normalise();}
void Project::moveGuide(int from,int to){if(from<0||to<0||from>=(int)sections.size()||to>=(int)sections.size()||from==to)return;for(int t=0;t<(int)tracks.size();++t)detachTrack(t);auto guide=sections[(size_t)from];sections.erase(sections.begin()+from);sections.insert(sections.begin()+to,guide);normalise();}
}
