// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/EqEditing.h"
#include "../Source/core/EqResponse.h"
#include <iostream>
#include <stdexcept>
void expect(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{using namespace fga;double value=0;
    for(const auto& item:std::vector<std::pair<std::string,double>>{{"1k",1000},{"1,5 kHz",1500},{"20000 Hz",20000},{"20",20},{"+2.5kHz",2500}}){expect(parseEqValue(item.first,0,value)&&value==item.second,"frequency entry lost its unit or decimal separator");}
    for(const char* bad:{"", "noise","1kOops","nan","inf","1,2.3","2..5","1e3","19","20001","-1k","+","--1"})expect(!parseEqValue(bad,0,value),"invalid frequency was accepted");
    expect(parseEqValue("-6,5 dB",1,value)&&value==-6.5,"gain entry failed");expect(!parseEqValue("25dB",1,value),"out-of-range gain accepted");expect(!parseEqValue("0",2,value)&&parseEqValue("0,707",2,value),"Q range/decimal invalid");
    EqSettings eq;eq.enabled=true;eq.bands={{true,0,250,3,1},{true,0,900,-5,2},{true,0,3000,2,.8},{true,5,7000,0,4}};eq.syncLegacyFromBands();const auto original=eq;
    expect(eq.setLegacyControl(2,4),"compact mid edit rejected");expect(eq.bands[0].gain==4,"compact mid did not select the first bell");for(size_t i=1;i<4;++i)expect(eq.bands[i]==original.bands[i],"compact controls overwrote another band's shape/frequency/gain");
    eq.flatten();for(double rate:{16000.,44100.,48000.,96000.})for(double hz:{30.,80.,1000.,6000.}){double db=0;for(const auto& band:eq.bands)db+=eqCoefficients(band,rate).magnitudeDb(hz,rate);expect(std::abs(db)<1.e-7,"reset/flat retained an audible cut or notch");}
    EqSettings legacy;legacy.highPass=300;legacy.low=6;legacy.flatten();for(const auto& b:legacy.bands)expect(!b.enabled||b.gain==0,"legacy flatten is not neutral");
    EqSettings full;full.bands.assign(16,{true,0,1000,0,1});expect(!full.setLegacyControl(0,200)&&full.bands.size()==16,"compact edit exceeded the band budget");
    for(double rate:{44100.,48000.,96000.}){EqBand bell{true,0,1000,6,1};expect(std::abs(eqCoefficients(bell,rate).magnitudeDb(1000,rate)-6)<1.e-7,"bell gain or sample-rate response incorrect");bell.enabled=false;expect(std::abs(eqCoefficients(bell,rate).magnitudeDb(1000,rate))<1.e-7,"bypassed filter changed response");}
    std::cout<<"PASS strict EQ values, compact-band isolation, exact flatten, band budget and frequency response\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
