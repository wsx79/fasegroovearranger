// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>
namespace fga {
// Discovery never loads plugin code in the workstation process.
class PluginCatalog : private juce::Thread {
public:
    struct State { juce::Array<juce::PluginDescription> plugins; juce::String current; int checked=0,total=0,failed=0; bool scanning=false; int count=0; };
    // Rows for the groove-style browser: FaseGroove engines first, then one manufacturer.
    struct BrowserRow {
        enum class Kind { engine, plugin, noEffect };
        Kind kind = Kind::plugin;
        int engine = -1, plugin = -1;
        bool instrument = false;
        juce::String name, detail, format, key, manufacturer;
    };
    explicit PluginCatalog(juce::File storage=defaultStorage(),bool includeStandard=true);
    ~PluginCatalog() override;
    static juce::File defaultStorage();
    static juce::FileSearchPath standardFolders();
    juce::File customFolder() const;
    // One personal folder. An empty file clears it. A repeat of the same path does nothing.
    bool setCustomFolder(const juce::File&);
    enum class ScanMode { changedOnly, retryFailed, all };
    bool scan(ScanMode mode=ScanMode::changedOnly);
    juce::String report() const;
    static juce::String pluginKey(const juce::PluginDescription&);
    // "FaseGroove" is always first. Other names are manufacturers.
    static juce::StringArray pluginCategories(const juce::Array<juce::PluginDescription>&);
    // category selects a group. A query searches every group. pinned keeps that order (favourites or recent).
    static std::vector<BrowserRow> browserRows(const juce::Array<juce::PluginDescription>&,const juce::String& category,const juce::String& query,const juce::StringArray& pinned);
    State state() const;
    // The scan progress and the plugin count, without copying the plugin list (for 4 Hz timers).
    State progressState() const;
    void cancel(){signalThreadShouldExit();}
    static int scanOne(const juce::String& format,const juce::String& identifier,const juce::File& output);
    static bool verify(const juce::File& probe,const juce::File& evidence);
private:
    struct Record { juce::String format,id,stamp,status,issue; juce::int64 validatedAt=0; juce::Array<juce::PluginDescription> plugins; };
    juce::File directory;
    const bool includeStandardPaths;
    mutable juce::CriticalSection mutex;
    juce::StringArray custom;
    juce::Array<Record> records;
    State progress;
    ScanMode scanMode=ScanMode::changedOnly;
    void run() override;
    void read();
    void writeFolders();
    void writeCache();
    void writeKnownList();
    void refreshPlugins();
    juce::String fingerprint(const juce::String&);
};
}
