// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
class MainComponent::InstrumentWindow : public juce::DocumentWindow {
public:
    struct Body : juce::Component {
        MainComponent& owner;
        juce::ComboBox section,advanced;
        juce::TextButton applyTrack{"APPLY TRACK"},applyClip{"APPLY CLIP"},edit{"EDIT MIDI"};
        Body(MainComponent& m):owner(m){
            for(auto* c:std::initializer_list<juce::Component*>{&section,&advanced,&applyTrack,&applyClip,&edit,&m.instrumentPort})addAndMakeVisible(c);
            section.onChange=[this]{if(section.getSelectedId()>0)owner.select(owner.selectedTrack,section.getSelectedId()-1);};
            advanced.onChange=[this]{auto names=owner.instrumentPanel.sectionNames();const int index=advanced.getSelectedItemIndex();if(index>=0&&index<names.size())owner.instrumentPort.setViewPosition(0,owner.instrumentPanel.sectionPosition(names[index]));};
            applyTrack.onClick=[this]{owner.regenerate(Scope::track,false);};applyClip.onClick=[this]{owner.regenerate(Scope::clip,false);};edit.onClick=[this]{owner.showEditor();};
            section.setTooltip(tr("Choose the song section to edit."));advanced.setTooltip(tr("Jump to the instrument controls."));
            applyTrack.setTooltip(controlTip("APPLY TRACK"));applyClip.setTooltip(controlTip("APPLY CLIP"));edit.setTooltip(controlTip("EDIT MIDI"));edit.getProperties().set("fgaIconOnly",true);edit.setTitle(tr("EDIT MIDI"));
        }
        void refresh(){
            section.clear(juce::dontSendNotification);for(const auto& part:owner.project.sections)section.addItem(utf(part.name),section.getNumItems()+1);section.setSelectedId(owner.selectedSection+1,juce::dontSendNotification);
            advanced.clear(juce::dontSendNotification);for(const auto& name:owner.instrumentPanel.sectionNames())advanced.addItem(tr(name),advanced.getNumItems()+1);advanced.setTextWhenNothingSelected(tr("ADVANCED"));
            applyTrack.setEnabled(hasGenerator(owner.project.tracks[(size_t)owner.selectedTrack]));applyClip.setEnabled(hasGenerator(owner.project.tracks[(size_t)owner.selectedTrack]));translateControls(*this);
        }
        void paint(juce::Graphics& g)override{g.fillAll(Daw::surface);g.setColour(Daw::line);g.drawHorizontalLine(43,0,(float)getWidth());}
        void resized()override{const int unit=std::max(100,(getWidth()-32)/5);section.setBounds(8,8,unit,28);advanced.setBounds(16+unit,8,unit,28);applyTrack.setBounds(24+2*unit,8,unit-8,28);applyClip.setBounds(24+3*unit,8,unit-8,28);edit.setBounds(24+4*unit,8,unit-8,28);owner.instrumentPort.setBounds(0,44,getWidth(),std::max(0,getHeight()-44));}
    };
    MainComponent& owner;Body* body;
    InstrumentWindow(MainComponent& m):DocumentWindow(tr("Instrument panel"),Daw::surface,DocumentWindow::closeButton),owner(m),body(new Body(m)){
        setLookAndFeel(&m.theme);setUsingNativeTitleBar(true);setAlwaysOnTop(true);setContentOwned(body,false);
        setResizable(true,false);setResizeLimits(780,420,2880,1960);centreWithSize(1200,760);
    }
    ~InstrumentWindow()override{clearContentComponent();setLookAndFeel(nullptr);}
    void refresh(){body->refresh();setName(tr("Instrument panel")+" / "+utf(owner.project.tracks[(size_t)owner.selectedTrack].name));}
    // Each instrument panel reports its own height; the window follows it instead of
    // leaving a large dead area under short panels. The centre is kept, otherwise the
    // window grows downwards and looks off-centre, and the height is clamped to the
    // display so a tall panel cannot fall off the screen.
    void fitToContent(){
        const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
        const int maxHeight=display?std::max(420,display->userArea.getHeight()-40):900;
        const int content=44+owner.instrumentPanel.getHeight()+2;
        const int height=juce::jlimit(420,maxHeight,content);
        if(height==lastFit)return;lastFit=height;
        const auto centre=getBounds().getCentre();
        setSize(getWidth(),height);
        setCentrePosition(centre);
    }
private:
    int lastFit=-1;
public:
    void closeButtonPressed()override{owner.instrumentPanel.flushRender();owner.dismissAuxiliaryWindow(*this);}
    bool keyPressed(const juce::KeyPress& key)override{return owner.keyPressed(key);}
};

