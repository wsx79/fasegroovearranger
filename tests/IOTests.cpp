// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/ProjectIO.h"
#include <iostream>
#include <stdexcept>
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
    auto p=fga::newProject(6);p.name="Garage – progetto";p.tracks[0].clips[0].notes.push_back({.125,1.375,65,117,2});p.tracks[2].locked=true;p.tracks[1].instrument="plugin";p.tracks[1].instrumentPath="/not/installed/Test.vst3";p.tracks[1].instrumentState="0123-state";p.tracks[5].gain=.317;p.tracks[5].pan=-.23;p.loop=true;p.loopStart=32;p.loopEnd=96;p.normalise();
    auto text=juce::JSON::toString(fga::projectJson(p));auto json=juce::JSON::parse(text);fga::Project restored;auto r=fga::projectFromJson(json,restored);check(r.wasOk(),"JSON parse");check(juce::JSON::toString(fga::projectJson(restored))==text,"lossless project round trip");
    auto tmp=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("arranger-io-tests","",false);tmp.createDirectory();auto file=tmp.getChildFile("song.fga");check(fga::saveProject(p,file).wasOk(),"atomic save");check(fga::loadProject(file,restored).wasOk(),"load");
    auto bytes=fga::midiFile(p,0);auto midi=tmp.getChildFile("piano.mid");midi.replaceWithData(bytes.data(),bytes.size());fga::Clip imported;check(fga::importMidi(midi,imported,p.length()).wasOk(),"MIDI import");size_t expected=0;for(const auto& c:p.tracks[0].clips)expected+=c.notes.size();check(imported.notes.size()>expected*.95,"MIDI matched note pairs");
    check(fga::projectFromJson(juce::var(),restored).failed(),"invalid format rejected");json.getDynamicObject()->setProperty("version",999);check(fga::projectFromJson(json,restored).failed(),"future schema rejected");json=juce::JSON::parse(text);json.getDynamicObject()->setProperty("tracks",juce::Array<juce::var>());check(fga::projectFromJson(json,restored).failed(),"missing tracks rejected");
    tmp.deleteRecursively();std::cout<<"Project persistence, schema validation and MIDI import passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
