// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleBrowser.h"
#include "AudioEngine.h"
#include "Localisation.h"
namespace fga {
namespace {
constexpr int gap=8,rowH=26;
constexpr const char* settingKey="sampleBrowser";
// Tooltips go through the arranger's control translation (fgaTip), like every other control.
void tip(juce::Component& c,const char* english){c.getProperties().set("fgaTip",english);if(auto* client=dynamic_cast<juce::SettableTooltipClient*>(&c))client->setTooltip(tr(english));}
juce::String channelsText(int channels){return channels==1?tr("Mono"):channels==2?tr("Stereo"):juce::String(channels)+" "+tr("channels");}
void caption(juce::Graphics& g,const juce::String& text,juce::Rectangle<int> r){
    g.setColour(Palette::accent);g.fillRoundedRectangle((float)r.getX(),(float)r.getCentreY()-6,3,12,1);
    g.setColour(Palette::text);g.setFont(FamilyTheme::font(11.5f,true));g.drawText(text,r.withTrimmedLeft(9),juce::Justification::centredLeft);
}
void folderIcon(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour c){
    juce::Path p;const float x=r.getX(),y=r.getY()+2,w=r.getWidth(),h=r.getHeight()-4;
    p.startNewSubPath(x,y+h);p.lineTo(x,y);p.lineTo(x+w*.38f,y);p.lineTo(x+w*.48f,y+h*.2f);p.lineTo(x+w,y+h*.2f);p.lineTo(x+w,y+h);p.closeSubPath();
    g.setColour(c.withAlpha(.25f));g.fillPath(p);g.setColour(c);g.strokePath(p,juce::PathStrokeType(1.2f));
}
void waveIcon(juce::Graphics& g,juce::Rectangle<float> r,juce::Colour c){
    g.setColour(c);const float mid=r.getCentreY();const float heights[]{.3f,.7f,1.f,.55f,.85f,.4f,.2f};
    for(int i=0;i<7;++i){const float h=r.getHeight()*.5f*heights[i];g.fillRect(r.getX()+i*r.getWidth()/7.f,mid-h,1.4f,h*2);}
}
}
struct SampleBrowser::Shared {
    std::atomic<int> generation{0},previewGeneration{0},placesGeneration{0};
    samples::InfoCache cache;
};

class SampleBrowser::Places : public juce::ListBoxModel {
public:
    explicit Places(SampleBrowser& b):owner(b){}
    std::vector<samples::Folder> items;
    std::vector<bool> offline;
    void rebuild(){items=owner.prefs.ordered();offline.assign(items.size(),false);}
    int getNumRows() override {return (int)items.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool) override {
        if(!juce::isPositiveAndBelow(row,(int)items.size()))return;
        const auto& item=items[(size_t)row];const juce::File f(item.path);
        const bool here=owner.folder==f||owner.folder.isAChildOf(f);const bool gone=row<(int)offline.size()&&offline[(size_t)row];
        auto r=juce::Rectangle<float>(0,0,(float)width,(float)height).reduced(2,1);
        if(here){g.setColour(Palette::accent2.withAlpha(.16f));g.fillRoundedRectangle(r,4);}
        auto star=r.removeFromLeft(22);
        juce::Path s;s.addStar(star.getCentre(),5,3.f,7.f);g.setColour(item.favourite?Palette::accent:Palette::dim.withAlpha(.6f));
        if(item.favourite)g.fillPath(s);else g.strokePath(s,juce::PathStrokeType(1.f));
        folderIcon(g,r.removeFromLeft(18).reduced(1,5),gone?Palette::dim:Palette::accent2);
        r.removeFromLeft(6);
        if(gone){g.setColour(Palette::toms);g.setFont(FamilyTheme::font(9.5f,true));g.drawText(tr("offline"),r.removeFromRight(52).toNearestInt(),juce::Justification::centredRight);}
        g.setColour(gone?Palette::dim:here?Palette::text:Palette::text.withAlpha(.85f));g.setFont(FamilyTheme::font(12.f,here));
        g.drawFittedText(f.getFileName().isEmpty()?item.path:f.getFileName(),r.toNearestInt(),juce::Justification::centredLeft,1,.8f);
    }
    void listBoxItemClicked(int row,const juce::MouseEvent& e) override {
        if(!juce::isPositiveAndBelow(row,(int)items.size()))return;const auto path=items[(size_t)row].path;
        if(e.mods.isPopupMenu()){owner.showPlaceMenu(path);return;}
        if(e.x<24){owner.toggleFavourite(path);return;}
        owner.navigate(juce::File(path));
    }
    juce::String getTooltipForRow(int row) override {return juce::isPositiveAndBelow(row,(int)items.size())?items[(size_t)row].path:juce::String();}
private:
    SampleBrowser& owner;
};

class SampleBrowser::Files : public juce::ListBoxModel {
public:
    explicit Files(SampleBrowser& b):owner(b){}
    int getNumRows() override {return (int)owner.rows.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override {
        if(!juce::isPositiveAndBelow(row,(int)owner.rows.size()))return;
        const auto& e=owner.entries[(size_t)owner.rows[(size_t)row]];
        auto r=juce::Rectangle<float>(0,0,(float)width,(float)height).reduced(2,1);
        g.setColour(selected?Palette::accent2.withAlpha(.22f):row%2?Palette::panel.withAlpha(.55f):juce::Colours::transparentBlack);g.fillRoundedRectangle(r,3);
        if(selected){g.setColour(Palette::accent2);g.fillRoundedRectangle(r.getX(),r.getY()+4,2,r.getHeight()-8,1);}
        r.removeFromLeft(6);auto icon=r.removeFromLeft(16).reduced(0,(float)height*.25f);
        const bool supported=!e.folder&&samples::isSupportedAudio(e.file),audio=!e.folder&&samples::isAudioFile(e.file);
        if(e.folder)folderIcon(g,icon,Palette::accent2);else if(audio)waveIcon(g,icon,supported?(selected?Palette::accent2:Palette::text.withAlpha(.7f)):Palette::dim);
        r.removeFromLeft(8);
        juce::String right;juce::Colour rightColour=Palette::dim;
        if(!e.folder){samples::AudioInfo info;
            if(audio&&!supported){right=tr("n/a");rightColour=Palette::toms.withAlpha(.8f);}
            else if(supported&&owner.shared->cache.get(e,info))right=info.valid?samples::formatDuration(info.seconds):tr("n/a");
            else right=samples::formatSize(e.size);}
        if(right.isNotEmpty()){g.setColour(rightColour);g.setFont(FamilyTheme::font(10.5f));g.drawText(right,r.removeFromRight(60).toNearestInt().withTrimmedRight(4),juce::Justification::centredRight);}
        g.setColour(e.folder||supported?Palette::text:Palette::dim);g.setFont(FamilyTheme::font(12.5f,e.folder));
        g.drawFittedText(e.folder?e.name:e.file.getFileNameWithoutExtension()+e.file.getFileExtension().toLowerCase(),r.toNearestInt(),juce::Justification::centredLeft,1,.85f);
    }
    void selectedRowsChanged(int) override {if(!owner.quiet)owner.selectionChanged();}
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override {owner.open(row);}
    void returnKeyPressed(int row) override {owner.open(row);}
    void deleteKeyPressed(int) override {}
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selected) override {
        if(selected.size()!=1)return {};const auto f=owner.fileAt(selected[0]);
        return samples::isSupportedAudio(f)?juce::var(juce::String(dragPrefix)+f.getFullPathName()):juce::var();
    }
    juce::String getTooltipForRow(int row) override {return owner.fileAt(row).getFullPathName();}
private:
    SampleBrowser& owner;
};

class SampleBrowser::Crumbs : public juce::Component {
public:
    explicit Crumbs(SampleBrowser& b):owner(b){}
    juce::Array<juce::File> chain;
    std::vector<std::pair<juce::Rectangle<int>,juce::File>> boxes;
    juce::String nameOf(const juce::File& f) const {const auto n=f.getFileName();return n.isEmpty()?f.getFullPathName():n;}
    void update(){chain=owner.folder==juce::File()?juce::Array<juce::File>():samples::breadcrumb(owner.folder,owner.prefs.rootFor(owner.folder));layout();repaint();}
    void resized() override {layout();}
    void layout(){
        boxes.clear();const auto font=FamilyTheme::font(11.5f,true);const int sep=14,ellipsis=18;int x=getWidth();
        for(int i=chain.size()-1;i>=0;--i){const int w=std::min(getWidth()-ellipsis,(int)std::ceil(juce::GlyphArrangement::getStringWidth(font,nameOf(chain[i])))+10);
            if(x-w-(i>0?sep:0)<(i>0?ellipsis:0)&&!boxes.empty())break;x-=w;boxes.insert(boxes.begin(),{{x,0,w,getHeight()},chain[i]});x-=sep;}
        const int shift=boxes.empty()?0:boxes.front().first.getX()-(boxes.front().second==chain[0]?0:ellipsis);for(auto& b:boxes)b.first.translate(-shift,0);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(Palette::field);g.fillRoundedRectangle(getLocalBounds().toFloat(),4);
        if(!boxes.empty()&&boxes.front().second!=chain[0]){g.setColour(Palette::dim);g.setFont(FamilyTheme::font(11.5f,true));g.drawText(juce::String::fromUTF8("…"),juce::Rectangle<int>(0,0,18,getHeight()),juce::Justification::centred);}
        for(size_t i=0;i<boxes.size();++i){const bool last=i+1==boxes.size();const auto& [box,file]=boxes[i];
            if(box==hover&&!last){g.setColour(Palette::control);g.fillRoundedRectangle(box.toFloat().reduced(0,3),3);}
            g.setColour(last?Palette::text:Palette::accent2.brighter(.2f));g.setFont(FamilyTheme::font(11.5f,true));g.drawFittedText(nameOf(file),box.reduced(5,0),juce::Justification::centredLeft,1,.7f);
            if(!last){g.setColour(Palette::dim);g.drawText("/",juce::Rectangle<int>(box.getRight(),0,14,getHeight()),juce::Justification::centred);}}
    }
    void mouseMove(const juce::MouseEvent& e) override {juce::Rectangle<int> next;for(const auto& b:boxes)if(b.first.contains(e.getPosition()))next=b.first;if(next!=hover){hover=next;repaint();}}
    void mouseExit(const juce::MouseEvent&) override {hover={};repaint();}
    void mouseUp(const juce::MouseEvent& e) override {for(const auto& b:boxes)if(b.first.contains(e.getPosition())&&b.second!=owner.folder){owner.navigate(b.second);return;}}
private:
    SampleBrowser& owner;
    juce::Rectangle<int> hover;
};

SampleBrowser::SampleBrowser(AudioEngine& e):engine(e),shared(std::make_shared<Shared>()){
    pool=std::make_unique<juce::ThreadPool>(juce::ThreadPoolOptions{}.withThreadName("Sample browser").withNumberOfThreads(2));
    prefs=samples::Preferences::fromJson(Localisation::setting(settingKey));
    places=std::make_unique<Places>(*this);files=std::make_unique<Files>(*this);crumbs=std::make_unique<Crumbs>(*this);
    places->rebuild();
    for(auto* list:{&placeList,&fileList}){list->setColour(juce::ListBox::backgroundColourId,Palette::field);list->setColour(juce::ListBox::outlineColourId,Palette::panelEdge);list->setOutlineThickness(1);addAndMakeVisible(list);}
    placeList.setModel(places.get());placeList.setRowHeight(24);
    fileList.setModel(files.get());fileList.setRowHeight(24);fileList.setMultipleSelectionEnabled(false);fileList.setWantsKeyboardFocus(true);
    addAndMakeVisible(*crumbs);
    search.setFont(FamilyTheme::font(13.f));search.setIndents(8,5);search.setColour(juce::TextEditor::backgroundColourId,Palette::field);
    search.onTextChange=[this]{rebuildRows();};search.onEscapeKey=[this]{search.clear();rebuildRows();fileList.grabKeyboardFocus();};search.onReturnKey=[this]{fileList.grabKeyboardFocus();if(fileList.getSelectedRow()<0&&!rows.empty())fileList.selectRow(0);};
    addAndMakeVisible(search);
    for(auto* box:{&filterBox,&sortBox})addAndMakeVisible(box);
    filterBox.onChange=[this]{if(quiet)return;prefs.filter=std::max(0,filterBox.getSelectedItemIndex());savePreferences();rebuildRows();};
    sortBox.onChange=[this]{if(quiet)return;prefs.sort=std::max(0,sortBox.getSelectedItemIndex());savePreferences();rebuildRows();};
    orderButton.onClick=[this]{prefs.ascending=!prefs.ascending;savePreferences();refreshText();rebuildRows();};
    autoButton.setClickingTogglesState(true);autoButton.setToggleState(prefs.autoPreview,juce::dontSendNotification);
    autoButton.setColour(juce::TextButton::buttonOnColourId,Palette::accent2);
    autoButton.onClick=[this]{prefs.autoPreview=autoButton.getToggleState();savePreferences();};
    addButton.onClick=[this]{chooseFolder();};
    loadButton.setColour(juce::TextButton::buttonColourId,Palette::accent);loadButton.onClick=[this]{loadSelected();};
    backButton.onClick=[this]{back();};forwardButton.onClick=[this]{forward();};upButton.onClick=[this]{up();};
    playButton.onColour=Palette::accent2;playButton.primary=true;
    playButton.onClick=[this]{if(engine.isPreviewing()||isTimerRunning())stopPreview();else preview(fileAt(fileList.getSelectedRow()));};
    for(juce::Component* c:std::initializer_list<juce::Component*>{&addButton,&orderButton,&autoButton,&loadButton,&backButton,&forwardButton,&upButton,&playButton})addAndMakeVisible(c);
    backButton.setConnectedEdges(juce::Button::ConnectedOnRight);forwardButton.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);upButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    setWantsKeyboardFocus(false);refreshText();updateButtons();
}
SampleBrowser::~SampleBrowser(){
    ++shared->generation;++shared->previewGeneration;++shared->placesGeneration;
    pool->removeAllJobs(true,5000);
    engine.stopPreview();
    placeList.setModel(nullptr);fileList.setModel(nullptr);
}
void SampleBrowser::refreshText(){
    quiet=true;
    filterBox.clear(juce::dontSendNotification);int id=1;for(const char* name:{"All audio","WAV","AIFF","FLAC","OGG","All files"})filterBox.addItem(tr(name),id++);
    sortBox.clear(juce::dontSendNotification);id=1;for(const char* name:{"Name","Date","Size","Length"})sortBox.addItem(tr(name),id++);
    filterBox.setSelectedItemIndex(prefs.filter,juce::dontSendNotification);sortBox.setSelectedItemIndex(prefs.sort,juce::dontSendNotification);quiet=false;
    search.setTextToShowWhenEmpty(tr("Search samples..."),Palette::dim);
    addButton.setButtonText(tr("ADD FOLDER"));autoButton.setButtonText(tr("AUTO"));upButton.setButtonText(juce::String::fromUTF8("↑"));
    orderButton.setButtonText(juce::String::fromUTF8(prefs.ascending?"↑":"↓"));
    tip(addButton,"Add folders of samples: they stay in the list for every project.");
    tip(filterBox,"File type");tip(sortBox,"Sort by");tip(orderButton,"Sort ascending / descending");
    tip(backButton,"Back");tip(forwardButton,"Forward");tip(upButton,"Up one folder (Backspace)");
    tip(playButton,"Play / stop the preview");tip(autoButton,"Preview every file you select");
    tip(placeList,"Click a folder to open it, the star for favourites, right-click to edit or remove it.");
    tip(loadButton,"Load the selected sample into the selected track's sampler (double-click or Enter). Drag it onto a track to choose the track.");
    refreshTarget();repaint();
}
void SampleBrowser::refreshTarget(){
    // LOAD names its target: the selected track.
    const auto name=targetName?targetName():juce::String();
    loadButton.setButtonText(tr("LOAD")+(name.isNotEmpty()?juce::String::fromUTF8("  ·  ")+Localisation::upper(name):juce::String()));
}
void SampleBrowser::savePreferences(){Localisation::setSetting(settingKey,prefs.toJson());}
void SampleBrowser::visibilityChanged(){
    if(!isVisible()){stopPreview();return;}
    checkPlaces();
    if(folder==juce::File()){
        juce::File start(prefs.lastFolder);
        if(prefs.lastFolder.isEmpty()&&!prefs.folders.empty())start=juce::File(prefs.ordered().front().path);
        if(start!=juce::File())navigate(start);
    }
}
bool SampleBrowser::addFolder(const juce::File& f){
    if(!prefs.addFolder(f))return false;
    savePreferences();places->rebuild();placeList.updateContent();checkPlaces();resized();repaint();return true;
}
void SampleBrowser::removeFolder(const juce::String& path){
    if(!prefs.removeFolder(path))return;
    savePreferences();places->rebuild();placeList.updateContent();checkPlaces();crumbs->update();resized();repaint();
}
void SampleBrowser::toggleFavourite(const juce::String& path){prefs.toggleFavourite(path);savePreferences();places->rebuild();placeList.updateContent();checkPlaces();placeList.repaint();}
void SampleBrowser::chooseFolder(const juce::String& replacing){
    const auto start=replacing.isNotEmpty()?juce::File(replacing):folder!=juce::File()?folder:juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    chooser=std::make_unique<juce::FileChooser>(tr("Choose a folder of samples"),start);
    juce::Component::SafePointer<SampleBrowser> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe,replacing](const juce::FileChooser& fc){
        if(safe==nullptr)return;const auto chosen=fc.getResult();if(!chosen.isDirectory())return;
        if(replacing.isNotEmpty()){if(safe->prefs.replaceFolder(replacing,chosen)){safe->savePreferences();safe->places->rebuild();safe->placeList.updateContent();safe->checkPlaces();safe->navigate(chosen);}return;}
        safe->addFolder(chosen);safe->navigate(chosen);
    });
}
void SampleBrowser::showPlaceMenu(const juce::String& path){
    juce::PopupMenu m;m.addSectionHeader(juce::File(path).getFileName());
    m.addItem(1,tr("Open"));m.addItem(2,tr("Favourite"),true,prefs.isFavourite(path));m.addItem(3,tr("Change folder..."));m.addItem(5,tr("Show in file manager"),juce::File(path).isDirectory());
    m.addSeparator();m.addItem(4,tr("Remove from the list"));
    juce::Component::SafePointer<SampleBrowser> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&placeList),[safe,path](int id){if(safe==nullptr)return;
        if(id==1)safe->navigate(juce::File(path));if(id==2)safe->toggleFavourite(path);if(id==3)safe->chooseFolder(path);if(id==4)safe->removeFolder(path);if(id==5)juce::File(path).revealToUser();});
}
// Availability of the saved folders, off the message thread (a sleeping network drive must
// not freeze the UI).
void SampleBrowser::checkPlaces(){
    const int gen=++shared->placesGeneration;auto state=shared;juce::StringArray paths;for(const auto& p:places->items)paths.add(p.path);
    juce::Component::SafePointer<SampleBrowser> safe(this);
    pool->addJob([state,gen,paths,safe]{
        std::vector<bool> offline;for(const auto& p:paths){if(state->placesGeneration.load()!=gen)return;offline.push_back(!juce::File(p).isDirectory());}
        juce::MessageManager::callAsync([safe,gen,offline]{if(safe==nullptr||safe->shared->placesGeneration.load()!=gen||offline.size()!=safe->places->items.size())return;safe->places->offline=offline;safe->placeList.repaint();});
    });
}
void SampleBrowser::navigate(const juce::File& f,bool remember){
    if(f==juce::File())return;
    folder=f;if(remember)history.visit(f);
    prefs.lastFolder=f.getFullPathName();savePreferences();
    entries.clear();rows.clear();reading=true;available=true;listingError.clear();info.clear();infoError=false;
    quiet=true;fileList.deselectAllRows();fileList.updateContent();fileList.scrollToEnsureRowIsOnscreen(0);quiet=false;
    crumbs->update();updateButtons();placeList.repaint();repaint();
    const int gen=++shared->generation;auto state=shared;juce::Component::SafePointer<SampleBrowser> safe(this);
    pool->addJob([state,gen,f,safe]{
        auto listing=std::make_shared<samples::Listing>(samples::list(f,[&]{return state->generation.load()!=gen;}));
        if(state->generation.load()!=gen)return;
        const auto items=listing->entries;
        juce::MessageManager::callAsync([safe,gen,listing]{if(safe!=nullptr&&safe->shared->generation.load()==gen)safe->listingArrived(std::move(*listing));});
        // Audio headers (duration, rate, channels) for the list, the info line and length sorting.
        juce::AudioFormatManager formats;formats.registerBasicFormats();auto last=juce::Time::getMillisecondCounter();bool pending=false;
        auto post=[&]{pending=false;last=juce::Time::getMillisecondCounter();juce::MessageManager::callAsync([safe,gen]{if(safe!=nullptr&&safe->shared->generation.load()==gen)safe->infoArrived();});};
        for(const auto& e:items){
            if(state->generation.load()!=gen)return;
            samples::AudioInfo known;if(e.folder||!samples::isSupportedAudio(e.file)||state->cache.get(e,known))continue;
            state->cache.put(e,samples::readInfo(e.file,formats));pending=true;
            if(juce::Time::getMillisecondCounter()-last>250)post();
        }
        if(pending)post();
    });
}
void SampleBrowser::listingArrived(samples::Listing listing){
    reading=false;available=listing.available;listingError=listing.error;entries=std::move(listing.entries);
    rebuildRows(false);updateButtons();repaint();
}
void SampleBrowser::infoArrived(){
    if(prefs.sort==samples::byLength)rebuildRows();else fileList.repaint();
    if(fileList.getSelectedRow()>=0)showFile(fileAt(fileList.getSelectedRow()));
}
double SampleBrowser::lengthOf(const samples::Entry& e) const{samples::AudioInfo i;return shared->cache.get(e,i)&&i.valid?i.seconds:-1.;}
void SampleBrowser::rebuildRows(bool keepSelection){
    const auto selected=keepSelection?fileAt(fileList.getSelectedRow()):juce::File();
    rows=samples::arrange(entries,search.getText(),prefs.filter,prefs.sort,prefs.ascending,[this](const samples::Entry& e){return lengthOf(e);});
    quiet=true;fileList.updateContent();fileList.deselectAllRows();
    if(selected!=juce::File())for(int r=0;r<(int)rows.size();++r)if(entries[(size_t)rows[(size_t)r]].file==selected){fileList.selectRow(r);break;}
    quiet=false;fileList.repaint();repaint();
}
juce::File SampleBrowser::fileAt(int row) const{return juce::isPositiveAndBelow(row,(int)rows.size())?entries[(size_t)rows[(size_t)row]].file:juce::File();}
void SampleBrowser::selectRow(int row){fileList.selectRow(row);}
void SampleBrowser::setSearch(const juce::String& text){search.setText(text,false);rebuildRows();}
void SampleBrowser::setFilter(int filter){filterBox.setSelectedItemIndex(std::clamp(filter,0,(int)samples::numFilters-1),juce::sendNotificationSync);}
void SampleBrowser::setSort(int sort,bool ascending){prefs.ascending=ascending;refreshText();sortBox.setSelectedItemIndex(std::clamp(sort,0,(int)samples::numSorts-1),juce::dontSendNotification);prefs.sort=sortBox.getSelectedItemIndex();savePreferences();rebuildRows();}
void SampleBrowser::back(){if(history.canBack())navigate(history.back(),false);}
void SampleBrowser::forward(){if(history.canForward())navigate(history.forward(),false);}
void SampleBrowser::up(){const auto parent=folder.getParentDirectory();if(folder!=juce::File()&&parent!=folder)navigate(parent);}
void SampleBrowser::selectionChanged(){
    const auto f=fileAt(fileList.getSelectedRow());showFile(f);updateButtons();
    if(prefs.autoPreview&&samples::isSupportedAudio(f))preview(f);
}
void SampleBrowser::showFile(const juce::File& f){
    infoError=false;
    if(f==juce::File()){info.clear();}
    else if(f.isDirectory()){info=tr("Folder")+": "+f.getFileName();}
    else if(!samples::isSupportedAudio(f)){info=tr(samples::isAudioFile(f)?"Not supported: use WAV, AIFF, FLAC or OGG.":"Not an audio file.");infoError=true;}
    else{
        samples::Entry e;for(int r:rows)if(entries[(size_t)r].file==f){e=entries[(size_t)r];break;}
        samples::AudioInfo i;
        if(!shared->cache.get(e,i))info=tr("Reading the file...");
        else if(!i.valid){info=tr("Unreadable audio file.");infoError=true;}
        else info=samples::formatDuration(i.seconds)+juce::String::fromUTF8("  ·  ")+i.format+juce::String::fromUTF8("  ·  ")+juce::String(i.sampleRate/1000.,1)+" kHz"
                  +juce::String::fromUTF8("  ·  ")+channelsText(i.channels)+(i.bits>0?juce::String::fromUTF8("  ·  ")+juce::String(i.bits)+"-bit":juce::String())
                  +juce::String::fromUTF8("  ·  ")+samples::formatSize(e.size);
    }
    repaint(infoArea);
}
void SampleBrowser::open(int row){
    const auto f=fileAt(row);if(f==juce::File())return;
    if(f.isDirectory()){navigate(f);return;}
    fileList.selectRow(row);loadSelected();
}
bool SampleBrowser::loadSelected(){
    const auto f=fileAt(fileList.getSelectedRow());
    if(f==juce::File()||f.isDirectory())return false;
    if(!samples::isSupportedAudio(f)){showFile(f);if(onMessage)onMessage(tr(samples::isAudioFile(f)?"Not supported: use WAV, AIFF, FLAC or OGG.":"Not an audio file."),true);return false;}
    if(onLoad)onLoad(f);
    return true;
}
void SampleBrowser::preview(const juce::File& f){
    if(f==juce::File()||f.isDirectory())return;
    if(!samples::isSupportedAudio(f)){showFile(f);return;}
    previewFile=f;const int gen=++shared->previewGeneration;auto state=shared;juce::Component::SafePointer<SampleBrowser> safe(this);
    pool->addJob([state,gen,f,safe]{
        juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
        if(state->previewGeneration.load()!=gen)return;
        if(!reader||reader->sampleRate<=0||reader->lengthInSamples<=0){juce::MessageManager::callAsync([safe,gen]{if(safe!=nullptr&&safe->shared->previewGeneration.load()==gen){safe->info=tr("Unreadable audio file.");safe->infoError=true;safe->repaint();}});return;}
        // Up to 30 seconds: enough to judge a sample, light on memory.
        const int length=(int)std::min<juce::int64>(reader->lengthInSamples,(juce::int64)(reader->sampleRate*30));
        auto audio=std::make_shared<juce::AudioBuffer<float>>((int)std::min(2u,reader->numChannels),length);
        reader->read(audio.get(),0,length,0,true,true);const double rate=reader->sampleRate;
        juce::MessageManager::callAsync([safe,gen,audio,rate]{
            if(safe==nullptr||safe->shared->previewGeneration.load()!=gen)return;
            safe->previewDeadline=juce::Time::getMillisecondCounterHiRes()+audio->getNumSamples()/rate*1000.+750.;
            safe->engine.startPreview(std::move(*audio),rate);safe->startTimerHz(8);safe->updateButtons();
        });
    });
}
void SampleBrowser::stopPreview(){++shared->previewGeneration;if(engine.isPreviewing())engine.stopPreview();stopTimer();updateButtons();}
// Runs only while a preview plays: notices its end to reset the button.
void SampleBrowser::timerCallback(){if(!engine.isPreviewing()||juce::Time::getMillisecondCounterHiRes()>previewDeadline){if(engine.isPreviewing())engine.stopPreview();stopTimer();updateButtons();}}
void SampleBrowser::updateButtons(){
    const auto f=fileAt(fileList.getSelectedRow());const bool audio=samples::isSupportedAudio(f);
    backButton.setEnabled(history.canBack());forwardButton.setEnabled(history.canForward());
    upButton.setEnabled(folder!=juce::File()&&folder.getParentDirectory()!=folder);
    const bool playing=isTimerRunning();playButton.setIcon(playing?IconButton::Icon::stop:IconButton::Icon::play);playButton.setEnabled(playing||audio);
    loadButton.setEnabled(audio);
}
juce::StringArray SampleBrowser::crumbNames() const{juce::StringArray names;for(const auto& f:crumbs->chain)names.add(crumbs->nameOf(f));return names;}
bool SampleBrowser::keyPressed(const juce::KeyPress& key){
    if(key==juce::KeyPress::backspaceKey&&!search.hasKeyboardFocus(false)){up();return true;}
    if(key==juce::KeyPress(juce::KeyPress::leftKey,juce::ModifierKeys::altModifier,0)){back();return true;}
    if(key==juce::KeyPress(juce::KeyPress::rightKey,juce::ModifierKeys::altModifier,0)){forward();return true;}
    if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='F'){search.grabKeyboardFocus();return true;}
    return false;
}
bool SampleBrowser::waitForListing(int milliseconds){
    const auto end=juce::Time::getMillisecondCounterHiRes()+milliseconds;
    while(reading&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30); // the first info batch
    return !reading;
}
void SampleBrowser::resized(){
    auto r=getLocalBounds().reduced(gap,6);
    headerArea=r.removeFromTop(rowH);addButton.setBounds(headerArea.removeFromRight(110));
    r.removeFromTop(6);
    const int placeRows=std::clamp((int)places->items.size(),1,std::max(1,std::min(5,(r.getHeight()-260)/24)));
    placeList.setBounds(r.removeFromTop(placeRows*24+2));r.removeFromTop(gap);
    auto nav=r.removeFromTop(rowH);backButton.setBounds(nav.removeFromLeft(28));forwardButton.setBounds(nav.removeFromLeft(28));upButton.setBounds(nav.removeFromLeft(28));nav.removeFromLeft(6);crumbs->setBounds(nav);
    r.removeFromTop(6);search.setBounds(r.removeFromTop(rowH));r.removeFromTop(6);
    auto options=r.removeFromTop(rowH);orderButton.setBounds(options.removeFromRight(30));options.removeFromRight(6);
    filterBox.setBounds(options.removeFromLeft((options.getWidth()-6)/2));options.removeFromLeft(6);sortBox.setBounds(options);
    r.removeFromTop(6);
    auto controls=r.removeFromBottom(28);playButton.setBounds(controls.removeFromLeft(40));controls.removeFromLeft(6);autoButton.setBounds(controls.removeFromLeft(64));controls.removeFromLeft(6);loadButton.setBounds(controls);
    r.removeFromBottom(6);infoArea=r.removeFromBottom(40);r.removeFromBottom(6);
    fileList.setBounds(r);
}
void SampleBrowser::paint(juce::Graphics& g){
    g.setColour(Palette::panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),6);
    g.setColour(Palette::panelEdge);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),6,1);
    caption(g,tr("SAMPLES"),headerArea);
    g.setColour(Palette::field);g.fillRoundedRectangle(infoArea.toFloat(),4);
    auto text=infoArea.reduced(8,4);const auto selected=fileAt(fileList.getSelectedRow());
    g.setFont(FamilyTheme::font(12.f,true));g.setColour(Palette::text);
    g.drawFittedText(selected!=juce::File()?selected.getFileName():folder!=juce::File()?folder.getFileName():tr("SAMPLES"),text.removeFromTop(16),juce::Justification::centredLeft,1,.8f);
    g.setFont(FamilyTheme::font(10.5f));g.setColour(infoError?Palette::toms:Palette::dim);
    juce::String line=info;
    if(selected==juce::File()&&!reading&&available&&folder!=juce::File()){int count=0;for(int r:rows)count+=entries[(size_t)r].folder?0:1;line=juce::String(count)+" "+tr("files");}
    g.drawFittedText(line,text,juce::Justification::centredLeft,2,.8f);
}
void SampleBrowser::paintOverChildren(juce::Graphics& g){
    juce::String text;juce::Colour colour=Palette::dim;
    if(places->items.empty()&&folder==juce::File())text=tr("Add folders of samples: they stay in the list for every project.");
    else if(reading)text=tr("Reading the folder...");
    else if(!available){text=tr("Folder not available. Check the drive or edit it in the list.");colour=Palette::toms;}
    else if(rows.empty())text=search.isEmpty()?tr("No audio files here."):tr("Nothing matches the search.");
    if(text.isNotEmpty()){g.setColour(colour);g.setFont(FamilyTheme::font(12.f));g.drawFittedText(text,fileList.getBounds().reduced(16),juce::Justification::centred,4,.9f);}
}
bool SampleBrowser::verifyLayout() const{
    juce::Array<juce::Component*> parts{const_cast<juce::ListBox*>(&placeList),const_cast<juce::ListBox*>(&fileList),const_cast<juce::TextEditor*>(&search),const_cast<juce::ComboBox*>(&filterBox),const_cast<juce::ComboBox*>(&sortBox),
        const_cast<juce::TextButton*>(&addButton),const_cast<juce::TextButton*>(&orderButton),const_cast<juce::TextButton*>(&autoButton),const_cast<juce::TextButton*>(&loadButton),const_cast<juce::TextButton*>(&upButton),
        const_cast<IconButton*>(&backButton),const_cast<IconButton*>(&forwardButton),const_cast<IconButton*>(&playButton),crumbs.get()};
    for(int i=0;i<parts.size();++i){const auto b=parts[i]->getBounds();if(b.isEmpty()||!getLocalBounds().contains(b)||b.intersects(infoArea))return false;
        for(int j=i+1;j<parts.size();++j)if(b.intersects(parts[j]->getBounds()))return false;}
    return fileList.getHeight()>=100;
}
}
