// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Branding.h"
#include "MainComponent.h"
#include "StudioIO.h"
#include "AiComposerWindow.h"
#include "AiComposer.h"
#include "UiText.h"
#include "ExportSupport.h"
#include "DarkTitleBar.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include <BinaryData.h>
#include <cmath>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1
 #endif
 #include <windows.h>
 #undef min
 #undef max
 #undef small
#endif
#if JUCE_MAC
 #include <CoreGraphics/CoreGraphics.h>
#endif
namespace fga {
#include "WorkspaceViewHelpers.h"
#include "NativeWindows.h"
namespace {
void settleNativeFocus(juce::Component& component){
    // Wait for native key-window notifications and deferred focus restoration.
    // Keep the exact focus condition; only the asynchronous completion is awaited.
    const auto until=juce::Time::getMillisecondCounterHiRes()+1500.;
    while(!component.hasKeyboardFocus(false)&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
}
}
bool MainComponent::verifyWindowFocus(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    if(!folder.createDirectory())return false;
    const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;
    bool ok=true;juce::String details;
    auto check=[&](const char* name,juce::DocumentWindow& window){
        window.grabKeyboardFocus();
        // Reproduce a missed mouse-up when the native tool window disappears. The
        // press is injected inside the peer, where JUCE attributes it to the window.
        auto source=juce::Desktop::getInstance().getMainMouseSource();
        if(auto* peer=window.getPeer())peer->handleMouseEvent(source.getType(),{4.f,4.f},juce::ModifierKeys::leftButtonModifier,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
        const bool pressed=source.isDragging();auto* down=source.getComponentUnderMouse();const bool owned=down&&(down==&window||window.isParentOf(down));const bool physicalDown=physicalMouseModifiers().isAnyMouseButtonDown();window.closeButtonPressed();
        const bool released=!source.isDragging();settleNativeFocus(*this);
        const bool focused=!window.isVisible()&&isEnabled()&&hasKeyboardFocus(false);
        const auto before=undoStack.size();const auto seed=project.settings.seed;
        const auto pos=getTopLevelComponent()->getLocalPoint(&songButton,songButton.getLocalBounds().getCentre()).toFloat();
        if(auto* peer=getPeer()){
            peer->handleMouseEvent(source.getType(),pos,juce::ModifierKeys::leftButtonModifier,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
            peer->handleMouseEvent(source.getType(),pos,{},juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
        }
        // Generation is disabled on purpose while a take is running, so a generated
        // seed can only be required when editing is actually available.
        const bool expectGeneration=!editingRestricted();
        const bool generated=undoStack.size()==std::min(size_t(40),before+1)&&project.settings.seed!=seed;
        const bool clicked=expectGeneration?generated:true;
        details+=juce::String(pressed&&owned&&released&&focused&&clicked?"PASS ":"FAIL ")+name+": pressed="+juce::String((int)pressed)+", owned="+juce::String((int)owned)+", physical-buttons="+juce::String((int)physicalDown)+", source-released="+juce::String((int)released)+", focus="+juce::String((int)focused)+", first-click generation="+juce::String((int)generated)+(expectGeneration?"":" (generation disabled during take)")+"\n";
        ok=pressed&&owned&&released&&focused&&clicked&&ok;if(generated)undo(false);
    };
    showArrangement();
    for(int repeat=0;repeat<3;++repeat){
        showStudio();audio.startRecording(0,0);updateEditingAvailability();check("Studio (including active take)",*studioWindow);audio.stop();updateEditingAvailability();
        showMixerRouting();check("Mixer",*mixerRoutingWindow);
        showAiComposer();check("AI",*aiWindow);
        showInstrument();check("Instrument",*instrumentWindow);
    }
    // Recovery must cancel a hidden pressed button without firing its action,
    // and must leave a gesture in a different visible window alone.
    juce::TextButton probeButton("Input recovery probe");int actions=0;probeButton.onClick=[&]{++actions;};
    juce::DocumentWindow probe("Input recovery",Daw::base,0);probe.setUsingNativeTitleBar(true);probe.setContentNonOwned(&probeButton,false);probe.centreWithSize(160,64);probe.setVisible(true);
    auto source=juce::Desktop::getInstance().getMainMouseSource();
    auto probeEvent=[&](juce::ModifierKeys mods){if(auto* peer=probe.getPeer())peer->handleMouseEvent(source.getType(),{80.f,32.f},mods,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());};
    probeEvent(juce::ModifierKeys::leftButtonModifier);const bool down=source.isDragging()&&probeButton.isDown();dismissAuxiliaryWindow(probe);
    const bool cancelled=down&&!source.isDragging()&&actions==0;ok=cancelled&&ok;
    details+=juce::String(cancelled?"PASS ":"FAIL ")+"stale release cancels hidden button without invoking its action\n";
    probe.setVisible(true);showMixerRouting();probeEvent(juce::ModifierKeys::leftButtonModifier);const bool begun=source.isDragging();mixerRoutingWindow->closeButtonPressed();
    const bool kept=source.isDragging()&&actions==0;probeEvent({});
    const bool independent=begun&&kept&&!source.isDragging();ok=independent&&ok;
    details+=juce::String(independent?"PASS ":"FAIL ")+"closing a tool preserves a gesture in another visible window (begun="+juce::String((int)begun)+", retained="+juce::String((int)kept)+")\n";
    dismissAuxiliaryWindow(probe);probe.clearContentComponent();
    showMixerRouting();setEnabled(false);mixerRoutingWindow->closeButtonPressed();
    const bool guarded=!isEnabled()&&!mixerRoutingWindow->isVisible();ok=guarded&&ok;
    details+=juce::String(guarded?"PASS ":"FAIL ")+"closing a tool preserves an independently disabled DAW\n";setEnabled(true);
    showArrangement();project=saved;undoStack=undos;redoStack=redos;changed(false);dirty=wasDirty;
    folder.getChildFile("window-focus.txt").replaceWithText(details);return ok;
#endif
}

bool MainComponent::verifyAuditRepairs(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    const auto saved=project;const auto savedUndo=undoStack,savedRedo=redoStack;const auto savedFile=projectFile;
    const bool savedDirty=dirty,savedDue=autosaveDue;const int savedTicks=ticks,oldTrack=selectedTrack,oldSection=selectedSection;
    const auto oldPage=page;bool ok=true;juce::String details;
    auto stage=[&](const char* label,bool passed){ok=passed&&ok;details+=juce::String(passed?"PASS ":"FAIL ")+label+"\n";};
    audio.stop();autosaveDue=false;ticks=0;undoStack.clear();redoStack.clear();
    project=newProject(1);selectedTrack=selectedSection=0;selectedClip=-1;noClipSelection=false;editCursor=-1;restoreInstruments();showArrangement();
    juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
    auto tick=[&]{audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
    auto beginTake=[&]{audio.stop();audio.seek(0);const bool started=audio.startRecording(0,0).wasOk();audio.liveMidi(0,juce::MidiMessage::noteOn(1,61,.8f));for(int i=0;i<16;++i)tick();audio.liveMidi(0,juce::MidiMessage::noteOff(1,61));tick();return started;};
    stage("take starts",beginTake());updateEditingAvailability();
    stage("recording keeps Main, Stop and Studio active while generation is disabled",isEnabled()&&stopButton.isEnabled()&&studioButton.isEnabled()&&!songButton.isEnabled());
    stopButton.onClick();stage("main Stop finalizes the take and restores editing",!audio.isRecording()&&!audio.isAudioRecording()&&!editingRestricted()&&songButton.isEnabled());
    stage("second take starts",beginTake());const auto checkpoints=undoStack.size();int accepted=0;
    closePending=true;completeCloseChoice(0,[&]{++accepted;});
    stage("Cancel close preserves the active take and Undo",audio.isRecording()&&accepted==0&&undoStack.size()==checkpoints&&!closePending);
    closePending=true;finishPendingSave({},[&]{++accepted;});
    stage("Cancel Save As preserves recording and cancels the continuation",audio.isRecording()&&accepted==0&&undoStack.size()==checkpoints&&!closePending);
    projectFile=juce::File();closePending=true;finishPendingSave(folder.getChildFile("missing-parent/failed.fga"),[&]{++accepted;});
    stage("failed save keeps unsaved work and does not continue",accepted==0&&dirty&&!audio.isRecording());
    stage("third take starts",beginTake());const auto destination=folder.getChildFile("close-saved.fga");closePending=true;
    finishPendingSave(destination,[&]{++accepted;});Project loaded;
    stage("successful first save writes the completed take and continues exactly once",accepted==1&&!dirty&&!audio.isRecording()&&projectFile==destination&&loadProject(destination,loaded).wasOk()&&std::any_of(loaded.tracks[0].clips[0].notes.begin(),loaded.tracks[0].clips[0].notes.end(),[](const Note& n){return n.pitch==61;}));
    audio.stop();dirty=false;requestClose([&]{++accepted;});stage("clean navigation continues immediately",accepted==2);
    const auto associated=folder.getChildFile("project prova con spazi.FGA");stage("project association fixture",destination.copyFileTo(associated));
    stage("project paths with spaces and uppercase extension",projectInCommandLine(associated.getFullPathName().quoted())==associated&&projectInCommandLine(associated.getFullPathName())==associated);
    const bool requestedOpen=openFromCommandLine(associated.getFullPathName().quoted());
    stage("project association shows loading before restoring the file",requestedOpen&&loadingView.isVisible());
    const auto openDeadline=juce::Time::getMillisecondCounterHiRes()+5000.;
    while(loadingView.isVisible()&&juce::Time::getMillisecondCounterHiRes()<openDeadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    stage("project association opens the saved arrangement",requestedOpen&&projectFile==associated&&midiFile(project)==midiFile(loaded)&&!loadingView.isVisible());
    // Drop onto the visible Snare row while another instrument is selected.
    showArrangement();arrangement.pixelsPerBar=24;updateSizes();select(0,0);arrangementPort.setViewPosition(0,0);
    auto wavFile=folder.getChildFile("drop-routing.wav");juce::WavAudioFormat wav;
    {std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(wavFile.createOutputStream().release(),44100,1,16,{},0));juce::AudioBuffer<float> signal(1,1024);for(int i=0;i<1024;++i)signal.setSample(0,i,.2f*std::sin(i*.07f));if(writer)writer->writeFromAudioSampleBuffer(signal,0,1024);else stage("drop WAV fixture",false);}
    const auto drop=getLocalPoint(&arrangement,juce::Point<int>(216+72,arrangement.rowY(1)+10));audio.seek(5.25);audio.play(true);const double position=audio.position();
    filesDropped({wavFile.getFullPathName()},drop.x,drop.y);waitForSampleImportForTests();
    stage("sample drop targets its row without seeking or stopping",selectedTrack==5&&project.tracks[5].instrument=="sample"&&project.tracks[0].instrument=="internal"&&audio.position()==position&&audio.isPlaying());
    auto midiProject=project;for(auto& t:midiProject.tracks)for(auto& c:t.clips)c.notes.clear();midiProject.tracks[0].clips[0].notes={{0,.5,69,100,0}};
    const auto bytes=midiFile(midiProject,0);const auto midi=folder.getChildFile("drop-routing.mid");midi.replaceWithData(bytes.data(),bytes.size());select(0,0);arrangementPort.setViewPosition(0,0);
    filesDropped({midi.getFullPathName()},drop.x,drop.y);
    stage("MIDI drop targets its row without seeking",selectedTrack==5&&audio.position()==position&&audio.isPlaying()&&project.tracks[5].clips[0].notes.size()==1&&project.tracks[5].clips[0].notes[0].pitch==69);
    project.tracks[5].freeTimeline=true;project.tracks[5].clips.clear();changed();filesDropped({midi.getFullPathName()},drop.x,drop.y);
    stage("MIDI drop on an empty lane creates a clip at the drop beat",project.tracks[5].clips.size()==1&&std::abs(project.tracks[5].clips[0].position-12)<1.e-6&&audio.position()==position);
    audio.stop();select(0,0);showEditor();clip().notes={{0,.25,60,90,0},{2,.25,64,95,0}};piano.fitNotes();piano.pixelsPerBeat=76;updateSizes();pianoPort.setViewPosition(0,0);
    const auto mouse=juce::Desktop::getInstance().getMainMouseSource();const auto now=juce::Time::getCurrentTime();
    auto event=[&](juce::Point<float> at){return juce::MouseEvent(mouse,at,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&piano,&piano,now,at,now,1,false);};
    auto count=undoStack.size();dirty=false;const auto empty=juce::Point<float>(piano.leftMargin()+(float)piano.pixelsPerBeat,piano.velocityTop()+piano.velocityHeight()*.5f);
    piano.mouseDown(event(empty));piano.mouseUp(event(empty));stage("empty velocity click leaves Undo and dirty unchanged",undoStack.size()==count&&!dirty);
    const auto first=clip().notes[0],second=clip().notes[1];piano.tool=2;
    auto notePoint=[&](const Note& n){return juce::Point<float>(piano.leftMargin()+(float)(n.start*piano.pixelsPerBeat)+2,piano.noteY(n)+piano.noteHeight()*.5f);};
    piano.mouseDown(event(notePoint(first)));piano.mouseDrag(event(notePoint(second)));piano.mouseUp(event(notePoint(second)));
    stage("eraser drag removes multiple notes as one Undo gesture",clip().notes.empty()&&undoStack.size()==count+1);undo(false);stage("one Undo restores all erased notes",clip().notes.size()==2);piano.tool=0;
    audio.stop();tick();dirty=true;autosaveDue=true;audio.play(true);const auto autosave=folder.getChildFile("autosave-snapshot.fga");autosave.deleteFile();autosaveIfIdle(autosave);
    stage("autosave is deferred while transport plays",autosaveDue&&!autosaveJob&&!autosave.existsAsFile());audio.stop();tick();const auto snapshotName=project.name;autosaveIfIdle(autosave);project.name="Edited after snapshot";
    const bool exited=autosaveJob&&autosaveJob->waitForThreadToExit(10000);Project snapshot;
    stage("idle autosave writes an independent snapshot without clearing dirty",exited&&loadProject(autosave,snapshot).wasOk()&&snapshot.name==snapshotName&&project.name=="Edited after snapshot"&&dirty);
    for(int i=0;i<20&&autosaveJob;++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    if(autosaveJob){++autosaveRequestId;retiredJobs.retire(std::move(autosaveJob));}
    audio.stop();project=saved;projectFile=savedFile;undoStack=savedUndo;redoStack=savedRedo;dirty=savedDirty;autosaveDue=savedDue;ticks=savedTicks;closePending=false;
    selectedTrack=oldTrack;selectedSection=oldSection;selectedClip=-1;noClipSelection=false;restoreInstruments();updateEditingAvailability();refreshControls();if(oldPage==Page::editor)showEditor();else showArrangement();
    folder.getChildFile("audit-repairs.txt").replaceWithText(details);return ok;
#endif
}

bool MainComponent::verifyPluginWindow(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    select(0,0);trackInspector.refresh();auto* direct=dynamic_cast<juce::Button*>(trackInspector.findChildWithID("inspector-plugin-editor"));if(direct)direct->onClick();
    if(!pluginWindow)return false;
    juce::String evidence;bool ok=true;
    auto check=[&](const char* name,bool passed){evidence+=juce::String(passed?"PASS ":"FAIL ")+name+"\n";ok=passed&&ok;};
    auto checkFocus=[&](const char* label){check(label,hasKeyboardFocus(false));if(!hasKeyboardFocus(false)){auto* focused=juce::Component::getCurrentlyFocusedComponent();evidence+="Focus diagnostics: foreground="+juce::String((int)juce::Process::isForegroundProcess())+", DAW descendants="+juce::String((int)hasKeyboardFocus(true))+", peer="+juce::String((int)(getPeer()&&getPeer()->isFocused()))+", focused="+(focused?focused->getName()+" / "+focused->getComponentID():juce::String("none"))+"\n";}};
    auto centred=[this]{auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();return display&&pluginWindow->getBounds().getCentre().getDistanceFrom(display->userArea.getCentre())<3;};
    auto settleCentre=[&]{const auto until=juce::Time::getMillisecondCounterHiRes()+1500.;while(pluginWindow&&!centred()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);};
    settleCentre();
    check("VST editor visible",pluginWindow->isVisible());check("VST editor always on top",pluginWindow->isAlwaysOnTop());check("VST editor centred",centred());if(!centred()){evidence+="Window bounds: "+pluginWindow->getBounds().toString()+"\n";if(auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())evidence+="Display user area: "+display->userArea.toString()+"\n";}
    pluginWindow->setTopLeftPosition(41,52);pluginWindow->closeButtonPressed();settleNativeFocus(*this);check("VST editor hidden after close",!pluginWindow->isVisible());checkFocus("DAW keyboard focus restored");
    pluginEditorButton.onClick();settleCentre();check("VST editor reopened",hasOpenPluginEditors());check("Reopened VST editor always on top",pluginWindow&&pluginWindow->isAlwaysOnTop());check("Reopened VST editor centred",pluginWindow&&centred());
    pluginWindow->closeButtonPressed();giveAwayKeyboardFocus();settleNativeFocus(*this);checkFocus("DAW focus recovered after a late native focus-loss notification");showArrangement();select(1,0);
    const int row=rowForTrack(0);arrangementPort.setViewPosition(0,std::max(0,arrangement.rowY(row)-rulerHeight));
    const auto view=arrangementPort.getViewArea();const auto point=headerChrome(view.getX(),arrangement.rowY(row),arrangement.rowHeight(row)).gear.getCentre().toFloat();
    const juce::MouseEvent gear(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&arrangement,&arrangement,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,false);
    arrangement.mouseMove(gear);check("VST gear tooltip follows hovered source",audio.plugin(0)&&arrangement.getTooltip()==tr("Open %plugin%").replace("%plugin%",audio.plugin(0)->getName()));
    const auto midi=midiFile(project);const auto history=undoStack.size();arrangement.mouseDown(gear);arrangement.mouseUp(gear);settleCentre();
    check("VST header gear opens native editor without MIDI or Undo changes",selectedTrack==0&&hasOpenPluginEditors()&&pluginWindow->isVisible()&&midiFile(project)==midi&&undoStack.size()==history);
    {exportFile(5,0,folder.getChildFile("before-freeze.wav"));const auto until=juce::Time::getMillisecondCounterHiRes()+60000;while(mediaBusy&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);check("Audio export retires its task before Freeze/Unfreeze",!mediaBusy&&!exportTask);}
    {auto* original=audio.plugin(0);const auto notes=midiFile(project);const auto history=undoStack.size();freezeTrack(0);const auto until=juce::Time::getMillisecondCounterHiRes()+60000;while(exportTask&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);check("Freeze retains and suspends real VST without MIDI/Undo edits",!exportTask&&audio.trackFrozen(0)&&audio.plugin(0)==original&&original&&original->isSuspended()&&midiFile(project)==notes&&undoStack.size()==history);if(audio.trackFrozen(0))freezeTrack(0);check("Unfreeze instantly resumes same real VST instance",!audio.trackFrozen(0)&&audio.plugin(0)==original&&original&&!original->isSuspended());}
    if(auto out=folder.getChildFile("plugin-inspector.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(trackInspector.createComponentSnapshot(trackInspector.getLocalBounds()),*out);}
    folder.getChildFile("plugin-window.txt").replaceWithText(evidence);if(juce::SystemStats::getEnvironmentVariable("FGA_SKIP_EXPORT_UI_TEST","0")=="1")folder.getChildFile("export-ui-skipped.txt").replaceWithText("SKIP previously verified export UI; default CI runs the complete flow.");else ok=verifyExports(folder.getChildFile("export-ui"))&&ok;closePluginEditor();return ok;
#endif
}


bool MainComponent::verifyLegacyHostedProjects(const juce::File& folder,const juce::PluginDescription& description){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder,description);return false;
#else
    juce::String report;bool ok=true;
    auto check=[&](bool pass,const juce::String& name){report+=(pass?"PASS ":"FAIL ")+name+"\n";folder.getChildFile("host-legacy.txt").replaceWithText(report);ok=pass&&ok;return pass;};
    auto* source=audio.plugin(0);if(!check(source!=nullptr,"source instrument available"))return false;
    juce::MemoryBlock native;source->getStateInformation(native);
    auto legacy=newProject(9022);legacy.sections={{Section::main,1,"Legacy VST"}};legacy.masterGain=1;legacy.normalise();
    for(auto& t:legacy.tracks){t.mute=true;t.effects.clear();t.parameters.clear();t.volume.enabled=false;for(auto& c:t.clips){c.notes.clear();c.feel={-1,-1,-1};}}
    legacy.masterAutomation.enabled=false;
    for(int ti=0;ti<3;++ti){auto& t=legacy.tracks[(size_t)ti];t.mute=false;t.gain=.5;t.instrument="plugin";t.instrumentPath=description.fileOrIdentifier.toStdString();t.instrumentId=description.createXml()->toString().toStdString();t.instrumentState=native.toBase64Encoding().toStdString();t.clips[0].notes={{0,3,69-ti*12,100,0}};}
    ParameterAutomation lane;lane.slot=0;lane.index=0;lane.name="Gain";lane.curve.enabled=true;lane.curve.points={{0,.25,0},{4,.5,0}};legacy.tracks[1].parameters.push_back(lane);
    auto reopenAndPlay=[&](const juce::File& file){
        audio.stop();dirty=false;open(file);const auto until=juce::Time::getMillisecondCounterHiRes()+15000.;
        bool sawIntermediate=false;
        while((loadingView.isVisible()||loadingPlugin)&&juce::Time::getMillisecondCounterHiRes()<until){juce::MessageManager::getInstance()->runDispatchLoopUntil(10);const int remaining=(int)restorePluginTasks.size()+loadingPlugin;sawIntermediate=sawIntermediate||(loadingView.isVisible()&&remaining>0&&remaining<restorePluginTotal);}
        check(sawIntermediate,"loading progress advances between saved plugins");
        if(!check(projectFile==file&&!loadingView.isVisible()&&loadingPlugin==0,"project and all saved instruments finish loading"))return false;
        for(int ti=0;ti<3;++ti){auto* plugin=audio.plugin(ti);if(!check(plugin&&plugin->getTotalNumOutputChannels()==4,"saved instrument retains secondary output: track "+juce::String(ti)))return false;}
        std::array<std::uint64_t,3> before{};for(int ti=0;ti<3;++ti)before[(size_t)ti]=audio.noteOnsSent(ti);
        playButton.onClick();juce::AudioBuffer<float> output(2,512);juce::AudioIODeviceCallbackContext context;float peak=0;
        for(int b=0;b<64;++b){audio.audioDeviceIOCallbackWithContext(nullptr,0,output.getArrayOfWritePointers(),2,512,context);peak=std::max(peak,output.getMagnitude(0,512));}
        bool played=audio.isPlaying()&&audio.position()>0&&std::isfinite(peak)&&peak>.001f;
        for(int ti=0;ti<3;++ti)played=played&&audio.noteOnsSent(ti)>before[(size_t)ti];
        audio.stop();return check(played,"open then Play sounds all saved instruments: "+file.getFileName());
    };
    for(int version:{1,2,3}){auto json=projectJson(legacy);json.getDynamicObject()->setProperty("version",version);const auto file=folder.getChildFile("host-legacy-v"+juce::String(version)+".fga");
        if(!check(file.replaceWithText(juce::JSON::toString(json)),"write legacy native-state fixture")||!reopenAndPlay(file))return false;
    }
    const auto saved=folder.getChildFile("host-upgraded.fga");if(!check(writeProjectFile(saved),"save upgraded hosted project")||!reopenAndPlay(saved))return false;
    for(int ti=0;ti<3;++ti)openHostedEditor(ti,0);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    bool allVisible=pluginWindows.size()==3;for(int ti=0;ti<3;++ti){auto* loaded=audio.plugin(ti);auto* editor=loaded?loaded->getActiveEditor():nullptr;allVisible=allVisible&&editor&&editor->getTopLevelComponent()->isVisible();}check(allVisible,"three instrument editors stay visible together");
    if(allVisible){pluginWindows.front()->closeButtonPressed();check(pluginWindows[1]->isVisible()&&pluginWindows[2]->isVisible(),"closing one instrument editor preserves the other windows");openHostedEditor(0,0);check(pluginWindows.size()==3&&pluginWindows.front()->isVisible(),"reopening an instrument reuses only its own editor");}closePluginEditor();
    source=audio.plugin(0);if(source&&source->getNumPrograms()>1){
        source->setCurrentProgram(1);source->getParameters()[0]->setValueNotifyingHost(.37f);audio.stop();
        const auto presetProject=folder.getChildFile("host-program.fga");if(!check(writeProjectFile(presetProject),"save chosen program and custom gain before playing any audio")||!reopenAndPlay(presetProject))return false;
        source=audio.plugin(0);check(source&&source->getCurrentProgram()==1&&source->getProgramName(1)=="Bright"&&std::abs(source->getParameters()[0]->getValue()-.37f)<1.e-5f,"reopening retains the selected program and its saved custom gain");
        check(audio.exportAudio(project,folder.getChildFile("host-program.wav"),0,false,[]{return false;},[](double){}).wasOk(),"export the restored instrument program");
        source->getParameters()[0]->setValueNotifyingHost(.61f);const auto undoCount=undoStack.size();
        loadSampleFile(0,folder.getChildFile("missing-sample.wav"));waitForSampleImportForTests();check(audio.plugin(0)==source&&undoStack.size()==undoCount,"failed sample replacement retains native instrument and Undo");
        loadSampleFile(0,folder.getChildFile("host-program.wav"));waitForSampleImportForTests();check(!audio.plugin(0)&&project.tracks[0].instrument=="sample"&&undoStack.size()==undoCount+1,"sample replacement creates one real Undo checkpoint");
        undo(false);const auto end=juce::Time::getMillisecondCounterHiRes()+15000.;while((loadingPlugin||loadingView.isVisible())&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        source=audio.plugin(0);check(source&&source->getCurrentProgram()==1&&std::abs(source->getParameters()[0]->getValue()-.61f)<1.e-5f,"Undo sample replacement restores the latest VST program and parameter edit");
    }
    return ok;
#endif
}

int ArrangementView::totalHeight() const {return rowY((int)owner.project.tracks.size());}
MainComponent::MainComponent(){
    Localisation::loadSaved();for(int track=0;track<maxTracks;++track){const auto key="trackHeight"+juce::String(track);arrangement.trackHeights[(size_t)track]=std::clamp(Localisation::setting(key.toRawUTF8(),"40").getIntValue(),40,240);}if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);setLookAndFeel(&theme);setWantsKeyboardFocus(true);project=newProject();selectedTrack=4;mixerHeight=std::clamp(Localisation::setting("mixerHeight","244").getIntValue(),160,4096);inspectorWidth=std::clamp(Localisation::setting("inspectorWidth","248").getIntValue(),224,480);sampleBrowserWidth=std::max(0,Localisation::setting("sampleBrowserWidth","0").getIntValue());
    inspectorVisible=Localisation::setting("inspectorVisible","1")=="1";
    mixerVisible=Localisation::setting("mixerVisible","1")=="1";
    arrangement.pixelsPerBar=std::clamp(Localisation::setting("arrangementZoom","6").getDoubleValue(),.05,160.);
    piano.pixelsPerBeat=std::clamp(Localisation::setting("pianoZoom","76").getDoubleValue(),2.,160.);
    addAndMakeVisible(timelineGrid);timelineGrid.addItemList({"SNAP: 1 bar","SNAP: 1 beat","SNAP: 1/2 beat","SNAP: 1/4 beat","SNAP: OFF"},1);timelineGrid.setSelectedId(2);timelineGrid.setTooltip(tr("Snap clip moves, clip edges and loop locators to bars or beats. OFF permits free positioning. Existing MIDI timing is unchanged. Hold Shift to bypass snapping."));timelineGrid.onChange=[this]{const double grids[]{4,1,.5,.25,0};timelineSnap=grids[std::clamp(timelineGrid.getSelectedItemIndex(),0,4)];};
    for(juce::Component* c:std::initializer_list<juce::Component*>{&backButton,&instrumentButton,&editButton,&leadButton,&arrangeButton,&guitarNotesButton,&durationLabel,&sectionSelector,&panelSections})addAndMakeVisible(c);
    instrumentPort.setViewedComponent(&instrumentPanel,false);instrumentPort.setScrollBarsShown(true,true);addAndMakeVisible(instrumentPort);
    backButton.onClick=[this]{showArrangement();};instrumentButton.onClick=[this]{showInstrument();};
    addAndMakeVisible(addTrackButton);addTrackButton.onClick=[this]{showAddTrackMenu();};addTrackButton.getProperties().set("fgaTip","Add a handwritten MIDI track or a sparse vocal chops part for Studio slices.");editButton.onClick=[this]{showEditor();};
    addAndMakeVisible(fitNotesButton);addAndMakeVisible(scaleGuideButton);scaleGuideButton.setToggleState(piano.showScaleGuide,juce::dontSendNotification);scaleGuideButton.getProperties().set("fgaTip","Show or hide the note lanes belonging to the current key and scale.");
    scaleGuideButton.onClick=[this]{piano.showScaleGuide=!piano.showScaleGuide;scaleGuideButton.setToggleState(piano.showScaleGuide,juce::dontSendNotification);Localisation::setSetting("pianoScaleGuide",piano.showScaleGuide?"1":"0");piano.repaint();};
    addAndMakeVisible(activeNotesButton);activeNotesButton.setComponentID("piano-active-notes");activeNotesButton.setToggleState(piano.showActiveNotes,juce::dontSendNotification);
    Localisation::bindTip(activeNotesButton,"Show or hide the current track's notes without muting playback. Hidden notes cannot be edited.");
    activeNotesButton.onClick=[this]{piano.setActiveNotesVisible(!piano.showActiveNotes);};
    muteButton.setClickingTogglesState(true);soloButton.setClickingTogglesState(true);
    int toolIndex=0;for(auto* button:{&drawTool,&selectTool,&eraseTool}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4001);const int index=toolIndex++;button->onClick=[this,index]{piano.tool=index;};}selectTool.setToggleState(true,juce::dontSendNotification);
    drawTool.setConnectedEdges(juce::Button::ConnectedOnRight);selectTool.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);eraseTool.setConnectedEdges(juce::Button::ConnectedOnLeft);
    drawTool.setComponentID("tool-draw");selectTool.setComponentID("tool-select");eraseTool.setComponentID("tool-erase");
    drawTool.getProperties().set("fgaTip","Pencil: click to draw notes, drag to move, drag the edge for duration.");selectTool.getProperties().set("fgaTip","Select: drag a rectangle to select notes, then move or edit them.");eraseTool.getProperties().set("fgaTip","Eraser: click or drag over notes to delete them.");
    addAndMakeVisible(regenerateClipButton);regenerateClipButton.setButtonText("REGENERATE");regenerateClipButton.getProperties().set("fgaIconOnly",true);regenerateClipButton.setTitle(tr("REGENERATE"));regenerateClipButton.setTooltip(tr("Regenerate the notes in this editor."));regenerateClipButton.setComponentID("editor-regenerate");regenerateClipButton.iconColour=Palette::accent;regenerateClipButton.onClick=[this]{regenerateEditedClip();};
    regenerateClipButton.getProperties().set("fgaTip","Regenerate this clip from the instrument panel settings: a new variation of the same groove. Locked clips are kept; Undo restores the notes.");
    int brushIndex=0;for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4002);const int index=brushIndex++;button->onClick=[this,index]{piano.brushVelocity=index==0?127:index==2?45:100;piano.flamBrush=index==3;};button->setTooltip(tr("Choose the hit to draw, as in the Drums and Guitar editor."));button->setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);}brushAccent.setConnectedEdges(juce::Button::ConnectedOnRight);brushFlam.setConnectedEdges(juce::Button::ConnectedOnLeft);brushHit.setToggleState(true,juce::dontSendNotification);
    fitNotesButton.onClick=[this]{piano.fitNotes();piano.repaint();};
    guitarNotesButton.onClick=[this]{piano.guitarNotes=!piano.guitarNotes;Localisation::setSetting("pianoGuitarNotes",piano.guitarNotes?"1":"0");showEditor();};
    leadButton.setClickingTogglesState(true);leadButton.onClick=[this]{if(loadingPlugin)return;checkpoint();project.settings.guitarLead=!project.settings.guitarLead;generate(project,Scope::track,0,selectedSection);generate(project,Scope::track,3,selectedSection);changed();};
    durationLabel.getProperties().set("fgaTip","Song length follows genre, harmonic phrases and groove development.");durationLabel.setTooltip(tr("Song length follows genre, harmonic phrases and groove development."));
    addAndMakeVisible(targetDurationLabel);addAndMakeVisible(targetDurationEditor);
    targetDurationLabel.setText("Target duration",juce::dontSendNotification);targetDurationLabel.setJustificationType(juce::Justification::centredRight);targetDurationLabel.setMinimumHorizontalScale(.6f);targetDurationLabel.setFont(Daw::font(Daw::body));targetDurationLabel.setColour(juce::Label::textColourId,Daw::textDim);targetDurationLabel.setComponentID("target-duration-label");
    targetDurationLabel.getProperties().set("fgaTip","Optional clock length, for example 3:30. The arrangement is not rewritten.");
    targetDurationEditor.setComponentID("target-duration");targetDurationEditor.setJustification(juce::Justification::centred);targetDurationEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),13.f,juce::Font::plain));targetDurationEditor.setIndents(4,2);targetDurationEditor.setTextToShowWhenEmpty(juce::String::fromUTF8("—"),Daw::textFaint);targetDurationEditor.setSelectAllWhenFocused(true);
    targetDurationEditor.getProperties().set("fgaTip","Optional clock length, for example 3:30. Leave empty to turn the target off. The arrangement is not rewritten.");
    targetDurationEditor.onReturnKey=[this]{commitTargetDuration();};targetDurationEditor.onFocusLost=[this]{commitTargetDuration();};
    addAndMakeVisible(targetToleranceLabel);addAndMakeVisible(targetToleranceEditor);addAndMakeVisible(targetFitLabel);
    targetToleranceLabel.setText("Tolerance",juce::dontSendNotification);targetToleranceLabel.setJustificationType(juce::Justification::centredRight);targetToleranceLabel.setMinimumHorizontalScale(.55f);targetToleranceLabel.setFont(Daw::font(Daw::body));targetToleranceLabel.setColour(juce::Label::textColourId,Daw::textDim);targetToleranceLabel.setComponentID("target-tolerance-label");
    targetToleranceLabel.getProperties().set("fgaTip","Tolerance in seconds around the target. Zero means an exact length. The song is not rewritten.");
    targetToleranceEditor.setComponentID("target-tolerance");targetToleranceEditor.setJustification(juce::Justification::centred);targetToleranceEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),13.f,juce::Font::plain));targetToleranceEditor.setIndents(2,2);targetToleranceEditor.setSelectAllWhenFocused(true);targetToleranceEditor.setText("5",false);
    targetToleranceEditor.onReturnKey=[this]{commitTargetTolerance();};targetToleranceEditor.onFocusLost=[this]{commitTargetTolerance();};
    targetFitLabel.setJustificationType(juce::Justification::centredLeft);targetFitLabel.setMinimumHorizontalScale(.55f);targetFitLabel.setFont(Daw::font(Daw::small,true));targetFitLabel.setComponentID("target-fit");targetFitLabel.getProperties().set("fgaRawText",true);
    arrangeButton.onClick=[this]{if(loadingPlugin)return;for(const auto& t:project.tracks){if(t.locked){message("Sblocca le tracce prima di ricostruire la struttura. GENERATE SONG mantiene la struttura e i blocchi.");return;}for(const auto& c:t.clips)if(c.locked){message("Sblocca i clip prima di ricostruire la struttura.");return;}}auto next=project;if(!rearrangeSong(next,(std::uint32_t)juce::Random::getSystemRandom().nextInt())){message("Arrangement change exceeds the project limits.",true);return;}checkpoint();audio.reusePluginStates(next);audio.stop();project=std::move(next);editCursor=-1;selectedSection=0;arrangement.selectedClips.clear();noClipSelection=false;changed();showArrangement();};
    panelSections.onChange=[this]{auto names=instrumentPanel.sectionNames();int index=panelSections.getSelectedItemIndex();if(index>=0&&index<names.size())instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(names[index]));};
    sectionSelector.onChange=[this]{if(!refreshing){select(selectedTrack,sectionSelector.getSelectedItemIndex());if(page==Page::editor)showEditor();}};
    addAndMakeVisible(chordEditButton);chordEditButton.onClick=[this]{if(juce::ModifierKeys::currentModifiers.isShiftDown())showChordMenu();else showChordCirclePopover(0);};chordsLabel.setInterceptsMouseClicks(true,false);chordsLabel.setMouseCursor(juce::MouseCursor::PointingHandCursor);chordsLabel.addMouseListener(this,false);
    inspectorPort.setViewedComponent(&trackInspector,false);inspectorPort.setScrollBarsShown(true,false);addAndMakeVisible(inspectorPort);addAndMakeVisible(mixer);
    for(auto* button:{&inspectorToggle,&mixerToggle,&followButton}){addAndMakeVisible(button);button->setClickingTogglesState(true);}
    addAndMakeVisible(creditsButton);creditsButton.setTooltip(tr("ABOUT"));creditsButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);creditsButton.onClick=[this]{showAbout();};
    addAndMakeVisible(quitButton);quitButton.setComponentID("app-quit");
    quitButton.getProperties().set("fgaTip","Quit the app. Unsaved changes can be saved or kept open.");
    quitButton.onClick=[]{if(auto* app=juce::JUCEApplication::getInstance())app->systemRequestedQuit();};
    addAndMakeVisible(fullScreenButton);fullScreenButton.setClickingTogglesState(false);
    fullScreenButton.onClick=[this]{applyFullScreen(!appFullScreen);};
    addAndMakeVisible(inspectorResizer);addChildComponent(browserResizer);trackInspector.addAndMakeVisible(inspectorCollapse);inspectorCollapse.setComponentID("inspector-collapse");
    inspectorCollapse.quiet=true;inspectorCollapse.getProperties().set("fgaTip","Hide the track inspector. Restore it with the inspector icon in the toolbar. Shortcut: I.");
    inspectorCollapse.onClick=[this]{inspectorVisible=!inspectorVisible;inspectorToggle.setToggleState(inspectorVisible,juce::dontSendNotification);resized();};
    addAndMakeVisible(fitSongButton);inspectorToggle.setToggleState(inspectorVisible,juce::dontSendNotification);mixerToggle.setToggleState(mixerVisible,juce::dontSendNotification);
    inspectorToggle.onClick=[this]{inspectorVisible=inspectorToggle.getToggleState();resized();};
    mixerToggle.onClick=[this]{mixerVisible=mixerToggle.getToggleState();resized();};
    followButton.onClick=[this]{followPlayhead=followButton.getToggleState();};
    fitSongButton.onClick=[this]{fitSong();};
    // Sample browser docked on the right of the arrangement (B).
    addAndMakeVisible(browserToggle);browserToggle.setClickingTogglesState(true);browserToggle.onClick=[this]{showSampleBrowser(browserToggle.getToggleState());};
    browserToggle.getProperties().set("fgaTip","Show the sample browser: your sample folders, preview and drag onto a track. Shortcut: B.");
    addChildComponent(sampleBrowser);
    sampleBrowser.onClose=[this]{showSampleBrowser(false);};
    sampleBrowser.onLoad=[this](const juce::File& file){if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}loadSampleFile(selectedTrack,file);};
    sampleBrowser.onMessage=[this](const juce::String& text,bool error){message(text,error);};
    sampleBrowser.targetName=[this]{return utf(project.tracks[(size_t)selectedTrack].name);};
    browserVisible=Localisation::setting("sampleBrowserVisible","0")=="1";browserToggle.setToggleState(browserVisible,juce::dontSendNotification);
    addAndMakeVisible(metronomeButton);metronomeButton.onClick=[this]{toggleMetronome();};
    metronomeButton.getProperties().set("fgaTip","Metronome while playing (never exported). Shortcut: C.");
    // Monospaced digits: the transport keeps a fixed width while it counts, so the bar/beat
    // readout never jitters and the surrounding LCD blocks never shift.
    positionLabel.setBorderSize(juce::BorderSize<int>(0));positionLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),Daw::display,juce::Font::bold));positionLabel.setColour(juce::Label::textColourId,Daw::text);positionLabel.setJustificationType(juce::Justification::centredLeft);positionLabel.setInterceptsMouseClicks(false,false);
    timeDisplay=Localisation::setting("timelineTimeDisplay","0")=="1";
    addAndMakeVisible(positionModeButton);positionModeButton.onClick=[this]{toggleTimeDisplay();};
    positionModeButton.getProperties().set("fgaTip","Click to switch bars and elapsed time. The musical grid and MIDI timing stay unchanged.");
    for(juce::Component* c:std::initializer_list<juce::Component*>{&timelineZoomOut,&timelineZoomIn,&timelineFitButton,&timelineZoom})addAndMakeVisible(c);
    timelineZoomOut.setComponentID("timeline-zoom-out");timelineZoomIn.setComponentID("timeline-zoom-in");timelineFitButton.setComponentID("timeline-fit");timelineZoom.setComponentID("timeline-zoom");
    timelineZoomOut.onClick=[this]{zoomAt(false,1./1.25,arrangementPort.getWidth()*.5);};timelineZoomIn.onClick=[this]{zoomAt(false,1.25,arrangementPort.getWidth()*.5);};timelineFitButton.onClick=[this]{fitSong();};timelineFitButton.setIcon(IconButton::Icon::fitWidth);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&timelineZoomOut,&timelineZoomIn,&timelineFitButton,&timelineZoom})c->setWantsKeyboardFocus(false);
    timelineZoom.setRange(.05,160,.001);timelineZoom.setSkewFactorFromMidPoint(6);timelineZoom.setSliderStyle(juce::Slider::LinearHorizontal);timelineZoom.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);timelineZoom.setScrollWheelEnabled(false);
    timelineZoom.onValueChange=[this]{zoomAt(false,timelineZoom.getValue()/arrangement.pixelsPerBar,arrangementPort.getWidth()*.5);};
    timelineZoomOut.getProperties().set("fgaTip","Zoom out of the arrangement without changing MIDI.");timelineZoomIn.getProperties().set("fgaTip","Zoom into the arrangement without changing MIDI.");timelineZoom.getProperties().set("fgaTip","Arrangement zoom. Drag to change the visible song length.");timelineFitButton.getProperties().set("fgaTip","Fit the complete song to the available timeline width. Shortcut: F.");
    for(auto* b:{&zoomSelectionButton,&loopSelectionButton})addAndMakeVisible(b);
    zoomSelectionButton.onClick=[this]{zoomSelection();};loopSelectionButton.onClick=[this]{loopSelection();};
    zoomSelectionButton.getProperties().set("fgaTip","Zoom to the selected clips or notes. Shortcut: Z.");
    loopSelectionButton.getProperties().set("fgaTip","Set the playback loop to the selected clips or notes. Shortcut: Ctrl/Cmd+L.");
    inspectorToggle.getProperties().set("fgaTip","Show track volume, pan, sound assignment and clip actions.");mixerToggle.getProperties().set("fgaTip","Show the mixer. Shortcut: F3.");fitSongButton.getProperties().set("fgaTip","Fit the complete song in the arrangement view.");followButton.getProperties().set("fgaTip","Scroll the arrangement to follow playback.");fullScreenButton.getProperties().set("fgaTip","Fill the screen with the arranger, without the window title bar. Shortcut: F11.");
    arrangement.getProperties().set("fgaTip","Wheel on ruler: zoom. Ctrl/Cmd+wheel: zoom at pointer. Shift+wheel: horizontal scroll.");piano.getProperties().set("fgaTip","Wheel on ruler: zoom. Ctrl/Cmd+wheel: zoom at pointer. Shift+wheel: horizontal scroll.");
    arrangementPort.setViewedComponent(&arrangement,false);pianoPort.setViewedComponent(&piano,false);arrangementPort.setScrollBarsShown(true,true);pianoPort.setScrollBarsShown(true,true);addAndMakeVisible(arrangementPort);addAndMakeVisible(pianoPort);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&playButton,&stopButton,&rewindButton,&undoButton,&redoButton,&loopButton,&songButton,&generateMenuButton,&trackButton,&sectionButton,&clipButton,&harmonyButton,&addSectionButton,&spotlightButton,&genreBox,&keyBox,&scaleBox,&extensionBox,&voicingBox,&progressionBox,&bassBox,&stringsBox,&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&zoomSlider,&positionLabel,&chordsLabel,&statusLabel,&soundButton,&pluginEditorButton,&sampleButton,&loadPluginButton,&gainSlider,&panSlider,&velocitySlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&soundBox,&gridBox,&quantiseButton,&transposeUp,&transposeDown,&deleteNotes,&importButton,&copyButton,&pasteButton,&sectionLoopButton,&pianoPlayButton,&inspectorLabel,&editorLabel})addAndMakeVisible(c);
    // These four labels kept JUCE's 15 px default and towered over the 10.5-12 px
    // controls around them; the theme's body size keeps the chrome consistent.
    for(auto* label:{&chordsLabel,&statusLabel,&editorLabel,&durationLabel,&targetDurationLabel}){label->setFont(Daw::font(Daw::body));label->setMinimumHorizontalScale(.65f);}
    inspectorLabel.getProperties().set("fgaRawText",true);
    auto slider=[this](juce::Slider& s,double lo,double hi,double step,const juce::String& suffix){s.setRange(lo,hi,step);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,22);s.setTextValueSuffix(suffix);s.onDragStart=[this]{checkpoint();};};
    addAndMakeVisible(automationButton);
    automationButton.onClick=[this]{showAutomationPicker(selectedTrack);};

    slider(tempoSlider,30,300,.1," BPM");slider(swingSlider,0,150,1,"% swing");slider(densitySlider,0,100,1,"% notes");slider(humanizeSlider,0,100,1,"% feel");slider(masterSlider,-60,6,.1," dB");
    for(auto* knob:{&densitySlider,&swingSlider,&humanizeSlider,&masterSlider}){knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knob->setMouseDragSensitivity(180);}
    densitySlider.setComponentID("song-density");swingSlider.setComponentID("song-swing");humanizeSlider.setComponentID("song-feel");masterSlider.setComponentID("master-gain");
    slider(zoomSlider,.05,160,.05," zoom");slider(gainSlider,-60,6,.1," dB");slider(panSlider,-100,100,1," pan");slider(velocitySlider,1,127,1," vel");velocitySlider.setValue(96);
    for(size_t i=0;i<genres().size();++i)genreBox.addItem(utf(genres()[i]),(int)i+1);
    for(int i=0;i<12;++i)keyBox.addItem(sgk::spelledNote(i,9,0),i+1);
    for(int i=0;i<13;++i)scaleBox.addItem(sgk::scaleName(i),i+1);
    for(int i=0;i<11;++i){extensionBox.addItem(sgk::extensionName(i),i+1);voicingBox.addItem(sgk::voicingName(i),i+1);}
    const auto& progressions=sgk::progressionPresets();for(size_t i=0;i<progressions.size();++i)progressionBox.addItem(progressions[i].name,(int)i+1);progressionBox.setTextWhenNothingSelected("PROGRESSION");
    for(int i=0;i<sgk::numBassModes;++i)bassBox.addItem(sgk::bassModeName(i),i+1);for(int i=0;i<sgk::numStringsModes;++i)stringsBox.addItem(sgk::stringsModeName(i),i+1);
    gridBox.addItem("1/4",1);gridBox.addItem("1/8",2);gridBox.addItem("1/16",3);gridBox.addItem("1/32",4);gridBox.setSelectedId(3);
    auto setting=[this](std::function<void()> fn){if(refreshing)return;checkpoint();fn();changed(false);message("Settings updated. REGENERATE applies them to MIDI. GENERATE SONG draws a new song.");};
    genreBox.onChange=[this,setting]{setting([this]{applyGenre(project,genreBox.getSelectedItemIndex());});message("Genre ready: NEW ARRANGEMENT rebuilds the form; GENERATE SONG varies tempo, length and patterns inside this genre.");};
    keyBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;const int key=keyBox.getSelectedItemIndex();const auto current=activeHarmony(project,selectedSection);if(key<0||key==current.key)return;instrumentPanel.flushRender();checkpoint();if(sectionHarmonyEnabled(project,selectedSection)){project.sections[(size_t)selectedSection].harmony.key=key;generate(project,Scope::section,selectedTrack,selectedSection,0);changed();}else{project.settings.key=key;applyInstrumentSettings(0,true);}if(instrumentIsOpen())instrumentPanel.rebuild();};
    scaleBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;const int scale=scaleBox.getSelectedItemIndex();const auto current=activeHarmony(project,selectedSection);if(scale<0||scale==current.scale)return;instrumentPanel.flushRender();checkpoint();if(sectionHarmonyEnabled(project,selectedSection)){project.sections[(size_t)selectedSection].harmony.scale=scale;generate(project,Scope::section,selectedTrack,selectedSection,0);changed();}else{project.settings.scale=scale;applyInstrumentSettings(0,true);}if(instrumentIsOpen())instrumentPanel.rebuild();};
    extensionBox.onChange=[this]{if(refreshing)return;instrumentPanel.flushRender();checkpoint();project.settings.extension=extensionBox.getSelectedItemIndex();applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    voicingBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;instrumentPanel.flushRender();checkpoint();project.settings.voicing=voicingBox.getSelectedItemIndex();applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    progressionBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;instrumentPanel.flushRender();checkpoint();if(sectionHarmonyEnabled(project,selectedSection)){auto edited=resolveSectionHarmony(project,selectedSection);chooseProgression(edited,progressionBox.getSelectedItemIndex());storeHarmony(project,selectedSection,edited);generate(project,Scope::section,selectedTrack,selectedSection,0);changed();}else{chooseProgression(project,progressionBox.getSelectedItemIndex());applyInstrumentSettings(0,true);}if(instrumentIsOpen())instrumentPanel.rebuild();};
    bassBox.onChange=[this,setting]{setting([this]{project.settings.bassMode=bassBox.getSelectedItemIndex();});};
    stringsBox.onChange=[this,setting]{setting([this]{project.settings.stringsMode=stringsBox.getSelectedItemIndex();});};
    tempoSlider.onValueChange=[this]{if(!refreshing){project.settings.bpm=tempoSlider.getValue();changed(false);}};
    auto feel=[this](juce::Slider& control,unsigned parameter,double Settings::* member){control.onValueChange=[this,&control,parameter,member]{if(refreshing||loadingPlugin||editingRestricted())return;project.settings.*member=control.getValue()/100.;queueGlobalFeel(control,parameter);};};
    feel(densitySlider,densityFeel,&Settings::density);feel(swingSlider,swingFeel,&Settings::swing);feel(humanizeSlider,humanizeFeel,&Settings::humanize);
    masterSlider.onValueChange=[this]{if(!refreshing){project.masterGain=gainFromControl(masterSlider);mixChanged();}};
    zoomSlider.onDragStart=nullptr;zoomSlider.onValueChange=[this]{const bool editor=page==Page::editor;auto& port=editor?pianoPort:arrangementPort;const double scale=editor?piano.pixelsPerBeat:arrangement.pixelsPerBar;zoomAt(editor,zoomSlider.getValue()/scale,port.getWidth()*.5);};zoomSlider.setValue(6,juce::sendNotificationSync);
    gainSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].gain=gainFromControl(gainSlider);mixChanged();}};
    panSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].pan=valueFromControl(panSlider)/100.;mixChanged();}};
    velocitySlider.onDragStart=[this]{if(!piano.selected.empty())checkpoint();};velocitySlider.onValueChange=[this]{if(!refreshing&&!piano.selected.empty()){for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i].velocity=(int)velocitySlider.getValue();changed();}};
    auto toggle=[this](juce::Button& b,bool Track::*field){b.onClick=[this,&b,field]{checkpoint();project.tracks[(size_t)selectedTrack].*field=b.getToggleState();changed(false);};};toggle(muteButton,&Track::mute);toggle(soloButton,&Track::solo);toggle(lockButton,&Track::locked);
    lockClipButton.onClick=[this]{checkpoint();clip().locked=lockClipButton.getToggleState();changed(false);};
    playButton.onClick=[this]{piano.stopPreview();if(audio.isRecording()||audio.isAudioRecording())stopPlaybackAndRecording();else togglePlayback();};stopButton.onClick=[this]{editCursor=-1;stopPlaybackAndRecording();};rewindButton.onClick=[this]{seekTimeline(0);};
    loopButton.onClick=[this]{toggleLoop();};
    loopButton.getProperties().set("fgaTip","Loop playback on or off. Shortcut: L. Ctrl/Cmd+L loops the selection; again removes it. Double-click the loop strip to clear it.");
    pianoPlayButton.setIcon(IconButton::Icon::play);pianoPlayButton.getProperties().set("fgaTip","Play only the selected clip in a loop. Click again to stop.");
    pianoPlayButton.onClick=[this]{if(!piano.selected.empty()){togglePlayback();return;}if(editingRestricted()||loadingPlugin||clipIndex()<0)return;piano.stopPreview();if(audio.isClipPlaying())audio.play(false);else{editCursor=-1;audio.playClip(selectedTrack,clipStart(),clipStart()+clipLength());}};
    sectionLoopButton.onClick=[this]{toggleClipLoop();};sectionLoopButton.getProperties().set("fgaTip","Loop the selected clip; click again to remove the loop.");
    addAndMakeVisible(studioButton);studioButton.setIcon(IconButton::Icon::studio);studioButton.getProperties().set("dawFeatured",true);Localisation::bindTip(studioButton,"Sampler, MIDI/audio recording and project samples");studioButton.onClick=[this]{showStudio();};
    addAndMakeVisible(aiComposeButton);aiComposeButton.primary=false;aiComposeButton.setButtonText("AI COMPOSER");aiComposeButton.getProperties().set("fgaTip","Compose MIDI notes and an arrangement with your selected AI service.");aiComposeButton.onClick=[this]{showAiComposer();};
    songButton.onClick=[this]{regenerate(Scope::song);};trackButton.onClick=[this]{regenerate(Scope::track,page!=Page::instrument);};sectionButton.onClick=[this]{regenerate(Scope::section);};clipButton.onClick=[this]{regenerate(Scope::clip,page!=Page::instrument);};
    // One slot for the four generation scopes: the main button still generates the song with a
    // single click, the caret opens track / section / clip. The three old buttons stay in the
    // component (their callbacks and shortcuts are unchanged) but are no longer laid out.
    generateMenuButton.onClick=[this]{showGenerateMenu();};
    generateMenuButton.getProperties().set("fgaTip","Choose what GENERATE rewrites: the song, the selected track, the section or the clip.");
    for(auto* b:{&trackButton,&sectionButton,&clipButton})b->setVisible(false);
    harmonyButton.onClick=[this]{checkpoint();const auto seed=(std::uint32_t)juce::Random::getSystemRandom().nextInt();if(sectionHarmonyEnabled(project,selectedSection)){auto edited=resolveSectionHarmony(project,selectedSection);randomiseHarmony(edited,seed);storeHarmony(project,selectedSection,edited);generate(project,Scope::section,selectedTrack,selectedSection,0);changed();message("Section chords written into unlocked tracks in this section.");}else{randomiseHarmony(project,seed);applyInstrumentSettings(0,true);message("New chords written into unlocked tracks. GENERATE SONG draws another harmony.");}};
    undoButton.onClick=[this]{undo(false);};redoButton.onClick=[this]{undo(true);};
    quantiseButton.onClick=[this]{modifySelected([this](Clip& c){quantise(c,piano.grid,1,clipLength());});};transposeUp.onClick=[this]{modifySelected([](Clip& c){transpose(c,12);});};transposeDown.onClick=[this]{modifySelected([](Clip& c){transpose(c,-12);});};deleteNotes.onClick=[this]{piano.deleteSelected();};
    gridBox.setSelectedId(std::clamp(Localisation::setting("pianoGrid","3").getIntValue(),1,4),juce::dontSendNotification);piano.grid=std::pow(.5,gridBox.getSelectedItemIndex());
    gridBox.onChange=[this]{piano.grid=std::pow(.5,gridBox.getSelectedItemIndex());Localisation::setSetting("pianoGrid",gridBox.getSelectedId());piano.repaint();};
    copyButton.onClick=[this]{clipboard.clear();for(size_t i=0;i<clip().notes.size();++i)if(piano.selected.empty()||piano.selected.count((int)i))clipboard.push_back(clip().notes[i]);message("Notes copied. Paste inserts at the cursor inside the clip.");};
    pasteButton.onClick=[this]{piano.pasteNotes();};
    soundButton.setButtonText("BUILT-IN");soundButton.onClick=[this]{useBuiltInSound(selectedTrack);};
    sampleButton.onClick=[this]{if(sampleImportActive())cancelSampleImport();else browseSamplesFor(selectedTrack);};
    loadPluginButton.onClick=[this]{showPluginMenu();};
    pluginEditorButton.onClick=[this]{openHostedEditor(selectedTrack,0);};
    soundBox.onChange=[this]{if(refreshing)return;checkpoint();project.tracks[(size_t)selectedTrack].sound=soundBox.getSelectedItemIndex();changed(false);};
    importButton.onClick=[this]{chooseFile(tr("Import notes into the selected section"),"*.mid;*.midi",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){importFile(f);});};
    addSectionButton.onClick=[this]{showAddSectionMenu();};spotlightButton.setIcon(IconButton::Icon::zoomIn);spotlightButton.setPrimary(true);spotlightButton.setTooltip(tr("Spotlight Quick Search (Ctrl/Cmd+K): search actions, genres, tracks, sounds and VST3/AU plugins."));spotlightButton.onClick=[this]{showSpotlightSearch();};
    saveButton.onClick=[this]{save(false);};saveAsButton.onClick=[this]{save(true);};
    openButton.onClick=[this]{chooseFile(tr("Open Fase Groove Arranger project"),"*.fga",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){open(f);});};
    newButton.onClick=[this]{juce::Component::SafePointer<MainComponent> safe(this);requestClose([safe]{if(safe)safe->createNewProject();});};
    exportButton.onClick=[this]{exportMenu();};settingsButton.onClick=[this]{showAudioSettings();};aboutButton.onClick=[this]{showAbout();};helpButton.onClick=[this]{showGuide(Localisation::current()!=Localisation::Language::italian);};
    playButton.onColour=Daw::play;loopButton.onColour=Daw::accent;metronomeButton.onColour=Daw::accent;
    // Icon-only actions: the tooltip names them (translated with the rest of the interface).
    for(const auto& entry:std::initializer_list<std::pair<IconButton*,const char*>>{{&newButton,"Start a new song project."},{&openButton,"Open a local .fga project."},{&saveButton,"Save the song and all instrument settings in a project."},{&saveAsButton,"SAVE AS"},{&exportButton,"Export the mix or separate tracks as MIDI, WAV or MP3."},{&settingsButton,"Choose the audio output and buffer size."},{&aboutButton,"Choose the language, control tooltips, credits and privacy."},{&helpButton,"GUIDE"},{&automationButton,"Choose a sound, instrument or effect parameter to automate."}})entry.first->getProperties().set("fgaTip",entry.second);
    for(auto* b:{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&inspectorToggle,&mixerToggle,&browserToggle,&zoomSelectionButton,&loopSelectionButton,&fitSongButton,&followButton,&automationButton,&fullScreenButton})b->quiet=true;
    for(auto* b:{&inspectorToggle,&mixerToggle,&browserToggle,&followButton,&automationButton,&fullScreenButton})b->onColour=b==&automationButton?Daw::automation:Daw::accent;
    tempoSlider.setSliderStyle(juce::Slider::LinearBarVertical);tempoSlider.setTextValueSuffix("");tempoSlider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,82,26);tempoSlider.setColour(juce::Slider::textBoxTextColourId,Daw::text);tempoSlider.setMouseDragSensitivity(400);tempoSlider.setVelocityBasedMode(false);tempoSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    for(auto* box:{&keyBox,&scaleBox}){box->getProperties().set("transportReadout",true);box->setColour(juce::ComboBox::backgroundColourId,juce::Colours::transparentBlack);box->setColour(juce::ComboBox::textColourId,Daw::text);box->getProperties().set("dawDisplay",box==&keyBox?17.f:13.f);}
    masterSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,58,22);masterSlider.setDoubleClickReturnValue(true,0);
