// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include <cmath>
#include <stdexcept>
namespace fga {
namespace {
using Bytes=std::vector<std::uint8_t>;
void be(Bytes& out,std::uint32_t x,int bytes){for(int i=bytes-1;i>=0;--i)out.push_back((std::uint8_t)(x>>(i*8)));}
void variable(Bytes& out,std::uint32_t value){std::uint8_t b[4];int i=0;b[i++]=(std::uint8_t)(value&127);while((value>>=7)&&i<4)b[i++]=(std::uint8_t)((value&127)|128);while(i)out.push_back(b[--i]);}
void meta(Bytes& out,std::uint32_t delta,int type,const Bytes& bytes){variable(out,delta);out.push_back(255);out.push_back((std::uint8_t)type);variable(out,(std::uint32_t)bytes.size());out.insert(out.end(),bytes.begin(),bytes.end());}
void text(Bytes& out,std::uint32_t delta,int type,const std::string& s){meta(out,delta,type,Bytes(s.begin(),s.end()));}
void chunk(Bytes& file,Bytes data){file.insert(file.end(),{'M','T','r','k'});be(file,(std::uint32_t)data.size(),4);file.insert(file.end(),data.begin(),data.end());}
}
std::vector<std::uint8_t> midiFile(const Project& p,int onlyTrack) {
    if(onlyTrack>=(int)p.tracks.size())throw std::out_of_range("MIDI track");
    Bytes out{'M','T','h','d'};be(out,6,4);be(out,1,2);be(out,onlyTrack<0?(std::uint32_t)p.tracks.size()+1:2,2);be(out,960,2);
    Bytes conductor; text(conductor,0,3,p.name); const auto tempo=(std::uint32_t)std::llround(60000000.0/std::clamp(p.settings.bpm,30.0,300.0));
    meta(conductor,0,0x51,{(std::uint8_t)(tempo>>16),(std::uint8_t)(tempo>>8),(std::uint8_t)tempo});meta(conductor,0,0x58,{4,2,24,8});
    // Key signature uses the scale's parent major: modal detail remains in the name.
    const int parentOffsets[]{0,-2,3,-7,-5,-4,3,3,1,0,-5,-7,-4};
    const int sharps[]{0,-5,2,-3,4,-1,6,1,-4,3,-2,5};
    int parent=(p.settings.key+parentOffsets[std::clamp(p.settings.scale,0,12)]+24)%12;
    bool minor=p.settings.scale==2||p.settings.scale==6||p.settings.scale==7;
    meta(conductor,0,0x59,{(std::uint8_t)sharps[parent],(std::uint8_t)(minor?1:0)});
    text(conductor,0,1,"Fase Groove Arranger | key="+std::to_string(p.settings.key)+" scale="+std::to_string(p.settings.scale));
    std::uint32_t last=0;
    for(size_t si=0;si<p.sections.size();++si){auto at=(std::uint32_t)std::llround(p.sectionStart((int)si)*960);text(conductor,at-last,6,p.sections[si].name);last=at;}
    auto end=(std::uint32_t)std::llround(p.length()*960);meta(conductor,end-last,0x2f,{});chunk(out,std::move(conductor));
    auto all=events(p,onlyTrack);
    for(size_t ti=0;ti<p.tracks.size();++ti){
        if(onlyTrack>=0&&(int)ti!=onlyTrack)continue;
        Bytes data;text(data,0,3,p.tracks[ti].name);int channel=ti>=4?9:(int)ti;
        // GM hints make exported MIDI immediately useful in a GM player.
        if(ti<4){static const int programs[]{4,33,48,26};data.insert(data.end(),{0,(std::uint8_t)(0xc0|channel),(std::uint8_t)programs[ti]});}
        last=0;
        for(const auto& e:all)if(e.track==(int)ti){auto at=(std::uint32_t)std::llround(std::max(0.0,e.beat)*960);variable(data,at-last);last=at;data.push_back((std::uint8_t)((e.velocity>0?0x90:0x80)|channel));data.push_back((std::uint8_t)e.pitch);data.push_back((std::uint8_t)e.velocity);}
        meta(data,std::max(end,last)-last,0x2f,{});chunk(out,std::move(data));
    }
    return out;
}
}
