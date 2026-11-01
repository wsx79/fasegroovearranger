// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
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
    const auto metadataKey=file.getFullPathName()+"\x1f"+juce::String(file.getSize())+"\x1f"+juce::String(file.getLastModificationTime().toMilliseconds());
    const auto key=juce::SHA256(metadataKey.toRawUTF8(),(size_t)metadataKey.getNumBytesAsUTF8()).toHexString();
    static juce::CriticalSection mutex;static std::map<juce::String,juce::String> cache;
    const auto store=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Fase Groove Arranger/Plugins/BinaryHashes.xml");
    const auto* manager=juce::MessageManager::getInstanceWithoutCreating();const bool ui=manager&&manager->isThisTheMessageThread();
    {const juce::ScopedTryLock lock(mutex);if(!lock.isLocked())return {};
        static bool loaded=false;if(!loaded){loaded=true;if(auto xml=juce::XmlDocument::parse(store))if(xml->hasTagName("BINARY_HASHES"))for(auto* item:xml->getChildIterator())if(cache.size()<2048){const auto hash=item->getStringAttribute("hash");if(hash.length()==64&&hash.containsOnly("0123456789abcdef"))cache.emplace(item->getStringAttribute("key"),hash);}}
        if(auto it=cache.find(key);it!=cache.end())return it->second;
    }
    // Never read a complete plugin binary on the message thread. The catalogue
    // worker warms missing entries; until then leave distinct paths separate.
    if(ui)return {};
    auto stream=file.createInputStream();if(!stream)return {};const auto hash=juce::SHA256(*stream).toHexString();
    const auto after=file.getFullPathName()+"\x1f"+juce::String(file.getSize())+"\x1f"+juce::String(file.getLastModificationTime().toMilliseconds());if(after!=metadataKey)return {};
    juce::XmlElement xml("BINARY_HASHES");{const juce::ScopedLock lock(mutex);if(cache.size()>=2048)cache.clear();cache[key]=hash;for(const auto& item:cache){auto* entry=xml.createNewChildElement("HASH");entry->setAttribute("key",item.first);entry->setAttribute("hash",item.second);}}
    store.getParentDirectory().createDirectory();juce::TemporaryFile temp(store);if(xml.writeTo(temp.getFile()))temp.overwriteTargetFileWithTemporary();return hash;
}
inline bool sameHostedPluginBinary(const juce::PluginDescription& a,const juce::PluginDescription& b){
    if(!sameHostedPluginType(a,b))return false;
    if(a.fileOrIdentifier==b.fileOrIdentifier)return true;
    const auto hash=hostedBinaryHash(a.fileOrIdentifier);return hash.isNotEmpty()&&hash==hostedBinaryHash(b.fileOrIdentifier);
}
inline std::vector<int> canonicalHostedPlugins(const juce::Array<juce::PluginDescription>& plugins){
    std::vector<int> selected;
    using Type=std::tuple<int,juce::String,juce::String,juce::String,juce::String,bool>;
    std::map<Type,std::vector<size_t>> groups;
    for(int i=0;i<plugins.size();++i){
        const auto& d=plugins.getReference(i);
        if(d.uniqueId==0){selected.push_back(i);continue;}
        auto& slots=groups[Type{d.uniqueId,d.pluginFormatName,d.name,d.manufacturerName,d.version,d.isInstrument}];
        auto slot=std::find_if(slots.begin(),slots.end(),[&](size_t index){return sameHostedPluginBinary(plugins.getReference(selected[index]),d);});
        auto found=slot==slots.end()?selected.end():selected.begin()+*slot;
        if(found==selected.end()){slots.push_back(selected.size());selected.push_back(i);}
        else{const auto& candidate=plugins.getReference(i).fileOrIdentifier;const auto& current=plugins.getReference(*found).fileOrIdentifier;
            if(candidate.length()<current.length()||(candidate.length()==current.length()&&candidate.compareIgnoreCase(current)<0))*found=i;}
    }
    return selected;
}
}
