// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
inline void verifyHarmonyContracts(){
    using namespace fga;
    struct Quality {int scale,degree,extension,root;unsigned mask;};
    const Quality qualities[]{
        {0,0,0,0,0x091}, {0,0,1,0,0x891}, {0,0,2,0,0x895},
        {0,4,1,7,0x491}, {2,0,1,0,0x489},
        {6,0,1,0,0x889}, {6,0,2,0,0x889},
        {7,0,1,0,0x889}, {7,0,2,0,0x889},
        {6,4,1,7,0x491}, {6,4,2,7,0x493}, {6,6,1,11,0x249}
    };
    static_assert((int)fgg::ChordType::mMaj7==(int)fgl::ChordType::mMaj7);
    static_assert((int)fgg::ChordType::dim7==(int)fgl::ChordType::dim7);
    static_assert((int)fgg::ChordType::dom7b9==(int)fgl::ChordType::dom7b9);
    for(int transpose=0;transpose<12;++transpose)for(const auto& q:qualities){
        const auto chord=bandChord(transpose,q.scale,q.degree,-1,(sgk::Extension)q.extension);
        const auto guitar=fgg::tonesOf(chord);const auto lead=fgl::tonesOf({chord.root,chord.type});
        check(guitar.root==(transpose+q.root)%12&&guitar.mask==q.mask,"guitar adapter matches the reference quality across every key");
        check(lead.root==guitar.root&&lead.mask==q.mask,"lead receives the same reference chord quality as guitar");
        for(int pitch:fgg::laneNotes(fgg::Lane::strumDown,guitar,0))check((q.mask&(1U<<((pitch-guitar.root+120)%12)))!=0,"rendered guitar strums stay inside the reference chord");
    }
    const auto minorMajor=bandChord(0,6,0,-1,sgk::Extension::seventh);
    unsigned strum=0;for(int pitch:fgg::laneNotes(fgg::Lane::strumDown,fgg::tonesOf(minorMajor),0))strum|=1U<<(pitch%12);
    check((strum&(1U<<11))!=0&&(strum&(1U<<10))==0,"C harmonic-minor tonic strum contains B natural and never substitutes Bb");
    for(const auto& free:std::array<std::pair<int,unsigned>,3>{{{0,0x895},{12,0x48d},{24,0x495}}}){
        const auto chord=bandChord(0,0,0,free.first,sgk::Extension::ninth);
        check(chord.root==0&&fgg::tonesOf(chord).mask==free.second,"free major, minor and dominant chord spelling survives the shared adapter");
    }
    auto project=newProject(0);project.sections={{Section::main,1,"Signature"}};for(auto& track:project.tracks)track.clips.assign(1,{});project.normalise();
    struct Signature {int sharps=99,minor=-1;std::string text;};
    auto conductor=[&](){
        const auto data=midiFile(project);check(data.size()>=22,"MIDI has a conductor track");
        const size_t end=22+((size_t)data[18]<<24)+((size_t)data[19]<<16)+((size_t)data[20]<<8)+data[21];size_t at=22;Signature found;
        check(end<=data.size(),"conductor length is valid");
        auto vlq=[&](){unsigned value=0;for(int i=0;i<4;++i){check(at<end,"bounded MIDI variable integer");const auto byte=data[at++];value=(value<<7)|(byte&127U);if(!(byte&128U))return value;}throw std::runtime_error("invalid MIDI variable integer");};
        while(at<end){vlq();check(at+2<=end&&data[at++]==255,"conductor contains SMF metadata");const int type=data[at++];const auto size=vlq();check(size<=end-at,"metadata length is bounded");
            if(type==0x59){check(size==2,"key signature has two bytes");found.sharps=(int)(std::int8_t)data[at];found.minor=data[at+1];}
            if(type==1)found.text.assign(data.begin()+(std::ptrdiff_t)at,data.begin()+(std::ptrdiff_t)(at+size));at+=size;
        }return found;
    };
    // Musical reference: these modal tonics share C's parent signature (the minor family is A).
    const int tonics[]{0,2,9,7,5,4,9,9,11,0,5,7,4};
    const int sfByParent[]{0,-5,2,-3,4,-1,-6,1,-4,3,-2,5};
    for(int scale=0;scale<13;++scale)for(int shift=0;shift<12;++shift){
        project.settings.scale=scale;project.settings.key=(tonics[scale]+shift)%12;const auto result=conductor();
        check(result.sharps==sfByParent[shift]&&result.minor==(scale==2||scale==6||scale==7),"all 156 key/mode combinations keep their reference SMF signature policy");
        const auto name=std::string(sgk::spelledNote(project.settings.key,project.settings.key,scale))+" "+sgk::scaleName(scale);
        check(result.text.find(name)!=std::string::npos,"MIDI text retains the readable tonic and exact mode");
    }
    project.settings.key=6;project.settings.scale=0;auto signature=conductor();check(signature.sharps==-6&&signature.minor==0&&signature.text.find("Gb")!=std::string::npos,"Gb export uses six flats and a matching tonic name");
    project.settings.key=3;project.settings.scale=2;signature=conductor();check(signature.sharps==-6&&signature.minor==1&&signature.text.find("Eb")!=std::string::npos,"Eb minor export uses six flats and the minor flag");
    project.settings.key=2;project.settings.scale=1;signature=conductor();check(signature.sharps==0&&signature.minor==0&&signature.text.find("D Dorian")!=std::string::npos,"D Dorian keeps its parent signature with exact modal text");
    project.sections.push_back({Section::drop,1,"Drop"});project.normalise();project.sections[1].harmony.enabled=true;project.sections[1].harmony.key=0;project.sections[1].harmony.scale=2;
    int signatures=0;const auto data=midiFile(project);const size_t end=22+((size_t)data[18]<<24)+((size_t)data[19]<<16)+((size_t)data[20]<<8)+data[21];
    for(size_t at=22;at+2<end;++at)if(data[at]==255&&data[at+1]==0x59)++signatures;
    check(signatures==2,"a section harmony change writes a second SMF key signature");
}