playButton.setTooltip(tr("Start or pause playback."));stopButton.setTooltip(tr("Stop playback and return to the beginning."));rewindButton.setTooltip(tr("Back to the beginning."));undoButton.setTooltip(tr("Undo"));redoButton.setTooltip(tr("Redo"));
    rewindButton.setConnectedEdges(juce::Button::ConnectedOnRight);playButton.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);stopButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    for(auto* b:{&songButton}){b->setColour(juce::TextButton::buttonColourId,Palette::accent);b->setColour(juce::TextButton::textColourOffId,Palette::background);}
    for(const auto& entry:std::initializer_list<std::pair<juce::Component*,const char*>>{{&genreBox,"GROOVE"},{&keyBox,"KEY"},{&scaleBox,"Scale / mode"},{&extensionBox,"Extensions"},{&voicingBox,"Voicing"},{&progressionBox,"PROGRESSION"},{&tempoSlider,"TEMPO"},{&swingSlider,"SWING"},{&densitySlider,"DENSITY"},{&humanizeSlider,"HUMANIZE"},{&masterSlider,"Volume"},{&gainSlider,"Volume"},{&panSlider,"PAN"},{&velocitySlider,"VELOCITY"},{&zoomSlider,"ZOOM"},{&durationLabel,"NEW ARRANGEMENT"}})entry.first->getProperties().set("fgaControl",entry.second);
    piano.getProperties().set("fgaTip","Draw notes, drag to move, drag the edge for duration; Alt-drag changes velocity.");

    addChildComponent(options);addChildComponent(updateOverlay);
    options.onLanguageChanged=[this]{applyLanguage();};
    options.onScaleChosen=[this](float scale){if(onScaleChosen)onScaleChosen(scale);};
    options.onTipsChanged=[this](bool enabled){Localisation::setTipsEnabled(enabled);applyLanguage();};
    addChildComponent(pluginSettings);addChildComponent(pluginBrowser);
    pluginBrowser.onChoose=[this](const PluginCatalog::BrowserRow& row){chooseBrowserRow(row);};
    // The Open File button hides the browser before launching its native chooser.
    // Defer restoration so the chooser/loading guards are already set.
    pluginBrowser.onDismiss=[this]{juce::Component::SafePointer<MainComponent> safe(this);juce::MessageManager::callAsync([safe]{if(safe)safe->resumeEffectChain();});};
    pluginBrowser.onRescan=[this]{pluginCatalog.scan(PluginCatalog::ScanMode::all);};
    pluginSettings.onReport=[this]{auto text=pluginCatalog.report();auto file=PluginCatalog::defaultStorage().getChildFile("ScanReport.txt");file.getParentDirectory().createDirectory();if(file.replaceWithText(text))text+="\n\n"+tr("Report file")+": "+file.getFullPathName();showDocument(tr("SCAN REPORT"),text);};
    pluginBrowser.onSettings=[this]{pluginSettings.show();};
    pluginBrowser.onOpenFile=[this]{const auto session=browserTrackSession;const int slot=browserEffectSlot;const bool fx=effectsBrowser;pluginFileDialogOpen=true;chooseFile(tr("Open VST3 file..."),"*.vst3",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[this,session,slot,fx](juce::File file){pluginFileDialogOpen=false;const int track=trackForSession(session);if(track>=0&&!loadingPlugin&&!editingRestricted()){if(fx)loadEffectFile(track,file,{},{},slot);else loadPluginFile(track,file);}resumeEffectChain();},[this]{pluginFileDialogOpen=false;resumeEffectChain();});};
    options.onPluginSettings=[this]{pluginSettings.show();};
    options.onAudioSettings=[this]{showAudioSettings();};
    options.onDocument=[this](int id){if(id==0)showGuide(Localisation::current()!=Localisation::Language::italian);if(id==1){const bool it=Localisation::current()==Localisation::Language::italian;showDocument(tr("Privacy and permissions"),juce::String::fromUTF8(it?BinaryData::PRIVACY_it_md:BinaryData::PRIVACY_md,it?BinaryData::PRIVACY_it_mdSize:BinaryData::PRIVACY_mdSize));}if(id==2)showDocument(tr("Licences and attributions"),juce::String::fromUTF8(BinaryData::THIRD_PARTY_NOTICES_md,BinaryData::THIRD_PARTY_NOTICES_mdSize));};
    options.onCheckUpdates=[this]{options.checkUpdatesButton.setEnabled(false);options.checkUpdatesButton.setButtonText(tr("CHECKING..."));updater.start(true);};
    updateOverlay.onInstall=[this](const Updater::Result& result){
        if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}
        juce::Component::SafePointer<MainComponent> safe(this);
        requestClose([safe,result]{
        if(!safe)return;auto& updateOverlay=safe->updateOverlay;
        const auto report=Updater::install(result);
        if(!report.done){safe->message(tr("The files could not be replaced automatically: the download is shown, copy it as the first time."),true);return;}
        updateOverlay.setVisible(false);
        if(report.relaunching){juce::MessageManager::callAsync([]{if(auto* app=juce::JUCEApplicationBase::getInstance())app->quit();});}
        else safe->message(tr("Updated. Close and reopen the app to use the new version."));
        });
    };
    Updater::cleanupAfterUpdate();
    updater.onResult=[this](const Updater::Result& result,bool manual){
        options.checkUpdatesButton.setEnabled(true);options.refreshText();
        if(result.status==Updater::Result::Status::ready){updateOverlay.show(result);
        }else if(manual){
            const auto text=result.status==Updater::Result::Status::upToDate?tr("You have the latest version."):result.status==Updater::Result::Status::noRelease?tr("No published release yet. Development builds are in GitHub Actions."):tr("Could not check for updates. Check your internet connection and try again.");
            message(text);juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,JUCE_APPLICATION_NAME_STRING,text);
        }
    };
    if(Updater::autoCheckEnabled()&&Updater::firstAutoCheckThisSession()&&!juce::JUCEApplication::getCommandLineParameters().contains("smoke-test"))updater.start(false);
    for(auto* s:{&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&gainSlider,&panSlider}){
        auto callback=s->onValueChange;const bool mix=s==&masterSlider||s==&gainSlider||s==&panSlider;trackGesture(*s,(s==&densitySlider||s==&swingSlider||s==&humanizeSlider)?std::function<void()>([this]{flushGlobalFeel();}):std::function<void()>(),mix);
        s->onValueChange=[this,s,callback,mix]{if(!refreshing&&!inGesture(*s)){if(mix)checkpointMix(s);else checkpoint(s);}if(callback)callback();};
    }
    for(auto* button:{&arrangeButton,&harmonyButton})button->setMouseClickGrabsKeyboardFocus(false);
    arrangeButton.setIcon(IconButton::Icon::song);songButton.setIcon(IconButton::Icon::dice);trackButton.setIcon(IconButton::Icon::dice);sectionButton.setIcon(IconButton::Icon::dice);clipButton.setIcon(IconButton::Icon::dice);
    leadButton.setIcon(IconButton::Icon::guitar);instrumentButton.setIcon(IconButton::Icon::gear);editButton.setIcon(IconButton::Icon::pencil);backButton.setIcon(IconButton::Icon::left);addSectionButton.setIcon(IconButton::Icon::song);
    refreshControls();audio.publish(project);setSize(1440,980);select(4,0);showArrangement();fitSong();translateControls(*this);acceptTargetEdit=true;message(juce::String::fromUTF8("Ready - Space: play/pause - Right-click: options - Autosave every minute."));
    if(!juce::JUCEApplication::getCommandLineParameters().contains("test")){recoveryArmed=true;juce::Component::SafePointer<MainComponent> safe(this);juce::MessageManager::callAsync([safe]{if(safe)safe->offerRecovery();});}
    if(!juce::JUCEApplication::getCommandLineParameters().contains("test")){project.enableReturnChains();restoreInstruments();}
    startTimerHz(20);
}
MainComponent::~MainComponent(){
    cancelSampleImport();
    vocalStudioWindow.reset();
    acceptTargetEdit=false;
    if(recoveryArmed)closeRecoverySession(recoveryDirectory());
    if(dismissChordPopover)dismissChordPopover();dismissChordPopover={};
    Localisation::setSetting("inspectorVisible",inspectorVisible?"1":"0");
    Localisation::setSetting("mixerVisible",mixerVisible?"1":"0");
    Localisation::setSetting("arrangementZoom",arrangement.pixelsPerBar);
    Localisation::setSetting("pianoZoom",piano.pixelsPerBeat);
    Localisation::setSetting("appFullScreen",appFullScreen?"1":"0");
exportTask.reset();effectsWindow.reset();if(automationPicker)automationPicker->dismiss();++autosaveRequestId;retiredJobs.retire(std::move(autosaveJob));studioWindow.reset();aiWindow.reset();updater.onResult=nullptr;if(musicalToolWindow)musicalToolWindow->closeButtonPressed();stopTimer();mixerRoutingWindow.reset();studioWindow.reset();instrumentWindow.reset();closePluginEditor();audio.stop();setLookAndFeel(nullptr);}
// A native close may hide a peer before JUCE receives the matching mouse-up.
// Recover only that hidden window, never a real held button or another gesture.

