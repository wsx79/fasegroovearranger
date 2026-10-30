// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginBrowser.h"
#include "Localisation.h"
namespace fga {
namespace {
bool matchesType(const PluginCatalog::BrowserRow& row,int type){if(type==0)return true;if(type==2)return row.kind==PluginCatalog::BrowserRow::Kind::plugin&&!row.instrument;return row.kind!=PluginCatalog::BrowserRow::Kind::noEffect&&(row.kind!=PluginCatalog::BrowserRow::Kind::plugin||row.instrument);}
juce::StringArray visibleCategories(const juce::Array<juce::PluginDescription>& plugins,int type){juce::Array<juce::PluginDescription> visible;bool familyEffect=false;for(const auto& d:plugins)if(type==0||(type==1?d.isInstrument:!d.isInstrument)){visible.add(d);familyEffect=familyEffect||(!d.isInstrument&&d.manufacturerName.trim().equalsIgnoreCase("FaseGroove"));}auto result=PluginCatalog::pluginCategories(visible);if(type==2&&!familyEffect)result.removeString("FaseGroove");return result;}
}
PluginBrowser::PluginBrowser(PluginCatalog& c):catalog(c){
    setWantsKeyboardFocus(true);
    search.setFont(FamilyTheme::font(14.f));
    search.setColour(juce::TextEditor::backgroundColourId,Palette::background);
    search.setColour(juce::TextEditor::textColourId,Palette::text);
    search.setColour(juce::TextEditor::outlineColourId,Palette::panelEdge);
    search.setColour(juce::TextEditor::focusedOutlineColourId,Palette::accent);
    search.setIndents(8,6);
    search.onTextChange=[this]{rebuildList();};
    search.onEscapeKey=[this]{setVisible(false);};
    addAndMakeVisible(search);
    closeButton.onClick=[this]{setVisible(false);};
    addAndMakeVisible(closeButton);
    for(auto* b:{&rescanButton,&settingsButton,&openButton}){
        b->setColour(juce::TextButton::buttonColourId,Palette::background);
        b->setColour(juce::TextButton::textColourOffId,Palette::accent2.brighter(.3f));
        addAndMakeVisible(*b);
    }
    rescanButton.onClick=[this]{if(onRescan)onRescan();};
    settingsButton.onClick=[this]{if(onSettings)onSettings();};
    openButton.onClick=[this]{setVisible(false);if(onOpenFile)onOpenFile();};
    categories.setViewedComponent(&column,false);
    categories.setScrollBarsShown(true,false);
    addAndMakeVisible(categories);
    list.setModel(this);list.setRowHeight(36);list.addMouseListener(this,true);list.setMouseMoveSelectsRows(false);
    list.setColour(juce::ListBox::backgroundColourId,Palette::background);
    list.setColour(juce::ListBox::outlineColourId,Palette::panelEdge);
    list.setOutlineThickness(1);
    addAndMakeVisible(list);
    int type=0;for(auto* button:{&allButton,&instrumentButton,&effectButton}){addAndMakeVisible(*button);const int value=type++;button->onClick=[this,value]{if(typeLocked)return;pluginType=value;refreshText();resized();selectCategory(firstCategory);};}
    refreshText();setVisible(false);
}
juce::StringArray PluginBrowser::loadList(const char* key)const{return juce::StringArray::fromTokens(Localisation::setting(key),"|",{});}
void PluginBrowser::saveList(const char* key,const juce::StringArray& names)const{Localisation::setSetting(key,names.joinIntoString("|"));}
bool PluginBrowser::isFavourite(const PluginCatalog::BrowserRow& row)const{return row.key.isNotEmpty()&&favouriteKeys.contains(row.key);}
void PluginBrowser::toggleFavourite(int row){
    if(!juce::isPositiveAndBelow(row,(int)entries.size())||entries[(size_t)row].key.isEmpty())return;
    const auto key=entries[(size_t)row].key;
    if(favouriteKeys.contains(key))favouriteKeys.removeString(key);else favouriteKeys.add(key);
    saveList("favPlugins",favouriteKeys);
    if(category==favourites&&search.isEmpty())rebuildList();else list.repaintRow(row);
    refreshText();
}
void PluginBrowser::refreshText(){
    search.setTextToShowWhenEmpty(tr("Search a plugin..."),Palette::dim);
    closeButton.setButtonText(tr("CLOSE"));
    rescanButton.setButtonText(tr("RESCAN"));
    settingsButton.setButtonText(tr("PLUGIN SETTINGS"));
    openButton.setButtonText(tr("Open VST3 file..."));
    favouriteKeys=loadList("favPlugins");
    const auto plugins=catalog.state().plugins;
    const auto cats=visibleCategories(plugins,pluginType);
    const int count=firstCategory+cats.size();
    while(column.buttons.size()>count)column.buttons.removeLast();
    while(column.buttons.size()<count){
        auto* b=column.buttons.add(new juce::TextButton());
        const int index=column.buttons.size()-1;
        b->setColour(juce::TextButton::buttonColourId,Palette::background);
        b->setColour(juce::TextButton::buttonOnColourId,Palette::accent);
        b->setColour(juce::TextButton::textColourOffId,index<firstCategory?Palette::accent:Palette::text);
        b->setColour(juce::TextButton::textColourOnId,Palette::background);
        b->onClick=[this,index]{search.clear();selectCategory(index);};
        column.addAndMakeVisible(b);
    }
    for(int i=0;i<column.buttons.size();++i){
        auto* b=column.buttons[i];b->setVisible(i<count);if(i>=count)continue;
        juce::String name;int items=-1;
        if(i==favourites){name=juce::String::fromUTF8("\xe2\x98\x85 ")+tr("Favourites");items=favouriteKeys.size();}
        else if(i==recent)name=tr("Recent");
        else{name=tr(cats[i-firstCategory]);const auto rows=PluginCatalog::browserRows(plugins,cats[i-firstCategory],{},{});items=(int)std::count_if(rows.begin(),rows.end(),[this](const auto& row){return matchesType(row,pluginType);});}
        b->setButtonText(items>=0?name+"  ("+juce::String(items)+")":name);
    }
    repaint();
}
void PluginBrowser::show(int family,const juce::String& key,int type,bool locked){
    pluginType=type;typeLocked=locked;for(auto* button:{&allButton,&instrumentButton,&effectButton})button->setEnabled(!locked);
    engineFamily=family;currentKey=key;search.clear();
    if(auto* parent=getParentComponent())setBounds(parent->getLocalBounds());
    refreshText();resized();
    int openAt=firstCategory;
    if(currentKey.startsWith("fg\x1f"))openAt=firstCategory;
    else{const auto plugins=catalog.state().plugins;const auto cats=visibleCategories(plugins,pluginType);for(int i=0;i<plugins.size();++i)if(PluginCatalog::pluginKey(plugins.getReference(i))==currentKey){const auto maker=plugins.getReference(i).manufacturerName.trim().isEmpty()?"Unknown manufacturer":plugins.getReference(i).manufacturerName.trim();const int index=cats.indexOf(maker,true);if(index>=0)openAt=firstCategory+index;break;}}
    selectCategory(openAt);setVisible(true);toFront(true);list.grabKeyboardFocus();startTimerHz(4);
}
void PluginBrowser::visibilityChanged(){if(!isVisible())stopTimer();}
void PluginBrowser::selectCategory(int index){const auto cats=visibleCategories(catalog.state().plugins,pluginType);category=juce::jlimit(0,std::max(firstCategory,firstCategory+cats.size()-1),index);rebuildList();}
void PluginBrowser::rebuildList(){
    const auto plugins=catalog.state().plugins;const auto cats=visibleCategories(plugins,pluginType);const auto query=search.getText().trim();
    if(query.isNotEmpty())entries=PluginCatalog::browserRows(plugins,{},query,{});
    else if(category==favourites)entries=PluginCatalog::browserRows(plugins,{},{},favouriteKeys);
    else if(category==recent)entries=PluginCatalog::browserRows(plugins,{},{},loadList("recentPlugins"));
    else if(category-firstCategory>=0&&category-firstCategory<cats.size())entries=PluginCatalog::browserRows(plugins,cats[category-firstCategory],{},{});
    else entries.clear();
    entries.erase(std::remove_if(entries.begin(),entries.end(),[this](const auto& row){return !matchesType(row,pluginType);}),entries.end());
    allButton.setToggleState(pluginType==0,juce::dontSendNotification);instrumentButton.setToggleState(pluginType==1,juce::dontSendNotification);effectButton.setToggleState(pluginType==2,juce::dontSendNotification);
    for(int i=0;i<column.buttons.size();++i)column.buttons[i]->setToggleState(i==category&&query.isEmpty(),juce::dontSendNotification);
    if(column.buttons.size()>favourites)column.buttons[favourites]->setButtonText(juce::String::fromUTF8("\xe2\x98\x85 ")+tr("Favourites")+"  ("+juce::String(favouriteKeys.size())+")");
    list.updateContent();
    quiet=true;list.deselectAllRows();
    for(int r=0;r<(int)entries.size();++r)if(entries[(size_t)r].key==currentKey){list.selectRow(r);break;}
    quiet=false;repaint();
}
void PluginBrowser::choose(int row){
    if(quiet||!isVisible()||!juce::isPositiveAndBelow(row,(int)entries.size()))return;
    const auto entry=entries[(size_t)row];
    if(onChoose)onChoose(entry);
    currentKey=entry.key;
    if(entry.key.isNotEmpty()){auto names=loadList("recentPlugins");names.removeString(entry.key);names.insert(0,entry.key);while(names.size()>12)names.remove(names.size()-1);saveList("recentPlugins",names);}
}
void PluginBrowser::timerCallback(){
    if(!isVisible())return;const auto state=catalog.progressState();
    const auto stamp=juce::String(state.count)+":"+juce::String((int)state.scanning)+":"+juce::String(state.failed);
    // Repaint only when the scan progress shown in the subtitle moves.
    if(stamp!=listStamp){listStamp=stamp;refreshText();resized();rebuildList();}
    else if(state.checked!=paintedChecked){paintedChecked=state.checked;repaint();}
    rescanButton.setEnabled(!state.scanning);
}
int PluginBrowser::getNumRows(){return (int)entries.size();}
void PluginBrowser::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected){
    if(!juce::isPositiveAndBelow(row,(int)entries.size()))return;
    const auto& e=entries[(size_t)row];
    auto r=juce::Rectangle<int>(0,0,width,height).reduced(4,2).toFloat();
    if(selected){g.setColour(Palette::accent.withAlpha(.22f));g.fillRoundedRectangle(r,5.f);g.setColour(Palette::accent);g.drawRoundedRectangle(r,5.f,1.2f);}
    else if(row==hoverRow){g.setColour(Palette::accent.withAlpha(.09f));g.fillRoundedRectangle(r,5.f);g.setColour(Palette::accent.withAlpha(.45f));g.drawRoundedRectangle(r,5.f,1.f);}
    auto star=r.removeFromLeft((float)starWidth-4.f);
    if(e.key.isNotEmpty()){g.setColour(isFavourite(e)?Palette::accent:Palette::dim.withAlpha(.5f));g.setFont(FamilyTheme::font(17.f,true));g.drawText(juce::String::fromUTF8(isFavourite(e)?"\xe2\x98\x85":"\xe2\x98\x86"),star,juce::Justification::centred);}
    auto text=r.reduced(6.f,0.f);auto aside=text.removeFromRight(juce::jmin(220.f,text.getWidth()*.42f));
    const auto title=e.kind==PluginCatalog::BrowserRow::Kind::plugin?e.name:tr(e.name);
    g.setColour(selected?Palette::accent:Palette::text);g.setFont(FamilyTheme::font(15.f,selected));
    g.drawText(title,text.removeFromTop(text.getHeight()*.62f),juce::Justification::bottomLeft);
    g.setColour(Palette::dim);g.setFont(FamilyTheme::font(11.f));
    auto detail=e.kind==PluginCatalog::BrowserRow::Kind::plugin?tr(e.detail)+(e.format.isNotEmpty()?"  ·  "+e.format:juce::String()):tr(e.detail);
    g.drawText(detail,text,juce::Justification::topLeft);
    if(e.format.isNotEmpty()||e.kind!=PluginCatalog::BrowserRow::Kind::plugin){g.setColour(selected?Palette::text:Palette::accent);g.setFont(FamilyTheme::font(13.f,true));g.drawText(e.kind==PluginCatalog::BrowserRow::Kind::plugin?e.format:tr(e.manufacturer),aside,juce::Justification::centredRight);}
}
void PluginBrowser::selectedRowsChanged(int){list.repaint();}
void PluginBrowser::listBoxItemClicked(int row,const juce::MouseEvent& e){if(e.x<starWidth)toggleFavourite(row);else choose(row);}
void PluginBrowser::listBoxItemDoubleClicked(int row,const juce::MouseEvent& e){if(e.x<starWidth)return;choose(row);setVisible(false);}
void PluginBrowser::returnKeyPressed(int row){choose(row);setVisible(false);}
bool PluginBrowser::keyPressed(const juce::KeyPress& key){if(key==juce::KeyPress::escapeKey){setVisible(false);return true;}return false;}
void PluginBrowser::mouseUp(const juce::MouseEvent& e){if(e.eventComponent!=this)return;if(!card().contains(e.position))setVisible(false);}
void PluginBrowser::mouseMove(const juce::MouseEvent& e){
    // ListBox rows live inside its viewport, so the event can come from a child.
    if(e.eventComponent!=&list&&!list.isParentOf(e.eventComponent))return;
    const auto p=e.getEventRelativeTo(&list).position;
    const int row=list.getRowContainingPosition((int)p.x,(int)p.y);
    if(row!=hoverRow){const int previous=hoverRow;hoverRow=row;if(previous>=0)list.repaintRow(previous);if(hoverRow>=0)list.repaintRow(hoverRow);}
}
void PluginBrowser::mouseExit(const juce::MouseEvent& e){
    if(e.eventComponent!=&list&&!list.isParentOf(e.eventComponent))return;
    if(hoverRow>=0){const int previous=hoverRow;hoverRow=-1;list.repaintRow(previous);}
}
void PluginBrowser::resized(){
    auto inner=card().toNearestInt().reduced(24,18);
    auto top=inner.removeFromTop(44);
    closeButton.setBounds(top.removeFromRight(100).withSizeKeepingCentre(100,30));
    top.removeFromRight(10);search.setBounds(top.removeFromRight(280).withSizeKeepingCentre(280,30));
    auto tabs=inner.removeFromTop(30);allButton.setBounds(tabs.removeFromLeft(80).reduced(2));instrumentButton.setBounds(tabs.removeFromLeft(130).reduced(2));effectButton.setBounds(tabs.removeFromLeft(130).reduced(2));
    inner.removeFromTop(10);
    auto left=inner.removeFromLeft(230);inner.removeFromLeft(14);
    openButton.setBounds(left.removeFromBottom(30));left.removeFromBottom(6);
    settingsButton.setBounds(left.removeFromBottom(30));left.removeFromBottom(6);
    rescanButton.setBounds(left.removeFromBottom(30));left.removeFromBottom(8);
    const int visible=juce::jmax(1,firstCategory+visibleCategories(catalog.state().plugins,pluginType).size());
    const int rowH=30;column.setSize(left.getWidth(),visible*rowH+6);
    for(int i=0;i<column.buttons.size();++i){const int y=i*rowH+(i>recent?6:0);column.buttons[i]->setBounds(juce::Rectangle<int>(0,y,column.getWidth(),rowH).reduced(0,2));}
    categories.setBounds(left);list.setBounds(inner);
}
void PluginBrowser::paintOverChildren(juce::Graphics& g){if(!entries.empty())return;g.setColour(Palette::dim);g.setFont(FamilyTheme::font(14.f));g.drawFittedText(tr(catalog.progressState().scanning?"Scanning plugins...":"No plugins in this view. Search, rescan or open a plugin file."),list.getBounds().reduced(24),juce::Justification::centred,3);}
void PluginBrowser::paint(juce::Graphics& g){
    g.fillAll(juce::Colours::black.withAlpha(.78f));
    auto c=card();g.setColour(Palette::panel);g.fillRoundedRectangle(c,10.f);g.setColour(Palette::panelEdge.brighter(.3f));g.drawRoundedRectangle(c,10.f,1.f);
    auto top=c.reduced(24.f,18.f).removeFromTop(44.f);
    g.setColour(Palette::accent);g.setFont(FamilyTheme::font(20.f,true));g.drawText(Localisation::upper(tr("Plugins")),top.removeFromTop(26.f),juce::Justification::centredLeft);
    g.setColour(Palette::dim);g.setFont(FamilyTheme::font(12.f));
    const auto state=catalog.state();
    auto subtitle=tr("Click a plugin to load it, double-click or Enter to close. The star keeps it in your favourites.");
    if(state.scanning)subtitle=tr("Scanning plugins...")+"  "+juce::String(state.checked)+" / "+juce::String(state.total);
    g.drawText(subtitle,top,juce::Justification::centredLeft);
}
bool PluginBrowser::verifyLayout()const{
    const auto c=card().toNearestInt();
    if(!isVisible())return false;
    struct Item { const juce::Component* control; } items[]{{&closeButton},{&search},{&list},{&rescanButton},{&settingsButton},{&openButton},{&categories}};
    for(const auto& item:items){const auto b=item.control->getBounds();if(b.isEmpty()||!c.contains(b))return false;}
    bool fase=false;for(auto* b:column.buttons)if(b->isVisible()&&b->getButtonText().contains("FaseGroove"))fase=true;
    return fase&&list.getHeight()>80&&!entries.empty();
}
}
