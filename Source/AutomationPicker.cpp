// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "UiText.h"
namespace fga {
namespace {
juce::String text(const std::string& value){return juce::String::fromUTF8(value.c_str());}
juce::String parameterId(juce::AudioProcessorParameter* parameter){if(auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter))return hosted->getParameterID();return {};}
class AutomationPicker final : public juce::Component,private juce::ListBoxModel {
public:
    AutomationPicker(MainComponent& component,int track,int preferred):owner(component),alive(&component),session(component.project.tracks[(size_t)track].sessionId){
        if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);
        setSize(500,470);for(auto* c:std::initializer_list<juce::Component*>{&source,&search,&list,&choose,&remove})addAndMakeVisible(c);
        source.setComponentID("automation-source");search.setComponentID("automation-search");list.setComponentID("automation-parameters");choose.setComponentID("automation-choose");remove.setComponentID("automation-remove");
        search.setTextToShowWhenEmpty(tr("Search a parameter..."),Daw::textFaint);search.onTextChange=[this]{rebuild();};source.onChange=[this]{rebuild();};
        list.setModel(this);list.setRowHeight(36);list.setColour(juce::ListBox::backgroundColourId,Daw::base);list.setOutlineThickness(1);list.setColour(juce::ListBox::outlineColourId,Daw::line);
        choose.setButtonText(tr("SHOW AUTOMATION"));remove.setButtonText(tr("Remove lane"));choose.onClick=[this]{activate();};remove.onClick=[this]{removeLane();};
        const auto& t=owner.project.tracks[(size_t)track];sources.push_back({-3,tr("Existing automation")});sources.push_back({-2,tr("Track and master")});if(t.instrument=="internal"||t.instrument=="sample")sources.push_back({-1,tr("Sound")+": "+owner.soundSourceName(track)});if(auto* plugin=owner.audio.plugin(track))sources.push_back({0,tr("Instrument")+": "+plugin->getName()});
        int position=0;for(const auto& fx:t.effects){++position;if(owner.audio.effectPlugin(track,fx.slot))sources.push_back({fx.slot,tr("Effect")+" "+juce::String(position)+": "+text(fx.displayName())});}
        int selected=2;for(size_t i=0;i<sources.size();++i){source.addItem(sources[i].second,(int)i+1);if(sources[i].first==preferred)selected=(int)i+1;}source.setSelectedId(selected,juce::dontSendNotification);rebuild();
        Localisation::bindTip(source,"Choose the track, its native sound, instrument plugin or one effect instance.");Localisation::bindTip(search,"Filter the parameters exposed by the selected sound or plugin.");Localisation::bindTip(choose,"Show the selected automation lane. Existing curves are reused.");Localisation::bindTip(remove,"Remove this parameter's automation curve. Undo restores it.");
    }
    void paint(juce::Graphics& g)override{if(!alive)return;g.fillAll(Daw::surface);g.setColour(Daw::text);g.setFont(FamilyTheme::font(16,true));g.drawText(tr("AUTOMATION"),juce::Rectangle<int>(16,8,getWidth()-32,28),juce::Justification::centredLeft);g.setColour(Daw::textFaint);g.setFont(FamilyTheme::font(11));const int track=owner.trackForSession(session);if(track>=0)g.drawText(juce::String((int)owner.project.tracks[(size_t)track].parameters.size())+" / "+juce::String(maxParameterLanes)+"  "+tr("Parameter lanes"),juce::Rectangle<int>(16,getHeight()-70,getWidth()-32,18),juce::Justification::centredLeft);if(entries.empty())g.drawText(tr("No matching parameters"),list.getBounds().reduced(10),juce::Justification::centred);}
    void resized()override{auto r=getLocalBounds().reduced(16);r.removeFromTop(28);source.setBounds(r.removeFromTop(30));r.removeFromTop(8);search.setBounds(r.removeFromTop(28));r.removeFromTop(8);auto bottom=r.removeFromBottom(68);bottom.removeFromTop(30);remove.setBounds(bottom.removeFromLeft(130));bottom.removeFromLeft(8);choose.setBounds(bottom);list.setBounds(r);}
    int getNumRows()override{return (int)entries.size();}
    juce::String getTooltipForRow(int row)override{if(row<0||row>=(int)entries.size())return {};const auto& entry=entries[(size_t)row];return entry.title+(entry.value.isEmpty()?juce::String():"\n"+entry.value);}
    void paintListBoxItem(int index,juce::Graphics& g,int width,int height,bool selected)override{if(index<0||index>=(int)entries.size())return;const auto& entry=entries[(size_t)index];if(selected){g.setColour(Daw::accent.withAlpha(.15f));g.fillRect(0,0,width,height);}g.setColour(entry.existing?Daw::automation:Daw::text);g.setFont(FamilyTheme::font(12,selected));g.drawFittedText(entry.title,juce::Rectangle<int>(10,0,width-115,height),juce::Justification::centredLeft,1,.6f);g.setColour(Daw::textFaint);g.setFont(FamilyTheme::font(10));g.drawText(entry.existing?tr("Already added"):entry.value,juce::Rectangle<int>(width-110,0,100,height),juce::Justification::centredRight);}
    void selectedRowsChanged(int)override{const int row=list.getSelectedRow();choose.setEnabled(row>=0&&row<(int)entries.size());remove.setEnabled(row>=0&&row<(int)entries.size()&&entries[(size_t)row].existing&&entries[(size_t)row].slot>=-1);}
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&)override{activate();}
    void returnKeyPressed(int)override{activate();}
