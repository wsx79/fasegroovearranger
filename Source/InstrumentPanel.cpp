// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InstrumentPanel.h"
#include "MainComponent.h"
#include "../ThirdParty/fasegrooveguitar/Source/engine/Generators.h"
#include "../ThirdParty/fasegroovedrums/Source/engine/Generators.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
namespace fga {
void drawFamilyLogo(juce::Graphics& g,juce::Rectangle<float> area){
    if(auto icon=juce::Drawable::createFromImageData(BinaryData::arranger_svg,BinaryData::arranger_svgSize))icon->drawWithin(g,area.removeFromLeft(area.getHeight()),juce::RectanglePlacement::centred,1);
    area.removeFromLeft(12);auto top=area.removeFromTop(area.getHeight()*.62f);
    auto slant=[&](const juce::String& text,juce::Rectangle<float> r,float kern){auto f=FamilyTheme::font(r.getHeight()*1.05f,true);f.setExtraKerningFactor(kern);juce::GlyphArrangement letters;letters.addFittedText(f,text,r.getX(),r.getY(),r.getWidth()-7,r.getHeight(),juce::Justification::centredLeft,1);juce::Path path;letters.createPath(path);path.applyTransform(juce::AffineTransform::shear(-.21f,0).translated(.21f*r.getCentreY(),0));g.fillPath(path);};
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffb35c),top.getTopLeft(),juce::Colour(0xffff7a3d),top.getBottomRight(),false));slant("FASE",top,.04f);g.setColour(Palette::text);slant("GROOVE ARRANGER",area,.14f);
}
InstrumentPanel::InstrumentPanel(MainComponent& m):owner(m){}
void InstrumentPanel::heading(const juce::String& name){headings.push_back({name,(int)controls.size()});}
void InstrumentPanel::apply(std::function<void(EngineOptions&)> fn){owner.checkpoint();fn(owner.project.tracks[(size_t)owner.selectedTrack].engine);owner.project.normalise();owner.changed(false);owner.message("Parametri pronti. APPLY TRACK o APPLY CLIP aggiorna il MIDI rispettando i blocchi.");}
void InstrumentPanel::knob(const juce::String& name,double lo,double hi,double step,double value,std::function<void(EngineOptions&,double)> fn,const juce::String& suffix){
    auto c=std::make_unique<juce::Slider>();c->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);c->setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,22);c->setRange(lo,hi,step);c->setValue(value,juce::dontSendNotification);c->setTextValueSuffix(suffix);c->onDragStart=[this]{owner.checkpoint();};auto* raw=c.get();c->onValueChange=[this,raw,fn]{fn(owner.project.tracks[(size_t)owner.selectedTrack].engine,raw->getValue());owner.dirty=true;owner.audio.updateMix(owner.project);};addAndMakeVisible(*c);controls.push_back({std::move(c),name,true});
}
void InstrumentPanel::choice(const juce::String& name,const juce::StringArray& items,int selected,std::function<void(EngineOptions&,int)> fn){auto c=std::make_unique<juce::ComboBox>();c->addItemList(items,1);c->setSelectedItemIndex(selected,juce::dontSendNotification);auto* raw=c.get();c->onChange=[this,raw,fn]{apply([&](EngineOptions& e){fn(e,raw->getSelectedItemIndex());});};addAndMakeVisible(*c);controls.push_back({std::move(c),name,false});}
void InstrumentPanel::toggle(const juce::String& name,bool value,std::function<void(EngineOptions&,bool)> fn){auto c=std::make_unique<juce::ToggleButton>(name);c->setToggleState(value,juce::dontSendNotification);auto* raw=c.get();c->onClick=[this,raw,fn]{apply([&](EngineOptions& e){fn(e,raw->getToggleState());});};addAndMakeVisible(*c);controls.push_back({std::move(c),{},false});}
void InstrumentPanel::rebuild(){
    controls.clear();headings.clear();const int track=owner.selectedTrack;const auto e=owner.project.tracks[(size_t)track].engine;const bool piano=track==0,bass=track==1,strings=track==2,guitar=track==3,drum=track>=4;
    heading(piano?"PIANO / GROOVE":bass?"BASS / GROOVE":strings?"STRINGS / GROOVE":guitar?"GUITAR / GROOVE":"DRUMS / "+juce::String::fromUTF8(owner.project.tracks[(size_t)track].name.c_str()).toUpperCase());
    juce::StringArray patterns{"Genre / automatic"};if(track<3)for(const auto& p:sgk::groovePatterns())patterns.add(juce::String::fromUTF8((p.category+" / "+p.name).c_str()));else if(guitar)for(const auto& p:fgg::guitarPatterns())patterns.add(juce::String::fromUTF8(p.name.c_str()));else for(const auto& p:fgd::drumPatterns())patterns.add(juce::String::fromUTF8(p.name.c_str()));choice("GROOVE",patterns,e.pattern+1,[](auto& o,int v){o.pattern=v-1;});
    choice("GROOVE LENGTH",{"1 bar","2 bars","4 bars"},e.grooveBars==4?2:e.grooveBars-1,[](auto& o,int v){o.grooveBars=v==2?4:v+1;});
    if(bass){juce::StringArray modes;for(int i=0;i<sgk::numBassModes;++i)modes.add(sgk::bassModeName(i));auto c=std::make_unique<juce::ComboBox>();c->addItemList(modes,1);c->setSelectedItemIndex(owner.project.settings.bassMode,juce::dontSendNotification);auto* raw=c.get();c->onChange=[this,raw]{owner.checkpoint();owner.project.settings.bassMode=raw->getSelectedItemIndex();owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),"BASS LINE",false});choice("BASS FEEL",{"Classic","Sub","Sub / sidechain gaps"},e.bassFeel,[](auto& o,int v){o.bassFeel=v;});}
    if(strings){juce::StringArray modes;for(int i=0;i<sgk::numStringsModes;++i)modes.add(sgk::stringsModeName(i));auto c=std::make_unique<juce::ComboBox>();c->addItemList(modes,1);c->setSelectedItemIndex(owner.project.settings.stringsMode,juce::dontSendNotification);auto* raw=c.get();c->onChange=[this,raw]{owner.checkpoint();owner.project.settings.stringsMode=raw->getSelectedItemIndex();owner.changed(false);};addAndMakeVisible(*c);controls.push_back({std::move(c),"STRINGS STYLE",false});}
    knob("SWING",0,150,1,(e.swing<0?owner.project.settings.swing:e.swing)*100,[](auto& o,double v){o.swing=v/100;}," %");knob("HUMANIZE",0,100,1,(e.humanize<0?owner.project.settings.humanize:e.humanize)*100,[](auto& o,double v){o.humanize=v/100;}," %");knob("DENSITY",0,100,1,(e.density<0?owner.project.settings.density:e.density)*100,[](auto& o,double v){o.density=v/100;}," %");knob("VELOCITY",1,127,1,e.velocity,[](auto& o,double v){o.velocity=(int)v;});
    if(piano){knob("GATE",2,200,1,e.gate*100,[](auto& o,double v){o.gate=v/100;}," %");knob("REGISTER",24,108,1,e.centre,[](auto& o,double v){o.centre=(int)v;});knob("STRUM",0,100,1,e.strum*100,[](auto& o,double v){o.strum=v/100;}," %");choice("STRUM DIRECTION",{"Up","Down","Alternate"},e.strumDirection,[](auto& o,int v){o.strumDirection=v;});choice("MOVEMENT",{"Off","Subtle","Lively"},e.movement,[](auto& o,int v){o.movement=v;});knob("RANGE LOW",0,126,1,e.rangeLow,[](auto& o,double v){o.rangeLow=(int)v;});knob("RANGE HIGH",1,127,1,e.rangeHigh,[](auto& o,double v){o.rangeHigh=(int)v;});toggle("Top +8va",e.octaveTop,[](auto& o,bool v){o.octaveTop=v;});toggle("Realism",e.realism,[](auto& o,bool v){o.realism=v;});
        juce::StringArray top,approach,feel;for(int i=0;i<sgk::numTopLines;++i)top.add(sgk::topLineName(i));for(int i=0;i<sgk::numApproachStyles;++i)approach.add(sgk::approachStyleName(i));for(int i=0;i<sgk::numHumanizeStyles;++i)feel.add(sgk::humanizeStyleName(i));choice("TOP LINE",top,e.topLine,[](auto& o,int v){o.topLine=v;});choice("APPROACH",approach,e.approach,[](auto& o,int v){o.approach=v;});choice("HUMANIZE STYLE",feel,e.humanizeStyle,[](auto& o,int v){o.humanizeStyle=v;});knob("EVOLVE",0,32,4,e.evolveBars,[](auto& o,double v){o.evolveBars=(int)v;}," bars");}
    else if(guitar||drum){knob("ACCENTS",0,100,1,e.accent*100,[](auto& o,double v){o.accent=v/100;}," %");if(guitar){knob("STRUM",0,80,1,e.strum*80,[](auto& o,double v){o.strum=v/80;}," ms");toggle("Let ring",e.letRing,[](auto& o,bool v){o.letRing=v;});toggle("Room for band",e.roomForBand,[](auto& o,bool v){o.roomForBand=v;});toggle("Push changes",e.pushChanges,[](auto& o,bool v){o.pushChanges=v;});}else{knob("MIDI NOTE",0,127,1,e.midiNote<0?fgd::defaultNote(track-4):e.midiNote,[](auto& o,double v){o.midiNote=(int)v;});toggle("Genre feel",e.genreFeel,[](auto& o,bool v){o.genreFeel=v;});toggle("Bass space",e.bassSpace,[](auto& o,bool v){o.bassSpace=v;});}}
    heading("PHRASES / FILLS");juce::StringArray fills{"Mix / automatic"};const int count=track<3?sgk::numFillStyles:guitar?fgg::numFillStyles:fgd::numFillStyles;for(int i=0;i<count;++i)fills.add(track<3?sgk::fillStyleName(i):guitar?fgg::fillStyleName(i):fgd::fillStyleName(i));choice("FILL STYLE",fills,e.fillStyle+1,[](auto& o,int v){o.fillStyle=v-1;});knob("FILL EVERY",0,32,1,e.fillEvery,[](auto& o,double v){o.fillEvery=(int)v;}," bars");knob("VARIATION EVERY",0,32,1,e.variationEvery,[](auto& o,double v){o.variationEvery=(int)v;}," bars");if(drum||guitar)toggle(guitar?"Strum after fill":"Crash after fill",e.afterFill,[](auto& o,bool v){o.afterFill=v;});for(size_t i=0;i<4;++i)choice("SEGMENT "+juce::String("ABCD").substring((int)i,(int)i+1),{"A","B","C","D"},e.segments[i],[i](auto& o,int v){o.segments[i]=v;});
    heading("GUIDE SOUND");knob("TONE",0,100,1,e.tone*100,[](auto& o,double v){o.tone=v/100;}," %");
    knob("REVERB",0,100,1,e.reverb*100,[](auto& o,double v){o.reverb=v/100;}," %");knob("CHORUS",0,100,1,e.chorus*100,[](auto& o,double v){o.chorus=v/100;}," %");knob("PUMP",0,100,1,e.pump*100,[](auto& o,double v){o.pump=v/100;}," %");knob("PUMP RELEASE",5,100,1,e.pumpRelease*100,[](auto& o,double v){o.pumpRelease=v/100;}," % beat");
    if(guitar||drum){heading(guitar?"GUITAR / LANE MIXER":"DRUM / POCKET");const int lanes=guitar?12:1;for(int i=0;i<lanes;++i){const size_t l=(size_t)i;const juce::String name=guitar?fgg::laneShortName(i):fgd::laneName(track-4);if(guitar){knob(name+" LEVEL",0,200,1,e.laneLevel[l]*100,[l](auto& o,double v){o.laneLevel[l]=v/100;}," %");knob(name+" OCTAVE",-2,2,1,e.laneOctave[l],[l](auto& o,double v){o.laneOctave[l]=(int)v;});toggle(name+" MUTE",e.laneMute[l],[l](auto& o,bool v){o.laneMute[l]=v;});}knob(name+" SWING",0,150,1,e.laneSwing[l]*100,[l](auto& o,double v){o.laneSwing[l]=v/100;}," %");knob(name+" PUSH / PULL",-100,100,1,e.laneShift[l],[l](auto& o,double v){o.laneShift[l]=v;}," ms");}}
    resized();repaint();
}
void InstrumentPanel::resized(){
    const int cols=5,gap=14,w=(getWidth()-60-gap*(cols-1))/cols;
    headingY.resize(headings.size());int y=18,col=0;
    for(size_t i=0;i<controls.size();++i){
        bool lanes=false;
        for(size_t h=0;h<headings.size();++h)if(headings[h].second==(int)i){
            if(col){y+=146;col=0;}headingY[h]=y;y+=42;
            lanes=headings[h].first=="GUITAR / LANE MIXER";
        }
        if(lanes){
            const int laneWidth=(getWidth()-60-11*8)/12;
            const int offsets[]{24,174,282,346,466},heights[]{124,82,32,94,94};
            for(int lane=0;lane<12;++lane)for(int parameter=0;parameter<5;++parameter){
                auto& c=controls[i+(size_t)(lane*5+parameter)];
                if(parameter==0)if(auto* slider=dynamic_cast<juce::Slider*>(c.component.get()))slider->setSliderStyle(juce::Slider::LinearVertical);
                c.component->setBounds(30+lane*(laneWidth+8),y+offsets[parameter],laneWidth,heights[parameter]);
            }
            y+=580;col=0;break;
        }
        auto& c=controls[i];c.component->setBounds(30+col*(w+gap),y+24,w,c.knob?102:32);
        if(++col==cols){col=0;y+=146;}
    }
    const int height=y+(col?146:0)+20;if(getHeight()!=height)setSize(getWidth(),height);
}
void InstrumentPanel::paint(juce::Graphics& g){
    g.fillAll(Palette::background);
    for(size_t h=0;h<headings.size()&&h<headingY.size();++h){
        const int y=headingY[h],bottom=h+1<headingY.size()?headingY[h+1]-10:getHeight()-10;
        auto area=juce::Rectangle<int>(18,y,getWidth()-36,std::max(32,bottom-y));
        g.setColour(Palette::panel.withAlpha(.55f));g.fillRoundedRectangle(area.toFloat(),7);
        g.setColour(Palette::panelEdge);g.drawRoundedRectangle(area.toFloat(),7,1);
        g.setColour(Palette::accent);g.fillRect(30,y+14,3,12);g.setFont(FamilyTheme::font(12,true));
        g.drawText(headings[h].first,42,y+8,getWidth()-84,26,juce::Justification::centredLeft);
    }
    for(const auto& c:controls){auto r=c.component->getBounds();g.setColour(Palette::panel);g.fillRoundedRectangle(r.expanded(5,5).withTop(r.getY()-22).toFloat(),7);g.setColour(Palette::dim);g.setFont(FamilyTheme::font(11,true));g.drawText(c.caption,r.getX(),r.getY()-22,r.getWidth(),20,juce::Justification::centred);}
}
}
