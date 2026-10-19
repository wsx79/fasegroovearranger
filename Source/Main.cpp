// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
namespace {
class ScaledContent:public juce::Component {
public:
    ScaledContent(){addAndMakeVisible(main);setSize(1440,980);}
    void resized()override{float scale=getWidth()/1440.f;main.setTransform(juce::AffineTransform::scale(scale));main.setBounds(0,0,1440,juce::roundToInt(getHeight()/scale));}
    fga::MainComponent main;
};
class Window:public juce::DocumentWindow {
public:
    Window():DocumentWindow("Fase Groove Arranger · FS Pitch",juce::Colour(0xff0d0f13),DocumentWindow::allButtons){setUsingNativeTitleBar(true);content=new ScaledContent;setContentOwned(content,true);setResizable(true,false);setResizeLimits(1008,686,2880,1960);auto area=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;centreWithSize(std::min(1440,area.getWidth()-60),std::min(980,area.getHeight()-70));setVisible(true);}
    void closeButtonPressed()override{if(content->main.requestClose())juce::JUCEApplication::getInstance()->quit();}
    ScaledContent* content=nullptr;
};
class Application:public juce::JUCEApplication {
public:
    const juce::String getApplicationName()override{return "Fase Groove Arranger";}
    const juce::String getApplicationVersion()override{return "0.1.0";}
    bool moreThanOneInstanceAllowed()override{return false;}
    void initialise(const juce::String&)override{
        auto args=getCommandLineParameterArray();
        if(args.size()==2&&args[0]=="--smoke-test"){
            window=std::make_unique<Window>();
            const auto folder=juce::File(args[1]);folder.createDirectory();
            auto image=window->content->main.createComponentSnapshot(window->content->main.getLocalBounds());
            if(auto stream=folder.getChildFile("interface.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(image,*stream);}
            auto& audio=window->content->main.audio;
            auto p=fga::newProject(1);p.sections={{fga::Section::drop,2,"Drop"}};p.normalise();fga::generate(p);
            bool ok=true;auto run=[&](juce::String name,int track,bool mp3){auto r=audio.exportAudio(p,folder.getChildFile(name),track,mp3,[]{return false;},[](double){});if(r.failed()){folder.getChildFile("error.txt").appendText(r.getErrorMessage()+"\n");ok=false;}};
            run("mix.wav",-1,false);run("mix.mp3",-1,true);
            for(int ti=0;ti<17;++ti)run("stem-"+juce::String(ti)+".wav",ti,false);
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
    void shutdown()override{window.reset();}
    void systemRequestedQuit()override{if(window)window->closeButtonPressed();}
private:std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Application)
