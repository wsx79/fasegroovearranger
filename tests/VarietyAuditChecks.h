// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Variety audit: every "new take" command must draw a different result from a different
// seed — songs, harmony, each instrument, sections, automation lanes and vocal chops.
namespace varietyaudit {
inline std::vector<int> shape(const fga::VolumeAutomation& lane){std::vector<int> v;for(const auto& p:lane.points){v.push_back((int)std::lround(p.beat*4));v.push_back((int)std::lround(p.value*50));}return v;}
// Sustained parts (strings) also differ by how long each chord is held, in half beats.
inline std::vector<int> notes(const fga::Clip& c,bool sustained=false){std::vector<int> v;for(const auto& n:c.notes){if(n.length<1./64.)continue;v.push_back((int)std::lround(n.start*4));v.push_back(n.pitch);if(sustained)v.push_back((int)std::lround(n.length*2));}return v;}
inline void verify(){
    int weak=0;const auto check=[&weak](bool ok,const char* message){if(!ok){++weak;std::cout<<"WEAK "<<message<<std::endl;}};
    using namespace fga;const unsigned takes=40;
    for(int genre=0;genre<numGenres;++genre){
        std::set<std::vector<int>> harmony,forms,rolled;std::set<int> tempos,keys;
        auto song=newProject(genre);
        for(unsigned seed=1;seed<=takes;++seed){generate(song,Scope::song,0,0,seed*7919U+11U);const auto& s=song.settings;
            std::vector<int> h{s.key,s.scale,s.chordCount,s.chordSteps};for(int d:s.degrees)h.push_back(d);for(int f:s.freeChords)h.push_back(f);harmony.insert(h);keys.insert(s.key);tempos.insert((int)std::lround(s.bpm));
            std::vector<int> f;for(const auto& sec:song.sections){f.push_back((int)sec.type);f.push_back(sec.bars);}forms.insert(f);}
        auto chords=newProject(genre);
        for(unsigned seed=1;seed<=takes;++seed){randomiseHarmony(chords,seed*104729U+3U);const auto& s=chords.settings;std::vector<int> h{s.scale,s.chordCount,s.chordSteps};for(int d:s.degrees)h.push_back(d);for(int f:s.freeChords)h.push_back(f);rolled.insert(h);}
        std::cout<<"VARIETY genre="<<genre<<" song_harmony="<<harmony.size()<<" keys="<<keys.size()<<" tempos="<<tempos.size()<<" forms="<<forms.size()<<" random_chords="<<rolled.size()<<"/"<<takes<<'\n';
        check(harmony.size()>=takes/2&&keys.size()>=6,"new songs draw different keys and progressions");
        check(rolled.size()>=takes/4,"RANDOM CHORDS draws different progressions");
        auto p=newProject(genre);p.settings.randomAutomation=false;p.settings.stringsMode=1;p.sections={{Section::main,8,"A"},{Section::drop,8,"B"}};p.normalise();generate(p);
        std::cout<<"VARIETY genre="<<genre<<" clip_takes:";
        for(int track=0;track<legacyTrackCount;++track){std::set<std::vector<int>> takesSeen;size_t audible=0;
            for(unsigned seed=1;seed<=takes;++seed){generate(p,Scope::clip,track,0,seed*31337U+5U);const auto v=notes(p.tracks[(size_t)track].clips[0],track==2);audible=std::max(audible,v.size()/(track==2?3:2));takesSeen.insert(v);}
            std::cout<<' '<<track<<'='<<takesSeen.size();
            // The kick keeps the floor and very sparse percussion has little to vary: they only
            // need more than one take. Every other part must clearly change.
            if(audible>=4)check(takesSeen.size()>=(track==4?2u:isDrumTrack(track)?3u:takes/4),"every instrument's new take differs with the seed");}
        std::set<std::vector<int>> sections;for(unsigned seed=1;seed<=takes;++seed){generate(p,Scope::section,0,1,seed*65537U+9U);std::vector<int> all;for(int track:{0,1,3,leadTrack}){const auto v=notes(p.tracks[(size_t)track].clips[1]);all.insert(all.end(),v.begin(),v.end());all.push_back(-1);}sections.insert(all);}
        std::cout<<" section="<<sections.size()<<'\n';check(sections.size()>=takes*3/4,"section regeneration differs with the seed");
    }
    {auto p=newProject(1);p.sections={{Section::intro,8,"Intro"},{Section::main,16,"Main"},{Section::buildup,8,"Build"},{Section::drop,16,"Drop"},{Section::breakdown,8,"Break"},{Section::outro,8,"Outro"}};p.normalise();
        ParameterAutomation lane;lane.name="Cutoff";p.tracks[3].parameters.push_back(lane);
        std::set<std::vector<int>> volume,pan,plugin,songPlugin,mains;std::set<int> figures;
        for(unsigned seed=1;seed<=takes;++seed){auto a=p;randomiseAutomation(a,Scope::track,2,0,seed*2654435761U,0);volume.insert(shape(a.tracks[2].volume));
            a=p;randomiseAutomation(a,Scope::track,3,0,seed*2654435761U,1);pan.insert(shape(a.tracks[3].panAutomation));
            a=p;randomiseAutomation(a,Scope::track,3,0,seed*2654435761U,2);const auto& curve=a.tracks[3].parameters[0].curve;plugin.insert(shape(curve));check(curve.moves(),"a plugin lane's own dice always writes movement");
            // The main section alone: the figure must change in kind, not only in amount.
            std::vector<int> main;int count=0;for(const auto& point:curve.points)if(point.beat>=32&&point.beat<96){main.push_back((int)std::lround(point.beat));++count;}mains.insert(main);figures.insert(count);
            a=p;generate(a,Scope::song,0,0,seed*40503U+1U);songPlugin.insert(shape(a.tracks[3].parameters[0].curve));}
        std::cout<<"VARIETY automation volume="<<volume.size()<<" pan="<<pan.size()<<" plugin="<<plugin.size()<<" plugin_main_rhythms="<<mains.size()<<" point_counts="<<figures.size()<<" song_plugin="<<songPlugin.size()<<"/"<<takes<<'\n';
        check(volume.size()>=takes*3/4&&pan.size()>=takes*3/4,"volume and pan dice differ with the seed");
        check(plugin.size()==takes&&mains.size()>=10&&figures.size()>=5,"plugin parameter dice draw different figures, not one curve with other amounts");
        check(songPlugin.size()>=takes*3/4,"song regeneration moves plugin lanes differently each time");
    }
    {auto p=newProject(4);p.sections={{Section::main,16,"Main"},{Section::buildup,8,"Build"}};p.normalise();const int track=p.addVocalChopsTrack();std::set<std::vector<int>> chops;
        for(unsigned seed=1;seed<=takes;++seed){generate(p,Scope::track,track,0,seed*977U+3U);chops.insert(notes(p.tracks[(size_t)track].clips[0]));}
        std::cout<<"VARIETY vocal_chops="<<chops.size()<<"/"<<takes<<'\n';check(chops.size()>=takes/2,"vocal chops regeneration differs with the seed");}
    ::check(weak==0,"variety audit found repeated results");
}
}
