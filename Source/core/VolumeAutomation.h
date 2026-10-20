// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace fga {
struct VolumePoint {
    double beat=0, value=1, curve=0; // linear gain; curve bends the outgoing segment [-1,1]
};
struct VolumeAutomation {
    bool enabled=false;
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
    void normalise(double length) {
        points.erase(std::remove_if(points.begin(),points.end(),[](const VolumePoint& p){return !std::isfinite(p.beat)||!std::isfinite(p.value)||!std::isfinite(p.curve);}),points.end());
        if(points.size()>16384)points.resize(16384);
        for(auto& p:points){p.beat=std::clamp(p.beat,0.,length);p.value=std::clamp(p.value,0.,2.);p.curve=std::clamp(p.curve,-1.,1.);}
        std::stable_sort(points.begin(),points.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
        std::vector<VolumePoint> unique;
        for(auto p:points){if(!unique.empty()&&p.beat-unique.back().beat<1./960.)unique.back()=p;else unique.push_back(p);}
        points=std::move(unique);
    }
};
}
