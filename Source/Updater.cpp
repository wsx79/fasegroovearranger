// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Updater.h"
#include "Localisation.h"
#include <array>
#include <atomic>
#include <vector>
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif


namespace fga::Updater
{
namespace
{
    constexpr const char* latestReleaseApi = "https://api.github.com/repos/wsx79/fasegroovearranger/releases/latest";
    constexpr const char* rawBase = "https://raw.githubusercontent.com/wsx79/fasegroovearranger/";

    const char* assetName (InstallKind kind)
    {
       #if JUCE_WINDOWS
        return kind == InstallKind::installer ? "FaseGrooveArranger-Windows-Setup.exe" : "FaseGrooveArranger-Windows.zip";
       #elif JUCE_MAC
        juce::ignoreUnused (kind);
        return "FaseGrooveArranger-macOS.zip";
       #else
        juce::ignoreUnused (kind);
        return "FaseGrooveArranger-Linux.zip";
       #endif
    }

    const juce::String productName ("Fase Groove Arranger");

    /** The standalone executable or app bundle this code is running from. */
    juce::File runningBundle()
    {
        auto module = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        juce::File bundle = module;
        for (auto f = module.getParentDirectory(); f != f.getParentDirectory(); f = f.getParentDirectory())
            if (f.hasFileExtension ("vst3;component;app"))
                bundle = f;   // the outermost one
        return bundle;
    }

    /** The standalone app in the unpacked family ZIP. */
    juce::File stagedCounterpart (const juce::File& staged, const juce::File& target)
    {
        return staged.getChildFile ("Standalone").getChildFile (target.getFileName());
    }

   #if JUCE_MAC
    bool runTool (const juce::StringArray& args)
    {
        juce::ChildProcess p;
        return p.start (args, 0) && p.waitForProcessToFinish (120000) && p.getExitCode() == 0;
    }
   #endif

    bool unpack (const juce::File& zip, const juce::File& folder)
    {
        juce::ZipFile checked (zip);
        if (checked.getNumEntries() == 0) return false;
        for (int i=0;i<checked.getNumEntries();++i)
        {
            const auto name=checked.getEntry(i)->filename.replace("\\", "/");
            juce::StringArray parts;parts.addTokens(name,"/",{});
            if(name.startsWithChar('/') || name.containsChar(':') || parts.contains("..")) return false;
        }
        folder.deleteRecursively();
        if (! folder.createDirectory())
            return false;
       #if JUCE_MAC
        return runTool ({ "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), folder.getFullPathName() });   // keeps bundles intact
       #else
        juce::ZipFile archive (zip);
        if (archive.getNumEntries() == 0 || archive.uncompressTo (folder).failed())
            return false;
        #if JUCE_LINUX || JUCE_BSD
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, true))   // zip entries lose the execute bit
            if (f.getFileExtension().isEmpty() || f.hasFileExtension ("so"))
                f.setExecutePermission (true);
        #endif
        return true;
       #endif
    }

    /** Only this running app is replaced; hosted instruments and user documents are untouched. */
    juce::Array<juce::File> updateTargets (const juce::File& staged)
    {
        juce::Array<juce::File> candidates;
        const auto running = runningBundle();
        candidates.add (running);


        juce::Array<juce::File> targets;
        for (const auto& f : candidates)
            if (f.exists() && f.getParentDirectory().hasWriteAccess() && stagedCounterpart (staged, f).exists()
                && ! targets.contains (f))
                targets.add (f);
        return targets;
    }

    /** Replaces copies of the plug-in as one transaction: everything is checked first, every
        change is recorded, and if any step fails all of them are undone (including the copies
        already replaced), so a failed update never leaves a half-new plug-in. */
    class Transaction
    {
    public:
        explicit Transaction (int failAfterSteps = -1) : failAfter (failAfterSteps) {}
        ~Transaction() { if (! committed) rollback(); }

