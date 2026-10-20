// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "Localisation.h"
namespace {
class ScaledContent:public juce::Component {
public:
    ScaledContent(){addAndMakeVisible(main);setSize(1440,980);}
    void resized()override{float scale=getWidth()/1440.f;main.setTransform(juce::AffineTransform::scale(scale));main.setBounds(0,0,1440,juce::roundToInt(getHeight()/scale));}
    fga::MainComponent main;
};
class Window:public juce::DocumentWindow {
public:
    Window():DocumentWindow(juce::String::fromUTF8("Fase Groove Arranger · FS Pitch"),juce::Colour(0xff0d0f13),DocumentWindow::allButtons){setUsingNativeTitleBar(true);setIcon(juce::ImageCache::getFromMemory(BinaryData::arranger_png,BinaryData::arranger_pngSize));content=new ScaledContent;setContentOwned(content,true);setResizable(true,false);setResizeLimits(1008,686,2880,1960);auto area=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;centreWithSize(std::min(1440,area.getWidth()-60),std::min(980,area.getHeight()-70));setVisible(true);
        content->main.onScaleChosen=[this](float scale){
            auto screen=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            int width=juce::jlimit(1008,std::max(1008,screen.getWidth()-60),juce::roundToInt(1440*scale));
            int height=juce::jlimit(686,std::max(686,screen.getHeight()-70),juce::roundToInt(980*scale));
            centreWithSize(width,height);Localisation::setSetting("uiScale",width/1440.f);
        };
        const float saved=Localisation::setting("uiScale","1").getFloatValue();if(saved>0&&saved!=1.f)content->main.onScaleChosen(saved);
    }
    void closeButtonPressed()override{if(content->main.requestClose())juce::JUCEApplication::getInstance()->quit();}
    ScaledContent* content=nullptr;
};
class Application:public juce::JUCEApplication {
public:
    const juce::String getApplicationName()override{return "Fase Groove Arranger";}
    const juce::String getApplicationVersion()override{return JUCE_APPLICATION_VERSION_STRING;}
    bool moreThanOneInstanceAllowed()override{return false;}
    void initialise(const juce::String&)override{
        juce::LookAndFeel::setDefaultLookAndFeel(&theme);
        auto args=getCommandLineParameterArray();
        if(args.size()==3&&args[0]=="--host-smoke-test"){
            window=std::make_unique<Window>();auto& audio=window->content->main.audio;
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
                folder.getChildFile("host.txt").replaceWithText("PASS native VST3 load, MIDI/playhead, editor lifecycle and project state restore");
                setApplicationReturnValue(0);quit();
            });return;
        }
        if(args.size()==2&&args[0]=="--smoke-test"){
            window=std::make_unique<Window>();
            const auto folder=juce::File(args[1]);folder.createDirectory();
            auto image=window->content->main.createComponentSnapshot(window->content->main.getLocalBounds());
            if(auto stream=folder.getChildFile("interface.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(image,*stream);}
            auto& audio=window->content->main.audio;
            auto p=fga::newProject(1);p.sections={{fga::Section::drop,2,"Drop"}};p.normalise();fga::generate(p);
            bool ok=window->content->main.writeUiEvidence(folder);auto run=[&](juce::String name,int track,bool mp3){auto r=audio.exportAudio(p,folder.getChildFile(name),track,mp3,[]{return false;},[](double){});if(r.failed()){folder.getChildFile("error.txt").appendText(r.getErrorMessage()+"\n");ok=false;}};
            run("mix.wav",-1,false);run("mix.mp3",-1,true);
            for(int ti=0;ti<17;++ti)run("stem-"+juce::String(ti)+".wav",ti,false);
            const auto guitarMidi=fga::midiFile(p,3);p.tracks[3].engine.laneVolume.fill(0);run("guitar-volume-zero.wav",3,false);ok=ok&&fga::midiFile(p,3)==guitarMidi;
            p.tracks[3].engine.laneVolume.fill(1);p.tracks[3].engine.laneMute.fill(true);run("guitar-lanes-muted.wav",3,false);ok=ok&&fga::midiFile(p,3)==guitarMidi;p.tracks[3].engine.laneMute.fill(false);
            p.loop=true;p.loopStart=0;p.loopEnd=4;audio.publish(p);audio.seek(3.999);audio.play(true);
            juce::AudioBuffer<float> live(2,512);juce::AudioIODeviceCallbackContext context;
            audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            bool transport=audio.position()>=0&&audio.position()<4&&audio.isPlaying();
            audio.play(false);audio.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
            transport=transport&&live.getMagnitude(0,512)==0;
            folder.getChildFile("transport.txt").replaceWithText(transport?"PASS seek, note chase, loop boundary and pause silence":"FAIL transport");ok=ok&&transport;
            for(auto& t:p.tracks)t.mute=true;
            run("silence.wav",-1,false);
            auto bytes=fga::midiFile(p);folder.getChildFile("song.mid").replaceWithData(bytes.data(),bytes.size());
            setApplicationReturnValue(ok?0:1);quit();return;
        }
        window=std::make_unique<Window>();
    }
    void shutdown()override{window.reset();juce::LookAndFeel::setDefaultLookAndFeel(nullptr);}
    void systemRequestedQuit()override{if(window)window->closeButtonPressed();}
private:FamilyLookAndFeel theme;std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Application)
