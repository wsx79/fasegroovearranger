// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace arrangementchecks {
inline std::vector<int> roles(const fga::Project& p){
    std::vector<int> result;for(const auto& s:p.sections)result.push_back((int)s.type);return result;
}
inline void verify(const std::string& folder={}){
    using namespace fga;
    for(int genre=0;genre<numGenres;++genre){
        auto base=newProject(genre);base.settings.randomAutomation=false;
        std::set<std::vector<int>> forms;
        for(unsigned seed=1;seed<=24;++seed){
            auto p=base;generate(p,Scope::song,0,0,seed);forms.insert(roles(p));
            verifyClubProject(p,genre,seed==1?folder:"");
            check(p.sections.front().type==Section::intro&&p.sections.back().type==Section::outro,"every automatic song has real DJ bookends");
            for(size_t si=0;si<p.sections.size();++si){const auto& s=p.sections[si];
                if(s.type==Section::intro||s.type==Section::outro)check(s.bars==16||s.bars==32,"DJ passages retain 16/32-bar lengths independently of harmony");
                if(s.type==Section::breakdown){
                    size_t next=si+1;while(next<p.sections.size()&&p.sections[next].type!=Section::drop)++next;
                    check(next<p.sections.size(),"every automatic breakdown leads to a musical return");
                    auto activity=[&](size_t at){double sum=0;for(const auto& t:p.tracks)for(const auto& n:t.clips[at].notes)sum+=n.velocity*n.velocity;return sum/p.sections[at].bars;};
                    // A MIDI activity proxy, independent of section duration;
                    // the native rendering audit separately measures audio RMS.
                    check(activity(next)>activity(si),"the return restores MIDI activity after the breakdown in every genre");
                }
                if(s.type!=Section::buildup||!genreProfile(genre).rollingBuild||si+1==p.sections.size()||p.sections[si+1].type!=Section::drop)continue;
                for(int t:{1,4})for(const auto& n:p.tracks[(size_t)t].clips[si].notes)
                    check(n.start+n.length<=s.bars*4.-1.+1e-8,"rolling builds leave one beat of kick/bass breathing space before the return");
            }
            if(seed==1){
                auto repeat=base;generate(repeat,Scope::song,0,0,seed);
                check(roles(repeat)==roles(p)&&repeat.sections.size()==p.sections.size(),"arrangement choice is deterministic from the same starting state and seed");
                for(size_t s=0;s<p.sections.size();++s)check(repeat.sections[s].bars==p.sections[s].bars,"arrangement lengths are deterministic");
                for(size_t t=0;t<p.tracks.size();++t)check(repeat.tracks[t].clips==p.tracks[t].clips,"arrangement MIDI is deterministic");
                auto applied=p;generate(applied);check(roles(applied)==roles(p),"Apply preserves the arrangement");
                for(size_t t=0;t<p.tracks.size();++t)check(applied.tracks[t].clips==p.tracks[t].clips,"Apply reproduces the stored arrangement MIDI exactly");
                const auto prior=roles(p);generate(p,Scope::song,0,0,seed+100);check(roles(p)!=prior,"successive full songs choose different section-role sequences");
            }
        }
        check(forms.size()>=3,"every genre varies section roles, beyond changing their duration");
        auto layout=base;check(rearrangeSong(layout,91),"factory rearrangement succeeds");
        check(layout.sections.front().bars==base.sections.front().bars&&layout.sections.back().bars==base.sections.back().bars,"layout-only editing preserves the source mix runway lengths");
        verifyClubProject(layout,genre,"");
        for(int t:{0,1,2,3,leadTrack}){
            check(layout.tracks[(size_t)t].clips.front().notes==base.tracks[(size_t)t].clips.front().notes,"rearranging reuses the actual intro MIDI");
            check(layout.tracks[(size_t)t].clips.back().notes==base.tracks[(size_t)t].clips.back().notes,"rearranging reuses the actual outro MIDI");
        }
        // Equal source seeds isolate orchestration from random riff choice.
        auto stages=base;stages.sections={{Section::main,16,"First"},{Section::breakdown,8,"Space"},{Section::buildup,8,"Lift"},{Section::drop,16,"Return"},{Section::main,16,"Reprise"},{Section::drop,16,"Final"}};stages.normalise();
        for(auto& t:stages.tracks)for(auto& c:t.clips){c.seed=42;c.motifSeed=0;}
        generate(stages);
        for(int t:{10,12}){
            for(const auto& n:stages.tracks[(size_t)t].clips.front().notes)check(n.start>=24,"opening body phrase leaves space before auxiliary percussion enters");
        }
        for(const auto& n:stages.tracks[(size_t)leadTrack].clips.front().notes){
            const int bar=(int)(n.start/4)%8;
            check(isElectronicGenre(genre)?bar<4||bar>=6:bar>=4,"lead leaves a real compositional response window");
        }
        // User-built section identities and local progressions must survive.
        auto custom=base;custom.sections[1].name="My verse";const auto original=roles(custom);generate(custom,Scope::song,0,0,314);
        check(roles(custom)==original&&custom.sections[1].name=="My verse","song generation preserves authored forms");
        auto local=base;local.sections[1].harmony.enabled=true;local.sections[1].harmony.key=6;const auto localRoles=roles(local);generate(local,Scope::song,0,0,314);
        check(roles(local)==localRoles&&local.sections[1].harmony.enabled&&local.sections[1].harmony.key==6,"alternative forms never relocate authored section harmony");
        auto scoped=base;generate(scoped,Scope::section,0,1,314);check(roles(scoped)==roles(base)&&scoped.length()==base.length(),"section regeneration preserves the complete form");
        std::cout<<"ARRANGEMENT_REVIEW genre="<<genre<<" distinct_roles="<<forms.size()<<"/24 DJ_runways=PASS development=PASS\n";
    }
    for(int chords:{3,5,7}){
        auto p=newProject(1);p.settings.chordCount=chords;p.settings.chordSteps=64;p.tracks[0].engine.grooveBars=3;
        arrangeSong(p);generate(p);check(rearrangeSong(p,42),"odd harmonic cycles can be rearranged");
        check(p.sections.front().bars==32&&p.sections.back().bars==32,"long odd harmonic cycles cannot inflate the DJ bookends");
        for(const auto& s:p.sections)check(s.bars%8==0,"long harmonic cycles continue across an intact eight-bar arrangement grid");
    }
}
}
