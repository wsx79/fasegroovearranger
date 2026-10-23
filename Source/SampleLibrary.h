// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Sample browser model: folders and favourites (saved as user preferences), one folder's
// listing, search/filter/sort, navigation history and a small audio-info cache. No UI and no
// audio thread here: listing and info reading run on the browser's background thread.
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <unordered_map>
#include <vector>
namespace fga::samples {

// Read by the arranger's sampler (JUCE basic formats, MP3 decoding is not built in).
bool isSupportedAudio(const juce::File&);
// Audio files shown in "All audio": the supported ones plus common formats that are listed
// with a clear "not supported" note instead of silently hidden.
bool isAudioFile(const juce::File&);

struct Folder { juce::String path; bool favourite=false; };

struct Preferences {
    std::vector<Folder> folders;
    juce::String lastFolder;
    int sort=0,filter=0;
    bool ascending=true,autoPreview=false;
    juce::String toJson() const;
    static Preferences fromJson(const juce::String&);
    bool addFolder(const juce::File&);                       // false if already there
    bool removeFolder(const juce::String& path);
    bool replaceFolder(const juce::String& path,const juce::File& with);
    void toggleFavourite(const juce::String& path);
    bool isFavourite(const juce::String& path) const;
    std::vector<Folder> ordered() const;                     // favourites first, then as added
    // The configured folder that contains `f` (the breadcrumb root), or an empty File.
    juce::File rootFor(const juce::File& f) const;
};

struct Entry {
    juce::File file;
    juce::String name;      // file name
    bool folder=false;
    juce::int64 size=0;
    juce::Time modified;
};

struct Listing {
    juce::File folder;
    std::vector<Entry> entries;
    bool available=true;
    juce::String error;
};
// One folder, not recursive, hidden files skipped. `stop` is polled so a newer request can
// cancel a long listing. Never call on the audio thread.
Listing list(const juce::File& folder,const std::function<bool()>& stop={});

struct AudioInfo {
    bool valid=false;
    double seconds=0,sampleRate=0;
    int channels=0,bits=0;
    juce::String format;
};
AudioInfo readInfo(const juce::File&,juce::AudioFormatManager&);

// Thread-safe. Keyed by path, size and date, so an edited file is read again.
class InfoCache {
public:
    bool get(const juce::File&,AudioInfo&) const;  // reads size and date from disk
    void put(const juce::File&,const AudioInfo&);
    bool get(const Entry&,AudioInfo&) const; // uses the listing's size and date: no disk access
    void put(const Entry&,const AudioInfo&);
    int size() const;
private:
    static juce::String key(const juce::String& path,juce::int64 size,juce::int64 modified);
    mutable juce::CriticalSection lock;
    std::unordered_map<juce::String,AudioInfo> map;
};

enum Filter { allAudio=0, wavOnly, aiffOnly, flacOnly, oggOnly, allFiles, numFilters };
enum Sort { byName=0, byDate, bySize, byLength, numSorts };
bool passesFilter(const Entry&,int filter);
// Indexes into `entries` to show: folders first (by name), then the files that pass the
// filter and the search (every word, any case), sorted. Unknown lengths sort last.
std::vector<int> arrange(const std::vector<Entry>&,const juce::String& search,int filter,int sort,bool ascending,
                         const std::function<double(const Entry&)>& lengthOf={});

// From the root folder down to `folder` (just `folder`'s own chain up to the volume when it
// is not inside `root`).
juce::Array<juce::File> breadcrumb(const juce::File& folder,const juce::File& root);

class History {
public:
    void visit(const juce::File&);     // drops the forward list
    bool canBack() const {return index>0;}
    bool canForward() const {return index+1<(int)items.size();}
    juce::File back();
    juce::File forward();
    juce::File current() const {return index>=0?items[(size_t)index]:juce::File();}
private:
    std::vector<juce::File> items;
    int index=-1;
};

juce::String formatDuration(double seconds);   // 0:03.25 / 2:14
juce::String formatSize(juce::int64 bytes);    // 812 KB / 4.2 MB
}
