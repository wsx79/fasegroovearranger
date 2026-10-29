// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN 1
 #endif
 #include <dwmapi.h>
 #undef small
 #pragma comment(lib, "dwmapi.lib")
#endif
namespace fga {
#if JUCE_WINDOWS
// The default Windows title bar stays light even for a dark app. Attribute 20 is
// the Windows 10 2004+ switch, 19 the earlier one.
inline void applyDarkTitleBar(juce::Component& component){
    if(auto* peer=component.getPeer())if(void* handle=peer->getNativeHandle()){
        const BOOL dark=TRUE;
        if(FAILED(::DwmSetWindowAttribute(static_cast<HWND>(handle),20,&dark,sizeof(dark))))
            ::DwmSetWindowAttribute(static_cast<HWND>(handle),19,&dark,sizeof(dark));
    }
}
#else
inline void applyDarkTitleBar(juce::Component&){}
#endif
// Applies the attribute once per native window, including tool windows created
// later. Cheap enough for the UI timer: a handful of pointers and a set lookup.
inline void applyDarkTitleBarToAll(){
   #if JUCE_WINDOWS
    for(int i=0;i<juce::TopLevelWindow::getNumTopLevelWindows();++i)
        if(auto* window=juce::TopLevelWindow::getTopLevelWindow(i))
            if(dynamic_cast<juce::DocumentWindow*>(window)!=nullptr)
                applyDarkTitleBar(*window);
   #endif
}
}
