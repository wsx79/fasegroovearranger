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
// Review fixes (0.57.27): Sub Sine survives, chords cycle from the section start, the bass keeps its
// sidechain, the minor pentatonic, the build-up's dominant, Latin progressions.
inline void verifyReviewFixes(){
    {auto p=fga::newProject(0);const int sub=p.addPartTrack(1);p.tracks[(size_t)sub].sound=7;p.normalise();check(p.tracks[(size_t)sub].sound==7,"Sub Sine (guide sound 7) survives normalise");}
    for(int g:{0,fga::afroHouseGenre,fga::latinHouseGenre,(int)fga::Genre::gospel}){auto p=fga::newProject(g);check(p.tracks[1].engine.pump>0&&p.tracks[1].engine.pumpSource==1,"four-on-the-floor genres keep the kick sidechain on the bass");}
    {auto p=fga::newProject(0);p.settings.chordCount=8;p.settings.chordSteps=16;for(int i=0;i<8;++i)p.settings.degrees[(size_t)i]=i%7;p.sections={{fga::Section::intro,4,"I"},{fga::Section::main,8,"A"}};p.normalise();
        // a section starting mid-cycle: its first bar plays the progression's first chord, as the generators do
        check(fga::chordNameAt(p,p.sectionStart(1)+.01)==fga::chordNameAt(p,.01),"each section's chord cycle starts at its first bar");}
    {auto p=fga::newProject(0);p.settings.scale=2;p.settings.chordCount=4;p.sections={{fga::Section::main,16,"A"},{fga::Section::buildup,8,"B"},{fga::Section::drop,16,"D"}};p.normalise();fga::generate(p,fga::Scope::song,0,0,99);
        bool dominant=false;for(int si=0;si<(int)p.sections.size();++si)if(p.sections[(size_t)si].type==fga::Section::buildup){const auto h=fga::activeHarmony(p,si);const double last=p.sectionStart(si+1)-2;const auto ch=fga::chordAtBeat(h,last);dominant=((ch.rootPc-h.key+12)%12==7)&&ch.third==4;}
        check(dominant,"a minor build-up turns on a major dominant in its last bar");}
    {std::set<int> seen;for(std::uint32_t seed=1;seed<=30;++seed){auto p=fga::newProject(fga::latinHouseGenre);fga::randomiseHarmony(p,seed);std::string d;for(int i=0;i<p.settings.chordCount;++i)d+=std::to_string(p.settings.degrees[(size_t)i]);seen.insert(p.settings.chordCount);}
        auto fresh=fga::newProject(fga::latinHouseGenre);check(fresh.settings.chordCount==4,"a new Latin project starts on a four-chord Latin phrase");}
    {const auto set=sgk::pentatonicNotes(0,2,0,11);std::set<int> pcs;for(int i=0;i<set.size;++i)pcs.insert(set.notes[(size_t)i]%12);check(pcs==std::set<int>{0,3,5,7,10},"the minor pentatonic is 1 b3 4 5 b7");}
    std::cout<<"PASS review fixes: Sub Sine, section chord cycles, bass sidechain, minor dominant, Latin, pentatonic\n";
}
// Second review (0.57.28): the drop resolves the build-up's dominant, later drops vary, short Afro/Latin
// intros stand on bass and percussion, Latin RANDOM CHORDS keeps the mode, automation blocks = valueAt.
inline void verifyReviewFixes2(){
    int dropsOnTonic=0,dropsSeen=0,variedSongs=0,songsWithTwo=0;
    for(int g:{0,1,3,8,9,12})for(std::uint32_t seed:{5u,77u,901u}){auto p=fga::newProject(g);p.settings.randomAutomation=false;fga::generate(p,fga::Scope::song,0,0,seed);std::vector<std::vector<int>> phrases;
        for(int si=0;si<(int)p.sections.size();++si)if(p.sections[(size_t)si].type==fga::Section::drop&&p.sections[(size_t)si].harmony.automatic){const auto h=fga::activeHarmony(p,si);++dropsSeen;dropsOnTonic+=h.degrees[0]==0&&h.freeChords[0]<0;
            std::vector<int> d(h.degrees.begin(),h.degrees.begin()+h.chordCount);if(std::find(phrases.begin(),phrases.end(),d)==phrases.end())phrases.push_back(d);else phrases.push_back({-1});}
        if(phrases.size()>=2){++songsWithTwo;variedSongs+=std::set<std::vector<int>>(phrases.begin(),phrases.end()).size()>=2;}}
    check(dropsSeen>0&&dropsOnTonic==dropsSeen,"every automatic drop starts on the tonic, so the build-up's dominant resolves");
    check(songsWithTwo==0||variedSongs==songsWithTwo,"a later drop takes another phrase (or the same with another cadence)");
    for(int g:{0,1,3,8,9,12})for(std::uint32_t seed:{5u,77u,901u}){auto p=fga::newProject(g);p.settings.randomAutomation=false;fga::generate(p,fga::Scope::song,0,0,seed);const auto main=fga::activeHarmony(p,-1);
        for(int si=0;si<(int)p.sections.size();++si)if(p.sections[(size_t)si].type==fga::Section::drop&&p.sections[(size_t)si].harmony.automatic){const auto d=fga::activeHarmony(p,si);bool same=d.chordCount==main.chordCount;for(int i=0;same&&i<d.chordCount;++i)same=d.degrees[(size_t)i]==main.degrees[(size_t)i]&&d.freeChords[(size_t)i]==main.freeChords[(size_t)i];check(!same,"the drop's phrase contrasts with the main part's");}}
    for(int g:{fga::latinHouseGenre,fga::afroHouseGenre}){auto p=fga::newProject(g);p.settings.randomAutomation=false;p.sections={{fga::Section::intro,32,"Intro"},{fga::Section::main,16,"Main"}};p.normalise();fga::generate(p);
        double bass=1e9,strings=1e9;for(const auto& n:p.tracks[1].clips[0].notes)bass=std::min(bass,n.start);for(const auto& n:p.tracks[2].clips[0].notes)strings=std::min(strings,n.start);check(strings>=bass-4,"a 32-bar Afro/Latin intro brings the strings in with the bass, not before");}
    for(int g:{fga::latinHouseGenre,fga::afroHouseGenre})for(int bars:{8,16}){auto p=fga::newProject(g);p.settings.randomAutomation=false;p.sections={{fga::Section::intro,bars,"Intro"},{fga::Section::main,16,"Main"}};p.normalise();fga::generate(p);
        double bass=1e9,strings=1e9;for(const auto& n:p.tracks[1].clips[0].notes)bass=std::min(bass,n.start);for(const auto& n:p.tracks[2].clips[0].notes)strings=std::min(strings,n.start);
        check((bars<16||bass<36)&&strings>=bass-4,"a short Afro/Latin intro: rhythm-only DJ blend, then the bass, and no strings alone before it");}
    {auto p=fga::newProject(fga::latinHouseGenre);for(std::uint32_t seed=1;seed<=20;++seed){const int scale=p.settings.scale;fga::randomiseHarmony(p,seed);check(p.settings.scale==scale,"Latin RANDOM CHORDS keeps the song's mode");}}
    {fga::VolumeAutomation a;a.enabled=true;a.points={{0,.2,0},{1.5,1.4,.5},{3,.6,-.3},{8,1,0}};std::vector<float> out(300);a.render(out.data(),300,-.2,1./37.);
        bool same=true;for(int s=0;s<300;++s)same=same&&std::abs(out[(size_t)s]-(float)a.valueAt(-.2+s/37.))<1e-5f;check(same,"automation rendered over a block equals valueAt sample by sample");}
    std::cout<<"PASS second review fixes: drops on the tonic ("<<dropsOnTonic<<"/"<<dropsSeen<<"), varied drops, short Afro/Latin intros, Latin mode, automation blocks\n";
}