private:
    struct Entry {int slot,index;juce::String title,id,value;bool existing=false;};
    MainComponent& owner;juce::Component::SafePointer<MainComponent> alive;std::uint64_t session;
    juce::ComboBox source;juce::TextEditor search;juce::ListBox list;
    juce::TextButton choose,remove;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::vector<std::pair<int,juce::String>> sources;std::vector<Entry> entries;
    void rebuild(){if(!alive)return;entries.clear();const int track=owner.trackForSession(session);if(track<0)return;const auto& t=owner.project.tracks[(size_t)track];const int selection=source.getSelectedId()-1;if(selection<0||selection>=(int)sources.size())return;const int slot=sources[(size_t)selection].first;const auto query=search.getText().trim();
        auto add=[&](int target,int index,const juce::String& title,const juce::String& id={},const juce::String& value={}){if(query.isNotEmpty()&&!title.containsIgnoreCase(query))return;bool existing=target==-2&&(index==0?!t.volume.isDefault():index==1?!t.panAutomation.isDefault():!owner.project.masterAutomation.isDefault());for(const auto& lane:t.parameters)if(lane.slot==target&&((id.isNotEmpty()&&text(lane.parameterId)==id)||(id.isEmpty()&&lane.index==index)))existing=true;entries.push_back({target,index,title,id,value,existing});};
        if(slot==-2){add(-2,0,tr("Volume"));add(-2,1,tr("Pan"));add(-2,2,tr("Master volume"));}
        else if(slot==-3){if(!t.volume.isDefault())add(-2,0,tr("Volume"));if(!t.panAutomation.isDefault())add(-2,1,tr("Pan"));if(!owner.project.masterAutomation.isDefault())add(-2,2,tr("Master volume"));for(const auto& lane:t.parameters){juce::String prefix=lane.slot<0?tr("Sound"):lane.slot==0?tr("Instrument"):t.effect(lane.slot)?text(t.effect(lane.slot)->displayName()):tr("Missing effect");add(lane.slot,lane.index,prefix+" / "+text(lane.name),text(lane.parameterId));}}
        else if(slot==-1){if(t.instrument=="internal"){add(-1,0,tr("Tone"));add(-1,1,tr("Reverb"));add(-1,2,tr("Chorus"));add(-1,3,tr("Pump"));}else if(t.instrument=="sample"){add(-1,4,tr("Filter cutoff"));add(-1,5,tr("Filter resonance"));}}
        else if(auto* plugin=slot==0?owner.audio.plugin(track):owner.audio.effectPlugin(track,slot)){for(int i=0;i<plugin->getParameters().size();++i){auto* p=plugin->getParameters()[i];if(!p->isAutomatable())continue;const auto name=p->getName(128);if(name.isNotEmpty())add(slot,i,name,parameterId(p),p->getText(p->getValue(),32));}}
        list.updateContent();list.deselectAllRows();choose.setEnabled(false);remove.setEnabled(false);repaint();
    }
    void dismiss(){if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();}
    void activate(){if(!alive)return;const int row=list.getSelectedRow(),track=owner.trackForSession(session);if(track<0||row<0||row>=(int)entries.size())return;const auto entry=entries[(size_t)row];owner.selectAutomation(track,entry.slot,entry.index,entry.title,entry.id);dismiss();}
    void removeLane(){if(!alive)return;const int row=list.getSelectedRow(),track=owner.trackForSession(session);if(track<0||row<0||row>=(int)entries.size()||owner.pluginsBusy()||owner.editingRestricted())return;const auto entry=entries[(size_t)row];auto& lanes=owner.project.tracks[(size_t)track].parameters;auto it=std::find_if(lanes.begin(),lanes.end(),[&](const auto& lane){return lane.slot==entry.slot&&(entry.id.isNotEmpty()?text(lane.parameterId)==entry.id:lane.index==entry.index);});if(it==lanes.end())return;owner.checkpoint();lanes.erase(it);owner.resetAutomationTarget(track);owner.changed(false);rebuild();}
};
}
void MainComponent::resetAutomationTarget(int track){arrangement.laneTarget[(size_t)track]=ArrangementView::laneVolume;}
void MainComponent::showAutomationPicker(int track,int source){if(loadingPlugin||editingRestricted()||!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return;if(automationPicker)automationPicker->dismiss();select(track,selectedSection);arrangement.volumeVisible[(size_t)track]=true;updateSizes();arrangement.repaint();auto content=std::make_unique<AutomationPicker>(*this,track,source);auto& box=juce::CallOutBox::launchAsynchronously(std::move(content),automationButton.getScreenBounds(),nullptr);automationPicker=&box;box.setDismissalMouseClicksAreAlwaysConsumed(false);}
void MainComponent::selectAutomation(int track,int slot,int index,const juce::String& name,const juce::String& id){if(loadingPlugin||editingRestricted()||!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return;auto& t=project.tracks[(size_t)track];int target=ArrangementView::laneVolume;
    if(slot==-2){if(index<0||index>2)return;target=index;}
    else{
        int found=-1;for(size_t i=0;i<t.parameters.size();++i)if(t.parameters[i].slot==slot&&(id.isNotEmpty()?text(t.parameters[i].parameterId)==id:t.parameters[i].index==index)){found=(int)i;break;}
        if(found<0){if(slot>=0){auto* plugin=slot==0?audio.plugin(track):audio.effectPlugin(track,slot);if(!plugin||index<0||index>=plugin->getParameters().size())return;if(id.isNotEmpty()&&parameterId(plugin->getParameters()[index])!=id)return;}
if(t.parameters.size()>=(size_t)maxParameterLanes){message("The automation lane limit has been reached.",true);return;}checkpoint();ParameterAutomation lane;lane.slot=slot;lane.index=index;lane.name=name.toStdString();lane.parameterId=id.toStdString();t.parameters.push_back(std::move(lane));found=(int)t.parameters.size()-1;changed(false);}target=ArrangementView::lanePlugin+found;}
    arrangement.laneTarget[(size_t)track]=target;arrangement.volumeVisible[(size_t)track]=true;updateSizes();arrangement.repaint();
}
}
