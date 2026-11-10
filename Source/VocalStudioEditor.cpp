// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "ProjectIO.h"
#include <cstdlib>
namespace fga {
// Modeless: notes remain editable in the existing piano roll while this panel is open.
class VocalStudioEditor final:public juce::Component,private juce::Timer {
public:
    VocalStudioEditor(MainComponent& main,const std::string& id):owner(&main),clipId(id){
        for(auto* label:{&heading,&lyricsLabel,&syllableLabel,&phonemesLabel,&regionLabel,&modelLabel,&status,&links}){addAndMakeVisible(label);label->setColour(juce::Label::textColourId,juce::Colours::white);}
        lyricsLabel.setText("English lyrics (draft)",juce::dontSendNotification);
        syllableLabel.setText("Syllable / word",juce::dontSendNotification);
        phonemesLabel.setText("Pronunciation override (draft; support unverified)",juce::dontSendNotification);
        regionLabel.setText("Vocal region: start / end in beats within this clip",juce::dontSendNotification);
        modelLabel.setText("Local voicebank folder (optional; no model is loaded)",juce::dontSendNotification);
        for(auto* editor:{&lyrics,&text,&phonemes,&regionStart,&regionEnd,&modelPath})addAndMakeVisible(editor);
        lyrics.setMultiLine(true);lyrics.setReturnKeyStartsNewLine(true);lyrics.setInputRestrictions(65536);
        text.setInputRestrictions(256);phonemes.setInputRestrictions(512);modelPath.setInputRestrictions(4096);
        for(auto* box:{&syllables,&voices})addAndMakeVisible(box);
        voices.addItem("No voice selected",1);voices.addItem("Qixuan (candidate only)",2);
        addAndMakeVisible(allowIntroOutro);allowIntroOutro.setButtonText("Allow singing in DJ intro / outro (explicit override)");
        for(auto* button:{&apply,&add,&remove,&assign,&rest,&focus})addAndMakeVisible(button);
        apply.setButtonText("Apply text / settings");add.setButtonText("Add syllable");remove.setButtonText("Delete syllable");
        assign.setButtonText("Assign selected roll notes");rest.setButtonText("Rest / unassign");focus.setButtonText("Show clip in piano roll");
        apply.onClick=[this]{applyDraft();};add.onClick=[this]{addSyllable();};remove.onClick=[this]{deleteSyllable();};
        assign.onClick=[this]{assignSelection(false);};rest.onClick=[this]{assignSelection(true);};
        focus.onClick=[this]{int t,c;if(locate(t,c)){owner->selectClip(t,c,true);owner->showEditor();}};
        syllables.onChange=[this]{loadSyllable();};
        setSize(700,620);refresh();startTimerHz(4);
    }
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff20242c));}
    void resized()override{
        auto area=getLocalBounds().reduced(16);
        heading.setBounds(area.removeFromTop(28));
        lyricsLabel.setBounds(area.removeFromTop(24));lyrics.setBounds(area.removeFromTop(75));area.removeFromTop(8);
        auto row=area.removeFromTop(28);syllables.setBounds(row.removeFromLeft(290));row.removeFromLeft(8);add.setBounds(row.removeFromLeft(160));row.removeFromLeft(8);remove.setBounds(row);
        syllableLabel.setBounds(area.removeFromTop(24));text.setBounds(area.removeFromTop(28));
        phonemesLabel.setBounds(area.removeFromTop(24));phonemes.setBounds(area.removeFromTop(28));
        links.setBounds(area.removeFromTop(26));row=area.removeFromTop(28);assign.setBounds(row.removeFromLeft(250));row.removeFromLeft(8);rest.setBounds(row.removeFromLeft(160));
        regionLabel.setBounds(area.removeFromTop(24));row=area.removeFromTop(28);regionStart.setBounds(row.removeFromLeft(105));row.removeFromLeft(8);regionEnd.setBounds(row.removeFromLeft(105));row.removeFromLeft(12);allowIntroOutro.setBounds(row);
        area.removeFromTop(8);voices.setBounds(area.removeFromTop(28));modelLabel.setBounds(area.removeFromTop(24));modelPath.setBounds(area.removeFromTop(28));
        row=area.removeFromBottom(30);apply.setBounds(row.removeFromLeft(230));row.removeFromLeft(8);focus.setBounds(row.removeFromLeft(230));status.setBounds(area);
    }