        /** Checks that every file can be replaced before anything is touched. */
        static bool preflight (const juce::File& staged, const juce::Array<juce::File>& targets)
        {
            for (const auto& target : targets)
            {
                const auto fresh = stagedCounterpart (staged, target);
                if (! fresh.exists() || ! target.getParentDirectory().hasWriteAccess())
                    return false;
               #if JUCE_WINDOWS
                // Leftovers of an earlier update that are still loaded would block the renames.
                juce::Array<juce::File> files;
                if (fresh.isDirectory()) files = fresh.findChildFiles (juce::File::findFiles, true);
                else                     files.add (fresh);
                for (const auto& f : files)
                {
                    const auto dest = fresh.isDirectory() ? target.getChildFile (f.getRelativePathFrom (fresh)) : target;
                    const auto old = dest.getSiblingFile (dest.getFileName() + ".old");
                    if (old.exists() && ! old.deleteFile())
                        return false;
                }
               #endif
            }
            return true;
        }

        bool replace (const juce::File& fresh, const juce::File& target)
        {
           #if JUCE_WINDOWS
            // A loaded DLL (or the running .exe) can't be overwritten and its folder can't be
            // renamed, but the file itself can: move each old file aside, copy the new one in.
            juce::Array<juce::File> files;
            if (fresh.isDirectory()) files = fresh.findChildFiles (juce::File::findFiles, true);
            else                     files.add (fresh);

            for (const auto& f : files)
            {
                const auto dest = fresh.isDirectory() ? target.getChildFile (f.getRelativePathFrom (fresh)) : target;
                if (! step() || ! dest.getParentDirectory().createDirectory())
                    return false;

                juce::File backup;
                if (dest.exists())
                {
                    backup = dest.getSiblingFile (dest.getFileName() + ".old");
                    if (backup.exists() || ! dest.moveFileTo (backup))
                        return false;
                }
                changes.push_back ({ dest, backup });
                if (! f.copyFileTo (dest))
                    return false;
            }
            return true;
           #else
            // macOS / Linux: build the new copy next to the old one, then swap them by renaming
            // (the running code keeps working from the old files until the DAW restarts).
            const auto incoming = target.getSiblingFile (target.getFileName() + ".new");
            const auto backup = target.getSiblingFile (target.getFileName() + ".old");
            incoming.deleteRecursively();
            backup.deleteRecursively();
            if (! step())
                return false;
            #if JUCE_MAC
            if (! runTool ({ "/usr/bin/ditto", fresh.getFullPathName(), incoming.getFullPathName() }))
            {
                incoming.deleteRecursively();
                return false;
            }
            runTool ({ "/usr/bin/xattr", "-dr", "com.apple.quarantine", incoming.getFullPathName() });
            #else
            if (! (fresh.isDirectory() ? fresh.copyDirectoryTo (incoming) : fresh.copyFileTo (incoming)))
            {
                incoming.deleteRecursively();
                return false;
            }
            for (const auto& f : incoming.isDirectory() ? incoming.findChildFiles (juce::File::findFiles, true) : juce::Array<juce::File> { incoming })
                if (f.getFileExtension().isEmpty() || f.hasFileExtension ("so") || f == incoming)
                    f.setExecutePermission (true);
            #endif
            if (! step() || ! target.moveFileTo (backup))
            {
                incoming.deleteRecursively();
                return false;
            }
            changes.push_back ({ target, backup });
            if (! step() || ! incoming.moveFileTo (target))
            {
                incoming.deleteRecursively();
                return false;
            }
            return true;
           #endif
        }

        /** Keeps the new files; the old ones are deleted (or, on Windows, at the next start if loaded). */
        void commit()
        {
            committed = true;
            for (const auto& c : changes)
                if (c.backup != juce::File())
                    c.backup.deleteRecursively();
        }

        void rollback()
        {
            for (auto it = changes.rbegin(); it != changes.rend(); ++it)
            {
                it->dest.deleteRecursively();
                if (it->backup != juce::File())
                    it->backup.moveFileTo (it->dest);
            }
            changes.clear();
            committed = true;
        }

    private:
        struct Change { juce::File dest, backup; };   // backup empty: dest is a new file

        bool step() { return failAfter < 0 || steps++ < failAfter; }   // failure injection for the tests

        std::vector<Change> changes;
        int failAfter = -1, steps = 0;
        bool committed = false;
    };

