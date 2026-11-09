// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
inline void verifyHarmonyAudit(){
    using namespace fga;
    auto project=newProject(electroHouseGenre);
    bool borrowed=false;for(unsigned seed=1;seed<=256;++seed){auto p=project;p.settings.genre=0;p.settings.scale=0;p.settings.chordCount=4;p.settings.freeChords.fill(-1);randomiseHarmony(p,seed);for(int chord:p.settings.freeChords)borrowed=borrowed||chord>=0;}
    check(borrowed,"Random Chords retains chromatic preset roots and chord types");
    auto manual=project;manual.settings.genre=0;manual.settings.scale=0;manual.settings.freeChords[1]=19;manual.settings.manualChords[1]=true;randomiseHarmony(manual,29);check(manual.settings.freeChords[1]==19,"manual chromatic slot survives harmony dice");
    for(int origin=0;origin<2;++origin){
        auto draws=newProject(0);draws.settings.scale=0;draws.settings.freeChords.fill(-1);draws.settings.manualChords.fill(false);
        if(origin){const auto& library=sgk::progressionPresets();for(size_t i=0;i<library.size();++i)if(library[i].scale==0){auto slots=sgk::presetSlots(library[i],draws.settings.extension);if(std::any_of(slots.begin(),slots.end(),[](const auto& slot){return slot.freeCode>=0;})){chooseProgression(draws,(int)i);break;}}}
        std::set<std::pair<std::array<int,8>,std::array<int,8>>> late;
        for(unsigned seed=1;seed<=120;++seed){randomiseHarmony(draws,seed);if(seed>20)late.insert({draws.settings.degrees,draws.settings.freeChords});check(std::none_of(draws.settings.manualChords.begin(),draws.settings.manualChords.end(),[](bool flag){return flag;}),"generated chromatic chords never acquire manual ownership");}
        check(late.size()>10,"repeated Random Chords remains live after chromatic draws and preset selection");
    }
    for(int genre:{0,technoGenre,electroHouseGenre}){auto chosen=manual;chosen.settings.genre=genre;for(unsigned seed=1;seed<=20;++seed){randomiseHarmony(chosen,seed);check(chosen.settings.manualChords[1]&&chosen.settings.freeChords[1]==19,"repeated harmony dice preserve explicit user ownership");}}
    std::set<std::array<int,8>> progressions;std::set<int> speeds,modes,counts;std::array<int,13> distribution{};
    for(unsigned seed=0;seed<512;++seed){
        Settings s;s.genre=electroHouseGenre;applyElectronicHarmony(s,seed);check(s.scale==2&&s.chordCount==4,"Electro progressions use natural minor");
        for(int i=0;i<s.chordCount;++i)check(chordAtBeat(s,i*s.chordSteps*.25).fifth==7,"Electro curated degrees do not introduce diminished chords");
        progressions.insert(s.degrees);speeds.insert(s.chordSteps);
        auto repeated=s;applyElectronicHarmony(repeated,seed);check(repeated.degrees==s.degrees&&repeated.chordSteps==s.chordSteps,"electronic harmony is deterministic");
        s.genre=technoGenre;applyElectronicHarmony(s,seed);modes.insert(s.scale);counts.insert(s.chordCount);check(s.voicing==0,"electronic default does not advertise an unused rootless voicing");++distribution[(size_t)s.scale];
        check(s.degrees[0]==0&&(s.chordSteps==32||s.chordSteps==64),"Techno anchors its drone/vamp to the tonic");
        for(int i=0;i<s.chordCount;++i)check(chordAtBeat(s,i*s.chordSteps*.25).fifth==7,"Techno vamps avoid unintended diminished chords");
    }
    check(progressions.size()>=16&&speeds==std::set<int>{16,32},"Electro has a wider harmonic vocabulary and one/two bars per chord");
    check(modes==std::set<int>{1,2,5}&&counts==std::set<int>{1,2},"Techno varies mode and drone/vamp");
    check(distribution[1]>150&&distribution[2]>120&&distribution[5]>80,"Techno mode weighting retains Dorian, Aeolian and Phrygian vocabulary");
    for(int genre:{technoGenre,electroHouseGenre})for(unsigned seed=1;seed<=16;++seed){
        auto p=project;p.settings.seed=seed;applyGenre(p,genre);auto expected=p.settings;applyElectronicHarmony(expected,seed);
        check(p.settings.scale==expected.scale&&p.settings.degrees==expected.degrees&&p.settings.chordSteps==expected.chordSteps,"genre selection passes its seed to electronic harmony");
        randomiseHarmony(p,seed+1);check(p.settings.degrees[0]==0||genre==electroHouseGenre,"electronic Random Chords keeps Techno tonic");check(genre!=electroHouseGenre||p.settings.scale==2,"electronic harmony dice keep Electro Aeolian");
    }
    applyGenre(manual,(int)Genre::ukGarage);check(sgk::scaleIsMinor(manual.settings.scale),"UK Garage starts with minor harmony");
    project.settings.key=0;project.settings.scale=2;project.settings.chordSteps=16;project.settings.chordCount=4;project.settings.degrees={0,5,2,6,0,5,2,6};project.settings.freeChords.fill(-1);project.settings.humanize=0;project.settings.swing=0;
    for(int track:{0,leadTrack}){auto& e=project.tracks[(size_t)track].engine;e.humanize=0;e.swing=0;e.density=1;}
    Clip chords;chords.seed=chords.motifSeed=1;generateElectronicClip(project,chords,0,0,32,Section::main,false);
    check(!chords.notes.empty()&&std::any_of(chords.notes.begin(),chords.notes.end(),[](const Note& n){return std::fmod(n.start,1.)==.5;}),"Electro chord stabs retain offbeat drive");
    std::array<int,4> previous{};bool first=true;
    for(size_t i=0;i<chords.notes.size();i+=4){check(i+3<chords.notes.size(),"every Electro stab has four voices");std::set<int> actual;const auto chord=chordAtBeat(project.settings,chords.notes[i].start);std::set<int> expected{chord.rootPc,(chord.rootPc+chord.third)%12,(chord.rootPc+chord.fifth)%12};
        for(size_t voice=0;voice<4;++voice){const auto& note=chords.notes[i+voice];check(note.start==chords.notes[i].start&&note.pitch>=55&&note.pitch<=72,"Electro chord voices stay together in their intended register");actual.insert(note.pitch%12);if(!first)check(std::abs(note.pitch-previous[voice])<=7,"Electro inversions keep voice movement bounded");previous[voice]=note.pitch;}
        check(actual==expected,"Electro stabs include root, third and fifth");first=false;
    }
    Clip lead;lead.seed=lead.motifSeed=1;generateElectronicClip(project,lead,leadTrack,0,32,Section::drop,false);int last=67;bool colour=false;
    for(const auto& note:lead.notes){check(note.pitch>=60&&note.pitch<=84,"electronic lead stays in its intended register");for(int shift:{-12,12})if(note.pitch+shift>=60&&note.pitch+shift<=84)check(std::abs(note.pitch-last)<=std::abs(note.pitch+shift-last),"lead uses the nearest available octave");const auto chord=chordAtBeat(project.settings,note.start);const int interval=(note.pitch-chord.rootPc+120)%12;colour=colour||(interval!=0&&interval!=chord.third&&interval!=chord.fifth);last=note.pitch;}
    check(colour,"Electro lead includes weak-beat seventh/ninth colour");
    project.settings.genre=technoGenre;project.tracks[0].parameters.clear();addElectronicToneAutomation(project);
    check(project.tracks[0].parameters.size()==1&&project.tracks[0].parameters[0].curve.active(),"Techno receives a native tone build envelope");
    project.tracks[0].parameters[0].generatedTone=false;project.tracks[0].parameters[0].curve.points={{0,.73,0}};addElectronicToneAutomation(project);check(project.tracks[0].parameters[0].curve.points.size()==1&&project.tracks[0].parameters[0].curve.valueAt(12)==.73,"existing tone automation is not overwritten");
    project.tracks[0].parameters.clear();project.tracks[0].locked=true;addElectronicToneAutomation(project);check(project.tracks[0].parameters.empty(),"locked Techno track does not acquire automation");project.tracks[0].locked=false;
    {
        auto automatic=newProject(technoGenre);automatic.settings.randomAutomation=false;automatic.sections={{Section::main,8,"Main"},{Section::buildup,4,"Build"},{Section::drop,8,"Drop"}};
        for(auto& t:automatic.tracks){t.freeTimeline=false;t.parameters.clear();t.clips.assign(3,{});}automatic.normalise();addElectronicToneAutomation(automatic);
        auto owned=[](Project& p)->ParameterAutomation&{for(auto& lane:p.tracks[0].parameters)if(lane.generatedTone)return lane;throw std::runtime_error("missing generated Tone");};
        check(owned(automatic).curve.valueAt(32)==.2,"owned Tone starts at build");automatic.resizeGuide(0,12);generate(automatic,Scope::song,0,0,123);
        const auto build=std::find_if(automatic.sections.begin(),automatic.sections.end(),[](const SongSection& section){return section.type==Section::buildup;});check(build!=automatic.sections.end(),"test build remains present");const double start=automatic.sectionStart((int)(build-automatic.sections.begin()));
        check(std::abs(owned(automatic).curve.valueAt(start)-.2)<1.e-9,"owned Tone follows regenerated form after guide edits with random automation off");
        auto protectedLane=automatic;owned(protectedLane).curve.locked=true;const auto protectedValue=owned(protectedLane).curve.valueAt(start);addElectronicToneAutomation(protectedLane);check(owned(protectedLane).curve.valueAt(start)==protectedValue,"locked generated envelope is protected");
        auto trackLocked=automatic;trackLocked.tracks[0].locked=true;applyGenre(trackLocked,0);check(owned(trackLocked).generatedTone,"genre change preserves automation on a locked track");
        applyGenre(automatic,0);check(std::none_of(automatic.tracks[0].parameters.begin(),automatic.tracks[0].parameters.end(),[](const ParameterAutomation& lane){return lane.generatedTone;}),"leaving Techno removes only owned Tone automation");
        for(int genre:{technoGenre,electroHouseGenre})for(unsigned seed=1;seed<=8;++seed){auto song=newProject(genre);song.sections={{Section::main,4,"Main"}};for(auto& t:song.tracks)t.clips.assign(1,{});song.normalise();generate(song,Scope::song,0,0,seed);check(song.settings.voicing==0&&song.settings.extension==0,"new-song feel cannot restore unused electronic rootless voicing");}
    }
    project.sections={{Section::main,32,"Main"},{Section::main,32,"Main 2"}};project.normalise();for(auto& t:project.tracks){t.engine.pedalCC=false;for(auto& c:t.clips)c.notes.clear();}project.tracks[0].engine.pedalCC=true;for(auto& clip:project.tracks[0].clips)clip.notes={{0,.25,60,100,0}};
    int expressions=0;for(const auto& event:events(project,0))if(event.controller==11)++expressions;check(expressions==1,"constant expression emits one CC11 across adjacent clips");
    project.sections[1].type=Section::buildup;expressions=0;int previousCC=-1;for(const auto& event:events(project,0))if(event.controller==11){check(event.velocity!=previousCC,"expression events only contain actual value changes");previousCC=event.velocity;++expressions;}
    check(expressions>2&&expressions<=70&&previousCC==127,"build expression ramp and final reset survive CC11 compression");
    project.sections[0].type=Section::buildup;int boundaryEvents=0;for(const auto& event:events(project,0))if(event.controller==11&&event.beat==128){++boundaryEvents;check(event.velocity==60,"next build starts at its requested expression");}check(boundaryEvents==1,"adjacent builds do not emit conflicting expression values at the same tick");
}
