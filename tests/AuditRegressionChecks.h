// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
inline void verifyAuditRegressions(){
    using namespace fga;
    {
        auto p=newProject(0);p.sections={{Section::main,4,"A"},{Section::main,4,"B"}};p.normalise();
        for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();
        auto& t=p.tracks[0];t.freeTimeline=true;t.clips.clear();Clip crossing;crossing.span=24;crossing.locked=true;
        crossing.notes={{4,1,60,100,0},{15,4,67,90,0},{20,1,64,100,0}};
        check(p.placeClip(0,crossing,0)>=0&&p.duplicateSection(0),"duplicate section across independent clip");
        auto contains=[&](double at,int pitch){for(auto e:events(p,0))if(e.controller<0&&e.velocity>0&&e.pitch==pitch&&std::abs(e.beat-at)<1e-8)return true;return false;};
        check(contains(4,60)&&contains(20,60)&&contains(36,64),"section duplication preserves original tail and inserts copied notes");
        check(contains(15,67)&&contains(31,67)&&contains(32,67),"crossing sustained notes survive insertion as clipped segments");
        for(auto& c:p.tracks[0].clips)check(c.locked,"insertion preserves clip lock metadata");
    }
    for(int steps:{8,16,32,64})for(double split:{.5,2.,3.25,5.,10.5,17.25}){
        auto p=newProject(0);p.sections={{Section::main,8,"Main"}};p.normalise();
        p.settings.key=0;p.settings.scale=0;p.settings.extension=0;p.settings.bassMode=1;p.settings.chordCount=2;p.settings.chordSteps=steps;
        p.settings.degrees={0,3,0,0,0,0,0,0};p.settings.freeChords.fill(-1);
        auto& e=p.tracks[1].engine;e.pattern=sgk::userPatternIndex();e.userLeft.fill('.');e.userRight.fill('.');
        for(int i=0;i<32;i+=4)e.userLeft[(size_t)i]='x';e.humanize=0;e.swing=0;e.fillEvery=0;e.variationEvery=0;e.realism=false;
        generate(p,Scope::track,1);const auto untouched=p.tracks[0].clips;
        const int tail=p.splitClip(1,0,split);check(tail>=0,"fractional bass clip split");generate(p,Scope::clip,1,tail);
        const auto& c=p.tracks[1].clips[(size_t)tail];check(!c.notes.empty(),"regenerated bass window stays audible");
        for(const auto& n:c.notes){const double absolute=p.clipStart(1,tail)+n.start;const int slot=(int)std::floor((absolute+1e-7)/(steps*.25))%2;
            check(n.pitch%12==(slot?5:0),"regenerated split clip keeps the absolute chord phase");check(n.start>=0&&n.start+n.length<=p.clipLength(1,tail)+1e-7,"cropped notes stay within the clip");}
        check(p.tracks[0].clips==untouched,"clip regeneration leaves other tracks untouched");
    }
    for(int genre:{technoGenre,electroHouseGenre}){
        auto p=newProject(genre);p.sections={{Section::main,4,"Main"}};p.normalise();generate(p,Scope::track,leadTrack);
        const auto baseline=p.tracks[leadTrack].clips[0].notes;check(!baseline.empty(),"electronic lead octave fixture");
        for(int octave:{-2,-1,1,2}){p.tracks[leadTrack].engine.globalOctave=octave;generate(p,Scope::track,leadTrack);auto expected=baseline;
            for(auto& n:expected)n.pitch+=12*octave;check(p.tracks[leadTrack].clips[0].notes==expected,"electronic lead octave transposes without altering rhythm or dynamics");}
    }
}