    bool replaceAll (const juce::File& staged, const juce::Array<juce::File>& targets, int failAfterSteps)
    {
        if (targets.isEmpty() || ! Transaction::preflight (staged, targets))
            return false;

        Transaction transaction (failAfterSteps);
        for (const auto& target : targets)
            if (! transaction.replace (stagedCounterpart (staged, target), target))
            {
                transaction.rollback();
                return false;
            }
        transaction.commit();
        return true;
    }

   #if JUCE_LINUX || JUCE_BSD
    // The plug-in is built without libcurl (no extra dependency), so on Linux the system's
    // curl command does the HTTPS work.
    bool curl (juce::Thread& thread, juce::StringArray args, juce::String* output)
    {
        args.insert (0, "curl");
        args.insert (1, "-sSfL");
        args.insert (2, "--max-time");
        args.insert (3, output != nullptr ? "15" : "600");
        args.insert (4, "-A");
        args.insert (5, "FaseGrooveArranger-Updater");

        args.addArray(juce::StringArray{"--connect-timeout","10","--speed-time","15","--speed-limit","1024","--max-filesize","268435456"});
        juce::ChildProcess process;
        if (! process.start (args, output != nullptr ? juce::ChildProcess::wantStdOut : 0))
            return false;

        juce::MemoryOutputStream out;
        while (process.isRunning())
        {
            if (thread.threadShouldExit())
            {
                process.kill();
                return false;
            }
            if (output != nullptr)
            {
                char buffer[4096];
                const int n = process.readProcessOutput (buffer, (int) sizeof (buffer));
                if (n > 0) out.write (buffer, (size_t) n);
                if(out.getDataSize()>1024*1024){process.kill();return false;}
                if(n<=0)juce::Thread::sleep (20);
            }
            else
            {
                juce::Thread::sleep (50);
            }
        }

        if (output != nullptr)
        {
            char buffer[4096];
            for (int n; (n = process.readProcessOutput (buffer, (int) sizeof (buffer))) > 0;)
                out.write (buffer, (size_t) n);
            *output = out.toUTF8();
        }
        return process.getExitCode() == 0;
    }
   #endif

   #if ! (JUCE_LINUX || JUCE_BSD)
    std::unique_ptr<juce::InputStream> open (const juce::String& url)
    {
        return juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                      .withConnectionTimeoutMs (15000)
                                                      .withExtraHeaders ("User-Agent: FaseGrooveArranger-Updater\r\nAccept: application/vnd.github+json"));
    }
   #endif

    bool fetchText (juce::Thread& thread, const juce::String& url, juce::String& text)
    {
       #if JUCE_LINUX || JUCE_BSD
        return curl (thread, { url }, &text) && text.isNotEmpty();
       #else
        juce::ignoreUnused (thread);
        auto stream = open (url);
        if (stream == nullptr)
            return false;
        juce::MemoryOutputStream data;
        while(!thread.threadShouldExit()&&!stream->isExhausted()&&data.getDataSize()<=1024*1024){char b[4096];const int n=stream->read(b,sizeof(b));if(n<=0)break;data.write(b,(size_t)n);}
        if(data.getDataSize()>1024*1024)return false;text=data.toUTF8();
        return text.isNotEmpty()&&!thread.threadShouldExit();
       #endif
    }

    bool fetchFile (juce::Thread& thread, const juce::String& url, const juce::File& target)
    {
        auto partial = target.getSiblingFile (target.getFileName() + ".part");
        partial.deleteFile();

       #if JUCE_LINUX || JUCE_BSD
        if (! curl (thread, { "-o", partial.getFullPathName(), url }, nullptr))
        {
            partial.deleteFile();
            return false;
        }
       #else
        {
            auto stream = open (url);
            if (stream == nullptr)
                return false;

            juce::FileOutputStream out (partial);
            if (! out.openedOk())
                return false;

            juce::HeapBlock<char> buffer (65536);
            for (;;)
            {
                if (thread.threadShouldExit())
                {
                    out.flush();
                    partial.deleteFile();
                    return false;
                }
                const int n = stream->read (buffer, 65536);
                if (n <= 0)
                    break;
                out.write (buffer, (size_t) n);
            }
            out.flush();
            if (out.getStatus().failed() || ! stream->isExhausted())
            {
                partial.deleteFile();
                return false;
            }
        }
       #endif

        target.deleteFile();
        return partial.getSize() > 0 && partial.moveFileTo (target);
    }

