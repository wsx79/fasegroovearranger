// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ProjectIO.h"
#include <cmath>
namespace fga {
namespace {
juce::var object(){return new juce::DynamicObject;}
void set(juce::var& o,const char* key,const juce::var& v){o.getDynamicObject()->setProperty(key,v);}
void set(juce::var& o,const char* key,const std::string& v){set(o,key,juce::var(juce::String::fromUTF8(v.c_str())));}
void set(juce::var& o,const char* key,const char* v){set(o,key,juce::var(v));}
double num(const juce::var& o,const char* key,double fallback){auto v=o.getProperty(key,fallback);if(!(v.isDouble()||v.isInt()||v.isInt64()))return fallback;double value=(double)v;return std::isfinite(value)?std::clamp(value,-1000000.,1000000.):fallback;}
std::string str(const juce::var& o,const char* key,const char* fallback=""){return o.getProperty(key,fallback).toString().toStdString();}
}
juce::var projectJson(const Project& p){
    auto o=object();set(o,"format","fase-groove-arranger");set(o,"version",1);set(o,"name",p.name);
    auto s=object();const auto& c=p.settings;
    set(s,"genre",c.genre);set(s,"key",c.key);set(s,"scale",c.scale);set(s,"extension",c.extension);set(s,"voicing",c.voicing);
    set(s,"bpm",c.bpm);set(s,"swing",c.swing);set(s,"density",c.density);set(s,"humanize",c.humanize);set(s,"seed",(juce::int64)c.seed);
    set(s,"bassMode",c.bassMode);set(s,"stringsMode",c.stringsMode);set(s,"chordCount",c.chordCount);
    juce::Array<juce::var> degrees,free;for(int v:c.degrees)degrees.add(v);for(int v:c.freeChords)free.add(v);set(s,"degrees",degrees);set(s,"freeChords",free);set(o,"settings",s);
    set(o,"masterGain",p.masterGain);set(o,"loop",p.loop);set(o,"loopStart",p.loopStart);set(o,"loopEnd",p.loopEnd);
    juce::Array<juce::var> sections,tracks;
    for(const auto& section:p.sections){auto a=object();set(a,"type",(int)section.type);set(a,"bars",section.bars);set(a,"name",section.name);sections.add(a);}set(o,"sections",sections);
    for(const auto& t:p.tracks){auto a=object();set(a,"name",t.name);set(a,"kind",t.kind);set(a,"gain",t.gain);set(a,"pan",t.pan);set(a,"mute",t.mute);set(a,"solo",t.solo);set(a,"locked",t.locked);set(a,"sound",t.sound);set(a,"sampleRoot",t.sampleRoot);
        set(a,"instrument",t.instrument);set(a,"instrumentPath",t.instrumentPath);set(a,"instrumentId",t.instrumentId);set(a,"instrumentState",t.instrumentState);
        juce::Array<juce::var> clips;for(const auto& clip:t.clips){auto b=object();set(b,"seed",(juce::int64)clip.seed);set(b,"locked",clip.locked);juce::Array<juce::var> notes;
            for(const auto& n:clip.notes){juce::Array<juce::var> data;data.add(n.start);data.add(n.length);data.add(n.pitch);data.add(n.velocity);data.add(n.lane);notes.add(data);}set(b,"notes",notes);clips.add(b);}set(a,"clips",clips);tracks.add(a);
    }set(o,"tracks",tracks);return o;
}
juce::Result projectFromJson(const juce::var& o,Project& result){
    if(!o.isObject()||o["format"].toString()!="fase-groove-arranger"||(int)o["version"]!=1)return juce::Result::fail("Formato progetto non supportato.");
    auto* sections=o["sections"].getArray();auto* tracks=o["tracks"].getArray();
    if(!sections||sections->isEmpty()||sections->size()>64||!tracks||tracks->size()!=trackCount)return juce::Result::fail("Struttura del progetto non valida (17 tracce, 1–64 sezioni).");
    Project p;p.name=str(o,"name","Untitled groove");auto& s=p.settings;const auto j=o["settings"];
    s.genre=(int)num(j,"genre",1);s.key=(int)num(j,"key",9);s.scale=(int)num(j,"scale",1);s.extension=(int)num(j,"extension",2);s.voicing=(int)num(j,"voicing",0);
    s.bpm=num(j,"bpm",122);s.swing=num(j,"swing",.2);s.density=num(j,"density",.55);s.humanize=num(j,"humanize",.2);s.seed=(std::uint32_t)(juce::int64)j.getProperty("seed",1979);
    s.bassMode=(int)num(j,"bassMode",3);s.stringsMode=(int)num(j,"stringsMode",1);s.chordCount=(int)num(j,"chordCount",4);
    for(int i=0;i<8;++i){if(j["degrees"].isArray()&&j["degrees"].size()>i)s.degrees[(size_t)i]=(int)j["degrees"][i];if(j["freeChords"].isArray()&&j["freeChords"].size()>i)s.freeChords[(size_t)i]=(int)j["freeChords"][i];}
    p.masterGain=num(o,"masterGain",.8);p.loop=(bool)o["loop"];p.loopStart=num(o,"loopStart",0);p.loopEnd=num(o,"loopEnd",0);
    for(const auto& a:*sections){if(!a.isObject())return juce::Result::fail("Sezione non valida.");p.sections.push_back({(Section)(int)num(a,"type",0),(int)num(a,"bars",8),str(a,"name","Main")});}
    size_t noteCount=0;
    for(const auto& a:*tracks){if(!a.isObject())return juce::Result::fail("Traccia non valida.");Track t;t.name=str(a,"name","Track");t.gain=num(a,"gain",.75);t.pan=num(a,"pan",0);t.mute=(bool)a["mute"];t.solo=(bool)a["solo"];t.locked=(bool)a["locked"];t.sound=(int)num(a,"sound",0);t.sampleRoot=(int)num(a,"sampleRoot",60);
        t.instrument=str(a,"instrument","internal");if(t.instrument!="internal"&&t.instrument!="sample"&&t.instrument!="plugin")return juce::Result::fail("Tipo strumento non valido.");
        t.instrumentPath=str(a,"instrumentPath");t.instrumentId=str(a,"instrumentId");t.instrumentState=str(a,"instrumentState");
        auto* clips=a["clips"].getArray();if(!clips||clips->size()!=sections->size())return juce::Result::fail("Clip e sezioni non corrispondono.");
        for(const auto& b:*clips){Clip c;c.seed=(std::uint32_t)(juce::int64)b.getProperty("seed",1);c.locked=(bool)b["locked"];auto* notes=b["notes"].getArray();if(!notes)return juce::Result::fail("Note mancanti.");
            noteCount+=(size_t)notes->size();if(noteCount>2000000)return juce::Result::fail("Progetto troppo grande (massimo 2 milioni di note).");
            for(const auto& n:*notes){if(!n.isArray()||n.size()<4)return juce::Result::fail("Evento MIDI non valido.");double start=(double)n[0],length=(double)n[1];if(!std::isfinite(start)||!std::isfinite(length))return juce::Result::fail("Tempo MIDI non valido.");c.notes.push_back({start,length,(int)n[2],(int)n[3],n.size()>4?(int)n[4]:0});}t.clips.push_back(std::move(c));
        }p.tracks.push_back(std::move(t));
    }
    p.normalise();result=std::move(p);return juce::Result::ok();
}
juce::Result saveProject(const Project& p,const juce::File& file){juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithText(juce::JSON::toString(projectJson(p))))return juce::Result::fail("Scrittura progetto fallita.");return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Impossibile sostituire il progetto.");}
juce::Result loadProject(const juce::File& file,Project& p){if(!file.existsAsFile()||file.getSize()>256*1024*1024)return juce::Result::fail("File mancante o superiore a 256 MB.");juce::var json;auto r=juce::JSON::parse(file.loadFileAsString(),json);return r.failed()?r:projectFromJson(json,p);}
juce::Result importMidi(const juce::File& file,Clip& clip,double beats){
    auto in=file.createInputStream();if(!in)return juce::Result::fail("Impossibile aprire MIDI.");juce::MidiFile midi;if(!midi.readFrom(*in))return juce::Result::fail("File MIDI non valido.");const int ticks=midi.getTimeFormat();if(ticks<=0)return juce::Result::fail("MIDI SMPTE non supportato: usare PPQ.");
    Clip next=clip;next.notes.clear();
    for(int ti=0;ti<midi.getNumTracks();++ti){auto sequence=*midi.getTrack(ti);sequence.updateMatchedPairs();for(int i=0;i<sequence.getNumEvents();++i){auto* e=sequence.getEventPointer(i);if(e->message.isNoteOn()&&e->noteOffObject){auto start=e->message.getTimeStamp()/ticks;auto length=(e->noteOffObject->message.getTimeStamp()-e->message.getTimeStamp())/ticks;if(start<beats)next.notes.push_back({start,length,e->message.getNoteNumber(),e->message.getVelocity(),0});}}}
    tidyNotes(next,beats);if(next.notes.empty())return juce::Result::fail("Nessuna nota nella durata della sezione selezionata.");clip=std::move(next);return juce::Result::ok();
}
}
