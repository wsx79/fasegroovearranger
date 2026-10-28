// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AiComposer.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/Theory.h"
#include "../ThirdParty/fasegroovedrums/Source/engine/Drums.h"
#include <cmath>
#include <map>
#include <atomic>
#include <thread>
#include <chrono>
#include <set>
#include <cstring>
namespace fga {
namespace {
juce::var object(std::initializer_list<std::pair<const char*,juce::var>> items){auto* d=new juce::DynamicObject;for(auto& kv:items)d->setProperty(kv.first,kv.second);return juce::var(d);}
using Array=juce::Array<juce::var>;
struct Invalid {juce::String message;};
void need(bool ok,const juce::String& message){if(!ok)throw Invalid{message};}
bool numeric(const juce::var& v){return v.isInt()||v.isInt64()||v.isDouble();}
double number(const juce::var& v,double lo,double hi,const char* name){need(numeric(v),juce::String("Missing or invalid ")+name);const double n=(double)v;need(std::isfinite(n)&&n>=lo&&n<=hi,juce::String("Out of range: ")+name);return n;}
int integer(const juce::var& v,int lo,int hi,const char* name){const double n=number(v,lo,hi,name);need(n==std::floor(n),juce::String("Expected an integer: ")+name);return (int)n;}
const Array& array(const juce::var& v,int lo,int hi,const char* name){need(v.isArray()&&v.size()>=lo&&v.size()<=hi,juce::String("Invalid list: ")+name);return *v.getArray();}
juce::String string(const juce::var& v,const char* name,int limit=100){need(v.isString()&&v.toString().trim().isNotEmpty()&&v.toString().length()<=limit,juce::String("Invalid text: ")+name);return v.toString().trim();}
juce::var get(const juce::var& v,const char* key){return v.getProperty(key,juce::var());}
bool anyLock(const Project& p){for(const auto& t:p.tracks){if(t.locked)return true;for(const auto& c:t.clips)if(c.locked)return true;}return false;}
bool selected(const AiOptions& o,int t){return std::find(o.tracks.begin(),o.tracks.end(),t)!=o.tracks.end();}

}
juce::String aiSystemPrompt(){return R"AI(You are the composer inside Fase Groove Arranger, a MIDI DAW. Compose actual MIDI notes and a complete developing arrangement using ONLY the supplied track IDs. No audio, code, markdown, tools or file paths. Reply with ONE valid JSON object.
Schema:
{"title":"...","summary":"brief musical explanation in the user's language","bpm":138,"key":0,"scale":1,"genre":12,"degrees":[0,5,3,4],"chord_steps":16,"target_seconds":360,
 "sections":[{"name":"Intro","type":"intro","bars":16}],
 "patterns":[{"id":"kick","track":4,"beats":4,"notes":[[0,36,0.5,110],[1,36,0.5,105],[2,36,0.5,110],[3,36,0.5,105]]}],
 "clips":[{"section":0,"pattern":"kick","offset":0,"repeat":true,"transpose":0,"velocity":1.0}]}
Notes are [start_beat, MIDI_pitch, length_beats, velocity_1_to_127, optional_guitar_or_lead_lane_0_to_11]. Times use quarter-note beats, 4/4. A pattern lasts 0.25..128 beats, contains up to 512 notes inside that length, and belongs to ONE track. Give every pattern a unique ID. Clips repeat their pattern from offset to the end of their section unless repeat=false; use repeat=false with a late offset for fills and transition hits. Transpose/velocity are optional; avoid overlapping clip placements on the same track. Drum pitches must match supplied MIDI mappings; Lead MUST be monophonic. Patterns may be polyphonic for Piano, Strings, Guitar and manual MIDI tracks. Use track pitch ranges. Compose differentiated phrases, chord progressions, bass movement, lead motifs, percussion variations, fills and arrangement dynamics. Do not merely loop one bar on every track for the entire song. Silence and instrument entrances/exits should create musical development. You write the notes yourself: no random-generator commands.
Section types: intro, main, breakdown, buildup, drop, outro. Use 1..64 sections, each 1..128 bars. Sections should normally follow 8/16/32-bar phrases; the DAW fits them to requested duration, within half a bar. Maximum 256 patterns, 2048 clips, 500000 expanded notes. BPM 30..300. Key 0=C..11=B. Scale and genre indices are supplied in context; genre is a fallback for subsequent manual regeneration, not a restriction on your style or BPM.
Return degrees (1..8 scale-degree integers 0..6) and chord_steps (8/16/32/64 sixteenth steps per chord) to describe the global progression used by your MIDI, so the DAW chord display and later edits follow the same harmony.
For a NEW SONG, replace its complete MIDI arrangement, using only enabled tracks. Retain track sounds and mix; never request a plugin or sample file. A duration_seconds of 0 in context means infer the duration from the task. A nonzero duration_seconds overrides the text. target_seconds MUST reflect the user's requested minutes (default 360 if unspecified). For TRACK EDIT mode, retain the exact existing sections, BPM, key and scale; rewrite ONLY enabled tracks, keeping locked clips intact. Use musical context to follow the existing harmony; leave other tracks unchanged. Do not explain the JSON outside the summary field.)AI";}
juce::String aiContext(const Project& p,const AiOptions& o){
    Array tracks,sections,scales,styles;
    for(int i=0;i<sgk::numScales;++i)scales.add(object({{"id",i},{"name",sgk::scaleName(i)}}));
    auto names=genres();for(int i=0;i<(int)names.size();++i)styles.add(object({{"id",i},{"name",names[(size_t)i].c_str()}}));
    for(size_t i=0;i<p.sections.size();++i)sections.add(object({{"name",p.sections[i].name.c_str()},{"type",sectionName(p.sections[i].type)},{"bars",p.sections[i].bars}}));
    for(int i:o.tracks){if(i<0||i>=(int)p.tracks.size())continue;const auto& t=p.tracks[(size_t)i];Array examples;
        if(!o.wholeSong)for(size_t ci=0;ci<t.clips.size()&&examples.size()<6;++ci){Array notes;for(size_t n=0;n<t.clips[ci].notes.size()&&n<32;++n){const auto& x=t.clips[ci].notes[n];notes.add(Array{x.start,x.pitch,x.length,x.velocity,x.lane});}examples.add(object({{"start",p.clipStart(i,(int)ci)},{"beats",p.clipLength(i,(int)ci)},{"locked",t.clips[ci].locked},{"notes",notes}}));}
        const auto role=isDrumTrack(i)?"drum":i==0?"piano":i==1?"bass":i==2?"strings":i==3?"guitar":i==leadTrack?"lead":"MIDI";
        tracks.add(object({{"id",i},{"name",t.name.c_str()},{"role",role},{"low",t.engine.rangeLow},{"high",t.engine.rangeHigh},{"drum_pitch",isDrumTrack(i)?(t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote(i-4)):-1},{"examples",examples}}));
    }
    Array reference;
    if(!o.wholeSong)for(size_t ti=0;ti<p.tracks.size();++ti)if(isHarmonicTrack((int)ti)&&!selected(o,(int)ti)){Array clips;for(size_t ci=0;ci<p.tracks[ti].clips.size()&&clips.size()<3;++ci){const auto& c=p.tracks[ti].clips[ci];if(c.notes.empty())continue;Array notes;for(size_t i=0;i<c.notes.size()&&i<24;++i){const auto& n=c.notes[i];notes.add(Array{n.start,n.pitch,n.length,n.velocity});}clips.add(object({{"start",p.clipStart((int)ti,(int)ci)},{"notes",notes}}));}reference.add(object({{"track",(int)ti},{"clips",clips}}));}
    // Harmony context excludes sample/plugin paths, serialised state and credentials.
    Array chords;for(const auto& name:chordNames(p.settings))chords.add(name.c_str());
    return juce::JSON::toString(object({{"task",o.prompt},{"mode",o.wholeSong?"NEW SONG":"TRACK EDIT"},{"duration_seconds",o.seconds},{"bpm",p.settings.bpm},{"key",p.settings.key},{"scale",p.settings.scale},{"chord_beats",p.settings.chordSteps*.25},{"chords",chords},{"scales",scales},{"genres",styles},{"current_sections",sections},{"reference_parts",reference},{"enabled_tracks",tracks}}),true);
}
juce::Result aiHttpRequest(const Project& p,const AiOptions& o,AiHttpRequest& out){
    if(o.prompt.trim().isEmpty()||o.prompt.length()>6000)return juce::Result::fail("Write a musical request (up to 6000 characters).");
    if(o.model.trim().isEmpty()||o.model.length()>160||o.model.containsAnyOf("\r\n"))return juce::Result::fail("Choose a valid model.");
    if(o.tracks.empty())return juce::Result::fail("Select at least one instrument.");
    for(int track:o.tracks)if(!juce::isPositiveAndBelow(track,(int)p.tracks.size())||p.tracks[(size_t)track].locked)return juce::Result::fail("An enabled track is locked or unavailable.");
    if(o.wholeSong&&anyLock(p))return juce::Result::fail("Unlock tracks and clips for a new song, or choose selected-track editing.");
    if(o.provider!=AiProvider::ollama&&(o.key.trim().isEmpty()||o.key.containsAnyOf("\r\n")||o.key.length()>2048))return juce::Result::fail("Enter the API key for the selected service.");
    const auto system=aiSystemPrompt(),user=aiContext(p,o);out.headers="Content-Type: application/json\n";
    Array messages{object({{"role","user"},{"content",user}})};
    if(o.provider==AiProvider::anthropic){out.url="https://api.anthropic.com/v1/messages";out.headers+="x-api-key: "+o.key.trim()+"\nanthropic-version: 2023-06-01\n";out.body=juce::JSON::toString(object({{"model",o.model},{"max_tokens",16384},{"system",system},{"messages",messages}}),true);}
    else if(o.provider==AiProvider::gemini){out.url="https://generativelanguage.googleapis.com/v1beta/models/"+juce::URL::addEscapeChars(o.model,false)+":generateContent";out.headers+="x-goog-api-key: "+o.key.trim()+"\n";out.body=juce::JSON::toString(object({{"systemInstruction",object({{"parts",Array{object({{"text",system}})}}})},{"contents",Array{object({{"role","user"},{"parts",Array{object({{"text",user}})}}})}},{"generationConfig",object({{"responseMimeType","application/json"},{"maxOutputTokens",16384}})}}),true);}
    else {messages.insert(0,object({{"role","system"},{"content",system}}));auto body=object({{"model",o.model},{"messages",messages},{"stream",false}});
        if(o.provider==AiProvider::openai){out.url="https://api.openai.com/v1/chat/completions";out.headers+="Authorization: Bearer "+o.key.trim()+"\n";body.getDynamicObject()->setProperty("max_completion_tokens",16384);body.getDynamicObject()->setProperty("response_format",object({{"type","json_object"}}));}
        else {juce::String endpoint;auto normalised=aiOllamaEndpoint(o.endpoint,endpoint);if(normalised.failed())return normalised;out.url=endpoint+"/api/chat";body.getDynamicObject()->setProperty("format","json");body.getDynamicObject()->setProperty("options",object({{"num_predict",8192},{"num_ctx",16384}}));if(o.model.startsWithIgnoreCase("qwen3"))body.getDynamicObject()->setProperty("think",false);}
        out.body=juce::JSON::toString(body,true);
    }return juce::Result::ok();
}
juce::Result aiResponse(AiProvider provider,const juce::var& reply,juce::String& result){
    result.clear();
    if(provider==AiProvider::openai){const auto& choices=get(reply,"choices");if(choices.isArray()&&choices.size()>0){const auto choice=choices[0];if(get(choice,"finish_reason").toString()=="length")return juce::Result::fail("The model reached its output limit. Request fewer instruments or shorter patterns.");result=get(get(choice,"message"),"content").toString();}}
    else if(provider==AiProvider::anthropic){if(get(reply,"stop_reason").toString()=="max_tokens")return juce::Result::fail("The model reached its output limit. Request fewer instruments or shorter patterns.");const auto a=get(reply,"content");if(a.isArray())for(auto& v:*a.getArray())if(get(v,"type").toString()=="text")result+=get(v,"text").toString();}
    else if(provider==AiProvider::gemini){const auto a=get(reply,"candidates");if(a.isArray()&&a.size()>0){if(get(a[0],"finishReason").toString()=="MAX_TOKENS")return juce::Result::fail("The model reached its output limit. Request fewer instruments or shorter patterns.");const auto parts=get(get(a[0],"content"),"parts");if(parts.isArray())for(auto& v:*parts.getArray())if(!(bool)get(v,"thought"))result+=get(v,"text").toString();}}
    else {if(get(reply,"done_reason").toString()=="length")return juce::Result::fail("The local model reached its output limit.");result=get(get(reply,"message"),"content").toString();}
    if(result.trim().isEmpty())return juce::Result::fail("The service returned no composition. Check the model and request.");return juce::Result::ok();
}
juce::Result aiCompile(const Project& base,const AiOptions& o,const juce::String& text,Project& output,juce::String& summary){
    try{
        need(text.getNumBytesAsUTF8()<=4*1024*1024,"Composition response is too large.");auto cleaned=text.trim();if(cleaned.startsWith("```")){const int line=cleaned.indexOfChar('\n'),end=cleaned.lastIndexOf("```");need(line>=0&&end>line,"Incomplete composition JSON.");cleaned=cleaned.substring(line+1,end).trim();}
        juce::var root;auto parse=juce::JSON::parse(cleaned,root);need(parse.wasOk()&&root.isObject(),"The model did not return valid composition JSON. Try again or choose another model.");
        need(!o.tracks.empty(),"Select at least one instrument.");for(int t:o.tracks)need(t>=0&&t<(int)base.tracks.size()&&!base.tracks[(size_t)t].locked,"An enabled track is locked or unavailable.");
        Project p=base;const auto& form=array(get(root,"sections"),1,64,"sections");
        std::map<int,std::vector<Clip>> independentLocks;
        if(o.wholeSong){need(!anyLock(base),"Unlock tracks and clips before replacing the song.");p.name=string(get(root,"title"),"title").toStdString();p.settings.bpm=number(get(root,"bpm"),30,300,"bpm");p.settings.freeTempo=true;p.settings.key=integer(get(root,"key"),0,11,"key");p.settings.scale=integer(get(root,"scale"),0,12,"scale");p.settings.genre=integer(get(root,"genre"),0,12,"genre");
            if(root.hasProperty("degrees")){const auto& degrees=array(get(root,"degrees"),1,8,"degrees");p.settings.chordCount=degrees.size();p.settings.freeChords.fill(-1);for(int i=0;i<degrees.size();++i)p.settings.degrees[(size_t)i]=integer(degrees[i],0,6,"degree");}
            if(root.hasProperty("chord_steps")){const int steps=integer(get(root,"chord_steps"),8,64,"chord_steps");need(steps==8||steps==16||steps==32||steps==64,"Chord duration must be 8, 16, 32 or 64 steps.");p.settings.chordSteps=steps;}
            p.sections.clear();
            const juce::StringArray types{"main","intro","breakdown","buildup","drop","outro"};for(auto& s:form){const auto type=get(s,"type").toString().toLowerCase().removeCharacters(" -_");const int ti=types.indexOf(type);need(ti>=0,"Unknown section type.");p.sections.push_back({(Section)ti,integer(get(s,"bars"),1,128,"bars"),string(get(s,"name"),"section name",64).toStdString()});}
            const double seconds=o.seconds>0?o.seconds:number(get(root,"target_seconds"),30,1800,"target_seconds");need(seconds>=30&&seconds<=1800,"Song duration must be 30 seconds to 30 minutes.");const int total=(int)std::lround(seconds*p.settings.bpm/240.);need(total>=(int)p.sections.size()&&total<=(int)p.sections.size()*128,"The section count cannot fit the requested duration.");
            int sum=0;for(auto& s:p.sections)sum+=s.bars;
            // Preserve the model's phrases when already close; otherwise scale in
            // eight-bar phrases. Put the remaining duration adjustment in the ending.
            if(std::abs(sum-total)>8){const double ratio=(double)total/sum;for(auto& s:p.sections){const int unit=s.bars>=8?8:1;s.bars=std::clamp((int)std::lround(s.bars*ratio/unit)*unit,1,128);}}
            int assigned=0;for(const auto& s:p.sections)assigned+=s.bars;
            for(int i=(int)p.sections.size()-1;i>=0&&assigned!=total;--i){auto& bars=p.sections[(size_t)i].bars;const int delta=std::clamp(total-assigned,1-bars,128-bars);bars+=delta;assigned+=delta;}
            need(assigned==total,"The requested duration cannot fit this arrangement.");
            p.clearLoop();p.masterAutomation={};p.settings.randomAutomation=false;
            for(auto& t:p.tracks){t.freeTimeline=false;t.clips.assign(p.sections.size(),{});t.volume={};t.panAutomation={};t.parameters.clear();}
        }else {need(form.size()==(int)p.sections.size(),"Track editing must retain the current sections.");for(int i=0;i<form.size();++i)need(integer(get(form[i],"bars"),1,128,"bars")==p.sections[(size_t)i].bars,"Track editing cannot change section lengths.");
            for(int t:o.tracks){if(p.tracks[(size_t)t].freeTimeline){for(const auto& c:p.tracks[(size_t)t].clips)if(c.locked)independentLocks[t].push_back(c);p.tracks[(size_t)t].freeTimeline=false;p.tracks[(size_t)t].clips.assign(p.sections.size(),{});}for(auto& c:p.tracks[(size_t)t].clips)if(!c.locked){c.notes.clear();c.loopSource.clear();c.loopLength=0;}}
        }
        struct Pattern{int track;double beats;std::vector<Note> notes;};std::map<juce::String,Pattern> patterns;
        for(auto& v:array(get(root,"patterns"),1,256,"patterns")){const auto id=string(get(v,"id"),"pattern id",80);const int t=integer(get(v,"track"),0,(int)p.tracks.size()-1,"track");need(selected(o,t),"The model used a disabled track.");Pattern pattern{t,number(get(v,"beats"),.25,128,"pattern beats"),{}};
            for(auto& x:array(get(v,"notes"),1,512,"notes")){const auto& n=array(x,4,5,"note");Note note{number(n[0],0,pattern.beats,"note start"),number(n[2],1./960.,128,"note length"),integer(n[1],0,127,"MIDI pitch"),integer(n[3],1,127,"velocity"),n.size()==5?integer(n[4],0,isDrumTrack(t)?12:11,"lane"):0};need(note.start<pattern.beats&&note.start+note.length<=pattern.beats+1./960.,"A note extends beyond its pattern.");if(isDrumTrack(t)){note.pitch=p.tracks[(size_t)t].engine.midiNote>=0?p.tracks[(size_t)t].engine.midiNote:fgd::defaultNote(t-4);note.lane=t-4;}pattern.notes.push_back(note);}
            need(patterns.emplace(id,std::move(pattern)).second,"Pattern IDs must be unique.");
        }
        size_t count=0;for(auto& v:array(get(root,"clips"),1,2048,"clips")){const int si=integer(get(v,"section"),0,(int)p.sections.size()-1,"section index");const auto found=patterns.find(string(get(v,"pattern"),"pattern reference",80));need(found!=patterns.end(),"A clip refers to an unknown pattern.");const auto& pattern=found->second;auto& clip=p.tracks[(size_t)pattern.track].clips[(size_t)si];if(clip.locked)continue;
            const double span=p.clipLength(pattern.track,si),originalSpan=(double)get(form[si],"bars")*4;double offset=v.hasProperty("offset")?number(get(v,"offset"),0,originalSpan,"offset"):0;need(offset<originalSpan,"A clip starts after its section.");
            const bool repeat=(bool)v.getProperty("repeat",true);
            // Duration fitting must move closing fills with the section ending.
            if(o.wholeSong&&span!=originalSpan){if(!repeat&&offset>=originalSpan-pattern.beats)offset=std::max(0.,span-(originalSpan-offset));else offset=std::min(span-1./960.,std::round(offset*span/originalSpan*4)/4);}
            const int transpose=v.hasProperty("transpose")?integer(get(v,"transpose"),-48,48,"transpose"):0;const double velocity=v.hasProperty("velocity")?number(get(v,"velocity"),.05,2,"velocity multiplier"):1;
            for(double at=offset;at<span-1./960.;at+=pattern.beats){for(auto note:pattern.notes){note.start+=at;if(note.start>=span)continue;note.length=std::min(note.length,span-note.start);if(!isDrumTrack(pattern.track)){note.pitch+=transpose;need(note.pitch>=0&&note.pitch<=127,"Transposition exceeds MIDI range.");}note.velocity=std::clamp((int)std::lround(note.velocity*velocity),1,127);clip.notes.push_back(note);need(++count<=500000,"The composition contains too many notes.");}if(!repeat)break;}
        }
        need(count>0,"The composition has no playable notes.");for(int t:o.tracks)for(size_t i=0;i<p.tracks[(size_t)t].clips.size();++i){auto& c=p.tracks[(size_t)t].clips[i];if(c.locked)continue;
            if(t==leadTrack){std::stable_sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});for(size_t n=0;n+1<c.notes.size();++n){need(c.notes[n+1].start-c.notes[n].start>=1./960.,"Lead must be monophonic: simultaneous notes are not supported.");c.notes[n].length=std::min(c.notes[n].length,c.notes[n+1].start-c.notes[n].start);}}
            tidyNotes(c,p.clipLength(t,(int)i));}
        for(auto& entry:independentLocks){p.detachTrack(entry.first);for(auto& locked:entry.second){const double at=locked.position;need(p.placeClip(entry.first,std::move(locked),at)>=0,"Cannot preserve a locked independent clip.");}}
        p.normalise();count=0;for(const auto& t:p.tracks)for(const auto& c:t.clips)count+=c.notes.size();summary=get(root,"summary").toString().substring(0,2000)+"\n\n"+juce::String(p.settings.bpm,1)+juce::String::fromUTF8(" BPM  ·  ")+juce::String((int)std::lround(p.length()*60/p.settings.bpm))+juce::String::fromUTF8(" s  ·  ")+juce::String((int)p.sections.size())+juce::String::fromUTF8(" sections  ·  ")+juce::String((int)count)+" MIDI notes";output=std::move(p);return juce::Result::ok();
    }catch(const Invalid& e){return juce::Result::fail(e.message);}catch(...){return juce::Result::fail("The composition could not be read. Try a simpler request.");}
}
namespace {
juce::Result fetchJson(const AiHttpRequest& request,bool local,bool discovery,juce::var& envelope,const std::function<bool()>& cancelled){
    if(cancelled())return juce::Result::fail("Generation cancelled.");
    // Native networking on Windows/macOS, libcurl on Linux. Request bodies and
    // credentials stay in memory: no shell, process arguments or temporary files.
    const int limitSeconds=discovery?8:local?900:240;
    juce::WebInputStream stream(discovery?juce::URL(request.url):juce::URL(request.url).withPOSTData(request.body),!discovery);
    stream.withCustomRequestCommand(discovery?"GET":"POST").withExtraHeaders(request.headers.replace("\n","\r\n")).withConnectionTimeout(limitSeconds*1000).withNumRedirectsToFollow(0);
    std::atomic<bool> finished{false},timedOut{false};const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(limitSeconds);
    std::thread watcher([&]{while(!finished.load()){if(cancelled()||std::chrono::steady_clock::now()>=deadline){timedOut=std::chrono::steady_clock::now()>=deadline;stream.cancel();return;}juce::Thread::sleep(20);}});
    struct Finish{std::atomic<bool>& done;std::thread& thread;~Finish(){done=true;thread.join();}} finish{finished,watcher};
    stream.connect(nullptr);if(cancelled())return juce::Result::fail("Generation cancelled.");if(timedOut)return juce::Result::fail(discovery?"Ollama did not respond in time. Check that the server is running and try again.":"The model took too long. Try a smaller model or a simpler request.");
    const int status=stream.getStatusCode();if(status<=0)return juce::Result::fail(local?"Cannot reach Ollama. Open the Ollama application (or run ollama serve), then check the server address and try again.":"Connection failed. Check the server address, network and system TLS support.");
    juce::MemoryOutputStream response;char buffer[8192];while(!stream.isExhausted()){if(cancelled())return juce::Result::fail("Generation cancelled.");const int n=stream.read(buffer,sizeof(buffer));if(n<=0)break;response.write(buffer,(size_t)n);if(response.getDataSize()>4194304)return juce::Result::fail("The response is too large.");}
    if(cancelled())return juce::Result::fail("Generation cancelled.");
    if(timedOut||stream.isError())return juce::Result::fail("The connection ended before the composition was complete.");
    const auto parsed=juce::JSON::parse(response.toUTF8(),envelope);
    if(status==401||status==403)return juce::Result::fail(local?"This Ollama server requires authentication. Use a local Ollama server for generation without an API key.":"The service rejected the API key or model access.");
    if(status==429)return juce::Result::fail("The service quota or rate limit was reached. Check API billing and retry later.");
    if(status<200||status>=300){
        if(local&&status==404)return juce::Result::fail(discovery?"Ollama API not found. Enter the server address, normally http://127.0.0.1:11434.":"The selected model is not installed in Ollama. Refresh the model list or download the model in Ollama.");
        // Only expose bounded Ollama error text; online providers may echo credentials.
        const auto detail=local?get(envelope,"error").toString().substring(0,500):juce::String();
        return juce::Result::fail("The service returned HTTP "+juce::String(status)+". "+detail);
    }
    if(parsed.failed())return juce::Result::fail("The service returned an unreadable response.");
    if(local&&get(envelope,"error").toString().isNotEmpty())return juce::Result::fail(get(envelope,"error").toString().substring(0,500));
    return juce::Result::ok();
}
}
juce::Result aiOllamaEndpoint(const juce::String& input,juce::String& endpoint){
    auto value=input.trim().trimCharactersAtEnd("/");
    if(value.isEmpty())value="http://127.0.0.1:11434";
    if(!value.contains("://"))value="http://"+value;
    // Accept an address copied from common Ollama setup instructions.
    for(const auto* suffix:{"/api/chat","/api/tags","/api","/v1"})
        if(value.endsWithIgnoreCase(suffix)){value=value.dropLastCharacters((int)std::strlen(suffix)).trimCharactersAtEnd("/");break;}
    const juce::URL url(value);
    if((!value.startsWithIgnoreCase("http://")&&!value.startsWithIgnoreCase("https://"))||url.getDomain().isEmpty()||value.containsAnyOf("\r\n?#@ \t"))
        return juce::Result::fail("Enter the Ollama server address, for example http://127.0.0.1:11434.");
    endpoint=value;return juce::Result::ok();
}
juce::Result aiOllamaModels(const juce::String& address,juce::StringArray& models,const std::function<bool()>& cancelled){
    models.clear();juce::String endpoint;auto result=aiOllamaEndpoint(address,endpoint);if(result.failed())return result;
    juce::var root;result=fetchJson({endpoint+"/api/tags","Content-Type: application/json\n",{}},true,true,root,cancelled);if(result.failed())return result;
    const auto items=get(root,"models");if(!items.isArray())return juce::Result::fail("This address did not return an Ollama model list. Check the server address.");
    for(const auto& item:*items.getArray()){
        const auto name=get(item,"name").toString().trim();
        // Cloud-backed models require an account and are not offline models.
        if(get(item,"remote_host").toString().isEmpty()&&get(item,"remote_model").toString().isEmpty()&&name.isNotEmpty()&&name.length()<=160&&!name.containsAnyOf("\r\n")&&!name.containsIgnoreCase(":cloud")&&!name.containsIgnoreCase("-cloud"))models.addIfNotAlreadyThere(name);
    }
    models.sort(true);
    if(models.isEmpty())return juce::Result::fail("Ollama is connected but no local models are installed. Download a text model in Ollama, then refresh the list.");
    return juce::Result::ok();
}
juce::Result aiGenerate(const Project& base,const AiOptions& options,Project& output,juce::String& summary,const std::function<bool()>& cancelled){
    AiHttpRequest request;auto check=aiHttpRequest(base,options,request);if(check.failed())return check;
    juce::var envelope;check=fetchJson(request,options.provider==AiProvider::ollama,false,envelope,cancelled);if(check.failed())return check;
    juce::String composition;check=aiResponse(options.provider,envelope,composition);if(check.failed())return check;
    if(cancelled())return juce::Result::fail("Generation cancelled.");
    return aiCompile(base,options,composition,output,summary);
}
}