    juce::File downloadsFolder()
    {
        auto downloads = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
        if (! downloads.isDirectory())
            downloads = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        return downloads.getChildFile ("Fase Groove Arranger updates");
    }

    /** Markdown -> plain text that reads well in a text box. */
    juce::String plain (juce::String text)
    {
        text = text.replace ("\r\n", "\n").replace ("**", "").replace ("*", "").replace ("`", "");
        juce::StringArray raw, lines;
        raw.addLines (text);
        for (auto& line : raw)
        {
            // Changelog lines are wrapped at ~100 columns: join them back (the text box wraps).
            if (line.startsWith ("  ") && line.trim().isNotEmpty() && ! lines.isEmpty() && lines[lines.size() - 1].isNotEmpty())
                lines.getReference (lines.size() - 1) << " " << line.trim();
            else
                lines.add (line);
        }
        for (auto& line : lines)
        {
            if (line.startsWith ("### ")) line = Localisation::upper (line.substring (4));
            else if (line.startsWith ("## ")) line = line.substring (3);
            else if (line.startsWith ("- ")) line = juce::String::fromUTF8 ("\xe2\x80\xa2 ") + line.substring (2);
        }
        return lines.joinIntoString ("\n").trim();
    }
}

bool autoCheckEnabled()
{
    return Localisation::setting ("autoUpdate", "1") != "0";
}

void setAutoCheckEnabled (bool enabled)
{
    Localisation::setSetting ("autoUpdate", enabled ? "1" : "0");
}

void skipVersion (const juce::String& version)
{
    Localisation::setSetting ("skippedVersion", version);
}

bool isNewer (const juce::String& a, const juce::String& b)
{
    auto parts = [] (juce::String v)
    {
        v = v.trim().trimCharactersAtStart ("vV");
        juce::StringArray tokens;
        tokens.addTokens (v.upToFirstOccurrenceOf ("-", false, false), ".", {});
        std::array<int, 3> n {};
        for (int i = 0; i < 3 && i < tokens.size(); ++i)
            n[(size_t) i] = tokens[i].getIntValue();
        return n;
    };
    return parts (a) > parts (b);
}

juce::String changelogSection (const juce::String& changelog, const juce::String& version)
{
    const auto v = version.trimCharactersAtStart ("vV");
    juce::StringArray lines;
    lines.addLines (changelog);

    juce::StringArray section;
    bool inside = false;
    for (auto& line : lines)
    {
        if (line.startsWith ("## "))
        {
            if (inside)
                break;
            inside = line.startsWith ("## [" + v + "]") || line.startsWith ("## " + v + " ") || line == "## " + v;
            continue;
        }
        if (inside)
            section.add (line);
    }
    return plain (section.joinIntoString ("\n"));
}

InstallKind installKind()
{
    return testing::installKindFor(runningBundle());
}

InstallKind testing::installKindFor(const juce::File& bundle)
{
   #if JUCE_WINDOWS
    // Detect setup copies, including custom folders, before considering writable portable copies.
    if(bundle.getSiblingFile("FaseGrooveArranger.install.ini").existsAsFile()
       &&bundle.getSiblingFile("unins000.exe").existsAsFile())return InstallKind::installer;
   #endif
    return bundle.exists() && bundle.getParentDirectory().hasWriteAccess() ? InstallKind::portable : InstallKind::manual;
}

bool isStandaloneApp()
{
    return juce::JUCEApplicationBase::isStandaloneApp();
}

