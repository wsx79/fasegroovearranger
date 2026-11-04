// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace deepauditchecks {
inline void verify(){
    using namespace fga;
    // C5: reproduce the audit after inheriting all three header controls. APPLY
    // must be idempotent on every excluded take, including another clip on the
    // same track, independent timelines, electronic styles and vocal chops.
    for(int genre:{0,technoGenre,electroHouseGenre})for(auto scope:{Scope::track,Scope::clip,Scope::section}){
        auto p=globalfeelchecks::fixture(genre);p.addVocalChopsTrack();
        p.settings.density=.72;p.settings.swing=.13;p.settings.humanize=.29;
        applyGlobalFeel(p,densityFeel|swingFeel|humanizeFeel);
        const auto before=p;generate(p,scope,0,0,1234);
        check(p.settings.density!=before.settings.density,"scoped generation still moves the header controls");
        for(int ti=0;ti<(int)p.tracks.size();++ti)for(int ci=0;ci<(int)p.tracks[(size_t)ti].clips.size();++ci){
            const bool selected=scope==Scope::track?ti==0:scope==Scope::clip?ti==0&&ci==0:ci==0;
            if(!selected&&p.tracks[(size_t)ti].clips[(size_t)ci].notes!=before.tracks[(size_t)ti].clips[(size_t)ci].notes)std::cerr<<"C5 scope="<<(int)scope<<" genre="<<genre<<" track="<<ti<<" clip="<<ci<<"\n";
            if(!selected)check(p.tracks[(size_t)ti].clips[(size_t)ci].notes==before.tracks[(size_t)ti].clips[(size_t)ci].notes,"scoped generation preserves excluded MIDI");
        }
        const auto take=p;
        for(int ti=0;ti<(int)p.tracks.size();++ti)applyInstrumentChange(p,ti,false);
        for(int ti=0;ti<(int)p.tracks.size();++ti)check(p.tracks[(size_t)ti].clips==take.tracks[(size_t)ti].clips,"unchanged APPLY preserves effective feel of every take after scoped regeneration");
        p.settings.swing=.81;applyGlobalFeel(p,swingFeel);
        for(const auto& t:p.tracks)for(const auto& c:t.clips)check(c.feel[1]==.81,"explicit header changes reach takes with frozen feel");
    }
    {
        auto p=globalfeelchecks::fixture(0);const int vocal=p.addVocalChopsTrack();p.settings.density=.72;p.settings.swing=.13;p.settings.humanize=.29;applyGlobalFeel(p,densityFeel|swingFeel|humanizeFeel);
        for(int ti:{0,vocal}){auto& t=p.tracks[(size_t)ti];auto c=t.clips[0];c.span=32;c.loopLength=16;c.loopSource=c.notes;rebuildClipLoop(c,32);t.freeTimeline=true;t.clips={c};}
        generate(p,Scope::section,0,1,2345);
        for(int ti:{0,vocal}){const auto& clips=p.tracks[(size_t)ti].clips;check(clips.size()==2&&clips[0].feel==std::array<double,3>{.72,.13,.29}&&clips[1].feel==std::array<double,3>{p.settings.density,p.settings.swing,p.settings.humanize},"section regeneration splits independent piano and vocal takes before assigning feel");const auto before=clips;applyInstrumentChange(p,ti,false);check(p.tracks[(size_t)ti].clips==before,"APPLY preserves feel on both sides of a section split");}
    }
    // O1: four-beat source, split at beat six. Both fragments retain phase.
    Project p;p.normalise();auto& t=p.tracks[0];t.freeTimeline=true;t.clips.clear();
    Clip loop;loop.span=4;loop.notes={{0,.5,60,100,0}};t.clips={loop};
    check(p.resizeClipLoop(0,0,16)&&p.splitClip(0,0,6)==1,"create and split extended loop");
    check(p.tracks[0].clips[0].loopLength==4&&p.tracks[0].clips[1].loopLength==4&&p.tracks[0].clips[1].loopPhase==2,"split retains period and right-fragment phase");
    check(p.resizeClipLoop(0,0,10),"extend left loop fragment");
    const auto& left=p.tracks[0].clips[0];check(left.notes.size()==3&&left.notes[0].start==0&&left.notes[1].start==4&&left.notes[2].start==8,"left loop extends on original grid after split");
    p=Project{};p.normalise();p.tracks[0].freeTimeline=true;p.tracks[0].clips={loop};p.resizeClipLoop(0,0,16);p.splitClip(0,0,6);
    check(p.resizeClipLoop(0,1,14),"extend right loop fragment");const auto& right=p.tracks[0].clips[1];check(right.notes.size()==3&&right.notes[0].start==2&&right.notes[1].start==6&&right.notes[2].start==10,"right loop extends with original phase");
    // C3/C4: source seconds survive splits, overwrites, copies and long clips.
    Project audio;audio.normalise();audio.tracks[0].freeTimeline=true;Clip take;take.span=720;take.audioOffset=3;take.audioDuration=360;take.notes={{0,720,60,127,0}};audio.tracks[0].clips={take};audio.normalise();
    check(audio.clipLength(0,0)==720&&audio.tracks[0].clips[0].notes[0].length==720,"long audio region retains complete timeline and note duration");
    check(audio.splitClip(0,0,4)==1,"split audio region");check(audio.tracks[0].clips[0].audioDuration==2&&audio.tracks[0].clips[1].audioOffset==5&&audio.tracks[0].clips[1].audioDuration==358,"audio split retains source offset and duration");
    const int copy=audio.moveClip(0,1,1,0,true);check(copy>=0&&audio.tracks[1].clips[(size_t)copy].audioOffset==5,"audio copy retains source offset");
    Clip replacement;replacement.span=4;check(audio.placeClip(0,replacement,8)>=0,"overwrite middle of audio region");const int tail=audio.clipAtBeat(0,12);check(tail>=0&&audio.tracks[0].clips[(size_t)tail].audioOffset==9,"overwrite advances retained audio tail offset");
}
}
