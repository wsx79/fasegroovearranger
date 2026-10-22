// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
namespace fga {
// Discovery never loads plugin code in the workstation process.
class PluginCatalog : private juce::Thread {
public:
    struct State { juce::Array<juce::PluginDescription> plugins; juce::String current; int checked=0,total=0,failed=0; bool scanning=false; };
    explicit PluginCatalog(juce::File storage=defaultStorage(),bool includeStandard=true);
    ~PluginCatalog() override;
    static juce::File defaultStorage();
    static juce::FileSearchPath standardFolders();
    juce::StringArray folders() const;
    bool addFolder(const juce::File&);
    bool removeFolder(int);
    bool scan(bool retryFailures=false);
    State state() const;
    void cancel(){signalThreadShouldExit();}
    static int scanOne(const juce::String& format,const juce::String& identifier,const juce::File& output);
    static bool verify(const juce::File& probe,const juce::File& evidence);
private:
    struct Record { juce::String format,id,stamp,status; juce::int64 validatedAt=0; juce::Array<juce::PluginDescription> plugins; };
    juce::File directory;
    const bool includeStandardPaths;
    mutable juce::CriticalSection mutex;
    juce::StringArray custom;
    juce::Array<Record> records;
    State progress;
    bool retry=false;
    void run() override;
    void read();
    void writeFolders();
    void writeCache();
    void refreshPlugins();
    juce::String fingerprint(const juce::String&);
};
}