namespace
{
    /** The delayed launcher receives a positional argument; paths never become shell code. */
    bool relaunchLater(const juce::File& app)
    {
       #if JUCE_WINDOWS
        const auto helper=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("FaseGrooveArranger-relaunch", ".ps1",false);
        if(!helper.replaceWithText("param([int]$OwnerPid,[string]$App)\nWait-Process -Id $OwnerPid -ErrorAction SilentlyContinue\nStart-Process -FilePath $App\nRemove-Item -LiteralPath $PSCommandPath\n"))return false;
        juce::ChildProcess p;
        return p.start(juce::StringArray{"powershell.exe","-NoProfile","-NonInteractive","-WindowStyle","Hidden","-ExecutionPolicy","Bypass","-File",helper.getFullPathName(),"-OwnerPid",juce::String(_getpid()),"-App",app.getFullPathName()},0);
       #else
        juce::ChildProcess p;
        #if JUCE_MAC
        return p.start (juce::StringArray { "/bin/sh", "-c", "while kill -0 \"$2\" 2>/dev/null; do sleep 0.2; done; exec /usr/bin/open -n \"$1\"", "fga-update", app.getFullPathName(),juce::String((juce::int64)getpid()) }, 0);
        #else
        return p.start (juce::StringArray { "/bin/sh", "-c", "while kill -0 \"$2\" 2>/dev/null; do sleep 0.2; done; exec \"$1\"", "fga-update", app.getFullPathName(),juce::String((juce::int64)getpid()) }, 0);
        #endif
       #endif
    }

}

InstallReport install (const Result& result)
{
    InstallReport report;

    if(!testing::verifyDownload(result.file,result.digest))return report;
    // Linux resolves /proc/self/exe to the renamed/deleted old image after a swap.
    // Capture the destination now so relaunch always opens the new executable.
    const auto app=runningBundle();
    if (result.kind == InstallKind::installer && result.file.existsAsFile())
    {
        // Same setup, no wizard: it keeps the folders and parts you chose last time and asks to
        // close the DAW only if the plug-in is in use. In the standalone app the setup reopens it
        // when it is done (RELAUNCH), and the app quits so its files can be replaced.
        const bool standalone = isStandaloneApp();
        report.installerStarted = report.done
            = result.file.startAsProcess ((standalone ? juce::String("/SILENT /NORESTART /CLOSEAPPLICATIONS /RELAUNCH=1") : juce::String("/SILENT /NORESTART"))
                                         + " /DIR=\"" + app.getParentDirectory().getFullPathName() + "\"");
        report.relaunching = report.done && standalone;
        if(!report.done)result.file.revealToUser();
        return report;
    }

    if (result.kind == InstallKind::portable && result.staged.isDirectory())
    {
        // All copies or none: on any error everything already replaced is put back.
        const auto targets = updateTargets (result.staged);
        if (replaceAll (result.staged, targets, -1))
        {
            for (const auto& t : targets)
                report.updated.add (t.getFullPathName());
            report.done = true;
            report.relaunching = isStandaloneApp() && relaunchLater(app);
            return report;
        }
    }

    // Fallback: show the download so it can be installed by hand.
   #if JUCE_MAC
    result.file.startAsProcess();   // Archive Utility unzips it next to the zip
    result.file.getParentDirectory().revealToUser();
   #elif JUCE_WINDOWS
    if (result.file.hasFileExtension ("exe"))
        report.installerStarted = result.file.startAsProcess();
    else
        result.file.revealToUser();
   #else
    result.file.revealToUser();
   #endif
    return report;
}

namespace testing
{
    bool unpack(const juce::File& zip,const juce::File& folder){return fga::Updater::unpack(zip,folder);}

    bool verifyDownload(const juce::File& file,const juce::String& digest)
    {
        const auto hash=digest.fromFirstOccurrenceOf("sha256:",false,false).toLowerCase();
        if(!digest.startsWith("sha256:")||hash.length()!=64||hash.containsOnly("0123456789abcdef")==false||!file.existsAsFile()||file.getSize()<=0||file.getSize()>256*1024*1024)return false;
        auto input=file.createInputStream();return input!=nullptr&&juce::SHA256(*input).toHexString()==hash;
    }

    juce::String releaseAsset(const juce::var& release,InstallKind kind,juce::String& digest)
    {
        digest.clear();const auto tag=release.getProperty("tag_name",{}).toString();
        if(tag.isEmpty()||!tag.containsOnly("vV0123456789.")||(bool)release.getProperty("draft",false)||(bool)release.getProperty("prerelease",false))return {};
        const juce::String name=assetName(kind);
        const auto expected="https://github.com/wsx79/fasegroovearranger/releases/download/"+tag+"/"+name;
        if(auto* assets=release.getProperty("assets",{}).getArray())for(const auto& a:*assets)
            if(a.getProperty("name",{}).toString()==name&&a.getProperty("browser_download_url",{}).toString()==expected){
                digest=a.getProperty("digest",{}).toString();
                if(digest.startsWith("sha256:")&&digest.length()==71)return expected;
            }
        digest.clear();return {};
    }

