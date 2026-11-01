// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ElectronicStyles.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegroovedrums/Source/engine/Drums.h"
#include <cmath>
namespace fga {
namespace {
std::uint32_t draw(std::uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;return x^(x>>16);}
sgk::Chord chordAt(const Settings& s,double beat){sgk::GrooveSettings g;g.keyRoot=s.key;g.scale=s.scale;g.extension=(sgk::Extension)s.extension;g.progressionLength=s.chordCount;g.progression=s.degrees;g.freeChords=s.freeChords;return sgk::GrooveEngine::chordForSlot(g,(int)(std::max(0.,beat)/(s.chordSteps*.25))%s.chordCount);}
}
bool generateElectronicClip(const Project& p,Clip& clip,int track,double begin,double beats,Section role,bool requested){
    const auto& s=p.settings;if(!isElectronicGenre(s.genre))return false;
    if(track==0||track==2||track==3){if(requested)return false;clip.notes.clear();return true;}
    const bool techno=s.genre==technoGenre,mixing=role==Section::intro||role==Section::outro,breakdown=role==Section::breakdown,build=role==Section::buildup,drop=role==Section::drop;
    const auto& e=p.tracks[(size_t)track].engine;const double density=std::clamp(e.density<0?s.density:e.density,0.,1.);
    clip.notes.clear();if(density<=0)return true;
    const auto motif=clip.motifSeed?clip.motifSeed:clip.seed;const int variant=(int)(draw(motif)%4);
    auto note=[&](double at,double length,int pitch,int velocity,int lane=0){
        if(at<0||at>=beats)return;
        const auto random=draw(clip.seed+(std::uint32_t)(at*960)+track*199U);
        const double human=e.humanize<0?s.humanize:e.humanize;
        at+=((int)(random%201)-100)*human*.00015;
        if(((int)std::lround(at*4)&1)!=0)at+=(e.swing<0?s.swing:e.swing)*.08;
        at=std::clamp(at,0.,beats-1./960.);
        clip.notes.push_back({at,std::min(length,beats-at),std::clamp(pitch,0,127),std::clamp(velocity*e.velocity/100,1,127),lane});
    };
    const int bars=(int)std::ceil(beats/4.);
    for(int bar=0;bar<bars;++bar){const double base=bar*4.;const auto variation=draw(motif+bar*313U);const bool fill=!mixing&&e.fillEvery>0&&(bar+1)%e.fillEvery==0;
        if(track==1){
            if(build&&bar>=bars-2)continue;
            if(breakdown){if(bar%4==0){const auto chord=chordAt(s,begin+base);note(base,1.4,36+chord.rootPc,70);}continue;}
            static constexpr int cells[4][7]{{0,3,6,8,11,14,-1},{2,5,6,10,13,14,-1},{0,2,6,8,11,14,-1},{0,3,7,8,10,14,15}};
            for(int cell:cells[variant]){if(cell<0)continue;if((variation+(unsigned)cell*17)%100>(unsigned)(35+density*65))continue;
                const double at=base+cell*.25;const auto chord=chordAt(s,begin+at);int pitch=24+chord.rootPc;if(pitch<28)pitch+=12;
                if(!techno&&(cell==7||cell==15))pitch+=12;else if(techno&&cell==14&&(bar%4)==3)pitch+=chord.fifth;
                note(at,std::clamp(e.gate*.32,.08,.28),pitch,(cell%4==0?104:88)+(int)(variation%11));
            }
        }else if(track==leadTrack){
            if(mixing&&bar<16)continue;
            if(breakdown&&bar%4!=0)continue;
            if(techno&&bar%4==3)continue;
            static constexpr int technoCells[]{1,6,10};static constexpr int electroCells[]{0,3,6,10,12,15};
            const int count=techno?3:6;
            for(int i=0;i<count;++i){if((variation+i*23)%100>(unsigned)(25+density*75))continue;
                int cell=techno?technoCells[(i+variant)%3]:electroCells[(i+variant)%6];const double at=base+cell*.25;const auto chord=chordAt(s,begin+at);
                const int tones[]{0,chord.third,chord.fifth,12};int degree=techno?(i+variant)%2:(i+bar/2+variant)%4;
                note(at,breakdown?.65:std::clamp(e.gate*.5,.09,.45),60+chord.rootPc+tones[degree],drop?103:86);
            }
        }else if(isDrumTrack(track)){
            const int lane=track-4,pitch=e.midiNote>=0?e.midiNote:fgd::defaultNote(lane);
            auto hit=[&](double at,int velocity){note(base+at,lane==12?1.:.12,pitch,velocity,lane);};
            if(lane==0){if(!breakdown)for(int beat=0;beat<4;++beat)if(!(build&&bar==bars-1&&beat>=2))hit(beat,techno?116:120);}
            else if(lane==1){if(build){const double step=bar>=bars-2?.25:bar>=bars/2?.5:1.;for(double at=0;at<4;at+=step)hit(at,75+(int)(30*bar/std::max(1,bars-1)));}else if(fill){hit(2.75,76);hit(3.5,91);hit(3.75,101);}}
            else if(lane==2){if(!breakdown){hit(1,techno?89:111);hit(3,techno?95:115);}}
            else if(lane==4){const int steps=techno?16:8;for(int step=0;step<steps;++step){if(breakdown&&step%4!=2)continue;if(step%2&&density<.35)continue;hit(step*4./steps,(step%4==2?85:techno?58:70)+(int)(variation%8));}}
            else if(lane==5){if(!breakdown&&!build)for(int beat=0;beat<4;++beat)hit(beat+.5,drop?96:80);}
            else if(lane==6){if(drop&&(techno||bar%8>=4))for(int beat=0;beat<4;++beat)hit(beat+.5,techno?80:67);}
            else if(lane==7){if(!breakdown)for(int step=1;step<16;step+=2)if((variation+step*31)%100<(unsigned)(density*100))hit(step*.25,54+(step%4)*7);}
            else if(lane==3){if(!mixing&&!breakdown&&bar%2==variant%2){hit(1.75,65);if(techno)hit(3.25,60);}}
            else if(lane==8){if(!techno&&drop&&bar%2==0){hit(.5,68);hit(2.5,75);}}
            else if(lane==9){if(techno&&!mixing&&!breakdown&&density>.65&&bar%4==2)hit(2.75,70);}
            else if(lane==10){if(techno&&drop&&bar%4==3)hit(3.5,63);}
            else if(lane==11){if(fill){hit(3,88);hit(3.5,96);}}
            else if(lane==12){if(!mixing&&(drop||role==Section::main)&&(bar==0||(bar%16==0&&drop)))hit(0,105);}
        }
    }
    tidyNotes(clip,beats);return true;
}
}
