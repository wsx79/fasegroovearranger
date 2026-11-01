// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>
#include <vector>
#include <algorithm>
#include <map>
namespace fga {
inline bool sameHostedPluginType(const juce::PluginDescription& a,const juce::PluginDescription& b){
    return a.uniqueId!=0&&a.uniqueId==b.uniqueId&&a.pluginFormatName==b.pluginFormatName
        &&a.name==b.name&&a.manufacturerName==b.manufacturerName&&a.version==b.version
        &&a.isInstrument==b.isInstrument;
}
inline juce::String hostedBinaryHash(const juce::String& path){
    if(!juce::File::isAbsolutePath(path))return {};
    const juce::File file(path);
    // Be conservative with bundles: identical executable code need not mean
    // identical bundled resources. Only standalone binary copies are coalesced.
    if(!file.existsAsFile())return {};
    const auto key=file.getFullPathName()+"\x1f"+juce::String(file.getSize())+"\x1f"+juce::String(file.getLastModificationTime().toMilliseconds());
    static juce::CriticalSection mutex;static std::map<juce::String,juce::String> cache;
    const juce::ScopedLock lock(mutex);if(auto it=cache.find(key);it!=cache.end())return it->second;
    auto stream=file.createInputStream();if(!stream)return {};auto hash=juce::SHA256(*stream).toHexString();if(cache.size()>2048)cache.clear();cache.emplace(key,hash);return hash;
}
inline bool sameHostedPluginBinary(const juce::PluginDescription& a,const juce::PluginDescription& b){
    if(!sameHostedPluginType(a,b))return false;
    if(a.fileOrIdentifier==b.fileOrIdentifier)return true;
    const auto hash=hostedBinaryHash(a.fileOrIdentifier);return hash.isNotEmpty()&&hash==hostedBinaryHash(b.fileOrIdentifier);
}
inline std::vector<int> canonicalHostedPlugins(const juce::Array<juce::PluginDescription>& plugins){
    std::vector<int> selected;
    for(int i=0;i<plugins.size();++i){
        auto found=std::find_if(selected.begin(),selected.end(),[&](int previous){return sameHostedPluginBinary(plugins.getReference(previous),plugins.getReference(i));});
        if(found==selected.end())selected.push_back(i);
        else{const auto& candidate=plugins.getReference(i).fileOrIdentifier;const auto& current=plugins.getReference(*found).fileOrIdentifier;
            if(candidate.length()<current.length()||(candidate.length()==current.length()&&candidate.compareIgnoreCase(current)<0))*found=i;}
    }
    return selected;
}
}
