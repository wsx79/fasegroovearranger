// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/StudioIO.h"
#include "../Source/ProjectIO.h"
#include <stdexcept>
namespace {void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}}
void runStudioIOTests(){
    auto p=fga::newProject(11);auto& t=p.tracks[0];t.sampler.mode=2;t.sampler.reverse=true;t.sampler.sync=true;t.sampler.sourceBpm=93;t.sampler.sliceCount=3;t.sampler.slices[0]=0;t.sampler.slices[1]=.2;t.sampler.slices[2]=.7;t.group=1;t.sends={.3,.4};p.buses[3].feedback=.68;p.normalise();
    p.tracks[4].instrument="sample";p.tracks[4].sampler.mode=2;p.tracks[4].clips[0].notes={{0,.1,36,100,0},{.25,.1,37,100,0}};p.normalise();require(p.tracks[4].clips[0].notes[1].pitch==37,"drum sampler preserves separate slice trigger pitches");p.tracks[4].instrument="internal";p.normalise();
    fga::Project copy;auto json=fga::projectJson(p);require((int)json["version"]==3,"studio schema version");require(fga::projectFromJson(json,copy).wasOk(),"studio project parse");require(copy.tracks[0].sampler==t.sampler&&copy.tracks[0].sends==t.sends&&copy.buses==p.buses,"sampler and buses roundtrip");
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-studio-io","",false);dir.createDirectory();
    struct Cleanup{juce::File dir;~Cleanup(){dir.deleteRecursively();}}cleanup{dir};
    auto original=dir.getChildFile("original.wav");original.replaceWithText("sample-content-for-copy-test");auto originalBytes=original.loadFileAsString();t.instrument="sample";t.instrumentPath=original.getFullPathName().toStdString();p.tracks[1].instrument="sample";p.tracks[1].instrumentPath=t.instrumentPath;
    auto target=dir.getChildFile("collected/song.fga");target.getParentDirectory().createDirectory();fga::Project collected;
    require(fga::collectProjectMedia(p,target,collected).wasOk(),"collect project");require(collected.tracks[0].instrumentPath==collected.tracks[1].instrumentPath,"collect deduplicates identical samples");require(original.loadFileAsString()==originalBytes,"collect preserves originals");require(fga::loadProject(target,copy).wasOk(),"collected project loads");require(fga::sampleFile(copy.tracks[0]).existsAsFile(),"relative media resolved");
    auto moved=dir.getChildFile("moved");require(target.getParentDirectory().moveFileTo(moved),"move portable folder");require(fga::loadProject(moved.getChildFile("song.fga"),copy).wasOk()&&fga::sampleFile(copy.tracks[0]).existsAsFile(),"collected project survives folder move");
    fga::SamplerPreset preset{t.sampler,57,original,{}};auto presetFile=dir.getChildFile("presets/test.fgs");presetFile.getParentDirectory().createDirectory();require(fga::saveSamplerPreset(preset,presetFile).wasOk(),"save portable preset");fga::SamplerPreset back;require(fga::loadSamplerPreset(presetFile,back).wasOk()&&back.root==57&&back.settings==preset.settings&&back.file.existsAsFile(),"preset sample and settings roundtrip");
    auto missing=p;missing.tracks[0].instrumentPath=dir.getChildFile("absent.wav").getFullPathName().toStdString();auto failed=dir.getChildFile("failed.fga");require(fga::collectProjectMedia(missing,failed,collected).failed()&&!failed.existsAsFile(),"missing sample never publishes partial project");
    failed.replaceWithText("KEEP");require(fga::collectProjectMedia(missing,failed,collected).failed()&&failed.loadFileAsString()=="KEEP","failed collection preserves an existing destination project");
    auto cancel=dir.getChildFile("cancel.fga");require(fga::collectProjectMedia(p,cancel,collected,[]{return true;}).failed()&&!cancel.existsAsFile(),"cancel never publishes project");
    p.tracks[0].instrumentPath=dir.getChildFile("missing/original.wav").getFullPathName().toStdString();p.tracks[1].instrument="internal";fga::RelinkResult found;require(fga::relinkProjectMedia(p,dir,found).wasOk()&&found.found==1,"unique basename relink");
    auto duplicate=dir.getChildFile("duplicates/original.wav");duplicate.getParentDirectory().createDirectory();duplicate.replaceWithText("different sample with same basename");p.tracks[0].instrumentPath=dir.getChildFile("missing/original.wav").getFullPathName().toStdString();found={};require(fga::relinkProjectMedia(p,dir,found).wasOk()&&found.found==0&&found.missing.size()==1,"ambiguous basename never relinks to the wrong sample");    // Cancel while a temporary media copy already contains bytes: keep the old
    // project and remove all partial files, including the in-flight temp file.
    auto large=dir.getChildFile("large.wav");juce::MemoryBlock payload(4*1024*1024,true);require(large.replaceWithData(payload.getData(),payload.getSize()),"large media fixture");
    auto largeProject=p;for(auto& tr:largeProject.tracks)tr.instrument="internal";largeProject.tracks[0].instrument="sample";largeProject.tracks[0].instrumentPath=large.getFullPathName().toStdString();
    auto cancelledTarget=dir.getChildFile("cancel-mid-copy.fga");cancelledTarget.replaceWithText("KEEP");auto media=dir.getChildFile("cancel-mid-copy.media");bool sawPartial=false;
    auto midCopy=[&]{for(const auto& entry:juce::RangedDirectoryIterator(media,false,"*",juce::File::findFiles))if(entry.getFile().getSize()>=65536){sawPartial=true;return true;}return false;};
    const auto unchanged=juce::JSON::toString(fga::projectJson(collected),false,17);
    require(fga::collectProjectMedia(largeProject,cancelledTarget,collected,midCopy).failed()&&sawPartial,"cancel observed within a copied file");
    require(cancelledTarget.loadFileAsString()=="KEEP"&&media.getNumberOfChildFiles(juce::File::findFiles)==0,"cancel removes partial media and preserves destination");
    require(juce::JSON::toString(fga::projectJson(collected),false,17)==unchanged,"cancel does not publish in-memory partial collection");
    require(found.unverified.isEmpty(),"ambiguous legacy matches never claim verification");
    auto legacy=largeProject;legacy.tracks[0].instrumentPath=dir.getChildFile("missing/large.wav").getFullPathName().toStdString();fga::RelinkResult legacyReport;
    require(fga::relinkProjectMedia(legacy,dir,legacyReport).wasOk()&&legacyReport.found==1&&legacyReport.unverified.size()==1,"legacy filename-only match is explicitly unverified");
    auto escaping=largeProject;escaping.tracks[0].instrumentPath="../original.wav";auto unsafe=dir.getChildFile("unsafe/test.fga");unsafe.getParentDirectory().createDirectory();unsafe.replaceWithText(juce::JSON::toString(fga::projectJson(escaping)));
    const auto oldCopy=juce::JSON::toString(fga::projectJson(copy),false,17);
    require(fga::loadProject(unsafe,copy).failed()&&juce::JSON::toString(fga::projectJson(copy),false,17)==oldCopy,"relative project escape rejected without modifying destination");
    escaping.tracks[0].instrumentPath="..\\original.wav";unsafe.replaceWithText(juce::JSON::toString(fga::projectJson(escaping)));require(fga::loadProject(unsafe,copy).failed(),"Windows-style relative escape rejected");
    auto badPreset=juce::JSON::parse(presetFile.loadFileAsString());badPreset.getDynamicObject()->setProperty("sample","../original.wav");auto unsafePreset=presetFile.getSiblingFile("unsafe.fgs");unsafePreset.replaceWithText(juce::JSON::toString(badPreset));require(fga::loadSamplerPreset(unsafePreset,back).failed(),"preset cannot escape its media directory");
    auto jsonFile=dir.getChildFile("bounded.json");juce::var parsed;
    jsonFile.replaceWithText(juce::String::repeatedString("[",49)+"0"+juce::String::repeatedString("]",49));require(fga::readBoundedJson(jsonFile,parsed).failed(),"deep JSON rejected before recursive parsing");
    jsonFile.replaceWithText("{\"text\":\""+juce::String::repeatedString("[",100)+"\\\"braces}\\\\\"}");require(fga::readBoundedJson(jsonFile,parsed).wasOk(),"quoted and escaped delimiters do not consume nesting budget");
    require(fga::readBoundedJson(jsonFile,parsed,16).failed(),"JSON byte limit enforced before parsing");
    jsonFile.replaceWithText("{\"value\":17}",true,true);require(fga::readBoundedJson(jsonFile,parsed).wasOk()&&(int)parsed["value"]==17,"bounded parser retains UTF-16 BOM compatibility");

}
