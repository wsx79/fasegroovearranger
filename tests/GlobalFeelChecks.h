// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace globalfeelchecks {
inline fga::Project fixture(int genre){
    fga::Project p;p.normalise();fga::applyGenre(p,genre);
    p.sections={{fga::Section::main,4,"Main"},{fga::Section::drop,4,"Drop"}};
    for(auto& t:p.tracks){t.clips.assign(2,{});t.parameters.clear();}
    p.normalise();fga::generate(p);return p;
}
inline size_t count(const fga::Project& p){size_t n=0;for(const auto& t:p.tracks)for(const auto& c:t.clips)n+=c.notes.size();return n;}
inline std::vector<fga::Note> notes(const fga::Project& p,int track){std::vector<fga::Note> n;for(const auto& c:p.tracks[(size_t)track].clips)n.insert(n.end(),c.notes.begin(),c.notes.end());return n;}
inline void verify(){
    using namespace fga;
    for(int genre=0;genre<numGenres;++genre){
        const auto base=fixture(genre);
        auto low=base,high=base;low.settings.density=.05;high.settings.density=.95;
        applyGlobalFeel(low,densityFeel);applyGlobalFeel(high,densityFeel);
        check(count(high)>count(low),"header Density must increase automatic note activity across every genre");
        check(high.settings.genre==base.settings.genre&&high.settings.degrees==base.settings.degrees&&high.sections.size()==base.sections.size(),"global feel keeps genre, harmony and song form");
        for(int ti:{0,1,3,leadTrack})check(high.tracks[(size_t)ti].engine.density<0,"global Density takes precedence over preset track density");
        low=base;high=base;low.settings.humanize=0;high.settings.humanize=1;
        applyGlobalFeel(low,humanizeFeel);applyGlobalFeel(high,humanizeFeel);
        for(int ti:{0,1,3,leadTrack})if(!notes(low,ti).empty())check(notes(low,ti)!=notes(high,ti),"header Feel affects each active harmonic family, not just any track");
        low=base;high=base;low.settings.swing=0;high.settings.swing=1.2;
        applyGlobalFeel(low,swingFeel);applyGlobalFeel(high,swingFeel);
        bool shifted=false;for(int ti=0;ti<trackCount;++ti)shifted=shifted||notes(low,ti)!=notes(high,ti);
        check(shifted,"header Swing changes offbeats in every genre");
        for(auto scope:{Scope::song,Scope::section,Scope::track,Scope::clip}){
            auto p=base;const auto before=p.settings;generate(p,scope,0,0,2381U);
            check(std::lround(p.settings.density*100)!=std::lround(before.density*100)&&std::lround(p.settings.swing*100)!=std::lround(before.swing*100)&&std::lround(p.settings.humanize*100)!=std::lround(before.humanize*100),"each fresh regeneration chooses new visible Density, Swing and Feel values");
            check(p.settings.density>=.12&&p.settings.density<=.9&&p.settings.humanize>=.08&&p.settings.humanize<=.48,"regenerated feel stays in musical bounds");
            if(genre==technoGenre)check(p.settings.swing<=.08,"Techno new takes stay tight");
            if(genre==electroHouseGenre)check(p.settings.swing>=.04&&p.settings.swing<=.20,"Electro new takes use controlled shuffle");
            const auto varied=p.settings;applyInstrumentChange(p,0,true);
            check(p.settings.density==varied.density&&p.settings.swing==varied.swing&&p.settings.humanize==varied.humanize,"applying harmony does not reroll the visible feel");
            p.settings.density=.01;p.settings.swing=1.49;p.settings.humanize=.99;applyGlobalFeel(p,densityFeel|swingFeel|humanizeFeel);
            generate(p,scope,0,0,9127U);check(p.settings.density!=.01&&p.settings.swing!=1.49&&p.settings.humanize!=.99,"manual feel values are rerolled by the next explicit regeneration");
        }
    }
    {
        auto p=fixture(0);const int manual=p.addMidiTrack(),vocal=p.addVocalChopsTrack();p.tracks[(size_t)manual].clips[0].notes={{1,.5,72,100,0}};
        p.tracks[0].locked=true;p.tracks[1].clips[0].locked=true;const auto lockedTrack=p.tracks[0].clips;const auto lockedClip=p.tracks[1].clips[0];const auto handwritten=p.tracks[(size_t)manual].clips;
        p.settings.swing=0;applyGlobalFeel(p,swingFeel);const auto straight=notes(p,vocal);p.settings.swing=1.2;applyGlobalFeel(p,swingFeel);
        check(notes(p,vocal)!=straight,"global Swing also regenerates added Vocal chops");
        check(p.tracks[0].clips==lockedTrack&&p.tracks[1].clips[0]==lockedClip&&p.tracks[(size_t)manual].clips==handwritten,"global feel preserves locked track, locked clip and handwritten MIDI exactly");
    }
    for(int genre:{0,technoGenre,electroHouseGenre}){
        auto p=fixture(genre);auto& c=p.tracks[0].clips[0];c.span=4;tidyNotes(c,4);check(p.resizeClipLoop(0,0,16),"create a real four-beat clip loop");
        const auto period=p.tracks[0].clips[0].loopLength,span=p.tracks[0].clips[0].span;
        p.settings.humanize=.75;applyGlobalFeel(p,humanizeFeel);
        const auto& loop=p.tracks[0].clips[0];check(loop.loopLength==period&&loop.span==span&&!loop.loopSource.empty(),"global feel retains the loop period and source");
        auto repeated=loop;check(rebuildClipLoop(repeated,p.clipLength(0,0))&&repeated.notes==loop.notes,"loop repetitions follow the updated source");
        const auto before=p.settings;check(regenerateClip(p,0,0,881),"regenerate a looped editor clip");
        check(p.tracks[0].clips[0].loopLength==period&&p.settings.density!=before.density&&p.settings.swing!=before.swing&&p.settings.humanize!=before.humanize,"editor regeneration preserves the loop and updates all visible feel values");
    }
    for(int genre:{technoGenre,electroHouseGenre}){
        auto p=fixture(genre);const auto piano=p.tracks[0].clips,guitar=p.tracks[3].clips;
        applyInstrumentChange(p,0,true);
        check(p.tracks[0].clips==piano&&p.tracks[3].clips==guitar,"applying existing settings keeps the electronic renderer and MIDI");
        p.settings.humanize=0;applyGlobalFeel(p,humanizeFeel);
        check(p.tracks[0].clips[0].notes.size()<100&&p.tracks[3].clips[0].notes.size()<20,"electronic backing stays sparse after a global feel edit");
    }
    {
        auto p=fixture(0);for(auto& t:p.tracks)t.locked=true;const auto before=p.settings;generate(p,Scope::song,0,0,5721);
        check(p.settings.density==before.density&&p.settings.swing==before.swing&&p.settings.humanize==before.humanize,"all-locked regeneration does not move feel knobs without changing parts");
    }
}
}
