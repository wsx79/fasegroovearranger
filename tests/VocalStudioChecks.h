// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
inline void verifyVocalStudioCore(){
    using namespace fga;
    Project p;p.sections={{Section::intro,16,"Intro"},{Section::main,8,"Main"},{Section::outro,16,"Outro"}};p.normalise();
    const int t=p.addMidiTrack();auto& c=p.tracks[(size_t)t].clips[0];c.position=0;c.span=p.length();
    c.notes={{64,1,60,100,0},{66,1,62,90,0},{68,2,64,95,0}};
    const auto midiBefore=midiFile(p);enableVocalClip(c,c.span);const auto clipId=c.vocal->id;
    const int first=c.notes[0].vocalId,second=c.notes[1].vocalId;
    c.vocal->syllables={{1,"hel","",{first,second}},{2,"lo","",{c.notes[2].vocalId}}};c.vocal->nextSyllableId=3;
    check(midiFile(p)==midiBefore,"vocal metadata does not alter existing MIDI output");
    check(!vocalBeatAllowed(p,t,0,0)&&vocalBeatAllowed(p,t,0,64)&&!vocalBeatAllowed(p,t,0,96),"vocal intro/outro excluded across an independent clip");
    c.vocal->allowIntroOutro=true;check(vocalBeatAllowed(p,t,0,0)&&vocalBeatAllowed(p,t,0,96),"explicit intro/outro override");
    c.vocal->regionStart=64;c.vocal->regionEnd=70;check(!vocalBeatAllowed(p,t,0,63)&&!vocalBeatAllowed(p,t,0,70),"vocal region is half-open");
    c.notes[0].pitch=65;c.notes[0].start=67;tidyNotes(c,c.span);
    check(c.vocal->syllables[0].noteIds==std::vector<int>({first,second}),"pitch/onset sorting preserves lyric IDs");
    c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.vocalId==first;}),c.notes.end());tidyNotes(c,c.span);
    check(c.vocal->syllables[0].noteIds==std::vector<int>({second}),"deleted note links pruned");
    c.notes.push_back({71,1,67,100,0});tidyNotes(c,c.span);check(c.notes.back().vocalId>first,"new note never reuses a deleted ID");
    check(!assignVocalSyllable(c,1,{123456}),"invalid assignment is rejected atomically");
    check(assignVocalSyllable(c,2,{second})&&c.vocal->syllables[0].noteIds.empty(),"one note cannot belong to two syllables");
    check(assignVocalSyllable(c,2,{})&&c.vocal->syllables[1].noteIds.empty(),"rest assignment");
    c.notes.push_back(c.notes[0]);c.notes.back().start=73;tidyNotes(c,c.span);
    std::set<int> ids;for(const auto& n:c.notes)check(ids.insert(n.vocalId).second,"copied notes receive distinct IDs");
    p.normalise();check(p.tracks[(size_t)t].clips[0].vocal->id==clipId,"clip ID survives normalization");
    // Exercise the same timeline operations used by the UI; musical data is not regenerated.
    auto& clip=p.tracks[(size_t)t].clips[0];clip.vocal->regionStart=64;clip.vocal->regionEnd=80;
    const int split=p.splitClip(t,0,68);check(split==1,"split vocal clip on existing timeline");
    const auto& left=p.tracks[(size_t)t].clips[0];const auto& right=p.tracks[(size_t)t].clips[1];
    check(left.vocal->id==clipId&&right.vocal->id!=clipId&&right.vocal->regionStart==0&&right.vocal->regionEnd==12,"split clips retain independent identities and shift vocal region");
    const auto rightId=right.vocal->id;const int copied=p.moveClip(t,1,t,180,true);
    check(copied>=0&&p.tracks[(size_t)t].clips[(size_t)copied].vocal->id!=rightId,"copied clip has a fresh ID");
    const auto source=p.tracks[(size_t)t].clips[(size_t)copied].vocal->id;
    const int moved=p.moveClip(t,copied,t,300,false);check(moved>=0&&p.tracks[(size_t)t].clips[(size_t)moved].vocal->id==source,"moved clip retains identity");
    p.foldFreeTracks();check(p.tracks[(size_t)t].freeTimeline,"folding cannot discard vocal metadata");
    Clip loop;loop.notes={{0,1,60,100,0}};loop.loopLength=4;loop.loopSource=loop.notes;loop.span=4;enableVocalClip(loop,4);
    const int loopId=loop.notes[0].vocalId;loop.vocal->syllables={{1,"hold","",{loopId}}};loop.vocal->nextSyllableId=2;
    check(rebuildClipLoop(loop,12)&&loop.notes[0].vocalId==loopId&&loop.vocal->syllables[0].noteIds==std::vector<int>{loopId},"loop expansion retains existing assignments, new repeats unassigned");
    const int chops=p.addVocalChopsTrack();auto& chop=p.tracks[(size_t)chops].clips[0];enableVocalClip(chop,64);p.normalise();
    check(!p.tracks[(size_t)chops].clips[0].vocal&&p.tracks[(size_t)chops].vocalChops,"Vocal Studio cannot merge with Vocal Chops");
}
