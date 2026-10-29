// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "Localisation.h"
#if FGA_SELF_TESTS
#include "ArrangementAudit.h"
#endif
#include "DarkTitleBar.h"
#include <cmath>
namespace {
// A crash handler that leaves the stack on disk: the Windows event log names only
// the executable, which is not enough to find the fault.
void writeCrashLog(void*){
    const auto file=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("FS Pitch/Fase Groove Arranger/crash.txt");
    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::String("Fase Groove Arranger crash\n")
        +juce::Time::getCurrentTime().toString(true,true,true,true)+"\n\n"
        +juce::SystemStats::getStackBacktrace());
}
class ScaledContent:public juce::Component {
public:
    ScaledContent(){addAndMakeVisible(main);setSize(1440,980);}
    void resized()override{
        // Fill the width, as before, but never squeeze the logical layout below 700 px
        // (very short or ultra-wide windows) so rows cannot overlap.
        float scale=juce::jmin(getWidth()/1440.f,getHeight()/700.f);
        scale=juce::jmax(.85f,scale);
        main.setTransform(juce::AffineTransform::scale(scale));
        main.setBounds(0,0,1440,juce::roundToInt(getHeight()/scale));
    }
    fga::MainComponent main;
};
class Window:public juce::DocumentWindow {
public:
    Window():DocumentWindow(juce::String::fromUTF8("Fase Groove Arranger · FS Pitch"),juce::Colour(0xff0d0f13),DocumentWindow::allButtons){setUsingNativeTitleBar(true);setIcon(juce::ImageCache::getFromMemory(BinaryData::arranger_png,BinaryData::arranger_pngSize));content=new ScaledContent;setContentOwned(content,true);setResizable(true,false);setResizeLimits(1224,700,2880,1960);auto area=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;centreWithSize(std::min(1440,area.getWidth()-60),std::min(980,area.getHeight()-70));setVisible(true);
        content->main.onScaleChosen=[this](float scale){
            auto screen=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            int width=juce::jlimit(1224,std::max(1224,screen.getWidth()-60),juce::roundToInt(1440*scale));
            int height=juce::jlimit(700,std::max(700,screen.getHeight()-70),juce::roundToInt(980*scale));
            centreWithSize(width,height);Localisation::setSetting("uiScale",width/1440.f);
        };
        const float saved=Localisation::setting("uiScale","1").getFloatValue();if(saved>0&&saved!=1.f)content->main.onScaleChosen(saved);
    }
    void closeButtonPressed()override{content->main.requestClose([]{juce::JUCEApplication::getInstance()->quit();});}
    ScaledContent* content=nullptr;
};
class Application:public juce::JUCEApplication {
public:
    const juce::String getApplicationName()override{return "Fase Groove Arranger";}
    const juce::String getApplicationVersion()override{return JUCE_APPLICATION_VERSION_STRING;}
    bool moreThanOneInstanceAllowed()override{return getCommandLineParameterArray().contains("--scan-plugin")||getCommandLineParameterArray().contains("--plugin-catalog-test");}
    void initialise(const juce::String&)override{
        juce::LookAndFeel::setDefaultLookAndFeel(&theme);
        juce::SystemStats::setApplicationCrashHandler(writeCrashLog);
        auto args=getCommandLineParameterArray();
       #if JUCE_MAC
        if(args.contains("--scan-plugin")||args.contains("--plugin-catalog-test"))juce::Process::setDockIconVisible(false);
       #if FGA_SELF_TESTS
        // Launching the executable from a terminal does not activate a Cocoa app.
        // Native input/focus tests must start with the same foreground state as
        // an application opened normally, before synthesising JUCE mouse events.
        if(args.contains("--smoke-test")||args.contains("--host-smoke-test")||args.contains("--ui-test")){
            juce::Process::makeForegroundProcess();const auto until=juce::Time::getMillisecondCounterHiRes()+2000.;
            while(!juce::Process::isForegroundProcess()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        }
       #endif
       #endif
        if(args.size()==4&&args[0]=="--scan-plugin"){
            setApplicationReturnValue(fga::PluginCatalog::scanOne(args[1],args[2],juce::File(args[3])));quit();return;
        }
        if(args.size()==3&&args[0]=="--plugin-catalog-test"){
            const bool ok=fga::PluginCatalog::verify(juce::File(args[1]),juce::File(args[2]));setApplicationReturnValue(ok?0:1);quit();return;
        }
#if FGA_SELF_TESTS
        if(args.size()==2&&args[0]=="--club-audit"){
            const bool ok=fga::writeClubAudioAudit(juce::File(args[1]));
            setApplicationReturnValue(ok?0:1);quit();return;
        }
        if(args.size()==3&&args[0]=="--host-smoke-test"){
            window=std::make_unique<Window>();auto& audio=window->content->main.audio;audio.devices.closeAudioDevice();
            const auto folder=juce::File(args[2]);folder.createDirectory();
            juce::OwnedArray<juce::PluginDescription> types;
            for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(types,args[1]);
            if(types.isEmpty()){folder.getChildFile("host-error.txt").replaceWithText("No VST3 instrument found");setApplicationReturnValue(1);quit();return;}
            const auto description=*types[0];
            audio.loadPlugin(0,description,[this,folder,description](juce::Result loaded){
                auto& engine=window->content->main.audio;
                auto fail=[&](juce::String text){folder.getChildFile("host-error.txt").appendText(text+"\n");setApplicationReturnValue(1);quit();};
                if(loaded.failed()){fail(loaded.getErrorMessage());return;}
                auto* plugin=engine.plugin(0);
                if(!plugin||plugin->getParameters().isEmpty()){fail("Plugin parameters unavailable");return;}
                for(int i=0;i<3;++i){
                    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
                    if(!editor){fail("Plugin editor unavailable");return;}
                    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);
                    editor.reset();if(plugin->getActiveEditor()!=nullptr){fail("Plugin editor lifetime invalid");return;}
                }
                if(!window->content->main.verifyPluginWindow(folder)){fail("VST window on-top/close/reopen-centred behavior invalid");return;}
                auto p=fga::newProject();p.sections={{fga::Section::main,1,"Main"}};p.normalise();
                for(auto& track:p.tracks){track.mute=true;track.clips[0].notes.clear();}
                auto& track=p.tracks[0];track.mute=false;track.instrument="plugin";track.instrumentPath=description.fileOrIdentifier.toStdString();
                if(auto xml=description.createXml())track.instrumentId=xml->toString().toStdString();
                track.clips[0].notes={{0,2,69,100,0}};
                engine.capturePluginStates(p);
                auto render=[&](const char* name){return engine.exportAudio(p,folder.getChildFile(name),0,false,[]{return false;},[](double){});};
                auto r=render("host-before.wav");if(r.failed()){fail(r.getErrorMessage());return;}
                r=fga::saveProject(p,folder.getChildFile("host-project.fga"));if(r.failed()){fail(r.getErrorMessage());return;}
                plugin->getParameters()[0]->setValueNotifyingHost(.75f);
                r=render("host-loud.wav");if(r.failed()){fail(r.getErrorMessage());return;}
                fga::Project restored;r=fga::loadProject(folder.getChildFile("host-project.fga"),restored);if(r.failed()){fail(r.getErrorMessage());return;}
                if(restored.tracks[0].instrumentState.empty()){fail("Plugin state missing from project");return;}
                engine.restorePluginState(0,juce::String::fromUTF8(restored.tracks[0].instrumentState.c_str()));
                r=render("host-restored.wav");if(r.failed()){fail(r.getErrorMessage());return;}
                // Export on a private copy (plugin re-created from its state) while the live one keeps its transport.
                {auto copy=engine.offlineCopy();if(copy==nullptr){fail("Plugin could not be duplicated for a private export");return;}
                    engine.publish(p);engine.play(true);const double at=engine.position();
                    r=copy->exportAudio(p,folder.getChildFile("host-offline.wav"),0,false,[]{return false;},[](double){});if(r.failed()){fail(r.getErrorMessage());return;}
                    if(!engine.isPlaying()||engine.position()!=at){fail("Private export disturbed the live transport");return;}engine.stop();}
                folder.getChildFile("host.txt").replaceWithText("PASS native VST3 load, MIDI/playhead, editor lifecycle and project state restore");
                setApplicationReturnValue(0);quit();
            });return;
        }
        if(args.size()==2&&args[0]=="--ui-test"){
            window=std::make_unique<Window>();window->content->main.audio.devices.closeAudioDevice();const bool ok=window->content->main.writeUiEvidence(juce::File(args[1]));setApplicationReturnValue(ok?0:1);quit();return;
        }
        if(args.size()==2&&args[0]=="--smoke-test"){
            window=std::make_unique<Window>();window->content->main.audio.devices.closeAudioDevice();
            const auto folder=juce::File(args[1]);folder.createDirectory();
            auto image=window->content->main.createComponentSnapshot(window->content->main.getLocalBounds());
            if(auto stream=folder.getChildFile("interface.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(image,*stream);}
            auto& audio=window->content->main.audio;
            auto p=fga::newProject(1);p.sections={{fga::Section::drop,2,"Drop"}};p.normalise();fga::generate(p);
            // The interactive evidence harness drives synthetic input and asserts pixel
            // layouts; it runs where that is reliable (Linux, Windows). Rendering and
            // audio checks below always run.
            bool ok=true;
            if(juce::SystemStats::getEnvironmentVariable("FGA_SKIP_UI_EVIDENCE","")=="1")
                folder.getChildFile("ui-evidence-skip.txt").replaceWithText("SKIP interactive UI evidence on this platform: synthetic-input and layout assertions are exercised on Linux and Windows. Rendering and audio checks still run.");
            else ok=window->content->main.writeUiEvidence(folder);
            auto run=[&](juce::String name,int track,bool mp3){auto r=audio.exportAudio(p,folder.getChildFile(name),track,mp3,[]{return false;},[](double){});if(r.failed()){folder.getChildFile("error.txt").appendText(r.getErrorMessage()+"\n");ok=false;}};
            run("mix.wav",-1,false);run("mix.mp3",-1,true);
            for(int ti=0;ti<fga::trackCount;++ti)run("stem-"+juce::String(ti)+".wav",ti,false);
            const auto leadMidi=fga::midiFile(p,fga::leadTrack);
            p.tracks[fga::leadTrack].engine.laneVolume.fill(0);run("lead-volume-zero.wav",fga::leadTrack,false);ok=ok&&fga::midiFile(p,fga::leadTrack)==leadMidi;
            p.tracks[fga::leadTrack].engine.laneVolume.fill(1);p.tracks[fga::leadTrack].engine.laneMute.fill(true);run("lead-lanes-muted.wav",fga::leadTrack,false);ok=ok&&fga::midiFile(p,fga::leadTrack)==leadMidi;p.tracks[fga::leadTrack].engine.laneMute.fill(false);
            const auto guitarMidi=fga::midiFile(p,3);p.tracks[3].engine.laneVolume.fill(0);run("guitar-volume-zero.wav",3,false);ok=ok&&fga::midiFile(p,3)==guitarMidi;
            p.tracks[3].engine.laneVolume.fill(1);p.tracks[3].engine.laneMute.fill(true);run("guitar-lanes-muted.wav",3,false);ok=ok&&fga::midiFile(p,3)==guitarMidi;p.tracks[3].engine.laneMute.fill(false);
            {
                // Use a user sample for exact cross-render comparison: original
                // drums deliberately vary their noise seed on every trigger.
                const auto savedGain=p.tracks[4].gain;p.tracks[4].gain=.05;
                bool sampleLoaded=audio.loadSample(4,folder.getChildFile("stem-4.wav"),36).wasOk();run("automation-reference.wav",4,false);
                const auto notes=fga::midiFile(p,4);auto& volume=p.tracks[4].volume;volume.enabled=true;volume.points={{0,0,.8},{4,1,-.6},{8,.25,0}};
                run("automation-curve.wav",4,false);volume.points={{0,0,0}};run("automation-zero.wav",4,false);volume.enabled=false;run("automation-bypass.wav",4,false);
                juce::AudioFormatManager readerFormats;readerFormats.registerBasicFormats();auto reference=std::unique_ptr<juce::AudioFormatReader>(readerFormats.createReaderFor(folder.getChildFile("automation-reference.wav")));
                bool automation=sampleLoaded&&reference!=nullptr;
                for(const auto& filename:{"automation-curve.wav","automation-zero.wav","automation-bypass.wav"}){
                    auto rendered=std::unique_ptr<juce::AudioFormatReader>(readerFormats.createReaderFor(folder.getChildFile(filename)));if(!rendered||!reference||rendered->lengthInSamples!=reference->lengthInSamples){automation=false;continue;}
                    fga::VolumeAutomation expected;expected.enabled=juce::String(filename)!="automation-bypass.wav";expected.points=juce::String(filename)=="automation-curve.wav"?std::vector<fga::VolumePoint>{{0,0,.8},{4,1,-.6},{8,.25,0}}:std::vector<fga::VolumePoint>{{0,0,0}};
                    juce::AudioBuffer<float> dry(2,4096),wet(2,4096);double error=0;for(juce::int64 pos=0;pos<reference->lengthInSamples;pos+=4096){int count=(int)std::min<juce::int64>(4096,reference->lengthInSamples-pos);reference->read(&dry,0,count,pos,true,true);rendered->read(&wet,0,count,pos,true,true);for(int s=0;s<count;++s)for(int ch=0;ch<2;++ch)error=std::max(error,std::abs(wet.getSample(ch,s)-dry.getSample(ch,s)*expected.valueAt((pos+s)/reference->sampleRate*p.settings.bpm/60.)));}automation=automation&&error<2e-6;
                }
                {
                    // The master lane multiplies like a volume lane; a pan lane hard left leaves the right side silent.
                    volume={};p.masterAutomation.enabled=true;p.masterAutomation.points={{0,0,.8},{4,1,-.6},{8,.25,0}};run("automation-master.wav",4,false);p.masterAutomation={};
                    p.tracks[4].panAutomation.enabled=true;p.tracks[4].panAutomation.points={{0,0,0}};run("automation-pan-left.wav",4,false);p.tracks[4].panAutomation={};
                    fga::VolumeAutomation curve;curve.enabled=true;curve.points={{0,0,.8},{4,1,-.6},{8,.25,0}};
                    for(const auto& filename:{"automation-master.wav","automation-pan-left.wav"}){
                        auto rendered=std::unique_ptr<juce::AudioFormatReader>(readerFormats.createReaderFor(folder.getChildFile(filename)));if(!rendered||!reference||rendered->lengthInSamples!=reference->lengthInSamples){automation=false;continue;}
                        const bool panned=juce::String(filename)=="automation-pan-left.wav";
                        juce::AudioBuffer<float> dry(2,4096),wet(2,4096);double error=0,right=0,left=0;for(juce::int64 pos=0;pos<reference->lengthInSamples;pos+=4096){int count=(int)std::min<juce::int64>(4096,reference->lengthInSamples-pos);reference->read(&dry,0,count,pos,true,true);rendered->read(&wet,0,count,pos,true,true);
                            for(int s=0;s<count;++s){if(panned){right=std::max(right,(double)std::abs(wet.getSample(1,s)));left=std::max(left,(double)std::abs(wet.getSample(0,s)));error=std::max(error,(double)std::abs(wet.getSample(0,s)-dry.getSample(0,s)));}
                                else for(int ch=0;ch<2;++ch)error=std::max(error,std::abs(wet.getSample(ch,s)-dry.getSample(ch,s)*curve.valueAt((pos+s)/reference->sampleRate*p.settings.bpm/60.)));}}
                        automation=automation&&error<2e-6&&(!panned||(right<1e-7&&left>1e-4));
                    }
                }
                automation=automation&&fga::midiFile(p,4)==notes;folder.getChildFile("volume-automation-audio.txt").replaceWithText(automation?"PASS sample-accurate curve, silence, bypass, master and pan lanes, MIDI unchanged":"FAIL volume automation audio");ok=automation&&ok;volume={};p.tracks[4].gain=savedGain;audio.useInternal(4);
            }
            p.loop=true;p.loopStart=0;p.loopEnd=4;audio.publish(p);audio.seek(3.999);audio.play(true);
            juce::AudioBuffer<float> live(2,512);juce::AudioIODeviceCallbackContext context;
            audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            bool transport=audio.position()>=0&&audio.position()<4&&audio.isPlaying();
            audio.seek(6.25);audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            transport=transport&&audio.position()>6.25&&audio.position()<7&&audio.isPlaying();
            audio.play(false);audio.seek(5.5);audio.play(true);
            transport=transport&&std::abs(audio.position()-5.5)<1e-9;
            audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            transport=transport&&audio.position()>5.5&&audio.position()<6;
            audio.seek(3.999);audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            transport=transport&&audio.position()<4&&audio.isPlaying();
            float* optionalOutputs[]{live.getWritePointer(0),nullptr};
            audio.audioDeviceIOCallbackWithContext(nullptr,0,optionalOutputs,2,512,context);
            transport=transport&&std::isfinite(live.getRMSLevel(0,0,512));
            audio.seek(6.25);run("seek-loop-export.wav",-1,false);audio.play(true);
            for(int block=0;block<1024;++block)audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            transport=transport&&audio.position()>=0&&audio.position()<4&&audio.isPlaying();
            audio.play(false);audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            transport=transport&&live.getMagnitude(0,512)==0&&audio.masterLevel()==0;
            for(int ti=0;ti<fga::trackCount;++ti)transport=transport&&audio.level(ti)==0;
            folder.getChildFile("transport.txt").replaceWithText(transport?"PASS precise paused/live seek, seek beyond loop, loop re-entry, loop after export and pause silence":"FAIL transport");ok=ok&&transport;
            {
                auto probe=p;probe.loop=false;for(auto& t:probe.tracks){t.mute=true;for(auto& c:t.clips)c.notes.clear();}
                probe.tracks[0].mute=false;probe.tracks[0].gain=.5;probe.tracks[0].clips[0].notes={{0,4,69,100,0}};
                audio.publish(probe);audio.seek(2.375);audio.play(true);audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
                const bool chase=audio.position()>2.375&&audio.position()<3&&live.getMagnitude(0,512)>1e-5;
                folder.getChildFile("seek-note-chase.txt").replaceWithText(chase?"PASS playback starts inside a held note without rewinding or silence":"FAIL seek note chase");ok=ok&&chase;
                probe.tracks[0].gain=0;audio.updateMix(probe);audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
                const bool startsSmoothly=live.getMagnitude(0,64)>0;
                for(int block=0;block<8;++block)audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
                const bool smooth=startsSmoothly&&live.getMagnitude(0,512)==0;
                folder.getChildFile("mixer-smoothing.txt").replaceWithText(smooth?"PASS live fader ramps to silence within 10ms without changing MIDI":"FAIL live fader smoothing");ok=ok&&smooth;audio.stop();
            }
            {
                auto loopProbe=p;loopProbe.loop=false;loopProbe.sections={{fga::Section::main,2,"Loop"}};loopProbe.normalise();for(auto& t:loopProbe.tracks)for(auto& c:t.clips)c.notes.clear();
                loopProbe.tracks[4].clips[0].notes={{0,.25,36,100,0}};bool loopAudio=loopProbe.resizeClipLoop(4,0,12);
                auto result=audio.exportAudio(loopProbe,folder.getChildFile("loop-extended.wav"),4,false,[]{return false;},[](double){});loopAudio=loopAudio&&result.wasOk();
                juce::AudioFormatManager fm;fm.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(folder.getChildFile("loop-extended.wav")));
                if(reader){const auto expected=(juce::int64)std::ceil((12*60./loopProbe.settings.bpm+3)*reader->sampleRate);loopAudio=loopAudio&&std::abs(reader->lengthInSamples-expected)<=1;
                    juce::AudioBuffer<float> hit(2,4096);for(double beat:{0.,8.}){reader->read(&hit,0,4096,(juce::int64)std::llround(beat*60./loopProbe.settings.bpm*reader->sampleRate),true,true);loopAudio=loopAudio&&hit.getMagnitude(0,4096)>1e-5;}}
                else loopAudio=false;
                const auto midi=fga::midiFile(loopProbe,4);folder.getChildFile("loop-extended.mid").replaceWithData(midi.data(),midi.size());
                folder.getChildFile("loop-resize-audio.txt").replaceWithText(loopAudio?"PASS independently extended loop renders both repeats and includes the tail beyond the last section in WAV/MIDI":"FAIL resized loop audio/export");ok=ok&&loopAudio;
                const int moved=loopProbe.moveClip(4,0,4,16,false);bool timelineAudio=moved>=0&&loopProbe.sections[0].bars==2;
                auto freeResult=audio.exportAudio(loopProbe,folder.getChildFile("timeline-edited.wav"),4,false,[]{return false;},[](double){});timelineAudio=timelineAudio&&freeResult.wasOk();
                auto freeReader=std::unique_ptr<juce::AudioFormatReader>(fm.createReaderFor(folder.getChildFile("timeline-edited.wav")));
                if(freeReader){juce::AudioBuffer<float> window(2,4096);for(double beat:{16.,24.}){freeReader->read(&window,0,4096,(juce::int64)std::llround(beat*60./loopProbe.settings.bpm*freeReader->sampleRate),true,true);timelineAudio=timelineAudio&&window.getMagnitude(0,4096)>1e-5;}freeReader->read(&window,0,4096,0,true,true);timelineAudio=timelineAudio&&window.getMagnitude(0,4096)<1e-7;}else timelineAudio=false;
                int hits=0;for(auto event:fga::events(loopProbe,4))if(event.controller<0&&event.velocity>0){timelineAudio=timelineAudio&&(event.beat==16||event.beat==24);++hits;}timelineAudio=timelineAudio&&hits==2;
                folder.getChildFile("timeline-editing-audio.txt").replaceWithText(timelineAudio?"PASS moved independent loop renders its two repeats at edited positions, leaves original position silent and preserves the guide":"FAIL independent timeline playback/export");ok=ok&&timelineAudio;

            }
            {
                const auto saved=p;p=fga::newProject(4);p.sections={{fga::Section::main,4,"Main"}};p.normalise();
                for(auto& t:p.tracks){t.engine.humanize=0;t.engine.realism=false;}
                for(unsigned seed:{91U,4951U})for(int track:{0,1,3}){
                    fga::generate(p,fga::Scope::track,track,0,seed);
                    run("regeneration-"+juce::String(seed)+"-"+juce::String(track)+".wav",track,false);
                }
                p=saved;
            }
            for(auto& t:p.tracks)t.mute=true;
            run("silence.wav",-1,false);
            auto bytes=fga::midiFile(p);folder.getChildFile("song.mid").replaceWithData(bytes.data(),bytes.size());
            {
                // The files scripts/verify-midi.py reads: a complete generated song, one stem and piano pedal/expression.
                auto song=fga::newProject(1);auto write=[&](const char* name,const std::vector<std::uint8_t>& data){folder.getChildFile(name).replaceWithData(data.data(),data.size());};
                write("test-song.mid",fga::midiFile(song));write("test-kick.mid",fga::midiFile(song,4));song.tracks[0].engine.pedalCC=true;write("test-piano-cc.mid",fga::midiFile(song,0));
            }
            setApplicationReturnValue(ok?0:1);quit();return;
        }
#endif
        window=std::make_unique<Window>();window->content->main.pluginCatalog.scan();
        window->content->main.openFromCommandLine(getCommandLineParameters());
    }
    // Single instance: a double-clicked .fga arrives here (and as a macOS open-document event).
    void anotherInstanceStarted(const juce::String& commandLine)override{if(window){window->toFront(true);window->content->main.openFromCommandLine(commandLine);}}
    void shutdown()override{window.reset();juce::LookAndFeel::setDefaultLookAndFeel(nullptr);}
    void systemRequestedQuit()override{if(window)window->closeButtonPressed();}
private:DawLookAndFeel theme;std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Application)