    bool replaceAll (const juce::File& staged, const juce::Array<juce::File>& targets, int failAfterSteps)
    {
        return fga::Updater::replaceAll (staged, targets, failAfterSteps);
    }
}

void cleanupAfterUpdate()
{
    const auto bundle = runningBundle();
    juce::Array<juce::File> leftovers;
    if (bundle.isDirectory())
        leftovers = bundle.findChildFiles (juce::File::findFiles, true, "*.old");
    leftovers.add (bundle.getSiblingFile (bundle.getFileName() + ".old"));
    for (const auto& f : leftovers)
        if (f.exists())
            f.deleteRecursively();   // fails quietly while something still has it loaded
}

bool firstAutoCheckThisSession()
{
    static std::atomic<bool> checked { false };
    return ! checked.exchange (true);
}

//==============================================================================
Checker::Checker() : juce::Thread ("Fase Groove Arranger update check") {}

Checker::~Checker()
{
    stopThread (18000);
}

void Checker::start (bool manual)
{
    if (isThreadRunning())
        return;
    manualCheck = manual;
    startThread (juce::Thread::Priority::low);
}

void Checker::run()
{
    Result result;
    const bool manual = manualCheck;

    [&]
    {
        juce::String json;
        if (! fetchText (*this, latestReleaseApi, json))
            return;

        const auto release = juce::JSON::parse (json);
        const auto tag = release.getProperty ("tag_name", {}).toString();
        if (release.getProperty("message",{}).toString()=="Not Found"){result.status=Result::Status::noRelease;return;}
        if (tag.isEmpty() || (bool) release.getProperty ("draft", false) || (bool) release.getProperty ("prerelease", false))
            return;

        result.version = tag.trimCharactersAtStart ("vV");
        result.kind = installKind();
        if (! isNewer (tag, JUCE_APPLICATION_VERSION_STRING))
        {
            result.status = Result::Status::upToDate;
            return;
        }
        if (! manual && Localisation::setting ("skippedVersion") == result.version)
        {
            result.status = Result::Status::upToDate;
            return;
        }

        const auto assetUrl=testing::releaseAsset(release,result.kind,result.digest);
        if (assetUrl.isEmpty())
            return;

        // What's new: the changelog in the UI language when there is one, else the release notes.
        if (Localisation::current() == Localisation::Language::italian)
        {
            juce::String changelog;
            if (fetchText (*this, rawBase + tag + "/CHANGELOG.it.md", changelog))
                result.notes = changelogSection (changelog, tag);
        }
        if (result.notes.isEmpty())
            result.notes = plain (release.getProperty ("body", {}).toString());

        const auto folder = downloadsFolder();
        folder.createDirectory();
        auto file = folder.getChildFile (juce::File::createLegalFileName (juce::String (assetName (result.kind))
                                             .replace ("FaseGrooveArranger-", "FaseGrooveArranger-" + result.version + "-")));
        if (! testing::verifyDownload(file,result.digest))
            if (! fetchFile (*this, assetUrl, file) || !testing::verifyDownload(file,result.digest))
            {file.deleteFile();return;}

        result.file = file;

        // A portable copy: unpack now, so INSTALL NOW only has to swap the files.
        if (result.kind == InstallKind::portable)
        {
            const auto staged = folder.getChildFile (result.version);
            if (unpack (file, staged))
                result.staged = staged;
            else
                result.kind = InstallKind::manual;
        }
        result.status = Result::Status::ready;
    }();

    if (threadShouldExit())
        return;

    juce::WeakReference<Checker> self (this);
    juce::MessageManager::callAsync ([self, result, manual]
    {
        if (self != nullptr && self->onResult != nullptr)
            self->onResult (result, manual);
    });
}
}

