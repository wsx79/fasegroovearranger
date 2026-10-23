// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Sample browser model: saved folders and favourites, listing, filters, search, sorting,
// breadcrumb, history, audio info and its cache.
#include "../Source/SampleLibrary.h"
#include <iostream>
#include <stdexcept>
using namespace fga::samples;
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static void writeWav(const juce::File& f,double rate,int channels,int samples){
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(f.createOutputStream().release(),rate,(unsigned)channels,24,{},0));
    juce::AudioBuffer<float> b(channels,samples);b.clear();w->writeFromAudioSampleBuffer(b,0,samples);
}
int main(){
    try{
        auto dir=juce::File::createTempFile("fga-browser");dir.createDirectory();
        auto kits=dir.getChildFile("Kits");kits.createDirectory();kits.getChildFile("909").createDirectory();
        writeWav(kits.getChildFile("Kick 10.wav"),44100,1,44100);       // 1 s
        writeWav(kits.getChildFile("kick 2.wav"),48000,2,24000);        // 0.5 s
        writeWav(kits.getChildFile("Snare long.wav"),44100,2,88200);    // 2 s
        kits.getChildFile("loop.mp3").replaceWithText("not decoded here");
        kits.getChildFile("notes.txt").replaceWithText("text");
        kits.getChildFile("broken.wav").replaceWithText("not audio");
        kits.getChildFile(".hidden.wav").replaceWithText("x");

        // Preferences round-trip: folders, favourites, last folder, view options.
        Preferences p;check(p.addFolder(kits)&&!p.addFolder(kits),"folders are unique");
        check(p.addFolder(dir),"second folder");p.toggleFavourite(dir.getFullPathName());p.lastFolder=kits.getChildFile("909").getFullPathName();
        p.sort=byLength;p.ascending=false;p.filter=wavOnly;p.autoPreview=true;
        auto q=Preferences::fromJson(p.toJson());
        check(q.folders.size()==2&&q.isFavourite(dir.getFullPathName())&&!q.isFavourite(kits.getFullPathName()),"folders and favourites saved");
        check(q.lastFolder==p.lastFolder&&q.sort==byLength&&!q.ascending&&q.filter==wavOnly&&q.autoPreview,"last folder and view options saved");
        check(q.ordered().front().path==dir.getFullPathName(),"favourites first");
        check(q.rootFor(kits.getChildFile("909"))==kits,"deepest configured root");
        auto moved=dir.getChildFile("Moved");check(q.replaceFolder(kits.getFullPathName(),moved)&&q.folders[0].path==moved.getFullPathName(),"edit a folder");
        check(q.lastFolder==moved.getChildFile("909").getFullPathName(),"edited folder keeps the last visited place inside it");
        check(q.removeFolder(moved.getFullPathName())&&q.folders.size()==1&&!q.removeFolder("/nope"),"remove a folder");
        check(Preferences::fromJson("garbage").folders.empty()&&Preferences::fromJson("{\"folders\":[{\"path\":\"relative\"}],\"sort\":99}").folders.empty(),"bad settings are ignored");
        check(Preferences::fromJson("{\"sort\":99}").sort==numSorts-1,"out of range values are clamped");

        // Listing: one level, hidden files skipped, unavailable folders reported.
        auto listing=list(kits);check(listing.available&&listing.entries.size()==7,"one level, no hidden files");
        auto missing=list(dir.getChildFile("gone"));check(!missing.available&&missing.entries.empty()&&missing.error.isNotEmpty(),"unavailable folder");
        bool stopped=false;auto cancelled=list(kits,[&]{return stopped=true;});check(stopped&&cancelled.entries.empty(),"listing can be cancelled");

        // Audio info and cache.
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        auto info=readInfo(kits.getChildFile("kick 2.wav"),formats);
        check(info.valid&&info.channels==2&&info.sampleRate==48000&&std::abs(info.seconds-.5)<1e-9&&info.bits==24&&info.format=="WAV","WAV info");
        check(!readInfo(kits.getChildFile("broken.wav"),formats).valid&&!readInfo(kits.getChildFile("loop.mp3"),formats).valid,"unreadable and unsupported files have no info");
        InfoCache cache;cache.put(kits.getChildFile("kick 2.wav"),info);AudioInfo cached;check(cache.get(kits.getChildFile("kick 2.wav"),cached)&&cached.sampleRate==48000,"cache hit");
        check(!cache.get(kits.getChildFile("Kick 10.wav"),cached),"cache miss");
        writeWav(kits.getChildFile("kick 2.wav"),44100,1,1000);check(!cache.get(kits.getChildFile("kick 2.wav"),cached),"an edited file is read again");
        writeWav(kits.getChildFile("kick 2.wav"),48000,2,24000);

        // Filters, search and sorting (folders first, natural order, unknown lengths last).
        auto names=[&](const std::vector<int>& rows){juce::StringArray s;for(int r:rows)s.add(listing.entries[(size_t)r].name);return s.joinIntoString("|");};
        listing=list(kits);
        check(names(arrange(listing.entries,"",allAudio,byName,true))=="909|broken.wav|kick 2.wav|Kick 10.wav|loop.mp3|Snare long.wav","all audio, natural name order, folders first");
        check(names(arrange(listing.entries,"",allFiles,byName,true)).contains("notes.txt"),"all files");
        check(!names(arrange(listing.entries,"",wavOnly,byName,true)).contains("loop.mp3"),"type filter");
        check(names(arrange(listing.entries,"KICK",allAudio,byName,true))=="kick 2.wav|Kick 10.wav","search ignores case");
        check(names(arrange(listing.entries,"kick 10",allAudio,byName,true))=="Kick 10.wav","every search word must match");
        check(names(arrange(listing.entries,"",wavOnly,byName,false))=="909|Snare long.wav|Kick 10.wav|kick 2.wav|broken.wav","name descending");
        auto length=[&](const Entry& e){AudioInfo i=readInfo(e.file,formats);return i.valid?i.seconds:-1.;};
        check(names(arrange(listing.entries,"",wavOnly,byLength,true,length))=="909|kick 2.wav|Kick 10.wav|Snare long.wav|broken.wav","length ascending, unknown last");
        check(names(arrange(listing.entries,"",wavOnly,byLength,false,length))=="909|Snare long.wav|Kick 10.wav|kick 2.wav|broken.wav","length descending, unknown last");
        check(names(arrange(listing.entries,"",wavOnly,bySize,false)).startsWith("909|Snare long.wav"),"size descending");
        kits.getChildFile("Kick 10.wav").setLastModificationTime(juce::Time(2001,0,1,0,0));
        listing=list(kits);check(names(arrange(listing.entries,"",wavOnly,byDate,true)).startsWith("909|Kick 10.wav"),"date ascending");

        // Thousands of files stay fast to arrange (the list itself is virtualised).
        std::vector<Entry> many(20000);for(int i=0;i<20000;++i){many[(size_t)i].name="hit "+juce::String(20000-i)+".wav";many[(size_t)i].file=kits.getChildFile(many[(size_t)i].name);many[(size_t)i].size=i;}
        const auto t0=juce::Time::getMillisecondCounterHiRes();auto rows=arrange(many,"hit",allAudio,byName,true);
        check(rows.size()==20000&&many[(size_t)rows.front()].name=="hit 1.wav"&&juce::Time::getMillisecondCounterHiRes()-t0<2000,"20000 files sorted quickly");

        // Breadcrumb and history.
        auto crumbs=breadcrumb(kits.getChildFile("909"),kits);check(crumbs.size()==2&&crumbs[0]==kits,"breadcrumb from the configured root");
        check(breadcrumb(kits,juce::File()).getLast()==kits&&breadcrumb(kits,juce::File()).size()>2,"breadcrumb without a root reaches the volume");
        History h;h.visit(dir);h.visit(kits);h.visit(kits.getChildFile("909"));check(h.canBack()&&!h.canForward(),"history");
        check(h.back()==kits&&h.back()==dir&&!h.canBack()&&h.forward()==kits,"back and forward");
        h.visit(dir.getChildFile("Other"));check(!h.canForward()&&h.current()==dir.getChildFile("Other"),"a new visit drops forward");

        check(formatDuration(.5)=="0:00.50"&&formatDuration(134)=="2:14"&&formatSize(2048)=="2 KB"&&formatSize(5*1024*1024)=="5.0 MB","formatting");
        dir.deleteRecursively();
        std::cout<<"Sample browser model passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
