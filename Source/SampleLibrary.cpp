// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleLibrary.h"
#include <algorithm>
namespace fga::samples {
namespace {
juce::String extensionOf(const juce::File& f){return f.getFileExtension().toLowerCase().trimCharactersAtStart(".");}
}
bool isSupportedAudio(const juce::File& f){return juce::StringArray{"wav","wave","aif","aiff","flac","ogg"}.contains(extensionOf(f));}
bool isAudioFile(const juce::File& f){return isSupportedAudio(f)||juce::StringArray{"mp3","m4a","aac","wma","opus","caf"}.contains(extensionOf(f));}

juce::String Preferences::toJson() const{
    auto* root=new juce::DynamicObject();juce::Array<juce::var> list;
    for(const auto& f:folders){auto* o=new juce::DynamicObject();o->setProperty("path",f.path);o->setProperty("favourite",f.favourite);list.add(juce::var(o));}
    root->setProperty("folders",list);root->setProperty("last",lastFolder);root->setProperty("sort",sort);root->setProperty("filter",filter);
    root->setProperty("ascending",ascending);root->setProperty("autoPreview",autoPreview);
    return juce::JSON::toString(juce::var(root),true);
}
Preferences Preferences::fromJson(const juce::String& text){
    Preferences p;const auto v=juce::JSON::parse(text);if(!v.isObject())return p;
    if(auto* list=v["folders"].getArray())for(const auto& item:*list){const auto path=item["path"].toString();
        if(path.isNotEmpty()&&juce::File::isAbsolutePath(path)&&std::none_of(p.folders.begin(),p.folders.end(),[&](const Folder& f){return f.path==path;}))
            p.folders.push_back({path,(bool)item["favourite"]});}
    p.lastFolder=v["last"].toString();
    p.sort=std::clamp((int)v.getProperty("sort",0),0,(int)numSorts-1);p.filter=std::clamp((int)v.getProperty("filter",0),0,(int)numFilters-1);
    p.ascending=(bool)v.getProperty("ascending",true);p.autoPreview=(bool)v.getProperty("autoPreview",false);
    return p;
}
bool Preferences::addFolder(const juce::File& f){
    const auto path=f.getFullPathName();if(path.isEmpty())return false;
    for(const auto& x:folders)if(x.path==path)return false;
    folders.push_back({path,false});return true;
}
bool Preferences::removeFolder(const juce::String& path){
    const auto before=folders.size();folders.erase(std::remove_if(folders.begin(),folders.end(),[&](const Folder& f){return f.path==path;}),folders.end());return folders.size()!=before;
}
bool Preferences::replaceFolder(const juce::String& path,const juce::File& with){
    const auto next=with.getFullPathName();if(next.isEmpty())return false;
    for(const auto& x:folders)if(x.path==next&&next!=path)return false;
    for(auto& x:folders)if(x.path==path){x.path=next;const juce::File last(lastFolder),from(path);if(last==from)lastFolder=next;else if(last.isAChildOf(from))lastFolder=with.getChildFile(last.getRelativePathFrom(from)).getFullPathName();return true;}
    return false;
}
void Preferences::toggleFavourite(const juce::String& path){for(auto& x:folders)if(x.path==path)x.favourite=!x.favourite;}
bool Preferences::isFavourite(const juce::String& path) const{for(const auto& x:folders)if(x.path==path)return x.favourite;return false;}
std::vector<Folder> Preferences::ordered() const{auto result=folders;std::stable_partition(result.begin(),result.end(),[](const Folder& f){return f.favourite;});return result;}
juce::File Preferences::rootFor(const juce::File& f) const{
    juce::File best;for(const auto& x:folders){const juce::File root(x.path);if((f==root||f.isAChildOf(root))&&root.getFullPathName().length()>best.getFullPathName().length())best=root;}
    return best;
}

Listing list(const juce::File& folder,const std::function<bool()>& stop){
    Listing out;out.folder=folder;
    if(!folder.isDirectory()){out.available=false;out.error=folder.exists()?"Not a folder":"Folder not available";return out;}
    for(const auto& item:juce::RangedDirectoryIterator(folder,false,"*",juce::File::findFilesAndDirectories|juce::File::ignoreHiddenFiles)){
        if(stop&&stop())break;
        Entry e;e.file=item.getFile();e.name=e.file.getFileName();
        if(e.name.startsWithChar('.'))continue; // dot names are hidden on every platform
        e.folder=item.isDirectory();e.size=e.folder?0:item.getFileSize();e.modified=item.getModificationTime();
        out.entries.push_back(std::move(e));
    }
    return out;
}

AudioInfo readInfo(const juce::File& f,juce::AudioFormatManager& formats){
    AudioInfo info;if(!isSupportedAudio(f))return info;
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
    if(!reader||reader->sampleRate<=0)return info;
    info.valid=true;info.sampleRate=reader->sampleRate;info.channels=(int)reader->numChannels;info.bits=(int)reader->bitsPerSample;
    info.seconds=(double)reader->lengthInSamples/reader->sampleRate;info.format=extensionOf(f).toUpperCase();if(info.format=="WAVE")info.format="WAV";if(info.format=="AIF")info.format="AIFF";
    return info;
}

juce::String InfoCache::key(const juce::String& path,juce::int64 size,juce::int64 modified){return path+"|"+juce::String(size)+"|"+juce::String(modified);}
bool InfoCache::get(const juce::File& f,AudioInfo& out) const{Entry e;e.file=f;e.size=f.getSize();e.modified=f.getLastModificationTime();return get(e,out);}
void InfoCache::put(const juce::File& f,const AudioInfo& info){Entry e;e.file=f;e.size=f.getSize();e.modified=f.getLastModificationTime();put(e,info);}
bool InfoCache::get(const Entry& e,AudioInfo& out) const{const auto k=key(e.file.getFullPathName(),e.size,e.modified.toMilliseconds());const juce::ScopedLock l(lock);auto it=map.find(k);if(it==map.end())return false;out=it->second;return true;}
void InfoCache::put(const Entry& e,const AudioInfo& info){const auto k=key(e.file.getFullPathName(),e.size,e.modified.toMilliseconds());const juce::ScopedLock l(lock);if(map.size()>50000)map.clear();map[k]=info;} // light: a few thousand folders' worth
int InfoCache::size() const{const juce::ScopedLock l(lock);return (int)map.size();}

bool passesFilter(const Entry& e,int filter){
    if(e.folder)return true;
    const auto ext=extensionOf(e.file);
    switch(filter){
        case wavOnly:return ext=="wav"||ext=="wave";
        case aiffOnly:return ext=="aif"||ext=="aiff";
        case flacOnly:return ext=="flac";
        case oggOnly:return ext=="ogg";
        case allFiles:return true;
        default:return isAudioFile(e.file);
    }
}
std::vector<int> arrange(const std::vector<Entry>& entries,const juce::String& search,int filter,int sort,bool ascending,const std::function<double(const Entry&)>& lengthOf){
    const auto words=juce::StringArray::fromTokens(search.toLowerCase()," ","");
    std::vector<int> folders,files;
    for(int i=0;i<(int)entries.size();++i){const auto& e=entries[(size_t)i];if(!passesFilter(e,filter))continue;
        const auto name=e.name.toLowerCase();bool match=true;for(const auto& w:words)if(w.isNotEmpty()&&!name.contains(w)){match=false;break;}
        if(!match)continue;(e.folder?folders:files).push_back(i);}
    auto byNameLess=[&](int a,int b){return entries[(size_t)a].name.compareNatural(entries[(size_t)b].name)<0;};
    std::sort(folders.begin(),folders.end(),byNameLess);
    std::vector<double> lengths;if(sort==byLength){lengths.assign(entries.size(),-1);if(lengthOf)for(int i:files)lengths[(size_t)i]=lengthOf(entries[(size_t)i]);}
    std::stable_sort(files.begin(),files.end(),[&](int a,int b){
        const auto& x=entries[(size_t)a];const auto& y=entries[(size_t)b];
        if(sort==byLength){const double lx=lengths[(size_t)a],ly=lengths[(size_t)b];if((lx<0)!=(ly<0))return ly<0; // unknown last, either direction
            if(lx!=ly)return ascending?lx<ly:lx>ly;}
        else if(sort==byDate&&x.modified!=y.modified)return ascending?x.modified<y.modified:x.modified>y.modified;
        else if(sort==bySize&&x.size!=y.size)return ascending?x.size<y.size:x.size>y.size;
        const int c=x.name.compareNatural(y.name);return sort==byName&&!ascending?c>0:c<0;});
    folders.insert(folders.end(),files.begin(),files.end());return folders;
}

juce::Array<juce::File> breadcrumb(const juce::File& folder,const juce::File& root){
    juce::Array<juce::File> chain;const bool inside=root!=juce::File()&&(folder==root||folder.isAChildOf(root));
    for(auto f=folder;f!=juce::File();){chain.insert(0,f);if(inside&&f==root)break;const auto parent=f.getParentDirectory();if(parent==f)break;f=parent;}
    return chain;
}

void History::visit(const juce::File& f){
    if(index>=0&&items[(size_t)index]==f)return;
    items.resize((size_t)(index+1));items.push_back(f);index=(int)items.size()-1;
    if(items.size()>200){items.erase(items.begin());--index;}
}
juce::File History::back(){if(canBack())--index;return current();}
juce::File History::forward(){if(canForward())++index;return current();}

juce::String formatDuration(double seconds){
    if(seconds<0)return "--";
    const int minutes=(int)(seconds/60);const double rest=seconds-minutes*60;
    return seconds<60?juce::String(minutes)+":"+juce::String(rest,2).paddedLeft('0',5):juce::String(minutes)+":"+juce::String((int)rest).paddedLeft('0',2);
}
juce::String formatSize(juce::int64 bytes){
    if(bytes<1024)return juce::String(bytes)+" B";
    if(bytes<1024*1024)return juce::String((bytes+512)/1024)+" KB";
    return juce::String((double)bytes/(1024.*1024.),1)+" MB";
}
}
