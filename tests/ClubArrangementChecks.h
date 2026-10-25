// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iomanip>
#include <set>
#include <sstream>

inline void verifyClubProject(const fga::Project& p,int genre,const std::string& folder){
    const auto& profile=fga::genreProfile(genre);
    check(p.settings.bpm>=profile.minBpm&&p.settings.bpm<=profile.maxBpm,"tempo outside genre working range");
    const auto last=p.sections.size()-1;
    const double outroEnd=p.sections.back().bars*4.;
    for(size_t si=0;si<p.sections.size();++si){
        check(p.sections[si].bars%8==0,"generated club section must have complete eight-bar phrases");
        check(std::fmod(p.sectionStart((int)si),32.)==0,"DJ phrase boundary drift");
        if(si>0&&si<last)check(p.sections[si].type!=fga::Section::intro&&p.sections[si].type!=fga::Section::outro,"mix passages belong at the song edges");
    }
    // The first/final phrase offers a rhythm-only blend. Musical layers are
    // introduced/removed within the same intro/outro, not outside the song.
    for(int ti=0;ti<4;++ti){
        for(auto n:p.tracks[(size_t)ti].clips.front().notes)check(n.start>=32,"melodic instrument enters before the first mix phrase ends");
        for(auto n:p.tracks[(size_t)ti].clips.back().notes)check(n.start+n.length<=outroEnd-32+1e-8,"held harmony crosses into the final mix phrase");
    }
    for(int bar=0;bar<8;++bar){
        bool in=false,out=false;
        for(auto n:p.tracks[4].clips.front().notes)in=in||(n.start>=bar*4&&n.start<(bar+1)*4);
        for(auto n:p.tracks[4].clips.back().notes)out=out||(n.start>=outroEnd-32+bar*4&&n.start<outroEnd-32+(bar+1)*4);
        check(in&&out,"mix phrase has a whole bar without its kick reference");
    }
    bool theme=false,bass=false;
    for(size_t si=1;si<last;++si)if(p.sections[si].type==fga::Section::main||p.sections[si].type==fga::Section::drop){
        theme=theme||!p.tracks[0].clips[si].notes.empty();bass=bass||!p.tracks[1].clips[si].notes.empty();
    }
    check(theme&&bass,"club body must contain its theme and bass");
    if(!profile.rollingBuild)for(size_t si=0;si<p.sections.size();++si)if(p.sections[si].type==fga::Section::buildup){
        int hits=0;const double end=p.sections[si].bars*4.;
        for(auto n:p.tracks[5].clips[si].notes)if(n.start>=end-4)++hits;
        check(hits<=8,"soulful/organic lift must not become a generic 16th-note snare build");
    }
    if(folder.empty())return;
    const std::string prefix=folder+"/club-"+std::to_string(genre);
    auto midi=fga::midiFile(p);std::ofstream file(prefix+".mid",std::ios::binary);file.write((const char*)midi.data(),(std::streamsize)midi.size());
    std::ofstream report(prefix+".tsv");
    report<<"genre\tbpm\tsection\ttype\tbar\tseconds\tphrase_bars";
    for(const auto& t:p.tracks)report<<'\t'<<t.name;
    report<<'\n';double beat=0;
    for(size_t si=0;si<p.sections.size();++si){
        const auto& sec=p.sections[si];
        for(int bar=0;bar<sec.bars;bar+=8){
            report<<fga::genres()[(size_t)genre]<<'\t'<<p.settings.bpm<<'\t'<<sec.name<<'\t'<<fga::sectionName(sec.type)<<'\t'<<beat/4+bar+1<<'\t'<<(beat+bar*4)*60/p.settings.bpm<<"\t8";
            for(const auto& t:p.tracks){int notes=0;for(auto n:t.clips[si].notes)if(n.start>=bar*4&&n.start<(bar+8)*4)++notes;report<<'\t'<<notes;}
            report<<'\n';
        }
        beat+=sec.bars*4;
    }
    check(file.good()&&report.good(),"club evidence write failed");
}
inline void verifyClubSeedCases(){
    for(int genre=0;genre<13;++genre){
        auto p=fga::newProject(genre);
        for(std::uint32_t seed:{1U,42U,4294967295U}){
            p.settings.seed=seed;fga::applyGenre(p,genre);fga::arrangeSong(p);fga::generate(p);
            verifyClubProject(p,genre,"");
        }
    }
}
inline void verifyClubEdgeCases(){
    auto base=fga::newProject(1);
    {auto malformed=base;malformed.settings.bpm=std::numeric_limits<double>::quiet_NaN();malformed.settings.key=-500;malformed.settings.scale=500;
        const auto safe=fga::midiFile(malformed);check(safe.size()>100&&safe[11]==fga::trackCount+1,"MIDI metadata is normalised before serialization");
        bool rejected=false;try{fga::midiFile(base,-2);}catch(const std::out_of_range&){rejected=true;}check(rejected,"invalid negative MIDI stem index rejected");}

    for(double bpm:{80.,140.,std::numeric_limits<double>::quiet_NaN()}){
        auto p=base;p.settings.bpm=bpm;fga::arrangeSong(p);
        check(p.settings.bpm>=118&&p.settings.bpm<=125,"soulful automatic arrangement cannot run at 140 BPM");
    }
    for(int track:{1,2,8}){auto p=base;p.tracks[(size_t)track].engine.grooveBars=4;fga::arrangeSong(p);check(p.length()>base.length(),"long bass/strings/drum motifs must affect development too");}
    auto low=base,high=base;low.settings.bpm=118;high.settings.bpm=125;fga::arrangeSong(low);fga::arrangeSong(high);
    check(low.length()==high.length()&&low.length()*60/low.settings.bpm>high.length()*60/high.settings.bpm,"tempo changes seconds, not arbitrary bar stretching");
    for(int count:{3,5,7}){
        auto p=base;p.settings.chordCount=count;p.settings.chordSteps=64;p.tracks[0].engine.grooveBars=3;
        fga::arrangeSong(p);for(auto sec:p.sections)check(sec.bars%8==0,"odd harmonic cycles must not destroy the DJ grid");
    }
    auto a=fga::pianoEngineSettings(base,0,123),b=fga::pianoEngineSettings(base,0,456);
    check(a.userRight==b.userRight&&a.userLeft==b.userLeft,"section variation must not replace the song motif");
    base.tracks[0].engine.pattern=sgk::userPatternIndex();base.tracks[1].engine.pattern=0;base.tracks[3].engine.pattern=0;
    fga::applyGenre(base,6);
    check(base.tracks[0].engine.pattern==sgk::userPatternIndex()&&base.tracks[1].engine.pattern==-1&&base.tracks[3].engine.pattern==-1,"genre change resets old factory grooves, preserves user work");
    auto p=fga::newProject(1);p.settings.key=0;p.settings.scale=0;p.settings.extension=0;p.settings.bassMode=1;p.settings.chordCount=3;p.settings.chordSteps=16;p.settings.degrees={0,3,4,0,0,0,0,0};p.settings.freeChords.fill(-1);
    p.sections={{fga::Section::main,8,"First phrase"},{fga::Section::main,8,"Continuation"}};
    auto& e=p.tracks[1].engine;e.pattern=sgk::userPatternIndex();e.userLeft.fill('.');e.userRight.fill('.');e.userLeft[0]='x';e.userLeft[16]='x';e.humanize=0;e.swing=0;e.fillEvery=0;e.variationEvery=0;e.realism=false;
    fga::generate(p,fga::Scope::track,1);
    check(!p.tracks[1].clips[1].notes.empty()&&p.tracks[1].clips[1].notes.front().pitch%12==7,"C-F-G cycle must continue on G at global bar nine, not restart on C");
}