bool MainComponent::verifyAiComposer(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    showAiComposer();bool ok=aiComposeButton.isVisible()&&aiComposeButton.getTooltip().isNotEmpty()&&aiWindow->verify(folder);aiWindow->closeButtonPressed();
    const auto language=Localisation::current();aiWindow.reset();Localisation::setCurrent(Localisation::Language::italian);showAiComposer();const auto itFolder=folder.getChildFile("ai-it");itFolder.createDirectory();ok=aiWindow->verify(itFolder)&&ok;aiWindow->closeButtonPressed();aiWindow.reset();Localisation::setCurrent(language);
    const auto saved=project;AiOptions o;o.tracks={4,leadTrack};o.prompt="6 minute techno";
    const juce::String json=R"({"title":"AI techno","summary":"Six-minute composition","bpm":138,"key":0,"scale":1,"genre":13,"target_seconds":360,"sections":[{"name":"Intro","type":"intro","bars":16},{"name":"Drop","type":"drop","bars":32},{"name":"Outro","type":"outro","bars":16}],"patterns":[{"id":"kick","track":4,"beats":4,"notes":[[0,36,0.5,110],[1,36,0.5,100],[2,36,0.5,110],[3,36,0.5,100]]},{"id":"lead","track":17,"beats":4,"notes":[[0,60,0.5,100],[1,63,0.5,90],[2,67,1,100]]}],"clips":[{"section":0,"pattern":"kick"},{"section":1,"pattern":"kick"},{"section":1,"pattern":"lead"},{"section":2,"pattern":"kick"}]})";
    Project result;juce::String summary;ok=aiCompile(project,o,json,result,summary).wasOk()&&ok;ok=applyAiComposition(saved,result,true).wasOk()&&ok;ok=ok&&audio.isPlaying()&&project.settings.bpm==138&&tempoSlider.getValue()==138&&durationLabel.getText()=="6:00"&&std::abs(project.length()*60/138-360)<.01&&!project.tracks[leadTrack].clips[1].notes.empty();undo(false);ok=ok&&midiFile(project)==midiFile(saved);project.settings.density+=.01;ok=ok&&applyAiComposition(saved,result,false).failed();project=saved;audio.stop();changed();
    folder.getChildFile("ai-composer.txt").replaceWithText(ok?"PASS composer window, provider choices, masked session key, real six-minute MIDI insertion, 138 BPM persistence, playback, Undo and stale-result protection":"FAIL AI composer");return ok;
#endif
}


