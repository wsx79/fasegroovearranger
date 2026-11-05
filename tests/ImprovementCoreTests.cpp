// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Project.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include <iostream>
#include <stdexcept>
using namespace fga;
static void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
    auto p=newProject(1);p.sections={{Section::main,4,"Verse"},{Section::buildup,4,"Build"},{Section::drop,4,"Drop"}};p.normalise();generate(p);
    auto previous=p;auto& h=p.sections[1].harmony;h.enabled=true;h.key=6;h.scale=0;h.chordCount=2;h.degrees={5,3,0,0,0,0,0,0};
    generate(p,Scope::section,0,1,0);
    for(int ti=0;ti<trackCount;++ti){require(p.tracks[(size_t)ti].clips[0]==previous.tracks[(size_t)ti].clips[0],"section harmony changed preceding clip");require(p.tracks[(size_t)ti].clips[2]==previous.tracks[(size_t)ti].clips[2],"section harmony changed excluded drop");}
    auto settings=pianoEngineSettings(p,0,p.tracks[0].clips[1].seed,0,&p.tracks[0].clips[1]);require(settings.keyRoot==6&&settings.progressionLength==2,"independent harmony not supplied to renderer");require(p.tracks[0].clips[1].notes!=previous.tracks[0].clips[1].notes,"section harmony did not change MIDI");
    p.tracks[16].instrument="sample";p.tracks[16].sampleRoot=49;p.sections[1].preDropSilence=true;p.sections[1].snareRoll=true;p.sections[1].reverseCrash=true;generate(p,Scope::section,0,1,0);
    for(int ti:{1,4})for(const auto& n:p.tracks[(size_t)ti].clips[1].notes){if(n.start+n.length>14+1e-6)std::cerr<<"Transition track="<<ti<<" start="<<n.start<<" length="<<n.length<<" clip="<<p.clipLength(ti,1)<<"\n";require(n.start+n.length<=14+1e-6,"pre-drop silence contains kick or bass");}
    require(std::any_of(p.tracks[16].clips[1].notes.begin(),p.tracks[16].clips[1].notes.end(),[](const Note& n){return n.reverse&&n.start==12&&n.length==4;}),"missing per-hit reverse crash");
    int rolls=0;for(const auto& n:p.tracks[5].clips[1].notes)if(n.start>=12)++rolls;require(rolls>=14,"accelerating roll is absent");
    auto locked=p;p.tracks[4].clips[1].locked=true;const auto keep=p.tracks[4].clips[1];generate(p,Scope::section,0,1,0);require(p.tracks[4].clips[1]==keep,"transition overwrites locked clip");
    p=previous;p.settings.grooveEnabled=true;p.settings.grooveOffsets.fill(.04);p.settings.grooveAccents.fill(.6);generate(p,Scope::track,0,0,0);require(p.tracks[0].clips[0].notes!=previous.tracks[0].clips[0].notes,"groove template has no effect");require(p.tracks[1].clips==previous.tracks[1].clips,"groove modifies an excluded track");
    Project c;c.normalise();c.tracks[0].freeTimeline=true;Clip clip;clip.span=8;clip.notes={{0,8,60,100,0}};clip.expression[0].enabled=true;clip.expression[0].points={{0,1,0},{4,1,0},{4.25,0,0}};clip.expression[2].enabled=true;clip.expression[2].points={{0,.5,0},{4,1,0},{8,.5,0}};c.tracks[0].clips={clip};const auto e=events(c,0);require(std::any_of(e.begin(),e.end(),[](const TimedEvent& x){return x.controller==64&&x.velocity==127;}),"sustain curve omitted");require(std::any_of(e.begin(),e.end(),[](const TimedEvent& x){return x.controller==128&&x.velocity>8192;}),"pitch bend omitted");const auto midi=midiFile(c,0);require(std::find(midi.begin(),midi.end(),0xe0)!=midi.end(),"export lost pitch bend status");require(c.splitClip(0,0,3)==1,"expression split failed");require(std::abs(c.tracks[0].clips[1].expression[2].valueAt(0)-clip.expression[2].valueAt(3))<1e-9,"expression split lost chase value");
    clip.audioOffset=2;clip.audioDuration=4;clip.clipFadeIn=.4;clip.clipFadeOut=.7;c.tracks[0].clips={clip};c.splitClip(0,0,3);require(c.tracks[0].clips[0].clipFadeIn==.4&&c.tracks[0].clips[0].clipFadeOut==0&&c.tracks[0].clips[1].clipFadeIn==0&&c.tracks[0].clips[1].clipFadeOut==.7,"split did not preserve only outer user fades");
    c.tracks[0].clips[0].span=maxClipBeats;c.tracks[0].clips[0].expression[2].points={{0,.5,0},{maxClipBeats,.6,0}};const auto bounded=events(c,0);require(bounded.size()<20000,"long MIDI curve expansion is not bounded");
    std::cout<<"PASS section harmony/scopes, transitions/locks, extracted groove, MIDI expression/export/split, audio fades\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
