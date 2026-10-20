// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Project.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    {auto p=fga::newProject(1);p.sections={{fga::Section::main,4,"Main"}};p.normalise();auto& e=p.tracks[0].engine;e.pattern=sgk::userPatternIndex();e.userRight.fill('.');e.userLeft.fill('.');e.userRight[0]='x';e.userVelRight[0]=37;e.realism=false;e.humanize=0;e.swing=0;e.strum=0;e.fillEvery=0;e.variationEvery=0;fga::generate(p,fga::Scope::track,0);check(!p.tracks[0].clips[0].notes.empty(),"custom piano grid generates notes");auto before=p.tracks[0].clips[0].notes;e.userVelRight[0]=120;fga::generate(p,fga::Scope::track,0);check(p.tracks[0].clips[0].notes[0].velocity>before[0].velocity,"per-step piano velocity reaches engine");e.inversions[0]=2;fga::generate(p,fga::Scope::track,0);check(p.tracks[0].clips[0].notes!=before,"per-chord inversion reaches engine");e.pedalCC=true;auto events=fga::events(p,0);bool pedal=false,expression=false;for(auto event:events){pedal=pedal||event.controller==64;expression=expression||event.controller==11;}check(pedal&&expression,"pedal and expression controller events");auto original=p;fga::applyPianoIntention(p,0,1,true,true,123);check(p.settings.degrees==original.settings.degrees&&p.tracks[0].engine.userRight==original.tracks[0].engine.userRight,"DNA rhythm and harmony locks");}
    for(int genre=0;genre<13;++genre){
        auto p=fga::newProject(genre);auto original=p;
        for(double duration:{4.,6.,8.}){auto form=p;form.settings.songMinutes=duration;fga::arrangeSong(form);const auto minutes=form.length()/form.settings.bpm;check(minutes>=duration,"requested duration reached");check(minutes<duration+.55,"phrase rounding bounded");}
        if(genre==1){auto lead=p;lead.settings.guitarLead=true;fga::generate(lead,fga::Scope::track,0);check(lead.tracks[0].clips!=p.tracks[0].clips,"original guitar lead piano answers");for(int ti=1;ti<17;++ti)check(lead.tracks[(size_t)ti].clips==p.tracks[(size_t)ti].clips,"lead piano generation isolation");auto shifted=p;shifted.tracks[4].engine.midiNote=50;fga::generate(shifted,fga::Scope::track,4);for(const auto& c:shifted.tracks[4].clips)for(auto n:c.notes)check(n.pitch==50,"custom drum MIDI mapping");}
        check(p.tracks.size()==17,"17 instrument tracks");check(p.sections.size()>=6,"genre-specific song stages");
        check(p.length()*60/p.settings.bpm>=300,"minimum requested five minute arrangement");
        check(p.length()*60/p.settings.bpm<335,"phrase aligned duration");
        check(p.sections.front().bars>=16&&p.sections.back().bars>=16,"mixable edges");
        for(int track=0;track<4;++track){for(auto n:p.tracks[(size_t)track].clips.front().notes)check(n.start>=64,"clean intro");for(auto n:p.tracks[(size_t)track].clips.back().notes)check(n.start+n.length<=p.sections.back().bars*4.-64+1e-6,"clean outro");}
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
        std::cout<<"PASS "<<fga::genres()[(size_t)genre]<<"\n";
    }
    fga::Clip c; c.notes={{.13,.5,60,96,0},{.3,.2,127,100,0}};fga::quantise(c,.25);check(c.notes[0].start==.25,"quantise");fga::transpose(c,12);check(c.notes[0].pitch==72&&c.notes[1].pitch==127,"transpose clamp");
    c.notes.push_back({std::numeric_limits<double>::quiet_NaN(),1,60,90,0});fga::tidyNotes(c,1);check(c.notes.size()==2,"invalid input sanitised");
    auto edge=fga::newProject();edge.loop=true;edge.loopStart=0;edge.loopEnd=.00000001;edge.normalise();check(!edge.loop,"sub-tick loop disabled");
    std::cout<<"All arranger core tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
