// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginCatalog.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <vector>
#include <algorithm>
#include <set>
namespace fga {
namespace {
juce::String pathKey(juce::String p) {
   #if JUCE_WINDOWS
    return p.toLowerCase();
   #else
    return p;
   #endif
}
bool belongs(const juce::PluginDescription& d,const juce::String& identifier){
    if(d.fileOrIdentifier==identifier)return true;
    return d.pluginFormatName=="VST3"&&juce::File::isAbsolutePath(identifier)&&juce::File::isAbsolutePath(d.fileOrIdentifier)&&juce::File(d.fileOrIdentifier).isAChildOf(juce::File(identifier));
}
void atomicXml(const juce::File& file,const juce::XmlElement& xml){
    file.getParentDirectory().createDirectory();juce::TemporaryFile temp(file);
    if(xml.writeTo(temp.getFile()))temp.overwriteTargetFileWithTemporary();
}
}
juce::File PluginCatalog::defaultStorage(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Fase Groove Arranger/Plugins");}
juce::FileSearchPath PluginCatalog::standardFolders(){
    juce::AudioPluginFormatManager formats;formats.addDefaultFormats();juce::FileSearchPath paths;
    for(int i=0;i<formats.getNumFormats();++i)paths.addPath(formats.getFormat(i)->getDefaultLocationsToSearch());
   #if JUCE_MAC
    paths.add(juce::File("/Library/Audio/Plug-Ins/Components"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/Components"));
   #endif
    paths.removeRedundantPaths();return paths;
}
PluginCatalog::PluginCatalog(juce::File storage,bool includeStandard):Thread("Plugin catalogue"),directory(std::move(storage)),includeStandardPaths(includeStandard){read();}
PluginCatalog::~PluginCatalog(){signalThreadShouldExit();stopThread(5000);}
juce::File PluginCatalog::customFolder()const{const juce::ScopedLock lock(mutex);return custom.isEmpty()?juce::File():juce::File(custom[0]);}
PluginCatalog::State PluginCatalog::state()const{const juce::ScopedLock lock(mutex);auto s=progress;s.count=s.plugins.size();return s;}
PluginCatalog::State PluginCatalog::progressState()const{const juce::ScopedLock lock(mutex);State s;s.current=progress.current;s.checked=progress.checked;s.total=progress.total;s.failed=progress.failed;s.scanning=progress.scanning;s.count=progress.plugins.size();return s;}
bool PluginCatalog::setCustomFolder(const juce::File& file){
    const juce::ScopedLock lock(mutex);if(progress.scanning)return false;
    if(file==juce::File()){if(custom.isEmpty())return false;custom.clear();writeFolders();return true;}
    if(!file.isDirectory())return false;
    const auto path=file.getLinkedTarget().getFullPathName();
    if(custom.size()==1&&pathKey(custom[0])==pathKey(path))return false;
    custom.clear();custom.add(path);writeFolders();return true;
}
void PluginCatalog::writeFolders(){juce::XmlElement xml("PLUGIN_FOLDERS");for(const auto& p:custom)xml.createNewChildElement("FOLDER")->setAttribute("path",p);atomicXml(directory.getChildFile("Folders.xml"),xml);}
void PluginCatalog::read(){
    if(auto xml=juce::XmlDocument::parse(directory.getChildFile("Folders.xml")))for(auto* e:xml->getChildIterator())if(e->hasTagName("FOLDER")){auto p=e->getStringAttribute("path");if(juce::File::isAbsolutePath(p))custom.addIfNotAlreadyThere(p);}
    if(custom.size()>1){const auto first=custom[0];custom.clear();custom.add(first);writeFolders();}
    if(auto xml=juce::XmlDocument::parse(directory.getChildFile("Catalog.xml")))if(xml->hasTagName("PLUGIN_CATALOG")&&xml->getIntAttribute("version")>=1&&xml->getIntAttribute("version")<=2){
        const bool rescan=xml->getIntAttribute("version")<2;
        for(auto* e:xml->getChildIterator())if(e->hasTagName("ENTRY")){
            Record r;r.format=e->getStringAttribute("format");r.id=e->getStringAttribute("id");r.stamp=e->getStringAttribute("stamp");r.status=e->getStringAttribute("status");r.validatedAt=e->getStringAttribute("validatedAt").getLargeIntValue();
            if(rescan)r.stamp.clear();
            // An interrupted child is quarantined until changed or explicitly retried.
            if(r.status=="pending")r.status="failed";
            for(auto* child:e->getChildIterator()){juce::PluginDescription d;if(d.loadFromXml(*child))r.plugins.add(d);}records.add(std::move(r));
        }
    }
    if(records.isEmpty())if(auto xml=juce::XmlDocument::parse(directory.getChildFile("KnownPlugins.xml")))if(xml->hasTagName("KNOWNPLUGINS")){
        juce::KnownPluginList known;known.recreateFromXml(*xml);
        for(const auto& d:known.getTypes()){Record r;r.format=d.pluginFormatName;r.id=d.fileOrIdentifier;r.status="ok";r.plugins.add(d);records.add(std::move(r));}
    }
    refreshPlugins();if(!progress.plugins.isEmpty())writeKnownList();
}
void PluginCatalog::writeCache(){
    juce::XmlElement xml("PLUGIN_CATALOG");xml.setAttribute("version",2);
    for(const auto& r:records){auto* e=xml.createNewChildElement("ENTRY");e->setAttribute("format",r.format);e->setAttribute("id",r.id);e->setAttribute("stamp",r.stamp);e->setAttribute("status",r.status);e->setAttribute("validatedAt",juce::String(r.validatedAt));for(const auto& d:r.plugins)e->addChildElement(d.createXml().release());}
    atomicXml(directory.getChildFile("Catalog.xml"),xml);writeKnownList();
}
namespace {
juce::String makerOf(const juce::PluginDescription& d){auto name=d.manufacturerName.trim();return name.isEmpty()?"Unknown manufacturer":name;}
void addEngines(std::vector<PluginCatalog::BrowserRow>& rows,const juce::String& query){
    struct Engine { const char* name; const char* detail; int id; };
    static const Engine engines[]{{"FaseGroove Piano","Built-in piano",0},{"FaseGroove Bass","Built-in bass",1},{"FaseGroove Strings","Built-in strings",2},{"FaseGroove Guitar","Built-in guitar",3},{"FaseGroove Drums","Built-in drums",4},{"FaseGroove Lead","Built-in lead",5}};
    for(const auto& e:engines){
        const juce::String name(e.name);
        if(query.isNotEmpty()&&!name.containsIgnoreCase(query)&&!juce::String(e.detail).containsIgnoreCase(query)&&!juce::String("FaseGroove").containsIgnoreCase(query))continue;
        PluginCatalog::BrowserRow row;row.kind=PluginCatalog::BrowserRow::Kind::engine;row.engine=e.id;row.name=name;row.detail=e.detail;row.manufacturer="FaseGroove";row.key="fg\x1f"+juce::String(e.id);rows.push_back(row);
    }
    if(query.isEmpty()||juce::String("Remove all effects").containsIgnoreCase(query)||juce::String("FaseGroove").containsIgnoreCase(query)){
        PluginCatalog::BrowserRow row;row.kind=PluginCatalog::BrowserRow::Kind::noEffect;row.name="Remove all effects";row.detail="Remove all effects";row.manufacturer="FaseGroove";row.key=juce::String("fg")+"\x1f"+"clear";rows.push_back(row);
    }
}
PluginCatalog::BrowserRow rowFor(const juce::PluginDescription& d,int index){
    PluginCatalog::BrowserRow row;row.kind=PluginCatalog::BrowserRow::Kind::plugin;row.plugin=index;row.instrument=d.isInstrument;row.name=d.name;row.detail=d.isInstrument?"Instrument":"Effect";row.format=d.pluginFormatName;row.manufacturer=makerOf(d);row.key=PluginCatalog::pluginKey(d);return row;
}
}
void PluginCatalog::writeKnownList(){
    juce::KnownPluginList known;for(const auto& d:progress.plugins)known.addType(d);
    if(auto xml=known.createXml())atomicXml(directory.getChildFile("KnownPlugins.xml"),*xml);
}
void PluginCatalog::refreshPlugins(){
    progress.plugins.clear();progress.failed=0;
    for(const auto& r:records){if(r.status=="failed")++progress.failed;if(r.status!="ok")continue;for(const auto& d:r.plugins)progress.plugins.add(d);}
    std::sort(progress.plugins.begin(),progress.plugins.end(),[](const auto& a,const auto& b){const int maker=a.manufacturerName.compareIgnoreCase(b.manufacturerName);if(maker!=0)return maker<0;if(a.isInstrument!=b.isInstrument)return a.isInstrument;return a.name.compareIgnoreCase(b.name)<0;});
}
bool PluginCatalog::scan(bool retryFailures){
    const juce::ScopedLock lock(mutex);if(progress.scanning||isThreadRunning())return false;
    retry=retryFailures;progress.scanning=true;progress.checked=progress.total=0;progress.current.clear();startThread();return true;
}
juce::String PluginCatalog::fingerprint(const juce::String& id){
    if(!juce::File::isAbsolutePath(id))return id; // AU registry identifiers are rescanned at startup.
    const juce::File file(id);juce::StringArray stamps;
    auto add=[&](const juce::File& f){stamps.add(f.getRelativePathFrom(file)+":"+juce::String(f.getSize())+":"+juce::String(f.getLastModificationTime().toMilliseconds()));};
    add(file);if(file.isDirectory())for(const auto& entry:juce::RangedDirectoryIterator(file,true,"*",juce::File::findFiles,juce::File::FollowSymlinks::no)){if(threadShouldExit())break;add(entry.getFile());}
    stamps.sort(false);const auto data=stamps.joinIntoString("\n");return juce::SHA256(data.toRawUTF8(),(size_t)data.getNumBytesAsUTF8()).toHexString();
}
void PluginCatalog::run(){
    auto paths=includeStandardPaths?standardFolders():juce::FileSearchPath();if(auto extra=customFolder();extra.isDirectory())paths.add(extra);paths.removeRedundantPaths();
    std::map<juce::String,Record> discovered;std::set<juce::String> visited;
    std::vector<juce::File> pending;for(int i=0;i<paths.getNumPaths();++i)pending.push_back(paths[i]);
    while(!pending.empty()&&!threadShouldExit()){
        auto dir=pending.back();pending.pop_back();if(!dir.isDirectory()||!visited.insert(pathKey(dir.getFullPathName())).second)continue;
        for(const auto& entry:juce::RangedDirectoryIterator(dir,false,"*",juce::File::findFilesAndDirectories,juce::File::FollowSymlinks::no)){
            if(threadShouldExit())break;const auto file=entry.getFile();
            if(file.hasFileExtension("vst3")){Record r;r.format="VST3";r.id=file.getLinkedTarget().getFullPathName();discovered.emplace("VST3:"+pathKey(r.id),r);}
            else if(file.isDirectory()&&!file.isSymbolicLink())pending.push_back(file);
        }
    }
   #if JUCE_MAC
    juce::AudioPluginFormatManager formats;formats.addDefaultFormats();
    for(int i=0;includeStandardPaths&&i<formats.getNumFormats();++i){auto* format=formats.getFormat(i);if(format->getName()=="AudioUnit")for(auto id:format->searchPathsForPlugins({},true,true)){Record r;r.format=format->getName();r.id=id;discovered.emplace(r.format+":"+id,r);}}
   #endif
    if(threadShouldExit()){const juce::ScopedLock lock(mutex);progress.scanning=false;return;}
    juce::Array<Record> previous;{const juce::ScopedLock lock(mutex);previous=records;records.clear();for(const auto& old:previous)if(discovered.count(old.format+":"+pathKey(old.id)))records.add(old);refreshPlugins();progress.total=(int)discovered.size();}
    auto storeRecord=[&](const Record& r){for(auto& old:records)if(old.format==r.format&&pathKey(old.id)==pathKey(r.id)){old=r;return;}records.add(r);};
    for(auto& item:discovered){
        if(threadShouldExit())break;auto r=item.second;r.stamp=fingerprint(r.id);if(threadShouldExit())break;
        for(const auto& old:previous)if(old.format==r.format&&pathKey(old.id)==pathKey(r.id)&&old.stamp==r.stamp&&!(retry&&old.status=="failed")&&(juce::File::isAbsolutePath(r.id)||old.status=="failed")){r=old;break;}
        {const juce::ScopedLock lock(mutex);progress.current=r.id;}
        const bool updated=r.status.isEmpty();
        if(updated){
            r.status="pending";{const juce::ScopedLock lock(mutex);storeRecord(r);writeCache();}
            const auto output=directory.getNonexistentChildFile("Scan-",".xml",false);directory.createDirectory();juce::ChildProcess child;
            const juce::StringArray args{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--scan-plugin",r.format,r.id,output.getFullPathName()};
            bool finished=false;
            if(child.start(args,0)){const double deadline=juce::Time::getMillisecondCounterHiRes()+60000.;while(!threadShouldExit()&&juce::Time::getMillisecondCounterHiRes()<deadline){if(child.waitForProcessToFinish(50)){finished=true;break;}}if(!finished)child.kill();}
            r.status="failed";r.validatedAt=juce::Time::currentTimeMillis();
            if(finished&&child.getExitCode()==0)if(auto xml=juce::XmlDocument::parse(output))if(xml->hasTagName("PLUGIN_SCAN")){
                r.status="ok";for(auto* e:xml->getChildIterator()){juce::PluginDescription d;if(d.loadFromXml(*e)&&d.pluginFormatName==r.format&&belongs(d,r.id))r.plugins.add(d);}
            }
            output.deleteFile();{const juce::ScopedLock lock(mutex);storeRecord(r);}
        }else{const juce::ScopedLock lock(mutex);storeRecord(r);}
        {const juce::ScopedLock lock(mutex);++progress.checked;if(updated){refreshPlugins();writeCache();}}
    }
    const juce::ScopedLock lock(mutex);refreshPlugins();writeCache();progress.current.clear();progress.scanning=false;
}
juce::String PluginCatalog::pluginKey(const juce::PluginDescription& d){return d.pluginFormatName+"\x1f"+d.fileOrIdentifier+"\x1f"+d.name+"\x1f"+(d.isInstrument?"i":"e");}
juce::StringArray PluginCatalog::pluginCategories(const juce::Array<juce::PluginDescription>& plugins){
    juce::StringArray makers;for(const auto& d:plugins){const auto maker=makerOf(d);if(!maker.equalsIgnoreCase("FaseGroove"))makers.addIfNotAlreadyThere(maker);}
    makers.sort(true);juce::StringArray cats;cats.add("FaseGroove");cats.addArray(makers);return cats;
}
std::vector<PluginCatalog::BrowserRow> PluginCatalog::browserRows(const juce::Array<juce::PluginDescription>& plugins,const juce::String& category,const juce::String& query,const juce::StringArray& pinned){
    std::vector<BrowserRow> all;addEngines(all,{});
    for(int i=0;i<plugins.size();++i)if(makerOf(plugins.getReference(i)).equalsIgnoreCase("FaseGroove"))all.push_back(rowFor(plugins.getReference(i),i));
    for(int i=0;i<plugins.size();++i)if(!makerOf(plugins.getReference(i)).equalsIgnoreCase("FaseGroove"))all.push_back(rowFor(plugins.getReference(i),i));
    const auto text=query.trim();
    if(text.isNotEmpty()){
        std::vector<BrowserRow> found;for(const auto& row:all)if(row.name.containsIgnoreCase(text)||row.detail.containsIgnoreCase(text)||row.manufacturer.containsIgnoreCase(text)||row.format.containsIgnoreCase(text))found.push_back(row);return found;
    }
    if(!pinned.isEmpty()){
        std::vector<BrowserRow> found;for(const auto& key:pinned){for(const auto& row:all)if(row.key==key){found.push_back(row);break;}}return found;
    }
    std::vector<BrowserRow> found;if(category.isEmpty())return found;
    for(const auto& row:all){if(row.manufacturer.equalsIgnoreCase(category))found.push_back(row);}return found;
}
int PluginCatalog::scanOne(const juce::String& name,const juce::String& id,const juce::File& output){
    juce::AudioPluginFormatManager formats;formats.addDefaultFormats();juce::XmlElement result("PLUGIN_SCAN");
    for(int i=0;i<formats.getNumFormats();++i){auto* format=formats.getFormat(i);if(format->getName()!=name)continue;juce::OwnedArray<juce::PluginDescription> types;format->findAllTypesForFile(types,id);for(auto* d:types)result.addChildElement(d->createXml().release());return !types.isEmpty()&&result.writeTo(output)?0:1;}return 1;
}
}
namespace fga {
bool PluginCatalog::verify(const juce::File& probe,const juce::File& evidence){
    evidence.createDirectory();const auto root=evidence.getChildFile("catalog-fixture");root.deleteRecursively();root.createDirectory();
    const auto library=root.getChildFile("Custom plugins with spaces"),nested=library.getChildFile("Maker/Nested");nested.createDirectory();
    const auto target=nested.getChildFile(probe.getFileName());
    auto copy=[&]{return probe.isDirectory()?probe.copyDirectoryTo(target):probe.copyFileTo(target);};
    bool ok=true;juce::String failedAt;auto note=[&](const char* name){if(!ok&&failedAt.isEmpty())failedAt=name;};
    {
        juce::PluginDescription fx,keys;fx.name="Spring";fx.manufacturerName="Acme";fx.pluginFormatName="VST3";fx.fileOrIdentifier="/plugins/Spring.vst3";fx.isInstrument=false;keys.name="Keys";keys.manufacturerName="Zebra";keys.pluginFormatName="VST3";keys.fileOrIdentifier="/plugins/Keys.vst3";keys.isInstrument=true;
        juce::Array<juce::PluginDescription> sample;sample.add(fx);sample.add(keys);const auto cats=pluginCategories(sample);
        ok=cats.size()==3&&cats[0]=="FaseGroove"&&cats[1]=="Acme"&&cats[2]=="Zebra"&&ok;
        const auto fg=browserRows(sample,"FaseGroove",{},{});ok=fg.size()==7&&fg[0].kind==BrowserRow::Kind::engine&&fg[0].engine==0&&fg[4].engine==4&&fg[5].engine==5&&fg[6].kind==BrowserRow::Kind::noEffect&&ok;
        const auto acme=browserRows(sample,"Acme",{},{});ok=acme.size()==1&&!acme[0].instrument&&acme[0].name=="Spring"&&ok;
        const auto foundRows=browserRows(sample,{},"keys",{});ok=foundRows.size()==1&&foundRows[0].instrument&&ok;
        const auto pinned=browserRows(sample,{},{},juce::StringArray(pluginKey(fx)));ok=pinned.size()==1&&pinned[0].name=="Spring"&&ok;
        juce::KnownPluginList round;round.addType(fx);round.addType(keys);auto xml=round.createXml();juce::KnownPluginList back;if(xml)back.recreateFromXml(*xml);int inst=0,eff=0;for(const auto& d:back.getTypes())(d.isInstrument?inst:eff)++;ok=xml&&xml->hasTagName("KNOWNPLUGINS")&&inst==1&&eff==1&&ok;note("model");
    }
    ok=ok&&standardFolders().getNumPaths()>=2&&copy();note("copy");const auto invalid=nested.getChildFile("Invalid.vst3");invalid.replaceWithText("invalid plugin fixture");
    auto wait=[](PluginCatalog& c){const auto end=juce::Time::getMillisecondCounterHiRes()+120000.;while(c.isThreadRunning()&&juce::Time::getMillisecondCounterHiRes()<end)juce::Thread::sleep(20);return !c.isThreadRunning()&&!c.state().scanning;};
    auto validation=[&](const juce::File& store,const juce::String& id){auto xml=juce::XmlDocument::parse(store.getChildFile("Catalog.xml"));if(xml)for(auto* e:xml->getChildIterator())if(e->getStringAttribute("id")==id)return e->getStringAttribute("validatedAt");return juce::String();};
    const auto store=root.getChildFile("Cache");
    {
        PluginCatalog catalog(store,false);ok=catalog.setCustomFolder(nested)&&catalog.setCustomFolder(library)&&catalog.customFolder().getFullPathName()==library.getLinkedTarget().getFullPathName()&&!catalog.setCustomFolder(library)&&ok;
        ok=catalog.scan()&&wait(catalog)&&ok;note("scan-run");auto state=catalog.state();int instruments=0;for(const auto& d:state.plugins)if(belongs(d,target.getFullPathName())){if(d.isInstrument)++instruments;ok=!d.manufacturerName.isEmpty()&&ok;}ok=instruments==1&&state.failed>=1&&ok;if(!ok&&failedAt.isEmpty())failedAt="scan-count "+juce::String(instruments)+"/"+juce::String(state.plugins.size())+" failed "+juce::String(state.failed);
        auto knownXml=juce::XmlDocument::parse(store.getChildFile("KnownPlugins.xml"));juce::KnownPluginList known;if(knownXml)known.recreateFromXml(*knownXml);int listed=0;for(const auto& d:known.getTypes())if(belongs(d,target.getFullPathName())&&d.isInstrument)++listed;ok=knownXml&&knownXml->hasTagName("KNOWNPLUGINS")&&listed==1&&ok;note("known-list");
        const auto validTime=validation(store,target.getFullPathName()),badTime=validation(store,invalid.getFullPathName());
        ok=catalog.scan()&&wait(catalog)&&ok;ok=validTime==validation(store,target.getFullPathName())&&badTime==validation(store,invalid.getFullPathName())&&ok;
        ok=catalog.scan(true)&&wait(catalog)&&ok;ok=badTime!=validation(store,invalid.getFullPathName())&&ok;
        if(target.isDirectory())target.getChildFile("scan-test-marker.txt").replaceWithText("updated bundle");else target.setLastModificationTime(juce::Time::getCurrentTime()+juce::RelativeTime::seconds(3));
        ok=catalog.scan()&&wait(catalog)&&ok;ok=validTime!=validation(store,target.getFullPathName())&&ok;
    }
    {
        auto xml=juce::XmlDocument::parse(store.getChildFile("Catalog.xml"));if(xml){for(auto* e:xml->getChildIterator())if(e->getStringAttribute("id")==invalid.getFullPathName())e->setAttribute("status","pending");atomicXml(store.getChildFile("Catalog.xml"),*xml);}else ok=false;
        PluginCatalog restored(store,false);ok=restored.customFolder().getFullPathName()==library.getLinkedTarget().getFullPathName()&&!restored.state().scanning&&restored.state().failed>=1&&ok;
        bool found=false;for(const auto& d:restored.state().plugins)found=found||belongs(d,target.getFullPathName());ok=found&&ok;
        target.deleteRecursively();ok=restored.scan()&&wait(restored)&&ok;for(const auto& d:restored.state().plugins)ok=!belongs(d,target.getFullPathName())&&ok;
        ok=copy()&&restored.scan()&&wait(restored)&&ok;found=false;for(const auto& d:restored.state().plugins)found=found||belongs(d,target.getFullPathName());ok=found&&ok;
        ok=restored.setCustomFolder(juce::File())&&restored.scan()&&wait(restored)&&ok;for(const auto& d:restored.state().plugins)ok=!belongs(d,target.getFullPathName())&&ok;
    }
    evidence.getChildFile("plugin-catalog.txt").replaceWithText(ok?"PASS isolated native VST3 discovery, one nested custom folder, instruments and effects, manufacturer browser order with FaseGroove first, KnownPluginList XML, unchanged-plugin cache, update detection, failed-plugin quarantine/retry, interrupted-scan recovery, removal and rediscovery":"FAIL plugin catalogue integration @ "+failedAt);
    root.deleteRecursively();return ok;
}
}
