// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace fga {class MainComponent;class ProjectAssistantPanel;
class MixAssistantWindow:public juce::DocumentWindow {
public:
 explicit MixAssistantWindow(MainComponent&);~MixAssistantWindow()override;
 void open();void closeButtonPressed()override;
 bool verify(const juce::File&);
private:class Panel;Panel* panel;ProjectAssistantPanel* projectPanel;juce::TabbedComponent* tabs;MainComponent& owner;
};}
