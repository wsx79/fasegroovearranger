// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InstrumentPanel.h"
#include "MainComponent.h"
#include "UiText.h"
#include "PianoStepGrid.h"
#include "../ThirdParty/fasegrooveguitar/Source/engine/Generators.h"
#include "../ThirdParty/fasegroovedrums/Source/engine/Generators.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
namespace fga {
class ChordSummary:public juce::Component,public juce::SettableTooltipClient {
public:
    MainComponent& owner;std::function<void()> edit;
    ChordSummary(MainComponent& o,std::function<void()> fn):owner(o),edit(std::move(fn)){setTooltip(controlTip("PROGRESSION"));}
    void mouseDown(const juce::MouseEvent&)override{edit();}
    void paint(juce::Graphics& g)override{auto names=chordNames(owner.project.settings);const int rows=((int)names.size()+3)/4;const float w=getWidth()/4.f,h=getHeight()/(float)std::max(1,rows);for(size_t i=0;i<names.size();++i){auto r=juce::Rectangle<float>((i%4)*w,(i/4)*h,w,h).reduced(3,3);g.setColour(Palette::field);g.fillRoundedRectangle(r,4);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(r,4,1);g.setColour(Palette::dim);g.setFont(FamilyTheme::font(9,true));g.drawText(juce::String((int)i+1),r.withHeight(18).reduced(5,0),juce::Justification::centredLeft);g.setColour(Palette::accent);g.setFont(FamilyTheme::font(14,true));g.drawText(juce::String::fromUTF8(names[i].c_str()),r,juce::Justification::centred);}}
};
void drawFamilyLogo(juce::Graphics& g,juce::Rectangle<float> area){
    if(auto icon=juce::Drawable::createFromImageData(BinaryData::arranger_svg,BinaryData::arranger_svgSize))icon->drawWithin(g,area.removeFromLeft(area.getHeight()),juce::RectanglePlacement::centred,1);
    area.removeFromLeft(12);auto top=area.removeFromTop(area.getHeight()*.62f);
    auto slant=[&](const juce::String& text,juce::Rectangle<float> r,float kern){auto f=FamilyTheme::font(r.getHeight()*1.05f,true);f.setExtraKerningFactor(kern);juce::GlyphArrangement letters;letters.addFittedText(f,text,r.getX(),r.getY(),r.getWidth()-7,r.getHeight(),juce::Justification::centredLeft,1);juce::Path path;letters.createPath(path);path.applyTransform(juce::AffineTransform::shear(-.21f,0).translated(.21f*r.getCentreY(),0));g.fillPath(path);};
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffb35c),top.getTopLeft(),juce::Colour(0xffff7a3d),top.getBottomRight(),false));slant("FASE",top,.04f);g.setColour(Palette::text);slant("GROOVE ARRANGER",area,.14f);
}
InstrumentPanel::InstrumentPanel(MainComponent& m):owner(m){
    addChildComponent(circle);
    circle.setFreeMode(true);
    circle.onPick=[this](int root,bool minor){owner.checkpoint();owner.project.settings.key=root;owner.project.settings.scale=minor?2:0;owner.changed(false);updateCircle();};
    circle.onMoveChord=[this](int slot,int degree,int root,bool minor){auto& s=owner.project.settings;if(slot<0||slot>=s.chordCount)return;owner.checkpoint();if(degree>=0){s.degrees[(size_t)slot]=degree;s.freeChords[(size_t)slot]=-1;}else s.freeChords[(size_t)slot]=sgk::freeChordCode(root-s.key,minor);owner.changed(false);updateCircle();};
}
void InstrumentPanel::updateCircle(){
    auto s=pianoEngineSettings(owner.project,owner.selectedTrack,owner.clip().seed);
    std::vector<CircleOfFifths::ChordMark> chords,diatonic;
    std::array<sgk::Chord,8> cs;std::array<sgk::VoicedChord,8> voiced;
    for(int i=0;i<s.progressionLength;++i){cs[(size_t)i]=sgk::GrooveEngine::chordForSlot(s,i);voiced[(size_t)i]=sgk::voiceChord(cs[(size_t)i],s.voicing,s.centre,nullptr);}
    auto scores=sgk::GrooveEngine::harmonyScores(s,cs,voiced);std::vector<float> transitions;
    for(int i=0;i<s.progressionLength;++i){const auto& c=cs[(size_t)i];chords.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::GrooveEngine::numeralForSlot(s,i).c_str()),scores.slot[(size_t)i]});transitions.push_back(scores.transition[(size_t)i]);}
    for(int i=0;i<7;++i){auto c=sgk::buildChord(s.keyRoot,s.scale,i,s.extension);diatonic.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::romanNumeral(i,c).c_str()),1});}
    circle.setState(s.keyRoot,s.scale==2,tr(sgk::scaleName(s.scale)),chords,transitions,-1,diatonic);
}
void InstrumentPanel::heading(const juce::String& name){headings.push_back({name,(int)controls.size()});}
void InstrumentPanel::apply(std::function<void(EngineOptions&)> fn){owner.checkpoint();fn(owner.project.tracks[(size_t)owner.selectedTrack].engine);owner.project.normalise();owner.changed(false);owner.message("Settings apply on the next generation; the MIDI already written stays editable.");}
void InstrumentPanel::knob(const juce::String& name,double lo,double hi,double step,double value,std::function<void(EngineOptions&,double)> fn,const juce::String& suffix){
    auto c=std::make_unique<juce::Slider>();c->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);c->setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,16);c->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);c->setRange(lo,hi,step);c->setValue(value,juce::dontSendNotification);c->setTextValueSuffix(suffix);c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onDragStart=[this]{owner.checkpoint();};auto* raw=c.get();c->onValueChange=[this,raw,fn]{fn(owner.project.tracks[(size_t)owner.selectedTrack].engine,raw->getValue());owner.dirty=true;owner.audio.updateMix(owner.project);};addAndMakeVisible(*c);controls.push_back({std::move(c),name,true});
}
void InstrumentPanel::choice(const juce::String& name,const juce::StringArray& items,int selected,std::function<void(EngineOptions&,int)> fn){auto c=std::make_unique<juce::ComboBox>();c->addItemList(items,1);c->setSelectedItemIndex(selected,juce::dontSendNotification);auto* raw=c.get();c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onChange=[this,raw,fn]{apply([&](EngineOptions& e){fn(e,raw->getSelectedItemIndex());});};addAndMakeVisible(*c);controls.push_back({std::move(c),name,false});}
void InstrumentPanel::sharedChoice(const juce::String& name,const juce::StringArray& items,int selected,std::function<void(int)> fn){auto c=std::make_unique<juce::ComboBox>();c->addItemList(items,1);c->setSelectedItemIndex(selected,juce::dontSendNotification);c->getProperties().set("fgaControl",name);auto* raw=c.get();c->onChange=[this,raw,fn]{owner.checkpoint();fn(raw->getSelectedItemIndex());owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),name,false});}
void InstrumentPanel::action(const juce::String& name,std::function<void()> fn){auto c=std::make_unique<juce::TextButton>(tr(name));c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onClick=std::move(fn);addAndMakeVisible(*c);controls.push_back({std::move(c),{},false});}
void InstrumentPanel::toggle(const juce::String& name,bool value,std::function<void(EngineOptions&,bool)> fn){auto c=std::make_unique<juce::ToggleButton>(tr(name));c->setToggleState(value,juce::dontSendNotification);auto* raw=c.get();c->getProperties().set("fgaControl",name);c->setTooltip(controlTip(name));c->onClick=[this,raw,fn]{apply([&](EngineOptions& e){fn(e,raw->getToggleState());});};addAndMakeVisible(*c);controls.push_back({std::move(c),{},false});}
void InstrumentPanel::rebuild(){
    controls.clear();headings.clear();const int track=owner.selectedTrack;const auto e=owner.project.tracks[(size_t)track].engine;const bool piano=track==0,bass=track==1,strings=track==2,guitar=track==3,drum=track>=4;
    if(piano){heading("HARMONY & SCALES");juce::StringArray voicings,extensions;for(int i=0;i<sgk::numVoicings;++i)voicings.add(sgk::voicingName(i));for(int i=0;i<sgk::numExtensions;++i)extensions.add(sgk::extensionName(i));sharedChoice("Voicing",voicings,owner.project.settings.voicing,[this](int v){owner.project.settings.voicing=v;});sharedChoice("Extensions",extensions,owner.project.settings.extension,[this](int v){owner.project.settings.extension=v;});}
    heading(piano?"PIANO / GROOVE":bass?"BASS / GROOVE":strings?"STRINGS / GROOVE":guitar?"GUITAR / GROOVE":"DRUMS / "+juce::String::fromUTF8(owner.project.tracks[(size_t)track].name.c_str()).toUpperCase());
    juce::StringArray patterns{"Genre / automatic"};if(track<3)for(const auto& p:sgk::groovePatterns())patterns.add(juce::String::fromUTF8((p.category+" / "+p.name).c_str()));else if(guitar)for(const auto& p:fgg::guitarPatterns())patterns.add(juce::String::fromUTF8(p.name.c_str()));else for(const auto& p:fgd::drumPatterns())patterns.add(juce::String::fromUTF8(p.name.c_str()));if(track<3){patterns.add("* Random riff *");patterns.add("* User pattern *");}choice("GROOVE",patterns,e.pattern+1,[](auto& o,int v){o.pattern=v-1;});
    choice("GROOVE LENGTH",{"1 bar","2 bars","4 bars"},e.grooveBars==4?2:e.grooveBars-1,[](auto& o,int v){o.grooveBars=v==2?4:v+1;});
    if(bass){juce::StringArray modes;for(int i=0;i<sgk::numBassModes;++i)modes.add(sgk::bassModeName(i));auto c=std::make_unique<juce::ComboBox>();c->addItemList(modes,1);c->setSelectedItemIndex(owner.project.settings.bassMode,juce::dontSendNotification);auto* raw=c.get();c->onChange=[this,raw]{owner.checkpoint();owner.project.settings.bassMode=raw->getSelectedItemIndex();owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),"BASS LINE",false});choice("BASS FEEL",{"Classic","Sub","Sub + sidechain gaps"},e.bassFeel,[](auto& o,int v){o.bassFeel=v;});}
    if(strings){juce::StringArray modes;for(int i=0;i<sgk::numStringsModes;++i)modes.add(sgk::stringsModeName(i));auto c=std::make_unique<juce::ComboBox>();c->addItemList(modes,1);c->setSelectedItemIndex(owner.project.settings.stringsMode,juce::dontSendNotification);auto* raw=c.get();c->onChange=[this,raw]{owner.checkpoint();owner.project.settings.stringsMode=raw->getSelectedItemIndex();owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),"STRINGS STYLE",false});}
    knob("SWING",0,150,1,(e.swing<0?owner.project.settings.swing:e.swing)*100,[](auto& o,double v){o.swing=v/100;}," %");knob("HUMANIZE",0,100,1,(e.humanize<0?owner.project.settings.humanize:e.humanize)*100,[](auto& o,double v){o.humanize=v/100;}," %");knob("DENSITY",0,100,1,(e.density<0?owner.project.settings.density:e.density)*100,[](auto& o,double v){o.density=v/100;}," %");knob("VELOCITY",1,127,1,e.velocity,[](auto& o,double v){o.velocity=(int)v;});
    if(piano){knob("GATE",2,200,1,e.gate*100,[](auto& o,double v){o.gate=v/100;}," %");knob("REGISTER",24,108,1,e.centre,[](auto& o,double v){o.centre=(int)v;});knob("STRUM",0,100,1,e.strum*100,[](auto& o,double v){o.strum=v/100;}," %");choice("STRUM DIRECTION",{"Up","Down","Alternate"},e.strumDirection,[](auto& o,int v){o.strumDirection=v;});choice("MOVEMENT",{"Off","Subtle","Lively"},e.movement,[](auto& o,int v){o.movement=v;});auto range=std::make_unique<juce::Slider>(juce::Slider::TwoValueHorizontal,juce::Slider::NoTextBox);range->getProperties().set("fgaControl","Range");range->setRange(0,127,1);range->setMinAndMaxValues(e.rangeLow,e.rangeHigh,juce::dontSendNotification);range->setTooltip(controlTip("RANGE LOW"));range->onDragStart=[this]{owner.checkpoint();};auto* rangePtr=range.get();range->onValueChange=[this,rangePtr]{auto& o=owner.project.tracks[(size_t)owner.selectedTrack].engine;o.rangeLow=(int)rangePtr->getMinValue();o.rangeHigh=(int)rangePtr->getMaxValue();owner.dirty=true;};addAndMakeVisible(*range);controls.push_back({std::move(range),"Range",false});toggle("Top +8va",e.octaveTop,[](auto& o,bool v){o.octaveTop=v;});toggle("Realism",e.realism,[](auto& o,bool v){o.realism=v;});
        juce::StringArray top,approach,feel;for(int i=0;i<sgk::numTopLines;++i)top.add(sgk::topLineName(i));for(int i=0;i<sgk::numApproachStyles;++i)approach.add(sgk::approachStyleName(i));for(int i=0;i<sgk::numHumanizeStyles;++i)feel.add(sgk::humanizeStyleName(i));choice("TOP LINE",top,e.topLine,[](auto& o,int v){o.topLine=v;});choice("APPROACH",approach,e.approach,[](auto& o,int v){o.approach=v;});choice("HUMANIZE STYLE",feel,e.humanizeStyle,[](auto& o,int v){o.humanizeStyle=v;});knob("EVOLVE",0,32,4,e.evolveBars,[](auto& o,double v){o.evolveBars=(int)v;}," bars");}
    else if(guitar||drum){knob("ACCENTS",0,100,1,e.accent*100,[](auto& o,double v){o.accent=v/100;}," %");if(guitar){knob("STRUM",0,80,1,e.strum*80,[](auto& o,double v){o.strum=v/80;}," ms");toggle("Let ring",e.letRing,[](auto& o,bool v){o.letRing=v;});toggle("Room for band",e.roomForBand,[](auto& o,bool v){o.roomForBand=v;});toggle("Push changes",e.pushChanges,[](auto& o,bool v){o.pushChanges=v;});}else{knob("MIDI NOTE",0,127,1,e.midiNote<0?fgd::defaultNote(track-4):e.midiNote,[](auto& o,double v){o.midiNote=(int)v;});toggle("Genre feel",e.genreFeel,[](auto& o,bool v){o.genreFeel=v;});toggle("Bass space",e.bassSpace,[](auto& o,bool v){o.bassSpace=v;});}}
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
    knob("REVERB",0,100,1,e.reverb*100,[](auto& o,double v){o.reverb=v/100;}," %");knob("CHORUS",0,100,1,e.chorus*100,[](auto& o,double v){o.chorus=v/100;}," %");knob("PUMP",0,100,1,e.pump*100,[](auto& o,double v){o.pump=v/100;}," %");knob("PUMP RELEASE",5,100,1,e.pumpRelease*100,[](auto& o,double v){o.pumpRelease=v/100;}," % beat");
    choice("Sidechain Rate",{"1/4","1/8","1/2","1/16"},e.pumpRate,[](auto& o,int v){o.pumpRate=v;});choice("Sidechain Source",{"Tempo (internal)","Kick MIDI"},e.pumpSource,[](auto& o,int v){o.pumpSource=v;});
    if(guitar||drum){heading(guitar?"GUITAR / LANE MIXER":"DRUM / POCKET");const int lanes=guitar?12:1;for(int i=0;i<lanes;++i){const size_t l=(size_t)i;const juce::String name=guitar?fgg::laneShortName(i):fgd::laneName(track-4);if(guitar){knob(name+" LEVEL",0,200,1,e.laneLevel[l]*100,[l](auto& o,double v){o.laneLevel[l]=v/100;}," %");knob(name+" OCTAVE",-2,2,1,e.laneOctave[l],[l](auto& o,double v){o.laneOctave[l]=(int)v;});toggle(name+" MUTE",e.laneMute[l],[l](auto& o,bool v){o.laneMute[l]=v;});}knob(name+" SWING",0,150,1,e.laneSwing[l]*100,[l](auto& o,double v){o.laneSwing[l]=v/100;}," %");knob(name+" PUSH / PULL",-100,100,1,e.laneShift[l],[l](auto& o,double v){o.laneShift[l]=v;}," ms");}}
    if(guitar){
        juce::StringArray lanes;for(int i=0;i<12;++i)lanes.add(fgg::laneShortName(i));
        sharedChoice("Guitar part",lanes,guitarLane,[this](int v){guitarLane=v;juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
        knob("Part level",0,200,1,e.laneLevel[(size_t)guitarLane]*100,[lane=guitarLane](auto& o,double v){o.laneLevel[(size_t)lane]=v/100;}," %");
        choice("Part octave",{"-2","-1","0","+1","+2"},e.laneOctave[(size_t)guitarLane]+2,[lane=guitarLane](auto& o,int v){o.laneOctave[(size_t)lane]=v-2;});
        toggle("Part mute",e.laneMute[(size_t)guitarLane],[lane=guitarLane](auto& o,bool v){o.laneMute[(size_t)lane]=v;});
        auto chords=std::make_unique<ChordSummary>(owner,[this]{owner.showChordMenu();});chords->getProperties().set("fgaControl","Chord summary");addAndMakeVisible(*chords);controls.push_back({std::move(chords),{},false});
    }
    if(track>=3)action("NEW FEEL",[this]{apply([](auto& o){o.humanizeSeed=(std::uint32_t)juce::Random::getSystemRandom().nextInt({1,100000});});});
    action("NEW RIFF",[this]{apply([](auto& o){o.riffSeed=(std::uint32_t)juce::Random::getSystemRandom().nextInt({1,100000});});});
    if(track<4){
        juce::StringArray lengths;for(int i=1;i<=8;++i)lengths.add(juce::String(i));
        sharedChoice("Length",lengths,owner.project.settings.chordCount-1,[this](int v){owner.project.settings.chordCount=v+1;juce::Component::SafePointer<InstrumentPanel> self(this);juce::MessageManager::callAsync([self]{if(self)self->rebuild();});});
        sharedChoice("Each chord",{"1/2 bar","1 bar","2 bars","4 bars"},owner.project.settings.chordSteps==8?0:owner.project.settings.chordSteps==16?1:owner.project.settings.chordSteps==32?2:3,[this](int v){owner.project.settings.chordSteps=8<<v;});
        for(int i=0;i<8;++i)sharedChoice("Degree "+juce::String(i+1),{"I","ii","iii","IV","V","vi","vii"},owner.project.settings.degrees[(size_t)i],[this,i](int v){owner.project.settings.degrees[(size_t)i]=v;owner.project.settings.freeChords[(size_t)i]=-1;updateCircle();repaint();});
    }
    circle.setVisible(track<3);if(track<3)updateCircle();
    for(auto& c:controls){const auto name=c.component->getProperties()["fgaControl"].toString();if(name.startsWith("Degree "))c.component->setEnabled(name.fromLastOccurrenceOf(" ",false,false).getIntValue()<=owner.project.settings.chordCount);}
    translateControls(*this);resized();repaint();
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
    auto put=[&](const juce::String& name,int x,int y,int w,int h=28){for(auto& c:controls)if(c.component->getProperties()["fgaControl"].toString()==name){c.component->setBounds(x,y,w,h);return;}};
    auto tool=[&](const juce::String& name,int x,int y,int w){put(name,x,y,w,24);};
    const int progHeight=track<4?114:0;
    if(track<4){
        card("PROGRESSION",18,10,W-36,104);
        put("Length",32,79,94,24);put("Each chord",134,79,112,24);
        tool("VARIANTS",32,31,100);tool("LEARN",140,31,104);
        auto names=chordNames(owner.project.settings);const int slotW=(W-284)/8;
        for(int i=0;i<8;++i){const int x=266+i*slotW;put("Degree "+juce::String(i+1),x,79,slotW-8,24);
            if(piano&&track==0){put("Inversion "+juce::String(i+1),x,30,slotW-8,22);for(auto& c:controls)if(c.component->getProperties()["fgaControl"].toString()=="Inversion "+juce::String(i+1))if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get())){slider->setSliderStyle(juce::Slider::IncDecButtons);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,32,20);}}
        }
    }
    const int y=progHeight+10;
    if(piano){
        // Same four adjacent panels as the extended Piano, without shared KEY/BPM/LINK.
        const int hw=300,cw=230,sw=252,gx=18+hw+10+cw+10,gw=W-gx-sw-28;
        card("HARMONY & SCALES",18,y,hw,340);card("CIRCLE OF FIFTHS",328,y,cw,340);
        card("GROOVE",gx,y,gw,340);card("GUIDE SOUND",W-sw-18,y,sw,340);
        put("Extensions",32,y+48,125);put("APPROACH",167,y+48,137);
        put("Voicing",32,y+108,162);put("TOP LINE",202,y+108,102);
        put("BASS LINE",32,y+108,272);put("STRINGS STYLE",32,y+108,272);
        put("REGISTER",38,y+167,76,100);put("STRUM",120,y+167,76,100);
        put("MOVEMENT",202,y+165,102);put("Range",202,y+230,102,24);
        put("Top +8va",32,y+288,128,24);put("STRUM DIRECTION",168,y+286,136,24);
        put("BASS FEEL",32,y+168,272);
        circle.setBounds(338,y+28,cw-20,298);
        put("GROOVE",gx+14,y+48,gw-124);tool("NEW RIFF",gx+gw-102,y+48,88);put("GROOVE LENGTH",gx+14,y+104,(gw-38)/2);put("VARIATION EVERY",gx+24+(gw-38)/2,y+104,(gw-38)/2,28);
        put("FILL EVERY",gx+14,y+160,(gw-38)*2/5,28);put("FILL STYLE",gx+24+(gw-38)*2/5,y+160,(gw-38)*3/5,28);
        tool("NEW FEEL",gx+gw-100,y+214,86);put("HUMANIZE STYLE",gx+14,y+214,gw-124,24);
        int k=0;for(const auto* n:{"SWING","GATE","VELOCITY","HUMANIZE","DENSITY"}){const int kw=(gw-24)/5;put(n,gx+12+k++*kw,y+266,kw,58);}
        const int sx=W-sw-4;int kx=0;
        for(const auto* n:{"TONE","REVERB","CHORUS"})put(n,sx+(kx++)*74,y+59,70,76);
        put("Sidechain Source",sx,y+174,126);put("Sidechain Rate",sx+134,y+174,90);
        put("PUMP",sx,y+239,100,78);put("PUMP RELEASE",sx+114,y+239,110,78);
    }else{
        // Drums/Guitar: the original wide groove/ABCD row and four compact cards.
        card("GROOVE",18,y,W-36,118);
        put("GROOVE",32,y+37,272);tool("NEW RIFF",32,y+79,116);tool("NEW FEEL",156,y+79,148);put("GROOVE LENGTH",316,y+37,100);put("DENSITY",316,y+79,100,26);
        for(int i=0;i<4;++i){const int x=438+i*(W-466)/4;put("SEGMENT "+juce::String("ABCD").substring(i,i+1),x,y+48,(W-490)/4,28);}
        const int my=y+128,part=290,chords=260,sound=262,grooveX=18+part+10+chords+10,grooveW=W-grooveX-sound-28;
        card(guitar?"GUITAR PART":"KIT & SOUNDS",18,my,part,186);
        card(guitar?"CHORDS":"DRUM / POCKET",318,my,chords,186);
        card("GROOVE",grooveX,my,grooveW,186);card("GUIDE SOUND",W-sound-18,my,sound,186);
        if(guitar){put("Guitar part",32,my+43,164);put("Part mute",202,my+43,92,24);put("Part octave",32,my+108,70);put("Part level",202,my+81,92,55);put("Let ring",32,my+154,100,24);put("Room for band",136,my+154,158,24);put("Chord summary",326,my+28,244,150);put("STRUM",W-sound+4,my+153,100,24);put("Strum after fill",grooveX+grooveW-136,my+6,122,20);}
        else{put("MIDI NOTE",32,my+45,100,74);put("Genre feel",148,my+42,146,24);put("Bass space",148,my+76,146,24);put("Crash after fill",32,my+143,180,24);const auto name=juce::String(fgd::laneName(track-4));put(name+" SWING",332,my+58,110,80);put(name+" PUSH / PULL",450,my+58,110,80);}
        put("FILL EVERY",grooveX+14,my+42,(grooveW-38)/2);put("FILL STYLE",grooveX+24+(grooveW-38)/2,my+42,(grooveW-38)/2);
        put("VARIATION EVERY",grooveX+14,my+84,grooveW-28,24);
        int k=0;for(const auto* n:{"SWING","HUMANIZE","VELOCITY","ACCENTS"}){const int kw=(grooveW-24)/4;put(n,grooveX+12+k++*kw,my+127,kw,52);}
        const int sx=W-sound-4;int kx=0;for(const auto* n:{"TONE","REVERB","CHORUS"})put(n,sx+(kx++)*78,my+57,74,70);
    }
    int bottom=y+(piano?350:324);
    if(piano){
        card("RHYTHM / VELOCITY",18,bottom,W-36,280);
        int tx=32;for(const auto* n:{"RND PIANO","RND BASS","RND VELOCITY","MORPH","DNA","A/B","IMPORT MIDI","PRESETS"}){tool(n,tx,bottom+31,112);tx+=118;}
        for(auto& c:controls)if(dynamic_cast<PianoStepGrid*>(c.component.get()))c.component->setBounds(32,bottom+76,W-64,180);
        // Segment order and locks stay beside the rhythm as in the original grid.
        bottom+=290;
        card("PERFORMANCE",18,bottom,W-36,124);
        for(int i=0;i<4;++i){const int x=32+i*164;put("SEGMENT "+juce::String("ABCD").substring(i,i+1),x,bottom+46,72,24);put("Velocity order "+juce::String("ABCD").substring(i,i+1),x+80,bottom+46,72,24);put("Lock "+juce::String("ABCD").substring(i,i+1),x,bottom+80,150,24);}
        put("Riff seed",704,bottom+45,110,50);put("Humanize seed",824,bottom+45,110,50);put("EVOLVE",944,bottom+45,100,50);put("Realism",1052,bottom+42,150,24);put("Pedal & Expression CC",1052,bottom+78,W-1090,24);
        bottom+=134;
    }
    if(guitar){
        card("GUITAR / LANE MIXER",18,bottom,W-36,310);
        const int lw=(W-64)/12;for(int lane=0;lane<12;++lane){const auto name=juce::String(fgg::laneShortName(lane));const int x=32+lane*lw;
            put(name+" LEVEL",x,bottom+52,lw-8,104);put(name+" OCTAVE",x,bottom+185,lw-8,28);put(name+" MUTE",x,bottom+225,lw-8,24);
            for(auto& c:controls)if(c.component->getProperties()["fgaControl"].toString()==name+" LEVEL")if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get()))slider->setSliderStyle(juce::Slider::LinearVertical);
        }bottom+=320;
    }
    // Less frequently used lane timing and sound parameters remain reachable in
    // their own strips; none of the extended Piano parameters are discarded.
    std::vector<Control*> remaining;for(auto& c:controls)if(c.component->getWidth()==0)remaining.push_back(&c);
    if(!remaining.empty()){
        const int cols=8,rows=((int)remaining.size()+cols-1)/cols;card("TIMING / PERFORMANCE",18,bottom,W-36,36+rows*94);
        int index=0;for(auto* c:remaining){const int cw=(W-64)/cols;c->component->setBounds(32+(index%cols)*cw,bottom+52+(index/cols)*94,cw-12,c->knob?60:26);++index;}
        bottom+=46+rows*94;
    }
    headingY.resize(headings.size());for(size_t i=0;i<headings.size();++i){headingY[i]=0;for(const auto& c:cards)if(c.title==headings[i].first||(c.title=="GROOVE"&&(headings[i].first.contains("GROOVE")||headings[i].first=="PHRASES / FILLS"))||(c.title=="RHYTHM / VELOCITY"&&headings[i].first=="Musical tools"))headingY[i]=c.bounds.getY();}
    if(getHeight()!=bottom+10)setSize(W,bottom+10);
}
void InstrumentPanel::paint(juce::Graphics& g){
    g.fillAll(Palette::background);
    for(const auto& card:cards){g.setColour(Palette::panel);g.fillRoundedRectangle(card.bounds.toFloat(),6);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(card.bounds.toFloat(),6,1);g.setColour(Palette::accent);g.fillRect(card.bounds.getX()+13,card.bounds.getY()+11,2,10);auto font=FamilyTheme::font(10,true);font.setExtraKerningFactor(.08f);g.setFont(font);g.setColour(Palette::text);g.drawText(tr(card.title),card.bounds.getX()+23,card.bounds.getY()+4,card.bounds.getWidth()-32,24,juce::Justification::centredLeft);}
    for(const auto& c:controls){if(c.component->getWidth()==0||dynamic_cast<PianoStepGrid*>(c.component.get()))continue;auto r=c.component->getBounds();if(c.caption.startsWith("Degree ")||c.caption.startsWith("Inversion "))continue;g.setColour(Palette::dim);g.setFont(FamilyTheme::font(9,true));g.drawText(tr(c.caption).toUpperCase(),r.getX(),r.getY()-17,r.getWidth(),15,c.knob?juce::Justification::centred:juce::Justification::centredLeft);}
    if(owner.selectedTrack<4){auto names=chordNames(owner.project.settings);const int sw=(getWidth()-284)/8;for(size_t i=0;i<names.size();++i){g.setColour(Palette::text);g.setFont(FamilyTheme::font(14,true));g.drawText(juce::String::fromUTF8(names[i].c_str()),266+(int)i*sw,51,sw-8,20,juce::Justification::centred);}}
}
}