// Every plugin editor sits under a bar that says where it works, in the destination's colour:
// "PIANO → INSERT 2 → COMPRESSOR", "RETURN A → REVERB · SHARED BY 5 TRACKS", with Show track.
class EditorFrame final : public juce::Component, private juce::ComponentListener {
public:
    static constexpr int barHeight=30;
    EditorFrame(juce::Component* body,juce::String path,juce::Colour tone,std::function<void()> show):editor(body),text(std::move(path)),colour(tone),onShow(std::move(show)){
        addAndMakeVisible(editor.get());editor->addComponentListener(this);
        showTrack.setButtonText(tr("Show track"));showTrack.onClick=[this]{if(onShow)onShow();};showTrack.setVisible((bool)onShow);addAndMakeVisible(showTrack);
        setSize(editor->getWidth(),editor->getHeight()+barHeight);
    }
    ~EditorFrame()override{if(editor)editor->removeComponentListener(this);}
    juce::Component* body()const{return editor.get();}
    const juce::String& path()const{return text;}
    void setPath(juce::String path){text=std::move(path);repaint();}
    void paint(juce::Graphics& g)override{auto bar=getLocalBounds().removeFromTop(barHeight);g.setColour(Daw::well);g.fillRect(bar);g.setColour(colour);g.fillRect(bar.removeFromLeft(5));g.fillRect(0,barHeight-2,getWidth(),2);
        g.setColour(Daw::text);g.setFont(Daw::font(12.5f,true));g.drawFittedText(text,bar.reduced(10,0).withTrimmedRight(showTrack.isVisible()?110:0),juce::Justification::centredLeft,1,.8f);}
    void resized()override{auto area=getLocalBounds();auto bar=area.removeFromTop(barHeight);showTrack.setBounds(bar.removeFromRight(104).reduced(4,4));if(editor&&!resizing){const juce::ScopedValueSetter<bool> guard(resizing,true);editor->setBounds(area);}}
private:
    void componentMovedOrResized(juce::Component&,bool,bool wasResized)override{if(wasResized&&!resizing){const juce::ScopedValueSetter<bool> guard(resizing,true);setSize(editor->getWidth(),editor->getHeight()+barHeight);}}
    std::unique_ptr<juce::Component> editor;juce::String text;juce::Colour colour;std::function<void()> onShow;juce::TextButton showTrack;bool resizing=false;
};
class MainComponent::PluginWindow : public juce::DocumentWindow,private juce::AudioProcessorListener {
public:
    MainComponent& owner;juce::AudioPluginInstance& processor;
    std::uint64_t session;int slot,track;
    PluginWindow(MainComponent& m,juce::AudioPluginInstance& p,std::uint64_t id,int effectSlot,int trackIndex):DocumentWindow(p.getName(),Palette::panel,DocumentWindow::closeButton),owner(m),processor(p),session(id),slot(effectSlot),track(trackIndex){const juce::ScopedLock processorLock(p.getCallbackLock());setUsingNativeTitleBar(true);setAlwaysOnTop(true);auto* editor=p.createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(p);const bool resizable=editor->isResizable();juce::Colour tone;const auto path=m.effectPath(trackIndex,effectSlot,tone);juce::Component::SafePointer<MainComponent> safe(&m);std::function<void()> show;if(trackIndex<maxTracks)show=[safe,owned=id]{if(!safe)return;const int ti=safe->trackForSession(owned);if(ti>=0)safe->select(ti,std::max(0,safe->selectedSection));};frame=new EditorFrame(editor,path,tone,show);setContentOwned(frame,true);processor.addListener(this);editor->addMouseListener(this,true);markState();setResizable(resizable,false);setDraggable(false);setSize(frame->getWidth(),frame->getHeight());showCentred();}
    EditorFrame* frame=nullptr;
    void showCentred(){
        setVisible(true);centreWithSize(getWidth(),getHeight());
        // Cocoa can apply the native frame/content size after the first message
        // turn. Re-centre once it has settled, including on subsequent opens.
        juce::Component::SafePointer<PluginWindow> safe(this);
        juce::Timer::callAfterDelay(100,[safe]{if(safe&&safe->isVisible())safe->centreWithSize(safe->getWidth(),safe->getHeight());});
    }
    void markState()noexcept{owner.audio.invalidatePluginState(track,slot);}
    void activeWindowStatusChanged()override{
        if(isActiveWindow()){owner.lastActivePluginSession=session;owner.lastActivePluginSlot=slot;}else markState();
        juce::DocumentWindow::activeWindowStatusChanged();
    }
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{markState();}
    void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&)override{markState();}
    void mouseUp(const juce::MouseEvent& e)override{markState();juce::DocumentWindow::mouseUp(e);}
    ~PluginWindow()override{
        {const juce::ScopedLock processorLock(processor.getCallbackLock());processor.removeListener(this);}
        if(frame&&frame->body())frame->body()->removeMouseListener(this);
        clearContentComponent();markState();
    }
    void closeButtonPressed()override{markState();owner.dismissAuxiliaryWindow(*this);}
};

class MainComponent::AutosaveJob : public juce::Thread {
public:
    AutosaveJob(Project p,juce::File file,juce::File recovery,juce::String origin,std::function<void(juce::Result)> finished):Thread("Autosave"),snapshot(std::move(p)),target(file),recoveryDir(recovery),originalPath(std::move(origin)),complete(std::move(finished)){}
    ~AutosaveJob()override{signalThreadShouldExit();stopThread(-1);}
    void run()override{if(threadShouldExit())return;auto result=saveProject(snapshot,target);if(result.wasOk()&&recoveryDir!=juce::File())result=storeRecoverySnapshot(snapshot,originalPath,recoveryDir);if(!threadShouldExit())juce::MessageManager::callAsync([callback=complete,result]{callback(result);});}
private:Project snapshot;juce::File target,recoveryDir;juce::String originalPath;std::function<void(juce::Result)> complete;
};

