// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InstrumentPanel.h"
#include "MainComponent.h"
#include "UiText.h"
#include "Branding.h"
#include "FamilyControls.h"
#include "PianoStepGrid.h"
#include "../ThirdParty/fasegrooveguitar/Source/engine/Generators.h"
#include "../ThirdParty/fasegroovedrums/Source/engine/Generators.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
namespace fga {
class ChordSummary:public juce::Component,public juce::SettableTooltipClient {
public:
    MainComponent& owner;std::function<void()> edit;
    juce::TextButton progression { tr("PROGRESSION...") };
    IconButton dice { IconButton::Icon::dice };
    ChordSummary(MainComponent& o,std::function<void()> fn,std::function<void()> randomise):owner(o),edit(std::move(fn)){
        setTooltip(controlTip("PROGRESSION"));
        progression.onClick=edit;progression.setTooltip(controlTip("PROGRESSION"));addAndMakeVisible(progression);
        dice.iconColour=Palette::accent;dice.onClick=std::move(randomise);dice.setTooltip(controlTip("RANDOM CHORDS"));addAndMakeVisible(dice);
    }
    void resized()override{
        // Guitar::ChordPanel: progression followed by its 36px dice button.
        // The key selector remains in the shared DAW header.
        progression.setBounds(0,0,getWidth()-40,26);dice.setBounds(getWidth()-36,0,36,26);
    }
    void mouseDown(const juce::MouseEvent&)override{edit();}
    void paint(juce::Graphics& g)override{
        auto names=chordNames(owner.project.settings);const int rows=names.size()>4?2:1,perRow=((int)names.size()+rows-1)/rows;const float w=getWidth()/(float)perRow,h=76.f/rows;
        for(size_t i=0;i<names.size();++i){
            const bool small=rows>1;
            auto r=juce::Rectangle<float>((i%perRow)*w,34+(i/perRow)*h,w,h).reduced(3,small?1.5f:0.f);
            g.setColour(Palette::control);g.fillRoundedRectangle(r,6);
            g.setColour(Palette::panelEdge.brighter(.15f));g.drawRoundedRectangle(r.reduced(.5f),6,1);
            auto inner=r.reduced(4,small?2.f:6.f);
            g.setColour(Palette::dim);g.setFont(FamilyTheme::font(small?8.f:9.5f,true));
            g.drawText(small?juce::String((int)i+1):tr("Bar").toUpperCase()+" "+juce::String((int)i+1),
                       inner.removeFromTop(small?9.f:12.f),juce::Justification::centred);
            const auto name=juce::String::fromUTF8(names[i].c_str());
            g.setColour(Palette::text);g.setFont(FamilyTheme::font(small?12.5f:name.length()>5?15.f:19.f,true));
            g.drawFittedText(name,inner.toNearestInt(),juce::Justification::centred,1,.6f);
        }
        g.setColour(Palette::dim);g.setFont(FamilyTheme::font(10));g.drawText(tr("The chord slots play"),0,getHeight()-28,getWidth(),26,juce::Justification::centred);
    }
};
// DrumPads geometry and rendering from Drums::PluginEditor. In the multitrack
// arranger a pad selects that drum's settings; right-click toggles track mute.
class DrumPads : public juce::Component,public juce::SettableTooltipClient {
public:
    MainComponent& owner;
    explicit DrumPads(MainComponent& o):owner(o){setTooltip(tr("Click a pad to select its drum track. Right-click: mute."));}
    juce::Rectangle<float> padBounds(int lane)const{
        const float w=getWidth()/4.f,h=getHeight()/4.f;
        auto r=juce::Rectangle<float>(w*(lane%4),h*(lane/4),w,h).reduced(4);
        if(lane==fgd::numLanes-1)r=juce::Rectangle<float>(0,h*3,getWidth(),h).reduced(4);
        return r;
    }
    void paint(juce::Graphics& g)override{
        for(int lane=0;lane<fgd::numLanes;++lane){
            const auto r=padBounds(lane);const auto& track=owner.project.tracks[(size_t)lane+4];
            const auto ink=lane==0?Palette::accent:lane<4?Palette::accent2:lane<7||lane==12?Palette::cymbals:lane==11?Palette::toms:Palette::percussion;
            const float light=std::clamp(owner.audio.level(lane+4),0.f,1.f);const bool selected=owner.selectedTrack==lane+4;
            g.setColour(Palette::control.interpolatedWith(ink,track.mute?0.f:.12f+.75f*light));g.fillRoundedRectangle(r,6);
            g.setColour(selected?Palette::text:ink.withAlpha(track.mute?.2f:.55f));g.drawRoundedRectangle(r.reduced(.5f),6,selected?2.f:1.f);
            auto text=r.reduced(6,4);g.setColour(track.mute?Palette::dim:Palette::text);g.setFont(FamilyTheme::font(11,true));g.drawText(tr(fgd::laneShortName(lane)),text.removeFromTop(text.getHeight()*.6f),juce::Justification::bottomLeft);
            g.setFont(FamilyTheme::font(9.5f));g.setColour(Palette::dim);g.drawText(track.mute?tr("MUTE"):track.instrument=="sample"?tr("SAMPLE"):juce::String(track.engine.midiNote<0?fgd::defaultNote(lane):track.engine.midiNote),text,juce::Justification::topLeft);
        }
    }
    void mouseDown(const juce::MouseEvent& e)override{
        for(int lane=0;lane<fgd::numLanes;++lane)if(padBounds(lane).contains(e.position)){
            if(e.mods.isPopupMenu()){owner.checkpoint();auto& t=owner.project.tracks[(size_t)lane+4];t.mute=!t.mute;owner.changed(false);repaint();}
            else{juce::Component::SafePointer<MainComponent> safe(&owner);juce::MessageManager::callAsync([safe,lane]{if(safe){safe->select(lane+4,safe->selectedSection);safe->showInstrument();}});}
            return;
        }
    }
};
void drawFamilyLogo(juce::Graphics& g,juce::Rectangle<float> area){Branding::drawFullLogo(g,area);}
InstrumentPanel::InstrumentPanel(MainComponent& m):owner(m){
    addChildComponent(circle);
    circle.setFreeMode(Localisation::setting("freeCircle","1")!="0");
    circle.onPick=[this](int root,bool minor){owner.checkpoint();owner.project.settings.key=root;owner.project.settings.scale=minor?2:0;owner.changed(false);updateCircle();};
    circle.onMoveChord=[this](int slot,int degree,int root,bool minor){auto& s=owner.project.settings;if(slot<0||slot>=s.chordCount)return;owner.checkpoint();if(degree>=0){s.degrees[(size_t)slot]=degree;s.freeChords[(size_t)slot]=-1;}else s.freeChords[(size_t)slot]=sgk::freeChordCode(root-s.key,minor);owner.changed(false);updateCircle();};
}
void InstrumentPanel::updateCircle(){
    auto s=pianoEngineSettings(owner.project,owner.selectedTrack,owner.clip().seed,owner.clip().motifSeed);
    std::vector<CircleOfFifths::ChordMark> chords,diatonic;
    std::array<sgk::Chord,8> cs;std::array<sgk::VoicedChord,8> voiced;
    for(int i=0;i<s.progressionLength;++i){cs[(size_t)i]=sgk::GrooveEngine::chordForSlot(s,i);voiced[(size_t)i]=sgk::voiceChord(cs[(size_t)i],s.voicing,s.centre,nullptr);}
    auto scores=sgk::GrooveEngine::harmonyScores(s,cs,voiced);std::vector<float> transitions;
    for(int i=0;i<s.progressionLength;++i){const auto& c=cs[(size_t)i];chords.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::GrooveEngine::numeralForSlot(s,i).c_str()),scores.slot[(size_t)i]});transitions.push_back(scores.transition[(size_t)i]);}
    for(int i=0;i<7;++i){auto c=sgk::buildChord(s.keyRoot,s.scale,i,s.extension);diatonic.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::romanNumeral(i,c).c_str()),1});}
    circle.setState(s.keyRoot,s.scale==2,tr(sgk::scaleName(s.scale)),chords,transitions,-1,diatonic);
}
void InstrumentPanel::refreshFromProject(){
    const auto& t=owner.project.tracks[(size_t)owner.selectedTrack];const auto& s=owner.project.settings;
    for(auto& c:controls){const auto name=c.component->getProperties()["fgaControl"].toString();if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get())){if(name=="VOLUME"||name=="Drum volume")slider->setValue(t.gain*100,juce::dontSendNotification);if(name=="Part volume")slider->setValue(t.engine.laneVolume[(size_t)guitarLane]*100,juce::dontSendNotification);for(int lane=0;lane<12&&owner.selectedTrack==3;++lane)if(name==juce::String(fgg::laneShortName(lane))+" VOLUME")slider->setValue(t.engine.laneVolume[(size_t)lane]*100,juce::dontSendNotification);}
        if(auto* b=dynamic_cast<juce::Button*>(c.component.get())){if(name=="HEAR"){b->setToggleState(!t.mute,juce::dontSendNotification);b->setEnabled(t.instrument!="plugin");}if(name=="Mute")b->setToggleState(t.mute,juce::dontSendNotification);}
        if(auto* box=dynamic_cast<juce::ComboBox*>(c.component.get())){if(name=="Sound"){box->setSelectedItemIndex(t.sound,juce::dontSendNotification);box->setEnabled(t.instrument=="internal");}if(name=="Section")box->setSelectedItemIndex(owner.selectedSection,juce::dontSendNotification);if(name=="Voicing")box->setSelectedItemIndex(s.voicing,juce::dontSendNotification);if(name=="Extensions")box->setSelectedItemIndex(s.extension,juce::dontSendNotification);if(name.startsWith("Degree "))box->setSelectedItemIndex(s.degrees[(size_t)std::clamp(name.fromLastOccurrenceOf(" ",false,false).getIntValue()-1,0,7)],juce::dontSendNotification);}
    }
    if(owner.selectedTrack<3)updateCircle();repaint();
}
void InstrumentPanel::heading(const juce::String& name){headings.push_back({name,(int)controls.size()});}
void InstrumentPanel::apply(std::function<void(EngineOptions&)> fn){owner.checkpoint();fn(owner.project.tracks[(size_t)owner.selectedTrack].engine);owner.project.normalise();owner.changed(false);owner.message("Settings apply on the next generation; the MIDI already written stays editable.");}
void InstrumentPanel::knob(const juce::String& name,double lo,double hi,double step,double value,std::function<void(EngineOptions&,double)> fn,const juce::String& suffix){
    auto c=std::make_unique<juce::Slider>();c->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);c->setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,owner.selectedTrack<3?16:15);c->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);c->setRange(lo,hi,step);c->setValue(value,juce::dontSendNotification);c->setTextValueSuffix(suffix);c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onDragStart=[this]{owner.checkpoint();};auto* raw=c.get();c->onValueChange=[this,raw,fn,name]{fn(owner.project.tracks[(size_t)owner.selectedTrack].engine,raw->getValue());if(name=="VOLUME"||name=="Drum volume")owner.gainSlider.setValue(raw->getValue(),juce::dontSendNotification);owner.dirty=true;owner.audio.updateMix(owner.project);if(name=="VOLUME"||name=="Drum volume"||name=="Part volume"||name.endsWith(" VOLUME"))refreshFromProject();};addAndMakeVisible(*c);controls.push_back({std::move(c),name,true});
}
void InstrumentPanel::choice(const juce::String& name,const juce::StringArray& items,int selected,std::function<void(EngineOptions&,int)> fn){auto c=std::make_unique<juce::ComboBox>();c->addItemList(items,1);c->setSelectedItemIndex(selected,juce::dontSendNotification);auto* raw=c.get();c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onChange=[this,raw,fn]{apply([&](EngineOptions& e){fn(e,raw->getSelectedItemIndex());});};addAndMakeVisible(*c);controls.push_back({std::move(c),name,false});}
void InstrumentPanel::sharedChoice(const juce::String& name,const juce::StringArray& items,int selected,std::function<void(int)> fn){auto c=std::make_unique<juce::ComboBox>();c->addItemList(items,1);c->setSelectedItemIndex(selected,juce::dontSendNotification);if(selected<0)c->setTextWhenNothingSelected("Browse...");c->getProperties().set("fgaControl",name);auto* raw=c.get();c->onChange=[this,raw,fn]{owner.checkpoint();fn(raw->getSelectedItemIndex());owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),name,false});}
void InstrumentPanel::action(const juce::String& name,std::function<void()> fn){std::unique_ptr<juce::Button> c;if(name=="RND LANE"||name=="<"||name==">"){auto icon=std::make_unique<IconButton>(name=="<"?IconButton::Icon::left:name==">"?IconButton::Icon::right:IconButton::Icon::dice);if(name=="RND LANE")icon->iconColour=Palette::accent;c=std::move(icon);}else c=std::make_unique<juce::TextButton>(tr(name=="RANDOM CHORDS"?"Random":name));c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onClick=std::move(fn);addAndMakeVisible(*c);controls.push_back({std::move(c),{},false});}
void InstrumentPanel::toggle(const juce::String& name,bool value,std::function<void(EngineOptions&,bool)> fn){
    std::unique_ptr<juce::Button> c;
    const bool compact=name=="Part mute"||name=="Mute"||name=="Crash after fill"||name=="Strum after fill";
    const auto text=tr((name=="Part mute"||name=="Mute")?"MUTE":name=="Crash after fill"?"CRASH":name=="Strum after fill"?"BIG STRUM":name=="HEAR"?"Guide sound":name);
    if(compact)c=std::make_unique<juce::TextButton>(text);else c=std::make_unique<juce::ToggleButton>(text);
    c->setClickingTogglesState(true);c->setToggleState(value,juce::dontSendNotification);auto* raw=c.get();c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onClick=[this,raw,fn]{apply([&](EngineOptions& e){fn(e,raw->getToggleState());});};addAndMakeVisible(*c);controls.push_back({std::move(c),{},false});
}

