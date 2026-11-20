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
    check(std::any_of(p.sections.begin(),p.sections.end(),[](const fga::SongSection& s){return s.name=="UK shuffle";}),"new song keeps the current genre's theme across alternate forms");
    check(std::none_of(p.sections.begin(),p.sections.end(),[](const fga::SongSection& s){return s.name=="Soulful verse"||s.name=="Soulful chorus";}),"new genre does not retain the previous genre's sections");
    verifyClubProject(p,6,"");
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
    {   // Articulations touch only the takes a generation wrote, never a locked track or a locked CC1 curve.
        auto a=fga::newProject(0);auto& t=a.tracks[0];t.instrument="plugin";t.articulation=3;
        t.clips[1].notes={{0,.2,60,90,0},{.3,.2,62,90,0}};const auto hand=t.clips[1].notes;
        t.locked=true;fga::applyArticulation(a,0,1);check(a.tracks[0].clips[1].notes==hand,"a locked track keeps its note lengths under Legato");
        a.tracks[0].locked=false;fga::generate(a,fga::Scope::clip,0,0,5);check(a.tracks[0].clips[1].notes==hand,"regenerating one clip leaves the articulation of the others alone");
        a.tracks[0].articulation=1;auto& lane=a.tracks[0].clips[1].expression[1];lane.enabled=true;lane.locked=true;lane.points={{0,.3,0}};const auto curve=lane;
        fga::applyArticulation(a,0,1);check(a.tracks[0].clips[1].expression[1]==curve,"Dynamics keeps a locked CC1 curve");
    }
    {   // Latin House: guitar and lead answer the montuno only when the piano is heard.
        auto lp=fga::newProject(fga::latinHouseGenre);lp.tracks[0].mute=true;lp.settings.randomAutomation=false;fga::generate(lp);std::set<long> piano;int together=0;
        for(size_t ci=0;ci<lp.tracks[0].clips.size();++ci){const double at=lp.clipStart(0,(int)ci);for(const auto& n:lp.tracks[0].clips[ci].notes)piano.insert(std::lround((at+n.start)*4));}
        for(int ti:{3,fga::leadTrack})for(size_t ci=0;ci<lp.tracks[(size_t)ti].clips.size();++ci){const double at=lp.clipStart(ti,(int)ci);for(const auto& n:lp.tracks[(size_t)ti].clips[ci].notes)together+=piano.count(std::lround((at+n.start)*4))>0;}
        check(together>0,"a muted piano leaves the Latin guitar and lead whole");
    }
    {
        auto a=fga::newProject(0);auto& t=a.tracks[1];t.instrument="plugin";t.articulation=1;
        auto& c=t.clips[1];c.notes={{0,1./32.,24,90,0},{1,1./32.,25,90,0},{2,.2,60,90,0}};
        c.expression[1].enabled=true;c.expression[1].locked=true;c.expression[1].points={{0,.4,0}};const auto curve=c.expression[1];
        check(fga::setTrackArticulation(a,1,3)&&c.expression[1]==curve,"profile menu change preserves a locked CC1 lane");
        check(c.notes.size()==3&&c.notes[0].pitch==24&&c.notes[1].pitch==25,"Dynamics and Legato never identify musical C0/C#0 as keyswitches");
        t.locked=true;const auto before=c.notes;check(!fga::setTrackArticulation(a,1,0)&&c.notes==before&&c.expression[1]==curve,"locked track rejects profile changes");
        t.locked=false;check(fga::setTrackArticulation(a,1,2),"enable owned keyswitches");
        check(std::any_of(c.notes.begin(),c.notes.end(),[](const auto& n){return n.articulationKey;}),"keyswitches carry ownership");
        check(fga::setTrackArticulation(a,1,0)&&std::none_of(c.notes.begin(),c.notes.end(),[](const auto& n){return n.articulationKey;}),"Off removes only owned keyswitches");
        check(std::any_of(c.notes.begin(),c.notes.end(),[](const auto& n){return n.pitch==24&&!n.articulationKey;}),"Off preserves the musician's low note");
    }
    {
        bool responds=false;
        for(unsigned seed=1;seed<=8&&!responds;++seed){
            auto on=fga::newProject(fga::latinHouseGenre);on.settings.randomAutomation=false;on.settings.humanize=0;on.settings.swing=0;
            for(auto& t:on.tracks){t.engine.humanize=0;t.engine.swing=0;for(auto& c:t.clips)c.seed=seed;}
            auto off=on;off.tracks[0].mute=true;fga::generate(on);fga::generate(off);
            size_t heard=0,silent=0;for(const auto& c:on.tracks[3].clips)heard+=c.notes.size();for(const auto& c:off.tracks[3].clips)silent+=c.notes.size();
            responds=heard<silent;
        }
        check(responds,"audible Latin montuno thins weak guitar attacks; mute disables the interaction");
    }
    {   // VOCALS PRESENT: one project setting, read by every generation; OFF is the old behaviour.
        auto off=fga::newProject(1);off.settings.randomAutomation=false;check(!off.settings.vocalsPresent,"VOCALS PRESENT is off by default");
        auto on=off;on.settings.vocalsPresent=true;fga::generate(off,fga::Scope::song,0,0,4242);fga::generate(on,fga::Scope::song,0,0,4242);
        auto again=fga::newProject(1);again.settings.randomAutomation=false;fga::generate(again,fga::Scope::song,0,0,4242);
        check(again.tracks[fga::leadTrack].clips==off.tracks[fga::leadTrack].clips&&again.tracks[0].clips==off.tracks[0].clips,"VOCALS PRESENT off leaves generation as it was (deterministic)");
        check(on.settings.vocalsPresent,"VOCALS PRESENT survives a song generation");
        for(int ti:{1,4,5})check(on.tracks[(size_t)ti].clips==off.tracks[(size_t)ti].clips,"VOCALS PRESENT never touches kick, bass or the backbeat");
        size_t leadOn=0,leadOff=0;bool verseRoom=true,introSame=true;
        for(size_t ci=0;ci<on.tracks[fga::leadTrack].clips.size();++ci){leadOn+=on.tracks[fga::leadTrack].clips[ci].notes.size();leadOff+=off.tracks[fga::leadTrack].clips[ci].notes.size();
            const int si=on.sectionAtBeat(on.clipStart(fga::leadTrack,(int)ci)+.01);if(si<0)continue;const auto type=on.sections[(size_t)si].type;const double from=on.sectionStart(si),at=on.clipStart(fga::leadTrack,(int)ci);
            if(type==fga::Section::intro||type==fga::Section::outro)introSame=introSame&&on.tracks[fga::leadTrack].clips[ci]==off.tracks[fga::leadTrack].clips[ci];
            if(type==fga::Section::main)for(const auto& n:on.tracks[fga::leadTrack].clips[ci].notes)verseRoom=verseRoom&&((int)std::floor((at+n.start-from)/4+1e-9))%4>=2;}
        check(leadOn<leadOff,"VOCALS PRESENT thins the lead");check(verseRoom,"in a verse the lead answers only in the second half of each phrase");check(introSame,"intros and outros stay instrumental and unchanged");
        for(int ti:{0,2,3})for(size_t ci=0;ci<on.tracks[(size_t)ti].clips.size();++ci){const int si=on.sectionAtBeat(on.clipStart(ti,(int)ci)+.01);if(si<0||(on.sections[(size_t)si].type!=fga::Section::main&&on.sections[(size_t)si].type!=fga::Section::drop))continue;
            for(const auto& n:on.tracks[(size_t)ti].clips[ci].notes)check(n.pitch<84||on.tracks[(size_t)ti].clips[ci].locked,"keys, strings and guitar stay below the voice's presence range");}
        auto regen=on;fga::generate(regen,fga::Scope::clip,fga::leadTrack,2,0);check(regen.tracks[fga::leadTrack].clips[2]==on.tracks[fga::leadTrack].clips[2],"a clip regeneration matches the song pass with VOCALS PRESENT");
        auto genre=on;fga::applyGenre(genre,7);check(genre.settings.vocalsPresent,"a genre change keeps VOCALS PRESENT");
    }
    {   // A section that returns varies: the lead an octave higher on odd returns, a layer entering late.
        auto r=fga::newProject(0);r.settings.randomAutomation=false;r.sections={{fga::Section::intro,8,"I"},{fga::Section::main,16,"A"},{fga::Section::drop,16,"D"},{fga::Section::main,16,"A2"},{fga::Section::drop,16,"D2"},{fga::Section::outro,8,"O"}};r.normalise();fga::generate(r,fga::Scope::song,0,0,77);
        auto top=[&](int ci){int t=0;for(const auto& n:r.tracks[fga::leadTrack].clips[(size_t)ci].notes)t=std::max(t,n.pitch);return t;};
        auto pcs=[&](int ti,int ci){std::set<int> s;for(const auto& n:r.tracks[(size_t)ti].clips[(size_t)ci].notes)s.insert(n.pitch%12);return s;};
        check(top(4)>=top(2)+7||top(2)+12>88,"the second drop lifts the lead");
        bool guitarLate=true;for(const auto& n:r.tracks[3].clips[3].notes)guitarLate=guitarLate&&n.start>=32-1./32.;check(guitarLate,"the returning verse brings the guitar in after eight bars");
        check(!r.tracks[0].clips[3].notes.empty()&&r.tracks[0].clips[3].notes!=r.tracks[0].clips[1].notes,"the returning verse's piano is not a copy");
        for(int ti:{0,fga::leadTrack}){const auto a=pcs(ti,2),b=pcs(ti,4);int common=0;for(int x:b)common+=a.count(x);check(common*10>=(int)b.size()*6,"a return keeps the harmony: its pitch classes are mostly the first one's");}
    }
    // Drums too.
    auto d=fga::newProject(1);d.tracks[4].clips[2].notes.clear();check(fga::regenerateClip(d,4,2,55)&&!d.tracks[4].clips[2].notes.empty(),"drum clip regenerates");
    // Locks: nothing changes.
    auto l=fga::newProject(1);l.tracks[0].clips[1].locked=true;const auto locked=l.tracks[0].clips[1];
    check(!fga::regenerateClip(l,0,1,9)&&l.tracks[0].clips[1]==locked,"locked clip refuses regenerate");
    l.tracks[1].locked=true;const auto lockedTrack=l.tracks[1].clips[1];check(!fga::regenerateClip(l,1,1,9)&&l.tracks[1].clips[1]==lockedTrack,"locked track refuses regenerate");
    check(!fga::regenerateClip(l,0,99,9),"missing clip refuses regenerate");
}
inline void verifyUserReportedFixes(){verifyLoopControls();verifyGenerateSong();verifyInstrumentChanges();verifyEditorRegenerate();}
}
