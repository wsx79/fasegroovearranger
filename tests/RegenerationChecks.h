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
inline std::vector<std::pair<int,int>> rhythmFingerprint(const fga::Clip& clip){
    std::vector<std::pair<int,int>> result;
    for(const auto& n:clip.notes)result.emplace_back((int)std::lround(n.start*4),n.lane);
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());
    return result;
}
inline bool sameHarmony(const fga::Settings& a,const fga::Settings& b){
    return a.scale==b.scale&&a.chordCount==b.chordCount
        &&std::equal(a.degrees.begin(),a.degrees.begin()+a.chordCount,b.degrees.begin())
        &&std::equal(a.freeChords.begin(),a.freeChords.begin()+a.chordCount,b.freeChords.begin());
}
inline void verifyRegeneration(){
    for(int genre=0;genre<13;++genre){
        auto p=fga::newProject(genre);p.sections={{fga::Section::main,8,"Main"},{fga::Section::main,8,"Main 2"}};p.normalise();
        std::array<std::set<MusicalFingerprint>,fga::trackCount> phrases;std::set<int> tempos,barCounts;
        for(unsigned seed:{91U,317U,1709U,4951U,9187U,17891U}){
            const auto before=p.settings;const auto sectionsBefore=p.sections.size();const auto barsBefore=(int)(p.length()/4);
            fga::generate(p,fga::Scope::song,0,0,seed);
            check(p.settings.key!=before.key,"generate song chooses a different key every time");
            check(!sameHarmony(p.settings,before),"generate song chooses a different progression, beyond transposition");
            check(p.settings.bassMode!=before.bassMode,"generate song chooses another genre bass figure");
            const auto& profile=fga::genreProfile(genre);
            check(p.settings.genre==genre&&p.settings.bpm>=profile.minBpm&&p.settings.bpm<=profile.maxBpm&&(int)std::lround(p.settings.bpm)!=(int)std::lround(before.bpm),"new song changes tempo inside the genre");
            check(p.sections.size()==sectionsBefore,"song regeneration keeps the section count");
            int bars=0;for(const auto& section:p.sections){check(section.bars%8==0,"regenerated sections stay on 8-bar phrases");bars+=section.bars;}
            check(bars!=barsBefore,"song regeneration changes the form length");
            tempos.insert((int)std::lround(p.settings.bpm));barCounts.insert(bars);
            for(int ti=0;ti<fga::trackCount;++ti)phrases[ti].insert(musicalFingerprint(p.tracks[ti].clips[0]));
            const auto bassSettings=fga::pianoEngineSettings(p,1,p.tracks[1].clips[0].seed,p.tracks[1].clips[0].motifSeed);
            const auto& source=sgk::bassLines()[(size_t)sgk::bassLineIndex((int)bassSettings.bass)].line;
            check(!bassSettings.bassLineOverride.empty(),"regenerated bass has a developed figure");
            for(size_t bar=0;bar<source.size();bar+=16){
                size_t first=bar;while(first<bar+16&&(source[first]=='.'||source[first]=='='||source[first]=='G'))++first;
                if(first<bar+16)check(bassSettings.bassLineOverride[first]==source[first],"bass retains its first pitched anchor in each bar, including offbeats");
            }
            const auto take=p;fga::generate(p);
            check(p.settings.key==take.settings.key&&sameHarmony(p.settings,take.settings)&&p.settings.bpm==take.settings.bpm&&p.length()==take.length(),"apply retains harmony, tempo and form");
            for(int ti=0;ti<fga::trackCount;++ti)check(p.tracks[ti].clips==take.tracks[ti].clips,"new musical takes must be reproducible without a new seed");
        }
        // Consecutive regenerations must differ in strong piano attacks. Changing only
        // key, velocity, duration, swing or humanisation cannot satisfy this check.
        {
            auto q=p;q.sections={{fga::Section::main,8,"Main"}};q.normalise();
            auto attacks=[](const sgk::GroovePattern& pattern){std::set<int> result;for(int i=0;i<32;++i){const char c=pattern.rightHand[(size_t)i%pattern.rightHand.size()];if(c!='.'&&c!='='&&c!='g')result.insert(i);}return result;};
            std::set<int> previous;int sum=0,near=0;
            for(unsigned take=0;take<16;++take){
                fga::generate(q,fga::Scope::track,0,0,7159U*(take+1));const auto& clip=q.tracks[0].clips[0];
                const auto current=attacks(sgk::GrooveEngine::basePattern(fga::pianoEngineSettings(q,0,clip.seed,clip.motifSeed)));
                if(take){std::vector<int> difference;std::set_symmetric_difference(previous.begin(),previous.end(),current.begin(),current.end(),std::back_inserter(difference));sum+=(int)difference.size();if(difference.size()<6)++near;}
                previous=current;
            }
            check(sum>=15*6&&near<=1,"consecutive piano takes need distinct rhythmic figures inside every genre");
        }
        // All scopes replace musical content while leaving other clips and sound assignments alone.
        for(auto scope:{fga::Scope::track,fga::Scope::section,fga::Scope::clip}){
            auto q=p;const auto before=q;
            fga::generate(q,scope,0,0,4517);
            check(q.settings.key==before.settings.key&&sameHarmony(q.settings,before.settings)&&q.settings.bpm==before.settings.bpm&&q.length()==before.length(),"partial regeneration retains harmony, tempo and form");
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
        // Exercise the reported failure with fixed presets/seeds and feel disabled.
        // Compare onset/lane rhythms, so transposition and velocity cannot pass this test.
        for(int ti:{0,1,3}) {
            auto fixed=p;fixed.settings.humanize=0;fixed.settings.swing=0;
            fixed.tracks[(size_t)ti].engine.pattern=0;fixed.tracks[(size_t)ti].engine.riffSeed=777;
            fixed.tracks[(size_t)ti].engine.humanizeSeed=888;
            std::set<std::vector<std::pair<int,int>>> rhythms;
            for(unsigned seed:{91U,317U,1709U,4951U,9187U,17891U}) {
                auto q=fixed;fga::generate(q,fga::Scope::clip,ti,0,seed);
                rhythms.insert(rhythmFingerprint(q.tracks[(size_t)ti].clips[0]));
                check(q.tracks[(size_t)ti].engine.riffSeed==0,"regeneration releases fixed riff seeds");
                for(int track=0;track<fga::trackCount;++track)for(int ci=0;ci<2;++ci)
                    if(track!=ti||ci!=0)check(q.tracks[(size_t)track].clips[(size_t)ci]==fixed.tracks[(size_t)track].clips[(size_t)ci],"fixed-preset regeneration preserves untargeted clips");
                const auto take=q;fga::generate(q,fga::Scope::clip,ti,0);
                check(q.tracks[(size_t)ti].clips[0]==take.tracks[(size_t)ti].clips[0],"apply reproduces fresh automatic patterns");
            }
            std::cout<<"Rhythms "<<genre<<"/"<<ti<<": "<<rhythms.size()<<"\n";
            check(rhythms.size()>=4,"piano bass and guitar need diverse rhythms even with a fixed preset/seed");
        }
        {
            auto q=p;auto& e=q.tracks[0].engine;
            e.pattern=sgk::userPatternIndex();e.userRight.fill('.');e.userLeft.fill('.');
            e.userRight[0]='X';e.userRight[4]='x';e.userLeft[0]='x';e.segmentLocked[0]=true;
            const auto before=e;fga::generate(q,fga::Scope::song,0,0,317);
            check(std::equal(e.userRight.begin(),e.userRight.begin()+8,before.userRight.begin())
                  &&std::equal(e.userLeft.begin(),e.userLeft.begin()+8,before.userLeft.begin()),"regeneration retains locked piano grid segments");
        }
        {
            auto q=p;q.tracks[4].locked=true;const auto before=q;
            fga::generate(q,fga::Scope::song,0,0,555);
            check(q.settings.key!=before.settings.key&&!sameHarmony(q.settings,before.settings),"locked drums do not prevent new song harmony");
            check(q.length()==before.length()&&q.tracks[4].clips==before.tracks[4].clips,"locks freeze the form and retain locked drums");
        }
        for(int lockedTrack:{0,1,2,3}) {
            auto q=p;auto& clip=q.tracks[(size_t)lockedTrack].clips[0];clip.locked=true;
            if(clip.notes.empty())clip.notes.push_back({0,1,60,90,0});const auto before=q;
            fga::generate(q,fga::Scope::song,0,0,555);
            check(q.length()==before.length()&&q.settings.key==before.settings.key&&sameHarmony(q.settings,before.settings),"locked harmonic notes keep the form, key and progression");
            check(q.tracks[(size_t)lockedTrack].clips[0]==before.tracks[(size_t)lockedTrack].clips[0],"locked harmonic clip remains intact");
        }
        std::cout<<"Diversity "<<genre<<":";for(int ti=0;ti<fga::trackCount;++ti)std::cout<<" "<<phrases[ti].size();std::cout<<"\n";
        check(phrases[0].size()>=3,"regeneration must change piano rhythm/pitches, excluding feel and velocity");
        check(phrases[1].size()>=3,"regeneration must develop the selected bass line beyond timing and velocity");
        check(phrases[3].size()>=3,"regeneration must change guitar articulation/rhythm, excluding feel and velocity");
        check(tempos.size()>=3&&barCounts.size()>=2,"repeated song generation varies tempo and length");
    }
}