bool MainComponent::writeLanguageEvidence(const juce::File& folder){
    const auto saved=Localisation::current();const auto originalJson=juce::JSON::toString(projectJson(project),false,17);bool ok=true;static const char* codes[]{"en","it","es","fr","de","pt"};static const char* expected[]{"NEW TRACK","NUOVA TRACCIA","NUEVA PISTA","NOUVELLE PISTE","NEUE SPUR","NOVA FAIXA"};
    for(int i=0;i<6;++i){Localisation::setCurrent((Localisation::Language)i);showArrangement();applyLanguage();refreshControls();ok=tr("NEW TRACK")==juce::String::fromUTF8(expected[i])&&ok;
        ok=genreBox.getSelectedItemIndex()==project.settings.genre&&keyBox.getSelectedItemIndex()==project.settings.key&&scaleBox.getSelectedItemIndex()==project.settings.scale&&extensionBox.getSelectedItemIndex()==project.settings.extension&&voicingBox.getSelectedItemIndex()==project.settings.voicing&&ok;
        ok=juce::JSON::toString(projectJson(project),false,17)==originalJson&&targetDurationLabel.getText()==tr("Target duration")&&targetToleranceLabel.getText()==tr("Tolerance")&&durationLabel.getRight()<=targetDurationLabel.getX()&&targetDurationEditor.getRight()<=targetToleranceLabel.getX()&&targetToleranceEditor.getRight()<=arrangeButton.getX()&&!targetDurationEditor.getBounds().intersects(arrangeButton.getBounds())&&!targetToleranceEditor.getBounds().intersects(arrangeButton.getBounds())&&!targetDurationLabel.getBounds().intersects(durationLabel.getBounds())&&ok;auto picture=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String("language-")+codes[i]+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}else ok=false;}
    Localisation::setCurrent(saved);applyLanguage();select(0,1);showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-performance"},{"RHYTHM / VELOCITY","piano-rhythm"},{"Musical tools","piano-tools"},{"GUIDE SOUND","piano-sound"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto image=instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*output)&&ok;}}
    Localisation::setCurrent(Localisation::Language::italian);applyLanguage();showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-extended-it"},{"RHYTHM / VELOCITY","piano-rhythm-it"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto picture=instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}}
    Localisation::setCurrent(saved);applyLanguage();showInstrument();
    ok=verifyPianoTools(*this,folder)&&ok;
    int tips=0;std::function<void(juce::Component&)> count=[&](auto& c){if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(&c))if(tip->getTooltip().isNotEmpty())++tips;for(auto* child:c.getChildren())count(*child);};count(*this);ok=tips>=50&&ok;
    folder.getChildFile("localisation.txt").replaceWithText(ok?"PASS six interface languages, live translation, extended piano sections and control tooltips":"FAIL localisation or extended controls");showArrangement();return ok;
}
// Every visible icon-only button must explain itself with a tooltip.

