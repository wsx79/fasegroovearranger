// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginSettings.h"
#include "Localisation.h"
namespace fga {
PluginSettings::PluginSettings(PluginCatalog& c):catalog(c){
    for(auto* control:std::initializer_list<juce::Component*>{&close,&add,&remove,&rescan,&retry,&cancel,&title,&standardLabel,&customLabel,&status,&hint,&standard,&list})addAndMakeVisible(control);
    for(auto* label:{&title,&standardLabel,&customLabel,&status,&hint}){label->setColour(juce::Label::textColourId,Palette::text);label->setFont(FamilyTheme::font(label==&title?17.f:12.f,label==&title));}
    standard.setMultiLine(true);standard.setReadOnly(true);standard.setFont(FamilyTheme::font(12));juce::StringArray paths;auto defaults=PluginCatalog::standardFolders();for(int i=0;i<defaults.getNumPaths();++i)paths.add(defaults[i].getFullPathName());standard.setText(paths.joinIntoString("\n"));
    list.setModel(this);list.setRowHeight(28);list.setColour(juce::ListBox::backgroundColourId,Palette::field);
    close.onClick=[this]{setVisible(false);};
    add.onClick=[this]{chooser=std::make_unique<juce::FileChooser>(tr("Add plugin folder"));juce::Component::SafePointer<PluginSettings> safe(this);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& chosen){if(safe&&safe->catalog.addFolder(chosen.getResult())){safe->folders=safe->catalog.folders();safe->list.updateContent();safe->catalog.scan();}});};
    remove.onClick=[this]{if(catalog.removeFolder(list.getSelectedRow())){folders=catalog.folders();list.updateContent();catalog.scan();}};
    rescan.onClick=[this]{catalog.scan();};retry.onClick=[this]{catalog.scan(true);};
    cancel.onClick=[this]{catalog.cancel();};
    refreshText();startTimerHz(4);setVisible(false);
}
void PluginSettings::show(){folders=catalog.folders();list.updateContent();refreshText();timerCallback();setVisible(true);toFront(false);}
void PluginSettings::refreshText(){
    title.setText(tr("PLUGIN SETTINGS"),juce::dontSendNotification);standardLabel.setText(tr("Standard folders"),juce::dontSendNotification);customLabel.setText(tr("Custom folders"),juce::dontSendNotification);
    close.setButtonText(tr("CLOSE"));add.setButtonText(tr("ADD FOLDER"));remove.setButtonText(tr("REMOVE"));rescan.setButtonText(tr("SCAN NOW"));retry.setButtonText(tr("RETRY FAILED"));cancel.setButtonText(tr("CANCEL"));
    hint.setText(tr("Automatic scan at every startup, including subfolders. Cached instruments are available immediately."),juce::dontSendNotification);
    hint.setMinimumHorizontalScale(1);hint.setJustificationType(juce::Justification::topLeft);
    add.setTooltip(tr("Add a folder and scan its subfolders for VST3 instruments."));remove.setTooltip(tr("Remove this search folder. Installed plugins and project assignments are kept."));rescan.setTooltip(tr("Find new, updated and removed plugins. Unchanged VST3 plugins use the saved catalogue."));retry.setTooltip(tr("Retry plugins skipped after a failed or interrupted scan."));
}
void PluginSettings::timerCallback(){
    if(!isVisible())return;const auto s=catalog.state();
    auto text=tr(s.scanning?"Scanning plugins...":s.checked<s.total?"Plugin scan stopped":"Plugin scan complete")+"  "+juce::String(s.plugins.size())+" "+tr("instruments")+"  "+juce::String(s.checked)+" / "+juce::String(s.total);
    if(s.failed)text+="  · "+juce::String(s.failed)+" "+tr("skipped");status.setText(text,juce::dontSendNotification);status.setTooltip(s.current);
    for(auto* b:{&add,&remove,&rescan,&retry})b->setEnabled(!s.scanning);list.setEnabled(!s.scanning);cancel.setEnabled(s.scanning);
}
void PluginSettings::paint(juce::Graphics& g){g.fillAll(juce::Colours::black.withAlpha(.65f));g.setColour(Palette::panel);g.fillRoundedRectangle(card().toFloat(),8);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(card().toFloat(),8,1);}
void PluginSettings::resized(){auto r=card().reduced(28);auto top=r.removeFromTop(32);close.setBounds(top.removeFromRight(90));title.setBounds(top);r.removeFromTop(16);standardLabel.setBounds(r.removeFromTop(22));standard.setBounds(r.removeFromTop(124));r.removeFromTop(14);customLabel.setBounds(r.removeFromTop(22));list.setBounds(r.removeFromTop(140));r.removeFromTop(8);auto row=r.removeFromTop(28);add.setBounds(row.removeFromLeft(165));row.removeFromLeft(8);remove.setBounds(row.removeFromLeft(130));r.removeFromTop(12);row=r.removeFromTop(28);rescan.setBounds(row.removeFromLeft(165));row.removeFromLeft(8);retry.setBounds(row.removeFromLeft(165));row.removeFromLeft(8);cancel.setBounds(row.removeFromLeft(120));r.removeFromTop(12);status.setBounds(r.removeFromTop(32));hint.setBounds(r);}
void PluginSettings::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected){if(row<0||row>=folders.size())return;if(selected){g.setColour(Palette::control);g.fillAll();}g.setColour(Palette::text);g.setFont(FamilyTheme::font(12));g.drawFittedText(folders[row],8,0,width-16,height,juce::Justification::centredLeft,1);}
bool PluginSettings::verifyLayout()const{for(auto* c:std::initializer_list<const juce::Component*>{&close,&add,&remove,&rescan,&retry,&cancel,&standard,&list,&status,&hint})if(c->getBounds().isEmpty()||!card().contains(c->getBounds()))return false;return true;}
}
