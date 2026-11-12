// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/ProjectIO.h"
#include "../Source/StudioIO.h"
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
void runVocalStudioIOTests(){
    using namespace fga;
    auto p=newProject(0);const int t=p.addMidiTrack();auto& c=p.tracks[(size_t)t].clips[0];c.notes={{0,1,60,100,0},{2,1,62,95,0}};
    enableVocalClip(c,c.span);auto& v=*c.vocal;v.lyrics="Hello world";v.voice="qixuan-candidate";v.modelPath="/missing/model";v.allowIntroOutro=true;
    v.syllables={{1,"hel","hh eh l",{c.notes[0].vocalId,c.notes[1].vocalId}}};v.nextSyllableId=2;
    p.normalise();const auto json=projectJson(p);Project restored;
    require(projectFromJson(juce::JSON::parse(juce::JSON::toString(json,false,17)),restored).wasOk()&&restored.tracks[(size_t)t].clips==p.tracks[(size_t)t].clips,"Vocal Studio JSON round trip including missing model and melisma");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-vocal-io","",false);temp.createDirectory();const auto file=temp.getChildFile("vocal.fga");
    require(saveProject(p,file).wasOk()&&loadProject(file,restored).wasOk()&&restored.tracks[(size_t)t].clips==p.tracks[(size_t)t].clips,"Vocal Studio actual file save/reopen");
    // Old engine identifiers are data, not executable backend selection.
    auto guide=p;auto& stored=*guide.tracks[(size_t)t].clips[0].vocal;stored.voice="ecantorix-en";stored.modelPath="/removed/helper.pl";
    const auto guideJson=juce::JSON::toString(projectJson(guide),false,17);
    require(saveProject(guide,file).wasOk()&&loadProject(file,restored).wasOk()&&juce::JSON::toString(projectJson(restored),false,17)==guideJson,"removed engine metadata round trips without migration, loading a helper or losing lyrics");
    temp.deleteRecursively();
    auto invalid=[&](const std::function<void(juce::var&)>& edit){auto bad=juce::JSON::parse(juce::JSON::toString(json));auto data=bad["tracks"][t]["clips"][0]["vocalStudio"];edit(data);const auto before=projectJson(restored);require(projectFromJson(bad,restored).failed(),"malformed vocal metadata rejected");require(juce::JSON::toString(before)==juce::JSON::toString(projectJson(restored)),"failed vocal load preserves open project");};
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("language","it");});
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("schema",2);});
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("lyrics",12);});
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("lyrics",juce::String::repeatedString("x",65537));});
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("regionStart",-1);});
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("allowIntroOutro",1);});
    invalid([](juce::var& v){v["noteIds"].getArray()->set(1,v["noteIds"][0]);});
    invalid([](juce::var& v){v["syllables"][0]["notes"].getArray()->set(0,99999);});
    invalid([](juce::var& v){v.getDynamicObject()->setProperty("nextNoteId",1);});
    auto loopProject=p;auto& loop=loopProject.tracks[(size_t)t].clips[0];loop.loopLength=4;loop.loopSource=loop.notes;loop.span=8;
    require(rebuildClipLoop(loop,8),"prepare vocal loop fixture");loopProject.normalise();Project loopLoaded;
    require(projectFromJson(projectJson(loopProject),loopLoaded).wasOk(),"vocal loop metadata round trip");
    const auto loopLinks=loopLoaded.tracks[(size_t)t].clips[0].vocal->syllables[0].noteIds;
    require(loopLoaded.resizeClipLoop(t,0,12)&&loopLoaded.tracks[(size_t)t].clips[0].vocal->syllables[0].noteIds==loopLinks,"loop resize after reopen retains original lyric assignments");
    auto duplicated=juce::JSON::parse(juce::JSON::toString(json));auto copy=duplicated["tracks"][t]["clips"][0];duplicated["tracks"][t]["clips"].getArray()->add(copy);
    require(projectFromJson(duplicated,restored).failed(),"duplicate persisted vocal clip identity rejected");
    auto legacy=juce::JSON::parse(juce::JSON::toString(json));legacy["tracks"][t]["clips"][0].getDynamicObject()->removeProperty("vocalStudio");
    require(projectFromJson(legacy,restored).wasOk()&&!restored.tracks[(size_t)t].clips[0].vocal,"older projects default to ordinary MIDI");
    auto chops=json;chops["tracks"][t].getDynamicObject()->setProperty("vocalChops",true);require(projectFromJson(chops,restored).failed(),"mixed Vocal Chops/Studio metadata rejected");
}