bool MainComponent::verifyTargetDurationControl(const juce::File& folder){
    if(!folder.createDirectory())return false;
    showArrangement();const auto saved=Localisation::current();bool ok=true;
    for(int width:{1440,1224}){setSize(width,980);resized();const auto song=durationLabel.getBounds(),label=targetDurationLabel.getBounds(),field=targetDurationEditor.getBounds(),arrange=arrangeButton.getBounds();
        const auto tol=targetToleranceLabel.getBounds(),tolField=targetToleranceEditor.getBounds();ok=ok&&song.getRight()<=label.getX()&&label.getRight()<=field.getX()&&field.getRight()<=tol.getX()&&tol.getRight()<=tolField.getX()&&tolField.getRight()<=arrange.getX()&&!song.intersects(label)&&!field.intersects(tol)&&!tolField.intersects(arrange)&&!tolField.intersects(leadButton.getBounds())&&getLocalBounds().contains(tolField)&&label.getWidth()>=48&&field.getWidth()>=48;}
    targetDurationEditor.setText("3:30",false);commitTargetDuration();ok=ok&&project.settings.targetEnabled&&project.settings.targetSeconds==210&&targetDurationEditor.getText()=="3:30";
    targetDurationEditor.setText("",false);commitTargetDuration();ok=ok&&!project.settings.targetEnabled&&targetDurationEditor.getText().isEmpty();
    project.settings.targetEnabled=true;project.settings.targetSeconds=90;targetDurationEditor.setText("nope",false);commitTargetDuration();ok=ok&&project.settings.targetEnabled&&(int)std::lround(project.settings.targetSeconds)==90&&targetDurationEditor.getText()=="1:30";
    project.settings.targetEnabled=false;project.settings.targetSeconds=0;refreshTargetDurationEditor();
    Localisation::setCurrent(Localisation::Language::italian);applyLanguage();resized();ok=ok&&targetDurationLabel.getText()==juce::String::fromUTF8("Durata obiettivo")&&durationLabel.getRight()<=targetDurationLabel.getX()&&targetDurationEditor.getRight()<=arrangeButton.getX();
    auto image=createComponentSnapshot({0,0,getWidth(),toolbarBottom()+4});if(auto out=folder.getChildFile("target-duration-toolbar.png").createOutputStream())ok=juce::PNGImageFormat().writeImageToStream(image,*out)&&ok;else ok=false;
    Localisation::setCurrent(saved);applyLanguage();setSize(1440,980);folder.getChildFile("target-duration-placement.txt").replaceWithText(ok?"PASS target duration sits after the song length and before NEW ARRANGEMENT":"FAIL target duration placement");return ok;
}
bool MainComponent::writeUiEvidence(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    if(!verifyTargetDurationControl(folder))return false;
    // A synthesised peer press is only attributed to a JUCE component on some window
    // stacks (it depends on which peer owns the pointer), so this regression runs
    // opt-in for a local check and never gates a release by itself.
    if(juce::SystemStats::getEnvironmentVariable("FGA_RUN_NATIVE_INPUT","")!="1")
        folder.getChildFile("window-focus.txt").replaceWithText("SKIP synthetic input regression: a synthesised peer press is not reliably attributed to a JUCE component. Set FGA_RUN_NATIVE_INPUT=1 to run it locally; the recovery code and its guard stay in place.");
    else if(!verifyWindowFocus(folder))return false;
    if(juce::SystemStats::getEnvironmentVariable("FGA_SKIP_EXPORT_UI_TEST","0")=="1"){
        folder.createDirectory();folder.getChildFile("export-ui-skipped.txt").replaceWithText("SKIP previously verified export UI; default CI runs the complete export flow.");
    }else if(!verifyExports(folder.getChildFile("export-ui")))return false;
    showStudio();const bool studioOk=studioWindow->verify(folder);studioWindow->closeButtonPressed();if(!studioOk)return false;
    if(!verifyAuditRepairs(folder))return false;
    {const int section=selectedSection;audio.stop();showChordCirclePopover(0);juce::Component::SafePointer<juce::Component> content;const auto until=juce::Time::getMillisecondCounterHiRes()+1500.;
     while(!content&&juce::Time::getMillisecondCounterHiRes()<until){juce::MessageManager::getInstance()->runDispatchLoopUntil(5);if(auto* box=dynamic_cast<juce::CallOutBox*>(juce::Component::getCurrentlyModalComponent()))content=box->findChildWithID("chord-circle-body");}bool safe=content!=nullptr;const auto history=undoStack.size();
     if(content){selectedSection=(int)project.sections.size()+10;content->resized();for(auto* child:content->getChildren())if(auto* circle=dynamic_cast<CircleOfFifths*>(child))circle->onPick(6,false);safe=safe&&undoStack.size()==history;selectedSection=section;if(dismissChordPopover)dismissChordPopover();if(content)content->resized();}
     selectedSection=section;folder.getChildFile("chord-popover-lifetime.txt").replaceWithText(safe?"PASS invalid section indices are guarded; explicit owner detachment releases audition and makes delayed callbacks inert":"FAIL chord popover lifetime");if(!safe)return false;}

    {
        const auto saved=project;const int track=selectedTrack;select(0,0);showInstrument();bool passed=false;
        for(auto* child:instrumentPanel.getChildren())if(auto* circle=dynamic_cast<CircleOfFifths*>(child)){
            circle->onMoveChord(0,-1,(project.settings.key+1)%12,true);const int chosen=project.settings.freeChords[0];
            passed=project.settings.manualChords[0];for(unsigned seed=1;seed<=20;++seed)randomiseHarmony(project,seed);
            passed=passed&&project.settings.manualChords[0]&&project.settings.freeChords[0]==chosen;
            circle->onMoveChord(0,0,project.settings.key,false);passed=passed&&!project.settings.manualChords[0]&&project.settings.freeChords[0]<0;
        }
        project=saved;select(track,0);changed();undoStack.clear();redoStack.clear();
        folder.getChildFile("manual-chord-ownership.txt").replaceWithText(passed?"PASS circle edits protect custom chromatic chords across repeated dice; choosing a degree releases the override":"FAIL manual circle chord ownership");if(!passed)return false;
    }
    {
        const auto saved=project;const int track=selectedTrack,section=selectedSection;bool passed=true;juce::String report;
        for(int genre:{0,technoGenre,electroHouseGenre}){
            project=newProject(genre);project.sections={{Section::main,8,"Repeated chords"}};project.settings.randomAutomation=false;
            for(auto& t:project.tracks){t.locked=false;t.freeTimeline=false;t.clips.assign(1,{});}project.normalise();select(0,0);changed();
            std::set<juce::String> harmonies;std::set<std::uint64_t> midi;bool roundTrip=true;
            for(int press=0;press<32;++press){
                harmonyButton.onClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
                if(press%8==7){Project loaded;roundTrip=projectFromJson(projectJson(project),loaded).wasOk()&&roundTrip;if(roundTrip){project=std::move(loaded);changed();}}
                if(press>=8){juce::StringArray names;for(const auto& name:chordNames(project.settings))names.add(utf(name));harmonies.insert(names.joinIntoString(" / "));const auto bytes=midiFile(project);std::uint64_t hash=1469598103934665603ULL;for(auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}midi.insert(hash);}
            }
            const bool automatic=std::none_of(project.settings.manualChords.begin(),project.settings.manualChords.end(),[](bool manual){return manual;});
            const bool varied=roundTrip&&automatic&&harmonies.size()>1&&midi.size()>1;passed=passed&&varied;
            report+=(varied?"PASS ":"FAIL ")+juce::String(genre)+": 32 actual New Chords actions with four save/reload cycles; late harmonies="+juce::String((int)harmonies.size())+", MIDI="+juce::String((int)midi.size())+"\n";
        }
        project=saved;select(track,section);changed();undoStack.clear();redoStack.clear();folder.getChildFile("repeated-new-chords.txt").replaceWithText(report);if(!passed)return false;
    }
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool previousDirty=dirty;const int track=selectedTrack,section=selectedSection,selected=selectedClip;const bool noSelection=noClipSelection;const double cursor=editCursor;
        for(auto& t:project.tracks){t.locked=false;for(auto& c:t.clips)c.locked=false;}
        for(auto& c:project.tracks[0].clips){c.notes={{.5,.75,123,117,0}};c.loopLength=0;c.loopSource.clear();}
        const auto before=juce::JSON::toString(projectJson(project),false,17);const auto count=undoStack.size();arrangeButton.onClick();
        bool passed=undoStack.size()==std::min((size_t)40,count+1);
        for(const auto& clip:project.tracks[0].clips)for(const auto& note:clip.notes)passed=passed&&note.pitch==123&&note.velocity==117&&note.length==.75;
        undo(false);passed=passed&&before==juce::JSON::toString(projectJson(project),false,17);
        project=saved;undoStack=undos;redoStack=redos;dirty=previousDirty;selectedTrack=track;selectedSection=section;selectedClip=selected;noClipSelection=noSelection;editCursor=cursor;audio.publish(project);refreshControls();
        folder.getChildFile("new-arrangement.txt").replaceWithText(passed?"PASS NEW ARRANGEMENT button reuses MIDI; one-step Undo restores the complete project":"FAIL NEW ARRANGEMENT command or Undo");if(!passed)return false;
    }
    {
        const auto savedProject=project;const auto savedUndo=undoStack,savedRedo=redoStack;const int oldTrack=selectedTrack,oldSection=selectedSection;const bool oldDirty=dirty;
        addMidiTrack(true);const int vocal=selectedTrack;auto initial=project.tracks[(size_t)vocal].clips;
        bool vocalOk=project.tracks[(size_t)vocal].vocalChops&&trackButton.isEnabled()&&clipButton.isEnabled()&&regenerateClipButton.isEnabled();
        regenerate(Scope::track);vocalOk=vocalOk&&project.tracks[(size_t)vocal].clips!=initial;undo(false);vocalOk=vocalOk&&project.tracks[(size_t)vocal].clips==initial;
        showInstrument();const auto names=instrumentPanel.controlNames();vocalOk=vocalOk&&names.contains("DENSITY")&&names.contains("GATE")&&names.contains("HUMANIZE")&&names.contains("STUDIO")&&!names.contains("Voicing")&&instrumentPanel.layoutProblems().isEmpty();
        for(auto* child:instrumentPanel.getChildren())if(child->getProperties()["fgaControl"].toString()=="DENSITY")if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(child))vocalOk=vocalOk&&tip->getTooltip()==tr("Number of short vocal triggers per phrase; zero leaves silence.");
        loadSampleFile(vocal,folder.getChildFile("drop-routing.wav"));waitForSampleImportForTests();showInstrument();const auto& part=project.tracks[(size_t)vocal];vocalOk=vocalOk&&part.instrument=="sample"&&part.sampler.mode==2&&part.sampler.sliceCount>0&&instrumentPanel.layoutProblems().isEmpty();
        for(const auto& c:part.clips)for(const auto& n:c.notes)vocalOk=vocalOk&&n.pitch>=part.sampler.sliceBase&&n.pitch<part.sampler.sliceBase+part.sampler.sliceCount;
        if(auto output=folder.getChildFile("vocal-chops.png").createOutputStream()){juce::PNGImageFormat png;vocalOk=png.writeImageToStream(instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds()),*output)&&vocalOk;}
        auto sound=project;sound.sections={{Section::main,4,"Vocal test"}};for(auto& t:sound.tracks){t.freeTimeline=false;t.clips.resize(1);}auto& voice=sound.tracks[(size_t)vocal];voice.clips[0].notes={{.5,.25,voice.sampler.sliceBase,100,0},{2.5,.25,voice.sampler.sliceBase+voice.sampler.sliceCount-1,100,0}};
        const auto audioFile=folder.getChildFile("vocal-chops.wav");vocalOk=audio.exportAudio(sound,audioFile,vocal,false,[]{return false;},[](double){}).wasOk()&&vocalOk;
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(audioFile.createInputStream().release(),true));
        if(reader&&reader->lengthInSamples>0&&reader->lengthInSamples<44100*30){juce::AudioBuffer<float> data((int)reader->numChannels,(int)reader->lengthInSamples);reader->read(&data,0,data.getNumSamples(),0,true,true);vocalOk=data.getMagnitude(0,data.getNumSamples())>.001f&&vocalOk;}else vocalOk=false;
        deleteMidiTrack(vocal);undo(false);vocalOk=vocalOk&&project.tracks[(size_t)vocal].vocalChops&&project.tracks[(size_t)vocal].instrument=="sample";
        folder.getChildFile("vocal-chops.txt").replaceWithText(vocalOk?"PASS optional vocal generator, live controls, regeneration and Undo, sampled slice mapping, panel layout and deletion recovery":"FAIL vocal chops workflow");
        project=savedProject;undoStack=savedUndo;redoStack=savedRedo;selectedTrack=oldTrack;selectedSection=oldSection;selectedClip=-1;restoreInstruments();changed();dirty=oldDirty;if(instrumentWindow)instrumentWindow->closeButtonPressed();showArrangement();
        if(!vocalOk)return false;
    }
    if(auto* busButton=dynamic_cast<juce::Button*>(mixer.findChildWithID("mixer-routing"))){busButton->onClick();if(!mixerRoutingWindow||!mixerRoutingWindow->verify(folder))return false;mixerRoutingWindow->closeButtonPressed();}else return false;
    if(!mixer.verifySends(folder)||!mixer.verifyAutomation(folder))return false;
    juce::String fail;
    auto mark=[&](const char* step,bool value){if(!value&&fail.isEmpty())fail=step;return value;};
    bool ok=mark("language",writeLanguageEvidence(folder)&&writeHelpEvidence(folder));
    for(int key=0;key<12;++key){keyBox.setSelectedId(key+1,juce::dontSendNotification);const auto text=keyBox.getText();const auto width=juce::GlyphArrangement::getStringWidth(theme.getComboBoxFont(keyBox),text);ok=mark("key-name-visible",width+8<=keyBox.getWidth()-20)&&ok;}
    keyBox.setSelectedId(project.settings.key+1,juce::dontSendNotification);
    {const auto settings=project.settings;for(const auto& key:std::array<std::pair<int,int>,4>{{{6,0},{3,2},{1,0},{1,1}}}){project.settings.key=key.first;project.settings.scale=key.second;refreshControls();ok=mark("key-spelling",keyBox.getText()==exportKey(project))&&ok;}project.settings=settings;refreshControls();}
    pluginSettings.show();ok=mark("plugin-settings",pluginSettings.verifyLayout())&&ok;
    if(pluginSettings.onReport){pluginSettings.onReport();auto* dialog=dynamic_cast<juce::DialogWindow*>(juce::Component::getCurrentlyModalComponent());ok=mark("plugin-scan-report",dialog&&dialog->isVisible()&&dialog->isAlwaysOnTop())&&ok;if(dialog)dialog->closeButtonPressed();}else ok=false;
    pluginSettings.setVisible(false);auto capture=[&](const juce::String& name){auto screenshot=(name.startsWith("instrument-")||name=="guitar-mixer"||name=="piano-family-extended")?instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds()):createComponentSnapshot(getLocalBounds());bool wrote=false;if(auto out=folder.getChildFile(name+".png").createOutputStream()){juce::PNGImageFormat png;wrote=png.writeImageToStream(screenshot,*out);}ok=mark(name=="plugin-browser"?"png-browser":name=="plugin-settings"?"png-settings":"png",wrote)&&ok;};
    // Context-dependent panels must not expose controls ignored by the renderer.
    {
        const auto savedProject=project;
        auto expect=[&](const char* step,bool value,const juce::StringArray& names){if(!value&&fail.isEmpty())fail=juce::String(step)+": "+names.joinIntoString(" | ");ok=value&&ok;};
        select(1,1);project.settings.bassMode=sgk::bassModeForLine(8);showInstrument();
        auto names=instrumentPanel.controlNames();expect("bass-line",!names.contains("RND BASS")&&!names.contains("GROOVE")&&!names.contains("A/B")&&names.contains("BASS LINE"),names);
        project.settings.bassMode=(int)sgk::BassMode::root;showInstrument();names=instrumentPanel.controlNames();expect("bass-root",names.contains("RND BASS")&&names.contains("GROOVE"),names);
        select(2,1);project.settings.stringsMode=(int)sgk::StringsMode::pad;showInstrument();names=instrumentPanel.controlNames();expect("strings-pad",!names.contains("RND PIANO")&&!names.contains("NEW FEEL")&&names.contains("STRINGS STYLE"),names);
        project.settings.stringsMode=(int)sgk::StringsMode::stabs;showInstrument();names=instrumentPanel.controlNames();expect("strings-stabs",names.contains("RND PIANO")&&names.contains("GROOVE"),names);
        select(4,1);showInstrument();names=instrumentPanel.controlNames();expect("kick",!names.contains("LEVEL")&&!names.contains("Drum volume")&&!names.contains("Mute")&&names.contains("MIDI NOTE")&&names.contains("VELOCITY")&&!names.contains("Crash after fill")&&names.contains("Bass space"),names);
        select(5,1);showInstrument();names=instrumentPanel.controlNames();expect("snare",!names.contains("Bass space")&&!names.contains("Crash after fill"),names);
        select(16,1);showInstrument();names=instrumentPanel.controlNames();expect("crash",names.contains("Crash after fill")&&!names.contains("Bass space"),names);
        select(leadTrack,1);showInstrument();names=instrumentPanel.controlNames();expect("lead",names.contains("Lead part")&&names.contains("ATTACK DELAY")&&names.contains("Resolve after fill")&&!names.contains("MIDI NOTE")&&!names.contains("SOLISTA"),names);
        project.tracks[leadTrack].instrument="sample";showInstrument();names=instrumentPanel.controlNames();expect("lead-sample",!names.contains("TONE")&&!names.contains("REVERB")&&!names.contains("Part volume")&&names.contains("Part level"),names);
        ok=mark("lead-external-panel",instrumentPanel.layoutProblems().isEmpty())&&ok;project.tracks[leadTrack].instrument="internal";
        select(4,1);
        // Track gain stays on the arranger strip. A sample hides the guide tone and reverb.
        project.tracks[4].instrument="sample";showInstrument();names=instrumentPanel.controlNames();expect("sample",!names.contains("TONE")&&!names.contains("REVERB")&&!names.contains("VOLUME"),names);
        project=savedProject;changed();
    }
    pluginSettings.show();capture("plugin-settings");pluginSettings.setVisible(false);
    pluginBrowser.show(0,juce::String("fg")+"\x1f"+"0");ok=mark("plugin-browser",pluginBrowser.verifyLayout())&&ok;capture("plugin-browser");pluginBrowser.setVisible(false);
    select(0,1);showInstrument();capture("piano-family-extended");const auto take=clip().seed;regenerate(Scope::clip);ok=mark("regenerate",clip().seed!=take&&clip().motifSeed!=0)&&ok;const auto generated=clip();if(instrumentWindow)instrumentWindow->body->applyClip.onClick();ok=mark("apply-clip",clip()==generated)&&ok;
    showArrangement();fitSong();ok=mark("fit-song",arrangementPort.isVisible()&&!pianoPort.isVisible()&&headerWidth+project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth())&&ok;capture("interface");
    ok=mark("automation",arrangement.verifyAutomation(folder))&&ok;
    ok=mark("ai-composer",verifyAiComposer(folder))&&ok;
    capture("daw-workspace");ok=mark("daw-workspace",verifyDawWorkspace(folder))&&ok;ok=mark("timeline",verifyTimelineEditing(folder))&&ok;ok=mark("navigation",verifyNavigation(folder))&&ok;ok=mark("sample-browser",verifySampleBrowser(folder))&&ok;
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;
        auto expected=project;expected.settings.key=(project.settings.key+2)%12;applyInstrumentChange(expected,0,true);
        undoStack.clear();redoStack.clear();keyBox.setSelectedId(expected.settings.key+1,juce::sendNotificationSync);
        const bool applied=project.settings.key==expected.settings.key&&juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(expected),false,17)&&undoStack.size()==1;
        undo(false);const bool restored=project.settings.key==saved.settings.key&&juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(saved),false,17);
        ok=mark("key-change-regenerates",applied&&restored)&&ok;
        folder.getChildFile("key-change.txt").replaceWithText(applied&&restored?"PASS header key change regenerates harmonic MIDI with current chords, preserves other tracks and supports one-step Undo":"FAIL header key change or Undo");
        project=saved;undoStack=undos;redoStack=redos;dirty=wasDirty;audio.publish(project);refreshControls();
    }
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;
        project=newProject(0);project.settings.key=0;project.settings.scale=0;generate(project);changed();
        bool modesOk=true;juce::String report;
        for(int mode:{2,0,1}){
            const auto before=project;auto expected=project;expected.settings.scale=mode;applyInstrumentChange(expected,0,true);
            undoStack.clear();redoStack.clear();scaleBox.setSelectedId(mode+1,juce::sendNotificationSync);
            bool notesChanged=false;for(int ti:{0,1,2,3,leadTrack})for(size_t ci=0;ci<project.tracks[(size_t)ti].clips.size();++ci)notesChanged=notesChanged||project.tracks[(size_t)ti].clips[ci].notes!=before.tracks[(size_t)ti].clips[ci].notes;
            const bool applied=notesChanged&&undoStack.size()==1&&juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(expected),false,17);
            undo(false);const bool restored=juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(before),false,17);
            redoButton.onClick();const bool redone=juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(expected),false,17);
            modesOk=applied&&restored&&redone&&modesOk;report+=juce::String(applied&&restored&&redone?"PASS ":"FAIL ")+sgk::scaleName(mode)+" regenerates harmonic MIDI, Undo and Redo\n";
        }
        ok=mark("scale-change-regenerates",modesOk)&&ok;folder.getChildFile("scale-change.txt").replaceWithText(report);
        project=saved;undoStack=undos;redoStack=redos;dirty=wasDirty;audio.publish(project);refreshControls();
    }
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;const int oldTrack=selectedTrack,oldSection=selectedSection;const double oldTimelineZoom=arrangement.pixelsPerBar,oldPianoZoom=piano.pixelsPerBeat;const auto oldScroll=arrangementPort.getViewPosition();
        auto fixture=[&]{audio.stop();project=Project{};project.normalise();applyGenre(project,0);project.sections={{Section::main,4,"Controls"}};for(auto& t:project.tracks)t.clips.assign(1,{});project.normalise();generate(project);selectedClip=0;noClipSelection=false;selectionBeat=1./1920.;project.tracks[1].locked=true;project.tracks[2].clips[0].locked=true;restoreInstruments();select(0,0);changed();};
        auto json=[&]{return juce::JSON::toString(projectJson(project),false,17);};
        bool feelOk=true;juce::String report;juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;
        auto tick=[&]{audio.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);};
        for(auto* control:{&densitySlider,&swingSlider,&humanizeSlider}){
            fixture();control->setValue(0,juce::sendNotificationSync);const auto before=project;const auto baseline=json();undoStack.clear();redoStack.clear();
            audio.play(true);tick();control->setValue(control==&swingSlider?120:95,juce::sendNotificationSync);tick();
            const auto after=json();const bool midiChanged=project.tracks[0].clips[0].notes!=before.tracks[0].clips[0].notes,locksKept=project.tracks[1].clips==before.tracks[1].clips&&project.tracks[2].clips[0]==before.tracks[2].clips[0];const auto history=undoStack.size();const bool stillPlaying=audio.isPlaying();const bool applied=midiChanged&&locksKept&&history==1&&stillPlaying;
            audio.stop();tick();undo(false);const bool restored=json()==baseline;if(!restored){folder.getChildFile(control->getComponentID()+"-before.json").replaceWithText(baseline);folder.getChildFile(control->getComponentID()+"-undo.json").replaceWithText(json());}undo(true);const bool redone=json()==after;
            report+="DETAIL "+control->getComponentID()+" MIDI="+juce::String((int)midiChanged)+" locks="+juce::String((int)locksKept)+" checkpoints="+juce::String((int)history)+" playing="+juce::String((int)stillPlaying)+" undo="+juce::String((int)restored)+" redo="+juce::String((int)redone)+"\n";
            feelOk=feelOk&&applied&&restored&&redone;report+=juce::String(applied&&restored&&redone?"PASS ":"FAIL ")+control->getComponentID()+" changes piano MIDI during Play, preserves locks, Undo/Redo\n";
        }
        fixture();const auto beforeDrag=json();undoStack.clear();redoStack.clear();densitySlider.onDragStart();densitySlider.setValue(25,juce::sendNotificationSync);densitySlider.setValue(85,juce::sendNotificationSync);const bool queued=pendingGlobalFeel!=0&&undoStack.size()==1;densitySlider.onDragEnd();const auto dragged=json();undo(false);const bool gesture=queued&&pendingGlobalFeel==0&&json()==beforeDrag;undo(true);feelOk=feelOk&&gesture&&json()==dragged;report+=juce::String(gesture?"PASS ":"FAIL ")+"coalesced drag applies final value and keeps one Undo\n";
        for(auto scope:{Scope::song,Scope::section,Scope::track,Scope::clip}){
            fixture();project.tracks[1].locked=false;project.tracks[2].clips[0].locked=false;changed();const auto before=project.settings;regenerate(scope,true);
            const bool moved=std::lround(before.density*100)!=std::lround(densitySlider.getValue())&&std::lround(before.swing*100)!=std::lround(swingSlider.getValue())&&std::lround(before.humanize*100)!=std::lround(humanizeSlider.getValue());
            const bool synced=std::abs(densitySlider.getValue()-project.settings.density*100)<.01&&std::abs(swingSlider.getValue()-project.settings.swing*100)<.01&&std::abs(humanizeSlider.getValue()-project.settings.humanize*100)<.01;
            feelOk=feelOk&&moved&&synced;report+=juce::String(moved&&synced?"PASS ":"FAIL ")+"fresh regeneration updates the three visible knob positions, scope="+juce::String((int)scope)+"\n";
        }
        fixture();const auto harmonyBefore=project;undoStack.clear();redoStack.clear();const int degree=(project.settings.degrees[0]+1)%7;applyChordMenuChoice(1000+degree);
        const bool chordChanged=project.settings.degrees[0]==degree&&project.tracks[0].clips[0].notes!=harmonyBefore.tracks[0].clips[0].notes&&undoStack.size()==1;
        undo(false);const bool chordUndo=project.settings.degrees==harmonyBefore.settings.degrees&&project.tracks[0].clips==harmonyBefore.tracks[0].clips;
        feelOk=feelOk&&chordChanged&&chordUndo;report+=juce::String(chordChanged&&chordUndo?"PASS ":"FAIL ")+"classic chord menu updates MIDI and supports Undo\n";
        ok=mark("global-feel-updates-midi",feelOk)&&ok;folder.getChildFile("global-feel.txt").replaceWithText(report);
        project=saved;undoStack=undos;redoStack=redos;dirty=wasDirty;restoreInstruments();arrangement.pixelsPerBar=oldTimelineZoom;piano.pixelsPerBeat=oldPianoZoom;select(oldTrack,oldSection);refreshControls();updateSizes();arrangementPort.setViewPosition(oldScroll);
    }
    {
        auto sameImage=[](const juce::Image& a,const juce::Image& b){if(a.getBounds()!=b.getBounds())return false;for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))return false;return true;};
        showArrangement();grabKeyboardFocus();const auto plain=mixerToggle.createComponentSnapshot(mixerToggle.getLocalBounds());
        mixerToggle.grabKeyboardFocus();const bool noStickyFocus=sameImage(plain,mixerToggle.createComponentSnapshot(mixerToggle.getLocalBounds()));grabKeyboardFocus();
        ok=mark("button-focus-feedback",noStickyFocus)&&ok;
        std::function<juce::Component*(juce::Component&)> findClose=[&](juce::Component& root)->juce::Component*{if(root.getComponentID()=="search-close")return &root;for(auto* child:root.getChildren())if(auto* found=findClose(*child))return found;return nullptr;};
        showSpotlightSearch();juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
        juce::Button* close=nullptr;for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* found=findClose(*juce::Desktop::getInstance().getComponent(i)))close=dynamic_cast<juce::Button*>(found);
        bool searchOk=close&&close->isShowing();
        if(close){juce::Component::SafePointer<juce::Component> popup(close->findParentComponentOfClass<juce::CallOutBox>());if(popup){if(auto out=folder.getChildFile("quick-search.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds()),*out);}}close->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(120);searchOk=searchOk&&(!popup||!popup->isShowing());}
        ok=mark("search-close-button",searchOk)&&ok;
        folder.getChildFile("new-ui-controls.txt").replaceWithText(juce::String(noStickyFocus?"PASS":"FAIL")+" no persistent focus border after button activation\n"+(searchOk?"PASS":"FAIL")+" Search opens and closes using X\n");
    }
    juce::String layoutReport,editingReport;
    for(int track:{4,16,1,0,2,3,leadTrack}){select(track,1);const auto mainPage=page;showInstrument();auto problems=instrumentPanel.layoutProblems();layoutReport+=juce::String(track)+": "+(problems.isEmpty()?"PASS":problems)+"\n";ok=mark("panel",problems.isEmpty())&&ok;ok=mark("instrument-page",instrumentIsOpen()&&instrumentPort.getTopLevelComponent()!=getTopLevelComponent()&&page==mainPage&&sectionSelector.getSelectedId()==selectedSection+1)&&ok;capture("instrument-"+juce::String(track));if(track==3){instrumentPort.setViewPosition(0,instrumentPanel.getHeight());capture("guitar-mixer");instrumentPort.setViewPosition(0,0);}showEditor();ok=mark("editor-page",pianoPort.getHeight()>400&&!arrangementPort.isVisible())&&ok;if(isDrumTrack(track))ok=mark("drum-lanes",piano.laneView())&&ok;if(track==1||track==2||track==leadTrack)ok=mark("pitched-lanes",!piano.laneView())&&ok;capture("editor-"+juce::String(track));ok=mark("scale-guide-visibility",scaleGuideButton.isVisible()==piano.scaleGuideAvailable())&&ok;if(track==0){const bool guide=piano.showScaleGuide;piano.showScaleGuide=true;scaleGuideButton.setToggleState(true,juce::dontSendNotification);capture("piano-scale-guide");piano.showScaleGuide=guide;scaleGuideButton.setToggleState(guide,juce::dontSendNotification);}const bool edits=piano.verifyEditing();ok=mark("editing",edits)&&ok;if(track==0){const auto json=juce::JSON::toString(projectJson(project),false,17);const auto undos=undoStack.size();pianoPlayButton.onClick();bool clipPlay=audio.isClipPlaying();juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;audio.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);clipPlay=clipPlay&&audio.position()>=clipStart()&&audio.position()<clipStart()+clipLength();pianoPlayButton.onClick();clipPlay=clipPlay&&!audio.isPlaying()&&json==juce::JSON::toString(projectJson(project),false,17)&&undos==undoStack.size();ok=mark("piano-clip-play",clipPlay)&&ok;folder.getChildFile("piano-clip-play.txt").replaceWithText(clipPlay?"PASS editor Play/Stop clip button uses isolated engine loop, leaves project and Undo unchanged":"FAIL piano clip playback");}editingReport+=juce::String(track)+": "+(edits?"PASS add, move with per-pitch audition, velocity, resize, delete, precise ruler seek and scale guide":"FAIL editing gestures")+"\n";}
    select(3,1);piano.guitarNotes=true;showEditor();const bool guitarEdits=piano.verifyEditing();ok=mark("guitar-note-editing",guitarEdits&&scaleGuideButton.isVisible())&&ok;editingReport+="guitar notes: "+juce::String(guitarEdits?"PASS drag audition and scale guide":"FAIL editing gestures")+"\n";piano.guitarNotes=false;
    select(leadTrack,1);showArrangement();
    if(auto output=folder.getChildFile("inspector.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(trackInspector.createComponentSnapshot(trackInspector.getLocalBounds()),*output);}
    capture("arrangement-lead");
    folder.getChildFile("panel-layout.txt").replaceWithText(layoutReport);folder.getChildFile("midi-editing.txt").replaceWithText(editingReport);
    select(3,1);piano.guitarNotes=true;showEditor();ok=mark("guitar-lanes",!piano.laneView())&&ok;capture("guitar-notes");ok=mark("guitar-notes",piano.verifyEditing())&&ok;piano.guitarNotes=false;showArrangement();select(4,0);
    {juce::StringArray missing;showArrangement();showSampleBrowser(true);missing.addArray(iconButtonsWithoutTips());showSampleBrowser(false);for(int track:{0,3,4,leadTrack}){select(track,1);showInstrument();missing.addArray(iconButtonsWithoutTips());showEditor();missing.addArray(iconButtonsWithoutTips());}showArrangement();select(4,0);
        if(!missing.isEmpty())fail="icon buttons without a tooltip: "+missing.joinIntoString(", ");ok=mark("icon-tips",missing.isEmpty())&&ok;}
    ok=mark("presentation",verifyPresentation(folder))&&ok;
    folder.getChildFile("ui.txt").replaceWithText(ok?"PASS separate arrangement, instrument panels, full MIDI editors, drum isolation, guitar lanes/notes, instrument regeneration creates a new take":"FAIL interface routing: "+fail);return ok;
#endif
}

}
