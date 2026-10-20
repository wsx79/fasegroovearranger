// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Project.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
#include "ClubArrangementChecks.h"
int main(int argc,char** argv){try{
    verifyClubEdgeCases();verifyClubSeedCases();
    {auto p=fga::newProject(1);p.sections={{fga::Section::main,4,"Main"}};p.normalise();auto& e=p.tracks[0].engine;e.pattern=sgk::userPatternIndex();e.userRight.fill('.');e.userLeft.fill('.');e.userRight[0]='x';e.userVelRight[0]=37;e.realism=false;e.humanize=0;e.swing=0;e.strum=0;e.fillEvery=0;e.variationEvery=0;fga::generate(p,fga::Scope::track,0);check(!p.tracks[0].clips[0].notes.empty(),"custom piano grid generates notes");auto before=p.tracks[0].clips[0].notes;e.userVelRight[0]=120;fga::generate(p,fga::Scope::track,0);check(p.tracks[0].clips[0].notes[0].velocity>before[0].velocity,"per-step piano velocity reaches engine");e.inversions[0]=2;fga::generate(p,fga::Scope::track,0);check(p.tracks[0].clips[0].notes!=before,"per-chord inversion reaches engine");e.pedalCC=true;auto events=fga::events(p,0);bool pedal=false,expression=false;for(auto event:events){pedal=pedal||event.controller==64;expression=expression||event.controller==11;}check(pedal&&expression,"pedal and expression controller events");auto original=p;fga::applyPianoIntention(p,0,1,true,true,123);check(p.settings.degrees==original.settings.degrees&&p.tracks[0].engine.userRight==original.tracks[0].engine.userRight,"DNA rhythm and harmony locks");}
    {auto p=fga::newProject(1);auto& e=p.tracks[0].engine;e.pattern=-1;e.riffSeed=123;auto a=fga::pianoEngineSettings(p,0,55);auto patternA=sgk::GrooveEngine::basePattern(a);e.riffSeed=9876;auto b=fga::pianoEngineSettings(p,0,55);auto patternB=sgk::GrooveEngine::basePattern(b);check(patternA.rightHand!=patternB.rightHand||patternA.leftHand!=patternB.leftHand,"riff seed changes the automatic genre groove");check(a.humanizeSeed==b.humanizeSeed,"riff and humanize seeds are independent");}
    for(int genre=0;genre<13;++genre){
        auto p=fga::newProject(genre);auto original=p;verifyClubProject(p,genre,argc>1?argv[1]:"");
        {auto form=p;form.settings.songMinutes=20;fga::arrangeSong(form);check(form.length()==p.length(),"legacy minutes do not constrain musical form");auto slow=p;slow.settings.bpm=80;fga::arrangeSong(slow);check(slow.length()==p.length(),"tempo does not stretch a musical form to minutes");auto developing=p;developing.tracks[0].engine.grooveBars=4;fga::arrangeSong(developing);check(developing.length()>p.length(),"longer groove develops over more complete phrases");}
        if(genre==1){auto sparse=p,busy=p;sparse.tracks[0].engine.density=0;busy.tracks[0].engine.density=1;fga::arrangeSong(sparse);fga::arrangeSong(busy);check(sparse.length()>busy.length(),"sparse and busy grooves have different phrase development");auto lead=p;lead.settings.guitarLead=true;fga::generate(lead,fga::Scope::track,0);check(lead.tracks[0].clips!=p.tracks[0].clips,"original guitar lead piano answers");for(int ti=1;ti<17;++ti)check(lead.tracks[(size_t)ti].clips==p.tracks[(size_t)ti].clips,"lead piano generation isolation");auto shifted=p;shifted.tracks[4].engine.midiNote=50;fga::generate(shifted,fga::Scope::track,4);for(const auto& c:shifted.tracks[4].clips)for(auto n:c.notes)check(n.pitch==50,"custom drum MIDI mapping");}
        check(p.tracks.size()==17,"17 instrument tracks");check(p.sections.size()>=6,"genre-specific song stages");
        check(p.length()*60/p.settings.bpm>=240,"full genre form includes intro and outro");
        check(p.length()*60/p.settings.bpm<480,"genre forms remain musically bounded");
        check(p.sections.front().type==fga::Section::intro&&p.sections.back().type==fga::Section::outro,"intro and outro belong to form");
        check(fga::trackAtRow(0)==4&&fga::trackAtRow(13)==1&&fga::trackAtRow(14)==0&&fga::trackAtRow(16)==3,"musical track order");
        check(p.length()>0,"song duration");check(fga::chordNames(p.settings).size()==4||p.settings.chordCount!=4,"harmony");
        for(size_t ti=0;ti<p.tracks.size();++ti)for(size_t si=0;si<p.sections.size();++si)for(auto n:p.tracks[ti].clips[si].notes){check(n.start>=0&&n.start+n.length<=p.sections[si].bars*4.+1e-6,"section bounds");check(n.length>0&&n.pitch>=0&&n.pitch<128&&n.velocity>0&&n.velocity<128,"valid MIDI");}
        fga::generate(p);for(size_t ti=0;ti<p.tracks.size();++ti)check(p.tracks[ti].clips==original.tracks[ti].clips,"repeatable stored seeds");
        fga::generate(p,fga::Scope::clip,0,4,9876);
        bool changed=false;
        for(size_t ti=0;ti<p.tracks.size();++ti)for(size_t si=0;si<p.sections.size();++si){if(ti==0&&si==4)changed=!(p.tracks[ti].clips[si]==original.tracks[ti].clips[si]);else check(p.tracks[ti].clips[si]==original.tracks[ti].clips[si],"clip regeneration isolation");}
        check(changed,"clip changed");
        p=original;p.tracks[0].locked=true;p.tracks[4].clips[4].locked=true;fga::generate(p,fga::Scope::song,0,0,12345);
        check(p.tracks[0].clips==original.tracks[0].clips,"track lock");check(p.tracks[4].clips[4].notes==original.tracks[4].clips[4].notes&&p.tracks[4].clips[4].seed==original.tracks[4].clips[4].seed,"clip lock");
        p=original;fga::generate(p,fga::Scope::track,3,0,1357);
        for(int ti=0;ti<17;++ti)if(ti!=3)check(p.tracks[(size_t)ti].clips==original.tracks[(size_t)ti].clips,"track scope");
        p=original;fga::generate(p,fga::Scope::section,0,3,2468);
        for(int ti=0;ti<17;++ti)for(size_t si=0;si<p.sections.size();++si)if(si!=3)check(p.tracks[(size_t)ti].clips[si]==original.tracks[(size_t)ti].clips[si],"section scope");
        p=original;p.insertSection(2,p.sections[1],1);check(p.tracks[0].clips[2]==original.tracks[0].clips[1],"duplicate MIDI");p.removeSection(2);check(p.length()==original.length(),"remove section duration");p.moveSection(1,4);check(p.tracks[0].clips[4]==original.tracks[0].clips[1],"move section MIDI");
        auto midi=fga::midiFile(original);check(midi.size()>500&&std::string(midi.begin(),midi.begin()+4)=="MThd","MIDI header");check(midi[11]==18,"conductor and 17 tracks");
        auto stem=fga::midiFile(original,4);check(stem[11]==2,"single stem MIDI");
        if(argc>1&&genre==1){auto pedal=original;pedal.tracks[0].engine.pedalCC=true;auto cc=fga::midiFile(pedal,0);std::ofstream ccFile(std::string(argv[1])+"/test-piano-cc.mid",std::ios::binary);ccFile.write((const char*)cc.data(),(std::streamsize)cc.size());std::ofstream f(std::string(argv[1])+"/test-song.mid",std::ios::binary);f.write((const char*)midi.data(),(std::streamsize)midi.size());std::ofstream g(std::string(argv[1])+"/test-kick.mid",std::ios::binary);g.write((const char*)stem.data(),(std::streamsize)stem.size());}
        std::cout<<"PASS "<<fga::genres()[(size_t)genre]<<" / "<<(int)(p.length()*60/p.settings.bpm)/60<<":"<<(int)(p.length()*60/p.settings.bpm)%60<<" / "<<p.length()/4<<" bars\n";
    }
    fga::Clip c; c.notes={{.13,.5,60,96,0},{.3,.2,127,100,0}};fga::quantise(c,.25);check(c.notes[0].start==.25,"quantise");fga::transpose(c,12);check(c.notes[0].pitch==72&&c.notes[1].pitch==127,"transpose clamp");
    c.notes.push_back({std::numeric_limits<double>::quiet_NaN(),1,60,90,0});fga::tidyNotes(c,1);check(c.notes.size()==2,"invalid input sanitised");
    auto edge=fga::newProject();edge.loop=true;edge.loopStart=0;edge.loopEnd=.00000001;edge.normalise();check(!edge.loop,"sub-tick loop disabled");
    auto automated=fga::newProject();const auto dryMidi=fga::midiFile(automated);auto& volume=automated.tracks[4].volume;
    check(!volume.enabled&&volume.valueAt(8)==1,"automation defaults bypassed");
    volume.points={{0,0,0},{8,1,0},{16,.5,0}};check(volume.valueAt(4)==1,"disabled envelope is unity");volume.enabled=true;
    check(volume.valueAt(4)==.5&&volume.valueAt(12)==.75&&volume.valueAt(100)==.5,"linear interpolation and endpoint hold");volume.points[0].curve=1;check(std::abs(volume.valueAt(4)-.75)<1e-12,"curved interpolation");
    double previous=-1;for(int k=0;k<=800;++k){auto v=volume.valueAt(k/100.);check(v>=previous&&v<=1,"bounded monotonic curve");previous=v;}
    check(fga::midiFile(automated)==dryMidi,"audio automation must not alter MIDI notes or CC");const auto savedVolume=volume.points;fga::generate(automated,fga::Scope::clip,4,1,992);check(volume.enabled&&volume.points.size()==savedVolume.size()&&volume.points[0].curve==1,"regenerate preserves automation");
    volume.points={{8,.2,.4},{automated.sectionStart(1)+4,.8,-.5}};const auto oldSecond=volume.points[1].beat;automated.insertSection(1,{fga::Section::main,8,"Inserted"});check(volume.points[1].beat==oldSecond+32,"insert shifts automation");automated.removeSection(1);check(volume.points[1].beat==oldSecond,"remove shifts automation back");
    automated.moveSection(0,1);check(volume.points[0].beat==4&&volume.points[0].value==.8,"automation follows moved sections");
    fga::VolumeAutomation malformedVolume;malformedVolume.enabled=true;malformedVolume.points={{4,3,2},{-1,-2,-3},{4,.3,.5},{std::numeric_limits<double>::quiet_NaN(),1,0}};malformedVolume.normalise(8);check(malformedVolume.points.size()==2&&malformedVolume.points[0].beat==0&&malformedVolume.points[0].value==0&&malformedVolume.points[1].value==.3,"sanitize sort and merge automation points");
    std::set<std::string> palettes;
    for(int g=0;g<13;++g){auto styled=fga::newProject(g);std::ostringstream signature;for(int t:{0,1,3,4})signature<<styled.tracks[(size_t)t].sound<<":"<<styled.tracks[(size_t)t].engine.tone<<":";palettes.insert(signature.str());check(styled.tracks[1].pan==0&&styled.tracks[1].engine.reverb==0&&styled.tracks[1].engine.chorus==0,"dry centred default bass");check(styled.tracks[4].gain>styled.tracks[8].gain,"kick anchors the kit without hats dominating");}
    check(palettes.size()==13,"thirteen distinct sound palettes, not one default kit/keyboard");
    auto personal=fga::newProject();personal.tracks[0].locked=true;personal.tracks[0].sound=6;personal.tracks[1].instrument="plugin";personal.tracks[1].instrumentPath="personal.vst3";personal.tracks[1].gain=.37;personal.tracks[3].instrument="sample";personal.tracks[3].instrumentPath="personal.wav";fga::applyGenre(personal,8);check(personal.tracks[0].sound==6&&personal.tracks[1].gain==.37&&personal.tracks[1].instrumentPath=="personal.vst3"&&personal.tracks[3].instrumentPath=="personal.wav","style preserves locked guides, personal instruments and routing");
    std::cout<<"All arranger core tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
