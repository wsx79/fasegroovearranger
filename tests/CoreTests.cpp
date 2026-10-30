// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Project.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <limits>
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.h"
#include "../ThirdParty/fasegroovedrums/Source/engine/GrooveEngine.h"
#include "../ThirdParty/fasegroovelead/Source/engine/GrooveEngine.h"
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
namespace {
int guitarMaxOn(const std::vector<fgg::NoteEvent>& events){int m=0;for(const auto& e:events)if(e.isNoteOn())m=std::max(m,e.velocity);return m;}
bool guitarHas(const fgg::GuitarPattern& p,char symbol,int lane=-1){for(int l=0;l<fgg::numLanes;++l){if(lane>=0&&l!=lane)continue;if(p.lanes[(size_t)l].find(symbol)!=std::string::npos)return true;}return false;}
fgg::GuitarSettings guitarOnly(std::initializer_list<std::pair<fgg::Lane,const char*>> lines){
    fgg::GuitarSettings s;s.swing=0;s.humanize=0;s.accentGroove=0;s.fillEveryBars=0;s.variationEvery=0;s.strumAfterFill=false;s.pattern=fgg::userPatternIndex();
    for(auto& lane:s.user)lane.fill('.');
    for(const auto& [lane,steps]:lines){std::string line;for(const char* c=steps;*c;++c)if(*c!=' ')line+=*c;for(int i=0;i<fgg::userPatternLength;++i)s.user[(size_t)lane][(size_t)i]=line[(size_t)(i%(int)line.size())];}
    return s;
}
void verifyGuitarAudibility(){
    auto base=[]{fgg::GuitarSettings s;s.pattern=fgg::randomPatternIndex();s.humanize=.3f;s.accentGroove=.85f;s.velocity=100;s.roomForBand=true;s.fillEveryBars=0;s.variationEvery=0;s.strumAfterFill=false;return s;};
    for(int genre=0;genre<13;++genre)for(float density:{0.f,.22f,.35f,.5f,.68f,1.f})for(std::uint32_t seed=1;seed<=60;++seed){
        const auto pattern=fgg::makeRandomPattern(seed,density,genre);
        check(guitarHas(pattern,'X'),"random guitar groove keeps an accent");
        bool speaking=false;for(int l=0;l<fgg::numLanes;++l)if(l!=(int)fgg::Lane::scratch&&guitarHas(pattern,'X',l))speaking=true;
        check(speaking,"guitar accent is not only a scratch");
        auto s=base();s.riffSeed=seed;s.density=density;s.randomGenre=genre;s.swing=fgg::genreSwing(genre);
        const auto events=fgg::GrooveEngine::renderClip(s,8);
        check(!events.empty()&&guitarMaxOn(events)>=74,"generated guitar groove stays audible");
    }
    for(const auto& bad:std::initializer_list<std::array<double,3>>{{0,57,0},{4,14,0},{4,14,.35},{4,14,1}}){
        auto s=base();s.randomGenre=(int)bad[0];s.riffSeed=(std::uint32_t)bad[1];s.density=(float)bad[2];
        check(guitarMaxOn(fgg::GrooveEngine::renderClip(s,8))>=84,"formerly silent guitar seeds stay audible");
    }
    for(int genre:{4,5,8})for(std::uint32_t seed=1;seed<=24;++seed){
        auto s=base();s.riffSeed=seed;s.density=.35f;s.randomGenre=genre;s.section=fgg::Section::intro;
        check(guitarMaxOn(fgg::GrooveEngine::renderClip(s,8))>=68,"funky and jackin intros are more than a ghost scratch");
    }
    auto s=guitarOnly({{fgg::Lane::chank,"X.x. .x.x X.x. .x.x"},{fgg::Lane::scratch,"g... g... g... g..."}});
    s.accentGroove=.85f;s.velocity=100;s.humanize=0;int accent=0,weak=127,ghost=127;
    for(const auto& e:fgg::GrooveEngine::renderClip(s,4)){
        if(!e.isNoteOn())continue;const int step=(int)std::llround(e.ppq/fgg::stepLength);
        if(e.lane==(int)fgg::Lane::scratch)ghost=std::min(ghost,e.velocity);else if(step%4==0)accent=std::max(accent,e.velocity);else if(step%4==1)weak=std::min(weak,e.velocity);
    }
    check(accent>=85&&weak<=accent-8&&ghost<=weak-8,"guitar accent stays above a weak 16th and a ghost stays quieter");
}
int countHits(const std::string& line){int n=0;for(char c:line)n+=fgg::isHit(c)?1:0;return n;}
bool barsDiffer(const fgg::GuitarPattern& p){for(const auto& line:p.lanes)if(line.size()>=32&&line.compare(0,16,line,16,16)!=0)return true;return false;}
bool hasSymbol(const fgg::GuitarPattern& p,char symbol,int lane=-1){for(int l=0;l<fgg::numLanes;++l){if(lane>=0&&l!=lane)continue;if(p.lanes[(size_t)l].find(symbol)!=std::string::npos)return true;}return false;}
int countOns(const std::vector<fgg::NoteEvent>& events){int n=0;for(const auto& e:events)n+=e.isNoteOn()?1:0;return n;}
void verifyGenreIdentity(){
    auto barOf=[](const fgg::GuitarPattern& p,int bar){std::string s;for(const auto& line:p.lanes){s+=line.size()>=(size_t)(bar*16+16)?line.substr((size_t)bar*16,16):line;s+='|';}return s;};
    auto gridOf=[](const fgg::GuitarPattern& p){std::string s;for(const auto& line:p.lanes){s+=line;s+='|';}return s;};
    auto hitsOn=[](const std::string& line,int pred){int n=0;for(int i=0;i<(int)line.size();++i)if(fgg::isHit(line[(size_t)i])&&(pred<0||i%4==pred))++n;return n;};
    for(int genre=0;genre<13;++genre){
        const bool extended=[&]{
            const auto chords=fgg::progressionChords(fgg::genreProgression(genre),0);
            const auto voiced=fgg::voiceChord(fgg::tonesOf(chords[0]),nullptr,true,fgg::guitarColourFor(genre));
            const auto& shape=voiced.stab.empty()?voiced.full:voiced.stab;
            for(int i=0;i<shape.size();++i){const int iv=((shape[i]-chords[0].root)%12+12)%12;if(iv==1||iv==2||iv==6||iv==9||iv==10||iv==11)return true;}
            return false;
        }();
        if(genre==1||genre==2||genre==3||genre==10||genre==11)check(extended,"extended guitar colour belongs to soulful, deep, gospel, neo-soul and chill");
        for(float density:{(genre==8?0.68f:genre==4?0.22f:0.35f),0.5f}){
            std::set<std::string> grids,bar1s,bar2s;int differ=0,regen=0,faithful=0;
            for(std::uint32_t seed=1;seed<=20;++seed){
                const auto p=fgg::makeRandomPattern(seed,density,genre);
                grids.insert(gridOf(p));bar1s.insert(barOf(p,0));bar2s.insert(barOf(p,1));
                differ+=barsDiffer(p)?1:0;
                regen+=gridOf(p)!=gridOf(fgg::makeSectionVariation(p,seed*97u+11u))?1:0;
                const int stab=countHits(p[fgg::Lane::stab]);
                const int stabOff=hitsOn(p[fgg::Lane::stab],2);
                int odd=0,total=0;for(const auto& line:p.lanes)for(size_t i=0;i<line.size();++i){total+=fgg::isHit(line[i])?1:0;odd+=fgg::isHit(line[i])&&i%2==1?1:0;}
                const bool accent=hasSymbol(p,'X');
                const bool skip=[&]{std::string any(32,'.');for(int l=0;l<fgg::numLanes;++l)if(l!=(int)fgg::Lane::scratch)for(int i=0;i<32&&i<(int)p.lanes[(size_t)l].size();++i)if(fgg::isHit(p.lanes[(size_t)l][(size_t)i]))any[(size_t)i]='x';int gaps=0;for(int bar=0;bar<2;++bar){int prev=-4;for(int i=0;i<16;++i)if(any[(size_t)(bar*16+i)]!='.'){if(i-prev>=3)++gaps;prev=i;}}return gaps>=2;}();
                bool ok=false;
                switch(genre){
                    case 0: case 5: case 9: ok=stab>=3&&stabOff*100>=stab*70&&countHits(p[fgg::Lane::power])==0;break;
                    case 4: case 8: ok=(countHits(p[fgg::Lane::chank])>0||countHits(p[fgg::Lane::scratch])>0)&&odd>0&&accent;break;
                    case 6: ok=skip&&total<=20;break;
                    case 1: case 2: ok=total<=18&&extended;break;
                    case 7: ok=countHits(p[fgg::Lane::arp])+countHits(p[fgg::Lane::chank])>0&&odd+hitsOn(p[fgg::Lane::arp],2)+hitsOn(p[fgg::Lane::chank],2)>0;break;
                    case 10: case 11: ok=countHits(p[fgg::Lane::lick])>0&&total<=22&&extended;break;
                    case 12: ok=stab>=3&&countHits(p[fgg::Lane::power])==0&&countHits(p[fgg::Lane::scratch])==0&&odd==0;break;
                    case 3: ok=odd==0&&countHits(p[fgg::Lane::strumDown])+countHits(p[fgg::Lane::strumUp])>=4&&countHits(p[fgg::Lane::scratch])==0&&extended;break;
                    default: ok=true;break;
                }
                faithful+=ok?1:0;
                fgg::GuitarSettings s;s.pattern=fgg::randomPatternIndex();s.riffSeed=seed;s.density=density;s.randomGenre=genre;s.swing=fgg::genreSwing(genre);s.fillEveryBars=0;s.variationEvery=0;s.humanize=.2f;s.accentGroove=.85f;
                check(countOns(fgg::GrooveEngine::renderClip(s,8.0))>0,"genre guitar groove plays");
            }
            check(differ==20,"each genre's two guitar bars answer each other");
            check((int)grids.size()>=17&&(int)bar1s.size()>=16&&(int)bar2s.size()>=16,"a genre draws many guitar rhythms");
            check(regen>=17,"regenerating a guitar section leaves the genre and changes the bar");
            check(faithful>=16,"random guitar stays on that genre's rhythm");
        }
    }
    check(std::abs(fgd::genreSwing(4)-0.72f)<1e-4f,"jackin shuffle is 62 percent");
    for(int genre=0;genre<13;++genre){
        std::set<std::string> beats;
        for(std::uint32_t seed=1;seed<=16;++seed){
            const auto a=fgd::makeRandomPattern(seed,.55f,genre);
            const auto b=fgd::makeRandomPattern(seed,.55f,genre);
            check(a.lanes==b.lanes,"a drum seed stays put");
            std::string grid;for(const auto& line:a.lanes)grid+=line;
            beats.insert(grid);
            if(genre==4){
                const auto& open=a[fgd::Lane::openHat];const auto& closed=a[fgd::Lane::closedHat];
                bool off=true,accent=true;
                for(int i=0;i<(int)open.size();++i){if(i%4==2)off=off&&open[(size_t)i]=='x';if(i%4==3)accent=accent&&(closed[(size_t)i]=='x'||closed[(size_t)i]=='X'||closed[(size_t)i]=='f');}
                check(off&&accent,"jackin keeps the offbeat open hat and the swung closed accent");
            }
        }
        check((int)beats.size()>=8,"each drum genre draws more than a couple of beats");
    }
    check(fgd::makeRandomPattern(9,.5f,0).lanes!=fgd::makeRandomPattern(9,.5f,4).lanes,"house and jackin drums are not the same beat");
    const auto classic=sgk::makeRandomPattern(4242,.55f);
    for(int genre:{0,9,12})check(sgk::makeRandomPattern(4242,.55f,genre).rightHand==classic.rightHand,"house-family piano keeps the classic figure");
    std::set<std::string> hands;
    for(int genre:{1,2,3,4,5,6,7,8,10,11}){
        const auto figure=sgk::makeRandomPattern(4242,.55f,genre);
        check(figure.rightHand!=classic.rightHand,"a non-house piano genre keeps its own riff");
        hands.insert(figure.rightHand);
        std::set<std::string> many;
        for(std::uint32_t seed:{11u,29u,47u,83u,149u,211u,307u,401u})many.insert(sgk::makeRandomPattern(seed,.55f,genre).rightHand);
        check((int)many.size()>=5,"one piano genre still draws many riffs");
    }
    check((int)hands.size()>=8,"piano genres do not share one riff");
    const auto gospel=sgk::randomProgression(17,8,1,3),plain=sgk::randomProgression(17,8,1);
    check(gospel!=plain&&gospel.size()==8,"gospel walks through the next chord");
    check(sgk::randomProgression(17,4,1,4)==sgk::randomProgression(17,4,1),"other genres keep the classic progression dice");
    auto stabs=fga::newProject(1);stabs.sections={{fga::Section::main,8,"Main"}};stabs.settings.stringsMode=(int)sgk::StringsMode::stabs;stabs.settings.humanize=0;stabs.settings.swing=0;stabs.normalise();
    for(int ti:{0,2}){auto& eng=stabs.tracks[(size_t)ti].engine;eng.realism=false;eng.humanize=0;eng.swing=0;eng.fillEvery=0;eng.variationEvery=0;eng.evolveBars=0;eng.movement=0;eng.pattern=-1;}
    fga::generate(stabs,fga::Scope::song,0,0,11);const auto firstStrings=stabs.tracks[2].clips[0].notes;
    fga::generate(stabs,fga::Scope::track,2,0,83);
    check(!stabs.tracks[2].clips[0].notes.empty()&&stabs.tracks[2].clips[0].notes!=firstStrings,"a new riff still rewrites the string stabs");
}
}
#include "ClubArrangementChecks.h"
#include "TimelineChecks.h"
#include "RegenerationChecks.h"
#include "ArrangerFixChecks.h"
// Engine integration: the vendored generators' seed and genre fixes are live, and the
// arranger's note extraction never leaves millisecond stubs or flams in pitched tracks.
void verifyEngineIntegration(){
    for(int genre=0;genre<13;++genre){
        int same=0;for(std::uint32_t seed=1;seed<=40;++seed){
            const auto a=fgg::makeRandomPattern(seed,.6f,genre),b=fgg::makeRandomPattern(seed+1,.6f,genre);
            bool equal=true;for(int l=0;l<fgg::numLanes;++l)equal=equal&&a.lanes[(size_t)l]==b.lanes[(size_t)l];same+=equal?1:0;}
        check(same<=2,"neighbouring guitar seeds give distinct grooves");
    }
    for(int genre:{1,2,3,4,5,6,7,8,10,11}){
        int same=0;for(std::uint32_t seed=1;seed<=24;++seed){const auto a=sgk::makeRandomPattern(seed,.6f,genre),b=sgk::makeRandomPattern(seed+64,.6f,genre);same+=a.rightHand==b.rightHand&&a.leftHand==b.leftHand?1:0;}
        check(same<=2,"piano genre riffs do not repeat every 64 seeds");
    }
    for(int genre=0;genre<13;++genre)for(std::uint32_t seed:{3U,77U}){
        auto p=fga::newProject(genre);fga::generate(p,fga::Scope::song,0,0,seed);
        for(int ti=0;ti<4;++ti)for(size_t ci=0;ci<p.tracks[(size_t)ti].clips.size();++ci){const auto& notes=p.tracks[(size_t)ti].clips[ci].notes;
            for(const auto& n:notes)check(n.length>=1./128.,"no millisecond note stubs in piano, bass, strings or guitar");
            if(ti==3)for(size_t k=1;k<notes.size();++k)for(size_t j=k;j-->0&&notes[k].start-notes[j].start<1./32.;)check(notes[j].pitch!=notes[k].pitch,"a guitar unison is one attack, not a flam");}
    }
}
void verifyLeadIntegration(){
    using namespace fga;
    check(leadTrack==17&&trackAtRow(17)==leadTrack&&rowForTrack(leadTrack)==17,"Lead keeps all legacy track IDs and musical rows");
    {
        fgl::LeadSettings s;s.pattern=fgl::userPatternIndex();s.humanize=s.swing=s.attackMs=0;s.pushChanges=false;s.resolveAfterFill=false;
        s.numChords=2;s.chords[0]={0,(int)fgl::ChordType::major};s.chords[1]={7,(int)fgl::ChordType::major};
        s.user[(size_t)fgl::Lane::root][0]='X';s.user[(size_t)fgl::Lane::root][8]='X';s.stepsPerChord=8;
        std::vector<int> half;for(auto event:fgl::GrooveEngine::renderClip(s,4))if(event.isNoteOn())half.push_back(event.note%12);
        check(half==std::vector<int>({0,7}),"Lead changes harmony after half a bar");
        s.stepsPerChord=16;std::vector<int> bar;for(auto event:fgl::GrooveEngine::renderClip(s,4))if(event.isNoteOn())bar.push_back(event.note%12);
        check(bar==std::vector<int>({0,0}),"Lead retains one-bar harmony when requested");
    }
    for(int genre=0;genre<13;++genre){
        auto p=newProject(genre);p.sections={{Section::main,8,"Main"}};p.normalise();generate(p,Scope::track,leadTrack,0,0);
        const auto original=p;const auto& notes=p.tracks[leadTrack].clips[0].notes;
        check(!notes.empty(),"Every family genre produces a lead melody");
        double off=0;for(const auto& n:notes){check(n.start+1e-8>=off,"Lead remains monophonic in Arranger clips");check(n.pitch>=0&&n.pitch<=127&&n.velocity>0&&n.velocity<=127,"Lead notes are valid MIDI");check(n.lane>=0&&n.lane<12,"Lead preserves melodic lane IDs");off=n.start+n.length;}
        generate(p,Scope::track,leadTrack,0,0);check(p.tracks[leadTrack].clips==original.tracks[leadTrack].clips,"Lead take is repeatable");
        generate(p,Scope::track,leadTrack,0,719u);
        for(int t=0;t<legacyTrackCount;++t)check(p.tracks[(size_t)t].clips==original.tracks[(size_t)t].clips,"Lead regeneration leaves the other instruments unchanged");
        check(p.tracks[leadTrack].clips!=original.tracks[leadTrack].clips,"Lead regeneration writes a new melodic take");
        auto locked=p;locked.tracks[leadTrack].locked=true;generate(locked,Scope::track,leadTrack,0,123u);check(locked.tracks[leadTrack].clips==p.tracks[leadTrack].clips,"Lead track lock survives regeneration");
        p.tracks[leadTrack].clips[0].locked=true;const auto before=p.tracks[leadTrack].clips;generate(p,Scope::clip,leadTrack,0,923u);check(p.tracks[leadTrack].clips==before,"Lead clip lock survives regeneration");
        p=original;p.settings.key=(p.settings.key+3)%12;applyInstrumentChange(p,0,true);check(p.tracks[leadTrack].clips!=original.tracks[leadTrack].clips,"Shared harmony edits re-render Lead");
        p=original;for(auto& mute:p.tracks[leadTrack].engine.laneMute)mute=true;applyInstrumentChange(p,leadTrack,false);check(p.tracks[leadTrack].clips[0].notes.empty(),"Lead lane mutes reach the generator");
        p=original;p.tracks[leadTrack].engine.laneVolume.fill(.15);applyInstrumentChange(p,leadTrack,false);check(p.tracks[leadTrack].clips==original.tracks[leadTrack].clips,"Lead audio faders do not change MIDI dynamics");
        for(int duration:{8,16,32,64}){p=original;p.settings.chordSteps=duration;applyInstrumentChange(p,leadTrack,true);check(!p.tracks[leadTrack].clips[0].notes.empty(),"Lead supports every shared harmonic duration");}
    }
}
void verifyAuditFixes(){
    fga::Settings s;s.key=9;s.scale=0;s.extension=1;s.chordCount=1;s.degrees[0]=2;s.freeChords[0]=-1;
    auto names=fga::chordNames(s);check(!names.empty()&&names[0].find("C#")!=std::string::npos&&names[0].find("Db")==std::string::npos,"A major spells iii as C#");
    check(std::string(sgk::spelledNote(6,5,0))=="Gb","F major spells Gb");
    auto altered=sgk::buildChord(11,8,4,sgk::Extension::alteredDominant);check(altered.seventh!=10,"Locrian degree 5 is not forced to a dominant seventh");
    auto sus=sgk::buildChord(0,0,0,sgk::Extension::sus);check(sus.suspended,"C major sus replaces the third");
    fga::Clip clip;clip.notes={{0,1,120,100,0},{1,1,60,100,0}};fga::transpose(clip,12);check(clip.notes[0].pitch==120&&clip.notes[1].pitch==60,"transpose rejects the whole selection at the MIDI edge");
    fga::transpose(clip,-12);check(clip.notes[0].pitch==108&&clip.notes[1].pitch==48,"transpose applies when every note stays in range");
}
int main(int argc,char** argv){try{
    verifyAuditFixes();
    verifyLeadIntegration();verifyEngineIntegration();fixchecks::verifyUserReportedFixes();verifyRegeneration();verifyTimelineEditing();verifyClubEdgeCases();verifyClubSeedCases();verifyGuitarAudibility();verifyGenreIdentity();
    {
        auto vocal=fga::newProject(4);vocal.sections={{fga::Section::main,16,"Main"},{fga::Section::drop,8,"Drop"},{fga::Section::intro,8,"Intro"}};vocal.normalise();
        const auto lead=vocal.tracks[fga::leadTrack].clips;const int manual=vocal.addMidiTrack();vocal.tracks[(size_t)manual].clips[0].notes={{1,.5,72,100,0}};
        const int index=vocal.addVocalChopsTrack();check(index==fga::trackCount+1&&vocal.tracks[fga::leadTrack].clips==lead,"vocal part preserves lead and manual routing");
        auto& t=vocal.tracks[(size_t)index];check(t.vocalChops&&fga::hasGenerator(t)&&!t.freeTimeline,"optional vocal generator follows sections");
        check(t.clips[0].notes.size()>=4&&t.clips[0].notes.size()<=12,"default vocal leaves space: sparse four-bar calls");
        for(const auto& c:t.clips)for(size_t n=0;n<c.notes.size();++n){check(c.notes[n].length<=.75&&c.notes[n].length>0,"short vocal gates");if(n)check(c.notes[n-1].start+c.notes[n-1].length<=c.notes[n].start,"vocal remains monophonic");}
        const auto original=t.clips;fga::generate(vocal,fga::Scope::track,index,0,0);check(t.clips==original,"vocal APPLY is deterministic");
        fga::generate(vocal,fga::Scope::clip,index,1,4321);check(t.clips[0]==original[0]&&t.clips[2]==original[2],"clip regeneration preserves other vocal sections");
        std::set<std::vector<double>> rhythms;for(unsigned seed=2;seed<14;++seed){fga::generate(vocal,fga::Scope::track,index,0,seed);std::vector<double> onsets;for(const auto& n:t.clips[0].notes)onsets.push_back(n.start);rhythms.insert(onsets);}check(rhythms.size()>3,"vocal regeneration draws different sparse rhythms");
        t.clips[0].locked=true;const auto locked=t.clips[0];fga::generate(vocal,fga::Scope::track,index,0,999);check(t.clips[0]==locked,"vocal clip lock survives generation");t.clips[0].locked=false;
        t.instrument="sample";t.sampler=fga::vocalSamplerSettings();fga::generate(vocal,fga::Scope::track,index,0,300);
        for(const auto& c:t.clips)for(const auto& n:c.notes)check(n.pitch>=60&&n.pitch<68,"vocal triggers actual slice keys");
        const auto old=t.sampler;auto before=t.clips[0].notes;t.clips[0].loopSource=before;t.sampler.sliceBase=124;t.sampler.sliceCount=3;fga::remapVocalChops(vocal,index,old);
        for(size_t n=0;n<before.size();++n){const auto& mapped=t.clips[0].notes[n];check(mapped.pitch>=124&&mapped.pitch<=126&&mapped.start==before[n].start&&mapped.length==before[n].length&&mapped.velocity==before[n].velocity,"changing slices keeps timing, duration and velocity");check(t.clips[0].loopSource[n].pitch==mapped.pitch,"loop source follows slice mapping");}
        t.engine.density=0;fga::generate(vocal,fga::Scope::track,index,0,0);check(t.clips[0].notes.empty(),"zero vocal density writes no notes");
        const auto identity=t.sessionId;check(vocal.deleteMidiTrack(manual)&&vocal.tracks[(size_t)manual].vocalChops&&vocal.tracks[(size_t)manual].sessionId==identity,"vocal generator follows routing compaction");
        check(vocal.deleteMidiTrack(manual),"optional vocal track can be removed");
        auto split=fga::newProject(4);split.sections={{fga::Section::main,4,"A"},{fga::Section::drop,4,"B"},{fga::Section::outro,4,"C"}};split.normalise();const int vc=split.addVocalChopsTrack();
        for(int ti:{0,vc}){auto& part=split.tracks[(size_t)ti];part.freeTimeline=true;fga::Clip c;c.span=48;c.notes={{5,.25,60,100,0},{20,.25,62,100,0},{40,.25,64,100,0}};part.clips={c};}
        fga::generate(split,fga::Scope::section,vc,1,221);for(int ti:{0,vc}){const auto& clips=split.tracks[(size_t)ti].clips;check(clips.size()==3&&clips[0].notes.size()==1&&clips[0].notes[0].start==5&&clips[2].notes.size()==1&&clips[2].notes[0].start==8,"section boundary splits retain outer notes without dangling track references");}
    }
    {
        auto p=fga::newProject();p.sections={{fga::Section::main,4,"A"},{fga::Section::drop,4,"B"}};p.normalise();
        for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();
        p.tracks[4].clips[0].notes={{0,.25,36,100,0},{8,2,36,70,0}};p.tracks[4].clips[1].notes={{1,.25,36,50,0}};p.normalise();
        const auto original=p.tracks[4].clips[0];
        check(p.resizeClipLoop(4,0,8),"shorten one clip");check(p.clipLength(4,0)==8&&p.sections[0].bars==4&&p.clipLength(5,0)==16,"clip duration independent of section and other tracks");
        check(p.resizeClipLoop(4,0,24),"extend one loop");check(p.tracks[4].clips[0].notes.size()==3&&p.tracks[4].clips[0].notes[1].start==8&&p.tracks[4].clips[0].notes[1].velocity==70&&p.tracks[4].clips[0].notes[2].start==16,"loop source survives shortening, repeats without velocity or timing changes");
        auto events=fga::events(p,4);bool hidden=false,repeated=false;for(auto e:events)if(e.velocity>0&&e.controller<0){hidden=hidden||e.beat==17;repeated=repeated||e.beat==16;}check(!hidden&&repeated,"extended loop masks covered MIDI instead of doubling notes");
        check(p.resizeClipLoop(4,0,4)&&p.resizeClipLoop(4,0,16)&&p.tracks[4].clips[0].notes==original.notes,"non-destructive shorten and restore");
        auto editedShort=p;check(editedShort.resizeClipLoop(4,0,4),"shortened loop for editing");editedShort.tracks[4].clips[0].notes[0].velocity=37;
        check(editedShort.resizeClipLoop(4,0,24)&&editedShort.tracks[4].clips[0].notes[0].velocity==37&&editedShort.tracks[4].clips[0].notes[1].start==8&&editedShort.tracks[4].clips[0].notes[2].velocity==37,"editing shortened loop retains hidden source notes");
        p.tracks[4].clips[0].notes[0].velocity=42;check(p.resizeClipLoop(4,0,24)&&p.tracks[4].clips[0].notes[2].velocity==42,"edited MIDI becomes the new loop source");
        check(p.resizeClipLoop(4,1,24)&&p.length()==40,"clip can extend beyond last section and exports include its tail");
        p.tracks[4].clips[1].span=0;p.normalise();check(p.resizeSectionLoop(0,6)&&p.sections[0].bars==6&&p.sectionStart(1)==24,"section edge moves every track and later markers");
        for(int ti=0;ti<fga::trackCount;++ti)check(p.clipLength(ti,0)==24,"section resize updates all track spans");
        check(p.tracks[4].clips[0].notes.size()==3,"section extension repeats MIDI");
        check(p.resizeSectionLoop(0,1)&&p.resizeSectionLoop(0,6)&&p.tracks[4].clips[0].notes.size()==3,"section shrink and re-extend preserves hidden MIDI");
        check(!p.resizeClipLoop(-1,0,16)&&!p.resizeClipLoop(0,0,std::numeric_limits<double>::quiet_NaN()),"reject invalid loop resize");
    }

    {auto p=fga::newProject(1);p.sections={{fga::Section::main,4,"Main"}};p.normalise();auto& e=p.tracks[0].engine;e.pattern=sgk::userPatternIndex();e.userRight.fill('.');e.userLeft.fill('.');e.userRight[0]='x';e.userVelRight[0]=37;e.realism=false;e.humanize=0;e.swing=0;e.strum=0;e.fillEvery=0;e.variationEvery=0;fga::generate(p,fga::Scope::track,0);check(!p.tracks[0].clips[0].notes.empty(),"custom piano grid generates notes");auto before=p.tracks[0].clips[0].notes;e.userVelRight[0]=120;fga::generate(p,fga::Scope::track,0);check(p.tracks[0].clips[0].notes[0].velocity>before[0].velocity,"per-step piano velocity reaches engine");e.inversions[0]=2;fga::generate(p,fga::Scope::track,0);check(p.tracks[0].clips[0].notes!=before,"per-chord inversion reaches engine");e.pedalCC=true;auto events=fga::events(p,0);bool pedal=false,expression=false;for(auto event:events){pedal=pedal||event.controller==64;expression=expression||event.controller==11;}check(pedal&&expression,"pedal and expression controller events");auto original=p;fga::applyPianoIntention(p,0,1,true,true,123);check(p.settings.degrees==original.settings.degrees&&p.tracks[0].engine.userRight==original.tracks[0].engine.userRight,"DNA rhythm and harmony locks");}
    {auto p=fga::newProject(1);auto& e=p.tracks[0].engine;e.pattern=-1;e.riffSeed=123;auto a=fga::pianoEngineSettings(p,0,55);auto patternA=sgk::GrooveEngine::basePattern(a);e.riffSeed=9876;auto b=fga::pianoEngineSettings(p,0,55);auto patternB=sgk::GrooveEngine::basePattern(b);check(patternA.rightHand!=patternB.rightHand||patternA.leftHand!=patternB.leftHand,"riff seed changes the automatic piano figure");check(a.humanizeSeed==b.humanizeSeed,"riff and humanize seeds are independent");}
    {
        auto attacks=[&](const fga::Clip& clip){std::string mask(32,'.');for(const auto& n:clip.notes){const int step=(int)std::lround(n.start*4.0);if(step>=0&&step<32)mask[(size_t)step]='x';}return mask;};
        auto hitsDiffer=[&](const std::string& a,const std::string& b){int d=0;const auto n=std::min(a.size(),b.size());for(size_t i=0;i<n;++i)d+=a[i]!=b[i];return d;};
        for(int genre:{0,4,8,11}){
            std::set<std::string> figures,piano,stabs;std::string firstFigure,firstPiano,firstStabs;int farFigure=0,farPiano=0,farStabs=0;
            for(unsigned seed:{11u,29u,47u,83u,149u,211u}){
                auto song=fga::newProject(genre);song.sections={{fga::Section::main,8,"Main"}};song.settings.stringsMode=(int)sgk::StringsMode::stabs;song.settings.humanize=0;song.settings.swing=0;song.normalise();
                for(int ti:{0,2}){auto& eng=song.tracks[(size_t)ti].engine;eng.realism=false;eng.humanize=0;eng.swing=0;eng.fillEvery=0;eng.variationEvery=0;eng.evolveBars=0;eng.movement=0;eng.pattern=-1;}
                fga::generate(song,fga::Scope::song,0,0,seed);
                const auto figure=sgk::GrooveEngine::basePattern(fga::pianoEngineSettings(song,0,song.tracks[0].clips[0].seed,song.tracks[0].clips[0].motifSeed)).rightHand;
                const auto pianoMask=attacks(song.tracks[0].clips[0]);const auto stabMask=attacks(song.tracks[2].clips[0]);
                if(figures.empty()){firstFigure=figure;firstPiano=pianoMask;firstStabs=stabMask;}
                else{farFigure=std::max(farFigure,hitsDiffer(firstFigure,figure));farPiano=std::max(farPiano,hitsDiffer(firstPiano,pianoMask));farStabs=std::max(farStabs,hitsDiffer(firstStabs,stabMask));}
                figures.insert(figure);piano.insert(pianoMask);stabs.insert(stabMask);
            }
            check(figures.size()>=5&&farFigure>=4,"regenerating piano writes a clearly different classic figure");
            check(piano.size()>=4&&farPiano>=4,"regenerated piano onsets differ by more than a ghost");
            check(stabs.size()>=4&&farStabs>=4&&stabs.count(std::string(32,'.'))==0,"regenerated string stabs follow a clearly different figure");
        }
        std::string house;
        for(int genre:{0,9,12}){
            auto song=fga::newProject(genre);song.settings.density=.55;song.tracks[0].engine.pattern=-1;song.tracks[0].engine.riffSeed=4242;song.tracks[0].engine.density=.55;
            const auto figure=sgk::GrooveEngine::basePattern(fga::pianoEngineSettings(song,0,1));
            if(house.empty())house=figure.rightHand+figure.leftHand;
            check(figure.rightHand+figure.leftHand==house,"house, nineties and dance keep one classic piano figure");
        }
        std::set<std::string> others;
        for(int genre:{1,3,4,8,11}){
            auto song=fga::newProject(genre);song.settings.density=.55;song.tracks[0].engine.pattern=-1;song.tracks[0].engine.riffSeed=4242;song.tracks[0].engine.density=.55;
            const auto figure=sgk::GrooveEngine::basePattern(fga::pianoEngineSettings(song,0,1));
            const auto hand=figure.rightHand+figure.leftHand;
            check(hand!=house,"the song genre writes the piano figure outside the house family");
            others.insert(hand);
        }
        check((int)others.size()>=4,"soulful, gospel, jackin, funk and chill piano riffs stay apart");
    }
    for(int genre=0;genre<13;++genre){
        auto p=fga::newProject(genre);auto original=p;verifyClubProject(p,genre,argc>1?argv[1]:"");
        {auto slow=p;slow.settings.bpm=80;fga::arrangeSong(slow);check(slow.length()==p.length(),"tempo does not stretch a musical form to minutes");auto developing=p;developing.tracks[0].engine.grooveBars=4;fga::arrangeSong(developing);check(developing.length()>p.length(),"longer groove develops over more complete phrases");}
        if(genre==1){auto sparse=p,busy=p;sparse.tracks[0].engine.density=0;busy.tracks[0].engine.density=1;fga::arrangeSong(sparse);fga::arrangeSong(busy);check(sparse.length()>busy.length(),"sparse and busy grooves have different phrase development");auto lead=p;lead.settings.guitarLead=true;fga::generate(lead,fga::Scope::track,0);check(lead.tracks[0].clips!=p.tracks[0].clips,"original guitar lead piano answers");for(int ti=1;ti<fga::trackCount;++ti)check(lead.tracks[(size_t)ti].clips==p.tracks[(size_t)ti].clips,"lead piano generation isolation");auto shifted=p;shifted.tracks[4].engine.midiNote=50;fga::generate(shifted,fga::Scope::track,4);for(const auto& c:shifted.tracks[4].clips)for(auto n:c.notes)check(n.pitch==50,"custom drum MIDI mapping");}
        check(p.tracks.size()==fga::trackCount,"18 instrument tracks");check(p.sections.size()>=6,"genre-specific song stages");
        check(p.length()*60/p.settings.bpm>=240,"full genre form includes intro and outro");
        check(p.length()*60/p.settings.bpm<480,"genre forms remain musically bounded");
        check(p.sections.front().type==fga::Section::intro&&p.sections.back().type==fga::Section::outro,"intro and outro belong to form");
        check(fga::trackAtRow(0)==4&&fga::trackAtRow(13)==1&&fga::trackAtRow(14)==0&&fga::trackAtRow(16)==3,"musical track order");
        check(p.length()>0,"song duration");check(fga::chordNames(p.settings).size()==(size_t)p.settings.chordCount,"harmony");
        for(size_t ti=0;ti<p.tracks.size();++ti)for(size_t si=0;si<p.sections.size();++si)for(auto n:p.tracks[ti].clips[si].notes){check(n.start>=0&&n.start+n.length<=p.sections[si].bars*4.+1e-6,"section bounds");check(n.length>0&&n.pitch>=0&&n.pitch<128&&n.velocity>0&&n.velocity<128,"valid MIDI");}
        fga::generate(p);for(size_t ti=0;ti<p.tracks.size();++ti)check(p.tracks[ti].clips==original.tracks[ti].clips,"repeatable stored seeds");
        fga::generate(p,fga::Scope::clip,0,4,9876);
        bool changed=false;
        for(size_t ti=0;ti<p.tracks.size();++ti)for(size_t si=0;si<p.sections.size();++si){if(ti==0&&si==4)changed=!(p.tracks[ti].clips[si]==original.tracks[ti].clips[si]);else check(p.tracks[ti].clips[si]==original.tracks[ti].clips[si],"clip regeneration isolation");}
        check(changed,"clip changed");
        p=original;p.tracks[0].locked=true;p.tracks[4].clips[4].locked=true;fga::generate(p,fga::Scope::song,0,0,12345);
        check(p.tracks[0].clips==original.tracks[0].clips,"track lock");check(p.tracks[4].clips[4].notes==original.tracks[4].clips[4].notes&&p.tracks[4].clips[4].seed==original.tracks[4].clips[4].seed,"clip lock");
        p=original;fga::generate(p,fga::Scope::track,3,0,1357);
        for(int ti=0;ti<fga::trackCount;++ti)if(ti!=3)check(p.tracks[(size_t)ti].clips==original.tracks[(size_t)ti].clips,"track scope");
        p=original;fga::generate(p,fga::Scope::section,0,3,2468);
        for(int ti=0;ti<fga::trackCount;++ti)for(size_t si=0;si<p.sections.size();++si)if(si!=3)check(p.tracks[(size_t)ti].clips[si]==original.tracks[(size_t)ti].clips[si],"section scope");
        p=original;p.insertSection(2,p.sections[1],1);check(p.tracks[0].clips[2]==original.tracks[0].clips[1],"duplicate MIDI");p.removeSection(2);check(p.length()==original.length(),"remove section duration");p.moveSection(1,4);check(p.tracks[0].clips[4]==original.tracks[0].clips[1],"move section MIDI");
        auto midi=fga::midiFile(original);check(midi.size()>500&&std::string(midi.begin(),midi.begin()+4)=="MThd","MIDI header");check(midi[11]==fga::trackCount+1,"conductor and 18 tracks");
        auto stem=fga::midiFile(original,4);check(stem[11]==2,"single stem MIDI");
        if(argc>1&&genre==1){auto pedal=original;pedal.tracks[0].engine.pedalCC=true;auto cc=fga::midiFile(pedal,0);std::ofstream ccFile(std::string(argv[1])+"/test-piano-cc.mid",std::ios::binary);ccFile.write((const char*)cc.data(),(std::streamsize)cc.size());std::ofstream f(std::string(argv[1])+"/test-song.mid",std::ios::binary);f.write((const char*)midi.data(),(std::streamsize)midi.size());std::ofstream g(std::string(argv[1])+"/test-kick.mid",std::ios::binary);g.write((const char*)stem.data(),(std::streamsize)stem.size());}
        std::cout<<"PASS "<<fga::genres()[(size_t)genre]<<" / "<<(int)(p.length()*60/p.settings.bpm)/60<<":"<<(int)(p.length()*60/p.settings.bpm)%60<<" / "<<p.length()/4<<" bars\n";
    }
    fga::Clip c; c.notes={{.13,.5,60,96,0},{.3,.2,127,100,0}};fga::quantise(c,.25);check(c.notes[0].start==.25&&c.notes[0].length==.5,"quantise");fga::transpose(c,12);check(c.notes[0].pitch==60&&c.notes[1].pitch==127,"transpose rejects when one note would leave MIDI");
    fga::Clip held;held.notes={{.13,.5,60,80,0}};fga::quantise(held,.25,0);check(held.notes[0].start==.13,"quantise strength zero keeps the note");
    fga::Clip inside;inside.notes={{3.9,.2,60,90,0}};fga::quantise(inside,1,1,4);fga::tidyNotes(inside,4);check(inside.notes.size()==1&&inside.notes[0].start<4,"quantise stays inside the clip");
    fga::Clip plain;plain.notes={{1,.5,60,80,0}};auto untouched=plain;fga::humanize(plain,0,99,4);check(plain.notes==untouched.notes,"humanize zero is a no-op");
    fga::humanize(plain,1,99,4);check(plain.notes[0].velocity>=1&&plain.notes[0].velocity<=127&&plain.notes[0].start>=0&&plain.notes[0].start+plain.notes[0].length<=4,"humanize stays inside the clip");
    auto repeated=untouched;fga::humanize(repeated,1,99,4);check(repeated.notes==plain.notes&&repeated.notes!=untouched.notes,"humanize repeats from the seed and changes the note");
    {auto song=fga::newProject(1);song.sections={{fga::Section::main,4,"A"},{fga::Section::main,4,"B"}};song.normalise();song.tracks[0].clips[0].notes={{0,1,60,100,0}};song.tracks[0].clips[1].notes={{1,1,62,90,0}};check(song.duplicateSection(0),"duplicate section");check((int)song.sections.size()==3&&song.tracks[0].clips[1].notes[0].pitch==60&&song.tracks[0].clips[2].notes[0].pitch==62&&song.sectionStart(2)==32,"duplicate copies notes and shifts the next section");}
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
    {
        // Pan, master and plugin parameter lanes: the same envelope, edited by the same section operations.
        auto p=fga::newProject();p.sections={{fga::Section::main,4,"A"},{fga::Section::drop,8,"B"}};p.normalise();
        auto& t=p.tracks[3];t.panAutomation.enabled=true;t.panAutomation.points={{0,0,0},{16,1,0}};
        check(std::abs(fga::panFromAutomation(t.panAutomation.valueAt(8)))<1e-12&&fga::panFromAutomation(t.panAutomation.valueAt(16))==1&&fga::panFromAutomation(0)==-1,"pan envelope maps 0..1 to left..right");
        fga::ParameterAutomation lane;lane.slot=1;lane.index=3;lane.name="Cutoff";lane.curve.enabled=true;lane.curve.points={{4,.25,0},{20,.75,0},{40,1.7,0}};t.parameters.push_back(lane);
        p.masterAutomation.enabled=true;p.masterAutomation.points={{4,1.5,0},{20,.5,0}};
        p.normalise();check(t.parameters[0].curve.points[2].value==1&&p.masterAutomation.points[0].value==1.5&&t.panAutomation.points[1].value==1,"plugin and pan values clamp to 0..1, the master to 0..2");
        p.insertSection(1,{fga::Section::main,4,"New"});check(t.panAutomation.points[1].beat==32&&t.parameters[0].curve.points[1].beat==36&&p.masterAutomation.points[1].beat==36,"insert shifts every automation lane");
        p.removeSection(1);check(t.panAutomation.points[1].beat==16&&t.parameters[0].curve.points[1].beat==20&&p.masterAutomation.points[1].beat==20,"remove shifts every automation lane");
        p.resizeSection(0,8);check(t.parameters[0].curve.points[0].beat==8&&p.masterAutomation.points[0].beat==8&&t.panAutomation.points[1].beat==32,"resize scales every automation lane");
        p.moveSection(0,1);check(t.parameters[0].curve.points[2].beat==40&&t.parameters[0].curve.points[2].value==.25&&p.masterAutomation.points[1].beat==40&&p.masterAutomation.points[1].value==1.5,"automation lanes follow moved sections");
        for(int k=0;k<fga::maxParameterLanes+4;++k)t.parameters.push_back(lane);p.normalise();check(t.parameters.size()==(size_t)fga::maxParameterLanes,"parameter lane capacity is bounded");
        check(!fga::VolumeAutomation{}.active()&&fga::VolumeAutomation{}.isDefault(),"default lanes are bypassed and not saved");
    }
    {
        // Random automation: only when the project asks for it, deterministic, musical, scoped and lock-aware.
        auto old=fga::newProject();old.settings.randomAutomation=false;for(auto& t:old.tracks){t.volume={};t.panAutomation={};}fga::generate(old,fga::Scope::song,0,0,4242);
        bool untouched=true;for(const auto& t:old.tracks)untouched=untouched&&t.volume.isDefault()&&t.panAutomation.isDefault();check(untouched,"no automation unless the project asks for it");
        auto p=fga::newProject();check(p.settings.randomAutomation,"new projects write automation when regenerating");
        {bool moving=false;for(const auto& t:p.tracks)moving=moving||t.volume.moves()||t.panAutomation.moves();check(moving,"a new song already has automation");}
        p.sections={{fga::Section::intro,8,"Intro"},{fga::Section::main,8,"Main"},{fga::Section::buildup,8,"Build"},{fga::Section::drop,8,"Drop"},{fga::Section::breakdown,8,"Break"},{fga::Section::outro,8,"Outro"}};p.normalise();
        fga::ParameterAutomation lane;lane.name="Cutoff";p.tracks[3].parameters.push_back(lane);
        auto a=p,b=p;fga::generate(a,fga::Scope::song,0,0,777);fga::generate(b,fga::Scope::song,0,0,777);
        bool same=true;for(int ti=0;ti<fga::trackCount;++ti){const auto& x=a.tracks[(size_t)ti];const auto& y=b.tracks[(size_t)ti];same=same&&x.volume.points.size()==y.volume.points.size()&&x.panAutomation.points.size()==y.panAutomation.points.size();
            for(size_t k=0;same&&k<x.volume.points.size();++k)same=x.volume.points[k].beat==y.volume.points[k].beat&&x.volume.points[k].value==y.volume.points[k].value;}
        check(same,"same seed, same automation");
        const double intro0=a.sectionStart(0),intro1=a.sectionStart(1),outro0=a.sectionStart(5),outro1=a.sectionStart(6);
        bool noFades=true;for(const auto& t:a.tracks){for(double x=intro0;x<intro1;x+=.5)noFades=noFades&&t.volume.valueAt(x)==1;for(double x=outro0;x<outro1;x+=.5)noFades=noFades&&t.volume.valueAt(x)==1;}
        check(noFades,"no fade-in or fade-out in the intro and the outro");
        const auto& strings=a.tracks[2].volume;const double build=a.sectionStart(2),drop=a.sectionStart(3);
        check(strings.active()&&strings.valueAt(build+1)<.95&&strings.valueAt(drop-.1)>1.05&&strings.valueAt(drop)==1,"a riser through the build-up, back to unity on the drop");
        const auto& sweep=a.tracks[3].parameters[0].curve;check(sweep.active()&&sweep.valueAt(build+1)<.4&&sweep.valueAt(drop-.1)>.75,"the plugin parameter sweeps up through the build-up");
        check(a.tracks[4].volume.isDefault()&&a.tracks[1].panAutomation.isDefault(),"kick and bass keep the floor");
        auto c=a;c.tracks[2].volume.locked=true;c.tracks[3].locked=true;const auto lockedStrings=c.tracks[2].volume.points;const auto lockedGuitar=c.tracks[3].panAutomation.points;
        fga::generate(c,fga::Scope::song,0,0,1234);check(c.tracks[2].volume.points.size()==lockedStrings.size()&&c.tracks[3].panAutomation.points.size()==lockedGuitar.size(),"locked lanes and tracks keep their automation");
        auto d=a;const auto before=d.tracks[0].volume;fga::randomiseAutomation(d,fga::Scope::section,0,2,99);
        bool outside=true;for(double x=0;x<d.length();x+=.25)if(x<build||x>=drop)outside=outside&&std::abs(d.tracks[0].volume.valueAt(x)-before.valueAt(x))<1e-9;
        check(outside,"a section's dice leave the rest of the song as it was");
        auto e=a;const auto otherPan=e.tracks[2].panAutomation.points;fga::randomiseAutomation(e,fga::Scope::track,3,0,5,1);check(e.tracks[2].panAutomation.points.size()==otherPan.size()&&e.tracks[3].volume.points.size()==a.tracks[3].volume.points.size(),"a lane's dice move only that lane");
        check(fga::midiFile(a)==fga::midiFile(b),"automation never changes the MIDI");
    }
    fga::VolumeAutomation malformedVolume;malformedVolume.enabled=true;malformedVolume.points={{4,3,2},{-1,-2,-3},{4,.3,.5},{std::numeric_limits<double>::quiet_NaN(),1,0}};malformedVolume.normalise(8);check(malformedVolume.points.size()==2&&malformedVolume.points[0].beat==0&&malformedVolume.points[0].value==0&&malformedVolume.points[1].value==.3,"sanitize sort and merge automation points");
    {
        auto p=fga::newProject();p.sections={{fga::Section::main,4,"A"},{fga::Section::drop,8,"B"}};p.normalise();
        p.loop=true;p.loopStart=16;p.loopEnd=48;p.tracks[4].volume.enabled=true;p.tracks[4].volume.points={{8,.5,0},{20,.8,0},{48,1,0}};
        p.resizeSection(0,8);check(p.loopStart==32&&p.loopEnd==64,"loop follows section resize");check(p.tracks[4].volume.points[0].beat==16&&p.tracks[4].volume.points[1].beat==36,"automation scales inside resized section and shifts after it");
        p.insertSection(0,{fga::Section::intro,4,"New"});check(p.loopStart==48&&p.loopEnd==80,"loop follows section insertion");p.removeSection(0);check(p.loopStart==32&&p.loopEnd==64,"loop follows section removal");
        p.moveSection(1,0);check(p.loopStart==0&&p.loopEnd==32,"loop follows moved complete section including its end locator");
        p.tracks[4].clips[0].notes={{0,.25,36,100,0}};p.tracks[4].clips[1].notes={{1,.25,36,70,0}};
        check(fga::transferClip(p,4,0,1,false),"clip drag swap accepted");check(p.tracks[4].clips[0].notes[0].start==1&&p.tracks[4].clips[1].notes[0].start==0,"clip swap preserves both takes");
        check(fga::transferClip(p,4,1,0,true)&&p.tracks[4].clips[0].notes==p.tracks[4].clips[1].notes,"clip copy retains source");check(!fga::transferClip(p,99,0,1,false)&&!fga::transferClip(p,4,0,99,false),"invalid clip gestures rejected");
    }
    {auto p=fga::newProject();p.loop=true;p.loopStart=0;p.loopEnd=p.length();p.moveSection(0,2);check(p.loop&&p.loopStart==0&&p.loopEnd==p.length(),"whole-song loop survives section reorder");p.insertSection(0,{fga::Section::intro,4,"Inserted"});check(p.loop&&p.loopStart==0&&p.loopEnd==p.length(),"whole-song loop includes inserted sections");}
    // Musical regression: Jackin' keeps a short, swung bass/chop pocket rather
    // than layering the same string answers and held organ over every drop.
    for(std::uint32_t seed:{1U,42U,1979U,7891U}){
        auto jack=fga::newProject(4);jack.settings.seed=seed;fga::applyGenre(jack,4);fga::generate(jack);
        check(jack.settings.bassMode==sgk::bassModeForLine(22),"Jackin has funky octave/ghost pickups rather than a sub hold");
        check(jack.settings.stringsMode==0,"Jackin factory arrangement leaves room for the chopped hook");
        int chorus=0;while(jack.sections[(size_t)chorus].type!=fga::Section::drop)++chorus;
        const auto& bass=jack.tracks[1].clips[(size_t)chorus].notes;double lengths=0;bool swung=false;int lowest=127;
        for(auto n:bass){lowest=std::min(lowest,n.pitch);lengths+=n.length;double cell=std::fmod(n.start,1.);if(std::abs(cell-(.75+jack.settings.swing/12))<.015)swung=true;}
        check(lowest>=28&&lowest<=42,"Jackin rendered bass keeps a low fundamental under the octave riff");
        check(!bass.empty()&&lengths/bass.size()<.3&&swung,"Jackin bass has articulated notes and swung sixteenth pickups");
        check(jack.tracks[2].clips[(size_t)chorus].notes.empty(),"no automatic string wash in Jackin drop");
        for(auto n:jack.tracks[3].clips[(size_t)chorus].notes)check(((int)(n.start/4)%2)==1,"Jackin percussive guitar alternates with the chopped hook");
        const auto& keys=jack.tracks[0].clips[(size_t)chorus].notes;lengths=0;for(auto n:keys)lengths+=n.length;
        check(!keys.empty()&&lengths/keys.size()<.4,"Jackin hook is chopped, not a sustained keyboard bed");
        auto custom=jack;custom.settings.stringsMode=1;fga::generate(custom,fga::Scope::track,2);
        check(!custom.tracks[2].clips[(size_t)chorus].notes.empty(),"strings can still be enabled by the musician");
    }
    {
        auto countStrings=[&](const fga::Project& p){int n=0;for(const auto& c:p.tracks[2].clips)n+=(int)c.notes.size();return n;};
        auto findSection=[&](const fga::Project& p,fga::Section type){for(int i=0;i<(int)p.sections.size();++i)if(p.sections[(size_t)i].type==type)return i;return -1;};
        auto chordFollows=[&](const fga::Clip& clip){
            std::array<int,128> first{},next{};int n0=0,n1=0;
            for(auto n:clip.notes){if(n.start<0.05){first[(size_t)n.pitch]=1;++n0;}if(std::abs(n.start-4.)<0.05){next[(size_t)n.pitch]=1;++n1;}}
            if(n0<2||n1<2)return false;
            for(int pitch=0;pitch<128;++pitch)if(first[(size_t)pitch]!=next[(size_t)pitch])return true;
            return false;
        };
        for(int genre=0;genre<13;++genre){
            auto song=fga::newProject(genre);
            check(song.tracks[2].instrument=="internal"&&song.tracks[2].kind==2&&!song.tracks[2].mute,"strings track stays the internal strings instrument");
            if(genre==4){
                check(song.settings.stringsMode==0&&countStrings(song)==0,"Jackin factory song does not invent a string bed");
                auto again=song;fga::generate(again,fga::Scope::song,0,0,9001);
                check(again.settings.stringsMode==0&&countStrings(again)==0,"song regenerate keeps the Jackin strings rest");
                auto locked=song;locked.tracks[2].locked=true;fga::generate(locked,fga::Scope::track,2,0,17);
                check(locked.settings.stringsMode==0&&countStrings(locked)==0,"a locked strings track is not filled or restyled");
                auto held=song;const int drop=findSection(held,fga::Section::drop);held.tracks[2].clips[(size_t)drop].locked=true;
                fga::generate(held,fga::Scope::track,2,0,19);
                check(held.tracks[2].clips[(size_t)drop].notes.empty()&&!held.tracks[2].clips[(size_t)findSection(held,fga::Section::main)].notes.empty(),"a locked strings clip stays empty while the rest of the track is written");
                fga::generate(song,fga::Scope::track,2,0,21);
                check(song.settings.stringsMode==(int)sgk::StringsMode::pad,"regenerating silent strings turns on a harmony pad");
                for(size_t i=0;i<song.sections.size();++i)check(!song.tracks[2].clips[i].notes.empty(),"strings track regenerate writes every unlocked section");
                check(chordFollows(song.tracks[2].clips[(size_t)findSection(song,fga::Section::drop)]),"regenerated Jackin strings follow the next chord");
                auto section=fga::newProject(4);const int mainIndex=findSection(section,fga::Section::main);
                fga::generate(section,fga::Scope::section,0,mainIndex,23);
                check(section.settings.stringsMode==(int)sgk::StringsMode::pad&&!section.tracks[2].clips[(size_t)mainIndex].notes.empty(),"section regenerate writes strings even when another track is selected");
                auto clipSong=fga::newProject(4);const int clipDrop=findSection(clipSong,fga::Section::drop);
                fga::generate(clipSong,fga::Scope::clip,2,clipDrop,29);
                check(!clipSong.tracks[2].clips[(size_t)clipDrop].notes.empty()&&clipSong.tracks[2].clips[(size_t)findSection(clipSong,fga::Section::intro)].notes.empty(),"clip regenerate writes only the chosen strings clip");
                continue;
            }
            check(song.settings.stringsMode!=0,"genres that use strings leave the style on");
            for(size_t i=0;i<song.sections.size();++i)if(song.sections[i].bars>=8)check(!song.tracks[2].clips[i].notes.empty(),"strings sound in every section of eight bars or more");
            const int drop=findSection(song,fga::Section::drop);
            const auto& bed=song.tracks[2].clips[(size_t)drop];
            const bool holds=song.settings.stringsMode==(int)sgk::StringsMode::pad||song.settings.stringsMode==(int)sgk::StringsMode::highPad||song.settings.stringsMode==(int)sgk::StringsMode::widePad||song.settings.stringsMode==(int)sgk::StringsMode::discoOctaves;
            if(holds)check(chordFollows(bed),"string pads follow the harmony into the next chord");
            else{int low=127,high=0;for(auto n:bed.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}check((int)bed.notes.size()>=8&&low>=36&&high<=96,"rhythmic strings stay in the ensemble register");}
            auto chords=song;fga::randomiseHarmony(chords,4242);fga::generate(chords);
            check(chords.settings.stringsMode==song.settings.stringsMode&&countStrings(chords)>0,"new chords keep the strings style and rewrite the part");
            auto take=song;fga::generate(take,fga::Scope::song,0,0,5150);
            check(take.settings.stringsMode!=0&&countStrings(take)>0,"song regenerate keeps a strings part where the genre uses one");
        }
    }
    {
        auto findSection=[&](const fga::Project& p,fga::Section type){for(int i=0;i<(int)p.sections.size();++i)if(p.sections[(size_t)i].type==type)return i;return -1;};
        auto near=[&](const fga::Clip& clip,double beat,double tol){int vel=0;for(auto n:clip.notes)if(std::abs(n.start-beat)<=tol)vel=std::max(vel,n.velocity);return vel;};
        auto jackBody=[&](const fga::Project& p,int section,const char* what){
            check(section>=0&&p.sections[(size_t)section].bars>=4,what);
            const auto& kick=p.tracks[4].clips[(size_t)section];
            const auto& snare=p.tracks[5].clips[(size_t)section];
            const auto& clap=p.tracks[6].clips[(size_t)section];
            const auto& closed=p.tracks[8].clips[(size_t)section];
            const auto& open=p.tracks[9].clips[(size_t)section];
            const auto& shaker=p.tracks[11].clips[(size_t)section];
            const auto& tom=p.tracks[15].clips[(size_t)section];
            int openOff=0,odd=0,perc=0,back=0;double swing=0;int swung=0,aVel=0,aN=0,eVel=0,eN=0;
            for(int beat=0;beat<16;++beat){
                check(near(kick,beat,0.04)>=40,what);
                if(beat%4==1||beat%4==3){const int hit=std::max(near(clap,beat,0.06),near(snare,beat,0.06));check(hit>=70,what);back+=hit>=70;}
                const bool two=near(kick,beat-beat%4,0.04)>=40&&near(kick,beat-beat%4+1,0.04)<40&&near(kick,beat-beat%4+2.5,0.08)>=40;
                check(!two,what);
            }
            auto scan=[&](const fga::Clip& clip,int lane){
                for(auto n:clip.notes){
                    if(n.start<0||n.start>=16)continue;
                    const int step=(int)std::lround(n.start/0.25);
                    if(lane==5&&step%4==2&&n.velocity>=40)++openOff;
                    if((lane==4||lane==7)&&step%2==1&&n.velocity>=45){swing+=std::fmod(n.start,0.5)/0.5*100;++swung;++odd;}
                    if((lane==7||lane==11)&&step%2==1&&n.velocity>=28)++perc;
                    if(lane==4&&step%4==3){aVel+=n.velocity;++aN;}
                    if(lane==4&&step%4==1){eVel+=n.velocity;++eN;}
                }
            };
            scan(closed,4);scan(open,5);scan(shaker,7);scan(tom,11);
            check(openOff>=12&&odd>=8&&perc>=4&&back==8,what);
            check(swung>=8&&swing/swung>=56&&swing/swung<=63,what);
            check(aN>=8&&aVel/aN>=70,what);
            if(eN>0)check(aVel/aN>eVel/eN+15,what);
        };
        auto tempo=[&](const fga::Project& p,const char* what){
            check(p.settings.bpm>=122&&p.settings.bpm<=128,what);
            check(p.settings.swing>=0.36&&p.settings.swing<=0.72f,what);
        };
        for(std::uint32_t seed=1;seed<=12;++seed){
            auto song=fga::newProject(4);
            tempo(song,"Jackin factory tempo and shuffle stay in house range");
            jackBody(song,findSection(song,fga::Section::main),"Jackin factory main is a jack groove");
            jackBody(song,findSection(song,fga::Section::drop),"Jackin factory drop is a jack groove");
            jackBody(song,findSection(song,fga::Section::buildup),"Jackin factory lift keeps the jack groove");
            jackBody(song,findSection(song,fga::Section::outro),"Jackin factory outro starts on the jack groove");
            check(song.tracks[4].clips[(size_t)findSection(song,fga::Section::breakdown)].notes.empty(),"Jackin break has no kick and no two-step");
            fga::generate(song,fga::Scope::song,0,0,seed);
            tempo(song,"Jackin song regenerate stays at 122-128 and a 56-62% shuffle");
            jackBody(song,findSection(song,fga::Section::main),"regenerated Jackin main keeps the jack groove");
            jackBody(song,findSection(song,fga::Section::drop),"regenerated Jackin drop keeps the jack groove");
            jackBody(song,findSection(song,fga::Section::buildup),"regenerated Jackin lift does not become a garage roll");
            if(seed==1){
                auto section=fga::newProject(4);const int main=findSection(section,fga::Section::main);
                fga::generate(section,fga::Scope::section,4,main,501);
                jackBody(section,main,"section regenerate rewrites the jack groove");
                auto clip=fga::newProject(4);const int clipMain=findSection(clip,fga::Section::main);
                const auto kick=clip.tracks[4].clips[(size_t)clipMain].notes;
                fga::generate(clip,fga::Scope::clip,8,clipMain,77);
                check(clip.tracks[4].clips[(size_t)clipMain].notes==kick,"regenerating the hat leaves the kick clip in place");
                int swung=0;for(auto n:clip.tracks[8].clips[(size_t)clipMain].notes){if(n.start>=16)continue;const int step=(int)std::lround(n.start/0.25);if(step%4==3&&n.velocity>=70)++swung;}
                check(swung>=8,"regenerating the closed hat keeps the swung accent");
                auto locked=fga::newProject(4);const int drop=findSection(locked,fga::Section::drop);
                const auto frozen=locked.tracks[4].clips[(size_t)drop].notes;
                locked.tracks[4].clips[(size_t)drop].locked=true;
                fga::generate(locked,fga::Scope::track,4,0,64);
                check(locked.tracks[4].clips[(size_t)drop].notes==frozen,"a locked Jackin kick clip is not rewritten");
                jackBody(locked,findSection(locked,fga::Section::main),"regenerating the kick track keeps a four-on-the-floor main");
            }
        }
    }
    std::set<std::string> palettes;
    for(int g=0;g<13;++g){auto styled=fga::newProject(g);std::ostringstream signature;for(int t:{0,1,3,4})signature<<styled.tracks[(size_t)t].sound<<":"<<styled.tracks[(size_t)t].engine.tone<<":";palettes.insert(signature.str());check(styled.tracks[1].pan==0&&styled.tracks[1].engine.reverb==0&&styled.tracks[1].engine.chorus==0,"dry centred default bass");check(styled.tracks[4].gain*.95>styled.tracks[8].gain*.28&&styled.tracks[4].gain*.95<3*styled.tracks[8].gain*.28,"drum balance includes original engine lane calibration");}
    check(palettes.size()==13,"thirteen distinct sound palettes, not one default kit/keyboard");
    auto personal=fga::newProject();personal.tracks[0].locked=true;personal.tracks[0].sound=6;personal.tracks[1].instrument="plugin";personal.tracks[1].instrumentPath="personal.vst3";personal.tracks[1].gain=.37;personal.tracks[3].instrument="sample";personal.tracks[3].instrumentPath="personal.wav";fga::applyGenre(personal,8);check(personal.tracks[0].sound==6&&personal.tracks[1].gain==.37&&personal.tracks[1].instrumentPath=="personal.vst3"&&personal.tracks[3].instrumentPath=="personal.wav","style preserves locked guides, personal instruments and routing");
    {
        auto manual=fga::newProject(1);const int track=manual.addMidiTrack();
        check(track==fga::trackCount&&manual.tracks[(size_t)track].clips[0].notes.empty(),"new handwritten MIDI track starts empty");
        auto& t=manual.tracks[(size_t)track];t.name="Melodia personale";t.clips[0].notes={{0,1,60,101,0},{1.5,.5,67,94,0}};
        t.instrument="plugin";t.instrumentPath="personal.vst3";t.volume.enabled=true;t.volume.points={{0,.83,0}};const auto kept=t;
        fga::generate(manual,fga::Scope::song,0,0,38451);
        fga::generate(manual,fga::Scope::section,0,0,7951);
        fga::applyInstrumentChange(manual,0,true);fga::applyGenre(manual,6);fga::arrangeSong(manual);fga::generate(manual);
        check(manual.tracks[(size_t)track].clips==kept.clips&&manual.tracks[(size_t)track].name==kept.name&&manual.tracks[(size_t)track].instrumentPath==kept.instrumentPath,"manual MIDI, name and routing survive song/section/harmony/genre/form changes");
        int ons=0;for(const auto& e:fga::events(manual,track))if(e.velocity>0&&e.controller<0)++ons;
        check(ons==2&&!fga::midiFile(manual,track).empty(),"manual notes reach the export schedule");
        check(manual.tracks[(size_t)track].volume.points.size()==1&&manual.tracks[(size_t)track].volume.points[0].value==.83,"automatic generation preserves manual-track automation");
        fga::randomiseAutomation(manual,fga::Scope::track,track,0,5179,0);check(manual.tracks[(size_t)track].volume.points.size()>1&&manual.tracks[(size_t)track].clips==kept.clips,"manual automation lane dice works without rewriting handwritten MIDI");
        while(manual.tracks.size()<fga::maxTracks)check(manual.addMidiTrack()>=0,"add several handwritten tracks");
        check(manual.addMidiTrack()==-1&&manual.tracks.size()==fga::maxTracks,"manual track limit is bounded");
        for(int ti=fga::trackCount;ti<fga::maxTracks;++ti)check(fga::trackAtRow(ti)==ti&&fga::rowForTrack(ti)==ti&&!fga::isDrumTrack(ti),"manual routing and visual order stay stable");
    }
    {
        auto p=fga::newProject(1);p.sections={{fga::Section::main,8,"Main"}};p.normalise();
        p.tracks[fga::leadTrack].engine.roomForBand=false;p.tracks[fga::leadTrack].engine.fillEvery=0;p.tracks[fga::leadTrack].engine.variationEvery=0;
        fga::generate(p,fga::Scope::track,fga::leadTrack);const auto original=p.tracks[fga::leadTrack].clips[0].notes;
        p.tracks[fga::leadTrack].engine.globalOctave=1;fga::applyInstrumentChange(p,fga::leadTrack,false);const auto& higher=p.tracks[fga::leadTrack].clips[0].notes;
        check(original.size()==higher.size()&&!higher.empty(),"global lead octave retains attacks");
        for(size_t i=0;i<original.size();++i)check(higher[i].pitch==original[i].pitch+12&&higher[i].start==original[i].start&&higher[i].velocity==original[i].velocity,"global lead octave changes register, keeping the stored take");
    }
    {
        auto p=fga::newProject(1);const int first=p.addMidiTrack(),second=p.addMidiTrack();
        p.tracks[(size_t)second].name="Personal lead";p.tracks[(size_t)second].clips[0].notes={{1,1,77,112,0}};p.tracks[(size_t)second].instrument="plugin";p.tracks[(size_t)second].instrumentState="saved instrument";p.tracks[(size_t)second].effects.push_back({1,"","Test","/fx/Test.vst3","","saved effect",false});
        const auto keep=p.tracks[(size_t)second];const auto factory=p.tracks[0].clips;
        check(!p.deleteMidiTrack(0)&&!p.deleteMidiTrack(fga::leadTrack)&&!p.deleteMidiTrack(99),"only handwritten MIDI tracks can be deleted");
        check(p.deleteMidiTrack(first)&&p.tracks.size()==fga::trackCount+1,"delete a handwritten MIDI track");
        const auto& moved=p.tracks[(size_t)first];check(moved.sessionId==keep.sessionId,"compaction preserves track session identity");check(moved.name==keep.name&&moved.clips==keep.clips&&moved.instrumentState==keep.instrumentState&&moved.effects[0].state==keep.effects[0].state&&moved.kind==first&&p.tracks[0].clips==factory,"later manual notes, name, plugin and effect state follow their compacted routing ID");
        check(p.addMidiTrack()==second,"a deleted slot can be reused");
        auto names=fga::newProject(1);const int deleted=names.addMidiTrack();names.addMidiTrack();names.deleteMidiTrack(deleted);const auto added=names.addMidiTrack();
        check(names.tracks[(size_t)deleted].name!=names.tracks[(size_t)added].name,"default MIDI track names remain distinct after deletion and addition");
    }
    {
        check(fga::defaultDrumNoteLength(16)==2.&&fga::defaultDrumNoteLength(4)==1.&&fga::defaultDrumNoteLength(8)==.225&&fga::defaultDrumNoteLength(9)==.5,"handwritten drum gates match the original engine, including Crash");
    }
    std::cout<<"All arranger core tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
