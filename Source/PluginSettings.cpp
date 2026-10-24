// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginSettings.h"
#include "Localisation.h"
namespace fga {
PluginSettings::PluginSettings(PluginCatalog& c):catalog(c){
    for(auto* control:std::initializer_list<juce::Component*>{&close,&choose,&clear,&rescan,&retry,&cancel,&title,&standardLabel,&customLabel,&folderPath,&status,&hint,&standard})addAndMakeVisible(control);
    for(auto* label:{&title,&standardLabel,&customLabel,&folderPath,&status,&hint}){label->setColour(juce::Label::textColourId,Palette::text);label->setFont(FamilyTheme::font(label==&title?17.f:12.f,label==&title));}
    folderPath.setMinimumHorizontalScale(.7f);
    standard.setMultiLine(true);standard.setReadOnly(true);standard.setFont(FamilyTheme::font(12));juce::StringArray paths;auto defaults=PluginCatalog::standardFolders();for(int i=0;i<defaults.getNumPaths();++i)paths.add(defaults[i].getFullPathName());standard.setText(paths.joinIntoString("\n"));
    close.onClick=[this]{setVisible(false);};
    choose.onClick=[this]{chooser=std::make_unique<juce::FileChooser>(tr("Choose plugin folder"));juce::Component::SafePointer<PluginSettings> safe(this);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& chosen){if(safe&&safe->catalog.setCustomFolder(chosen.getResult())){safe->showFolder();safe->catalog.scan();}});};
    clear.onClick=[this]{if(catalog.setCustomFolder({})){showFolder();catalog.scan();}};
    rescan.onClick=[this]{catalog.scan();};retry.onClick=[this]{catalog.scan(true);};
    cancel.onClick=[this]{catalog.cancel();};
    refreshText();setVisible(false);
}
void PluginSettings::showFolder(){const auto folder=catalog.customFolder();folderPath.setText(folder==juce::File()?tr("No custom folder"):folder.getFullPathName(),juce::dontSendNotification);}
void PluginSettings::show(){showFolder();refreshText();timerCallback();setVisible(true);toFront(false);startTimerHz(4);}
// The 4 Hz status poll runs only while the panel is open.
void PluginSettings::visibilityChanged(){if(!isVisible())stopTimer();}
void PluginSettings::refreshText(){
    title.setText(tr("PLUGIN SETTINGS"),juce::dontSendNotification);standardLabel.setText(tr("Standard folders"),juce::dontSendNotification);customLabel.setText(tr("Custom folder"),juce::dontSendNotification);
    close.setButtonText(tr("CLOSE"));choose.setButtonText(tr("CHOOSE FOLDER"));clear.setButtonText(tr("CLEAR"));rescan.setButtonText(tr("RESCAN"));retry.setButtonText(tr("RETRY FAILED"));cancel.setButtonText(tr("CANCEL"));
    hint.setText(tr("Automatic scan at every startup of the default VST3 folders and one custom folder, including subfolders. Unchanged plugins stay in the saved list."),juce::dontSendNotification);
    hint.setMinimumHorizontalScale(1);hint.setJustificationType(juce::Justification::topLeft);
    choose.setTooltip(tr("Choose one folder and scan its subfolders for VST3 instruments and effects."));clear.setTooltip(tr("Clear the custom folder. Installed plugins and project assignments are kept."));rescan.setTooltip(tr("Find new, updated and removed plugins. Unchanged VST3 plugins use the saved catalogue."));retry.setTooltip(tr("Retry plugins skipped after a failed or interrupted scan."));
    showFolder();
}
void PluginSettings::timerCallback(){
    if(!isVisible())return;const auto s=catalog.progressState();
    auto text=tr(s.scanning?"Scanning plugins...":s.checked<s.total?"Plugin scan stopped":"Plugin scan complete")+"  "+juce::String(s.count)+" "+tr("plugins");
    if(s.failed){text+="  · "+juce::String(s.failed)+" "+tr("skipped");}
    if(s.scanning){text+="  "+juce::String(s.checked)+" / "+juce::String(s.total);}
    status.setText(text,juce::dontSendNotification);status.setTooltip(s.current);
    const bool busy=s.scanning;for(auto* b:{&choose,&clear,&rescan,&retry})b->setEnabled(!busy);cancel.setEnabled(busy);
}
void PluginSettings::paint(juce::Graphics& g){g.fillAll(juce::Colours::black.withAlpha(.65f));g.setColour(Palette::panel);g.fillRoundedRectangle(card().toFloat(),8);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(card().toFloat(),8,1);}
void PluginSettings::resized(){auto r=card().reduced(28);auto top=r.removeFromTop(32);close.setBounds(top.removeFromRight(90));title.setBounds(top);r.removeFromTop(16);standardLabel.setBounds(r.removeFromTop(22));standard.setBounds(r.removeFromTop(124));r.removeFromTop(14);customLabel.setBounds(r.removeFromTop(22));folderPath.setBounds(r.removeFromTop(28));r.removeFromTop(8);auto row=r.removeFromTop(28);choose.setBounds(row.removeFromLeft(180));row.removeFromLeft(8);clear.setBounds(row.removeFromLeft(120));r.removeFromTop(12);row=r.removeFromTop(28);rescan.setBounds(row.removeFromLeft(150));row.removeFromLeft(8);retry.setBounds(row.removeFromLeft(165));row.removeFromLeft(8);cancel.setBounds(row.removeFromLeft(120));r.removeFromTop(12);status.setBounds(r.removeFromTop(32));hint.setBounds(r.removeFromTop(72));}
bool PluginSettings::verifyLayout()const{for(auto* c:std::initializer_list<const juce::Component*>{&close,&choose,&clear,&rescan,&retry,&cancel,&standard,&folderPath,&status,&hint})if(c->getBounds().isEmpty()||!card().contains(c->getBounds()))return false;return true;}
}
