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
    auto duplicate=dir.getChildFile("duplicates/original.wav");duplicate.getParentDirectory().createDirectory();duplicate.replaceWithText("different sample with same basename");p.tracks[0].instrumentPath=dir.getChildFile("missing/original.wav").getFullPathName().toStdString();found={};require(fga::relinkProjectMedia(p,dir,found).wasOk()&&found.found==0&&found.missing.size()==1,"ambiguous basename never relinks to the wrong sample");
}
