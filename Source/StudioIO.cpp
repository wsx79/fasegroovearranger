// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "StudioIO.h"
#include "ProjectIO.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
namespace fga {
namespace {
juce::var object(){return juce::var(new juce::DynamicObject);}
void set(juce::var& o,const char* key,const juce::var& value){o.getDynamicObject()->setProperty(key,value);}
double number(const juce::var& o,const char* key,double fallback){auto v=o[key];return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v)?(double)v:fallback;}
// SHA256 consumes an InputStream; stop reads cooperatively at bounded blocks.
class CheckedInput : public juce::InputStream {
public:
    CheckedInput(juce::FileInputStream& stream,const std::function<bool()>& stop):input(stream),cancelled(stop){}
    juce::int64 getTotalLength()override{return input.getTotalLength();}
    juce::int64 getPosition()override{return input.getPosition();}
    bool setPosition(juce::int64 at)override{return !stopped()&&input.setPosition(at);}
    bool isExhausted()override{return stopped()||input.isExhausted();}
    int read(void* buffer,int bytes)override{return stopped()?0:input.read(buffer,std::min(bytes,65536));}
private:
    bool stopped()const{return cancelled&&cancelled();}
    juce::FileInputStream& input;const std::function<bool()>& cancelled;
};
juce::String hashFile(const juce::File& f,const std::function<bool()>& cancelled={}){
    auto in=f.createInputStream();if(!in)return {};CheckedInput checked(*in,cancelled);auto hash=juce::SHA256(checked).toHexString();
    return (cancelled&&cancelled())||in->getStatus().failed()||in->getPosition()!=f.getSize()?juce::String():hash;
}
bool absolute(const juce::String& p){return juce::File::isAbsolutePath(p)||(p.length()>2&&p[1]==':');}
juce::Result copyMedia(const juce::File& from,const juce::File& folder,juce::File& to,juce::String& hash,std::vector<juce::File>& created,const std::function<bool()>& cancelled={}){
    auto stopped=[&]{return cancelled&&cancelled();};
    if(stopped())return juce::Result::fail("Collection cancelled.");
    if(!from.existsAsFile())return juce::Result::fail("Missing sample: "+from.getFullPathName());
    if(from.getSize()>512ll*1024*1024)return juce::Result::fail("Sample file is too large to collect.");
    hash=hashFile(from,cancelled);if(hash.isEmpty())return juce::Result::fail(stopped()?"Collection cancelled.":"Cannot read sample: "+from.getFullPathName());
    if(auto r=folder.createDirectory();r.failed())return r;
    to=folder.getChildFile(hash+from.getFileExtension().toLowerCase());
    if(to.existsAsFile()){if(hashFile(to,cancelled)!=hash)return juce::Result::fail(stopped()?"Collection cancelled.":"Collected sample has changed: "+to.getFullPathName());return juce::Result::ok();}
    juce::TemporaryFile temp(to);auto input=from.createInputStream();auto output=temp.getFile().createOutputStream();
    if(!input||!output)return juce::Result::fail("Cannot copy sample: "+from.getFileName());
    std::array<char,65536> buffer;juce::int64 bytes=0;
    while(!input->isExhausted()){
        if(stopped())return juce::Result::fail("Collection cancelled.");
        const int n=input->read(buffer.data(),(int)buffer.size());if(n<=0)break;
        if(!output->write(buffer.data(),(size_t)n))return juce::Result::fail("Sample copy failed: "+from.getFileName());bytes+=n;
    }
    output->flush();const bool written=output->getStatus().wasOk();output.reset();
    if(stopped())return juce::Result::fail("Collection cancelled.");
    if(!written||bytes!=from.getSize()||hashFile(temp.getFile(),cancelled)!=hash||stopped()||!temp.overwriteTargetFileWithTemporary())return juce::Result::fail(stopped()?"Collection cancelled.":"Sample copy failed: "+from.getFileName());
    created.push_back(to);return juce::Result::ok();
}
}
juce::var samplerJson(const SamplerSettings& p){auto o=object();
    #define S(k) set(o,#k,p.k)
    S(mode);S(polyphony);S(transpose);S(sliceBase);S(loop);S(reverse);S(snap);S(filter);S(sync);
    S(sliceSnapBeats);S(start);S(end);S(loopStart);S(loopEnd);S(crossfade);S(fadeIn);S(fadeOut);S(attack);S(decay);S(sustain);S(release);S(fine);S(velocity);S(cutoff);S(resonance);S(sourceBpm);S(sensitivity);S(fadeInCurve);S(fadeOutCurve);S(attackCurve);S(decayCurve);S(releaseCurve);
    #undef S
    juce::Array<juce::var> slices;for(int i=0;i<p.sliceCount;++i)slices.add(p.slices[(size_t)i]);set(o,"slices",slices);return o;
}
juce::Result readSampler(const juce::var& o,SamplerSettings& p){
    if(o.isVoid())return juce::Result::ok();if(!o.isObject())return juce::Result::fail("Invalid sampler settings.");
    #define N(k) p.k=(decltype(p.k))number(o,#k,p.k)
    p.mode=(int)std::clamp(number(o,"mode",p.mode),-1.,2.);p.polyphony=(int)std::clamp(number(o,"polyphony",p.polyphony),1.,32.);p.transpose=(int)std::clamp(number(o,"transpose",p.transpose),-48.,48.);p.sliceBase=(int)std::clamp(number(o,"sliceBase",p.sliceBase),0.,127.);p.sliceSnapBeats=number(o,"sliceSnapBeats",0);N(start);N(end);N(loopStart);N(loopEnd);N(crossfade);N(fadeIn);N(fadeOut);N(attack);N(decay);N(sustain);N(release);N(fine);N(velocity);N(cutoff);N(resonance);N(sourceBpm);N(sensitivity);N(fadeInCurve);N(fadeOutCurve);N(attackCurve);N(decayCurve);N(releaseCurve);
    #undef N
    #define B(k) p.k=(bool)o.getProperty(#k,p.k)
    B(loop);B(reverse);B(snap);B(filter);B(sync);
    #undef B
    if(o.hasProperty("slices")){auto* a=o["slices"].getArray();if(!a||a->size()>128)return juce::Result::fail("Invalid sampler slices.");p.sliceCount=a->size();for(int i=0;i<a->size();++i){auto v=(*a)[i];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v))return juce::Result::fail("Invalid slice position.");p.slices[(size_t)i]=(double)v;}}
    p.normalise();return juce::Result::ok();
}
juce::var busesJson(const std::array<BusSettings,4>& buses){juce::Array<juce::var> a;for(const auto& p:buses){auto o=object();set(o,"name",p.name.c_str());set(o,"gain",p.gain);set(o,"pan",p.pan);set(o,"mute",p.mute);set(o,"solo",p.solo);set(o,"amount",p.amount);set(o,"damping",p.damping);set(o,"feedback",p.feedback);set(o,"division",p.division);set(o,"sidechainAmount",p.sidechainAmount);set(o,"sidechainSource",p.sidechainSource);set(o,"sidechainAttackMs",p.sidechainAttackMs);set(o,"sidechainReleaseMs",p.sidechainReleaseMs);set(o,"sidechainCurve",p.sidechainCurve);a.add(o);}return a;}
juce::Result readBuses(const juce::var& v,std::array<BusSettings,4>& buses){if(v.isVoid())return juce::Result::ok();auto* a=v.getArray();if(!a||a->size()!=4)return juce::Result::fail("Invalid bus routing.");for(int i=0;i<4;++i){const auto& o=(*a)[i];if(!o.isObject())return juce::Result::fail("Invalid bus settings.");auto& p=buses[(size_t)i];p.name=o.getProperty("name",p.name.c_str()).toString().substring(0,40).toStdString();
    #define N(k) p.k=number(o,#k,p.k)
    N(gain);N(pan);N(amount);N(damping);N(feedback);N(division);N(sidechainAmount);N(sidechainAttackMs);N(sidechainReleaseMs);N(sidechainCurve);
    p.sidechainSource=(int)std::clamp(number(o,"sidechainSource",4.),0.,17.);
    #undef N
    p.mute=(bool)o["mute"];p.solo=(bool)o["solo"];p.normalise();}return juce::Result::ok();}
