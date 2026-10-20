// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "UiText.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include <BinaryData.h>
#include <cmath>
namespace fga {
namespace {
juce::Colour colour(int t){if(t==0||t==3)return Palette::accent;if(t==1)return Palette::accent2;if(t==2)return Palette::strings;const int lane=t-4;return lane==0?Palette::accent:lane<=3?Palette::accent2:lane<=6?Palette::cymbals:lane<=10?Palette::percussion:Palette::toms;}
juce::Colour guitarColour(int lane){return lane<=1?Palette::accent:lane<=4?Palette::accent2:lane<=6?Palette::percussion:lane<=8?Palette::cymbals:Palette::toms;}
juce::String utf(const std::string& s){return juce::String::fromUTF8(s.c_str());}
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size=12,juce::Colour c=Palette::text){g.setColour(c);g.setFont(FamilyTheme::font(size));g.drawText(tr(s),r,juce::Justification::centredLeft);}
juce::String instrumentName(const Track& t){return t.instrument=="internal"?"Fase Groove engine":t.instrument=="sample"?juce::File(utf(t.instrumentPath)).getFileName():"VST / AU: "+(juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)).getFileName():utf(t.instrumentPath));}
constexpr int trackHeight=36,headerWidth=216;
}
void ArrangementView::paint(juce::Graphics& g){
    g.fillAll(Palette::field);auto view=owner.arrangementPort.getViewArea();const auto& p=owner.project;
    for(int row=0;row<trackCount;++row){const size_t ti=(size_t)trackAtRow(row);int y=32+row*trackHeight;const auto& t=p.tracks[ti];
        if((int)ti==owner.selectedTrack){g.setColour(colour((int)ti).withAlpha(.045f));g.fillRect(0,y,getWidth(),trackHeight);}
        for(size_t si=0;si<p.sections.size();++si){int x=headerWidth+(int)(p.sectionStart((int)si)/4*pixelsPerBar);int w=(int)(p.sections[si].bars*pixelsPerBar);juce::Rectangle<int> cell(x+2,y+3,w-4,trackHeight-6);const auto& c=t.clips[si];bool sel=(int)ti==owner.selectedTrack&&(int)si==owner.selectedSection;
            g.setColour(colour((int)ti).withAlpha(c.notes.empty()?.035f:t.mute?.07f:sel?.24f:.12f));g.fillRoundedRectangle(cell.toFloat(),4);
            g.setColour(sel?colour((int)ti):Palette::panelEdge);g.drawRoundedRectangle(cell.toFloat(),4,sel?1.5f:.5f);
            if(w>70){int low=127,high=0;for(auto n:c.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}const double total=p.sections[si].bars*4.;g.setColour(colour((int)ti).withAlpha(.65f));for(auto n:c.notes){float nx=cell.getX()+4+(float)(n.start/total*(w-12));float ny=cell.getBottom()-5-(float)((n.pitch-low)/(double)std::max(1,high-low)*16);g.fillRect(nx,ny,std::max(1.f,(float)(n.length/total*(w-12))),2.f);}}
            if(c.locked)label(g,"L",{cell.getRight()-17,y+4,14,16},10,Palette::accent);
        }g.setColour(Palette::panelEdge.withAlpha(.5f));g.drawHorizontalLine(y+trackHeight-1,0,(float)getWidth());
    }
    // Track headers stay visible while the song scrolls horizontally.
    for(int row=0;row<trackCount;++row){const size_t ti=(size_t)trackAtRow(row);int y=32+row*trackHeight;const auto& t=p.tracks[ti];int x=view.getX();g.setColour((int)ti==owner.selectedTrack?Palette::control:Palette::panel);g.fillRect(x,y,headerWidth,trackHeight-1);g.setColour(colour((int)ti));g.fillRect(x+8,y+8,3,20);label(g,juce::String(row+1).paddedLeft('0',2),{x+18,y+2,26,30},10,Palette::dim);label(g,utf(t.name),{x+48,y+1,98,22},12);label(g,t.locked?"LOCK":t.instrument=="internal"?"INTERNAL":t.instrument=="sample"?"SAMPLE":"VST / AU",{x+48,y+20,104,13},8,Palette::dim);
        label(g,"M",{x+153,y+5,17,24},11,t.mute?Palette::toms:Palette::dim);label(g,"S",{x+174,y+5,17,24},11,t.solo?Palette::accent:Palette::dim);
        float meter=std::clamp(owner.audio.level((int)ti),0.f,1.f);g.setColour(Palette::cymbals.withAlpha(.7f));g.fillRect(x+199,y+trackHeight-5-(int)(meter*25),4,(int)(meter*25));
    }
    int y=view.getY();g.setColour(Palette::panel);g.fillRect(view.getX(),y,view.getWidth(),32);
    for(size_t si=0;si<p.sections.size();++si){int x=headerWidth+(int)(p.sectionStart((int)si)/4*pixelsPerBar);int w=(int)(p.sections[si].bars*pixelsPerBar);g.setColour((int)si==owner.selectedSection?Palette::accent.withAlpha(.17f):Palette::control);g.fillRect(x+2,y+2,w-4,28);label(g,utf(p.sections[si].name)+juce::String::fromUTF8("  ·  ")+juce::String(p.sections[si].bars)+" bars",{x+10,y,w-16,30},11,(int)si==owner.selectedSection?Palette::accent:Palette::text);}
    g.setColour(Palette::panel);g.fillRect(view.getX(),y,headerWidth,32);label(g,"INSTRUMENT / TRACK",{view.getX()+16,y,190,32},10,Palette::dim);
    const float playX=headerWidth+(float)(owner.audio.position()/4*pixelsPerBar);if(playX>view.getX()+headerWidth){g.setColour(Palette::accent2);g.fillRect(playX,(float)y+31,1.5f,(float)view.getHeight()-31);}
}
void ArrangementView::mouseDown(const juce::MouseEvent& e){
    auto v=owner.arrangementPort.getViewArea();const int ti=trackAtRow(std::clamp((e.y-32)/trackHeight,0,trackCount-1));int si=0;const double bar=(e.x-headerWidth)/pixelsPerBar;
    for(size_t i=0;i<owner.project.sections.size();++i)if(bar>=owner.project.sectionStart((int)i)/4)si=(int)i;
    si=std::clamp(si,0,(int)owner.project.sections.size()-1);
    if(e.y<v.getY()+32){if(e.x>v.getX()+headerWidth){owner.select(owner.selectedTrack,si);if(e.mods.isPopupMenu())owner.showSectionMenu(si);else owner.audio.seek(owner.project.sectionStart(si));}return;}
    if(e.x<v.getX()+headerWidth){owner.select(ti,owner.selectedSection);if(e.mods.isPopupMenu())owner.showTrackMenu(ti);else if(e.x>=v.getX()+150&&e.x<v.getX()+171){owner.checkpoint();owner.project.tracks[(size_t)ti].mute=!owner.project.tracks[(size_t)ti].mute;owner.changed(false);}else if(e.x>=v.getX()+171&&e.x<v.getX()+194){owner.checkpoint();owner.project.tracks[(size_t)ti].solo=!owner.project.tracks[(size_t)ti].solo;owner.changed(false);}else owner.showInstrument();return;}
    owner.select(ti,si);if(e.mods.isPopupMenu())owner.showClipMenu(ti,si);else owner.showEditor();
}
PianoRoll::PianoRoll(MainComponent& m):owner(m){setWantsKeyboardFocus(true);}
bool PianoRoll::laneView()const{return owner.selectedTrack>=4||(owner.selectedTrack==3&&!guitarNotes);}
float PianoRoll::noteY(const Note& n)const{return laneView()?(owner.selectedTrack==3?40.f+n.lane*40.f:64.f):24.f+(127-n.pitch)*12.f;}
float PianoRoll::noteWidth(const Note& n)const{return std::max(5.f,(float)((owner.selectedTrack>=4?std::min(n.length,grid*.8):n.length)*pixelsPerBeat));}
float PianoRoll::noteHeight()const{return laneView()?(owner.selectedTrack==3?31.f:64.f):11.f;}
int PianoRoll::laneAt(float y)const{return owner.selectedTrack==3?std::clamp((int)((y-40)/40),0,11):0;}
int PianoRoll::pitch(float y)const{return owner.selectedTrack>=4?(owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote<0?fgd::defaultNote(owner.selectedTrack-4):owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote):std::clamp(127-(int)((y-24)/12),0,127);}
int PianoRoll::hit(juce::Point<float> p)const{const auto& c=owner.project.tracks[(size_t)owner.selectedTrack].clips[(size_t)owner.selectedSection];for(int i=(int)c.notes.size()-1;i>=0;--i){const auto& n=c.notes[(size_t)i];juce::Rectangle<float> r(64+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight());if(r.contains(p))return i;}return -1;}
void PianoRoll::paint(juce::Graphics& g){
    g.fillAll(Palette::field);const auto view=owner.pianoPort.getViewArea();const auto& notes=owner.clip().notes;
    if(!laneView())for(int n=0;n<128;++n){int y=24+(127-n)*12;bool black=n%12==1||n%12==3||n%12==6||n%12==8||n%12==10;g.setColour(black?Palette::background:Palette::panel.withAlpha(.75f));g.fillRect(0,y,getWidth(),12);g.setColour(Palette::panelEdge.withAlpha(n%12==0?.7f:.25f));g.drawHorizontalLine(y+11,0,(float)getWidth());}
    const double beats=owner.clipLength();for(double b=0;b<=beats;b+=grid){int x=64+(int)(b*pixelsPerBeat);bool bar=std::abs(std::fmod(b,4.))<.0001;g.setColour(Palette::panelEdge.withAlpha(bar?.9f:.35f));g.drawVerticalLine(x,0,(float)getHeight());}
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];juce::Rectangle<float> r(64+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight());g.setColour(selected.count((int)i)?Palette::text:(owner.selectedTrack==3?guitarColour(n.lane):colour(owner.selectedTrack)).withAlpha(.35f+n.velocity/200.f));g.fillRoundedRectangle(r,2);if(laneView()&&r.getWidth()>24){g.setColour(Palette::background);g.setFont(FamilyTheme::font(10));g.drawText(juce::MidiMessage::getMidiNoteName(n.pitch,true,true,4),r,juce::Justification::centred);}if(r.getWidth()>24){g.setColour(Palette::background.withAlpha(.7f));g.fillRect(r.getX()+2,r.getBottom()-3,(r.getWidth()-4)*n.velocity/127.f,1.5f);}}
    int vx=view.getX(),vy=view.getY();g.setColour(Palette::panel);g.fillRect(vx,vy,view.getWidth(),24);
    for(int b=0;b<(int)beats;b+=4)label(g,juce::String(b/4+1),{64+(int)(b*pixelsPerBeat)+5,vy,65,24},10,Palette::dim);
    if(laneView()){const int lanes=owner.selectedTrack==3?12:1;for(int l=0;l<lanes;++l){int y=owner.selectedTrack==3?40+l*40:64;g.setColour(Palette::panelEdge);g.drawHorizontalLine(y+35,0,(float)getWidth());g.setColour(Palette::panel);g.fillRect(vx,y,63,owner.selectedTrack==3?36:72);label(g,owner.selectedTrack==3?fgg::laneShortName(l):utf(owner.project.tracks[(size_t)owner.selectedTrack].name),{vx+3,y,60,36},9,owner.selectedTrack==3?guitarColour(l):colour(owner.selectedTrack));}}
    else for(int n=0;n<128;++n){int y=24+(127-n)*12;g.setColour(n%12==0?Palette::control:Palette::panel);g.fillRect(vx,y,63,12);label(g,juce::MidiMessage::getMidiNoteName(n,true,true,4),{vx+8,y,50,12},8,n%12==0?Palette::text:Palette::dim);}
    const auto local=owner.audio.position()-owner.project.sectionStart(owner.selectedSection);if(local>=0&&local<beats){float x=64+(float)(local*pixelsPerBeat);if(x>vx+64){g.setColour(Palette::accent2);g.fillRect(x,(float)vy+24,1.5f,(float)view.getHeight()-24);}}
}
void PianoRoll::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();if(e.x<owner.pianoPort.getViewArea().getX()+64||e.y<owner.pianoPort.getViewArea().getY()+24)return;
    int index=hit(e.position);if(e.mods.isPopupMenu()){if(index>=0){owner.checkpoint();if(laneView()&&owner.selectedTrack==3){const auto note=owner.clip().notes[(size_t)index];auto& notes=owner.clip().notes;notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return n.lane==note.lane&&std::abs(n.start-note.start)<.18;}),notes.end());}else owner.clip().notes.erase(owner.clip().notes.begin()+index);selected.clear();owner.changed();}return;}
    owner.checkpoint();anchor=e.position;resizing=false;velocityDrag=e.mods.isAltDown();before=owner.clip().notes;
    if(index>=0){if(e.mods.isShiftDown()){if(selected.count(index))selected.erase(index);else selected.insert(index);}else if(!selected.count(index)){selected.clear();selected.insert(index);if(laneView()&&owner.selectedTrack==3){const auto hitNote=before[(size_t)index];for(size_t k=0;k<before.size();++k)if(before[k].lane==hitNote.lane&&std::abs(before[k].start-hitNote.start)<.18)selected.insert((int)k);}}const auto& n=before[(size_t)index];resizing=owner.selectedTrack<4&&e.x>=64+(n.start+n.length)*pixelsPerBeat-6&&!velocityDrag;}
    else{if(!e.mods.isShiftDown())selected.clear();double start=std::floor(beat(e.position.x)/grid)*grid;if(start>=owner.clipLength())return;if(laneView()&&owner.selectedTrack==3){const int lane=laneAt(e.position.y);const auto& settings=owner.project.settings;const int slot=((int)(start/4))%settings.chordCount;auto chord=settings.freeChords[(size_t)slot]>=0?fgg::codeChord(settings.key,settings.freeChords[(size_t)slot],std::min(2,settings.extension)):fgg::diatonicChord(settings.key,settings.scale,settings.degrees[(size_t)slot],std::min(2,settings.extension));auto notes=fgg::laneNotes((fgg::Lane)lane,fgg::tonesOf(chord),(int)(start*4)%16);for(int note:notes){owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),note,(int)owner.velocitySlider.getValue(),lane});selected.insert((int)owner.clip().notes.size()-1);}}
        else{owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),pitch(e.position.y),(int)owner.velocitySlider.getValue(),0});selected.insert((int)owner.clip().notes.size()-1);}before=owner.clip().notes;}
    repaint();
}
void PianoRoll::mouseDrag(const juce::MouseEvent& e){
    if(before.empty()||selected.empty())return;const double delta=std::round((e.position.x-anchor.x)/pixelsPerBeat/grid)*grid;const int deltaPitch=-(int)std::round((e.position.y-anchor.y)/12.);auto& notes=owner.clip().notes;
    for(int i:selected)if(i>=0&&i<(int)notes.size()&&i<(int)before.size()){const auto n=before[(size_t)i];auto& now=notes[(size_t)i];if(velocityDrag)now.velocity=std::clamp(n.velocity+(int)(anchor.y-e.position.y),1,127);else if(resizing)now.length=std::clamp(n.length+delta,std::min(.0625,owner.clipLength()-n.start),owner.clipLength()-n.start);else{now.start=std::clamp(n.start+delta,0.,std::max(0.,owner.clipLength()-n.length));if(!laneView())now.pitch=std::clamp(n.pitch+deltaPitch,0,127);else if(owner.selectedTrack==3)now.lane=std::clamp(n.lane+(int)std::round((e.position.y-anchor.y)/40.),0,11);}}
    repaint();
}
void PianoRoll::mouseUp(const juce::MouseEvent&){
    if(before.empty())return;
    before.clear();std::vector<Note> wanted;
    for(int i:selected)if(i>=0&&i<(int)owner.clip().notes.size())wanted.push_back(owner.clip().notes[(size_t)i]);
    tidyNotes(owner.clip(),owner.clipLength());selected.clear();
    for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];for(const auto& old:wanted)if(n.pitch==old.pitch&&std::abs(n.start-old.start)<1./960.){selected.insert((int)i);break;}}
    owner.changed();
}
void PianoRoll::deleteSelected(){if(selected.empty())return;owner.checkpoint();auto& notes=owner.clip().notes;for(auto i=selected.rbegin();i!=selected.rend();++i)if(*i>=0&&*i<(int)notes.size())notes.erase(notes.begin()+*i);selected.clear();owner.changed();}
bool PianoRoll::keyPressed(const juce::KeyPress& key){if(key==juce::KeyPress::deleteKey||key==juce::KeyPress::backspaceKey){deleteSelected();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='A'){selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i)selected.insert((int)i);repaint();return true;}return owner.keyPressed(key);}

