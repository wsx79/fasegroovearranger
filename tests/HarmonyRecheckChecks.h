// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "../ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.h"
#include "../ThirdParty/fasegroovelead/Source/engine/GrooveEngine.h"
namespace harmonyrecheck {
inline fga::Project fixture(int genre){
    using namespace fga;auto p=newProject(genre);p.settings.randomAutomation=false;p.settings.key=0;p.settings.scale=0;p.settings.extension=0;p.settings.voicing=0;p.settings.chordCount=2;p.settings.chordSteps=8;p.settings.degrees={0,4,0,4,0,4,0,4};p.settings.freeChords.fill(-1);p.settings.bassMode=1;p.settings.stringsMode=1;
    p.sections={{Section::main,1,"A"},{Section::main,1,"B"}};auto& h=p.sections[1].harmony;h.enabled=true;h.key=6;h.scale=0;h.chordCount=2;h.chordSteps=8;h.degrees=p.settings.degrees;
    for(auto& t:p.tracks){t.freeTimeline=false;t.clips.assign(2,{});auto& e=t.engine;e.humanize=e.swing=e.strum=0;e.fillEvery=e.variationEvery=0;e.pushChanges=e.afterFill=e.realism=false;e.movement=e.evolveBars=0;}
    p.normalise();generate(p);return p;
}
inline void rootGrid(fga::Project& p,int track){
    if(track==2)p.settings.stringsMode=1;
    if(fga::isElectronicGenre(p.settings.genre))return;
    if(track==3||track==fga::leadTrack){auto& e=p.tracks[(size_t)track].engine;int found=-1;
        if(track==3){const auto& patterns=fgg::guitarPatterns();for(size_t i=0;i<patterns.size();++i)if(patterns[i].lanes[11].find_first_of("xXg")<16){found=(int)i;break;}}
        else{const auto& patterns=fgl::leadPatterns();for(size_t i=0;i<patterns.size();++i)if(patterns[i].lanes[11].find_first_of("xXg")<16){found=(int)i;break;}}
        check(found>=0,"root-only reference pattern exists");e.pattern=found;e.laneMute.fill(true);e.laneMute[11]=false;e.laneLevel[11]=1;return;
    }
    auto& e=p.tracks[(size_t)track].engine;e.pattern=sgk::userPatternIndex();e.userRight.fill('.');e.userLeft.fill('.');
    for(int i=0;i<32;i+=4)(track==1?e.userLeft:e.userRight)[(size_t)i]='X';
}
inline std::vector<std::pair<int,int>> signature(const fga::Clip& clip,bool rhythm){
    std::vector<std::pair<int,int>> result;int first=-1;
    if(clip.notes.empty())return result;
    // Follow the first entrance, excluding the final turnaround bar. Velocity,
    // transposition, micro-notes and humanisation cannot create another phrase.
    const double from=std::floor(clip.notes.front().start/4)*4,to=std::max(from+4,std::min(from+16,28.));
    for(const auto& n:clip.notes){if(n.start<from)continue;if(n.start>=to)break;if(n.length<1./64.)continue;if(first<0)first=n.pitch;result.emplace_back((int)std::lround((n.start-from)*4),rhythm?0:n.pitch-first);}
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
inline void verify(){
    using namespace fga;
    {auto p=fixture(0);p.sections[1].bars=8;p.settings.chordSteps=64;p.sections[1].harmony.enabled=false;p.normalise();
        for(double beat:{4.,15.,16.,20.,31.,32.}){double a,b;std::string name;chordCell(p,beat,a,b,name);check(a<=beat&&b>beat,"ruler chord cell contains the inspected beat");check(name==chordNameAt(p,beat),"ruler harmony matches played harmony at non-aligned section starts");check(b<=std::min((std::floor(beat/16)+1)*16,p.sectionStart(2)),"ruler stops at the actual global chord boundary");}}
    for(int genre=0;genre<numGenres;++genre){
        const auto base=fixture(genre);
        for(int track:{0,1,2,3,leadTrack})for(int mode:{0,1,2,5})for(int key=0;key<12;++key)for(double start:{0.,1.25})for(double phase:{0.,1.25,2.75}){
            if(mode!=0&&(key!=0||start!=1.25||phase!=2.75))continue;
            auto p=base;p.settings.scale=p.sections[1].harmony.scale=mode;p.settings.key=key;p.sections[1].harmony.key=(key+6)%12;rootGrid(p,track);auto& t=p.tracks[(size_t)track];t.freeTimeline=true;t.clips.assign(1,{});t.clips[0].seed=0;t.clips[0].position=start;t.clips[0].span=8-start;t.clips[0].loopLength=4;t.clips[0].loopPhase=phase;
            generate(p,Scope::clip,track,0,0);
            check(p.tracks[(size_t)track].clips.size()==2,"a regenerated free loop is partitioned at an independent harmony boundary");
            int attacks=0;for(size_t ci=0;ci<2;++ci){const auto& c=p.tracks[(size_t)track].clips[ci];check(c.loopLength==4,"harmony partition preserves the original loop period");
                const double expected=std::fmod(phase+c.position-start,4.);check(std::abs(c.loopPhase-expected)<1e-8,"harmony partition preserves source phase, including preroll");
                for(const auto& n:c.notes){++attacks;const double at=c.position+n.start;const auto chord=chordAtBeat(resolveSectionHarmony(p,p.sectionAtBeat(at)),at);const int iv=(n.pitch-chord.rootPc+120)%12;
                    bool fits=iv==0||iv==chord.third||iv==chord.fifth;
                    if(track==leadTrack&&isElectronicGenre(genre)){auto colour=resolveSectionHarmony(p,p.sectionAtBeat(at));colour.extension=2;const auto extended=chordAtBeat(colour,at);fits=fits||iv==extended.seventh||iv==extended.ninth;}
                    if(!fits)std::cerr<<"phase fixture genre="<<genre<<" track="<<track<<" mode="<<mode<<" key="<<key<<" start="<<start<<" phase="<<phase<<" clip="<<ci<<" position="<<c.position<<" sourcePhase="<<c.loopPhase<<" at="<<at<<" pitch="<<n.pitch<<" chord="<<chord.rootPc<<'/'<<chord.third<<'/'<<chord.fifth<<'\n';
                    check(fits,"generated loop notes follow the actual harmony across keys and nonzero source phases");}}
            check(attacks>0,"cross-section harmony fixture must be audible");
            const auto take=p.tracks[(size_t)track].clips;generate(p,Scope::clip,track,0,0);check(take[0].notes==p.tracks[(size_t)track].clips[0].notes&&take[1]==p.tracks[(size_t)track].clips[1],"APPLY keeps the selected source reproducible and excludes the neighbouring partition");
        }
        {auto p=base;rootGrid(p,0);p.tracks[0].freeTimeline=true;p.tracks[0].clips.assign(2,{});p.tracks[0].clips[0].span=8;p.tracks[0].clips[1].position=12;p.tracks[0].clips[1].span=4;p.tracks[0].clips[1].name="Untouched";p.tracks[0].clips[1].notes={{0,1,70,97,0}};
            const auto excluded=p.tracks[0].clips[1];const auto drums=p.tracks[4].clips;generate(p,Scope::clip,0,0,313);
            check(p.tracks[0].clips.size()==3&&p.tracks[0].clips.back().notes==excluded.notes&&p.tracks[0].clips.back().name==excluded.name&&p.tracks[4].clips==drums,"fresh cross-harmony generation preserves excluded clips and tracks");
            const auto all=p.tracks[0].clips;generate(p,Scope::track,0,0,0);check(p.tracks[0].clips[0].notes==all[0].notes&&p.tracks[0].clips[1].notes==all[1].notes,"new cross-section takes reproduce from their stored seeds");}
        {auto p=base;rootGrid(p,0);p.tracks[0].freeTimeline=true;p.tracks[0].clips.assign(1,{});p.tracks[0].clips[0].span=8;p.tracks[0].clips[0].notes={{1,1,60,100,0},{5,1,70,100,0}};
            auto expected=p;check(expected.splitClip(0,0,4)>=0,"section scope split fixture");const auto outside=expected.tracks[0].clips[1].notes;
            generate(p,Scope::section,0,0,37);check(p.tracks[0].clips.size()==2&&p.tracks[0].clips[1].notes==outside,"section regeneration preserves the MIDI outside its harmonic boundary");
            for(const auto& c:p.tracks[0].clips)check(c.loopLength==0,"automatic harmony partition does not turn an unlooped clip into a repeating source");}
        {auto p=base;p.tracks[0].freeTimeline=true;p.tracks[0].clips.assign(1,{});p.tracks[0].clips[0].span=8;p.tracks[0].clips[0].locked=true;p.tracks[0].clips[0].notes={{0,8,60,100,0}};p.normalise();const auto locked=p.tracks[0].clips;generate(p,Scope::track,0,0,55);check(p.tracks[0].clips==locked,"harmony partition cannot alter a locked take");}
        for(int track:{0,1,2,3,leadTrack})for(double period:{4.,8.}){
            auto q=base;q.sections[0].bars=8;q.sections[1].harmony.enabled=false;q.tracks[(size_t)track].clips[0].loopLength=period;q.normalise();
            generate(q,Scope::clip,track,0,57);const auto take=q.tracks[(size_t)track].clips[0];
            check(!take.notes.empty()&&take.loopLength==period,"short requested loops retain an audible source before section entry masks");
            applyInstrumentChange(q,track,false);check(q.tracks[(size_t)track].clips[0].notes==take.notes,"short loops reproduce on APPLY");
        }
        {auto q=base;q.settings.stringsMode=0;q.normalise();generate(q,Scope::song,0,0,63);
            generate(q,Scope::clip,2,0,57);const auto take=q.tracks[2].clips[0].notes;check(!take.empty(),"explicit Strings request overrides the new-song Off default");
            applyInstrumentChange(q,2,false);check(q.tracks[2].clips[0].notes==take,"APPLY retains the explicit Strings take while the default is Off");
            q.settings.key=(q.settings.key+1)%12;applyInstrumentChange(q,0,true);check(!q.tracks[2].clips[0].notes.empty(),"harmony changes retain explicitly requested Strings");
            generate(q,Scope::song,0,0,64);check(q.tracks[2].clips[0].notes.empty(),"a new whole song still respects the Strings Off default");
            q.settings.stringsMode=1;applyInstrumentChange(q,2,false);check(!q.tracks[2].clips[0].notes.empty(),"Strings Pad selection renders notes");
            q.settings.stringsMode=0;applyInstrumentChange(q,2,false);check(q.tracks[2].clips[0].notes.empty(),"changing the actual Strings style to Off clears its take");
        }
        auto p=newProject(genre);p.settings.randomAutomation=false;p.sections={{Section::main,8,"Review"},{Section::drop,8,"Reply"}};for(auto& t:p.tracks){t.clips.assign(2,{});t.engine.pattern=-1;t.engine.fillEvery=8;t.engine.variationEvery=8;t.engine.humanize=-1;t.engine.swing=-1;}p.normalise();generate(p);
        for(int track:{0,1,3,leadTrack}){auto song=p;std::set<std::vector<std::pair<int,int>>> music,rhythms;int repeats=0,stubs=0;std::vector<std::pair<int,int>> previous;
            for(unsigned seed=1;seed<=80;++seed){generate(song,Scope::clip,track,0,seed);const auto& clip=song.tracks[(size_t)track].clips[0];const auto current=signature(clip,false);const auto rhythm=signature(clip,true);
                auto dynamics=clip;for(size_t i=0;i<dynamics.notes.size();++i)dynamics.notes[i].velocity=1+(int)(i*37%127);check(signature(dynamics,false)==current,"velocity-only changes cannot inflate musical diversity");
                check(!current.empty(),"every requested genre/instrument has audible primary notes");if(seed>1&&current==previous)++repeats;previous=current;music.insert(current);rhythms.insert(rhythm);
                if(track==leadTrack)for(const auto& n:clip.notes)stubs+=n.length<1./128.;}
            check(music.size()>=8&&rhythms.size()>=4,"regeneration varies primary phrases and rhythms, not just jitter, velocity or tiny ornaments");check(stubs==0,"lead regeneration never leaves millisecond note fragments");check(repeats==0,"regeneration changes the primary musical phrase, beyond the ending fill");
            std::cout<<"HARMONY_RECHECK genre="<<genre<<" track="<<track<<" primary_music="<<music.size()<<"/80 rhythms="<<rhythms.size()<<" immediate="<<repeats<<" micro_notes="<<stubs<<'\n';
        }
    }
    {auto p=fixture(4);const int track=p.addVocalChopsTrack();auto& t=p.tracks[(size_t)track];t.engine.humanize=t.engine.swing=0;t.engine.density=.8;t.freeTimeline=true;t.clips.assign(1,{});t.clips[0].span=8;t.clips[0].loopLength=4;t.clips[0].loopPhase=1.25;
        generate(p,Scope::clip,track,0,37);check(p.tracks[(size_t)track].clips.size()==2,"pitched vocal guide follows independent section harmony");
        for(const auto& c:p.tracks[(size_t)track].clips)for(const auto& n:c.notes){const double at=c.position+n.start;const auto chord=chordAtBeat(resolveSectionHarmony(p,p.sectionAtBeat(at)),at);const int iv=(n.pitch-chord.rootPc+120)%12;check(iv==0||iv==chord.third||iv==chord.fifth,"pitched vocal notes follow their current harmonic cell");}}
    {auto p=fixture(0);p.settings.randomAutomation=true;p.tracks[0].freeTimeline=true;p.tracks[0].clips.assign(1,{});p.tracks[0].clips[0].span=8;p.tracks[0].volume.enabled=true;p.tracks[0].volume.points={{0,.7,0},{3,.8,0},{5,.4,0},{8,.7,0}};p.normalise();
        auto expected=p;randomiseAutomation(expected,Scope::clip,0,0,37);generate(p,Scope::clip,0,0,37);
        check(p.tracks[0].volume==expected.tracks[0].volume,"automation still covers the entire selected clip after harmony partitioning");}
    {int folded=0;for(int genre:{technoGenre,electroHouseGenre})for(int key=0;key<12;++key)for(unsigned seed=1;seed<=16;++seed){
        auto p=fixture(genre);p.settings.key=key;p.settings.chordCount=4;p.settings.chordSteps=16;p.settings.degrees={0,3,4,5,0,3,4,5};p.sections[1].harmony.enabled=false;
        Clip original,shifted;original.seed=original.motifSeed=shifted.seed=shifted.motifSeed=seed;generateElectronicClip(p,original,leadTrack,0,32,Section::drop,true);
        auto& e=p.tracks[leadTrack].engine;e.globalOctave=2;e.laneOctave.fill(2);generateElectronicClip(p,shifted,leadTrack,0,32,Section::drop,true);
        check(original.notes.size()==shifted.notes.size(),"octave controls retain the electronic phrase rhythm");
        const int offset=shifted.notes.empty()?0:shifted.notes.front().pitch-original.notes.front().pitch;
        for(size_t i=0;i<original.notes.size();++i){const auto& a=original.notes[i];const auto& b=shifted.notes[i];check(b.pitch-a.pitch==offset,"MIDI ceiling handling preserves melodic contour");folded+=a.pitch+48>127;check(b.pitch>=0&&b.pitch<=127&&a.pitch%12==b.pitch%12,"combined octave controls preserve harmony at MIDI limits");}}
        check(folded>0,"octave fixture exercises the MIDI ceiling");}
    for(int genre:{technoGenre,electroHouseGenre})for(int track:{3,leadTrack}){
        auto p=fixture(genre);p.sections[0].bars=8;p.sections[1].harmony.enabled=false;p.normalise();generate(p,Scope::track,track,0,0);const auto notes=p.tracks[(size_t)track].clips[0].notes;check(!notes.empty(),"electronic lane control fixture is audible");
        for(const auto& n:notes)check(track==3?(n.lane==10||n.lane==11):(n.lane==0||n.lane==1),"electronic notes retain their actual instrumental roles");
        p.tracks[(size_t)track].engine.laneMute.fill(true);applyInstrumentChange(p,track,false);check(p.tracks[(size_t)track].clips[0].notes.empty(),"electronic guitar/lead respect lane mute controls");
        p.tracks[(size_t)track].engine.laneMute.fill(false);p.tracks[(size_t)track].engine.laneLevel.fill(0);applyInstrumentChange(p,track,false);check(p.tracks[(size_t)track].clips[0].notes.empty(),"electronic guitar/lead respect zero lane levels");
    }
}
}
