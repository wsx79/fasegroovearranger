// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Adapted from Fase Groove Drums 7f1def6: preference, version comparison,
// platform HTTP transport and weak-reference delivery on the message thread.
#include "Updater.h"
#include "Localisation.h"
#include <array>
namespace fga::Updater {
bool autoCheckEnabled(){return Localisation::setting("autoUpdate","1")!="0";}
void setAutoCheckEnabled(bool enabled){Localisation::setSetting("autoUpdate",enabled?"1":"0");}
bool isNewer(const juce::String& a,const juce::String& b){
    auto parts=[](juce::String v){v=v.trim().trimCharactersAtStart("vV");juce::StringArray tokens;tokens.addTokens(v.upToFirstOccurrenceOf("-",false,false),".",{});std::array<int,3> n{};for(int i=0;i<3&&i<tokens.size();++i)n[(size_t)i]=tokens[i].getIntValue();return n;};
    return parts(a)>parts(b);
}
Checker::Checker():juce::Thread("Fase Groove Arranger update check"){}
Checker::~Checker(){stopThread(7000);}
void Checker::start(bool manual){if(isThreadRunning())return;manualCheck=manual;startThread(juce::Thread::Priority::low);}
void Checker::run(){
    constexpr auto api="https://api.github.com/repos/wsx79/fasegroovearranger/releases/latest";
    juce::String json;
   #if JUCE_LINUX || JUCE_BSD
    // Same curl transport as the family: no libcurl build dependency.
    juce::ChildProcess process;
    juce::StringArray args{"curl","-sSL","--max-time","5","-A","FaseGrooveArranger-Updater","-H","Accept: application/vnd.github+json",api};
    if(process.start(args,juce::ChildProcess::wantStdOut)){
        juce::MemoryOutputStream data;
        while(process.isRunning()){
            if(threadShouldExit()){process.kill();return;}
            char buffer[4096];int n=process.readProcessOutput(buffer,sizeof(buffer));
            if(n>0)data.write(buffer,(size_t)n);else wait(20);
            if(data.getDataSize()>1024*1024){process.kill();return;}
        }
        char buffer[4096];for(int n;(n=process.readProcessOutput(buffer,sizeof(buffer)))>0;)data.write(buffer,(size_t)n);
        if(process.getExitCode()==0)json=data.toUTF8();
    }
   #else
    auto stream=juce::URL(api).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(5000).withExtraHeaders("User-Agent: FaseGrooveArranger-Updater\r\nAccept: application/vnd.github+json"));
    if(stream){juce::MemoryOutputStream data;char buffer[4096];while(!threadShouldExit()&&!stream->isExhausted()&&data.getDataSize()<1024*1024){int n=stream->read(buffer,sizeof(buffer));if(n<=0)break;data.write(buffer,(size_t)n);}json=data.toUTF8();}
   #endif
    if(threadShouldExit())return;
    Result result;const auto release=juce::JSON::parse(json);
    const auto tag=release.getProperty("tag_name",{}).toString();
    if(tag.isNotEmpty()&&!release.getProperty("draft",false)&&!release.getProperty("prerelease",false)){
        result.version=tag.trimCharactersAtStart("vV");result.notes=release.getProperty("body",{}).toString();
        result.status=isNewer(tag,JUCE_APPLICATION_VERSION_STRING)?Result::Status::ready:Result::Status::upToDate;
        if(!manualCheck&&Localisation::setting("skippedVersion")==result.version)result.status=Result::Status::upToDate;
    }else if(release.getProperty("message",{}).toString()=="Not Found")result.status=Result::Status::noRelease;
    juce::WeakReference<Checker> self(this);const bool manual=manualCheck;
    juce::MessageManager::callAsync([self,result,manual]{if(self!=nullptr&&self->onResult)self->onResult(result,manual);});
}
}
