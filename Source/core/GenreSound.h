// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Project.h"
namespace fga {
// Curated settings for the ORIGINAL family instruments. No samples, external
// synthesizers or extra sound engines. Values are defaults applied by genre.
struct GenreSound {
    int piano,bass,guitar,kit,voicing,strings;
    double pianoTone,bassTone,guitarTone,drumTone,gate,strum,density;
    double pianoGain,bassGain,stringsGain,guitarGain;
};
inline const GenreSound& genreSound(int genre){
    static constexpr GenreSound styles[]{
        {1,3,0,0,0,6, .68,.16,.48,.58,.48,.12,.57, .56,.86,.17,.20}, // piano house: grand / 909
        {0,0,1,2,1,3, .38,.28,.32,.42,.67,.25,.50, .60,.84,.20,.30}, // soulful: Rhodes / live kit
        {3,3,1,1,2,2, .24,.05,.24,.28,.42,.08,.38, .47,.92,.18,.16}, // deep: organ / 808
        {5,0,1,2,1,6, .57,.30,.42,.48,.72,.28,.56, .48,.82,.22,.19}, // gospel: Leslie / live kit
        {1,4,0,0,0,0, .72,.54,.65,.68,.18,.00,.62, .56,.80,.12,.22}, // jackin: dry piano chops / articulate electric bass / 909
        {2,3,0,0,0,5, .75,.20,.60,.66,.30,.08,.64, .53,.86,.12,.21}, // piano jack: M1 / 909
        {0,3,0,1,2,5, .49,.04,.52,.62,.24,.05,.58, .48,.98,.12,.23}, // UK: chopped Rhodes / 808 sub
        {4,3,3,2,4,2, .35,.10,.40,.48,.52,.24,.48, .40,.88,.17,.35}, // Afro: dark keys / nylon / percussion
        {4,4,2,2,6,9, .61,.52,.70,.60,.40,.16,.61, .42,.78,.31,.48}, // disco: Wurli / chorus guitar / strings
        {2,3,0,0,0,1, .83,.22,.62,.70,.50,.08,.60, .60,.87,.20,.15}, // 90s: M1 / 909
        {0,4,1,3,2,3, .28,.35,.28,.22,.78,.34,.42, .58,.78,.18,.28}, // neo soul: dark Rhodes / lo-fi / jazz
        {6,0,3,3,3,3, .24,.18,.22,.18,1.1,.40,.30, .44,.73,.23,.24}, // chill: warm pad / nylon / lo-fi
        {2,3,0,0,8,4, .80,.18,.72,.72,.20,.03,.65, .56,.90,.12,.18}  // dance stabs: upper triads / 909
    };
    return styles[std::clamp(genre,0,12)];
}
inline void applyGenreSound(Project& p){
    const auto& a=genreSound(p.settings.genre);
    static constexpr int extensions[]{2,2,2,2,1,0,1,1,1,0,3,2,0};
    p.settings.extension=extensions[p.settings.genre];
    p.settings.voicing=a.voicing;p.settings.stringsMode=a.strings;
    p.settings.guitarLead=p.settings.genre==8;
    for(int ti=0;ti<trackCount;++ti){auto& t=p.tracks[(size_t)ti];if(t.locked)continue;auto& e=t.engine;
        e.fillEvery=isHarmonicTrack(ti)?16:8;e.variationEvery=8;e.afterFill=true;
        if(ti==0){e.gate=a.gate;e.strum=a.strum;e.centre=p.settings.genre==11?67:64;e.octaveTop=p.settings.genre==9;e.density=a.density;e.humanize=p.settings.genre==10?.32:.14;}
        if(ti==1){e.gate=.55;e.bassFeel=(p.settings.genre==2||p.settings.genre==6||p.settings.genre==7||p.settings.genre==12)?2:0;e.humanize=.05;e.realism=true;}
        if(p.settings.genre==4){
            // The bass is a short funky line, not a held low organ chord.
            if(ti==1){e.gate=.30;e.humanize=.08;e.bassFeel=1;}
        }
        if(ti==2)e.density=.3;
        if(ti==3){e.strum=p.settings.genre==8?.12:.22;e.letRing=p.settings.genre==11;e.roomForBand=true;e.density=p.settings.genre==8?.68:p.settings.genre==4?.22:.35;if(p.settings.genre==4)e.strum=.06;}
        if(ti==leadTrack){e.strum=.15;e.density=.42;e.humanize=.14;e.roomForBand=false;e.fillEvery=8;}
        // Keep user instruments and their mix/routing. Genre changes configure
        // their MIDI vocabulary, while the original guide palette is curated.
        if(t.instrument!="internal")continue;
        t.pan=0;e.chorus=0;e.pump=0;e.reverb=0;
        if(ti==0){t.sound=a.piano;t.gain=a.pianoGain;e.tone=a.pianoTone;e.reverb=p.settings.genre==11?.24:p.settings.genre==4?.025:.10;e.chorus=p.settings.genre==1||p.settings.genre==10?.12:0;}
        else if(ti==1){t.sound=a.bass;t.gain=a.bassGain;e.tone=a.bassTone;}
        else if(ti==2){t.gain=a.stringsGain;e.tone=p.settings.genre==8?.56:.25;e.reverb=.18;}
        else if(ti==3){t.sound=a.guitar;t.gain=a.guitarGain;e.tone=a.guitarTone;e.reverb=.07;t.pan=.14;}
        else if(ti==leadTrack){t.sound=a.guitar;t.gain=.22;e.tone=a.guitarTone;e.reverb=.12;}
        else {t.sound=a.kit;e.tone=a.drumTone;const int lane=ti-4;
            // GuideDrums already scales each lane (kick .95, snare .6, hats .28).
            // Keep that balance audible instead of attenuating the upper kit twice.
            static constexpr double levels[]{.60,.90,.85,.90,.85,.85,.75,.90,.75,.72,.72,.72,.72};
            t.gain=levels[lane];if(p.settings.genre==7&&(lane==7||lane==9))t.gain*=1.35;
            if(lane>=4)t.pan=lane%2==0?-.15:.15;
            if(lane==1||lane==2||lane==12)e.reverb=.05;
        }
    }
}
}
