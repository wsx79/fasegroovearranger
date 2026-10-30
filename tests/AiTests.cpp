// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AiComposer.h"
#include <stdexcept>
#include <iostream>
namespace {
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
juce::String fixture(){return R"({"title":"Techno","summary":"Six minutes","bpm":138,"key":0,"scale":1,"genre":12,"degrees":[0,5,3,4],"chord_steps":32,"target_seconds":360,"sections":[{"name":"Intro","type":"intro","bars":16},{"name":"Drop","type":"drop","bars":32},{"name":"Outro","type":"outro","bars":16}],"patterns":[{"id":"kick","track":4,"beats":4,"notes":[[0,36,0.5,110],[1,36,0.5,100],[2,36,0.5,110],[3,36,0.5,100]]},{"id":"lead","track":17,"beats":4,"notes":[[0,60,1.5,100],[1,63,0.5,90],[2,67,1,100]]}],"clips":[{"section":0,"pattern":"kick"},{"section":1,"pattern":"kick"},{"section":1,"pattern":"lead"},{"section":2,"pattern":"kick"}]})";}
}
void runAiTests(){
    {
        auto project=fga::newProject(4);const int track=project.addVocalChopsTrack();auto& vocal=project.tracks[(size_t)track];vocal.instrument="sample";vocal.sampler.sliceBase=72;vocal.sampler.sliceCount=3;
        fga::AiOptions options;options.tracks={track};options.wholeSong=false;options.model="test";options.prompt="sparse vocal chops";
        const auto context=juce::JSON::parse(fga::aiContext(project,options));const auto item=context["enabled_tracks"][0];
        require(item["role"].toString()=="vocal_chops"&&(int)item["low"]==72&&(int)item["high"]==74,"AI receives vocal slice role and actual key range");
        auto plan=juce::JSON::parse(fixture());plan["patterns"].getArray()->getReference(1).getDynamicObject()->setProperty("track",track);options.wholeSong=true;options.tracks={4,track};fga::Project result;result.name="KEEP";juce::String summary;
        auto invalid=fga::aiCompile(project,options,juce::JSON::toString(plan),result,summary);require(invalid.failed()&&invalid.getErrorMessage().contains("mapped slice keys")&&result.name=="KEEP","AI rejects unmapped vocal triggers atomically");
        auto& notes=*plan["patterns"].getArray()->getReference(1)["notes"].getArray();for(int i=0;i<notes.size();++i)notes.getReference(i).getArray()->getReference(1)=72+i;
        require(fga::aiCompile(project,options,juce::JSON::toString(plan),result,summary).wasOk(),"AI accepts mapped vocal triggers");
    }
    {auto project=fga::newProject(31);fga::AiOptions options;options.model="test-model";options.prompt="test melody";options.wholeSong=false;options.tracks={0};project.tracks[0].locked=true;fga::AiHttpRequest request;if(fga::aiHttpRequest(project,options,request).wasOk())throw std::runtime_error("locked track must be rejected before HTTP");project.tracks[0].locked=false;options.tracks={(int)project.tracks.size()};if(fga::aiHttpRequest(project,options,request).wasOk())throw std::runtime_error("missing track must be rejected before HTTP");}

    auto base=fga::newProject(1);base.tracks[4].instrument="plugin";base.tracks[4].instrumentPath="/PRIVATE/kick.vst3";base.tracks[4].instrumentId="PRIVATE-ID";base.tracks[4].instrumentState="PRIVATE-STATE";base.tracks[4].effectState="PRIVATE-FX";base.tracks[4].gain=.42;
    fga::AiOptions o;o.model="qwen3:8b";o.tracks={4,17};o.prompt="Una canzone techno di 6 minuti";o.key="secret-key";
    require(!fga::aiContext(base,o).contains("PRIVATE")&&!fga::aiContext(base,o).contains(o.key),"AI context excludes credentials, paths and plugin states");
    for(int i=0;i<4;++i){o.provider=(fga::AiProvider)i;fga::AiHttpRequest request;require(fga::aiHttpRequest(base,o,request).wasOk(),"provider request formats");require(request.body.contains("Six")||request.body.contains("task"),"musical request sent");require(!request.body.contains(o.key)&&!request.url.contains(o.key),"API keys absent from URL/body");require(request.body.contains("messages")||request.body.contains("contents"),"provider payload includes musical context");}
    fga::Project song;juce::String summary;require(fga::aiCompile(base,o,fixture(),song,summary).wasOk(),"valid AI composition compiles");require(song.settings.chordSteps==32&&song.settings.chordCount==4&&song.settings.degrees[1]==5,"AI harmony metadata matches its chord display");require(song.settings.freeTempo&&song.settings.bpm==138&&std::abs(song.length()*60/138-360)<.01,"six-minute techno retains 138 BPM beyond family presets");
    require(song.tracks[4].instrumentPath==base.tracks[4].instrumentPath&&song.tracks[4].instrumentState=="PRIVATE-STATE"&&song.tracks[4].gain==.42,"AI preserves assigned plugins, state and mix");
    require(!song.tracks[17].clips[1].notes.empty()&&song.tracks[0].clips[0].notes.empty(),"only chosen instruments receive new song MIDI");
    const auto& lead=song.tracks[17].clips[1].notes;for(size_t i=0;i+1<lead.size();++i)require(lead[i].start+lead[i].length<=lead[i+1].start+1./960.,"AI lead remains monophonic");
    fga::Project saved;require(fga::projectFromJson(fga::projectJson(song),saved).wasOk()&&saved.settings.bpm==138&&saved.settings.freeTempo,"AI BPM round-trips in .fga");fga::applyGenre(saved,1);require(!saved.settings.freeTempo,"manual genre selection restores its preset tempo policy");
    const auto before=juce::JSON::toString(fga::projectJson(song),false,17);auto bad=juce::JSON::parse(fixture());bad["patterns"][0].getDynamicObject()->setProperty("track",0);require(fga::aiCompile(base,o,juce::JSON::toString(bad),song,summary).failed(),"disabled track rejected");require(juce::JSON::toString(fga::projectJson(song),false,17)==before,"failed AI response is transactional");
    for(auto text:{juce::String("not json"),fixture().replace("[0,36,0.5,110]","[0,999,0.5,110]"),fixture().replace("[0,36,0.5,110]","[0,36,-1,110]"),fixture().replace("\"pattern\":\"kick\"","\"pattern\":\"missing\""),fixture().replace("\"id\":\"lead\"","\"id\":\"kick\""),fixture().replace("[1,63,0.5,90]","[0,63,0.5,90]")})require(fga::aiCompile(base,o,text,song,summary).failed(),"malformed notes, unknown patterns, duplicate IDs and polyphonic lead rejected");
    require(fga::aiCompile(base,o,"```json\n"+fixture()+"\n```",song,summary).wasOk(),"JSON fences from models accepted");o.seconds=240;require(fga::aiCompile(base,o,fixture(),song,summary).wasOk()&&std::abs(song.length()*60/138-240)<1.,"explicit duration overrides model duration, within half a bar");o.seconds=0;
    auto phrased=juce::JSON::parse(fixture());phrased["sections"][0].getDynamicObject()->setProperty("bars",32);phrased["sections"][1].getDynamicObject()->setProperty("bars",128);phrased["sections"][2].getDynamicObject()->setProperty("bars",48);require(fga::aiCompile(base,o,juce::JSON::toString(phrased),song,summary).wasOk()&&song.sections[0].bars==32&&song.sections[1].bars==128&&song.sections[2].bars==47,"duration fitting preserves DJ phrases, adjusting the ending first");
    phrased["clips"].getArray()->add(juce::JSON::parse(R"({"section":2,"pattern":"lead","offset":188,"repeat":false})"));require(fga::aiCompile(base,o,juce::JSON::toString(phrased),song,summary).wasOk()&&song.tracks[17].clips[2].notes.front().start==184,"closing fills move with the resized section ending");
    auto crash=juce::JSON::parse(fixture());crash["patterns"][0].getDynamicObject()->setProperty("track",16);o.tracks={16,17};require(fga::aiCompile(base,o,juce::JSON::toString(crash),song,summary).wasOk()&&song.tracks[16].clips[0].notes[0].pitch==49&&song.tracks[16].clips[0].notes[0].lane==12,"AI percussion follows the existing kit mapping");o.tracks={4,17};
    base.tracks[0].locked=true;require(fga::aiCompile(base,o,fixture(),song,summary).failed(),"full-song AI respects locks");base.tracks[0].locked=false;
    auto edit=base;edit.sections={{fga::Section::main,4,"Main"},{fga::Section::drop,4,"Drop"},{fga::Section::outro,4,"Outro"}};edit.normalise();edit.tracks[4].clips[0].locked=true;o.wholeSong=false;bad=juce::JSON::parse(fixture());for(int i=0;i<3;++i)bad["sections"][i].getDynamicObject()->setProperty("bars",4);
    require(fga::aiCompile(edit,o,juce::JSON::toString(bad),song,summary).wasOk(),"selected-track AI rewrite");require(song.settings.bpm==edit.settings.bpm&&song.sections[1].bars==4,"track editing preserves shared tempo and form");
    require(song.tracks[4].clips[0]==edit.tracks[4].clips[0]&&song.tracks[0].clips==edit.tracks[0].clips,"locked clips and unselected tracks untouched");
    edit.detachTrack(4);const auto locked=edit.tracks[4].clips[0];require(fga::aiCompile(edit,o,juce::JSON::toString(bad),song,summary).wasOk(),"selected AI can rewrite independent timelines");const int li=song.clipAtBeat(4,1);require(li>=0&&song.tracks[4].clips[(size_t)li].locked&&song.tracks[4].clips[(size_t)li].notes==locked.notes,"locked independent clip notes retained exactly");
    o.provider=fga::AiProvider::openai;o.key="bad\nheader";fga::AiHttpRequest request;require(fga::aiHttpRequest(base,o,request).failed(),"header injection rejected");o.provider=fga::AiProvider::ollama;o.endpoint="file:///etc/passwd";require(fga::aiHttpRequest(base,o,request).failed(),"non-HTTP Ollama endpoint rejected");
    juce::String address;
    for(const auto& input:{"127.0.0.1:11434"," http://127.0.0.1:11434/ ","http://127.0.0.1:11434/api","http://127.0.0.1:11434/api/chat","http://127.0.0.1:11434/v1",""})
        require(fga::aiOllamaEndpoint(input,address).wasOk()&&address=="http://127.0.0.1:11434","Ollama accepts common server address forms without duplicating API paths");
    for(const auto& input:{"file:///etc/passwd","http://user:password@localhost:11434","http://localhost:11434?key=secret","http://local\nhost:11434"})
        require(fga::aiOllamaEndpoint(input,address).failed(),"Ollama rejects credentials, query strings and unsafe addresses");
    o.key.clear();o.endpoint="localhost:11434/api/chat";
    require(fga::aiHttpRequest(base,o,request).wasOk()&&!request.headers.contains("Authorization")&&request.url=="http://localhost:11434/api/chat","local composition works without a key at normalized endpoint");
    const auto payload=juce::JSON::parse(request.body);
    require(payload.hasProperty("think")&&!(bool)payload["think"]&&(int)payload["options"]["num_ctx"]==16384,"local request limits memory and disables Qwen3 reasoning to produce MIDI JSON");
    o.provider=fga::AiProvider::openai;require(fga::aiHttpRequest(base,o,request).failed(),"online services still require their API key");
    juce::String response;
    require(fga::aiResponse(fga::AiProvider::openai,juce::JSON::parse(R"({"choices":[{"finish_reason":"length","message":{"content":"partial"}}]})"),response).failed(),"truncated output cannot become a song");
    const std::array<juce::String,4> replies{R"({"choices":[{"message":{"content":"{}"}}]})",R"({"content":[{"type":"text","text":"{}"}]})",R"({"candidates":[{"content":{"parts":[{"text":"{}"}]}}]})",R"({"message":{"content":"{}"}})"};
    for(int i=0;i<4;++i)require(fga::aiResponse((fga::AiProvider)i,juce::JSON::parse(replies[(size_t)i]),response).wasOk()&&response=="{}","four provider response envelopes");
    std::cout<<"AI composition: real MIDI, arrangement duration, routing, locks, validation and provider envelopes passed\n";
}
int runAiHttpTest(const juce::String& endpoint,bool cancel){auto p=fga::newProject(1);fga::AiOptions o;o.provider=fga::AiProvider::ollama;o.model="qwen3:8b";o.endpoint=endpoint;o.tracks={4,17};o.prompt="techno 6 minutes";fga::Project out;juce::String summary;const auto started=juce::Time::getMillisecondCounterHiRes();auto r=fga::aiGenerate(p,o,out,summary,[=]{return cancel&&juce::Time::getMillisecondCounterHiRes()-started>200;});if(cancel){require(r.failed()&&r.getErrorMessage().contains("cancelled"),"cancel interrupted the HTTP request");return 0;}if(r.failed()){std::cerr<<r.getErrorMessage()<<"\n";return 1;}require(out.settings.bpm==138&&std::abs(out.length()*60/138-360)<.01,"HTTP model response writes six-minute MIDI");return 0;}

int runAiModelsTest(const juce::String& endpoint,bool cancel){
    juce::StringArray models;const auto started=juce::Time::getMillisecondCounterHiRes();
    auto result=fga::aiOllamaModels(endpoint,models,[=]{return cancel&&juce::Time::getMillisecondCounterHiRes()-started>200;});
    if(cancel){require(result.failed()&&result.getErrorMessage().contains("cancelled"),"model discovery cancels a stalled connection");return 0;}
    if(result.failed()){require(models.isEmpty(),"failed discovery never supplies nonexistent models");std::cerr<<result.getErrorMessage()<<"\n";return 1;}
    require(models==juce::StringArray{"qwen2.5:7b","qwen3:8b"},"discovery reads installed local models, deduplicates and excludes cloud models");return 0;
}
