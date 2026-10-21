// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <set>
#include <tuple>
using MusicalFingerprint=std::vector<std::tuple<int,int,int>>;
inline MusicalFingerprint musicalFingerprint(const fga::Clip& c){
    MusicalFingerprint result;
    for(const auto& n:c.notes)result.emplace_back((int)std::lround(n.start*4),n.pitch,n.lane);
    std::sort(result.begin(),result.end());return result;
}
inline void verifyRegeneration(){
    for(int genre=0;genre<13;++genre){
        auto p=fga::newProject(genre);p.sections={{fga::Section::main,8,"Main"},{fga::Section::main,8,"Main 2"}};p.normalise();
        std::array<std::set<MusicalFingerprint>,fga::trackCount> phrases;
        for(unsigned seed:{91U,317U,1709U,4951U,9187U,17891U}){
            fga::generate(p,fga::Scope::song,0,0,seed);
            for(int ti=0;ti<fga::trackCount;++ti)phrases[ti].insert(musicalFingerprint(p.tracks[ti].clips[0]));
            const auto bassSettings=fga::pianoEngineSettings(p,1,p.tracks[1].clips[0].seed,p.tracks[1].clips[0].motifSeed);
            const auto& source=sgk::bassLines()[(size_t)sgk::bassLineIndex((int)bassSettings.bass)].line;
            check(!bassSettings.bassLineOverride.empty(),"regenerated bass has a developed figure");
            for(size_t bar=0;bar<source.size();bar+=16){
                size_t first=bar;while(first<bar+16&&(source[first]=='.'||source[first]=='='||source[first]=='G'))++first;
                if(first<bar+16)check(bassSettings.bassLineOverride[first]==source[first],"bass retains its first pitched anchor in each bar, including offbeats");
            }
            const auto take=p;fga::generate(p);
            for(int ti=0;ti<fga::trackCount;++ti)check(p.tracks[ti].clips==take.tracks[ti].clips,"new musical takes must be reproducible without a new seed");
        }
        // All scopes replace musical content while leaving other clips and sound assignments alone.
        for(auto scope:{fga::Scope::track,fga::Scope::section,fga::Scope::clip}){
            auto q=p;const auto before=q;
            fga::generate(q,scope,0,0,4517);
            check(musicalFingerprint(q.tracks[0].clips[0])!=musicalFingerprint(before.tracks[0].clips[0]),"scope changes the actual phrase");
            for(int ti=0;ti<fga::trackCount;++ti){
                check(q.tracks[ti].sound==before.tracks[ti].sound&&q.tracks[ti].gain==before.tracks[ti].gain,"regeneration preserves chosen sounds and mix");
                for(int ci=0;ci<2;++ci){
                    const bool targeted=scope==fga::Scope::track?ti==0:scope==fga::Scope::section?ci==0:ti==0&&ci==0;
                    if(!targeted)check(q.tracks[ti].clips[ci]==before.tracks[ti].clips[ci],"scope preserves every untargeted take");
                }
            }
            if(scope==fga::Scope::track)check(q.tracks[0].clips[0].motifSeed==q.tracks[0].clips[1].motifSeed,"track keeps one coherent motif through its sections");
            const auto take=q;fga::generate(q,scope,0,0);
            for(int ti=0;ti<fga::trackCount;++ti)check(q.tracks[ti].clips==take.tracks[ti].clips,"apply settings does not reroll a stored take");
        }
        std::cout<<"Diversity "<<genre<<":";for(int ti=0;ti<fga::trackCount;++ti)std::cout<<" "<<phrases[ti].size();std::cout<<"\n";
        check(phrases[0].size()>=3,"regeneration must change piano rhythm/pitches, excluding feel and velocity");
        check(phrases[1].size()>=3,"regeneration must develop the selected bass line beyond timing and velocity");
        check(phrases[3].size()>=3,"regeneration must change guitar articulation/rhythm, excluding feel and velocity");
    }
}
