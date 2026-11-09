// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace musicalchecks {
using Signature=std::vector<std::pair<int,int>>;
inline Signature signature(const fga::Clip& clip){
    Signature result;int first=-1;
    for(const auto& n:clip.notes){if(first<0)first=n.pitch;result.emplace_back((int)std::lround(n.start*4),n.pitch-first);}
    return result;
}
inline void verify(){
    using namespace fga;
    for(int genre=0;genre<numGenres;++genre){
        auto base=newProject(genre);base.sections={{Section::main,8,"Review"},{Section::drop,8,"Answer"}};
        for(auto& t:base.tracks)t.clips.assign(2,{});
        base.normalise();base.settings.randomAutomation=false;generate(base);
        std::set<std::vector<int>> harmonies;
        for(unsigned seed=1;seed<=24;++seed){
            auto song=base,repeat=base;generate(song,Scope::song,0,0,seed);generate(repeat,Scope::song,0,0,seed);
            check(song.tracks[0].clips==repeat.tracks[0].clips&&song.settings.degrees==repeat.settings.degrees,"all genres reproduce the same musical take from the same starting song and seed");
            check(song.settings.chordCount==1||song.settings.chordCount==2||song.settings.chordCount==4||song.settings.chordCount==8,"automatic song harmonies fit dance phrase lengths");
            std::vector<int> cycle;
            for(int i=0;i<song.settings.chordCount;++i){const auto chord=chordAtBeat(song.settings,i*song.settings.chordSteps*.25);cycle.push_back((chord.rootPc-song.settings.key+12)%12);cycle.push_back(chord.third);cycle.push_back(chord.fifth);}
            harmonies.insert(cycle);
            if(genre==deepHouseGenre||genre==(int)Genre::afroHouse)for(int i=0;i<song.settings.chordCount;++i)
                check(chordAtBeat(song.settings,i*song.settings.chordSteps*.25).fifth==7,"automatic deep/afro progressions avoid unintended structural diminished chords");
        }
        check(harmonies.size()>=6,"every genre offers different harmonic cycles beyond transposition");
        // Measure content rather than velocity/humanisation. Protect the other
        // section and another instrument while repeatedly regenerating one clip.
        for(int track:{0,1,3,leadTrack}){
            auto song=base;std::set<Signature> takes;int repeats=0;Signature previous;
            const auto excluded=song.tracks[(size_t)track].clips[1];const auto other=song.tracks[(size_t)(track==0?1:0)].clips;
            for(unsigned seed=1;seed<=64;++seed){
                generate(song,Scope::clip,track,0,seed);
                const auto current=signature(song.tracks[(size_t)track].clips[0]);
                check(!current.empty(),"every requested harmonic instrument produces a part in every genre");
                if(seed>1&&current==previous)++repeats;
                takes.insert(current);previous=current;
                check(song.tracks[(size_t)track].clips[1]==excluded&&song.tracks[(size_t)(track==0?1:0)].clips==other,"all genres retain excluded notes when regenerating a clip");
            }
            check(takes.size()>=8,"every genre/instrument offers at least eight distinct musical takes");
            if(isElectronicGenre(genre)){
                check(repeats==0,"electronic regeneration avoids immediate repetitions of actual notes");
                if(track==leadTrack)check(takes.size()>=40,"electronic lead varies melodic phrases independently of its rhythm");
            }
            const auto take=song.tracks[(size_t)track].clips[0];generate(song,Scope::clip,track,0,0);
            check(song.tracks[(size_t)track].clips[0].notes==take.notes,"APPLY preserves the selected musical take in every genre");
            song.tracks[(size_t)track].clips[0].locked=true;const auto locked=song.tracks[(size_t)track].clips[0];generate(song,Scope::clip,track,0,999);
            check(song.tracks[(size_t)track].clips[0]==locked,"all genres preserve a locked take");
            std::cout<<"MUSICAL_REVIEW genre="<<genre<<" track="<<track<<" distinct="<<takes.size()<<"/64 immediate_repeats="<<repeats<<'\n';
        }
        // Harmony dice preserve the user's scale, count and authored chord slots.
        auto settings=base.settings;settings.chordCount=4;settings.freeChords[1]=19;settings.manualChords[1]=true;
        for(unsigned seed=1;seed<=32;++seed){randomiseHarmony(settings,seed);check(settings.chordCount==4&&settings.freeChords[1]==19&&settings.manualChords[1],"harmony dice retain explicitly authored chords across every genre");}
    }
    for(int genre:{technoGenre,electroHouseGenre}){
        auto p=newProject(genre);p.sections={{Section::main,8,"Grid"}};for(auto& t:p.tracks)t.clips.assign(1,{});p.normalise();
        p.settings.chordCount=4;p.settings.scale=0;p.settings.degrees={0,3,4,5,0,3,4,5};p.settings.freeChords.fill(-1);
        for(auto& t:p.tracks){t.engine.density=1;t.engine.humanize=0;t.engine.swing=0;}
        for(auto role:{Section::main,Section::buildup,Section::drop})for(int track:{0,1,3,leadTrack}){
            Clip complete,preview;complete.seed=complete.motifSeed=preview.seed=preview.motifSeed=31;
            generateElectronicClip(p,complete,track,0,32,role,true);
            generateElectronicClip(p,preview,track,0,32,role,true,16);
            complete.notes.erase(std::remove_if(complete.notes.begin(),complete.notes.end(),[](const Note& n){return n.start>=16;}),complete.notes.end());
            check(complete.notes==preview.notes,"anti-repeat previews preserve the real section's build timing and phrase endings");
        }
        for(int key=0;key<12;++key)for(int steps:{8,16,32})for(double begin:{0.,1.25}){
            p.settings.key=key;p.settings.chordSteps=steps;
            Clip clip;clip.seed=clip.motifSeed=31;generateElectronicClip(p,clip,3,begin,16,Section::main,true);
            check(!clip.notes.empty(),"guitar chord-alignment fixture is audible");
            for(const auto& note:clip.notes){const auto chord=chordAtBeat(p.settings,begin+note.start);const int iv=(note.pitch-chord.rootPc+120)%12;
                check(iv==0||iv==chord.fifth,"electronic accompaniment follows the chord at its attack, including half-bar changes and offset clips");}
        }
        p.settings.scale=5;p.settings.chordSteps=16;p.settings.chordCount=2;p.settings.degrees={0,1,0,1,0,1,0,1};p.settings.extension=0;
        for(unsigned seed=1;seed<=32;++seed){Clip clip;clip.seed=clip.motifSeed=seed;generateElectronicClip(p,clip,0,.75,16,Section::drop,false);
            for(const auto& note:clip.notes){const auto chord=chordAtBeat(p.settings,.75+note.start);const int iv=(note.pitch-chord.rootPc+120)%12;
                check(iv==0||iv==chord.third||iv==chord.fifth,"electronic stabs never introduce an unauthored suspension against the lead");
                const double boundary=(std::floor((.75+note.start)/4)+1)*4;
                check(.75+note.start+note.length<=boundary+1.e-8,"sustained electronic harmony stops at the next changed chord");}
        }
    }
}
}
