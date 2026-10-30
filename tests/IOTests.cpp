// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/ProjectIO.h"
#include <iostream>
#include <set>
#include <stdexcept>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void runAiTests();
void runStudioIOTests();
int runAiHttpTest(const juce::String&,bool);
int runAiModelsTest(const juce::String&,bool);
int main(int argc,char** argv){try{
    if(argc==3&&(juce::String(argv[1])=="--ai-models-test"||juce::String(argv[1])=="--ai-models-cancel"))return runAiModelsTest(argv[2],juce::String(argv[1])=="--ai-models-cancel");
    if(argc==3&&(juce::String(argv[1])=="--ai-http-test"||juce::String(argv[1])=="--ai-http-cancel"))return runAiHttpTest(argv[2],juce::String(argv[1])=="--ai-http-cancel");
    runAiTests();runStudioIOTests();
    {
        auto vocal=fga::newProject(4);const int index=vocal.addVocalChopsTrack();fga::Project loaded;
        check(fga::projectFromJson(fga::projectJson(vocal),loaded).wasOk()&&loaded.tracks[(size_t)index].vocalChops&&loaded.tracks[(size_t)index].clips==vocal.tracks[(size_t)index].clips,"vocal generator role, seeds and MIDI round trip");
        auto legacy=fga::projectJson(vocal);legacy["tracks"].getArray()->getReference(index).getDynamicObject()->removeProperty("vocalChops");
        check(fga::projectFromJson(legacy,loaded).wasOk()&&!loaded.tracks[(size_t)index].vocalChops,"legacy manual MIDI remains manual");
        vocal.tracks[0].vocalChops=true;vocal.normalise();check(!vocal.tracks[0].vocalChops,"vocal role cannot replace fixed instrument routing");
    }
    {
        auto project=fga::newProject(19);while(project.tracks.size()<fga::maxTracks)project.addMidiTrack();
        for(auto& track:project.tracks)for(auto& c:track.clips)c.notes.clear();
        for(auto& track:project.tracks)track.clips[0].notes={{0,.5,60,100,0}};
        auto bytes=fga::midiFile(project);juce::MemoryInputStream stream(bytes.data(),bytes.size(),false);juce::MidiFile midi;check(midi.readFrom(stream),"read multi-port MIDI export");
        std::set<std::pair<int,int>> melodic;int melodicTracks=0;
        for(int ti=1;ti<midi.getNumTracks();++ti){int port=0;for(int i=0;i<midi.getTrack(ti)->getNumEvents();++i){const auto& message=midi.getTrack(ti)->getEventPointer(i)->message;
            if(message.isMetaEvent()&&message.getMetaEventType()==0x21&&message.getMetaEventLength()==1)port=message.getMetaEventData()[0];
            if(message.isNoteOn()&&message.getChannel()!=10){++melodicTracks;check(melodic.insert({port,message.getChannel()}).second,"melodic tracks never share a MIDI port/channel");break;}
        }}check(melodicTracks==19&&melodic.size()==19,"all 19 melodic instruments are separately routed");
        auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-midi-budget",".mid");
        struct Cleanup{juce::File file;~Cleanup(){file.deleteFile();}} cleanup{file};auto small=fga::midiFile(project,0);file.replaceWithData(small.data(),small.size());
        fga::Clip imported;imported.notes={{2,1,71,80,0}};const auto previous=imported.notes;
        check(fga::importMidi(file,imported,4,0).failed()&&imported.notes==previous,"MIDI budget failure keeps the original clip");check(fga::importMidi(file,imported,4,1).wasOk()&&imported.notes.size()==1,"MIDI import obeys the remaining project note budget");
        auto oversized=fga::newProject(1);oversized.tracks[0].clips[0].notes.assign(fga::maxProjectNotes+1,{0,1,60,100,0});file.replaceWithText("KEEP");check(fga::saveProject(oversized,file).failed()&&file.loadFileAsString()=="KEEP","oversized project cannot replace a readable destination");
    }

    {auto project=fga::newProject(72);auto& track=project.tracks[4];track.instrument="sample";track.instrumentPath="drum-fixture.wav";track.sampler.mode=1;track.clips[0].notes={{0,.25,36,100,0},{.5,.25,37,100,0}};project.normalise();fga::Project loaded;check(track.clips[0].notes[1].pitch==37&&fga::projectFromJson(fga::projectJson(project),loaded).wasOk()&&loaded.tracks[4].clips[0].notes[1].pitch==37,"sample drum MIDI pitches survive normalisation, recording and project round-trip outside slice mode");track.instrument="internal";project.normalise();check(track.clips[0].notes[0].pitch==track.clips[0].notes[1].pitch,"built-in drum guides keep their existing fixed MIDI mapping");}

    {
        auto original=fga::newProject(1);int ti=original.addMidiTrack();original.tracks[(size_t)ti].name="MIDI – melodia";original.tracks[(size_t)ti].clips[0].notes={{2,.75,74,103,0}};original.tracks[(size_t)ti].instrument="sample";original.tracks[(size_t)ti].instrumentPath="melody.wav";original.tracks[fga::leadTrack].engine.globalOctave=-1;
        const auto json=juce::JSON::toString(fga::projectJson(original),false,17);fga::Project loaded;
        check(fga::projectFromJson(juce::JSON::parse(json),loaded).wasOk(),"manual track project loads");
        check(juce::JSON::toString(fga::projectJson(loaded),false,17)==json,"manual notes, renamed tracks, routing and global Lead octave round-trip exactly");
    }
    auto p=fga::newProject(6);p.name="Garage – progetto";p.tracks[0].clips[0].notes.push_back({.125,1.375,65,117,2});p.tracks[0].clips[0].notes.push_back({.000012345678901234567,1./960.,110,87,0});p.tracks[2].locked=true;p.tracks[1].instrument="plugin";p.tracks[1].instrumentPath="/not/installed/Test.vst3";p.tracks[1].instrumentState="0123-state";p.tracks[5].gain=.317;p.tracks[5].pan=-.23;p.loop=true;p.loopStart=32;p.loopEnd=96;p.settings.guitarLead=true;p.tracks[3].engine.laneShift[5]=23;p.tracks[3].engine.laneVolume[5]=.37;p.tracks[3].engine.laneOctave[1]=-1;p.tracks[3].engine.laneMute[8]=true;p.tracks[0].engine.pattern=12;p.tracks[0].engine.gate=.78;p.tracks[0].engine.reverb=.37;p.settings.chordSteps=8;p.tracks[0].engine.inversions[2]=-2;p.tracks[0].engine.userRight[3]='X';p.tracks[0].engine.userVelRight[3]=89;p.tracks[0].engine.velocityOrder={3,2,1,0};p.tracks[0].engine.segmentLocked[1]=true;p.tracks[0].engine.riffSeed=987;p.tracks[0].engine.humanizeSeed=456;p.tracks[0].engine.pumpRate=2;p.tracks[0].engine.pedalCC=true;p.normalise();
    fga::generate(p,fga::Scope::clip,1,1,4951);
    // Loop state persists: an active selection loop and a removed loop both round-trip.
    for(int state=0;state<3;++state){auto l=p;if(state==0)l.setLoopRegion(8,24);else if(state==1){l.setLoopRegion(8,24);l.toggleLoopRegion(8,24);}else{l.setLoopRegion(8,24);l.toggleLoop();}
        fga::Project back;check(fga::projectFromJson(juce::JSON::parse(juce::JSON::toString(fga::projectJson(l),false,17)),back).wasOk(),"loop JSON parse");
        check(back.loop==l.loop&&back.loopStart==l.loopStart&&back.loopEnd==l.loopEnd,"loop on, removed and off states persist");}
    check(p.tracks[1].clips[1].motifSeed!=0,"regenerated motif present before save");
    p.tracks[4].volume.enabled=true;p.tracks[4].volume.points={{0,1,.5},{17.25,.3,-.8},{p.length(),0,0}};
    auto text=juce::JSON::toString(fga::projectJson(p),false,17);auto json=juce::JSON::parse(text);fga::Project restored;auto r=fga::projectFromJson(json,restored);check(r.wasOk(),"JSON parse");auto round=juce::JSON::toString(fga::projectJson(restored),false,17);if(round!=text){int diff=0;while(diff<std::min(round.length(),text.length())&&round[diff]==text[diff])++diff;std::cerr<<"round-trip difference at "<<diff<<"\noriginal: "<<text.substring(std::max(0,diff-70),diff+160)<<"\nrestored: "<<round.substring(std::max(0,diff-70),diff+160)<<"\n";}check(round==text,"stable project round trip");int stringNotes=0;for(const auto& c:restored.tracks[2].clips)stringNotes+=(int)c.notes.size();check(stringNotes>0&&restored.settings.stringsMode==p.settings.stringsMode&&restored.tracks[2].instrument=="internal","strings notes and style survive save and reload");for(size_t ti=0;ti<p.tracks.size();++ti)for(size_t si=0;si<p.sections.size();++si){const auto& original=p.tracks[ti].clips[si].notes;const auto& loaded=restored.tracks[ti].clips[si].notes;check(original.size()==loaded.size(),"note count preserved");for(size_t ni=0;ni<original.size();++ni)check(std::abs(original[ni].start-loaded[ni].start)<1e-12&&std::abs(original[ni].length-loaded[ni].length)<1e-12,"sub-tick timing preserved");}
    {auto edited=p;const double split=8;check(edited.splitClip(0,0,split)>=0,"independent timeline split");const int moved=edited.moveClip(0,1,0,edited.length()+4,false);check(moved>=0,"independent timeline move");auto json=fga::projectJson(edited);check((int)json["version"]==2,"edited timelines use version 2");fga::Project loaded;check(fga::projectFromJson(json,loaded).wasOk(),"load version 2 timeline");check(loaded.tracks[0].freeTimeline&&loaded.tracks[0].clips==edited.tracks[0].clips&&fga::midiFile(loaded)==fga::midiFile(edited),"independent positions MIDI and loop sources persist");
    while(!edited.tracks[0].clips.empty())edited.deleteClip(0,0);check(fga::projectFromJson(fga::projectJson(edited),loaded).wasOk()&&loaded.tracks[0].clips.empty(),"empty edited track persists");}
    auto tmp=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("arranger-io-tests","",false);tmp.createDirectory();auto file=tmp.getChildFile("song.fga");check(fga::saveProject(p,file).wasOk(),"atomic save");check(fga::loadProject(file,restored).wasOk(),"load");
    auto bytes=fga::midiFile(p,0);auto midi=tmp.getChildFile("piano.mid");midi.replaceWithData(bytes.data(),bytes.size());fga::Clip imported;check(fga::importMidi(midi,imported,p.length()).wasOk(),"MIDI import");size_t expected=0;for(const auto& c:p.tracks[0].clips)expected+=c.notes.size();check(imported.notes.size()>expected*.95,"MIDI matched note pairs");
    check(fga::projectFromJson(juce::var(),restored).failed(),"invalid format rejected");json.getDynamicObject()->setProperty("version",999);check(fga::projectFromJson(json,restored).failed(),"future schema rejected");json=juce::JSON::parse(text);json.getDynamicObject()->setProperty("tracks",juce::Array<juce::var>());check(fga::projectFromJson(json,restored).failed(),"missing tracks rejected");
    auto musical=p;auto loadedTake=restored;check(fga::projectFromJson(json=fga::projectJson(p),loadedTake).wasOk(),"reload musical take");fga::generate(musical,fga::Scope::clip,1,1);fga::generate(loadedTake,fga::Scope::clip,1,1);check(musical.tracks[1].clips[1]==loadedTake.tracks[1].clips[1],"saved musical take renders identically after loading");
    auto legacy=fga::projectJson(p);for(auto& track:*legacy["tracks"].getArray())for(auto& clip:*track["clips"].getArray())clip.getDynamicObject()->removeProperty("motifSeed");for(auto& track:*legacy["tracks"].getArray())track.getDynamicObject()->removeProperty("volumeAutomation");check(fga::projectFromJson(legacy,restored).wasOk()&&!restored.tracks[4].volume.enabled&&restored.tracks[4].volume.points.empty(),"old projects load with bypassed empty automation");
    {
        // An older project (no pan, master or plugin lanes) saves exactly as before; the new lanes round-trip.
        auto bare=p;for(auto& t:bare.tracks)t.panAutomation={};auto plain=juce::JSON::toString(fga::projectJson(bare),false,17);check(!plain.contains("panAutomation")&&!plain.contains("parameterAutomation")&&!plain.contains("masterAutomation"),"unused lanes are not written");
        auto lanes=p;lanes.tracks[1].panAutomation.enabled=true;lanes.tracks[1].panAutomation.points={{0,.2,.3},{8,.9,0}};lanes.masterAutomation.enabled=true;lanes.masterAutomation.points={{4,1.2,-.4}};
        fga::ParameterAutomation lane;lane.slot=1;lane.index=7;lane.name="Filter – cutoff";lane.curve.enabled=true;lane.curve.points={{2,.1,0},{6,.6,.5}};lanes.tracks[2].parameters.push_back(lane);
        auto saved=juce::JSON::toString(fga::projectJson(lanes),false,17);fga::Project back;check(fga::projectFromJson(juce::JSON::parse(saved),back).wasOk(),"automation lanes parse");
        const auto& b=back.tracks[2].parameters;check(back.tracks[1].panAutomation.enabled&&back.tracks[1].panAutomation.points.size()==2&&back.tracks[1].panAutomation.points[0].curve==.3&&back.masterAutomation.points.size()==1&&back.masterAutomation.points[0].value==1.2
              &&b.size()==1&&b[0].slot==1&&b[0].index==7&&b[0].name==lane.name&&b[0].curve.points[1].curve==.5,"pan, master and plugin lanes round-trip");
        check(juce::JSON::toString(fga::projectJson(back),false,17)==saved,"automation lanes save identically after loading");
        auto bad=juce::JSON::parse(saved);bad["tracks"][2].getDynamicObject()->setProperty("parameterAutomation",juce::var("x"));check(fga::projectFromJson(bad,back).failed(),"invalid plugin automation rejected");
    }
    {auto source=fga::projectJson(p);auto* list=source["tracks"].getArray();while(list->size()>3)list->removeLast();fga::Project migrated;check(fga::projectFromJson(source,migrated).wasOk()&&migrated.tracks.size()==fga::trackCount,"short legacy projects migrate to stable slots");check(migrated.tracks[0].clips==p.tracks[0].clips&&migrated.tracks[0].instrumentState==p.tracks[0].instrumentState,"legacy migration preserves existing MIDI and plugin state");for(int t=3;t<fga::legacyTrackCount;++t){check(migrated.tracks[(size_t)t].mute,"missing legacy slots start silent");for(const auto& clip:migrated.tracks[(size_t)t].clips)check(clip.notes.empty(),"migration never generates extra MIDI");}}
    {auto source=fga::projectJson(p);auto only=source["tracks"][3];source.getDynamicObject()->setProperty("tracks",juce::Array<juce::var>{only});fga::Project sparse;check(fga::projectFromJson(source,sparse).wasOk()&&sparse.tracks[3].clips==p.tracks[3].clips&&sparse.tracks[0].mute,"sparse legacy migration preserves explicit guitar slot");source.getDynamicObject()->setProperty("tracks",juce::Array<juce::var>{only,only});check(fga::projectFromJson(source,sparse).failed(),"ambiguous duplicate legacy slot is rejected");}
    {fga::EffectSlot fx;fx.path="/fx/Spring.vst3";fx.pluginId="<PLUGIN/>";fx.state="fx-state";fx.pluginName="Spring";fx.name="Voice spring";p.tracks[2].effects.push_back(fx);auto second=fx;second.slot=4;second.name="Second spring";second.bypassed=true;p.tracks[2].effects.push_back(second);check(fga::projectFromJson(fga::projectJson(p),restored).wasOk()&&restored.tracks[2].effects.size()==2&&restored.tracks[2].effects[1].slot==4&&restored.tracks[2].effects[1].bypassed&&restored.tracks[2].effects[0].name==fx.name&&restored.tracks[2].effects[0].state==fx.state&&restored.tracks[0].effects.empty(),"effect chain persists names, instance IDs, order, bypass and states");
        auto legacy=fga::projectJson(p);auto a=legacy["tracks"][2];a.getDynamicObject()->removeProperty("effects");a.getDynamicObject()->setProperty("effectPath","/fx/Old.vst3");a.getDynamicObject()->setProperty("effectId","<PLUGIN name=\"Legacy spring\"/>");a.getDynamicObject()->setProperty("effectState","old-state");check(fga::projectFromJson(legacy,restored).wasOk()&&restored.tracks[2].effects.size()==1&&restored.tracks[2].effects[0].slot==1&&restored.tracks[2].effects[0].state=="old-state","legacy single insert migrates to the first stable effect slot");
        auto invalid=fga::projectJson(p);invalid["tracks"][2]["effects"][1].getDynamicObject()->setProperty("slot",1);const auto before=juce::JSON::toString(fga::projectJson(restored));check(fga::projectFromJson(invalid,restored).failed()&&juce::JSON::toString(fga::projectJson(restored))==before,"duplicate effect IDs are rejected atomically");auto badLane=fga::projectJson(p);auto lane=fga::projectJson(p)["tracks"][2];juce::DynamicObject::Ptr data=new juce::DynamicObject();data->setProperty("slot",99);data->setProperty("index",0);data->setProperty("points",juce::Array<juce::var>{});lane.getDynamicObject()->setProperty("parameterAutomation",juce::Array<juce::var>{juce::var(data.get())});check(fga::projectFromJson(badLane,restored).wasOk(),"valid chain remains loadable");badLane["tracks"][2].getDynamicObject()->setProperty("parameterAutomation",lane["parameterAutomation"]);check(fga::projectFromJson(badLane,restored).failed(),"invalid automation targets never redirect to a valid instance");}

    {
        auto legacy=fga::projectJson(p);legacy["tracks"].getArray()->remove(fga::leadTrack);
        fga::Project loaded;check(fga::projectFromJson(legacy,loaded).wasOk(),"Legacy seventeen-track projects still load");
        check(loaded.tracks.size()==fga::trackCount&&loaded.tracks[fga::leadTrack].name=="Lead","Legacy project appends Lead without renumbering tracks");
        const auto upgraded=fga::projectJson(loaded);
        for(int t=0;t<fga::legacyTrackCount;++t)check(juce::JSON::toString(upgraded["tracks"][t],false,17)==juce::JSON::toString(legacy["tracks"][t],false,17),"Legacy notes, routing, effects and automation are preserved");
        for(const auto& c:loaded.tracks[fga::leadTrack].clips)check(c.notes.empty(),"Loading an old project does not add notes to its mix");
        loaded.tracks[fga::leadTrack].instrument="plugin";loaded.tracks[fga::leadTrack].instrumentPath="/lead/Lead.vst3";loaded.tracks[fga::leadTrack].instrumentState="lead-state";loaded.tracks[fga::leadTrack].engine.laneShift[4]=12.5;
        fga::Project back;check(fga::projectFromJson(fga::projectJson(loaded),back).wasOk()&&back.tracks[fga::leadTrack].instrumentState=="lead-state"&&back.tracks[fga::leadTrack].engine.laneShift[4]==12.5,"Lead routing and lane settings round-trip");
    }
    tmp.deleteRecursively();std::cout<<"Project persistence, schema validation and MIDI import passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
