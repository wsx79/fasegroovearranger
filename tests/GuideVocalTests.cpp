// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/GuideVocalRender.h"
#include "../Source/ProjectIO.h"
#include <stdexcept>
#include <iostream>
void runGuideVocalTests(){
    using namespace fga;
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Project p;p.sections={{Section::intro,16,"DJ intro"},{Section::main,8,"Verse"},{Section::outro,16,"DJ outro"}};p.normalise();p.settings.bpm=125;
    const int t=p.addMidiTrack();auto& c=p.tracks[(size_t)t].clips[0];c.span=160;c.notes={{0,1,60,90,0},{64,1,60,90,0},{66,2,64,100,0},{96,1,60,90,0}};enableVocalClip(c,160);
    for(const auto& n:c.notes)c.vocal->syllables.push_back({c.vocal->nextSyllableId++,"hello","",{n.vocalId}});
    GuideVocalPlan plan;check(guideVocalPlan(p,t,0,plan).wasOk()&&plan.start==64&&std::abs(plan.duration-1.92)<1e-6&&plan.audible.size()==2,"guide region and intro/outro exclusion");
    auto overlap=p;overlap.tracks[(size_t)t].clips[0].notes[2].start=64.5;check(guideVocalPlan(overlap,t,0,plan).failed(),"guide overlapping notes rejected");
    auto override=p;override.tracks[(size_t)t].clips[0].vocal->allowIntroOutro=true;
    check(guideVocalPlan(override,t,0,plan).wasOk()&&plan.start==0&&plan.audible.size()==4,"explicit intro/outro singing override");
    auto eight=p;eight.tracks[(size_t)t].clips[0].notes[2].length=30;
    check(guideVocalPlan(eight,t,0,plan).wasOk()&&std::abs(plan.duration-15.36)<1e-6,"eight bars at 125 BPM");
    check(guideVocalPlan(p,t,0,plan).wasOk(),"restore test plan");
    GuideVocalPlan plan2;auto unsafe=p;unsafe.tracks[(size_t)t].clips[0].vocal->syllables[1].text="[[phonemes]]";
    check(guideVocalPlan(unsafe,t,0,plan2).failed(),"raw phoneme/markup input rejected");
    std::cout<<"PASS guide MIDI timing, regions, intro/outro override and invalid input\n";
    AudioTake take;take.start=17;bool progress=false,cancel=false;
    const auto before=juce::JSON::toString(projectJson(p));
    check(renderGuideVocal(p,t,0,take,[&]{cancel=true;return false;},[&](double){progress=true;}).failed(),"removed backend must fail closed");
    check(!progress&&!cancel&&take.start==17&&take.file==juce::File()&&juce::JSON::toString(projectJson(p))==before,"dormant rendering has no callbacks, output or project mutations");
    std::cout<<"PASS dormant Vocal Studio planning and fail-closed render boundary; no synthesis backend\n";
}
