// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "StudioSettings.h"
#include <cctype>
#include <cstdlib>
namespace fga {
inline bool eqHasGain(int type){return type==(int)EqFilterType::Bell||type==(int)EqFilterType::LowShelf||type==(int)EqFilterType::HighShelf;}
// Strict, locale-independent entry. A failed edit leaves both the EQ and Undo intact.
inline bool parseEqValue(std::string text,int parameter,double& value){
    text.erase(std::remove_if(text.begin(),text.end(),[](unsigned char c){return std::isspace(c)!=0;}),text.end());
    for(auto& c:text)c=(char)std::tolower((unsigned char)c);
    auto suffix=[&](const char* tail){const std::string end(tail);if(text.size()<end.size()||text.compare(text.size()-end.size(),end.size(),end)!=0)return false;text.resize(text.size()-end.size());return true;};
    double factor=1;
    if(parameter==0){if(suffix("khz")||suffix("k"))factor=1000;else suffix("hz");}
    else if(parameter==1)suffix("db");
    if(text.empty()||text.find_first_not_of("0123456789.,+-")!=std::string::npos)return false;
    if(text.find(',')!=std::string::npos){if(text.find('.')!=std::string::npos||std::count(text.begin(),text.end(),',')!=1)return false;std::replace(text.begin(),text.end(),',','.');}
    // Parse decimal without depending on the host/plugin process C locale.
    size_t at=0;double sign=1;if(text[at]=='+'||text[at]=='-'){if(text[at++]=='-')sign=-1;}
    bool digit=false,dot=false;double parsed=0,decimal=.1;
    for(;at<text.size();++at){const char c=text[at];if(c=='.'&&!dot){dot=true;continue;}if(c<'0'||c>'9')return false;digit=true;if(dot){parsed+=(c-'0')*decimal;decimal*=.1;}else parsed=parsed*10+(c-'0');}
    parsed*=sign*factor;const double lo=parameter==0?20:parameter==1?-24:.1,hi=parameter==0?20000:parameter==1?24:18;
    if(!digit||!std::isfinite(parsed)||parsed<lo||parsed>hi)return false;value=parsed;return true;
}
}
