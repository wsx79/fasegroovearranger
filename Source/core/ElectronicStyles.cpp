// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ElectronicStyles.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegroovedrums/Source/engine/Drums.h"
#include <cmath>
#include <iterator>
namespace fga {
namespace {
std::uint32_t draw(std::uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;return x^(x>>16);}
int nearestPitch(int pitch,int previous,int low,int high){
    int best=low,score=1000;for(int note=low;note<=high;++note)if((note-pitch)%12==0){const int distance=std::abs(note-previous);if(distance<score){best=note;score=distance;}}return best;
}
std::array<int,4> voiceTriad(const sgk::Chord& chord,const std::array<int,4>& previous){
    std::array<int,4> best{55,60,64,67};int score=10000;
    for(int octave=3;octave<=5;++octave)for(int inversion=0;inversion<3;++inversion){
        std::array<int,3> triad{12*octave+chord.rootPc,12*octave+chord.rootPc+chord.third,12*octave+chord.rootPc+chord.fifth};
        for(int n=0;n<inversion;++n)triad[(size_t)n]+=12;std::sort(triad.begin(),triad.end());
        std::array<int,4> notes{triad[0],triad[1],triad[2],triad[0]+12};std::sort(notes.begin(),notes.end());
        if(notes.front()<55||notes.back()>72)continue;int cost=0;for(size_t n=0;n<notes.size();++n)cost+=std::abs(notes[n]-previous[n]);
        if(cost<score){score=cost;best=notes;}
    }return best;
}
}
sgk::Chord chordAtBeat(const Settings& s,double beat){sgk::GrooveSettings g;g.keyRoot=s.key;g.scale=s.scale;g.extension=(sgk::Extension)s.extension;g.progressionLength=s.chordCount;g.progression=s.degrees;g.freeChords=s.freeChords;return sgk::GrooveEngine::chordForSlot(g,(int)(std::max(0.,beat)/(s.chordSteps*.25))%s.chordCount);}
void applyElectronicFeel(Settings& s){s.swing=s.genre==technoGenre?0.:.12;s.humanize=.4;}
void applyElectronicHarmony(Settings& s,std::uint32_t seed){
    s.extension=0;s.voicing=0;s.freeChords.fill(-1);s.manualChords.fill(false);s.degrees.fill(0);
    if(s.genre==technoGenre){
        const auto mode=draw(seed+17U)%100;s.scale=mode<40?1:mode<75?2:5;
        const auto vamp=draw(seed+59U)%6;s.chordCount=vamp<2?1:2;s.chordSteps=draw(seed+113U)%2?32:64;
        // Slow modal vamps, with a tonic pedal still available. Avoid structural
        // diminished chords: the same degree has a different quality in each mode.
        static constexpr int seconds[3][5]{{3,4,6,1,2},{3,4,5,6,2},{1,3,5,6,1}};
        const int second=seconds[s.scale==1?0:s.scale==2?1:2][draw(seed+71U)%5];
        for(size_t i=1;i<s.degrees.size();i+=2)s.degrees[i]=second;
    }else{
        // These degrees describe natural minor. In Dorian VI would become vi-diminished.
        s.scale=2;s.chordCount=4;s.chordSteps=draw(seed+113U)%2?16:32;
        static constexpr int progressions[][4]{
            {0,5,2,6},{5,6,0,0},{0,6,5,6},{0,3,5,4},
            {0,5,3,6},{0,2,6,5},{3,5,6,0},{0,4,5,3},
            {0,3,6,2},{0,4,5,6},{0,5,6,4},{0,2,3,6},
            {0,6,3,5},{5,3,0,6},{2,6,0,5},{3,0,5,6}};
        const auto& row=progressions[seed?draw(seed)%std::size(progressions):0];for(size_t i=0;i<s.degrees.size();++i)s.degrees[i]=row[i%4];
    }
}
void addElectronicToneAutomation(Project& p){
    if(p.tracks.empty())return;auto& track=p.tracks[0];
    if(track.locked||std::any_of(track.clips.begin(),track.clips.end(),[](const Clip& c){return c.locked;}))return;
    auto owned=std::find_if(track.parameters.begin(),track.parameters.end(),[](const ParameterAutomation& lane){return lane.generatedTone;});
    if(p.settings.genre!=technoGenre){
        track.parameters.erase(std::remove_if(track.parameters.begin(),track.parameters.end(),[](const ParameterAutomation& lane){return lane.generatedTone&&!lane.curve.locked;}),track.parameters.end());return;
    }
    if(track.freeTimeline||track.instrument!="internal")return;
    if(owned!=track.parameters.end()&&owned->curve.locked)return;
    if(std::any_of(track.parameters.begin(),track.parameters.end(),[](const ParameterAutomation& lane){return !lane.generatedTone&&lane.slot==-1&&lane.index==0;}))return;
    if(owned==track.parameters.end()&&track.parameters.size()>=(size_t)maxParameterLanes)return;
    ParameterAutomation lane;lane.slot=-1;lane.index=0;lane.name="Tone";lane.generatedTone=true;lane.curve.enabled=true;double at=0;const double neutral=track.engine.tone;
    lane.curve.points={{0,neutral,0}};bool build=false;
    for(const auto& section:p.sections){const double end=at+section.bars*4.;if(section.type==Section::buildup){build=true;if(at>0)lane.curve.points.push_back({at-1./64.,neutral,0});lane.curve.points.push_back({at,.2,.35});lane.curve.points.push_back({end-1./16.,.85,0});lane.curve.points.push_back({end,neutral,0});}at=end;}
    if(build){lane.curve.points.push_back({p.length(),neutral,0});lane.curve.normalise(p.length(),1.);if(owned==track.parameters.end())track.parameters.push_back(std::move(lane));else *owned=std::move(lane);}
    else if(owned!=track.parameters.end())track.parameters.erase(owned);
}
bool generateElectronicClip(const Project& p,Clip& clip,int track,double begin,double beats,Section role,bool requested,double previewBeats,int harmonySection){
    // Chord slots follow the song's absolute beat, exactly as the ruler (chordNameAt),
    // the vendor engines (strings) and hand-entered guitar do. A section-relative phase
    // made independent section harmony play other chords than the ones shown and heard.
    const int section=harmonySection>=0?harmonySection:p.sectionAtBeat(begin);auto s=resolveSectionHarmony(p,section);if(!isElectronicGenre(s.genre))return false;
    if(track==2){if(requested||s.stringsMode!=0)return false;clip.notes.clear();return true;}
    const bool techno=s.genre==technoGenre,mixing=role==Section::intro||role==Section::outro,breakdown=role==Section::breakdown,build=role==Section::buildup,drop=role==Section::drop;
    const auto& e=p.tracks[(size_t)track].engine;const double density=std::clamp(takeFeel(e.density,clip.feel[0],s.density),0.,1.);
    clip.notes.clear();if(density<=0)return true;
    const auto motif=clip.motifSeed?clip.motifSeed:clip.seed;const int variant=(int)(draw(motif)%8);
    // Independent musical choices give a vocabulary of phrases rather than eight
    // bundles in which rhythm, contour and accompaniment always move together.
    const auto rhythm=draw(motif+401U),contour=draw(motif+809U),reply=draw(motif+1201U);
    // A sliced loop can have a source origin before beat zero. Wrap that
    // preroll through the current section's harmonic cycle, never another section.
    auto chordForBeat=[](const Settings& harmony,double beat){const double cycle=harmony.chordCount*harmony.chordSteps*.25;return chordAtBeat(harmony,std::fmod(std::fmod(beat,cycle)+cycle,cycle));};
    auto note=[&](double at,double length,int pitch,int velocity,int lane=0){
        if(at<0||at>=beats)return;
        if((track==3||track==leadTrack)&&(e.laneMute[(size_t)lane]||e.laneLevel[(size_t)lane]<=0))return;
        if(track==3||track==leadTrack){
            pitch+=12*e.laneOctave[(size_t)lane];
            velocity=(int)std::lround(velocity*e.laneLevel[(size_t)lane]);
        }
        const double nominalAt=at;
        const auto random=draw(clip.seed+(std::uint32_t)(at*960)+track*199U);
        const double human=takeFeel(e.humanize,clip.feel[2],s.humanize);
        const bool offbeat=((int)std::lround(at*4)&1)!=0;
        at+=((int)(random%201)-100)*human*.00015;
        if(offbeat)at+=(takeFeel(e.swing,clip.feel[1],s.swing))*.25;
        at=std::clamp(at,0.,beats-1./960.);
        if(isHarmonicTrack(track)){
            // Do not drag an old harmony over the next chord, or humanise its
            // attack to the other side of a change. Repeated chords can ring.
            const double chordBeats=s.chordSteps*.25,absolute=begin+at;
            const auto nominal=chordForBeat(s,begin+nominalAt);
            const auto actual=chordForBeat(s,absolute);
            if(nominal.rootPc!=actual.rootPc||nominal.third!=actual.third||nominal.fifth!=actual.fifth)
                at=nominalAt;
            const double boundary=(std::floor((begin+at+1.e-9)/chordBeats)+1.)*chordBeats;
            const auto current=chordForBeat(s,begin+at),next=chordForBeat(s,boundary);
            if(current.rootPc!=next.rootPc||current.third!=next.third||current.fifth!=next.fifth)
                length=std::min(length,std::max(1./960.,boundary-begin-at));
        }
        clip.notes.push_back({at,std::min(length,beats-at),track==3||track==leadTrack?pitch:std::clamp(pitch,0,127),std::clamp(velocity*e.velocity/100,1,127),lane});
    };
    std::array<int,4> previousVoicing{55,60,64,67};int previousLead=67,previousRoot=55;
    const int bars=(int)std::ceil(beats/4.);
    // Include the following bar in previews: humanisation can anticipate its
    // first attack across the window boundary. The fingerprint trims the tail.
    for(int bar=0;bar<bars&&(previewBeats<=0||bar*4.<previewBeats+4.);++bar){const double base=bar*4.;const double segment=s.chordSteps*.25;const bool chordPerBar=segment>=4.&&std::fmod(begin,4.)<1e-9;const auto barChord=chordForBeat(s,begin+base);const auto variation=draw(motif+bar*313U);const bool fill=!mixing&&e.fillEvery>0&&(bar+1)%e.fillEvery==0;
        if(track==1){
            if(build&&bar>=bars-2)continue;
            if(breakdown){if(bar%4==0){const auto chord=chordForBeat(s,begin+base);note(base,1.4,36+chord.rootPc,70);}continue;}
            // Sixteenth cells, -1 ends a row. Rows 8-11 add rolling, offbeat and gallop
            // figures that leave the four-to-the-floor kick its downbeats.
            static constexpr int cells[12][12]{{0,3,6,8,11,14,-1},{2,5,6,10,13,14,-1},{0,2,6,8,11,14,-1},{0,3,7,8,10,14,15,-1},{0,3,6,10,12,14,-1},{2,4,7,10,12,15,-1},{0,2,5,8,10,13,-1},{0,3,6,9,11,14,15,-1},
                {1,2,3,5,6,7,9,10,11,13,14,15},{2,6,10,14,-1},{2,3,6,7,10,11,14,15,-1},{0,2,3,6,8,10,11,14,-1}};
            const auto figure=draw(motif+97U)%(std::uint32_t)std::size(cells);
            for(int cell:cells[figure]){if(cell<0)break;if(cell!=cells[figure][0]&&(variation+(unsigned)cell*17)%100>(unsigned)(35+density*65))continue;
                const double at=base+cell*.25;const auto chord=chordPerBar?barChord:chordForBeat(s,begin+at);int pitch=24+chord.rootPc;if(pitch<28)pitch+=12;
                // Root anchors, with fifth/octave answers inside the same chord.
                // Repeat the two-bar idea; small phrase-end changes develop it.
                const auto tone=draw(contour+(unsigned)(cell+(bar%2)*16)*37U)%5;
                if(cell%4!=0&&cell!=cells[figure][0]){
                    if(tone==1)pitch=nearestPitch(chord.rootPc+chord.fifth,pitch,28,47);
                    else if(tone==2&&pitch+12<=47)pitch+=12;
                }
                note(at,std::clamp(e.gate*.32,.08,.28),pitch,(cell%4==0?104:88)+(int)(variation%11));
            }
        }else if(track==0||track==3){
            if((build&&(!techno||track==3))||(mixing&&(!techno||track==3||bar%4!=0))||(track==3&&(bar%4!=(beats<12?0:2)||breakdown)))continue;
            if(track==0&&techno){
                static constexpr double stabBeats[8][2]{{1.5,-1},{1.5,2.75},{0.75,2.25},{1.5,3.5},{1.75,-1},{2.5,-1},{.5,2.5},{.75,3.25}};
                const bool answer=bar%4>=2;const int fig=(rhythm+(answer?reply%5:0))%std::size(stabBeats);
                const int count=stabBeats[fig][1]>=0?2:1;
                for(int h=0;h<count;++h){
                    const double at=base+stabBeats[fig][h];const auto chord=chordForBeat(s,begin+at);
                    const int root=nearestPitch(chord.rootPc,previousRoot,48,60);previousRoot=root;const double length=breakdown?1.25:.16;
                    note(at,length,root,breakdown?55:70);note(at,length,root+chord.fifth,64);
                    if(!mixing&&!breakdown)note(at,length,root+12,57);
                    if(drop){note(at,length,root+chord.third,58);if(chord.seventh>=0)note(at,length,root+chord.seventh,52);}
                }
            }else if(track==0){
                static constexpr double stabs[8][6]{{.5,1.5,2.5,3.5,-1},{0,.75,1.5,2,2.75,3.5},{.5,1.5,2.5,3.25,-1},{.75,1.5,2.75,3.5,-1},
                    {.5,1.75,2.5,3.5,-1},{0,1.5,2.5,3.25,-1},{.75,1.5,2.5,3.75,-1},{.5,1.25,2.75,3.5,-1}};
                const int fig=(rhythm+(bar%4>=2?reply%5:0))%std::size(stabs);
                for(int hit=0;hit<(breakdown?1:6);++hit){if(!breakdown&&stabs[fig][hit]<0)break;const double at=base+(breakdown?1.:stabs[fig][hit]);
                    const auto chord=chordForBeat(s,begin+at);previousVoicing=voiceTriad(chord,previousVoicing);
                    for(size_t voice=0;voice<previousVoicing.size();++voice)note(at,breakdown?1.25:.18,previousVoicing[voice],(breakdown?52:hit==0?72:63)-(int)voice*3);
                }
            }else{
                static constexpr double answers[]{.5,1.25,1.5,1.75,2.25,2.5,2.75,3.5};
                const double at=base+answers[(rhythm+(bar%8>=4?reply%5:0))%std::size(answers)];
                const auto chord=chordForBeat(s,begin+at);const int root=nearestPitch(chord.rootPc,previousRoot,55,67);previousRoot=root;
                note(at,techno?.18:.45,root,58,11);note(at,techno?.18:.45,root+chord.fifth,51,10);
            }
        }else if(track==leadTrack){
            if((mixing&&bar%4!=0)||(build&&bar>=bars-1)||(breakdown&&bar%4!=0))continue;
            const bool answer=(bar%2)!=0;
            static constexpr int electroCells[8][6]{{0,3,6,8,11,14},{0,2,6,8,10,14},{0,3,6,10,12,14},{0,2,5,8,11,14},
                                                   {2,4,6,8,11,14},{0,3,6,9,12,14},{0,2,6,9,11,15},{0,4,6,8,10,14}};
            static constexpr int electroDegrees[8][6]{{0,0,2,0,1,2},{0,0,2,1,0,1},{1,0,2,1,2,0},{0,2,1,2,0,1},
                {0,1,2,1,0,2},{2,1,0,0,1,0},{0,2,2,1,0,1},{1,2,0,2,1,0}};
            static constexpr int technoCells[8][4]{{0,3,6,10},{0,3,6,14},{0,2,6,10},{0,3,7,10},
                                                  {2,6,10,14},{0,4,8,12},{0,3,6,12},{0,2,7,10}};
            static constexpr int technoDegrees[8][4]{{0,0,2,0},{0,0,2,1},{2,0,2,1},{0,2,0,2},
                {0,1,0,2},{2,1,0,0},{1,0,2,0},{0,2,1,0}};
            const int count=mixing?2:breakdown?1:techno?4:6;
            for(int i=0;i<count;++i){if(i>1&&density<.4&&i%2)continue;
                const int phrase=(contour+(answer?1+reply%3:0))%8,figure=(rhythm+(answer?1+reply%5:0))%8,cell=techno?technoCells[figure][i]:electroCells[figure][i];const double at=base+cell*.25;
                const auto chord=chordPerBar?barChord:chordForBeat(s,begin+at);const int tones[]{0,chord.third,chord.fifth};const int degree=techno?technoDegrees[phrase][i]:electroDegrees[phrase][i];
                int interval=tones[degree];
                const bool cadence=bar%4==3&&i==count-1;
                if(cadence)interval=contour%2?0:chord.third;
                else if(!techno&&!mixing&&!breakdown&&i==count-2&&cell%4!=0){auto colour=s;colour.extension=2;const auto extended=chordForBeat(colour,begin+at);const int upper=contour%2?extended.seventh:extended.ninth;if(upper>=0)interval=upper;}
                const int voiced=nearestPitch(chord.rootPc+interval,previousLead,60,84);previousLead=voiced;const int pitch=std::clamp(voiced+12*e.globalOctave,0,127);const double gate=breakdown?.8:techno?.13:std::clamp(e.gate*.55,.12,.3);
                note(at,cadence?std::max(gate,techno?.25:.35):gate,pitch,(i==0?drop?106:94:drop?93:81)+(int)(draw(motif+i*17U)%5),answer?1:0);
            }
        }else if(isDrumTrack(track)){
            const int lane=track-4,pitch=e.midiNote>=0?e.midiNote:fgd::defaultNote(lane);
            auto hit=[&](double at,int velocity){note(base+at,lane==12?1.:.12,pitch,velocity,lane);};
            if(lane==0){if(!breakdown)for(int beat=0;beat<4;++beat)if(!(build&&bar==bars-1&&beat>=2))hit(beat,techno?116:120);if(!breakdown&&!mixing&&variant%3==2&&bar%4==3)hit(3.75,70);}
            else if(lane==1){if(build){const double step=bar>=bars-2?.25:bar>=bars/2?.5:1.;for(double at=0;at<4;at+=step)hit(at,75+(int)(30*bar/std::max(1,bars-1)));}else if(fill){hit(2.75,76);hit(3.5,91);hit(3.75,101);}}
            else if(lane==2){if(!breakdown){hit(1,techno?89:111);hit(3,techno?95:115);if(variant%3==1&&bar%4==3)hit(2.75,52);if(variant%3==2&&bar%4==1)hit(3.875,58);if(techno&&variant%4==2&&bar%2==1)hit(3.75,62);}}
            else if(lane==4){const int steps=techno||variant%2?16:8;for(int step=0;step<steps;++step){if(breakdown&&step%4!=2)continue;if(step%2&&(density<.35||(draw(motif+unsigned(step)*53U)%5==0)))continue;hit(step*4./steps,(step%4==2?85:techno?58:70)+(int)(variation%8));}}
            else if(lane==5){if(!breakdown&&!build)for(int beat=0;beat<4;++beat)if(beat==0||(draw(motif+unsigned(beat)*71U)%5!=0))hit(beat+.5,drop?96:80);}
            else if(lane==6){if(drop&&(techno||bar%8>=4)){const int pattern=variant%3;if(pattern==0){hit(1,techno?85:75);hit(3,techno?90:82);if(bar%4==3)hit(3.75,65);}else for(int beat=0;beat<4;++beat)hit(beat+(pattern==1?.5:.0),techno?80:67);}}
            else if(lane==7){if(!breakdown)for(int step=1;step<16;step+=2)if((variation+step*31)%100<(unsigned)(density*100))hit(step*.25,54+(step%4)*7);}
            else if(lane==3){if(!mixing&&!breakdown&&bar%2==variant%2){hit(1.75,65);if(techno)hit(3.25,60);}}
            else if(lane==8){if(!techno&&drop&&bar%2==0){hit(.5,68);hit(2.5,75);}}
            else if(lane==9){if(techno&&!mixing&&!breakdown&&density>.65&&bar%4==2)hit(2.75,70);}
            else if(lane==10){if(techno&&drop&&bar%4==3)hit(3.5,63);}
            else if(lane==11){if(fill){hit(3,88);hit(3.5,96);}}
            else if(lane==12){if(!mixing&&(drop||role==Section::main)&&(bar==0||(bar%16==0&&drop)))hit(0,105);}
        }
    }
    if((track==3||track==leadTrack)&&!clip.notes.empty()){
        int low=clip.notes.front().pitch,high=low,shift=0;
        for(const auto& n:clip.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}
        // Fit the whole part by octaves, preserving its intervals and contour
        // when global and lane octave controls reach the MIDI ceiling.
        while(high+shift>127)shift-=12;while(low+shift<0)shift+=12;
        for(auto& n:clip.notes)n.pitch+=shift;
    }
    tidyNotes(clip,beats);return true;
}
std::uint32_t freshElectronicMotif(const Project& p,const Clip& previous,int track,double begin,double beats,Section role,std::uint32_t seed){
    // Four bars are enough to hear a call, its reply and a phrase ending. Keep
    // preview work bounded even for very long clips; no timing or accent tricks.
    const double window=std::min(beats,16.);
    auto fingerprint=[&](const Clip& clip,bool pitches){
        std::vector<std::pair<int,int>> result;int first=-1;
        const auto& source=clip.loopSource.empty()?clip.notes:clip.loopSource;
        for(const auto& n:source){if(n.start>=window)break;if(first<0)first=n.pitch;result.emplace_back((int)std::lround(n.start*4),pitches?n.pitch-first:0);}
        std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
    };
    const auto old=fingerprint(previous,true),oldRhythm=fingerprint(previous,false);
    if(old.empty())return seed;
    auto distance=[](const auto& a,const auto& b){size_t i=0,j=0,common=0;while(i<a.size()&&j<b.size()){if(a[i]==b[j]){++i;++j;++common;}else if(a[i]<b[j])++i;else ++j;}const auto total=a.size()+b.size()-common;return total?1.-(double)common/total:0.;};
    auto best=seed;double bestScore=-1;
    for(unsigned attempt=0;attempt<16;++attempt){
        const auto candidate=attempt?draw(seed+attempt*0x9e3779b9U):seed;
        Clip probe;probe.seed=previous.seed;probe.motifSeed=candidate;probe.feel=previous.feel;
        generateElectronicClip(p,probe,track,begin-previous.loopPhase,beats,role,true,window,p.sectionAtBeat(begin));
        if(probe.notes.empty())continue;
        const double score=.6*distance(oldRhythm,fingerprint(probe,false))+.4*distance(old,fingerprint(probe,true));
        if(score>bestScore){best=candidate;bestScore=score;}
        if(score>=.25)return candidate;
    }
    return best;
}
}