void InstrumentPanel::rebuild(){
    controls.clear();headings.clear();const int track=owner.selectedTrack;const auto e=owner.project.tracks[(size_t)track].engine;const bool piano=track==0,bass=track==1,strings=track==2,guitar=track==3,drum=track>=4;
    if(piano){heading("HARMONY & SCALES");juce::StringArray voicings,extensions;for(int i=0;i<sgk::numVoicings;++i)voicings.add(sgk::voicingName(i));for(int i=0;i<sgk::numExtensions;++i)extensions.add(sgk::extensionName(i));sharedChoice("Voicing",voicings,owner.project.settings.voicing,[this](int v){owner.project.settings.voicing=v;});sharedChoice("Extensions",extensions,owner.project.settings.extension,[this](int v){owner.project.settings.extension=v;});}
    heading(piano?"PIANO / GROOVE":bass?"BASS / GROOVE":strings?"STRINGS / GROOVE":guitar?"GUITAR / GROOVE":"DRUMS / "+juce::String::fromUTF8(owner.project.tracks[(size_t)track].name.c_str()).toUpperCase());
    juce::StringArray patterns{"Genre / automatic"};if(track<3)for(const auto& p:sgk::groovePatterns())patterns.add(juce::String::fromUTF8((p.category+" / "+p.name).c_str()));else if(guitar)for(const auto& p:fgg::guitarPatterns())patterns.add(juce::String::fromUTF8(p.name.c_str()));else for(const auto& p:fgd::drumPatterns())patterns.add(juce::String::fromUTF8(p.name.c_str()));if(track<3){patterns.add("* Random riff *");patterns.add("* User pattern *");}choice("GROOVE",patterns,e.pattern+1,[](auto& o,int v){o.pattern=v-1;});
    for(int direction:{-1,1})action(direction<0?"<":">",[this,direction]{
        const int track=owner.selectedTrack;int count=track<3?(int)sgk::groovePatterns().size():track==3?(int)fgg::guitarPatterns().size():(int)fgd::drumPatterns().size();
        apply([direction,count](auto& e){e.pattern=e.pattern<0?(direction>0?0:count-1):(e.pattern+direction+count)%count;});
        juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});
    });
    choice("GROOVE LENGTH",{"1 bar","2 bars","4 bars"},e.grooveBars==4?2:e.grooveBars-1,[](auto& o,int v){o.grooveBars=v==2?4:v+1;});
    if(bass){juce::StringArray modes;for(int i=0;i<sgk::numBassModes;++i)modes.add(sgk::bassModeName(i));auto c=std::make_unique<juce::ComboBox>();c->addItemList(modes,1);c->setSelectedItemIndex(owner.project.settings.bassMode,juce::dontSendNotification);auto* raw=c.get();c->getProperties().set("fgaControl","BASS LINE");c->onChange=[this,raw]{owner.checkpoint();owner.project.settings.bassMode=raw->getSelectedItemIndex();owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),"BASS LINE",false});choice("BASS FEEL",{"Classic","Sub","Sub + sidechain gaps"},e.bassFeel,[](auto& o,int v){o.bassFeel=v;});}
    if(strings){juce::StringArray modes;for(int i=0;i<sgk::numStringsModes;++i)modes.add(sgk::stringsModeName(i));auto c=std::make_unique<juce::ComboBox>();c->addItemList(modes,1);c->setSelectedItemIndex(owner.project.settings.stringsMode,juce::dontSendNotification);auto* raw=c.get();c->getProperties().set("fgaControl","STRINGS STYLE");c->onChange=[this,raw]{owner.checkpoint();owner.project.settings.stringsMode=raw->getSelectedItemIndex();owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),"STRINGS STYLE",false});}
    knob("SWING",0,150,1,(e.swing<0?owner.project.settings.swing:e.swing)*100,[](auto& o,double v){o.swing=v/100;}," %");knob("HUMANIZE",0,100,1,(e.humanize<0?owner.project.settings.humanize:e.humanize)*100,[](auto& o,double v){o.humanize=v/100;}," %");knob("DENSITY",0,100,1,(e.density<0?owner.project.settings.density:e.density)*100,[](auto& o,double v){o.density=v/100;}," %");knob("VELOCITY",1,127,1,e.velocity,[](auto& o,double v){o.velocity=(int)v;});
    if(piano){knob("GATE",2,200,1,e.gate*100,[](auto& o,double v){o.gate=v/100;}," %");knob("REGISTER",24,108,1,e.centre,[](auto& o,double v){o.centre=(int)v;});knob("STRUM",0,100,1,e.strum*100,[](auto& o,double v){o.strum=v/100;}," %");choice("STRUM DIRECTION",{"Up","Down","Alternate"},e.strumDirection,[](auto& o,int v){o.strumDirection=v;});choice("MOVEMENT",{"Off","Subtle","Lively"},e.movement,[](auto& o,int v){o.movement=v;});auto range=std::make_unique<juce::Slider>(juce::Slider::TwoValueHorizontal,juce::Slider::NoTextBox);range->getProperties().set("fgaControl","Range");range->setRange(0,127,1);range->setMinAndMaxValues(e.rangeLow,e.rangeHigh,juce::dontSendNotification);range->setTooltip(controlTip("RANGE LOW"));range->onDragStart=[this]{owner.checkpoint();};auto* rangePtr=range.get();range->onValueChange=[this,rangePtr]{auto& o=owner.project.tracks[(size_t)owner.selectedTrack].engine;o.rangeLow=(int)rangePtr->getMinValue();o.rangeHigh=(int)rangePtr->getMaxValue();owner.dirty=true;};addAndMakeVisible(*range);controls.push_back({std::move(range),"Range",false});toggle("Top +8va",e.octaveTop,[](auto& o,bool v){o.octaveTop=v;});toggle("Realism",e.realism,[](auto& o,bool v){o.realism=v;});
        juce::StringArray top,approach,feel;for(int i=0;i<sgk::numTopLines;++i)top.add(sgk::topLineName(i));for(int i=0;i<sgk::numApproachStyles;++i)approach.add(sgk::approachStyleName(i));for(int i=0;i<sgk::numHumanizeStyles;++i)feel.add(sgk::humanizeStyleName(i));choice("TOP LINE",top,e.topLine,[](auto& o,int v){o.topLine=v;});choice("APPROACH",approach,e.approach,[](auto& o,int v){o.approach=v;});choice("HUMANIZE STYLE",feel,e.humanizeStyle,[](auto& o,int v){o.humanizeStyle=v;});knob("EVOLVE",0,32,4,e.evolveBars,[](auto& o,double v){o.evolveBars=(int)v;}," bars");}
    else if(guitar||drum){knob("ACCENTS",0,100,1,e.accent*100,[](auto& o,double v){o.accent=v/100;}," %");if(guitar){knob("STRUM",0,80,1,e.strum*80,[](auto& o,double v){o.strum=v/80;}," ms");}else{juce::StringArray notes;for(int n=0;n<128;++n)notes.add(juce::MidiMessage::getMidiNoteName(n,true,true,3)+" / "+juce::String(n));choice("MIDI NOTE",notes,e.midiNote<0?fgd::defaultNote(track-4):e.midiNote,[](auto& o,int v){o.midiNote=v;});toggle("Genre feel",e.genreFeel,[](auto& o,bool v){o.genreFeel=v;});toggle("Bass space",e.bassSpace,[](auto& o,bool v){o.bassSpace=v;});}}
    if(track<3){
        heading("PERFORMANCE");
        knob("Riff seed",0,99999,1,e.riffSeed,[](auto& o,double v){o.riffSeed=(std::uint32_t)v;});
        knob("Humanize seed",0,99999,1,e.humanizeSeed,[](auto& o,double v){o.humanizeSeed=(std::uint32_t)v;});
        action("NEW FEEL",[this]{apply([](auto& o){o.humanizeSeed=(std::uint32_t)juce::Random::getSystemRandom().nextInt({1,100000});});rebuild();});
        if(piano){toggle("Pedal & Expression CC",e.pedalCC,[](auto& o,bool v){o.pedalCC=v;});for(int i=0;i<owner.project.settings.chordCount;++i)knob("Inversion "+juce::String(i+1),-3,3,1,e.inversions[(size_t)i],[i](auto& o,double v){o.inversions[(size_t)i]=(int)v;});}
    }
    heading("PHRASES / FILLS");juce::StringArray fills{"Mix / automatic"};const int count=track<3?sgk::numFillStyles:guitar?fgg::numFillStyles:fgd::numFillStyles;for(int i=0;i<count;++i)fills.add(track<3?sgk::fillStyleName(i):guitar?fgg::fillStyleName(i):fgd::fillStyleName(i));choice("FILL STYLE",fills,e.fillStyle+1,[](auto& o,int v){o.fillStyle=v-1;});juce::StringArray periods{"Off"};for(int i=1;i<=32;++i)periods.add(juce::String(i)+" bars");choice("FILL EVERY",periods,e.fillEvery,[](auto& o,int v){o.fillEvery=v;});choice("VARIATION EVERY",periods,e.variationEvery,[](auto& o,int v){o.variationEvery=v;});if(drum||guitar)toggle(guitar?"Strum after fill":"Crash after fill",e.afterFill,[](auto& o,bool v){o.afterFill=v;});for(size_t i=0;i<4;++i)choice("SEGMENT "+juce::String("ABCD").substring((int)i,(int)i+1),{"A","B","C","D"},e.segments[i],[i](auto& o,int v){o.segments[i]=v;});
    if(track<3){heading("RHYTHM / VELOCITY");auto grid=std::make_unique<PianoStepGrid>(owner);addAndMakeVisible(*grid);controls.push_back({std::move(grid),{},false});
        for(int i=0;i<4;++i){choice("Velocity order "+juce::String("ABCD").substring(i,i+1),{"A","B","C","D"},e.velocityOrder[(size_t)i],[i](auto& o,int v){o.velocityOrder[(size_t)i]=v;});toggle("Lock "+juce::String("ABCD").substring(i,i+1),e.segmentLocked[(size_t)i],[i](auto& o,bool v){o.segmentLocked[(size_t)i]=v;});}
        action(bass?"RND BASS":"RND PIANO",[this]{owner.randomPianoMaterial(false,owner.selectedTrack==1);rebuild();});action("RND VELOCITY",[this]{owner.randomPianoMaterial(true,false);rebuild();});
        heading("Musical tools");for(const auto& pair:std::initializer_list<std::pair<const char*,int>>{{"DNA",1},{"MORPH",2},{"A/B",3},{"VARIANTS",4},{"IMPORT MIDI",6},{"PRESETS",7},{"LEARN",8}})action(pair.first,[this,tool=pair.second]{owner.showPianoTools(tool);});
    }
    heading("GUIDE SOUND");knob("TONE",0,100,1,e.tone*100,[](auto& o,double v){o.tone=v/100;}," %");
    knob("REVERB",0,100,1,e.reverb*100,[](auto& o,double v){o.reverb=v/100;}," %");
    knob("VOLUME",0,200,1,owner.project.tracks[(size_t)track].gain*100,[this](auto&,double v){owner.project.tracks[(size_t)owner.selectedTrack].gain=v/100;}," %");
    juce::StringArray sounds;int soundCount=strings?1:track<2?7:4;
    for(int i=0;i<soundCount;++i)sounds.add(strings?"String ensemble":track<2?guideSoundName(i):guitar?GuideGuitar::toneName(i):GuideDrums::kitName(i));
    sharedChoice("Sound",sounds,std::min(owner.project.tracks[(size_t)track].sound,soundCount-1),[this](int value){owner.project.tracks[(size_t)owner.selectedTrack].sound=value;});
    action("BUILT-IN",[this]{if(owner.soundButton.onClick)owner.soundButton.onClick();refreshFromProject();});
    action("LOAD SAMPLE",[this]{if(owner.sampleButton.onClick)owner.sampleButton.onClick();});
    toggle("HEAR",!owner.project.tracks[(size_t)track].mute,[this](auto&,bool value){owner.project.tracks[(size_t)owner.selectedTrack].mute=!value;});
    auto sections=std::make_unique<juce::ComboBox>();for(size_t i=0;i<owner.project.sections.size();++i)sections->addItem(juce::String::fromUTF8(owner.project.sections[i].name.c_str()),(int)i+1);sections->setSelectedItemIndex(owner.selectedSection,juce::dontSendNotification);sections->getProperties().set("fgaControl","Section");auto* selected=sections.get();sections->onChange=[this,selected]{owner.select(owner.selectedTrack,selected->getSelectedItemIndex());};addAndMakeVisible(*sections);controls.push_back({std::move(sections),"Section",false});
    if(piano||bass||strings){
        for(int i=0;i<3;++i)action(i==0?"Piano":i==1?"Bass":"Strings",[this,i]{juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self,i]{if(self){self->owner.select(i,self->owner.selectedSection);self->owner.showInstrument();}});});
        auto free=std::make_unique<juce::ToggleButton>(tr("Free chords"));free->getProperties().set("fgaControl","Free chords");free->setToggleState(Localisation::setting("freeCircle","1")!="0",juce::dontSendNotification);auto* rawFree=free.get();free->onClick=[this,rawFree]{circle.setFreeMode(rawFree->getToggleState());Localisation::setSetting("freeCircle",rawFree->getToggleState()?"1":"0");};addAndMakeVisible(*free);controls.push_back({std::move(free),{},false});
    }
    if(drum){
        juce::StringArray lanes;for(int i=0;i<13;++i)lanes.add(fgd::laneName(i));
        auto selector=std::make_unique<juce::ComboBox>();selector->addItemList(lanes,1);selector->setSelectedItemIndex(track-4,juce::dontSendNotification);selector->getProperties().set("fgaControl","Drum");auto* raw=selector.get();selector->onChange=[this,raw]{const int track=raw->getSelectedItemIndex()+4;juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self,track]{if(self){self->owner.select(track,self->owner.selectedSection);self->owner.showInstrument();}});};addAndMakeVisible(*selector);controls.push_back({std::move(selector),{},false});
        auto pads=std::make_unique<DrumPads>(owner);pads->getProperties().set("fgaControl","Drum pads");addAndMakeVisible(*pads);controls.push_back({std::move(pads),{},false});
        action("RND LANE",[this]{owner.regenerate(Scope::clip);});
        toggle("Mute",owner.project.tracks[(size_t)track].mute,[this](auto&,bool value){owner.project.tracks[(size_t)owner.selectedTrack].mute=value;});
        knob("LEVEL",1,127,1,e.velocity,[](auto& o,double v){o.velocity=(int)v;});
        knob("Drum volume",0,200,1,owner.project.tracks[(size_t)track].gain*100,[this](auto&,double v){owner.project.tracks[(size_t)owner.selectedTrack].gain=v/100;}," %");
    }
    if(track<3){knob("CHORUS",0,100,1,e.chorus*100,[](auto& o,double v){o.chorus=v/100;}," %");knob("PUMP",0,100,1,e.pump*100,[](auto& o,double v){o.pump=v/100;}," %");knob("PUMP RELEASE",5,100,1,e.pumpRelease*100,[](auto& o,double v){o.pumpRelease=v/100;}," % beat");choice("Sidechain Rate",{"1/4","1/8","1/2","1/16"},e.pumpRate,[](auto& o,int v){o.pumpRate=v;});choice("Sidechain Source",{"Tempo (internal)","Kick MIDI"},e.pumpSource,[](auto& o,int v){o.pumpSource=v;});}
    if(guitar||drum){heading(guitar?"GUITAR / LANE MIXER":"DRUM / POCKET");const int lanes=guitar?12:1;for(int i=0;i<lanes;++i){const size_t l=(size_t)i;const juce::String name=guitar?fgg::laneShortName(i):fgd::laneName(track-4);if(guitar){knob(name+" VOLUME",0,200,1,e.laneVolume[l]*100,[l](auto& o,double v){o.laneVolume[l]=v/100;}," %");choice(name+" OCTAVE",{"-2","-1","0","+1","+2"},e.laneOctave[l]+2,[l](auto& o,int v){o.laneOctave[l]=v-2;});toggle(name+" MUTE",e.laneMute[l],[l](auto& o,bool v){o.laneMute[l]=v;});}knob(name+" SWING",0,150,1,e.laneSwing[l]*100,[l](auto& o,double v){o.laneSwing[l]=v/100;}," %");knob(name+" PUSH / PULL",-100,100,1,e.laneShift[l],[l](auto& o,double v){o.laneShift[l]=v;}," ms");}}
    if(guitar){
        action("PLAYING...",[this]{auto e=owner.project.tracks[3].engine;juce::PopupMenu menu;menu.addItem(1,tr("Let ring"),true,e.letRing);menu.addItem(2,tr("Room for band"),true,e.roomForBand);menu.addItem(3,tr("Push changes"),true,e.pushChanges);juce::Component::SafePointer<InstrumentPanel> self(this);menu.showMenuAsync(juce::PopupMenu::Options(),[self](int id){if(!self||id==0)return;self->apply([id](auto& o){if(id==1)o.letRing=!o.letRing;if(id==2)o.roomForBand=!o.roomForBand;if(id==3)o.pushChanges=!o.pushChanges;});self->rebuild();});});
        action("SOLISTA",[this]{if(owner.leadButton.onClick)owner.leadButton.onClick();});
        if(auto* button=dynamic_cast<juce::Button*>(controls.back().component.get())){button->setClickingTogglesState(true);button->setToggleState(owner.project.settings.guitarLead,juce::dontSendNotification);}
        juce::StringArray lanes;for(int i=0;i<12;++i)lanes.add(fgg::laneShortName(i));
        sharedChoice("Guitar part",lanes,guitarLane,[this](int v){guitarLane=v;juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
        knob("Part level",0,200,1,e.laneLevel[(size_t)guitarLane]*100,[lane=guitarLane](auto& o,double v){o.laneLevel[(size_t)lane]=v/100;}," %");
        knob("Part volume",0,200,1,e.laneVolume[(size_t)guitarLane]*100,[lane=guitarLane](auto& o,double v){o.laneVolume[(size_t)lane]=v/100;}," %");
        choice("Part octave",{"-2","-1","0","+1","+2"},e.laneOctave[(size_t)guitarLane]+2,[lane=guitarLane](auto& o,int v){o.laneOctave[(size_t)lane]=v-2;});
        toggle("Part mute",e.laneMute[(size_t)guitarLane],[lane=guitarLane](auto& o,bool v){o.laneMute[(size_t)lane]=v;});
        auto chords=std::make_unique<ChordSummary>(owner,[this]{owner.showChordMenu();},[this]{
            owner.checkpoint();randomiseHarmony(owner.project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());owner.changed(false);
            juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});
        });chords->getProperties().set("fgaControl","Chord summary");addAndMakeVisible(*chords);controls.push_back({std::move(chords),{},false});
    }
    if(track>=3)action("NEW FEEL",[this]{apply([](auto& o){o.humanizeSeed=(std::uint32_t)juce::Random::getSystemRandom().nextInt({1,100000});});});
    action("NEW RIFF",[this]{const auto& e=owner.project.tracks[(size_t)owner.selectedTrack].engine;if(owner.selectedTrack<3&&std::any_of(e.segmentLocked.begin(),e.segmentLocked.end(),[](bool locked){return locked;}))owner.randomPianoMaterial(false,owner.selectedTrack==1);else apply([](auto& o){o.riffSeed=(std::uint32_t)juce::Random::getSystemRandom().nextInt({1,100000});o.pattern=-1;});juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
    if(track<3){
        juce::StringArray presets;for(const auto& p:sgk::progressionPresets())presets.add(juce::String(p.category)+" / "+p.name);
        sharedChoice("Ready progressions",presets,-1,[this](int v){chooseProgression(owner.project,v);updateCircle();juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
        action("RANDOM CHORDS",[this]{owner.checkpoint();randomiseHarmony(owner.project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());owner.changed(false);updateCircle();juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
        juce::StringArray lengths;for(int i=1;i<=8;++i)lengths.add(juce::String(i));
        sharedChoice("Length",lengths,owner.project.settings.chordCount-1,[this](int v){owner.project.settings.chordCount=v+1;juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
        sharedChoice("Each chord",{"1/2 bar","1 bar","2 bars","4 bars"},owner.project.settings.chordSteps==8?0:owner.project.settings.chordSteps==16?1:owner.project.settings.chordSteps==32?2:3,[this](int v){owner.project.settings.chordSteps=8<<v;});
        for(int i=0;i<8;++i)sharedChoice("Degree "+juce::String(i+1),{"I","ii","iii","IV","V","vi","vii"},owner.project.settings.degrees[(size_t)i],[this,i](int v){owner.project.settings.degrees[(size_t)i]=v;owner.project.settings.freeChords[(size_t)i]=-1;updateCircle();repaint();});
    }
    circle.setVisible(track<3);if(track<3)updateCircle();
    for(auto& c:controls){const auto name=c.component->getProperties()["fgaControl"].toString();if(name.startsWith("Degree "))c.component->setEnabled(name.fromLastOccurrenceOf(" ",false,false).getIntValue()<=owner.project.settings.chordCount);}
    for(auto& c:controls){const auto name=c.component->getProperties()["fgaControl"].toString();if(auto* b=dynamic_cast<juce::Button*>(c.component.get())){int i=name=="Piano"?0:name=="Bass"?1:name=="Strings"?2:-1;if(i>=0){b->setToggleState(track==i,juce::dontSendNotification);b->setColour(juce::TextButton::buttonOnColourId,i==0?Palette::accent:i==1?Palette::accent2:Palette::strings);b->setConnectedEdges(i==0?juce::Button::ConnectedOnRight:i==2?juce::Button::ConnectedOnLeft:juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);}}}
    translateControls(*this);resized();refreshFromProject();repaint();
}
juce::String InstrumentPanel::layoutProblems()const{
    juce::String problems;
    for(size_t i=0;i<controls.size();++i){const auto& c=controls[i];auto r=c.component->getBounds();auto name=c.component->getProperties()["fgaControl"].toString();if(r.isEmpty()||!getLocalBounds().contains(r))problems+="Outside panel: "+name+"\n";
        for(size_t j=i+1;j<controls.size();++j)if(r.intersects(controls[j].component->getBounds()))problems+="Overlapping controls: "+name+" / "+controls[j].component->getProperties()["fgaControl"].toString()+"\n";
    }
    return problems;
}
// Port of Piano::layoutFull and Drums/Guitar::layout: the instrument owns
// panel composition. The DAW supplies transport, key, routing and the large editor.
void InstrumentPanel::resized(){
    if(getWidth()<100)return;
    cards.clear();for(auto& c:controls)c.component->setBounds({});
    const int W=getWidth();const int track=owner.selectedTrack;const bool piano=track<3,guitar=track==3;
    auto card=[&](const juce::String& title,int x,int y,int w,int h){cards.push_back({title,{x,y,w,h}});};
    auto put=[&](const juce::String& name,int x,int y,int w,int h=28){
        for(auto& c:controls){
            if(c.component->getProperties()["fgaControl"].toString()!=name)continue;
            // Piano::Knob reserves its first 16px for the caption;
            // Drums/Guitar::Knob reserve their last 14px instead.
            const bool pianoKnob=c.knob&&track<3&&!name.startsWith("Inversion ");
            const int captionHeight=pianoKnob?16:14;
            c.component->setBounds(x,y+(pianoKnob?16:0),w,
                                   c.knob?std::max(22,h-captionHeight):h);
            return;
        }
    };
    auto tool=[&](const juce::String& name,int x,int y,int w){put(name,x,y,w,24);};
    const int progHeight=track<3?138:0;
    if(track<3){
        card("PROGRESSION",18,10,W-36,128);
        put("Ready progressions",32,59,134,28);tool("RANDOM CHORDS",174,59,74);put("Length",32,105,94,24);put("Each chord",134,105,180,24);
        tool("VARIANTS",150,17,96);tool("LEARN",256,59,58);
        auto names=chordNames(owner.project.settings);const int slotW=(W-346)/8;
        for(int i=0;i<8;++i){const int x=328+i*slotW;put("Degree "+juce::String(i+1),x,101,slotW-8,24);
            if(piano&&track==0){put("Inversion "+juce::String(i+1),x,40,slotW-8,22);for(auto& c:controls)if(c.component->getProperties()["fgaControl"].toString()=="Inversion "+juce::String(i+1))if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get())){slider->setSliderStyle(juce::Slider::IncDecButtons);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,32,20);}}
        }
    }
    const int y=progHeight+10;
    if(piano){
        // Same four adjacent panels as the extended Piano, without shared KEY/BPM/LINK.
        const int hw=300,cw=262,sw=252,gx=18+hw+10+cw+10,gw=W-gx-sw-28;
        card("HARMONY & SCALES",18,y,hw,340);card("CIRCLE OF FIFTHS",328,y,cw,340);
        card("GROOVE",gx,y,gw,340);card("GUIDE SOUND",W-sw-18,y,sw,340);
        put("Extensions",32,y+48,125);put("APPROACH",167,y+48,137);
        tool("Piano",32,y+82,90);tool("Bass",122,y+82,90);tool("Strings",212,y+82,90);put("Voicing",40,y+132,150);if(track==0)put("A/B",156,y+114,34,15);put("TOP LINE",198,y+132,98);
        put("BASS LINE",40,y+132,256);put("STRINGS STYLE",40,y+132,256);
        put("REGISTER",40,y+187,76,78);put("STRUM",120,y+187,76,78);
        put("MOVEMENT",202,y+165,102);put("Range",202,y+230,102,24);
        put("Top +8va",32,y+288,128,24);put("STRUM DIRECTION",168,y+286,136,24);
        put("BASS FEEL",32,y+184,272);
        circle.setBounds(338,y+30,cw-20,268);put("Free chords",342,y+308,230,24);
        put("GROOVE",gx+14,y+46,gw-170);tool("<",gx+gw-152,y+50,20);tool(">",gx+gw-132,y+50,20);tool("NEW RIFF",gx+gw-98,y+46,84);const int third=(gw-44)/3;put("Section",gx+14,y+98,third);put("GROOVE LENGTH",gx+22+third,y+98,third);put("VARIATION EVERY",gx+30+third*2,y+98,third);
        put("FILL EVERY",gx+14,y+150,(gw-38)*2/5,28);put("FILL STYLE",gx+24+(gw-38)*2/5,y+150,(gw-38)*3/5,28);
        put("NEW FEEL",gx+gw-92,y+186,78,20);put("HUMANIZE STYLE",gx+gw-228,y+186,130,20);
        int k=0;const int feelCount=track==0?5:4;for(const auto* n:{"SWING","GATE","VELOCITY","HUMANIZE","DENSITY"}){if(track!=0&&juce::String(n)=="GATE")continue;const int kw=(gw-24)/feelCount;put(n,gx+12+k++*kw,y+218,kw,112);}
        const int sx=W-sw-4;tool("HEAR",sx,y+46,108);put("Sound",sx+116,y+46,108);tool("LOAD SAMPLE",sx,y+82,108);tool("BUILT-IN",sx+116,y+82,108);int kx=0;
        for(const auto* n:{"TONE","REVERB","VOLUME"})put(n,sx+(kx++)*74,y+112,70,72);
        put("Sidechain Source",sx,y+210,126,24);put("Sidechain Rate",sx+134,y+210,90,24);
        put("CHORUS",sx,y+242,70,88);put("PUMP",sx+74,y+242,70,88);put("PUMP RELEASE",sx+148,y+242,76,88);
    }else{
        // Drums/Guitar: the original wide groove/ABCD row and four compact cards.
        card("GROOVE",18,y,W-36,118);
        put("GROOVE",32,y+35,204,30);tool("<",238,y+35,30);tool(">",268,y+35,30);tool("NEW RIFF",188,y+84,110);put("GROOVE LENGTH",308,y+36,104);put("DENSITY",308,y+84,104,30);
        for(int i=0;i<4;++i){const int x=438+i*(W-466)/4;put("SEGMENT "+juce::String("ABCD").substring(i,i+1),x,y+84,(W-490)/4,26);}
        const int my=y+128,part=290,chords=260,sound=262,grooveX=18+part+10+chords+10,grooveW=W-grooveX-sound-28;
        card(guitar?"GUITAR PART":"KIT & SOUNDS",18,my,part,186);
        card(guitar?"CHORDS":"DRUM PADS",318,my,chords,186);
        card("GROOVE",grooveX,my,grooveW,186);card("GUIDE SOUND",W-sound-18,my,sound,186);
        if(guitar){tool("SOLISTA",212,my+7,88);put("Guitar part",32,my+38,164);put("Part mute",238,my+38,56,28);put("Part octave",32,my+88,70,26);put("Part level",182,my+84,56,58);put("Part volume",240,my+84,58,58);put("Chord summary",326,my+28,244,150);put("STRUM",W-sound+132,my+84,116,24);put("Strum after fill",grooveX+grooveW-136,my+6,122,20);tool("PLAYING...",W-112,my+6,84);}
        else{put("Drum",32,my+38,164);tool("RND LANE",200,my+38,34);put("Mute",238,my+38,56,28);put("MIDI NOTE",32,my+88,146,26);put("LEVEL",182,my+84,56,58);put("Drum volume",240,my+84,58,58);tool("LOAD SAMPLE",32,my+152,166);tool("BUILT-IN",202,my+152,92);put("Drum pads",324,my+24,248,156);}

        tool("NEW FEEL",grooveX+grooveW-160,my+4,84);put(guitar?"Strum after fill":"Crash after fill",grooveX+grooveW-72,my+4,62,24);
        const int half=(grooveW-38)/2;
        put("Section",grooveX+14,my+42,half,24);put("FILL EVERY",grooveX+24+half,my+42,half,24);
        put("VARIATION EVERY",grooveX+14,my+84,half,24);put("FILL STYLE",grooveX+24+half,my+84,half,24);
        int k=0;for(const auto* n:{"SWING","HUMANIZE","VELOCITY","ACCENTS"}){const int kw=(grooveW-20)/4;put(n,grooveX+8+k++*kw,my+110,kw,72);}
        const int sx=W-sound-4;tool("HEAR",sx,my+42,116);put("Sound",sx+120,my+42,116);
        if(guitar){tool("LOAD SAMPLE",32,my+152,166);tool("BUILT-IN",202,my+152,92);}
        else{put("Genre feel",sx,my+80,116,24);put("Bass space",sx+120,my+80,116,24);}
        int kx=0;for(const auto* n:{"TONE","REVERB","VOLUME"})put(n,sx+(kx++)*80,my+110,76,72);
    }
    if(!piano)for(auto& c:controls)if(c.component->getProperties()["fgaControl"].toString()=="DENSITY"||c.component->getProperties()["fgaControl"].toString()=="STRUM")if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get())){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,48,22);}
    for(auto& c:controls){const auto name=c.component->getProperties()["fgaControl"].toString();if(name=="REGISTER")if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get())){slider->textFromValueFunction=[](double v){return juce::MidiMessage::getMidiNoteName((int)v,true,true,4);};slider->updateText();}if(owner.selectedTrack==3&&name.endsWith(" MUTE"))if(auto* button=dynamic_cast<juce::Button*>(c.component.get()))button->setButtonText(tr("Mute"));}
    int bottom=y+(piano?350:324);
    if(piano){
        card("RHYTHM / VELOCITY",18,bottom,W-36,280);
        int tx=32;for(const auto* n:{"RND PIANO","RND BASS","RND VELOCITY","MORPH","DNA","A/B","IMPORT MIDI","PRESETS"}){bool exists=false;for(const auto& c:controls)exists=exists||c.component->getProperties()["fgaControl"].toString()==n;if(exists&&!(juce::String(n)=="A/B"&&track==0)){tool(n,tx,bottom+31,112);tx+=118;}}
        for(auto& c:controls)if(dynamic_cast<PianoStepGrid*>(c.component.get()))c.component->setBounds(32,bottom+76,W-64,180);
        // Segment order and locks stay beside the rhythm as in the original grid.
        bottom+=290;
        card("PERFORMANCE",18,bottom,W-36,124);
        for(int i=0;i<4;++i){const int x=32+i*164;put("SEGMENT "+juce::String("ABCD").substring(i,i+1),x,bottom+46,72,24);put("Velocity order "+juce::String("ABCD").substring(i,i+1),x+80,bottom+46,72,24);put("Lock "+juce::String("ABCD").substring(i,i+1),x,bottom+80,150,24);}
        put("Riff seed",704,bottom+45,110,50);put("Humanize seed",824,bottom+45,110,50);put("EVOLVE",944,bottom+45,100,50);put("Realism",1052,bottom+42,W-1090,24);put("Pedal & Expression CC",1052,bottom+78,W-1090,24);
        bottom+=134;
    }
    if(track>=4){
        card("DRUM / POCKET",18,bottom,W-36,128);
        const auto name=juce::String(fgd::laneName(track-4));put(name+" SWING",32,bottom+40,110,80);put(name+" PUSH / PULL",154,bottom+40,110,80);
        bottom+=138;
    }
    if(guitar){
        card("GUITAR / LANE MIXER",18,bottom,W-36,490);
        const int lw=(W-64)/12;for(int lane=0;lane<12;++lane){const auto name=juce::String(fgg::laneShortName(lane));const int x=32+lane*lw;
            put(name+" PUSH / PULL",x,bottom+72,lw-8,64);put(name+" SWING",x,bottom+160,lw-8,64);
            put(name+" VOLUME",x,bottom+250,lw-8,142);put(name+" OCTAVE",x,bottom+416,lw-8,24);put(name+" MUTE",x,bottom+454,lw-8,24);
            const auto ink=lane<=1?Palette::accent:lane<=4?Palette::accent2:lane<=6?Palette::percussion:lane<=8?Palette::cymbals:Palette::toms;
            for(auto& c:controls){const auto parameter=c.component->getProperties()["fgaControl"].toString();if(parameter.startsWith(name+" "))if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get())){slider->setColour(juce::Slider::rotarySliderFillColourId,ink);if(parameter==name+" VOLUME"){slider->setSliderStyle(juce::Slider::LinearVertical);slider->setColour(juce::Slider::trackColourId,ink);slider->setColour(juce::Slider::thumbColourId,Palette::text);slider->setColour(juce::Slider::backgroundColourId,Palette::field);}}}
        }bottom+=500;
    }
    // Every existing parameter has an explicit family section. Missing placement
    // is reported by layoutProblems(), rather than silently creating a new UI grid.
    headingY.resize(headings.size());for(size_t i=0;i<headings.size();++i){headingY[i]=0;for(const auto& c:cards)if(c.title==headings[i].first||(c.title=="GROOVE"&&(headings[i].first.contains("GROOVE")||headings[i].first=="PHRASES / FILLS"))||(c.title=="RHYTHM / VELOCITY"&&headings[i].first=="Musical tools"))headingY[i]=c.bounds.getY();}
    if(getHeight()!=bottom+10)setSize(W,bottom+10);
}
void InstrumentPanel::paint(juce::Graphics& g){
    g.fillAll(Palette::background);
    for(const auto& card:cards){
        const bool piano=owner.selectedTrack<3;
        const auto area=card.bounds;
        const auto r=area.toFloat();
        const float radius=piano?6.f:8.f;
        g.setColour(Palette::panel);g.fillRoundedRectangle(r,radius);
        g.setColour(Palette::panelEdge);g.drawRoundedRectangle(r.reduced(.5f),radius,1);
        if(piano){
            // Piano::drawPanel has its own smaller, letter-spaced title.
            auto head=area.reduced(14,8).removeFromTop(16).toFloat();
            g.setColour(Palette::accent);g.fillRoundedRectangle(head.getX(),head.getCentreY()-5,2.5f,10,1);
            auto font=FamilyTheme::font(11,true);font.setExtraKerningFactor(.12f);
            g.setFont(font);g.setColour(Palette::text.withAlpha(.85f));
            g.drawText(tr(card.title),head.withTrimmedLeft(9),juce::Justification::centredLeft);
            if(card.title=="HARMONY & SCALES"){
                const auto ink=owner.selectedTrack==0?Palette::accent:owner.selectedTrack==1?Palette::accent2:Palette::strings;
                const auto frame=juce::Rectangle<float>((float)area.getX()+14,(float)area.getY()+106,272,220);
                g.setColour(ink.withAlpha(.06f));g.fillRoundedRectangle(frame,6);
                g.setColour(ink.withAlpha(.7f));g.drawRoundedRectangle(frame.reduced(.5f),6,1.2f);
            }
        }else{
            // Drums/Guitar::drawPanel: the original title and rounded tick.
            g.setColour(Palette::accent);g.fillRoundedRectangle((float)area.getX()+12,(float)area.getY()+11,3,12,1);
            g.setColour(Palette::text.withAlpha(.92f));g.setFont(FamilyTheme::font(11.5f,true));
            g.drawText(tr(card.title).toUpperCase(),area.withHeight(34).withTrimmedLeft(21).withTrimmedRight(12),juce::Justification::centredLeft);
        }
    }
    if(owner.selectedTrack>=3)for(const auto& card:cards)if(card.title=="GROOVE"&&card.bounds.getHeight()==118){
        const auto& e=owner.project.tracks[(size_t)owner.selectedTrack].engine;
        for(int part=0;part<4;++part){const int x=432+part*(getWidth()-466)/4;auto r=juce::Rectangle<float>((float)x,(float)card.bounds.getY()+18,(float)(getWidth()-466)/4-8,96);
            g.setColour(Palette::field);g.fillRoundedRectangle(r,6);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(r.reduced(.5f),6,1);g.setColour(Palette::panelEdge.brighter(.2f));g.fillRoundedRectangle(r.reduced(10,0).withHeight(3).translated(0,2),1.5f);
            g.setColour(Palette::dim);g.setFont(FamilyTheme::font(11,true));g.drawText(juce::String(part+1)+"  "+tr("Bar"),r.reduced(10,6).withHeight(18),juce::Justification::centred);
            g.setColour(Palette::text);g.setFont(FamilyTheme::font(24,true));g.drawText(juce::String::charToString('A'+e.segments[(size_t)part]),r.withTrimmedTop(25).withTrimmedLeft(8).withWidth(30).withHeight(36),juce::Justification::centred);
            const double duration=e.grooveBars;for(const auto& n:owner.clip().notes)if(n.start>=part*duration&&n.start<(part+1)*duration){const float dx=r.getX()+44+(float)((n.start-part*duration)/duration*(r.getWidth()-56));g.setColour(Palette::accent.withAlpha(.4f+.6f*n.velocity/127.f));g.fillRoundedRectangle(dx,r.getY()+29+(n.lane%4)*6.f,3,4,1);}
        }
    }
    if(owner.selectedTrack<3)for(const auto& card:cards)if(card.title=="GROOVE"){g.setColour(Palette::dim);g.setFont(FamilyTheme::font(11,true));g.drawText(tr("FEEL"),card.bounds.getX()+14,card.bounds.getY()+186,140,20,juce::Justification::centredLeft);}
    if(owner.selectedTrack==3)for(const auto& card:cards)if(card.title=="GUITAR / LANE MIXER"){const int lw=(getWidth()-64)/12;for(int lane=0;lane<12;++lane){const int x=32+lane*lw;auto r=juce::Rectangle<float>((float)x-2,(float)card.bounds.getY()+32,(float)lw-4,450);g.setColour(Palette::field);g.fillRoundedRectangle(r,5);const auto ink=lane<=1?Palette::accent:lane<=4?Palette::accent2:lane<=6?Palette::percussion:lane<=8?Palette::cymbals:Palette::toms;g.setColour(ink);g.fillRoundedRectangle(r.withHeight(3).reduced(6,0),1.5f);g.setColour(Palette::text);g.setFont(FamilyTheme::font(10.5f,true));g.drawText(tr(fgg::laneShortName(lane)),r.withTrimmedTop(5).withHeight(16),juce::Justification::centred);}}
    for(const auto& c:controls){if(c.component->getWidth()==0||dynamic_cast<PianoStepGrid*>(c.component.get())||dynamic_cast<DrumPads*>(c.component.get()))continue;auto r=c.component->getBounds();if(c.caption=="HUMANIZE STYLE"||c.caption=="Guitar part"||(owner.selectedTrack>=3&&(c.caption=="GROOVE"||c.caption=="STRUM")))continue;if(c.caption.startsWith("Degree ")||c.caption.startsWith("Inversion ")||(owner.selectedTrack>=3&&c.caption.startsWith("SEGMENT ")))continue;g.setColour(Palette::dim);auto captionFont=FamilyTheme::font(owner.selectedTrack<3?10.f:11.f,true);if(owner.selectedTrack<3)captionFont.setExtraKerningFactor(.08f);g.setFont(captionFont);auto caption=c.caption;if(caption=="Drum volume")caption="Volume";if(caption.startsWithIgnoreCase("Part "))caption=caption.substring(5);if(owner.selectedTrack==3)for(int lane=0;lane<12;++lane){auto prefix=juce::String(fgg::laneShortName(lane))+" ";if(caption.startsWith(prefix))caption=caption.substring(prefix.length());}g.drawText(tr(caption).toUpperCase(),r.getX(),c.knob?(owner.selectedTrack<3?r.getY()-16:r.getBottom()):r.getY()-17,r.getWidth(),owner.selectedTrack<3?16:14,c.knob?juce::Justification::centred:juce::Justification::centredLeft);}
    if(owner.selectedTrack<3){auto names=chordNames(owner.project.settings);const int sw=(getWidth()-346)/8;for(size_t i=0;i<8;++i){auto card=juce::Rectangle<float>((float)(326+(int)i*sw),28,(float)(sw-4),104);g.setColour(Palette::field.withAlpha(i<names.size()?1.f:.45f));g.fillRoundedRectangle(card,6);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(card,6,1);if(i>=names.size())continue;g.setColour(Palette::text);g.setFont(FamilyTheme::font(14,true));g.drawText(juce::String::fromUTF8(names[i].c_str()),328+(int)i*sw,67,sw-8,22,juce::Justification::centred);}}
}
}
