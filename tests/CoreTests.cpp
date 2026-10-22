// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Project.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.h"
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
}
#include "ClubArrangementChecks.h"
#include "TimelineChecks.h"
#include "RegenerationChecks.h"
int main(int argc,char** argv){try{
    verifyRegeneration();verifyTimelineEditing();verifyClubEdgeCases();verifyClubSeedCases();verifyGuitarAudibility();
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
    {auto p=fga::newProject(1);auto& e=p.tracks[0].engine;e.pattern=-1;e.riffSeed=123;auto a=fga::pianoEngineSettings(p,0,55);auto patternA=sgk::GrooveEngine::basePattern(a);e.riffSeed=9876;auto b=fga::pianoEngineSettings(p,0,55);auto patternB=sgk::GrooveEngine::basePattern(b);check(patternA.rightHand!=patternB.rightHand||patternA.leftHand!=patternB.leftHand,"riff seed changes the automatic genre groove");check(a.humanizeSeed==b.humanizeSeed,"riff and humanize seeds are independent");}
    {
        auto attacks=[](const std::string& line){std::string out(line.size(),'.');for(size_t i=0;i<line.size();++i)if(line[i]=='X'||line[i]=='x'||line[i]=='g'||line[i]=='>'||line[i]=='u'||line[i]=='U'||line[i]=='d'||line[i]=='D')out[i]='x';return out;};
        for(int genre=0;genre<13;++genre){
            std::set<std::string> figures;
            for(std::uint32_t seed=1;seed<=12;++seed)figures.insert(attacks(sgk::makeRandomPattern(seed,.55f,genre).rightHand));
            check((int)figures.size()>=3,"each genre rolls more than two piano figures");
            auto song=fga::newProject(genre);song.sections={{fga::Section::main,4,"Main"}};song.normalise();
            std::set<std::vector<std::pair<int,int>>> rhythms;
            for(std::uint32_t seed:{3u,19u,41u,77u}){auto q=song;fga::generate(q,fga::Scope::track,0,0,seed);rhythms.insert(rhythmFingerprint(q.tracks[0].clips[0]));}
            check((int)rhythms.size()>=3,"regenerate changes the piano figure in every genre");
        }
        sgk::GrooveEngine live;sgk::GrooveSettings jack;jack.pattern=sgk::randomPatternIndex();jack.riffSeed=5;jack.riffDensity=.55f;jack.styleGenre=4;jack.humanize=0;jack.swing=0;jack.bass=sgk::BassMode::off;jack.strings=sgk::StringsMode::off;jack.fillEveryBars=0;jack.variationEvery=0;
        auto garage=jack;garage.styleGenre=6;
        std::vector<sgk::NoteEvent> first,second;live.render(0,4,jack,first);live.render(4,8,garage,second);
        auto steps=[&](const std::vector<sgk::NoteEvent>& ev,double origin){std::set<int> at;for(const auto& e:ev)if(e.isNoteOn()&&e.part==sgk::Part::chords)at.insert((int)std::lround((e.ppq-origin)*4));return at;};
        check(steps(first,0)!=steps(second,4),"a genre change rebuilds the cached piano riff");
    }
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
    fga::Clip c; c.notes={{.13,.5,60,96,0},{.3,.2,127,100,0}};fga::quantise(c,.25);check(c.notes[0].start==.25&&c.notes[0].length==.5,"quantise");fga::transpose(c,12);check(c.notes[0].pitch==72&&c.notes[1].pitch==127,"transpose clamp");
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
            check(p.settings.bpm>=120&&p.settings.bpm<=127,what);
            check(p.settings.swing>=0.36&&p.settings.swing<=0.72,what);
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
            tempo(song,"Jackin song regenerate stays at 120-127 and a 56-62% shuffle");
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
    std::cout<<"All arranger core tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