private:
    juce::Component::SafePointer<MainComponent> owner;
    std::string clipId;
    std::optional<VocalClip> displayed;
    juce::Label heading,lyricsLabel,syllableLabel,phonemesLabel,regionLabel,modelLabel,status,links;
    juce::TextEditor lyrics,text,phonemes,regionStart,regionEnd,modelPath;
    juce::ComboBox syllables,voices;
    juce::ToggleButton allowIntroOutro;
    juce::TextButton apply,add,remove,assign,rest,focus;
    bool locate(int& track,int& index)const{
        if(!owner)return false;
        for(size_t t=0;t<owner->project.tracks.size();++t)for(size_t c=0;c<owner->project.tracks[t].clips.size();++c){const auto& clip=owner->project.tracks[t].clips[c];if(clip.vocal&&clip.vocal->id==clipId){track=(int)t;index=(int)c;return true;}}
        return false;
    }
    Clip* current(){int t,c;return locate(t,c)?&owner->project.tracks[(size_t)t].clips[(size_t)c]:nullptr;}
    bool editable(){return owner&&!owner->loadingPlugin&&!owner->editingRestricted()&&current();}
    void commit(Clip next){
        auto* clip=current();if(!editable()||!clip||next==*clip)return;
        owner->checkpoint();*clip=std::move(next);owner->changed(false);refresh();
    }
    void refresh(){
        int t,c;if(!locate(t,c)){setEnabled(false);status.setText("Vocal clip is no longer in this project. Close and reopen Vocal Studio.",juce::dontSendNotification);return;}
        setEnabled(true);const auto& clip=owner->project.tracks[(size_t)t].clips[(size_t)c];const auto& v=*clip.vocal;
        heading.setText("Vocal Studio / "+juce::String::fromUTF8(owner->project.tracks[(size_t)t].name.c_str())+" / beat "+juce::String(owner->project.clipStart(t,c),2),juce::dontSendNotification);
        lyrics.setText(juce::String::fromUTF8(v.lyrics.c_str()),false);regionStart.setText(juce::String(v.regionStart,4),false);regionEnd.setText(juce::String(v.regionEnd,4),false);
        modelPath.setText(juce::String::fromUTF8(v.modelPath.c_str()),false);voices.setSelectedId(v.voice.empty()?1:2,juce::dontSendNotification);allowIntroOutro.setToggleState(v.allowIntroOutro,juce::dontSendNotification);
        const int selected=syllables.getSelectedId();syllables.clear(juce::dontSendNotification);
        for(const auto& syllable:v.syllables)syllables.addItem(juce::String::fromUTF8(syllable.text.c_str()).isEmpty()?"(empty syllable)":juce::String::fromUTF8(syllable.text.c_str()),syllable.id);
        if(std::any_of(v.syllables.begin(),v.syllables.end(),[&](const auto& s){return s.id==selected;}))syllables.setSelectedId(selected,juce::dontSendNotification);
        else if(!v.syllables.empty())syllables.setSelectedId(v.syllables.front().id,juce::dontSendNotification);
        displayed=v;loadSyllable();
        juce::String info="English only. MIDI guide only; singing render is not implemented. ";
        if(v.voice.empty())info+="No voice selected.";
        else if(v.modelPath.empty()||!juce::File::isAbsolutePath(juce::String::fromUTF8(v.modelPath.c_str()))||!juce::File(juce::String::fromUTF8(v.modelPath.c_str())).isDirectory())info+="Voicebank missing. Qixuan license and capabilities unverified.";
        else if(!juce::File(juce::String::fromUTF8(v.modelPath.c_str())).getChildFile("dsconfig.yaml").existsAsFile())info+="Model configuration missing (dsconfig.yaml). No model loaded.";
        else info+="Folder found; model compatibility, license and capabilities unverified. No model loaded.";
        status.setText(info,juce::dontSendNotification);
    }
    void timerCallback()override{
        auto* clip=current();if(!clip){refresh();return;}
        if(!isEnabled()||!displayed||!(*displayed==*clip->vocal))refresh();
        const bool canEdit=editable();for(auto* b:{&apply,&add,&remove,&assign,&rest})b->setEnabled(canEdit);
    }
    void loadSyllable(){
        auto* clip=current();if(!clip)return;
        const auto& entries=clip->vocal->syllables;const int id=syllables.getSelectedId();
        auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& s){return s.id==id;});
        text.setText(found==entries.end()?juce::String():juce::String::fromUTF8(found->text.c_str()),false);
        phonemes.setText(found==entries.end()?juce::String():juce::String::fromUTF8(found->phonemes.c_str()),false);
        links.setText(found==entries.end()?"Add a syllable, then select notes in the piano roll.":found->noteIds.empty()?"Rest: no notes assigned. Unassigned notes do not sing.":juce::String((int)found->noteIds.size())+" note(s) assigned; multiple notes form a melisma.",juce::dontSendNotification);
    }
    static bool number(const juce::String& input,double& value){const auto s=input.trim().toStdString();char* end=nullptr;value=std::strtod(s.c_str(),&end);return !s.empty()&&end==s.c_str()+s.size()&&std::isfinite(value);}
    bool applyDraft(){
        if(!editable())return false;
        int t,c;if(!locate(t,c))return false;Clip next=*current();auto& v=*next.vocal;double start,end;
        if(!number(regionStart.getText(),start)||!number(regionEnd.getText(),end)||start<0||end<start||end>owner->project.clipLength(t,c)){status.setText("Invalid region: use 0 <= start <= end <= clip length.",juce::dontSendNotification);return false;}
        auto raw=lyrics.getText().toStdString(),word=text.getText().toStdString(),pronunciation=phonemes.getText().toStdString(),path=modelPath.getText().trim().toStdString();
        if(raw.size()>65536||word.size()>256||pronunciation.size()>512||path.size()>4096){status.setText("Text exceeds the supported size.",juce::dontSendNotification);return false;}
        v.lyrics=std::move(raw);v.regionStart=start;v.regionEnd=end;v.allowIntroOutro=allowIntroOutro.getToggleState();v.voice=voices.getSelectedId()==2?"qixuan-candidate":"";v.modelPath=std::move(path);
        for(auto& entry:v.syllables)if(entry.id==syllables.getSelectedId()){entry.text=std::move(word);entry.phonemes=std::move(pronunciation);}
        commit(std::move(next));return true;
    }
    void addSyllable(){
        if(!applyDraft())return;
        Clip next=*current();auto& v=*next.vocal;if(v.syllables.size()>=4096)return;
        VocalSyllable entry;entry.id=v.nextSyllableId++;entry.text="syllable";const int id=entry.id;v.syllables.push_back(std::move(entry));commit(std::move(next));syllables.setSelectedId(id,juce::sendNotificationSync);
    }
    void deleteSyllable(){if(!applyDraft())return;Clip next=*current();auto& entries=next.vocal->syllables;const int id=syllables.getSelectedId();entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& s){return s.id==id;}),entries.end());commit(std::move(next));}
    void assignSelection(bool asRest){
        if(!applyDraft())return;
        int t,c;if(!locate(t,c))return;
        if(!asRest&&(owner->selectedTrack!=t||owner->clipIndex()!=c)){status.setText("Show this clip in the piano roll before assigning notes.",juce::dontSendNotification);return;}
        Clip next=*current();std::vector<int> ids;
        if(!asRest)for(int index:owner->piano.selected)if(index>=0&&index<(int)next.notes.size())ids.push_back(next.notes[(size_t)index].vocalId);
        if(!asRest&&ids.empty()){status.setText("Select one or more notes in the existing piano roll.",juce::dontSendNotification);return;}
        if(assignVocalSyllable(next,syllables.getSelectedId(),ids))commit(std::move(next));
    }
    friend class MainComponent;
};
namespace {
class VocalWindow final:public juce::DialogWindow {
public:
    explicit VocalWindow(juce::Component* editor):DialogWindow("Vocal Studio — English",juce::Colour(0xff20242c),true){setUsingNativeTitleBar(true);setContentOwned(editor,true);setResizable(true,false);setResizeLimits(700,620,1100,900);centreWithSize(700,620);}
    void closeButtonPressed()override{setVisible(false);}
};
}
void MainComponent::showVocalStudio(){
    if(loadingPlugin||editingRestricted())return;
    auto& track=project.tracks[(size_t)selectedTrack];
    if(!isManualTrack(track.kind)||track.vocalChops||clip().audioOffset>=0){message("Vocal Studio needs a separate manual MIDI clip. Vocal Chops and audio regions use their existing editors.",true);return;}
    if(!clip().vocal){checkpoint();enableVocalClip(clip(),clipLength());changed(false);}
    vocalStudioWindow=std::make_unique<VocalWindow>(new VocalStudioEditor(*this,clip().vocal->id));vocalStudioWindow->setVisible(true);
}
bool MainComponent::verifyVocalStudio(const juce::File& folder){
#if !FGA_SELF_TESTS
    return false;
#else
    folder.createDirectory();bool ok=true;juce::String report;
    auto check=[&](bool passed,const char* name){ok=ok&&passed;report+=(passed?"PASS ":"FAIL ")+juce::String(name)+"\n";};
    audio.stop();setSize(1440,980);project=Project{};project.sections={{Section::intro,16,"DJ intro"},{Section::main,8,"Verse"},{Section::outro,16,"DJ outro"}};project.normalise();
    const int track=project.addMidiTrack();auto& source=project.tracks[(size_t)track].clips[0];source.position=0;source.span=project.length();source.notes={{64,1,60,100,0},{66,1,62,100,0}};
    selectClip(track,0,true);showEditor();const auto notes=clip().notes;showVocalStudio();
    auto* panel=dynamic_cast<VocalStudioEditor*>(vocalStudioWindow->getContentComponent());if(!panel)return false;
    check(clip().notes.size()==notes.size()&&clip().notes[0].start==64&&clip().notes[0].pitch==60,"opening Vocal Studio preserves authored notes");
    check(!vocalBeatAllowed(project,track,0,0)&&!vocalBeatAllowed(project,track,0,96),"intro and outro silent by default in vocal region policy");
    panel->lyrics.setText("Hello",false);panel->add.onClick();panel->text.setText("hel",false);panel->phonemes.setText("hh eh l",false);
    panel->voices.setSelectedId(2,juce::dontSendNotification);panel->modelPath.setText("/missing/qixuan",false);panel->apply.onClick();
    piano.selected={0,1};panel->assign.onClick();
    check(clip().vocal->lyrics=="Hello"&&clip().vocal->syllables[0].noteIds.size()==2&&clip().vocal->syllables[0].phonemes=="hh eh l","real editor controls save English lyrics, pronunciation draft and melisma");
    check(panel->status.getText().contains("missing")&&panel->status.getText().contains("not implemented"),"missing model and unavailable singing are explicit");
    const auto originalLinks=clip().vocal->syllables[0].noteIds;
    piano.selected={0};copyButton.onClick();editCursor=0;pasteButton.onClick();
    check(clip().vocal->syllables[0].noteIds==originalLinks&&clip().notes[0].start==0&&clip().notes[0].vocalId!=originalLinks[0],"pasting before the original cannot steal lyric IDs");undo(false);panel->timerCallback();
    const auto saved=*clip().vocal;panel->allowIntroOutro.setToggleState(true,juce::dontSendNotification);panel->apply.onClick();
    check(vocalBeatAllowed(project,track,0,0),"explicit intro override from editor");undo(false);panel->timerCallback();
    check(*clip().vocal==saved&&!panel->allowIntroOutro.getToggleState(),"vocal metadata and editor state Undo");undo(true);panel->timerCallback();check(clip().vocal->allowIntroOutro&&panel->allowIntroOutro.getToggleState(),"vocal metadata and editor state Redo");
    const auto syllable=clip().vocal->syllables[0];checkpoint();clip().notes[0].start=67;clip().notes[0].pitch=65;tidyNotes(clip(),clipLength());changed();
    check(clip().vocal->syllables[0]==syllable,"existing note edits preserve lyric association after sorting");
    panel->rest.onClick();check(clip().vocal->syllables[0].noteIds.empty(),"explicit rest/unassign");undo(false);panel->timerCallback();
    const auto before=clip();panel->regionStart.setText("invalid",false);panel->apply.onClick();check(clip()==before,"invalid region cannot mutate project");panel->refresh();
    const auto file=folder.getChildFile("vocal.fga");Project reopened;check(saveProject(project,file).wasOk()&&loadProject(file,reopened).wasOk()&&reopened.tracks[(size_t)track].clips==project.tracks[(size_t)track].clips,"actual save/reopen with missing candidate model");
    if(auto out=folder.getChildFile("vocal-editor.png").createOutputStream())check(juce::PNGImageFormat().writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds()),*out),"editor screenshot");
    project=Project{};project.normalise();select(0,0);panel->timerCallback();check(!panel->isEnabled(),"stale editor safely disables after project replacement");vocalStudioWindow.reset();
    folder.getChildFile("vocal-studio.txt").replaceWithText(report);return ok;
#endif
}

}
