// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// ARRANGE FROM VOCAL on a synthetic voice: a sung C-E-G / F-A-C / G-B-D line (harmonics and
// vibrato, 120 BPM), then a rest bar. Notes, key, phrases and the chords fitted under them.
#pragma once
#include "../Source/core/VocalArrangement.h"
inline void verifyVocalArrangement(){
    constexpr double rate=22050,bpm=120;const double bps=bpm/60;
    const std::vector<std::vector<int>> bars{{60,64,67,64},{65,69,72,69},{67,71,74,71},{}};
    std::vector<float> voice((size_t)(rate*16*4/bps),0.f);   // 16 bars: the 4-bar phrase four times
    double phase=0;
    for(int rep=0;rep<4;++rep)for(int b=0;b<4;++b)for(int q=0;q<4&&!bars[(size_t)b].empty();++q){
        const double f=440*std::pow(2.,(bars[(size_t)b][(size_t)q]-69)/12.);const int from=(int)(((rep*4+b)*4+q)/bps*rate),to=(int)(((rep*4+b)*4+q+.9)/bps*rate);
        for(int s=from;s<to&&s<(int)voice.size();++s){const double t=(s-from)/rate;phase+=f*(1+.004*std::sin(2*3.14159265*5.5*t))/rate;
            const double env=std::min({1.,t/.02,(to-s)/rate/.02});voice[(size_t)s]=(float)(.3*env*(std::sin(2*3.14159265*phase)+.5*std::sin(4*3.14159265*phase)+.25*std::sin(6*3.14159265*phase)));}}
    const auto a=fga::analyseVocal(voice,rate,0,bps,16);
    check(a.notes.size()>=40&&a.notes.size()<=56,"vocal analysis finds one note per sung quarter");
    bool pitches=true;for(const auto& n:a.notes){const double beat=n.beat+.1;const int bar=(int)std::floor(beat/4)%4,q=(int)std::floor(std::fmod(beat,4.));pitches=pitches&&bar<3&&n.pitch==bars[(size_t)bar][(size_t)q];}
    check(pitches,"each detected note has the sung pitch at its beat");
    check(a.key==0&&a.scale==0,"a C-E-G / F-A-C / G-B-D melody is in C major");
    bool phrases=true;for(int b=0;b<16;++b)phrases=phrases&&a.bars[(size_t)b]==(b%4<3?1:0);check(phrases,"the rest bar of each phrase is found");
    auto p=fga::newProject(0);p.sections={{fga::Section::main,8,"Verse"},{fga::Section::main,8,"Chorus"}};p.normalise();
    check(fga::fitToVocal(p,a)==2,"both sung sections get chords from the vocal");
    const auto& h=p.sections[0].harmony;check(h.enabled&&h.chordCount==4&&h.chordSteps==16&&h.degrees[0]==0&&h.degrees[1]==3&&h.degrees[2]==4,"I - IV - V under C-E-G / F-A-C / G-B-D");
    check(p.settings.vocalsPresent&&p.settings.vocalPhrase==3&&p.settings.vocalBars==a.bars,"VOCALS PRESENT follows the imported phrases");
    fga::generate(p,fga::Scope::song,0,0,4242);bool answers=true;
    // the lead answers only in the bars the voice leaves free (main sections)
    for(size_t ci=0;ci<p.tracks[fga::leadTrack].clips.size();++ci){const double at=p.clipStart(fga::leadTrack,(int)ci);const int si=p.sectionAtBeat(at+.01);if(si<0||p.sections[(size_t)si].type!=fga::Section::main)continue;
        for(const auto& n:p.tracks[fga::leadTrack].clips[ci].notes){const int bar=(int)std::floor((at+n.start)/4+1e-9);if(bar<16)answers=answers&&!a.bars[(size_t)bar];}}
    check(answers,"the lead never plays over a sung bar");
    std::cout<<"PASS arrange from vocal: notes, key, phrases, chords, lead answers\n";
}
// MASTER TEMPO lane: off by default (constant BPM), user points only; time map and MIDI tempo map.
inline void verifyTempoLane(){
    auto p=fga::newProject(0);p.sections={{fga::Section::main,4,"A"}};p.normalise();fga::generate(p,fga::Scope::song,0,0,4242);p.settings.bpm=120;p.normalise();
    check(p.tempoAutomation.isDefault()&&p.bpmAt(9)==p.settings.bpm,"generation never writes the tempo lane: the BPM stays constant");
    check(std::abs(p.secondsAt(8)-4)<1e-9,"constant tempo: 8 beats at 120 BPM last 4 s");
    const auto constant=fga::midiFile(p,-1);
    auto countTempo=[](const std::vector<std::uint8_t>& f){int n=0;for(size_t i=0;i+2<f.size();++i)if(f[i]==0xFF&&f[i+1]==0x51&&f[i+2]==3)++n;return n;};
    check(countTempo(constant)==1,"without the lane the MIDI file has one tempo");
    p.tempoAutomation.enabled=true;p.tempoAutomation.points={{0,fga::automationFromBpm(120),0},{8,fga::automationFromBpm(60),0}};p.normalise();
    check(std::abs(p.bpmAt(4)-90)<1e-6&&std::abs(p.bpmAt(12)-60)<1e-6,"the lane ramps linearly and holds its last value");
    check(std::abs(p.secondsAt(8)-8*std::log(2.))<.002,"a 120 to 60 BPM ramp over 8 beats lasts 8 ln 2 s");
    check(std::abs(p.secondsAt(16)-(8*std::log(2.)+8))<.002,"after the ramp: one second per beat at 60 BPM");
    check(countTempo(fga::midiFile(p,-1))>=20,"the MIDI file carries the tempo ramp");
    p.tempoAutomation.enabled=false;check(fga::midiFile(p,-1)==constant&&std::abs(p.secondsAt(8)-4)<1e-9,"bypassing the lane restores the constant BPM exactly");
    std::cout<<"PASS master tempo lane: constant by default, time map, MIDI tempo map\n";
}
// Afro House drums: the clap can come from the first kick, and the congas play three drums.
inline void verifyAfroDrums(){
    int fromOne=0,fourClaps=0;
    for(std::uint32_t seed=1;seed<=80;++seed){const auto g=fgd::makeRandomPattern(seed,.6f,7);const auto& clap=g.lanes[(size_t)fgd::Lane::clap];const auto& kick=g.lanes[(size_t)fgd::Lane::kick];
        if(fgd::isHit(clap[0])&&fgd::isHit(kick[0]))++fromOne;if(fgd::isHit(clap[0])&&fgd::isHit(clap[4])&&fgd::isHit(clap[8])&&fgd::isHit(clap[12]))++fourClaps;}
    check(fromOne>=12&&fourClaps>=5,"Afro House: claps from the first kick (on all four, on 1 and 3, or the one accent) are generated");
    std::set<int> congas;for(std::uint32_t seed:{11u,23u,42u}){auto p=fga::newProject(fga::afroHouseGenre);p.settings.randomAutomation=false;fga::generate(p,fga::Scope::song,0,0,seed);
        for(const auto& c:p.tracks[(size_t)(4+(int)fgd::Lane::conga)].clips)for(const auto& n:c.notes)congas.insert(n.pitch);}
    check(congas.size()>=3,"Afro House congas play low, middle and high drums");
    std::cout<<"PASS Afro House drums: "<<fromOne<<"/80 claps from the first kick, "<<congas.size()<<" conga pitches\n";
}
inline void verifyExtensionRoundTrip(){
    auto p=fga::newProject(1);p.sections={{fga::Section::main,4,"Main"}};p.normalise();fga::generate(p);
    p.settings.extension=1;fga::applyInstrumentChange(p,0,true);p.settings.extension=0;fga::applyInstrumentChange(p,0,true);const auto triads=p.tracks[0].clips[0].notes;const auto all=p.tracks;
    p.settings.extension=2;fga::applyInstrumentChange(p,0,true);const bool changed=p.tracks[0].clips[0].notes!=triads;
    p.settings.extension=0;fga::applyInstrumentChange(p,0,true);
    check(changed&&p.tracks[0].clips[0].notes==triads,"piano extensions round trip: 7ths change the MIDI, triads come back exactly");
}
