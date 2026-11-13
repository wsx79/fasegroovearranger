// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Harmony / feel / form audit: chords stay in key, NEW FEEL moves feel only, a new form keeps
// the chords, and drops are sometimes preceded by a full-band stop.
namespace arrangementaudit {
inline void verify(){
    using namespace fga;int weak=0;const auto check=[&weak](bool ok,const std::string& message){if(!ok){++weak;std::cout<<"WEAK "<<message<<std::endl;}};
    // Harmony of each bar: the pitch classes that sound in it.
    const auto pitches=[](const Clip& c){std::vector<int> v(64,0);for(const auto& n:c.notes){const int bar=std::clamp((int)std::floor((n.start+.125)/4),0,63);v[(size_t)bar]|=1<<(n.pitch%12);}return v;};
    const auto timing=[](const Clip& c){std::vector<int> v;for(const auto& n:c.notes){v.push_back((int)std::lround(n.start*960));v.push_back(n.velocity);}return v;};
    int gaps=0,drops=0,songs=0,builds=0,bassOut=0;
    for(int genre=0;genre<numGenres;++genre){
        int outside=0,total=0;
        for(unsigned seed=1;seed<=12;++seed){auto p=newProject(genre);generate(p,Scope::song,0,0,seed*2654435761U);++songs;
            // Every pitched note belongs to the key of its section or to the chord sounding at its attack.
            for(int ti:{0,1,2,3,leadTrack})for(size_t ci=0;ci<p.tracks[(size_t)ti].clips.size();++ci){const auto& c=p.tracks[(size_t)ti].clips[ci];
                for(const auto& n:c.notes){const double at=p.clipStart(ti,(int)ci)+n.start;const int si=p.sectionAtBeat(at);const auto s=activeHarmony(p,si);const auto chord=chordAtBeat(s,at);
                    const auto& scale=sgk::scaleIntervals(s.scale);const int pc=((n.pitch-s.key)%12+12)%12,rel=((n.pitch-chord.rootPc)%12+12)%12;
                    const bool inKey=std::find(scale.begin(),scale.end(),pc)!=scale.end();bool inChord=rel==0;for(int iv:{chord.third,chord.fifth,chord.seventh,chord.ninth,chord.eleventh,chord.thirteenth,chord.sixth})inChord=inChord||(iv>=0&&iv%12==rel);
                    // Short off-beat chromatic notes are approach / blue notes; a long or on-beat one is a wrong chord tone.
                    const bool heavy=ti!=1&&ti!=leadTrack&&(n.length>=.5||(n.length>=.25&&std::abs(n.start-std::round(n.start))<.05));   /* bass and lead lines use chromatic approach notes */++total;outside+=heavy&&!(inKey||inChord);}}
            for(size_t si=0;si+1<p.sections.size();++si)if(p.sections[si].type==Section::buildup&&p.sections[si+1].type==Section::drop&&!p.tracks[1].clips[si].notes.empty()){++builds;const double finish=p.sectionStart((int)si+1)-p.clipStart(1,(int)si);
                bassOut+=std::none_of(p.tracks[1].clips[si].notes.begin(),p.tracks[1].clips[si].notes.end(),[&](const Note& n){return n.start>=finish-4+1./32.;});}
            for(size_t si=0;si+1<p.sections.size();++si)if(p.sections[si+1].type==Section::drop&&p.sections[si].type!=Section::drop){++drops;const int gap=p.sections[si].dropGap;if(!gap)continue;++gaps;
                const double finish=p.sectionStart((int)si+1);bool silent=true;
                for(int ti=0;ti<(int)p.tracks.size();++ti){if(ti==16)continue;for(size_t ci=0;ci<p.tracks[(size_t)ti].clips.size();++ci){const double start=p.clipStart(ti,(int)ci);for(const auto& n:p.tracks[(size_t)ti].clips[ci].notes){const double a=start+n.start,b=a+n.length;if(a<finish-1./32.&&b>finish-gap+1./64.)silent=false;}}}
                check(silent,"genre "+std::to_string(genre)+": the stop before the drop is silent");}
        }
        std::cout<<"AUDIT genre="<<genre<<" out_of_key="<<outside<<"/"<<total<<'\n';check(outside*1000<=total,"genre "+std::to_string(genre)+": long and on-beat notes stay in key or on the chord");
        // NEW FEEL: new timing and dynamics, same notes.
        auto p=newProject(genre);p.settings.randomAutomation=false;p.sections={{Section::main,8,"A"}};p.normalise();generate(p);
        for(int ti:{0,1,3,leadTrack,4,8}){auto& e=p.tracks[(size_t)ti].engine;std::set<std::vector<int>> feels;const auto reference=pitches(p.tracks[(size_t)ti].clips[0]);if(p.tracks[(size_t)ti].clips[0].notes.empty())continue;bool same=true;
            for(unsigned k=1;k<=8;++k){e.humanizeSeed=k*7919U;applyInstrumentChange(p,ti,false);const auto& c=p.tracks[(size_t)ti].clips[0];feels.insert(timing(c));same=same&&pitches(c)==reference;}
            std::cout<<"AUDIT genre="<<genre<<" track="<<ti<<" new_feel="<<feels.size()<<"/8 same_notes="<<same<<'\n';
            check(feels.size()>=4,"genre "+std::to_string(genre)+" track "+std::to_string(ti)+": NEW FEEL changes the feel");check(same,"genre "+std::to_string(genre)+" track "+std::to_string(ti)+": NEW FEEL keeps the harmony of every bar");}
        // A new form keeps the song's chords and draws a different layout.
        auto song=newProject(genre);const auto harmony=song.settings;std::set<std::vector<int>> forms;
        for(unsigned seed=1;seed<=10;++seed){auto next=song;if(!rearrangeSong(next,seed*104729U))continue;std::vector<int> f;for(const auto& s:next.sections){f.push_back((int)s.type);f.push_back(s.bars);}forms.insert(f);
            check(next.settings.key==harmony.key&&next.settings.scale==harmony.scale&&next.settings.degrees==harmony.degrees&&next.settings.freeChords==harmony.freeChords&&next.settings.chordCount==harmony.chordCount,"genre "+std::to_string(genre)+": a new arrangement keeps the chords");}
        check(forms.size()>=4,"genre "+std::to_string(genre)+": a new arrangement changes the form");
    }
    std::cout<<"AUDIT drop_gaps="<<gaps<<"/"<<drops<<" drops in "<<songs<<" songs\n";check(gaps*5>=drops&&gaps*5<=drops*4,"some, not all, drops are preceded by a stop");
    std::cout<<"AUDIT build_bass_out="<<bassOut<<"/"<<builds<<"\n";check(bassOut*2>=builds&&bassOut<builds,"the bass usually, not always, leaves before the drop");
    {   // Section energy: the same takes play less at Low and more at Peak; Auto keeps the notes as before.
        for(int genre:{0,4,13}){auto p=newProject(genre);p.settings.randomAutomation=false;p.sections={{Section::main,8,"A"},{Section::main,8,"B"}};p.normalise();generate(p);
            auto count=[&](int energy){auto q=p;q.sections[1].energy=energy;generate(q,Scope::section,0,1,0);double n=0;for(const auto& t:q.tracks)if(t.clips.size()>1)for(const auto& x:t.clips[1].notes)n+=x.velocity;return n;};   // activity: notes weighted by how hard they play
            const auto autoCount=count(-1),low=count(0),peak=count(3);std::cout<<"AUDIT energy genre="<<genre<<" low="<<low<<" auto="<<autoCount<<" peak="<<peak<<"\n";
            check(low<autoCount&&peak>autoCount,"genre "+std::to_string(genre)+": section energy changes how much the parts play");
            auto again=p;generate(again,Scope::section,0,1,0);size_t same=0,before=0;for(size_t ti=0;ti<p.tracks.size();++ti)if(p.tracks[ti].clips.size()>1){before+=p.tracks[ti].clips[1].notes.size();same+=again.tracks[ti].clips[1].notes.size();}
            check(same==before,"genre "+std::to_string(genre)+": Auto energy leaves the section as generated");}
    }
    ::check(weak==0,"arrangement audit found problems");
}
}
