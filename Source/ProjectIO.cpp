// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ProjectIO.h"
#include "StudioIO.h"
#include <cmath>
namespace fga {
namespace {
juce::var object(){return new juce::DynamicObject;}
void set(juce::var& o,const char* key,const juce::var& v){o.getDynamicObject()->setProperty(key,v);}
void set(juce::var& o,const char* key,const std::string& v){set(o,key,juce::var(juce::String::fromUTF8(v.c_str())));}
void set(juce::var& o,const char* key,const char* v){set(o,key,juce::var(v));}
double num(const juce::var& o,const char* key,double fallback){auto v=o.getProperty(key,fallback);if(!(v.isDouble()||v.isInt()||v.isInt64()))return fallback;double value=(double)v;return std::isfinite(value)?std::clamp(value,-1000000.,1000000.):fallback;}
std::string str(const juce::var& o,const char* key,const char* fallback=""){return o.getProperty(key,fallback).toString().toStdString();}
juce::var engineJson(const EngineOptions& e){auto o=object();
    set(o,"pattern",e.pattern);
    set(o,"riffSeed",(juce::int64)e.riffSeed);set(o,"humanizeSeed",(juce::int64)e.humanizeSeed);set(o,"pumpRate",e.pumpRate);set(o,"pumpSource",e.pumpSource);set(o,"pedalCC",e.pedalCC);
    set(o,"velocity",e.velocity);
    set(o,"grooveBars",e.grooveBars);
    set(o,"fillEvery",e.fillEvery);
    set(o,"fillStyle",e.fillStyle);
    set(o,"variationEvery",e.variationEvery);
    set(o,"centre",e.centre);
    set(o,"movement",e.movement);
    set(o,"strumDirection",e.strumDirection);
    set(o,"topLine",e.topLine);
    set(o,"approach",e.approach);
    set(o,"evolveBars",e.evolveBars);
    set(o,"humanizeStyle",e.humanizeStyle);
    set(o,"bassFeel",e.bassFeel);
    set(o,"rangeLow",e.rangeLow);
    set(o,"rangeHigh",e.rangeHigh);
    set(o,"midiNote",e.midiNote);set(o,"globalOctave",e.globalOctave);
    set(o,"swing",e.swing);
    set(o,"humanize",e.humanize);
    set(o,"density",e.density);
    set(o,"gate",e.gate);
    set(o,"strum",e.strum);
    set(o,"accent",e.accent);
    set(o,"tone",e.tone);
    set(o,"reverb",e.reverb);set(o,"chorus",e.chorus);set(o,"pump",e.pump);set(o,"pumpRelease",e.pumpRelease);
    set(o,"octaveTop",e.octaveTop);
    set(o,"realism",e.realism);
    set(o,"letRing",e.letRing);
    set(o,"roomForBand",e.roomForBand);
    set(o,"pushChanges",e.pushChanges);
    set(o,"afterFill",e.afterFill);
    set(o,"genreFeel",e.genreFeel);
    set(o,"bassSpace",e.bassSpace);
    {juce::Array<juce::var> values;for(auto v:e.segments)values.add(v);set(o,"segments",values);}
    {juce::Array<juce::var> values;for(auto v:e.laneVolume)values.add(v);set(o,"laneVolume",values);}
    {juce::Array<juce::var> values;for(auto v:e.laneLevel)values.add(v);set(o,"laneLevel",values);}
    {juce::Array<juce::var> values;for(auto v:e.laneSwing)values.add(v);set(o,"laneSwing",values);}
    {juce::Array<juce::var> values;for(auto v:e.laneShift)values.add(v);set(o,"laneShift",values);}
    {juce::Array<juce::var> values;for(auto v:e.laneOctave)values.add(v);set(o,"laneOctave",values);}
    {juce::Array<juce::var> values;for(auto v:e.laneMute)values.add(v);set(o,"laneMute",values);}
    {juce::Array<juce::var> a;for(auto v:e.inversions)a.add((int)v);set(o,"inversions",a);}
    {juce::Array<juce::var> a;for(auto v:e.velocityOrder)a.add((int)v);set(o,"velocityOrder",a);}
    {juce::Array<juce::var> a;for(auto v:e.segmentLocked)a.add((int)v);set(o,"segmentLocked",a);}
    {juce::Array<juce::var> a;for(auto v:e.userRight)a.add((int)v);set(o,"userRight",a);}
    {juce::Array<juce::var> a;for(auto v:e.userLeft)a.add((int)v);set(o,"userLeft",a);}
    {juce::Array<juce::var> a;for(auto v:e.userVelRight)a.add((int)v);set(o,"userVelRight",a);}
    {juce::Array<juce::var> a;for(auto v:e.userVelLeft)a.add((int)v);set(o,"userVelLeft",a);}
    return o;
}
void readEngine(const juce::var& o,EngineOptions& e){
    e.riffSeed=(std::uint32_t)(juce::int64)o.getProperty("riffSeed",0);e.humanizeSeed=(std::uint32_t)(juce::int64)o.getProperty("humanizeSeed",0);e.pumpRate=(int)num(o,"pumpRate",0);e.pumpSource=(int)num(o,"pumpSource",0);e.pedalCC=(bool)o.getProperty("pedalCC",false);
    if(auto* a=o["inversions"].getArray())for(size_t i=0;i<e.inversions.size()&&i<(size_t)a->size();++i)e.inversions[i]=(int)(*a)[(int)i];
    if(auto* a=o["velocityOrder"].getArray())for(size_t i=0;i<e.velocityOrder.size()&&i<(size_t)a->size();++i)e.velocityOrder[i]=(int)(*a)[(int)i];
    if(auto* a=o["segmentLocked"].getArray())for(size_t i=0;i<e.segmentLocked.size()&&i<(size_t)a->size();++i)e.segmentLocked[i]=(int)(*a)[(int)i];
    if(auto* a=o["userRight"].getArray())for(size_t i=0;i<e.userRight.size()&&i<(size_t)a->size();++i)e.userRight[i]=(int)(*a)[(int)i];
    if(auto* a=o["userLeft"].getArray())for(size_t i=0;i<e.userLeft.size()&&i<(size_t)a->size();++i)e.userLeft[i]=(int)(*a)[(int)i];
    if(auto* a=o["userVelRight"].getArray())for(size_t i=0;i<e.userVelRight.size()&&i<(size_t)a->size();++i)e.userVelRight[i]=(int)(*a)[(int)i];
    if(auto* a=o["userVelLeft"].getArray())for(size_t i=0;i<e.userVelLeft.size()&&i<(size_t)a->size();++i)e.userVelLeft[i]=(int)(*a)[(int)i];
    e.pattern=(int)num(o,"pattern",e.pattern);
    e.velocity=(int)num(o,"velocity",e.velocity);
    e.grooveBars=(int)num(o,"grooveBars",e.grooveBars);
    e.fillEvery=(int)num(o,"fillEvery",e.fillEvery);
    e.fillStyle=(int)num(o,"fillStyle",e.fillStyle);
    e.variationEvery=(int)num(o,"variationEvery",e.variationEvery);
    e.centre=(int)num(o,"centre",e.centre);
    e.movement=(int)num(o,"movement",e.movement);
    e.strumDirection=(int)num(o,"strumDirection",e.strumDirection);
    e.topLine=(int)num(o,"topLine",e.topLine);
    e.approach=(int)num(o,"approach",e.approach);
    e.evolveBars=(int)num(o,"evolveBars",e.evolveBars);
    e.humanizeStyle=(int)num(o,"humanizeStyle",e.humanizeStyle);
    e.bassFeel=(int)num(o,"bassFeel",e.bassFeel);
    e.rangeLow=(int)num(o,"rangeLow",e.rangeLow);
    e.rangeHigh=(int)num(o,"rangeHigh",e.rangeHigh);
    e.midiNote=(int)num(o,"midiNote",e.midiNote);e.globalOctave=(int)num(o,"globalOctave",0);
    e.swing=num(o,"swing",e.swing);
    e.humanize=num(o,"humanize",e.humanize);
    e.density=num(o,"density",e.density);
    e.gate=num(o,"gate",e.gate);
    e.strum=num(o,"strum",e.strum);
    e.accent=num(o,"accent",e.accent);
    e.tone=num(o,"tone",e.tone);
    e.reverb=num(o,"reverb",e.reverb);e.chorus=num(o,"chorus",e.chorus);e.pump=num(o,"pump",e.pump);e.pumpRelease=num(o,"pumpRelease",e.pumpRelease);
    e.octaveTop=(bool)o.getProperty("octaveTop",e.octaveTop);
    e.realism=(bool)o.getProperty("realism",e.realism);
    e.letRing=(bool)o.getProperty("letRing",e.letRing);
    e.roomForBand=(bool)o.getProperty("roomForBand",e.roomForBand);
    e.pushChanges=(bool)o.getProperty("pushChanges",e.pushChanges);
    e.afterFill=(bool)o.getProperty("afterFill",e.afterFill);
    e.genreFeel=(bool)o.getProperty("genreFeel",e.genreFeel);
    e.bassSpace=(bool)o.getProperty("bassSpace",e.bassSpace);
    if(auto* a=o["segments"].getArray())for(size_t i=0;i<e.segments.size()&&i<(size_t)a->size();++i)e.segments[i]=(int)(*a)[(int)i];
    if(auto* a=o["laneVolume"].getArray())for(size_t i=0;i<e.laneVolume.size()&&i<(size_t)a->size();++i)e.laneVolume[i]=(double)(*a)[(int)i];
    if(auto* a=o["laneLevel"].getArray())for(size_t i=0;i<e.laneLevel.size()&&i<(size_t)a->size();++i)e.laneLevel[i]=(double)(*a)[(int)i];
    if(auto* a=o["laneSwing"].getArray())for(size_t i=0;i<e.laneSwing.size()&&i<(size_t)a->size();++i)e.laneSwing[i]=(double)(*a)[(int)i];
    if(auto* a=o["laneShift"].getArray())for(size_t i=0;i<e.laneShift.size()&&i<(size_t)a->size();++i)e.laneShift[i]=(double)(*a)[(int)i];
    if(auto* a=o["laneOctave"].getArray())for(size_t i=0;i<e.laneOctave.size()&&i<(size_t)a->size();++i)e.laneOctave[i]=(int)(*a)[(int)i];
    if(auto* a=o["laneMute"].getArray())for(size_t i=0;i<e.laneMute.size()&&i<(size_t)a->size();++i)e.laneMute[i]=(bool)(*a)[(int)i];
}

}
// An automation envelope as saved since the first volume lanes: {"enabled", "points": [[beat, value, curve]...]}.
static juce::var envelopeJson(const VolumeAutomation& v){
    auto o=object();set(o,"enabled",v.enabled);if(v.locked)set(o,"locked",true);juce::Array<juce::var> points;
    for(const auto& p:v.points){juce::Array<juce::var> point;point.add(p.beat);point.add(p.value);point.add(p.curve);points.add(point);}
    set(o,"points",points);return o;
}
static juce::Result readEnvelope(const juce::var& o,VolumeAutomation& v){
    v.enabled=(bool)o.getProperty("enabled",false);v.locked=(bool)o.getProperty("locked",false);
    if(auto* points=o["points"].getArray()){
        if(points->size()>16384)return juce::Result::fail("Too many volume automation points.");
        for(const auto& point:*points){
            if(!point.isArray()||point.size()!=3)return juce::Result::fail("Invalid volume automation point.");
            for(int k=0;k<3;++k)if(!(point[k].isDouble()||point[k].isInt()||point[k].isInt64())||!std::isfinite((double)point[k]))return juce::Result::fail("Invalid volume automation value.");
            v.points.push_back({(double)point[0],(double)point[1],(double)point[2]});
        }
    }
    return juce::Result::ok();
}
juce::var projectJson(const Project& p){
    auto o=object();set(o,"format","fase-groove-arranger");set(o,"version",std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return !t.effects.empty()||t.instrumentState.rfind(hostedStatePrefix,0)==0||t.parameters.size()>8||std::any_of(t.parameters.begin(),t.parameters.end(),[](const auto& l){return l.slot<0;});})?4:extendedProject(p)?3:std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.freeTimeline;})?2:1);set(o,"name",p.name);
    auto s=object();const auto& c=p.settings;
    set(s,"genre",c.genre);set(s,"key",c.key);set(s,"scale",c.scale);set(s,"extension",c.extension);set(s,"voicing",c.voicing);
    set(s,"bpm",c.bpm);set(s,"swing",c.swing);set(s,"density",c.density);set(s,"humanize",c.humanize);set(s,"seed",(juce::int64)c.seed);
    set(s,"guitarLead",c.guitarLead);set(s,"randomAutomation",c.randomAutomation);if(c.freeTempo)set(s,"freeTempo",true);
    set(s,"bassMode",c.bassMode);set(s,"stringsMode",c.stringsMode);set(s,"chordCount",c.chordCount);set(s,"chordSteps",c.chordSteps);
    juce::Array<juce::var> degrees,free;for(int v:c.degrees)degrees.add(v);for(int v:c.freeChords)free.add(v);set(s,"degrees",degrees);set(s,"freeChords",free);set(o,"settings",s);
    if(extendedProject(p))set(o,"buses",busesJson(p.buses));set(o,"masterGain",p.masterGain);if(!p.masterAutomation.isDefault())set(o,"masterAutomation",envelopeJson(p.masterAutomation));set(o,"loop",p.loop);set(o,"loopStart",p.loopStart);set(o,"loopEnd",p.loopEnd);
    juce::Array<juce::var> sections,tracks;
    for(const auto& section:p.sections){auto a=object();set(a,"type",(int)section.type);set(a,"bars",section.bars);set(a,"name",section.name);sections.add(a);}set(o,"sections",sections);
    for(const auto& t:p.tracks){auto a=object();set(a,"name",t.name);set(a,"kind",t.kind);set(a,"gain",t.gain);set(a,"pan",t.pan);set(a,"mute",t.mute);set(a,"solo",t.solo);set(a,"locked",t.locked);set(a,"sound",t.sound);set(a,"sampleRoot",t.sampleRoot);
        if(!t.sampler.isDefault())set(a,"sampler",samplerJson(t.sampler));if(!t.sampleHash.empty())set(a,"sampleHash",t.sampleHash);if(t.group>=0)set(a,"group",t.group);if(t.sends[0]!=0||t.sends[1]!=0){juce::Array<juce::var> sends;for(auto v:t.sends)sends.add(v);set(a,"sends",sends);}
        if(t.vocalChops)set(a,"vocalChops",true);set(a,"engine",engineJson(t.engine));if(t.freeTimeline)set(a,"freeTimeline",true);
        set(a,"volumeAutomation",envelopeJson(t.volume));
        // Pan and plugin automation are written only when used: older projects save as before.
        if(!t.panAutomation.isDefault())set(a,"panAutomation",envelopeJson(t.panAutomation));
        if(!t.parameters.empty()){juce::Array<juce::var> lanes;for(const auto& lane:t.parameters){auto l=envelopeJson(lane.curve);set(l,"slot",lane.slot);set(l,"index",lane.index);set(l,"name",lane.name);if(!lane.parameterId.empty())set(l,"parameterId",lane.parameterId);lanes.add(l);}set(a,"parameterAutomation",lanes);}
        set(a,"instrument",t.instrument);set(a,"instrumentPath",t.instrumentPath);set(a,"instrumentId",t.instrumentId);set(a,"instrumentState",t.instrumentState);
        if(!t.effects.empty()){juce::Array<juce::var> effects;for(const auto& e:t.effects){auto fx=object();set(fx,"slot",e.slot);set(fx,"name",e.name);set(fx,"pluginName",e.pluginName);set(fx,"path",e.path);set(fx,"pluginId",e.pluginId);set(fx,"state",e.state);set(fx,"bypassed",e.bypassed);effects.add(fx);}set(a,"effects",effects);}
        juce::Array<juce::var> clips;for(const auto& clip:t.clips){auto b=object();set(b,"seed",(juce::int64)clip.seed);if(clip.motifSeed)set(b,"motifSeed",(juce::int64)clip.motifSeed);set(b,"locked",clip.locked);if(t.freeTimeline){set(b,"position",clip.position);set(b,"role",clip.role);}if(clip.span>0)set(b,"span",clip.span);
            if(clip.loopLength>0){set(b,"loopLength",clip.loopLength);juce::Array<juce::var> source;for(const auto& n:clip.loopSource){juce::Array<juce::var> data;data.add(n.start);data.add(n.length);data.add(n.pitch);data.add(n.velocity);data.add(n.lane);source.add(data);}set(b,"loopSource",source);}juce::Array<juce::var> notes;
            for(const auto& n:clip.notes){juce::Array<juce::var> data;data.add(n.start);data.add(n.length);data.add(n.pitch);data.add(n.velocity);data.add(n.lane);notes.add(data);}set(b,"notes",notes);clips.add(b);}set(a,"clips",clips);tracks.add(a);
    }set(o,"tracks",tracks);return o;
}
juce::Result projectFromJson(const juce::var& o,Project& result){
    if(!o.isObject()||o["format"].toString()!="fase-groove-arranger"||((int)o["version"]!=1&&(int)o["version"]!=2&&(int)o["version"]!=3&&(int)o["version"]!=4))return juce::Result::fail("Formato progetto non supportato.");
    auto* sections=o["sections"].getArray();auto* tracks=o["tracks"].getArray();
    if(!sections||sections->isEmpty()||sections->size()>64||!tracks||(tracks->size()<1||tracks->size()>maxTracks))return juce::Result::fail(juce::String::fromUTF8("Struttura del progetto non valida (1–32 tracce, 1–64 sezioni)."));
    Project p;p.name=str(o,"name","Untitled groove");auto& s=p.settings;const auto j=o["settings"];
    s.genre=(int)num(j,"genre",1);s.key=(int)num(j,"key",9);s.scale=(int)num(j,"scale",1);s.extension=(int)num(j,"extension",2);s.voicing=(int)num(j,"voicing",0);
    s.bpm=num(j,"bpm",122);s.swing=num(j,"swing",.2);s.density=num(j,"density",.55);s.humanize=num(j,"humanize",.2);s.seed=(std::uint32_t)(juce::int64)j.getProperty("seed",1979);
    s.chordSteps=(int)num(j,"chordSteps",16);s.guitarLead=(bool)j.getProperty("guitarLead",false);s.randomAutomation=(bool)j.getProperty("randomAutomation",true);s.freeTempo=(bool)j.getProperty("freeTempo",false);
    s.bassMode=(int)num(j,"bassMode",3);s.stringsMode=(int)num(j,"stringsMode",1);s.chordCount=(int)num(j,"chordCount",4);
    for(int i=0;i<8;++i){if(j["degrees"].isArray()&&j["degrees"].size()>i)s.degrees[(size_t)i]=(int)j["degrees"][i];if(j["freeChords"].isArray()&&j["freeChords"].size()>i)s.freeChords[(size_t)i]=(int)j["freeChords"][i];}
    if(auto r=readBuses(o["buses"],p.buses);r.failed())return r;
    p.masterGain=num(o,"masterGain",.8);if(auto r=readEnvelope(o["masterAutomation"],p.masterAutomation);r.failed())return r;p.loop=(bool)o["loop"];p.loopStart=num(o,"loopStart",0);p.loopEnd=num(o,"loopEnd",0);
    for(const auto& a:*sections){if(!a.isObject())return juce::Result::fail("Sezione non valida.");p.sections.push_back({(Section)(int)num(a,"type",0),(int)num(a,"bars",8),str(a,"name","Main")});}
    const bool migrateSlots=tracks->size()<legacyTrackCount;std::array<bool,maxTracks> usedSlots{};
    size_t noteCount=0;
    for(const auto& a:*tracks){if(!a.isObject())return juce::Result::fail("Traccia non valida.");Track t;t.kind=(int)p.tracks.size();if(migrateSlots){const double id=num(a,"kind",t.kind);if(id<0||id>=maxTracks||std::floor(id)!=id||usedSlots[(size_t)(int)id])return juce::Result::fail("Invalid or duplicate legacy track slot.");t.kind=(int)id;usedSlots[(size_t)t.kind]=true;}t.name=str(a,"name","Track");t.gain=num(a,"gain",.75);t.pan=num(a,"pan",0);t.mute=(bool)a["mute"];t.solo=(bool)a["solo"];t.locked=(bool)a["locked"];t.sound=(int)num(a,"sound",0);t.sampleRoot=(int)num(a,"sampleRoot",60);
        if(auto r=readSampler(a["sampler"],t.sampler);r.failed())return r;t.sampleHash=str(a,"sampleHash");t.group=(int)num(a,"group",-1);if(a.hasProperty("sends")){auto* sends=a["sends"].getArray();if(!sends||sends->size()!=2)return juce::Result::fail("Invalid send routing.");for(int i=0;i<2;++i)t.sends[(size_t)i]=(double)(*sends)[i];}
        t.vocalChops=(bool)a.getProperty("vocalChops",false);readEngine(a["engine"],t.engine);
        if(auto r=readEnvelope(a["volumeAutomation"],t.volume);r.failed())return r;
        if(auto r=readEnvelope(a["panAutomation"],t.panAutomation);r.failed())return r;
        if(a.hasProperty("parameterAutomation")){auto* lanes=a["parameterAutomation"].getArray();if(!lanes||lanes->size()>maxParameterLanes)return juce::Result::fail("Invalid plugin automation.");
            for(const auto& l:*lanes){if(!l.isObject())return juce::Result::fail("Invalid plugin automation lane.");ParameterAutomation lane;const double slot=num(l,"slot",0);if(slot< -1||slot>maxTrackEffects||std::floor(slot)!=slot)return juce::Result::fail("Invalid automation target.");lane.slot=(int)slot;lane.index=(int)num(l,"index",-1);lane.name=str(l,"name");lane.parameterId=str(l,"parameterId");if(auto r=readEnvelope(l,lane.curve);r.failed())return r;t.parameters.push_back(std::move(lane));}}
        t.instrument=str(a,"instrument","internal");if(t.instrument!="internal"&&t.instrument!="sample"&&t.instrument!="plugin")return juce::Result::fail("Tipo strumento non valido.");
        t.instrumentPath=str(a,"instrumentPath");t.instrumentId=str(a,"instrumentId");t.instrumentState=str(a,"instrumentState");
        if(a.hasProperty("effects")){auto* effects=a["effects"].getArray();if(!effects||effects->size()>maxTrackEffects)return juce::Result::fail("Invalid effect chain.");
            for(const auto& fx:*effects){const double slot=num(fx,"slot",0);if(!fx.isObject()||slot<1||slot>maxTrackEffects||std::floor(slot)!=slot||t.effect((int)slot))return juce::Result::fail("Invalid or duplicate effect slot.");EffectSlot e;e.slot=(int)slot;e.name=str(fx,"name");e.pluginName=str(fx,"pluginName");e.path=str(fx,"path");e.pluginId=str(fx,"pluginId");e.state=str(fx,"state");e.bypassed=(bool)fx["bypassed"];if(e.path.empty()&&e.pluginId.empty())return juce::Result::fail("Missing effect identifier.");t.effects.push_back(std::move(e));}
        }else if(!str(a,"effectPath").empty()||!str(a,"effectId").empty()){EffectSlot e;e.path=str(a,"effectPath");e.pluginId=str(a,"effectId");e.state=str(a,"effectState");if(auto xml=juce::XmlDocument::parse(juce::String::fromUTF8(e.pluginId.c_str())))e.pluginName=xml->getStringAttribute("name").toStdString();if(e.pluginName.empty())e.pluginName=juce::File(juce::String::fromUTF8(e.path.c_str())).getFileNameWithoutExtension().toStdString();t.effects.push_back(std::move(e));}
        t.freeTimeline=(int)o["version"]>=2&&(bool)a.getProperty("freeTimeline",false);
        auto* clips=a["clips"].getArray();if(!clips||(t.freeTimeline?clips->size()>4096:clips->size()!=sections->size()))return juce::Result::fail("Clip e sezioni non corrispondono.");
        for(const auto& b:*clips){Clip c;c.motifSeed=(std::uint32_t)(juce::int64)b.getProperty("motifSeed",0);c.seed=(std::uint32_t)(juce::int64)b.getProperty("seed",1);c.locked=(bool)b["locked"];auto* notes=b["notes"].getArray();if(!notes)return juce::Result::fail("Note mancanti.");
            noteCount+=(size_t)notes->size();if(noteCount>maxProjectNotes)return juce::Result::fail("Progetto troppo grande (massimo 2 milioni di note).");
            for(const auto& n:*notes){if(!n.isArray()||n.size()<4)return juce::Result::fail("Evento MIDI non valido.");double start=(double)n[0],length=(double)n[1];if(!std::isfinite(start)||!std::isfinite(length))return juce::Result::fail("Tempo MIDI non valido.");c.notes.push_back({start,length,(int)n[2],(int)n[3],n.size()>4?(int)n[4]:0});}
            if(t.freeTimeline){const auto pos=b["position"],span=b["span"];if(!(pos.isDouble()||pos.isInt()||pos.isInt64())||!(span.isDouble()||span.isInt()||span.isInt64())||!std::isfinite((double)pos)||!std::isfinite((double)span)||(double)pos<0||(double)pos>131072||(double)span<1./960.||(double)span>512)return juce::Result::fail("Invalid timeline clip position or duration.");c.position=(double)pos;c.role=(int)num(b,"role",-1);}
            c.span=num(b,"span",0);c.loopLength=num(b,"loopLength",0);if(!std::isfinite(c.span)||!std::isfinite(c.loopLength))return juce::Result::fail("Invalid loop duration.");
            if(b.hasProperty("loopSource")){auto* source=b["loopSource"].getArray();if(!source)return juce::Result::fail("Invalid loop source.");noteCount+=(size_t)source->size();if(noteCount>maxProjectNotes)return juce::Result::fail("Project loop source is too large.");
                for(const auto& n:*source){if(!n.isArray()||n.size()<4)return juce::Result::fail("Invalid loop MIDI event.");double start=(double)n[0],length=(double)n[1];if(!std::isfinite(start)||!std::isfinite(length))return juce::Result::fail("Invalid loop MIDI time.");c.loopSource.push_back({start,length,(int)n[2],(int)n[3],n.size()>4?(int)n[4]:0});}}
            t.clips.push_back(std::move(c));
        }p.tracks.push_back(std::move(t));
    }
    // Sparse legacy arrays retain explicit stable IDs, including hosted instruments.
    // Missing slots are silent placeholders, never newly generated music.
    if(migrateSlots){auto saved=std::move(p.tracks);Project defaults;defaults.sections=p.sections;defaults.settings=p.settings;defaults.normalise();size_t count=trackCount;for(const auto& track:saved)count=std::max(count,(size_t)track.kind+1);p.tracks=std::move(defaults.tracks);p.tracks.resize(count);
        for(size_t i=0;i<count;++i){auto& track=p.tracks[i];track.kind=(int)i;track.mute=true;if(track.name.empty())track.name="MIDI "+std::to_string((int)i-trackCount+1);}
        for(auto& track:saved){const auto index=(size_t)track.kind;p.tracks[index]=std::move(track);}
    }
    p.normalise();result=std::move(p);return juce::Result::ok();
}
// Validate nesting and container budgets before JUCE allocates a recursive var tree.
juce::Result readBoundedJson(const juce::File& file,juce::var& json,juce::int64 maxBytes){
    if(!file.existsAsFile()||file.getSize()>maxBytes)return juce::Result::fail("JSON file is missing or too large.");
    auto input=file.createInputStream();if(!input)return juce::Result::fail("Cannot read JSON file.");
    juce::MemoryOutputStream validated;std::array<char,65536> buffer;juce::int64 bytes=0;
    while(!input->isExhausted()){
        const int count=input->read(buffer.data(),(int)buffer.size());if(count<=0)break;if((bytes+=count)>maxBytes)return juce::Result::fail("JSON file is too large.");validated.write(buffer.data(),(size_t)count);
    }
    if(input->getStatus().failed())return juce::Result::fail("Cannot read JSON file.");
    // Keep the previous reader's UTF-8/UTF-16 BOM handling, and validate exactly
    // the decoded immutable document that will be given to the JSON parser.
    const auto document=juce::String::createStringFromData(validated.getData(),(int)validated.getDataSize());
    auto characters=document.getCharPointer();int depth=0;size_t elements=0;bool quoted=false,escaped=false;
    while(!characters.isEmpty()){const auto ch=characters.getAndAdvance();
        if(quoted){if(escaped)escaped=false;else if(ch=='\\')escaped=true;else if(ch=='"')quoted=false;continue;}
        if(ch=='"'){quoted=true;continue;}
        if(ch=='{'||ch=='['){if(++depth>48)return juce::Result::fail("JSON nesting exceeds 48 levels.");++elements;}
        else if(ch=='}'||ch==']'){if(--depth<0)return juce::Result::fail("Invalid JSON structure.");}
        else if(ch==',')++elements;
        if(elements>14000000)return juce::Result::fail("JSON contains too many elements.");
    }
    if(quoted||depth!=0)return juce::Result::fail("Invalid JSON structure.");
    return juce::JSON::parse(document,json);
}
// Store sub-tick humanisation with 17 fractional places, far beyond MIDI/audio
// resolution and within JUCE's stable numeric parsing precision.
juce::Result saveProject(const Project& p,const juce::File& file){
    size_t notes=0;for(const auto& track:p.tracks)for(const auto& clip:track.clips){notes+=clip.notes.size()+clip.loopSource.size();if(notes>maxProjectNotes)return juce::Result::fail("Project exceeds the note limit (2 million notes).");}
    auto json=projectJson(p);relativiseProjectMedia(json,file);const auto text=juce::JSON::toString(json,false,17);
    if((juce::int64)text.getNumBytesAsUTF8()>maxProjectBytes)return juce::Result::fail("Project exceeds the file size limit (256 MB).");
    juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithText(text))return juce::Result::fail("Scrittura progetto fallita.");
    return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Impossibile sostituire il progetto.");
}
juce::Result loadProject(const juce::File& file,Project& p){juce::var json;auto r=readBoundedJson(file,json);if(r.failed())return r;Project next;auto parsed=projectFromJson(json,next);if(parsed.failed())return parsed;if(auto resolved=resolveProjectMedia(next,file);resolved.failed())return resolved;p=std::move(next);return juce::Result::ok();}
juce::Result importMidi(const juce::File& file,Clip& clip,double beats,size_t maxNotes){
    if(!file.existsAsFile()||file.getSize()>maxMidiBytes)return juce::Result::fail("MIDI file is missing or exceeds 64 MB.");maxNotes=std::min(maxNotes,maxProjectNotes);
    auto in=file.createInputStream();if(!in)return juce::Result::fail("Impossibile aprire MIDI.");juce::SubregionStream bounded(in.get(),0,maxMidiBytes,false);juce::MidiFile midi;if(!midi.readFrom(bounded))return juce::Result::fail("File MIDI non valido.");const int ticks=midi.getTimeFormat();if(ticks<=0)return juce::Result::fail("MIDI SMPTE non supportato: usare PPQ.");
    Clip next=clip;next.notes.clear();next.loopSource.clear();next.loopLength=0;
    for(int ti=0;ti<midi.getNumTracks();++ti){auto sequence=*midi.getTrack(ti);sequence.updateMatchedPairs();for(int i=0;i<sequence.getNumEvents();++i){auto* e=sequence.getEventPointer(i);if(e->message.isNoteOn()&&e->noteOffObject){auto start=e->message.getTimeStamp()/ticks;auto length=(e->noteOffObject->message.getTimeStamp()-e->message.getTimeStamp())/ticks;if(start<beats){if(next.notes.size()>=maxNotes)return juce::Result::fail("MIDI import exceeds the project note limit (2 million notes).");next.notes.push_back({start,length,e->message.getNoteNumber(),e->message.getVelocity(),0});}}}}
    tidyNotes(next,beats);if(next.notes.empty())return juce::Result::fail("Nessuna nota nella durata della sezione selezionata.");clip=std::move(next);return juce::Result::ok();
}
}
