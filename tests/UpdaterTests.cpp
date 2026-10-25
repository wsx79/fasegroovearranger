// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Updater.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
}
int main(){
    namespace u=::Updater;   // the family updater (fasegroove-common), set up for the arranger in Product.cpp
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("FaseGrooveArranger-updater-test",{},false);
    struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}}cleanup{root};
    try{
        require(root.createDirectory().wasOk(),"temporary directory");
        require(u::isNewer("v0.7.2","0.7.1")&&!u::isNewer("0.7.1","0.7.1")&&u::isNewer("1.0.0","0.99.0"),"versions");
        require(u::changelogSection("## 0.7.1 — date\n\n- **New update**\n\n## 0.7.0 — date\nOld", "v0.7.1").contains("New update"),"unbracketed family changelog");
        require(u::changelogSection("## [0.7.1] - date\nNew\n## [0.7.0]\nOld", "0.7.1")=="New","bracketed changelog");
        const auto file=root.getChildFile("download.zip");require(file.replaceWithText("verified release payload"),"payload");
        const auto digest=[&]{auto in=file.createInputStream();require(in!=nullptr,"read payload");return "sha256:"+juce::SHA256(*in).toHexString();}();
        require(u::testing::verifyDownload(file,digest),"verify payload hash");
        file.appendText("corrupt");require(!u::testing::verifyDownload(file,digest),"reject modified download");
        require(!u::testing::verifyDownload(file,"sha256:bad"),"reject invalid checksum");
        juce::var release(new juce::DynamicObject),asset(new juce::DynamicObject);
       #if JUCE_WINDOWS
        const juce::String name="FaseGrooveArranger-Windows.zip";
       #elif JUCE_MAC
        const juce::String name="FaseGrooveArranger-macOS.zip";
       #else
        const juce::String name="FaseGrooveArranger-Linux.zip";
       #endif
        release.getDynamicObject()->setProperty("tag_name","v0.7.2");
        asset.getDynamicObject()->setProperty("name",name);asset.getDynamicObject()->setProperty("digest",digest);
        const auto url="https://github.com/wsx79/fasegroovearranger/releases/download/v0.7.2/"+name;
        asset.getDynamicObject()->setProperty("browser_download_url",url);
        auto setAsset=[&]{release.getDynamicObject()->setProperty("assets",juce::var(juce::Array<juce::var>{asset}));};setAsset();
        juce::String selected;require(u::testing::releaseAsset(release,u::InstallKind::portable,selected)==url&&selected==digest,"correct platform asset");
        release.getDynamicObject()->setProperty("prerelease",true);require(u::testing::releaseAsset(release,u::InstallKind::portable,selected).isEmpty(),"reject prerelease");release.getDynamicObject()->setProperty("prerelease",false);
        asset.getDynamicObject()->setProperty("browser_download_url","https://example.invalid/update.exe");setAsset();require(u::testing::releaseAsset(release,u::InstallKind::portable,selected).isEmpty(),"reject unrelated download URL");
        const auto unpack=root.getChildFile("unpacked"),zip=root.getChildFile("archive.zip");
        auto archive=[&](const juce::String& path){juce::ZipFile::Builder builder;builder.addEntry(new juce::MemoryInputStream("new",3,false),6,path,juce::Time::getCurrentTime());auto out=zip.createOutputStream();require(out!=nullptr&&builder.writeToStream(*out,nullptr),"create test archive");};
        archive("../outside.txt");require(!u::testing::unpack(zip,unpack)&&!root.getChildFile("outside.txt").exists(),"reject archive traversal");
        zip.deleteFile();archive("Standalone/part.txt");require(u::testing::unpack(zip,unpack)&&unpack.getChildFile("Standalone/part.txt").loadFileAsString()=="new","stage portable package");
        const auto staged=root.getChildFile("staged");juce::Array<juce::File> targets;
        for(const auto& namePart:{"app one","app two"}){
            auto fresh=staged.getChildFile("Standalone").getChildFile(namePart),old=root.getChildFile("installed").getChildFile(namePart);
            fresh.createDirectory();old.createDirectory();
            fresh.getChildFile("part1").replaceWithText("new one");fresh.getChildFile("part2").replaceWithText("new two");
            old.getChildFile("part1").replaceWithText("old one");old.getChildFile("part2").replaceWithText("old two");targets.add(old);
        }
       #if JUCE_WINDOWS
        constexpr int steps=4;
       #else
        constexpr int steps=6;
       #endif
        for(int fail=0;fail<steps;++fail){
            require(!u::testing::replaceAll(staged,targets,fail),"injected update failure");
            for(const auto& old:targets)require(old.getChildFile("part1").loadFileAsString()=="old one"&&old.getChildFile("part2").loadFileAsString()=="old two","rollback restores every old file");
        }
        require(u::testing::replaceAll(staged,targets,-1),"complete portable update");
        for(const auto& now:targets)require(now.getChildFile("part1").loadFileAsString()=="new one"&&now.getChildFile("part2").loadFileAsString()=="new two","all targets updated");
        const auto custom=root.getChildFile("custom folder/Fase Groove Arranger.exe");custom.getParentDirectory().createDirectory();custom.replaceWithText("binary");
        require(u::testing::installKindFor(custom)==u::InstallKind::portable,"portable copy detection");
       #if JUCE_WINDOWS
        custom.getSiblingFile("FaseGrooveArranger.install.ini").replaceWithText("[Installation]");custom.getSiblingFile("unins000.exe").replaceWithText("uninstall");
        require(u::testing::installKindFor(custom)==u::InstallKind::installer,"custom installer path detection");
       #endif
        std::cout<<"PASS family updater: version/changelog, platform asset, download checksum, archive paths, portable swap, failure rollback and install detection\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL updater: "<<e.what()<<"\n";return 1;}
}
