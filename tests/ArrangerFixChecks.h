// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Regression checks for user-reported arranger problems (loop, GENERATE SONG,
// instrument-panel changes and the MIDI editor's REGENERATE).
#pragma once
namespace fixchecks {
inline std::vector<std::tuple<int,int,int,int>> trackPrint(const fga::Track& t){
    std::vector<std::tuple<int,int,int,int>> r;
    for(size_t c=0;c<t.clips.size();++c)for(const auto& n:t.clips[c].notes)r.emplace_back((int)c,(int)std::lround(n.start*960),n.pitch,n.lane);
    return r;
}
inline void verifyLoopControls(){
    auto p=fga::newProject(1);
    p.clearLoop();check(!p.loop&&!p.hasLoopRegion(),"clear loop removes the region");
    check(p.toggleLoop()&&p.loop&&p.loopStart==0&&p.loopEnd==p.length(),"loop on without a region loops the song");
    check(!p.toggleLoop()&&!p.loop&&p.hasLoopRegion(),"loop off keeps the region");
    p.setLoopRegion(16,32);check(p.loop&&p.loopStart==16&&p.loopEnd==32,"selection loop set");
    const auto looped=p;
    check(!p.toggleLoopRegion(16,32)&&!p.loop&&!p.hasLoopRegion(),"same selection again removes the loop");
    p.normalise();check(!p.loop&&!p.hasLoopRegion(),"removed loop stays removed after normalise");
    check(p.toggleLoopRegion(32,16)&&p.loop&&p.loopStart==16&&p.loopEnd==32,"selection loop can be set again");
    check(p.loop==looped.loop&&p.loopStart==looped.loopStart&&p.loopEnd==looped.loopEnd,"toggle round trip is undo-consistent");
    check(p.toggleLoopRegion(0,8)&&p.loopStart==0&&p.loopEnd==8,"another selection moves the loop instead of removing it");
    p.clearLoop();p.resizeSection(0,16);p.normalise();check(!p.loop&&!p.hasLoopRegion(),"cleared loop survives section edits");
    p.setLoopRegion(5,5);check(!p.loop&&!p.hasLoopRegion(),"empty selection does not create a loop");
}
inline void verifyGenerateSong(){
    // A genre change followed by GENERATE SONG builds the new genre's form.
    auto p=fga::newProject(1);const auto soulful=p.sections;
    fga::applyGenre(p,6);fga::generate(p,fga::Scope::song,0,0,4242);
    auto garage=fga::newProject(6);
    check(p.sections.size()==garage.sections.size(),"new genre form after GENERATE SONG");
    for(size_t i=0;i<p.sections.size();++i)check(p.sections[i].name==garage.sections[i].name&&p.sections[i].type==garage.sections[i].type,"sections follow the current genre");
    // Timeline edits detach tracks; without locks a new song still varies the form.
    auto q=fga::newProject(1);q.detachTrack(4);q.detachTrack(0);
    std::vector<int> bars;for(const auto& s:q.sections)bars.push_back(s.bars);
    const auto kick=trackPrint(q.tracks[4]),piano=trackPrint(q.tracks[0]);
    fga::generate(q,fga::Scope::song,0,0,777);
    std::vector<int> after;for(const auto& s:q.sections)after.push_back(s.bars);
    check(after!=bars,"detached tracks no longer freeze the form");
    check(trackPrint(q.tracks[4])!=kick&&trackPrint(q.tracks[0])!=piano,"new song writes new parts");
    // Every press draws a new arrangement.
    auto a=q;fga::generate(a,fga::Scope::song,0,0,1001);auto b=a;fga::generate(b,fga::Scope::song,0,0,2002);
    check(trackPrint(a.tracks[4])!=trackPrint(b.tracks[4])||trackPrint(a.tracks[0])!=trackPrint(b.tracks[0]),"consecutive songs differ");
    // Locks keep both the structure and the locked notes.
    auto l=fga::newProject(1);l.tracks[4].clips[1].locked=true;const auto lockedClip=l.tracks[4].clips[1];const auto form=l.sections;
    fga::applyGenre(l,6);fga::generate(l,fga::Scope::song,0,0,31337);
    check(l.sections.size()==form.size()&&l.sections[1].name==form[1].name&&l.tracks[4].clips[1]==lockedClip,"locked clip keeps form and notes");
    // A form the user built is kept.
    auto u=fga::newProject(1);u.sections={{fga::Section::main,8,"Mine"},{fga::Section::drop,8,"Also mine"}};u.normalise();
    fga::applyGenre(u,6);fga::generate(u,fga::Scope::song,0,0,99);
    check(u.sections.size()==2&&u.sections[0].name=="Mine","user form survives a new song");
    (void)soulful;
}
inline void verifyInstrumentChanges(){
    auto p=fga::newProject(1);
    const auto bass=trackPrint(p.tracks[1]),piano=trackPrint(p.tracks[0]);
    bool grooveFollows=false;
    for(int pattern:{0,1,2,7}){auto q=p;q.tracks[0].engine.pattern=pattern;fga::applyInstrumentChange(q,0,false);if(trackPrint(q.tracks[0])!=piano)grooveFollows=true;check(trackPrint(q.tracks[1])==bass,"piano groove leaves bass notes alone");}
    check(grooveFollows,"piano groove change re-renders the piano part");
    auto again=p;fga::applyInstrumentChange(again,0,false);check(trackPrint(again.tracks[0])==piano,"applying unchanged settings keeps the take");
    // Harmony (key/scale/chords) also moves the parts that follow the chords.
    auto h=p;h.settings.key=(h.settings.key+5)%12;h.settings.scale=h.settings.scale==2?0:2;
    const auto strings=trackPrint(p.tracks[2]),guitar=trackPrint(p.tracks[3]);
    fga::applyInstrumentChange(h,0,true);
    check(trackPrint(h.tracks[0])!=piano&&trackPrint(h.tracks[1])!=bass,"harmony change re-renders piano and bass");
    check(strings.empty()||trackPrint(h.tracks[2])!=strings,"harmony change re-renders strings");
    check(guitar.empty()||trackPrint(h.tracks[3])!=guitar,"harmony change re-renders guitar");
    // Locks hold.
    auto k=p;k.tracks[1].locked=true;k.tracks[0].clips[1].locked=true;const auto lockedPiano=k.tracks[0].clips[1];
    k.settings.key=(k.settings.key+3)%12;fga::applyInstrumentChange(k,0,true);
    check(trackPrint(k.tracks[1])==bass&&k.tracks[0].clips[1]==lockedPiano,"locked track and clip ignore panel changes");
}
inline void verifyEditorRegenerate(){
    auto p=fga::newProject(1);
    p.tracks[0].clips[1].notes.clear();const auto other=p.tracks[0].clips[2];const auto motif=p.tracks[0].clips[1].motifSeed;const auto engine=p.tracks[0].engine.pattern;
    check(fga::regenerateClip(p,0,1,12345),"editor regenerate runs");
    check(!p.tracks[0].clips[1].notes.empty()&&p.tracks[0].clips[1].motifSeed==motif&&p.tracks[0].engine.pattern==engine,"regenerate keeps the panel's take and settings");
    check(p.tracks[0].clips[2]==other,"regenerate touches only the edited clip");
    // The new notes are exactly what the current instrument settings render.
    auto q=p;fga::generate(q,fga::Scope::clip,0,1,0);check(q.tracks[0].clips[1].notes==p.tracks[0].clips[1].notes,"regenerated clip follows the instrument settings");
    p.tracks[0].engine.pattern=3;check(fga::regenerateClip(p,0,1,777),"regenerate after a panel change");
    auto r=p;r.tracks[0].clips[1].notes.clear();fga::generate(r,fga::Scope::clip,0,1,0);check(r.tracks[0].clips[1].notes==p.tracks[0].clips[1].notes,"regenerate uses the new groove setting");
    // Drums too.
    auto d=fga::newProject(1);d.tracks[4].clips[1].notes.clear();check(fga::regenerateClip(d,4,1,55)&&!d.tracks[4].clips[1].notes.empty(),"drum clip regenerates");
    // Locks: nothing changes.
    auto l=fga::newProject(1);l.tracks[0].clips[1].locked=true;const auto locked=l.tracks[0].clips[1];
    check(!fga::regenerateClip(l,0,1,9)&&l.tracks[0].clips[1]==locked,"locked clip refuses regenerate");
    l.tracks[1].locked=true;const auto lockedTrack=l.tracks[1].clips[1];check(!fga::regenerateClip(l,1,1,9)&&l.tracks[1].clips[1]==lockedTrack,"locked track refuses regenerate");
    check(!fga::regenerateClip(l,0,99,9),"missing clip refuses regenerate");
}
inline void verifyUserReportedFixes(){verifyLoopControls();verifyGenerateSong();verifyInstrumentChanges();verifyEditorRegenerate();}
}
