// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
namespace fga {
struct VolumePoint {
    double beat=0, value=1, curve=0; // linear gain; curve bends the outgoing segment [-1,1]
};
struct VolumeAutomation {
    bool enabled=false;
    bool locked=false; // random automation keeps a locked lane as it is
    std::vector<VolumePoint> points;
    // Absolute musical time, independent of BPM. Hold the first/last value outside
    // the envelope. No MIDI CC is sent: this is post-instrument audio gain.
    double valueAt(double beat) const {
        if(!enabled||points.empty())return 1;
        auto end=std::upper_bound(points.begin(),points.end(),beat,[](double b,const VolumePoint& p){return b<p.beat;});
        if(end==points.begin())return end->value;
        const auto& a=*(end-1);
        if(end==points.end())return a.value;
        double t=std::clamp((beat-a.beat)/(end->beat-a.beat),0.,1.);
        t+=a.curve*t*(1.-t);
        return a.value+(end->value-a.value)*t;
    }
    bool active() const {return enabled&&!points.empty();}
    // True when the lane actually moves (not a flat line): shown in blue in the interface.
    bool moves() const {if(!active())return false;for(const auto& p:points)if(std::abs(p.value-points.front().value)>1e-6)return true;return false;}
    bool isDefault() const {return !enabled&&!locked&&points.empty();}
    // maxValue: 2 for a gain (volume, master), 1 for a pan or a plugin parameter (0..1).
    void normalise(double length,double maxValue=2.) {
        points.erase(std::remove_if(points.begin(),points.end(),[](const VolumePoint& p){return !std::isfinite(p.beat)||!std::isfinite(p.value)||!std::isfinite(p.curve);}),points.end());
        if(points.size()>16384)points.resize(16384);
        for(auto& p:points){p.beat=std::clamp(p.beat,0.,length);p.value=std::clamp(p.value,0.,maxValue);p.curve=std::clamp(p.curve,-1.,1.);}
        std::stable_sort(points.begin(),points.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
        std::vector<VolumePoint> unique;
        for(auto p:points){if(!unique.empty()&&p.beat-unique.back().beat<1./960.)unique.back()=p;else unique.push_back(p);}
        points=std::move(unique);
    }
};
// The same envelope drives the other automatable values. Values are 0..1:
// pan 0 = left, .5 = centre, 1 = right; a plugin parameter takes the value as it is.
inline double panFromAutomation(double value){return std::clamp(value*2.-1.,-1.,1.);}
inline double automationFromPan(double pan){return std::clamp((pan+1.)*.5,0.,1.);}
// Stable target: -1 = native sound, 0 = instrument, 1..8 = effect instance ID.
// The index is the plugin's own parameter index; the name is kept to show it and to find
// the parameter again if the plugin's list changes.
struct ParameterAutomation {
    int slot=0,index=-1;
    std::string name;
    std::string parameterId; // plugin-provided ID; index is resolved again when loading
    VolumeAutomation curve;
};
constexpr int maxParameterLanes=32;
constexpr int maxTrackEffects=8;
}