class MainComponent::PluginWindow : public juce::DocumentWindow {
public:
    explicit PluginWindow(juce::AudioPluginInstance& p):DocumentWindow(p.getName(),Palette::panel,DocumentWindow::closeButton){setUsingNativeTitleBar(true);auto* editor=p.createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(p);setContentOwned(editor,true);setResizable(editor->isResizable(),false);centreWithSize(getWidth(),getHeight());setVisible(true);}
    void closeButtonPressed()override{setVisible(false);}
};
MainComponent::MainComponent(){
    Localisation::loadSaved();if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);setLookAndFeel(&theme);setWantsKeyboardFocus(true);project=newProject();selectedTrack=4;
    for(juce::Component* c:std::initializer_list<juce::Component*>{&backButton,&instrumentButton,&editButton,&leadButton,&arrangeButton,&guitarNotesButton,&minutesSlider,&sectionSelector,&panelSections})addAndMakeVisible(c);
    instrumentPort.setViewedComponent(&instrumentPanel,false);instrumentPort.setScrollBarsShown(true,false);addAndMakeVisible(instrumentPort);
    backButton.onClick=[this]{showArrangement();};instrumentButton.onClick=[this]{showInstrument();};editButton.onClick=[this]{showEditor();};
    guitarNotesButton.onClick=[this]{piano.guitarNotes=!piano.guitarNotes;showEditor();};
    leadButton.onClick=[this]{checkpoint();project.settings.guitarLead=!project.settings.guitarLead;generate(project,Scope::track,0,selectedSection);generate(project,Scope::track,3,selectedSection);changed();};
    minutesSlider.setRange(4,12,.5);minutesSlider.setSliderStyle(juce::Slider::LinearHorizontal);minutesSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,72,24);minutesSlider.setTextValueSuffix(" min");minutesSlider.onDragStart=[this]{checkpoint();};minutesSlider.onValueChange=[this]{if(!refreshing){project.settings.songMinutes=minutesSlider.getValue();dirty=true;}};
    arrangeButton.onClick=[this]{if(loadingPlugin)return;for(const auto& t:project.tracks){if(t.locked){message("Sblocca le tracce prima di ricostruire la struttura. GENERATE SONG mantiene la struttura e i blocchi.");return;}for(const auto& c:t.clips)if(c.locked){message("Sblocca i clip prima di ricostruire la struttura.");return;}}checkpoint();audio.stop();arrangeSong(project);generate(project,Scope::song,0,0,(std::uint32_t)juce::Random::getSystemRandom().nextInt());selectedSection=0;changed();showArrangement();};
    panelSections.onChange=[this]{auto names=instrumentPanel.sectionNames();int index=panelSections.getSelectedItemIndex();if(index>=0&&index<names.size())instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(names[index]));};
    sectionSelector.onChange=[this]{if(!refreshing){select(selectedTrack,sectionSelector.getSelectedItemIndex());if(page==Page::editor)showEditor();}};
    addAndMakeVisible(chordEditButton);chordEditButton.onClick=[this]{showChordMenu();};
    arrangementPort.setViewedComponent(&arrangement,false);pianoPort.setViewedComponent(&piano,false);arrangementPort.setScrollBarsShown(true,true);pianoPort.setScrollBarsShown(true,true);addAndMakeVisible(arrangementPort);addAndMakeVisible(pianoPort);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&playButton,&stopButton,&rewindButton,&undoButton,&redoButton,&loopButton,&songButton,&trackButton,&sectionButton,&clipButton,&harmonyButton,&addSectionButton,&genreBox,&keyBox,&scaleBox,&extensionBox,&voicingBox,&progressionBox,&bassBox,&stringsBox,&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&zoomSlider,&positionLabel,&chordsLabel,&statusLabel,&soundButton,&pluginEditorButton,&sampleButton,&loadPluginButton,&gainSlider,&panSlider,&velocitySlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&soundBox,&gridBox,&quantiseButton,&transposeUp,&transposeDown,&deleteNotes,&importButton,&copyButton,&pasteButton,&sectionLoopButton,&inspectorLabel,&editorLabel})addAndMakeVisible(c);
    auto slider=[this](juce::Slider& s,double lo,double hi,double step,const juce::String& suffix){s.setRange(lo,hi,step);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,22);s.setTextValueSuffix(suffix);s.onDragStart=[this]{checkpoint();};};
    slider(tempoSlider,30,300,.1," BPM");slider(swingSlider,0,150,1,"% swing");slider(densitySlider,0,100,1,"% notes");slider(humanizeSlider,0,100,1,"% feel");slider(masterSlider,0,125,1,"% out");slider(zoomSlider,4,160,1," zoom");slider(gainSlider,0,200,1,"% gain");slider(panSlider,-100,100,1," pan");slider(velocitySlider,1,127,1," vel");velocitySlider.setValue(96);
    for(size_t i=0;i<genres().size();++i)genreBox.addItem(utf(genres()[i]),(int)i+1);
    for(int i=0;i<12;++i)keyBox.addItem(sgk::noteName(i),i+1);
    for(int i=0;i<13;++i)scaleBox.addItem(sgk::scaleName(i),i+1);
    for(int i=0;i<11;++i){extensionBox.addItem(sgk::extensionName(i),i+1);voicingBox.addItem(sgk::voicingName(i),i+1);}
    const auto& progressions=sgk::progressionPresets();for(size_t i=0;i<progressions.size();++i)progressionBox.addItem(progressions[i].name,(int)i+1);progressionBox.setTextWhenNothingSelected("PROGRESSION");
    for(int i=0;i<sgk::numBassModes;++i)bassBox.addItem(sgk::bassModeName(i),i+1);for(int i=0;i<sgk::numStringsModes;++i)stringsBox.addItem(sgk::stringsModeName(i),i+1);
    gridBox.addItem("1/4",1);gridBox.addItem("1/8",2);gridBox.addItem("1/16",3);gridBox.addItem("1/32",4);gridBox.setSelectedId(3);
    auto setting=[this](std::function<void()> fn){if(refreshing)return;checkpoint();fn();project.normalise();changed(false);message("Impostazioni aggiornate. GENERATE per applicarle al MIDI; il MIDI esistente resta modificabile.");};
    genreBox.onChange=[this,setting]{setting([this]{applyGenre(project,genreBox.getSelectedItemIndex());});message("Genere pronto: NEW ARRANGEMENT applica forma, durata e mix in/out. GENERATE SONG mantiene le sezioni attuali.");};
    keyBox.onChange=[this,setting]{setting([this]{project.settings.key=keyBox.getSelectedItemIndex();});};
    scaleBox.onChange=[this,setting]{setting([this]{project.settings.scale=scaleBox.getSelectedItemIndex();});};
    extensionBox.onChange=[this,setting]{setting([this]{project.settings.extension=extensionBox.getSelectedItemIndex();});};
    voicingBox.onChange=[this,setting]{setting([this]{project.settings.voicing=voicingBox.getSelectedItemIndex();});};
    progressionBox.onChange=[this,setting]{setting([this]{chooseProgression(project,progressionBox.getSelectedItemIndex());});};
    bassBox.onChange=[this,setting]{setting([this]{project.settings.bassMode=bassBox.getSelectedItemIndex();});};
    stringsBox.onChange=[this,setting]{setting([this]{project.settings.stringsMode=stringsBox.getSelectedItemIndex();});};
    tempoSlider.onValueChange=[this]{if(!refreshing){project.settings.bpm=tempoSlider.getValue();changed(false);}};
    swingSlider.onValueChange=[this]{if(!refreshing){project.settings.swing=swingSlider.getValue()/100.;dirty=true;}};
    densitySlider.onValueChange=[this]{if(!refreshing){project.settings.density=densitySlider.getValue()/100.;dirty=true;}};
    humanizeSlider.onValueChange=[this]{if(!refreshing){project.settings.humanize=humanizeSlider.getValue()/100.;dirty=true;}};
    masterSlider.onValueChange=[this]{if(!refreshing){project.masterGain=masterSlider.getValue()/100.;changed(false);}};
    zoomSlider.onValueChange=[this]{if(page==Page::editor)piano.pixelsPerBeat=zoomSlider.getValue();else arrangement.pixelsPerBar=zoomSlider.getValue();updateSizes();};zoomSlider.setValue(6,juce::sendNotificationSync);
    gainSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].gain=gainSlider.getValue()/100.;changed(false);}};
    panSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].pan=panSlider.getValue()/100.;changed(false);}};
    velocitySlider.onDragStart=[this]{if(!piano.selected.empty())checkpoint();};velocitySlider.onValueChange=[this]{if(!refreshing&&!piano.selected.empty()){for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i].velocity=(int)velocitySlider.getValue();changed();}};
    auto toggle=[this](juce::ToggleButton& b,bool Track::*field){b.onClick=[this,&b,field]{checkpoint();project.tracks[(size_t)selectedTrack].*field=b.getToggleState();changed(false);};};toggle(muteButton,&Track::mute);toggle(soloButton,&Track::solo);toggle(lockButton,&Track::locked);
    lockClipButton.onClick=[this]{checkpoint();clip().locked=lockClipButton.getToggleState();changed(false);};
    playButton.onClick=[this]{audio.play(!audio.isPlaying());};stopButton.onClick=[this]{audio.stop();};rewindButton.onClick=[this]{audio.seek(0);};
    loopButton.onClick=[this]{project.loop=loopButton.getToggleState();audio.setLoop(project.loop,project.loopStart,project.loopEnd);dirty=true;};
    sectionLoopButton.onClick=[this]{project.loopStart=project.sectionStart(selectedSection);project.loopEnd=project.loopStart+clipLength();project.loop=true;audio.setLoop(true,project.loopStart,project.loopEnd);refreshControls();};
    songButton.onClick=[this]{regenerate(Scope::song);};trackButton.onClick=[this]{regenerate(Scope::track);};sectionButton.onClick=[this]{regenerate(Scope::section);};clipButton.onClick=[this]{regenerate(Scope::clip);};
    harmonyButton.onClick=[this]{checkpoint();randomiseHarmony(project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());changed(false);message("Nuova armonia pronta: GENERATE SONG per applicarla a tutte le tracce sbloccate.");};
    undoButton.onClick=[this]{undo(false);};redoButton.onClick=[this]{undo(true);};
    quantiseButton.onClick=[this]{modifySelected([this](Clip& c){quantise(c,piano.grid);});};transposeUp.onClick=[this]{modifySelected([](Clip& c){transpose(c,12);});};transposeDown.onClick=[this]{modifySelected([](Clip& c){transpose(c,-12);});};deleteNotes.onClick=[this]{piano.deleteSelected();};
    gridBox.onChange=[this]{piano.grid=std::pow(.5,gridBox.getSelectedItemIndex());piano.repaint();};
    copyButton.onClick=[this]{clipboard.clear();for(size_t i=0;i<clip().notes.size();++i)if(piano.selected.empty()||piano.selected.count((int)i))clipboard.push_back(clip().notes[i]);message("MIDI copiato. PASTE inserisce nella sezione selezionata.");};
    pasteButton.onClick=[this]{if(clipboard.empty())return;checkpoint();clip().notes.insert(clip().notes.end(),clipboard.begin(),clipboard.end());tidyNotes(clip(),clipLength());piano.clearSelection();changed();};
    soundButton.setButtonText("BUILT-IN");soundButton.onClick=[this]{if(loadingPlugin)return;checkpoint();closePluginEditor();audio.useInternal(selectedTrack);auto& t=project.tracks[(size_t)selectedTrack];t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();changed(false);};
    sampleButton.onClick=[this]{if(loadingPlugin)return;int track=selectedTrack;chooseFile("Campione personale","*.wav;*.aif;*.aiff;*.flac;*.ogg",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,track](juce::File file){loadSampleFile(track,file);});};
    loadPluginButton.onClick=[this]{showPluginMenu();};
    pluginEditorButton.onClick=[this]{if(loadingPlugin)return;if(auto* p=audio.plugin(selectedTrack)){pluginWindow.reset();pluginWindow=std::make_unique<PluginWindow>(*p);}else message("Nessun plugin caricato su questa traccia.");};
    soundBox.onChange=[this]{if(refreshing)return;checkpoint();project.tracks[(size_t)selectedTrack].sound=soundBox.getSelectedItemIndex();changed(false);};
    importButton.onClick=[this]{chooseFile("Importa note nella sezione selezionata","*.mid;*.midi",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){importFile(f);});};
    addSectionButton.onClick=[this]{checkpoint();project.insertSection(selectedSection+1,{Section::main,8,"Main"});selectedSection++;generate(project,Scope::section,selectedTrack,selectedSection,(std::uint32_t)juce::Random::getSystemRandom().nextInt());changed();};
    saveButton.onClick=[this]{save(false);};saveAsButton.onClick=[this]{save(true);};
    openButton.onClick=[this]{chooseFile("Apri progetto Fase Groove Arranger","*.fga",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){open(f);});};
    newButton.onClick=[this]{if(loadingPlugin)return;if(!requestClose())return;audio.stop();pluginWindow.reset();checkpoint();for(int i=0;i<trackCount;++i)audio.useInternal(i);project=newProject(genreBox.getSelectedItemIndex());projectFile=juce::File();selectedSection=0;selectedTrack=4;changed();showArrangement();};
    exportButton.onClick=[this]{exportMenu();};settingsButton.onClick=[this]{showAudioSettings();};aboutButton.onClick=[this]{showAbout();};helpButton.onClick=[this]{showGuide(Localisation::current()!=Localisation::Language::italian);};
    for(auto* b:{&songButton,&clipButton}){b->setColour(juce::TextButton::buttonColourId,Palette::accent);b->setColour(juce::TextButton::textColourOffId,Palette::background);}
    for(const auto& entry:std::initializer_list<std::pair<juce::Component*,const char*>>{{&genreBox,"GROOVE"},{&keyBox,"KEY"},{&scaleBox,"Scale / mode"},{&extensionBox,"Extensions"},{&voicingBox,"Voicing"},{&progressionBox,"PROGRESSION"},{&tempoSlider,"TEMPO"},{&swingSlider,"SWING"},{&densitySlider,"DENSITY"},{&humanizeSlider,"HUMANIZE"},{&masterSlider,"Volume"},{&gainSlider,"Volume"},{&panSlider,"PAN"},{&velocitySlider,"VELOCITY"},{&zoomSlider,"ZOOM"},{&minutesSlider,"NEW ARRANGEMENT"}})entry.first->getProperties().set("fgaControl",entry.second);
    piano.getProperties().set("fgaTip","Draw notes, drag to move, drag the edge for duration; Alt-drag changes velocity.");

    refreshControls();audio.publish(project);setSize(1440,980);select(4,0);showArrangement();message(juce::String::fromUTF8("Pronto · Spazio: play/pausa · Tasto destro: opzioni · Autosave ogni minuto."));startTimerHz(20);
}
MainComponent::~MainComponent(){if(musicalToolWindow)musicalToolWindow->closeButtonPressed();stopTimer();pluginWindow.reset();audio.stop();setLookAndFeel(nullptr);}
void MainComponent::closePluginEditor(){pluginWindow.reset();}
void MainComponent::checkpoint(){undoStack.push_back(project);if(undoStack.size()>40)undoStack.erase(undoStack.begin());redoStack.clear();}
void MainComponent::changed(bool schedule){project.normalise();dirty=true;if(schedule)audio.publish(project);else audio.updateMix(project);refreshControls();arrangement.repaint();piano.repaint();repaint();}
void MainComponent::applyLanguage(){if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);else tooltips.reset();translateControls(*this);if(page==Page::instrument)instrumentPanel.rebuild();repaint();}
void MainComponent::showArrangement(){page=Page::arrangement;zoomSlider.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);resized();repaint();}
void MainComponent::showInstrument(){page=Page::instrument;instrumentPanel.rebuild();panelSections.clear(juce::dontSendNotification);for(auto name:instrumentPanel.sectionNames())panelSections.addItem(tr(name),panelSections.getNumItems()+1);panelSections.setTextWhenNothingSelected(tr("ADVANCED"));resized();instrumentPort.setViewPosition(0,0);repaint();}
void MainComponent::showEditor(){page=Page::editor;zoomSlider.setValue(piano.pixelsPerBeat,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);piano.grabKeyboardFocus();repaint();}
void MainComponent::select(int track,int section){selectedTrack=std::clamp(track,0,16);selectedSection=std::clamp(section,0,(int)project.sections.size()-1);piano.clearSelection();sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);refreshTrack();updateSizes();int centre=selectedTrack==1?40:selectedTrack>=4?fgd::defaultNote(selectedTrack-4):64;pianoPort.setViewPosition(0,piano.laneView()?0:std::max(0,24+(127-centre)*12-pianoPort.getHeight()/2));arrangement.repaint();piano.repaint();}
void MainComponent::refreshControls(){
    refreshing=true;auto& s=project.settings;minutesSlider.setValue(s.songMinutes,juce::dontSendNotification);leadButton.setToggleState(s.guitarLead,juce::dontSendNotification);leadButton.setButtonText("SOLISTA");sectionSelector.clear(juce::dontSendNotification);for(size_t i=0;i<project.sections.size();++i)sectionSelector.addItem(utf(project.sections[i].name),((int)i)+1);sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);genreBox.setSelectedId(s.genre+1,juce::dontSendNotification);keyBox.setSelectedId(s.key+1,juce::dontSendNotification);scaleBox.setSelectedId(s.scale+1,juce::dontSendNotification);extensionBox.setSelectedId(s.extension+1,juce::dontSendNotification);voicingBox.setSelectedId(s.voicing+1,juce::dontSendNotification);bassBox.setSelectedId(s.bassMode+1,juce::dontSendNotification);stringsBox.setSelectedId(s.stringsMode+1,juce::dontSendNotification);
    tempoSlider.setValue(s.bpm,juce::dontSendNotification);swingSlider.setValue(s.swing*100,juce::dontSendNotification);densitySlider.setValue(s.density*100,juce::dontSendNotification);humanizeSlider.setValue(s.humanize*100,juce::dontSendNotification);masterSlider.setValue(project.masterGain*100,juce::dontSendNotification);loopButton.setToggleState(project.loop,juce::dontSendNotification);
    juce::StringArray chords;for(auto name:chordNames(s))chords.add(utf(name));chordsLabel.setText("HARMONY   "+chords.joinIntoString("    /    "),juce::dontSendNotification);
    undoButton.setEnabled(!undoStack.empty());redoButton.setEnabled(!redoStack.empty());refreshing=false;refreshTrack();updateSizes();translateControls(*this);
}
void MainComponent::refreshTrack(){
    refreshing=true;const auto& t=project.tracks[(size_t)selectedTrack];gainSlider.setValue(t.gain*100,juce::dontSendNotification);panSlider.setValue(t.pan*100,juce::dontSendNotification);muteButton.setToggleState(t.mute,juce::dontSendNotification);soloButton.setToggleState(t.solo,juce::dontSendNotification);lockButton.setToggleState(t.locked,juce::dontSendNotification);lockClipButton.setToggleState(clip().locked,juce::dontSendNotification);
    soundBox.clear(juce::dontSendNotification);int count=selectedTrack==2?1:selectedTrack<2?7:4;for(int i=0;i<count;++i)soundBox.addItem(selectedTrack==2?"String ensemble":selectedTrack<2?guideSoundName(i):selectedTrack==3?GuideGuitar::toneName(i):GuideDrums::kitName(i),i+1);soundBox.setSelectedId(std::min(t.sound+1,count),juce::dontSendNotification);soundBox.setEnabled(t.instrument=="internal"&&selectedTrack!=2);
    inspectorLabel.setText(utf(t.name).toUpperCase()+juce::String::fromUTF8("   ·   ")+instrumentName(t),juce::dontSendNotification);editorLabel.setText("PIANO ROLL   /   "+utf(t.name)+"   /   "+utf(project.sections[(size_t)selectedSection].name)+"   /   "+juce::String((int)clip().notes.size())+" notes",juce::dontSendNotification);pluginEditorButton.setEnabled(!loadingPlugin&&audio.plugin(selectedTrack)!=nullptr);sampleButton.setEnabled(!loadingPlugin);refreshing=false;
}
void MainComponent::updateSizes(){arrangement.setSize(headerWidth+(int)(project.length()/4*arrangement.pixelsPerBar)+20,32+trackCount*trackHeight);piano.setSize(64+(int)(clipLength()*piano.pixelsPerBeat)+20,piano.laneView()?(selectedTrack==3?540:240):24+128*12);}
void MainComponent::undo(bool redo){if(loadingPlugin)return;auto& source=redo?redoStack:undoStack;auto& dest=redo?undoStack:redoStack;if(source.empty())return;auto before=project;dest.push_back(project);project=std::move(source.back());source.pop_back();project.normalise();selectedSection=std::min(selectedSection,(int)project.sections.size()-1);
    bool instrumentsChanged=false;for(size_t i=0;i<project.tracks.size();++i)if(project.tracks[i].instrument!=before.tracks[i].instrument||project.tracks[i].instrumentPath!=before.tracks[i].instrumentPath||project.tracks[i].instrumentId!=before.tracks[i].instrumentId||project.tracks[i].sampleRoot!=before.tracks[i].sampleRoot)instrumentsChanged=true;
    if(instrumentsChanged)restoreInstruments();piano.clearSelection();changed();if(page==Page::instrument)instrumentPanel.rebuild();}