bool extendedProject(const Project& p){if(p.buses!=defaultBuses())return true;return std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return !t.sampler.isDefault()||t.group>=0||t.sends[0]!=0||t.sends[1]!=0;});}
juce::File sampleFile(const Track& t,const juce::File& project){auto text=juce::String::fromUTF8(t.instrumentPath.c_str());if(text.isEmpty())return {};if(absolute(text))return juce::File::isAbsolutePath(text)?juce::File(text):juce::File();
    const auto folder=project==juce::File()?juce::File::getCurrentWorkingDirectory():project.getParentDirectory();const auto file=folder.getChildFile(text.replaceCharacter('\\','/'));return file.isAChildOf(folder)?file:juce::File();}
juce::Result resolveProjectMedia(Project& p,const juce::File& project){for(auto& t:p.tracks)if(t.instrument=="sample"&&!t.instrumentPath.empty()&&!absolute(juce::String(t.instrumentPath.c_str()))){auto file=sampleFile(t,project);if(file==juce::File())return juce::Result::fail("Relative sample path leaves the project folder.");t.instrumentPath=file.getFullPathName().toStdString();}return juce::Result::ok();}
void relativiseProjectMedia(juce::var& o,const juce::File& project){if(auto* tracks=o["tracks"].getArray())for(auto& t:*tracks)if(t["instrument"].toString()=="sample"){auto path=t["instrumentPath"].toString();if(juce::File::isAbsolutePath(path)){juce::File f(path);if(f.isAChildOf(project.getParentDirectory()))t.getDynamicObject()->setProperty("instrumentPath",f.getRelativePathFrom(project.getParentDirectory()).replaceCharacter('\\','/'));}}}
juce::Result collectProjectMedia(const Project& source,const juce::File& target,Project& collected,std::function<bool()> cancelled,std::function<void(double)> progress){
    Project packed=source;std::vector<juce::File> created;auto result=juce::Result::ok();int count=0,done=0;for(const auto& t:packed.tracks)if(t.instrument=="sample")++count;
    const auto folder=target.getSiblingFile(target.getFileNameWithoutExtension()+".media");
    std::map<juce::String,juce::File> content;
    for(auto& t:packed.tracks){if(t.instrument!="sample")continue;if(cancelled&&cancelled()){result=juce::Result::fail("Collection cancelled.");break;}
        juce::File dest;juce::String hash;result=copyMedia(sampleFile(t),folder,dest,hash,created,cancelled);if(result.failed())break;
        if(auto it=content.find(hash);it!=content.end()){if(dest!=it->second&&std::find(created.begin(),created.end(),dest)!=created.end()){dest.deleteFile();created.erase(std::remove(created.begin(),created.end(),dest),created.end());}dest=it->second;}else content[hash]=dest;
        t.instrumentPath=dest.getFullPathName().toStdString();t.sampleHash=hash.toStdString();if(progress)progress(++done/(double)std::max(1,count));
    }
    if(result.wasOk()&&cancelled&&cancelled())result=juce::Result::fail("Collection cancelled.");
    if(result.wasOk())result=saveProject(packed,target);
    if(result.failed()){for(auto& file:created)file.deleteFile();return result;}collected=std::move(packed);return result;
}
juce::Result relinkProjectMedia(Project& project,const juce::File& folder,RelinkResult& report,std::function<bool()> cancelled){
    if(!folder.isDirectory())return juce::Result::fail("Choose a sample folder.");
    juce::Array<juce::File> candidates;for(const auto& entry:juce::RangedDirectoryIterator(folder,true,"*",juce::File::findFiles)){if(cancelled&&cancelled())return juce::Result::fail("Relink cancelled.");if(entry.getFile().hasFileExtension("wav;aif;aiff;flac;ogg;mp3"))candidates.add(entry.getFile());if(candidates.size()>50000)return juce::Result::fail("Choose a smaller sample folder.");}
    auto next=project;std::map<juce::String,juce::String> hashes;
    for(auto& t:next.tracks){if(t.instrument!="sample"||sampleFile(t).existsAsFile())continue;auto name=juce::String(t.instrumentPath.c_str()).replaceCharacter('\\','/').fromLastOccurrenceOf("/",false,false);juce::Array<juce::File> found;
        for(const auto& f:candidates){if(cancelled&&cancelled())return juce::Result::fail("Relink cancelled.");if(!t.sampleHash.empty()){auto key=f.getFullPathName();auto it=hashes.find(key);if(it==hashes.end())it=hashes.emplace(key,hashFile(f,cancelled)).first;if(it->second==juce::String(t.sampleHash.c_str()))found.add(f);}else if(f.getFileName().equalsIgnoreCase(name))found.add(f);}
        if(found.size()==1){t.instrumentPath=found[0].getFullPathName().toStdString();++report.found;if(t.sampleHash.empty())report.unverified.add(juce::String(t.name.c_str())+": "+found[0].getFileName());}else if(found.size()>1&&!t.sampleHash.empty()){t.instrumentPath=found[0].getFullPathName().toStdString();++report.found;}else report.missing.add(juce::String(t.name.c_str())+": "+name);
    }if(cancelled&&cancelled())return juce::Result::fail("Relink cancelled.");project=std::move(next);return juce::Result::ok();
}
juce::Result saveSamplerPreset(const SamplerPreset& preset,const juce::File& file){std::vector<juce::File> created;juce::File copied;juce::String hash;auto r=copyMedia(preset.file,file.getSiblingFile(file.getFileNameWithoutExtension()+".media"),copied,hash,created);if(r.failed())return r;auto o=object();set(o,"format","fase-groove-sampler");set(o,"version",1);set(o,"root",preset.root);set(o,"sample",copied.getRelativePathFrom(file.getParentDirectory()).replaceCharacter('\\','/'));set(o,"hash",hash);set(o,"settings",samplerJson(preset.settings));juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithText(juce::JSON::toString(o,false))||!temp.overwriteTargetFileWithTemporary()){for(const auto& f:created)f.deleteFile();return juce::Result::fail("Cannot save sampler preset.");}return juce::Result::ok();}
juce::Result loadSamplerPreset(const juce::File& file,SamplerPreset& preset,bool verifyHash){if(!file.existsAsFile()||file.getSize()>1024*1024)return juce::Result::fail("Invalid sampler preset.");juce::var o;auto r=readBoundedJson(file,o,1024*1024);if(r.failed())return r;if(o["format"].toString()!="fase-groove-sampler"||(int)o["version"]!=1)return juce::Result::fail("Unsupported sampler preset.");SamplerPreset p;p.root=std::clamp((int)number(o,"root",60),0,127);if(auto read=readSampler(o["settings"],p.settings);read.failed())return read;auto path=o["sample"].toString();if(path.isEmpty()||juce::File::isAbsolutePath(path))return juce::Result::fail("Invalid preset sample path.");p.file=file.getSiblingFile(path.replaceCharacter('\\','/'));if(!p.file.isAChildOf(file.getParentDirectory()))return juce::Result::fail("Preset sample path leaves the preset folder.");p.hash=o["hash"].toString();if(!p.file.existsAsFile()||p.hash.isEmpty()||(verifyHash&&hashFile(p.file)!=p.hash))return juce::Result::fail("Preset sample is missing or changed. Restore its media folder.");preset=std::move(p);return juce::Result::ok();}
namespace {
constexpr int recoverySlots=3;
juce::File sessionMarker(const juce::File& dir){return dir.getChildFile("session.open");}
juce::File recoveryMeta(const juce::File& dir){return dir.getChildFile("latest.txt");}
int recoverySlot(int slot){return std::clamp(slot,0,recoverySlots-1);}
juce::File recoverySnap(const juce::File& dir,int slot){return dir.getChildFile("snap-"+juce::String(recoverySlot(slot))+".fga");}
juce::File recoverySide(const juce::File& dir,int slot){return dir.getChildFile("snap-"+juce::String(recoverySlot(slot))+".txt");}
juce::Result writeTextAtomic(const juce::File& target,const juce::String& text){juce::TemporaryFile temp(target);if(!temp.getFile().replaceWithText(text)||!temp.overwriteTargetFileWithTemporary())return juce::Result::fail("Cannot record recovery.");return juce::Result::ok();}
}
juce::File recoveryDirectory(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger/recovery");}
void openRecoverySession(const juce::File& dir){dir.createDirectory();sessionMarker(dir).replaceWithText("open");}
void closeRecoverySession(const juce::File& dir){sessionMarker(dir).deleteFile();clearRecoverySnapshots(dir);}
void clearRecoverySnapshots(const juce::File& dir){for(int i=0;i<recoverySlots;++i){recoverySnap(dir,i).deleteFile();recoverySide(dir,i).deleteFile();}recoveryMeta(dir).deleteFile();}
juce::Result storeRecoverySnapshot(const Project& project,const juce::String& originalPath,const juce::File& dir){
    if(auto made=dir.createDirectory();made.failed())return made;
    int slot=0;if(recoveryMeta(dir).existsAsFile()){auto lines=juce::StringArray::fromLines(recoveryMeta(dir).loadFileAsString());if(lines.size()>=3)slot=(lines[2].getIntValue()+1)%recoverySlots;}
    const auto file=recoverySnap(dir,slot);const auto saved=saveProject(project,file);if(saved.failed())return saved;
    Project probe;if(loadProject(file,probe).failed()){file.deleteFile();return juce::Result::fail("Recovery snapshot is unreadable.");}
    const auto path=originalPath.contains("\n")?juce::String():originalPath;const auto millis=juce::String(juce::Time::currentTimeMillis());
    if(auto side=writeTextAtomic(recoverySide(dir,slot),path+"\n"+millis);side.failed()){file.deleteFile();return side;}
    if(auto meta=writeTextAtomic(recoveryMeta(dir),path+"\n"+millis+"\n"+juce::String(slot));meta.failed())return meta;
    return juce::Result::ok();
}
bool pendingRecovery(const juce::File& dir,RecoveryOffer& offer){
    if(!sessionMarker(dir).existsAsFile()||!recoveryMeta(dir).existsAsFile())return false;
    const auto lines=juce::StringArray::fromLines(recoveryMeta(dir).loadFileAsString());if(lines.size()<3)return false;
    const int slot=lines[2].getIntValue();if(slot<0||slot>=recoverySlots)return false;
    for(int attempt=0;attempt<recoverySlots;++attempt){const int s=(slot-attempt+recoverySlots)%recoverySlots;const auto file=recoverySnap(dir,s);Project probe;if(loadProject(file,probe).failed())continue;
        juce::String path,when;const auto side=recoverySide(dir,s);if(side.existsAsFile()){const auto remembered=juce::StringArray::fromLines(side.loadFileAsString());if(remembered.size()>=2){path=remembered[0];when=remembered[1];}}
        else if(attempt==0){path=lines[0];when=lines[1];}
        offer.snapshot=file;offer.originalPath=path;offer.millis=when.getLargeIntValue();return true;}
    return false;
}
void listMissingResources(const Project& project,const juce::File& projectFile,std::vector<MissingResource>& out){
    auto add=[&](const juce::String& track,const char* kind,const juce::String& path){if(path.isEmpty())return;out.push_back({track,kind,path});};
    for(const auto& track:project.tracks){
        if(track.instrument=="sample"){const auto file=sampleFile(track,projectFile);if(!track.instrumentPath.empty()&&!file.existsAsFile())add(juce::String::fromUTF8(track.name.c_str()),"sample",file==juce::File()?juce::String::fromUTF8(track.instrumentPath.c_str()):file.getFullPathName());}
        else if(track.instrument=="plugin"&&!track.instrumentPath.empty()&&!juce::File(juce::String::fromUTF8(track.instrumentPath.c_str())).exists())add(juce::String::fromUTF8(track.name.c_str()),"plugin",juce::String::fromUTF8(track.instrumentPath.c_str()));
        for(const auto& fx:track.effects)if(!fx.path.empty()&&!juce::File(juce::String::fromUTF8(fx.path.c_str())).exists())add(juce::String::fromUTF8(track.name.c_str()),"plugin",juce::String::fromUTF8(fx.path.c_str()));
    }
    for(const auto& fx:project.masterEffects.effects)if(!fx.path.empty()&&!juce::File(juce::String::fromUTF8(fx.path.c_str())).exists())add("Master","plugin",juce::String::fromUTF8(fx.path.c_str()));
}
juce::Result packagePortableProject(const Project& source,const juce::File& folder,PortableReport& report){
    if(folder.exists()&&!folder.isDirectory())return juce::Result::fail("Choose a folder.");
    if(folder.getNumberOfChildFiles(juce::File::findFilesAndDirectories)>0)return juce::Result::fail("Choose an empty folder. Existing files were not changed.");
    for(const auto& track:source.tracks)if(track.instrument=="sample"&&juce::String::fromUTF8(track.instrumentPath.c_str()).endsWithIgnoreCase(".sfz"))return juce::Result::fail("SFZ maps are not bundled. Only single audio files used by sample tracks can be copied.");
    if(auto made=folder.createDirectory();made.failed())return made;
    auto name=juce::File::createLegalFileName(juce::String::fromUTF8(source.name.c_str()));if(name.isEmpty())name="Song";
    const auto target=folder.getChildFile(name+".fga");const auto media=folder.getChildFile(name+".media");
    if(target.existsAsFile()||media.exists())return juce::Result::fail("The destination already contains this project.");
    Project collected;auto result=collectProjectMedia(source,target,collected);if(result.failed()){target.deleteFile();media.deleteRecursively();return result;}
    Project reloaded;auto removePartial=[&]{target.deleteFile();media.deleteRecursively();};
    if(loadProject(target,reloaded).failed()){removePartial();return juce::Result::fail("The portable project could not be read back.");}
    for(const auto& track:reloaded.tracks)if(track.instrument=="sample"&&!track.instrumentPath.empty()&&!sampleFile(track,target).existsAsFile()){removePartial();return juce::Result::fail("A collected sample is not inside the portable folder.");}
    for(const auto& track:source.tracks){if(track.instrument=="plugin"&&!track.instrumentPath.empty())report.externalPlugins.add(juce::String::fromUTF8(track.name.c_str())+": "+juce::String::fromUTF8(track.instrumentPath.c_str()));for(const auto& fx:track.effects)if(!fx.path.empty())report.externalPlugins.add(juce::String::fromUTF8(fx.pluginName.c_str()));}
    for(const auto& fx:source.masterEffects.effects)if(!fx.path.empty())report.externalPlugins.add("Master: "+juce::String::fromUTF8(fx.pluginName.c_str()));
    report.project=target;return juce::Result::ok();
}
}