void MainComponent::regenerate(Scope scope){if(loadingPlugin)return;checkpoint();audio.stop();auto seed=page==Page::instrument?0u:(std::uint32_t)juce::Random::getSystemRandom().nextInt();if(seed==0&&page!=Page::instrument)seed=1;generate(project,scope,selectedTrack,selectedSection,seed);piano.clearSelection();changed();message(juce::String::fromUTF8("Nuova generazione pronta · blocchi rispettati · UNDO per tornare indietro."));}
void MainComponent::modifySelected(std::function<void(Clip&)> action){checkpoint();if(piano.selected.empty())action(clip());else{Clip part;for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())part.notes.push_back(clip().notes[(size_t)i]);action(part);size_t k=0;for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i]=part.notes[k++];}tidyNotes(clip(),clipLength());piano.clearSelection();changed();}
void MainComponent::paint(juce::Graphics& g){
    g.fillAll(Palette::background);drawFamilyLogo(g,{18,9,300,53});
    label(g,"FS PITCH / PIANO + DRUMS + GUITAR",{332,13,330,20},10,Palette::dim);
    const double seconds=project.length()*60/project.settings.bpm;label(g,utf(project.name)+(dirty?" *":"")+"   /   "+juce::String((int)seconds/60)+":"+juce::String((int)seconds%60).paddedLeft('0',2),{332,38,330,20},13);
    for(int y:{69,110,192,getHeight()-31}){g.setColour(Palette::panelEdge);g.fillRect(12,y,getWidth()-24,1);}
    if(page!=Page::arrangement)label(g,(page==Page::instrument?"INSTRUMENT / ":"MIDI EDITOR / ")+utf(project.tracks[(size_t)selectedTrack].name).toUpperCase(),{196,122,400,28},18,colour(selectedTrack));
}
void MainComponent::resized(){
    const int w=getWidth();auto place=[](juce::Component& c,int x,int y,int width,int height=28){c.setBounds(x,y,width,height);};
    const bool song=page==Page::arrangement,engine=page==Page::instrument,edit=page==Page::editor;
    arrangementPort.setVisible(song);instrumentPort.setVisible(engine);pianoPort.setVisible(edit);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&genreBox,&songButton,&trackButton,&sectionButton,&clipButton,&densitySlider,&swingSlider,&humanizeSlider,&progressionBox,&harmonyButton,&extensionBox,&voicingBox,&bassBox,&stringsBox,&addSectionButton,&zoomSlider,&minutesSlider,&arrangeButton})c->setVisible(song);
    bassBox.setVisible(false);stringsBox.setVisible(false); // instrument-owned styles live in their panels
    for(juce::Component* c:std::initializer_list<juce::Component*>{&soundButton,&soundBox,&sampleButton,&loadPluginButton,&pluginEditorButton,&gainSlider,&panSlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&inspectorLabel})c->setVisible(engine);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&editorLabel,&sectionLoopButton,&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes,&gridBox,&velocitySlider})c->setVisible(edit);
    if(edit&&selectedTrack>=4){transposeDown.setVisible(false);transposeUp.setVisible(false);}
    backButton.setVisible(!song);instrumentButton.setVisible(edit);editButton.setVisible(engine);sectionSelector.setVisible(!song);leadButton.setVisible(!song&&selectedTrack==3);guitarNotesButton.setVisible(edit&&selectedTrack==3);panelSections.setVisible(engine&&selectedTrack!=3);
    int x=w-752;for(auto* b:{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton}){place(*b,x,20,b==&aboutButton?112:82);x+=b==&aboutButton?120:90;}
    x=16;place(rewindButton,x,77,36);x+=42;place(playButton,x,77,62);x+=68;place(stopButton,x,77,60);x+=66;place(loopButton,x,77,90);x+=96;place(positionLabel,x,77,114);x+=120;place(tempoSlider,x,77,158);x+=166;place(keyBox,x,77,64);x+=72;place(scaleBox,x,77,160);x+=168;place(undoButton,x,77,62);x+=68;place(redoButton,x,77,62);x+=74;place(masterSlider,x,77,156);place(chordEditButton,w-150,77,132);
    if(song){
        x=16;place(genreBox,x,118,178);x+=188;place(minutesSlider,x,118,172);x+=182;place(arrangeButton,x,118,170);x+=180;place(songButton,x,118,154);x+=164;place(trackButton,x,118,116);x+=124;place(sectionButton,x,118,124);x+=132;place(clipButton,x,118,114);x+=124;place(addSectionButton,x,118,114);
        x=16;place(progressionBox,x,159,210);x+=218;place(harmonyButton,x,159,118);x+=126;place(extensionBox,x,159,116);x+=124;place(voicingBox,x,159,116);x+=124;place(densitySlider,x,159,150);x+=158;place(swingSlider,x,159,150);x+=158;place(humanizeSlider,x,159,150);x+=158;place(zoomSlider,x,159,150);
        arrangementPort.setBounds(12,202,w-24,getHeight()-284);place(chordsLabel,18,getHeight()-73,w-36,28);
    }else{
        place(backButton,16,122,160);place(sectionSelector,600,122,230);place(editButton,w-170,122,150);place(instrumentButton,w-200,122,180);place(leadButton,850,122,260);place(panelSections,850,122,260);
        if(engine){
            place(inspectorLabel,20,159,w-40,26);x=20;for(auto* b:{&soundButton,&sampleButton,&loadPluginButton,&pluginEditorButton}){place(*b,x,203,150);x+=160;}place(soundBox,x,203,190);place(gainSlider,x+200,203,165);place(panSlider,x+375,203,160);
            x=20;place(muteButton,x,245,100);place(soloButton,x+110,245,100);place(lockButton,x+220,245,160);place(lockClipButton,x+390,245,160);place(trackButton,w-340,245,150);trackButton.setVisible(true);trackButton.setButtonText("APPLY TRACK");place(clipButton,w-175,245,150);clipButton.setVisible(true);clipButton.setButtonText("APPLY CLIP");
            instrumentPort.setBounds(12,286,w-24,getHeight()-325);instrumentPanel.setSize(instrumentPort.getWidth()-16,instrumentPanel.getHeight());
        }else{
            place(editorLabel,20,159,640,28);place(guitarNotesButton,w-340,159,180);place(sectionLoopButton,w-150,159,132);
            x=18;for(auto* b:{&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes}){int width=b==&importButton?115:b==&quantiseButton?103:64;place(*b,x,203,width,28);x+=width+6;}place(gridBox,x+3,203,88);place(velocitySlider,x+101,203,154);place(zoomSlider,x+265,203,160);zoomSlider.setVisible(true);pianoPort.setBounds(12,246,w-24,getHeight()-285);
        }
    }
    chordsLabel.setVisible(song);
    if(song){trackButton.setButtonText("NEW TRACK");clipButton.setButtonText("NEW CLIP");}
    place(statusLabel,18,getHeight()-29,w-36,25);updateSizes();translateControls(*this);
}
bool MainComponent::writeLanguageEvidence(const juce::File& folder){
    const auto saved=Localisation::current();const auto originalJson=juce::JSON::toString(projectJson(project),false,17);bool ok=true;static const char* codes[]{"en","it","es","fr","de","pt"};static const char* expected[]{"NEW TRACK","NUOVA TRACCIA","NUEVA PISTA","NOUVELLE PISTE","NEUE SPUR","NOVA FAIXA"};
    for(int i=0;i<6;++i){Localisation::setCurrent((Localisation::Language)i);showArrangement();applyLanguage();refreshControls();ok=tr("NEW TRACK")==juce::String::fromUTF8(expected[i])&&ok;
        ok=genreBox.getSelectedItemIndex()==project.settings.genre&&keyBox.getSelectedItemIndex()==project.settings.key&&scaleBox.getSelectedItemIndex()==project.settings.scale&&extensionBox.getSelectedItemIndex()==project.settings.extension&&voicingBox.getSelectedItemIndex()==project.settings.voicing&&ok;
        ok=juce::JSON::toString(projectJson(project),false,17)==originalJson&&ok;auto picture=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String("language-")+codes[i]+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}else ok=false;}
    Localisation::setCurrent(saved);applyLanguage();select(0,1);showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-performance"},{"RHYTHM / VELOCITY","piano-rhythm"},{"Musical tools","piano-tools"},{"GUIDE SOUND","piano-sound"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto image=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*output)&&ok;}}
    Localisation::setCurrent(Localisation::Language::italian);applyLanguage();showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-extended-it"},{"RHYTHM / VELOCITY","piano-rhythm-it"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto picture=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}}
    Localisation::setCurrent(saved);applyLanguage();showInstrument();
    ok=verifyPianoTools(*this,folder)&&ok;
    int tips=0;std::function<void(juce::Component&)> count=[&](auto& c){if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(&c))if(tip->getTooltip().isNotEmpty())++tips;for(auto* child:c.getChildren())count(*child);};count(*this);ok=tips>=50&&ok;
    folder.getChildFile("localisation.txt").replaceWithText(ok?"PASS six interface languages, live translation, extended piano sections and control tooltips":"FAIL localisation or extended controls");showArrangement();return ok;
}
bool MainComponent::writeUiEvidence(const juce::File& folder){
    bool ok=writeLanguageEvidence(folder)&&writeHelpEvidence(folder);auto capture=[&](const juce::String& name){auto screenshot=createComponentSnapshot(getLocalBounds());if(auto out=folder.getChildFile(name+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(screenshot,*out)&&ok;}else ok=false;};
    select(0,1);showInstrument();const auto take=clip().seed;regenerate(Scope::clip);ok=ok&&clip().seed==take;
    showArrangement();ok=ok&&arrangementPort.isVisible()&&!pianoPort.isVisible()&&headerWidth+project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth();capture("interface");
    for(int track:{4,1,0,2,3}){select(track,1);showInstrument();ok=ok&&instrumentPort.isVisible()&&!pianoPort.isVisible()&&sectionSelector.getSelectedId()==selectedSection+1;capture("instrument-"+juce::String(track));if(track==3){instrumentPort.setViewPosition(0,instrumentPanel.getHeight());capture("guitar-mixer");instrumentPort.setViewPosition(0,0);}showEditor();ok=ok&&pianoPort.getHeight()>400&&!arrangementPort.isVisible();if(track>=4)ok=ok&&piano.laneView();if(track==1||track==2)ok=ok&&!piano.laneView();capture("editor-"+juce::String(track));}
    select(3,1);piano.guitarNotes=true;showEditor();ok=ok&&!piano.laneView();capture("guitar-notes");piano.guitarNotes=false;showArrangement();select(4,0);
    folder.getChildFile("ui.txt").replaceWithText(ok?"PASS separate arrangement, instrument panels, full MIDI editors, drum isolation, guitar lanes/notes, apply preserves take":"FAIL interface routing");return ok;
}
void MainComponent::timerCallback(){
    const double beat=audio.position();positionLabel.setText(juce::String((int)beat/4+1).paddedLeft('0',3)+" : "+juce::String((int)beat%4+1)+"   /   "+juce::String((int)project.length()/4),juce::dontSendNotification);playButton.setButtonText(audio.isPlaying()?"PAUSE":"PLAY");arrangement.repaint();piano.repaint();
    if(++ticks%1200==0&&dirty&&!loadingPlugin){audio.capturePluginStates(project);auto path=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger/autosave.fga");path.getParentDirectory().createDirectory();auto result=saveProject(project,path);if(result.failed())message(result.getErrorMessage(),true);}
}
void MainComponent::message(const juce::String& text,bool error){statusLabel.setColour(juce::Label::textColourId,error?Palette::toms:Palette::dim);statusLabel.getProperties().set("labelSource",text);statusLabel.getProperties().set("labelLast",tr(text));statusLabel.setText(tr(text),juce::dontSendNotification);}
bool MainComponent::keyPressed(const juce::KeyPress& k){if(k==juce::KeyPress::escapeKey){showArrangement();return true;}if(k==juce::KeyPress::spaceKey){audio.play(!audio.isPlaying());return true;}if(k.getModifiers().isCommandDown()){int code=k.getKeyCode();if(code=='S'){save(k.getModifiers().isShiftDown());return true;}if(code=='Z'){undo(k.getModifiers().isShiftDown());return true;}if(code=='Y'){undo(true);return true;}}return false;}
bool MainComponent::requestClose(){if(!dirty)return true;return juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,"Modifiche non salvate","Il progetto contiene modifiche. Scartarle?\nUsa SAVE per conservarle prima di chiudere.","Scarta","Annulla");}
void MainComponent::chooseFile(const juce::String& title,const juce::String& filter,int flags,std::function<void(juce::File)> callback){chooser=std::make_unique<juce::FileChooser>(tr(title),projectFile.existsAsFile()?projectFile.getParentDirectory():juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),filter);juce::Component::SafePointer<MainComponent> safe(this);chooser->launchAsync(flags,[safe,callback](const juce::FileChooser& fc){auto f=fc.getResult();if(safe!=nullptr&&f!=juce::File())callback(f);});}
void MainComponent::save(bool as){if(loadingPlugin){message("Attendi il caricamento dello strumento.");return;}audio.capturePluginStates(project);if(as||projectFile==juce::File()){chooseFile("Salva progetto","*.fga",juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](juce::File file){projectFile=file.withFileExtension("fga");project.name=projectFile.getFileNameWithoutExtension().toStdString();auto r=saveProject(project,projectFile);if(r.wasOk()){dirty=false;message("Progetto salvato: "+projectFile.getFullPathName());repaint();}else message(r.getErrorMessage(),true);});}else{auto r=saveProject(project,projectFile);if(r.wasOk()){dirty=false;message("Progetto salvato.");repaint();}else message(r.getErrorMessage(),true);}}
void MainComponent::open(const juce::File& file){if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}if(!requestClose())return;Project next;auto r=loadProject(file,next);if(r.failed()){message(r.getErrorMessage(),true);return;}audio.stop();checkpoint();project=std::move(next);projectFile=file;selectedTrack=selectedSection=0;dirty=false;piano.clearSelection();audio.publish(project);refreshControls();restoreInstruments();select(4,0);showArrangement();message("Progetto aperto. I file audio e i plugin restano esterni al progetto.");repaint();}
void MainComponent::restoreInstruments(){pluginWindow.reset();for(int i=0;i<trackCount;++i){audio.useInternal(i);auto t=project.tracks[(size_t)i];if(t.instrument=="sample"){auto r=audio.loadSample(i,juce::File(utf(t.instrumentPath)),t.sampleRoot);if(r.failed()){audio.markMissing(i);message("Campione mancante su "+utf(t.name)+": ricollegalo con LOAD SAMPLE.",true);}}else if(t.instrument=="plugin")loadPluginFile(i,juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)):juce::File(),utf(t.instrumentId),utf(t.instrumentState));}audio.publish(project);}
void MainComponent::loadSampleFile(int track,const juce::File& file){pluginWindow.reset();checkpoint();auto& t=project.tracks[(size_t)track];int root=track>=4?fgd::defaultNote(track-4):60;auto r=audio.loadSample(track,file,root);if(r.failed()){message(r.getErrorMessage(),true);return;}t.instrument="sample";t.instrumentPath=file.getFullPathName().toStdString();t.sampleRoot=root;t.instrumentId.clear();t.instrumentState.clear();changed(false);message(juce::String::fromUTF8("Campione caricato · nota radice ")+juce::String(root)+" (tasto destro sulla traccia per cambiarla).");}
void MainComponent::loadPluginFile(int track,const juce::File& file,const juce::String& id,const juce::String& state){
    if(id.isNotEmpty())audio.markMissing(track);
    if(auto xml=juce::XmlDocument::parse(id)){
        juce::PluginDescription description;
        if(description.loadFromXml(*xml)){
            if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){message("Plugin mancante: usa LOAD VST3 / AU per ricollegarlo.",true);return;}
            loadPluginDescription(track,description,state,true);return;
        }
    }
    if(!file.exists()){message("Plugin mancante su "+utf(project.tracks[(size_t)track].name)+": usa LOAD VST3 / AU per ricollegarlo.",true);return;}
    juce::OwnedArray<juce::PluginDescription> found;
    for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
    juce::PluginDescription* selected=nullptr;
    for(auto* d:found)if((id.isEmpty()&&selected==nullptr)||d->createIdentifierString()==id)selected=d;
    if(!selected){message("Nessuno strumento compatibile trovato. Scegli il bundle VST3.",true);return;}
    loadPluginDescription(track,*selected,state,id.isNotEmpty());
}
void MainComponent::loadPluginDescription(int track,const juce::PluginDescription& description,const juce::String& state,bool restoring){
    if(!description.isInstrument){message("Scegli uno strumento: questo slot riceve MIDI e produce audio.",true);return;}
    pluginWindow.reset();if(!restoring)checkpoint();++loadingPlugin;loadPluginButton.setEnabled(false);
    juce::Component::SafePointer<MainComponent> safe(this);
    audio.loadPlugin(track,description,[safe,track,description,state,restoring](juce::Result r){
        if(safe==nullptr)return;
        --safe->loadingPlugin;safe->loadPluginButton.setEnabled(safe->loadingPlugin==0);
        if(r.failed()){safe->message(r.getErrorMessage(),true);return;}
        auto& t=safe->project.tracks[(size_t)track];t.instrument="plugin";
        t.instrumentPath=description.fileOrIdentifier.toStdString();
        if(auto xml=description.createXml())t.instrumentId=xml->toString().toStdString();
        t.instrumentState=state.toStdString();
        if(state.isNotEmpty())safe->audio.restorePluginState(track,state);
        if(restoring){safe->audio.updateMix(safe->project);safe->refreshTrack();}else safe->changed(false);
        safe->message("Plugin pronto: "+description.name);
    });
}
void MainComponent::showPluginMenu(){
    juce::PopupMenu menu;menu.addItem(1,"Strumento VST3 da file");
   #if JUCE_MAC
    menu.addItem(2,"Audio Units installate");
   #endif
    juce::Component::SafePointer<MainComponent> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadPluginButton),[safe](int id){
        if(safe==nullptr)return;
        if(id==1){int track=safe->selectedTrack;safe->chooseFile("Scegli un plugin VST3","*.vst3",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[safe,track](juce::File file){if(safe!=nullptr)safe->loadPluginFile(track,file);});}
        if(id==2){
            auto list=std::make_shared<juce::OwnedArray<juce::PluginDescription>>();
            for(int i=0;i<safe->audio.formats.getNumFormats();++i){auto* format=safe->audio.formats.getFormat(i);if(format->getName().containsIgnoreCase("AudioUnit")){
                auto ids=format->searchPathsForPlugins(juce::FileSearchPath(),false,true);
                for(auto key:ids)format->findAllTypesForFile(*list,key);
            }}
            juce::PopupMenu units;for(int i=0;i<list->size();++i)if((*list)[i]->isInstrument)units.addItem(i+1,(*list)[i]->name+juce::String::fromUTF8(" · ")+(*list)[i]->manufacturerName);
            if(units.getNumItems()==0){safe->message("Nessuna Audio Unit strumento installata.");return;}
            int track=safe->selectedTrack;units.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&safe->loadPluginButton),[safe,list,track](int chosen){if(safe!=nullptr&&chosen>0&&chosen<=list->size())safe->loadPluginDescription(track,*(*list)[chosen-1]);});
        }
    });
}
void MainComponent::importFile(const juce::File& f){checkpoint();auto r=importMidi(f,clip(),clipLength());if(r.failed())message(r.getErrorMessage(),true);else{piano.clearSelection();changed();message("MIDI importato nel clip selezionato.");}}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files){for(auto f:files)if(f.endsWithIgnoreCase(".fga")||f.endsWithIgnoreCase(".mid")||f.endsWithIgnoreCase(".midi")||f.endsWithIgnoreCase(".wav")||f.endsWithIgnoreCase(".aiff")||f.endsWithIgnoreCase(".flac"))return true;return false;}
void MainComponent::filesDropped(const juce::StringArray& files,int,int){if(files.isEmpty())return;juce::File f(files[0]);if(f.hasFileExtension("fga"))open(f);else if(f.hasFileExtension("mid;midi"))importFile(f);else loadSampleFile(selectedTrack,f);}
}
