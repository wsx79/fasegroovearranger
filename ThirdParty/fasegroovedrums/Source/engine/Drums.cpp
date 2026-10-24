// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Drums.h"

#include <algorithm>

namespace fgd
{

const char* laneName (int lane) noexcept
{
    static const char* names[] { "Kick", "Snare", "Clap", "Rim", "Closed hat", "Open hat", "Ride",
                                 "Shaker", "Tambourine", "Conga", "Cowbell", "Tom", "Crash" };
    return lane >= 0 && lane < numLanes ? names[lane] : "";
}

const char* laneShortName (int lane) noexcept
{
    static const char* names[] { "KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "RIDE",
                                 "SHAKER", "TAMB", "CONGA", "BELL", "TOM", "CRASH" };
    return lane >= 0 && lane < numLanes ? names[lane] : "";
}

int defaultNote (int lane) noexcept
{
    static const int notes[] { 36, 38, 39, 37, 42, 46, 51, 82, 54, 63, 56, 45, 49 };
    return lane >= 0 && lane < numLanes ? notes[lane] : 36;
}

const char* noteMapName (int map) noexcept
{
    static const char* names[] { "General MIDI", "Drum rack (16 pads, C1-D#2)", "MPC pads (bank A)", "One note per drum from C1" };
    return map >= 0 && map < numNoteMaps ? names[map] : "";
}

int noteMapNote (int map, int lane) noexcept
{
    // Lanes: kick, snare, clap, rim, closed hat, open hat, ride, shaker, tambourine, conga, cowbell, tom, crash.
    static const int rack[] { 36, 38, 39, 37, 42, 46, 51, 44, 40, 47, 48, 43, 49 };   // every drum on one of the 16 pads
    static const int mpc[]  { 36, 38, 40, 37, 42, 46, 51, 82, 44, 48, 47, 45, 49 };   // the MPC's default pads
    if (lane < 0 || lane >= numLanes)
        return 36;
    switch (map)
    {
        case 1:  return rack[lane];
        case 2:  return mpc[lane];
        case 3:  return 36 + lane;
        default: return defaultNote (lane);
    }
}

int laneForGeneralMidiNote (int note) noexcept
{
    switch (note)
    {
        case 35: case 36:                     return (int) Lane::kick;
        case 38: case 40:                     return (int) Lane::snare;
        case 39:                              return (int) Lane::clap;
        case 37:                              return (int) Lane::rim;
        case 42: case 44:                     return (int) Lane::closedHat;
        case 46:                              return (int) Lane::openHat;
        case 51: case 53: case 59:            return (int) Lane::ride;
        case 69: case 70: case 82:            return (int) Lane::shaker;
        case 54:                              return (int) Lane::tambourine;
        case 60: case 61: case 62: case 63: case 64:
                                              return (int) Lane::conga;
        case 56:                              return (int) Lane::cowbell;
        case 41: case 43: case 45: case 47: case 48: case 50:
                                              return (int) Lane::tom;
        case 49: case 52: case 55: case 57:   return (int) Lane::crash;
        default:                              return -1;
    }
}

//==============================================================================
void normalisePattern (DrumPattern& p, int minimumLength)
{
    size_t length = (size_t) std::max (1, minimumLength);
    for (const auto& line : p.lanes)
        length = std::max (length, line.size());

    for (auto& line : p.lanes)
    {
        for (auto& c : line)
            if (! isHit (c))
                c = '.';

        if (line.empty())
            line.assign (length, '.');

        // A one-bar line in a two-bar groove plays twice.
        const std::string base = line;
        while (line.size() < length)
            line += base.substr (0, std::min (base.size(), length - line.size()));
    }
}

void fitPattern (DrumPattern& p, int length)
{
    length = std::max (1, length);
    for (auto& line : p.lanes)
    {
        for (auto& c : line)
            if (! isHit (c))
                c = '.';
        if (line.empty())
            line.assign ((size_t) length, '.');
        const std::string base = line;
        while ((int) line.size() < length)
            line += base.substr (0, std::min (base.size(), (size_t) length - line.size()));
        line.resize ((size_t) length);
    }
}

namespace
{
    std::string compact (const char* s)
    {
        std::string out;
        for (; *s != 0; ++s)
            if (*s != ' ')
                out += *s;
        return out;
    }

    struct Line { Lane lane; const char* steps; };

    DrumPattern groove (const char* name, const char* category, std::initializer_list<Line> lines)
    {
        DrumPattern p;
        p.name = name;
        p.category = category;
        for (const auto& l : lines)
            p[l.lane] = compact (l.steps);
        normalisePattern (p);
        return p;
    }

    constexpr auto K = Lane::kick, S = Lane::snare, C = Lane::clap, R = Lane::rim, CH = Lane::closedHat,
                   OH = Lane::openHat, RD = Lane::ride, SH = Lane::shaker, T = Lane::tambourine,
                   CG = Lane::conga, CB = Lane::cowbell, TM = Lane::tom, CR = Lane::crash;

    constexpr const char* fourFloor = "X... X... X... X...";
    constexpr const char* backbeat  = ".... X... .... X...";
    constexpr const char* offbeats  = "..X. ..X. ..X. ..X.";
    constexpr const char* crashBar1 = "X... .... .... .... .... .... .... ....";
}

const std::vector<DrumPattern>& drumPatterns()
{
    static const std::vector<DrumPattern> patterns {
        // Every house groove is built the same way: the kick on the beats, the clap on 2 and 4,
        // ONE hat line (mostly on the offbeat; the accent groove makes the other 16ths soft),
        // maybe an open hat accent, and at most one or two sparse percussion lines.

        // ---- Piano House: the offbeat open hat, a shaker, claps on 2 & 4 ----
        groove ("Piano House Classic", "Piano House",
                { { K, fourFloor }, { C, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { CH, "x... x... x... x..." },
                  { SH, "..xx ..x. ..xx ..x." } }),
        groove ("Piano House Shuffle", "Piano House",
                { { K, fourFloor }, { C, ".... X... .... X..x" }, { CH, "..x. ..x. ..x. ..xx" }, { OH, ".... .... .... .x.." },
                  { SH, "x..x ..x. ..x. x..." } }),
        groove ("Sunset Piano", "Piano House",
                { { K, fourFloor }, { C, backbeat }, { CH, "..x. ..x. ..xx ..x." }, { OH, ".... .... .... .... .... .... .... .x.." },
                  { CG, ".... .... .... .... .... .... .xxx x..." }, { SH, "..xx ..x. ..xx ..x." } }),
        groove ("Piano Stab Groove", "Piano House",
                { { K, fourFloor }, { C, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { T, ".... x... .... x..." } }),
        groove ("Ibiza Piano", "Piano House",
                { { K, fourFloor }, { C, ".... Xg.. .... Xx.." }, { OH, "..x. ..x. ..x. ..x." }, { SH, "g.gx g.xx ..xx g.xx" },
                  { CG, ".... .... .... ...x" } }),

        // ---- Soulful: almost straight, a 16th shaker or tambourine with a life of its own, congas ----
        groove ("Soulful Sunday", "Soulful",
                { { K, fourFloor }, { C, backbeat }, { CH, "..x. ..x. ..x. ..x." }, { SH, "xxxx xxxx xxxx xxxx" },
                  { CG, "..xx g... ..xx g..g" } }),
        groove ("Soulful Shaker", "Soulful",
                { { K, fourFloor }, { C, ".... X... .... X..x" }, { CH, "x.x. x.x. x.x. x.x." }, { SH, "g.xg xgxg g.xg xgxg" },
                  { OH, ".... .... .... .x.." } }),
        groove ("Soulful Tamb & Congas", "Soulful",
                { { K, fourFloor }, { C, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { T, "..xx ..x. ..xx ..x." },
                  { CG, ".... ..x. .... x.x." } }),
        groove ("Soulful Keys Groove", "Soulful",
                { { K, fourFloor }, { C, backbeat }, { S, ".... .... .... ..g. .... .... .... ..g." }, { CH, "..x. ..xx ..x. ..x." },
                  { SH, "xxxx xxxx xxxx xxxx" }, { TM, ".... .... .... ...x" } }),
        groove ("Soulful Tambourine", "Soulful",
                { { K, fourFloor }, { C, backbeat }, { CH, "..x. ..x. ..x. ..xx" }, { T, "xxxx xxxx xxxx xxxx" },
                  { CG, "x... .... .... ...." } }),

        // ---- Deep: soft kick, the rim instead of (or with) the clap, sparse and swung ----
        groove ("Deep Rim Shot", "Deep",
                { { K, "x... x... x... x..." }, { C, ".... x... .... x..." }, { CH, "..x. ..x. ..x. ..x." },
                  { R, "..x. .x.. .... ...." }, { SH, "..xx ..x. ..xx ..x." } }),
        groove ("Deep Three-Step Rim", "Deep",
                { { K, "x... x... x... x..." }, { C, ".... x... .... x..." }, { CH, "..x. ..xg ..x. ..xg" },
                  { R, "x..x ..x. x..x ..x." }, { TM, ".... .... .... ..xg" } }),
        groove ("Deep Night", "Deep",
                { { K, "x... x... x... x..." }, { C, ".... x... .... x..x" }, { CH, "x.x. x.x. x.x. x.x." },
                  { OH, ".... .... .... .x.." }, { SH, "g.gx g.xx ..xx g.xx" } }),
        groove ("Deep Shuffle", "Deep",
                { { K, "x... x... x... x... x... x... x... x..g" }, { C, ".... x... .... x..." }, { CH, ".xxx .xxx .xxx .xxx" },
                  { R, ".... .... .... ...x" } }),
        groove ("Dub Deep", "Deep",
                { { K, "x... x... x... x..." }, { R, ".... x... .... x..." }, { CH, "..x. ..x. ..x. ..x." },
                  { TM, ".... .... ..xx ...g" }, { SH, "..xx ..x. ..xx ..x." } }),

        // ---- Gospel: claps and snare together, the tambourine leads ----
        groove ("Gospel Tambourine", "Gospel",
                { { K, fourFloor }, { C, backbeat }, { S, ".... x... .... x..." }, { OH, "..x. ..x. ..x. ..x." },
                  { T, "xxxx xxxx xxxx xxxx" } }),
        groove ("Gospel Shout", "Gospel",
                { { K, fourFloor }, { C, ".... X..x .... X..." }, { S, ".... x... ..g. x..." }, { CH, "x.x. x.x. x.x. x.x." },
                  { T, "..xx ..x. ..xx ..x." } }),
        groove ("Praise Break", "Gospel",
                { { K, fourFloor }, { C, backbeat }, { S, ".... X..g .... X.gg" }, { OH, "..x. ..x. ..x. ..x." },
                  { T, ".... x... .... x..." } }),
        groove ("Sunday Choir", "Gospel",
                { { K, fourFloor }, { C, backbeat }, { CH, "..x. ..x. ..x. ..xx" }, { T, "x..x ..x. ..x. x..." },
                  { CG, ".... .... .... ...x" } }),
        groove ("Gospel House Claps", "Gospel",
                { { K, fourFloor }, { C, ".... Xg.. .... Xx.." }, { S, ".... x... .... x..." }, { CH, "..x. x.x. ..x. x.x." },
                  { T, "xxxx xxxx xxxx xxxx" }, { OH, ".... .... .... .x.." } }),

        // ---- Jackin': Chicago. The offbeat hat, the clap and its pick-ups, a single cowbell or
        //      rim, an open hat on the last beat; the swing does the jacking. Under the clap a 909
        //      snare: soft on the beats, loud swung pick-ups and a turnaround every other bar ----
        groove ("Chicago 909", "Jackin'",
                { { K, fourFloor }, { C, backbeat }, { CH, "..x. ..xg ..x. ..x. ..x. ..xg ..x. .gx." }, { OH, ".... .... .... .x.." },
                  { CB, ".... .... .... ...x" }, { SH, "..xx ..x. ..xx ..x." },
                  { S, ".... x... .... x... .... x... ..g. x..x" } }),
        groove ("Jack Your Body", "Jackin'",
                { { K, "X... X... X... X... X... X... X... X..g" }, { C, ".... X..x .... X..." }, { CH, "x.x. x.x. x.x. x.x." }, { OH, ".x.. .... .... ...." }, { SH, "..xx ..x. ..xx ..x." },
                  { S, "g... x... g... x... g... x... g... x.xx" } }),
        groove ("Jackin' Shuffle", "Jackin'",
                { { K, fourFloor }, { C, backbeat }, { CH, "..x. .gx. ..x. .gxx" }, { R, "...x ...x ...x ...x" },
                  { S, ".... x..x .... x... .... x..x .... x..." } }),
        groove ("Ghetto Jack", "Jackin'",
                { { K, "X... X... X... X.X." }, { C, ".... Xg.. .... Xx.." }, { CH, "..x. ..xx ..x. ..x." },
                  { TM, "xx.x x.xx .... ...." }, { S, ".... x..x .... x... .... x..x ..x. x.xx" } }),
        groove ("Jack Cowbell", "Jackin'",
                { { K, "X... X... X... X... X... X..g X... X..." }, { C, backbeat }, { CH, "..x. x.xg ..x. x.x." }, { CB, "x..x ..x. ..x. x..." }, { SH, "g.gx g.xx ..xx g.xx" } }),
        groove ("Percolator Jack", "Jackin'",
                { { K, fourFloor }, { C, backbeat }, { CH, "g.x. g.x. g.x. g.xg" }, { CG, ".... .... .xxx x..." },
                  { TM, ".... .x.. .... ...." }, { OH, ".... .... .... ...x" } }),
        groove ("Jackin' Claps", "Jackin'",
                { { K, fourFloor }, { C, ".... X... .... X..x .... X... .... X.x." }, { CH, "..xx ..x. ..x. ..x." }, { SH, "..xx ..x. ..xx ..x." },
                  { S, "...g .... ...g .... ...g .... ..g. ...x" } }),
        groove ("Warehouse Jack", "Jackin'",
                { { K, "X... X... X... X... X... X... X... X.x." }, { C, backbeat }, { RD, "x... x... xx.. x..." }, { CH, "..x. ..x. ..xg ..x." }, { SH, "..x. ..xg ..x. .gx." } }),
        groove ("Raw Jack Claps", "Jackin'",
                { { K, fourFloor }, { C, ".... X..x .... X..." }, { S, ".... .... ..x. ...." }, { CH, "..x. ..x. ..x. ..xx" },
                  { OH, ".... .... .... .x.." }, { SH, "..xx ..x. ..xx ..x." } }),
        groove ("Chicago Layers", "Jackin'",
                { { K, fourFloor }, { C, backbeat }, { CH, "x.xg x.x. x.xg x.xx" }, { T, ".... x... .... x..." },
                  { OH, ".... .... .... .x.." } }),
        groove ("Jack Hybrid", "Jackin'",
                { { K, fourFloor }, { C, ".g.. X... g... X... ..g. X... g.g. X..." }, { S, "g... x... g... x..x g... x... g... x.xx" },
                  { CH, "..x. ..x. ..x. ..xx" }, { OH, ".... .... .... .x.." } }),
        groove ("Jack Snare Turnaround", "Jackin'",
                { { K, fourFloor }, { C, backbeat }, { S, ".... x... .... x..x .... x... ..x. XgXX" }, { CH, "..x. ..x. ..x. ..x. ..x. ..x. ..x. .x.." },
                  { R, ".... .... .... ...x" }, { T, ".... x... .... x..." } }),
        groove ("Rim Jack", "Jackin'",
                { { K, "X... X... X... X..g X... X... X... X..." }, { C, backbeat }, { CH, "..xg ..x. ..xg ..x." }, { R, "..xx g... ..xx ...g" }, { SH, "xgx. xgxx xgx. xgxx" } }),

        // ---- Jackin' Piano: the jack bounce under piano stabs ----
        groove ("Jackin' Piano Bounce", "Jackin' Piano",
                { { K, fourFloor }, { C, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { CH, "x... x... x... x..." },
                  { SH, "x..x ..x. ..x. x..." } }),
        groove ("Swing Jack Piano", "Jackin' Piano",
                { { K, fourFloor }, { C, ".... X..x .... X..." }, { CH, "..x. ..x. ..x. ..xx" }, { OH, ".... .... .... .x.." }, { SH, "..xx ..xx ..xg ..xx" } }),
        groove ("Jack Stab Beat", "Jackin' Piano",
                { { K, "X... X... X... X.X." }, { C, backbeat }, { CH, "..x. x.x. ..x. x.x." }, { R, ".... .... .... ...x" }, { SH, "g.xg xgxg g.xg xgxg" } }),
        groove ("Piano Jack Groove", "Jackin' Piano",
                { { K, fourFloor }, { C, ".... Xg.. .... Xx.." }, { OH, "..x. ..x. ..x. ..x." }, { CG, "x... .... .... ...." }, { SH, "..xx ..x. ..xx ..x." } }),
        groove ("Bouncy Jack Keys", "Jackin' Piano",
                { { K, fourFloor }, { C, backbeat }, { CH, "..xx ..x. ..x. ..x." }, { T, ".... x... .... x..." } }),

        // ---- Garage & UK: two-step kicks, shuffled hats, rims ----
        groove ("Two-Step", "Garage & UK",
                { { K, "X... .... ..x. .... X... .... ..x. ...x" }, { S, backbeat }, { CH, "x.xx x.xx x.xx x.xx" },
                  { R, ".x.. ...x .x.. ..x." } }),
        groove ("2-Step Rim Shuffle", "Garage & UK",
                { { K, "X... .... ..x. .... X... .... ..x. ...x" }, { S, ".... X... ...g X... .... X... ...g X..." },
                  { CH, "x.xx x.xx x.xx x.xx" }, { R, "...x ..x. ...x ..x." } }),
        groove ("Speed Garage", "Garage & UK",
                { { K, fourFloor }, { S, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { CH, "x..x x..x x..x x..x" } }),
        groove ("UK Shuffle", "Garage & UK",
                { { K, "X... ...x ..x. ...." }, { S, backbeat }, { CH, "..xx ..xx ..xx ..xx" }, { R, "...x ..x. ...x ..x." } }),
        groove ("Garage Skip", "Garage & UK",
                { { K, "X... .... ..X. ...x" }, { S, ".... X... .... X..g" }, { CH, "x.xx x.x. x.xx x.x." },
                  { OH, ".... .... .... ..x." } }),

        // ---- Afro & Latin: the clave, shakers, toms and congas talking ----
        groove ("Afro House", "Afro & Latin",
                { { K, fourFloor }, { R, "x... ..x. .... x... .... x... x... ...." }, { SH, "xxxx xxxx xxxx xxxx" },
                  { TM, "..x. .xx. .... ...." }, { CG, "..xx g... ..xx g..g" } }),
        groove ("Afro Son Clave", "Afro & Latin",
                { { K, fourFloor }, { C, ".... x... .... x..." }, { R, "x... ..x. .... x... .... x... x... ...." },
                  { SH, "..xx ..x. ..xx ..x." }, { CG, ".... ..x. .... x.x." } }),
        groove ("Afro Toms", "Afro & Latin",
                { { K, fourFloor }, { C, ".... x... .... x..." }, { SH, "g.gx g.xx ..xx g.xx" }, { TM, "xx.x x.xx .... ...." },
                  { OH, "..x. ..x. ..x. ..x." } }),
        groove ("Latin House", "Afro & Latin",
                { { K, fourFloor }, { C, backbeat }, { CB, "x..x ..x. ..x. x..." }, { CG, ".... ..x. .... x.x." },
                  { CH, "..x. ..x. ..x. ..x." }, { SH, "g.xg xgxg g.xg xgxg" } }),
        groove ("Tribal House", "Afro & Latin",
                { { K, fourFloor }, { TM, "..x. .xx. ..xx ...g" }, { CG, ".... .... .xxx x..." }, { SH, "..xx ..x. ..xx ..x." },
                  { R, "x... ..x. .... x... .... x... x... ...." } }),

        // ---- Funky & Disco: the open hat on the offbeat with closed 16ths around it, the snare ----
        groove ("Disco Classic", "Funky & Disco",
                { { K, fourFloor }, { S, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { CH, "xx.x .x.x xx.x .x.x" },
                  { T, ".... x... .... x..." } }),
        groove ("Disco Sixteenths", "Funky & Disco",
                { { K, fourFloor }, { S, backbeat }, { CH, "xxxx xxxx xxxx xxxx" }, { OH, ".... .... .... .x.." } }),
        groove ("Funky Groove", "Funky & Disco",
                { { K, fourFloor }, { S, ".... X... ..g. X..." }, { CH, "x.x. x.x. x.x. x.xx" }, { T, "..xx ..x. ..xx ..x." } }),
        groove ("Filter Disco", "Funky & Disco",
                { { K, fourFloor }, { C, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { T, "..xx ..x. ..xx ..x." } }),
        groove ("Boogie House", "Funky & Disco",
                { { K, "X... X... X... X.X." }, { S, backbeat }, { CH, "..x. ..x. ..x. ..xx" }, { CB, ".... .... ..x. ...." },
                  { CG, "..x. .xx. .... ...." }, { T, "..xx ..x. ..xx ..x." } }),

        // ---- '90s Piano House: the 909 open hat on every offbeat, clap and snare together ----
        groove ("90s Italo", "'90s Piano House",
                { { K, fourFloor }, { C, backbeat }, { S, ".... x... .... x..." }, { OH, "..x. ..x. ..x. ..x." },
                  { CH, "x... x... x... x..." }, { T, "..xx ..x. ..xx ..x." } }),
        groove ("Jersey 909", "'90s Piano House",
                { { K, "X... X... X... X... X... X... X... X.x." }, { C, backbeat }, { S, ".... x... .... x..." },
                  { OH, "..x. ..x. ..x. ..x." }, { CH, "...x .... ...x .x.." }, { T, "..xx ..x. ..xx ..x." } }),
        groove ("Rave Piano", "'90s Piano House",
                { { K, fourFloor }, { C, backbeat }, { OH, "..x. ..x. ..x. ..x." }, { T, "xxxx xxxx xxxx xxxx" } }),
        groove ("Classic Club 90s", "'90s Piano House",
                { { K, "X... X... X... X... X... X... X... X.x." }, { C, backbeat }, { CH, "..x. ..x. ..x. ..xx" },
                  { OH, ".... .... .... .x.." }, { T, "..xx ..x. ..xx ..x." } }),
        groove ("NY Garage 90s", "'90s Piano House",
                { { K, fourFloor }, { C, ".... X..x .... X..." }, { CH, "x.x. x.x. x.x. x.x." }, { SH, "..xx ..x. ..xx ..x." } }),

        // ---- Neo Soul & Broken: no four on the floor, lazy pockets, broken kicks ----
        groove ("Neo Soul Pocket", "Neo Soul & Broken",
                { { K, "X... .... ..x. .... X... ...x ..x. ...." }, { S, ".... X..g ..g. X..." },
                  { CH, "x.xg x.xg x.xg x.xg" } }),
        groove ("Broken Beat", "Neo Soul & Broken",
                { { K, "X... ..x. .x.. .... X... ..x. ...x ..x." }, { S, ".... X... .... .X.. .... X... ..g. X..." },
                  { CH, "x.x. x.xx x.x. x.x." }, { R, ".... .... x... ...." } }),
        groove ("Dilla Swing", "Neo Soul & Broken",
                { { K, "X... ...x ..x. .... X... .... ..x. .x.." }, { S, ".... X... .... X... .... X... .... f..." },
                  { CH, "x.xg x.x. x.xg x.x." } }),
        groove ("West London Broken", "Neo Soul & Broken",
                { { K, "X... ..x. ..x. .... X..x ..x. .... ..x." }, { C, ".... x... .... X... .... x... ..x. ..X." },
                  { CH, "x.xx x.x. x.xx x.x." }, { CG, ".... .x.. .... ..x." } }),

        // ---- Chill & Pads: soft kicks, rims and shakers under long chords ----
        groove ("Chill House", "Chill & Pads",
                { { K, "x... x... x... x..." }, { R, ".... x... .... x..." }, { SH, "g.x. g.x. g.x. g.x." },
                  { OH, "..g. ..g. ..g. ..g." } }),
        groove ("Lounge Rim", "Chill & Pads",
                { { K, "x... ..x. x... ...." }, { R, ".... x..g .... x..." }, { RD, "x.g. x.g. x.g. x.g." } }),
        groove ("Half-Time Chill", "Chill & Pads",
                { { K, "X... .... ..x. ...." }, { S, ".... .... X... ...." }, { CH, "x.g. x.g. x.g. x.g." },
                  { SH, "..x. ..x. ..x. ..x." } }),
        groove ("Sunset Shaker", "Chill & Pads",
                { { K, "x... x... x... x..." }, { C, ".... x... .... x..." }, { SH, "xgxg xgxg xgxg xgxg" },
                  { CG, ".... ..x. .... .x.x" } }),

        // ---- Dance Stabs: big, straight club beats ----
        groove ("Dance Floor", "Dance Stabs",
                { { K, fourFloor }, { C, backbeat }, { S, ".... x... .... x..." }, { OH, offbeats },
                  { CH, "xx.x xx.x xx.x xx.x" } }),
        groove ("Euro Dance", "Dance Stabs",
                { { K, fourFloor }, { S, ".... X... .... X... .... X... .... X.xx" }, { C, backbeat }, { OH, offbeats },
                  { CH, "x... x... x... x..." }, { T, "..x. ..x. ..x. ..x." }, { CR, crashBar1 } }),
        groove ("Stab Pump", "Dance Stabs",
                { { K, fourFloor }, { OH, offbeats }, { C, ".... X... .... X..x" }, { R, "...x ...x ...x ...x" } }),
        groove ("Hands Up", "Dance Stabs",
                { { K, fourFloor }, { S, ".... X... .... X... .... X... ..X. XXXX" }, { C, backbeat }, { OH, offbeats },
                  { CH, "xxxx xxxx xxxx xxxx" }, { CR, crashBar1 } }),
    };
    return patterns;
}

const std::vector<const char*>& grooveCategories()
{
    static const std::vector<const char*> categories { "Piano House", "Soulful", "Deep", "Gospel", "Jackin'", "Jackin' Piano", "Garage & UK",
                                                       "Afro & Latin", "Funky & Disco", "'90s Piano House", "Neo Soul & Broken",
                                                       "Chill & Pads", "Dance Stabs" };
    return categories;
}

int categoryIndex (const std::string& category) noexcept
{
    const auto& c = grooveCategories();
    for (size_t i = 0; i < c.size(); ++i)
        if (category == c[i])
            return (int) i;
    return -1;
}

const std::vector<std::vector<int>>& presetsByGenre()
{
    static const auto byGenre = []
    {
        std::vector<std::vector<int>> lists (grooveCategories().size());
        const auto& presets = drumPatterns();
        for (size_t i = 0; i < presets.size(); ++i)
            if (const int genre = categoryIndex (presets[i].category); genre >= 0)
                lists[(size_t) genre].push_back ((int) i);
        return lists;
    }();
    return byGenre;
}

float genreSwing (int category) noexcept
{
    // In 16th shuffle: 50 + swing*100/6. 0.36 is 56%, 0.6 is 60%, 0.72 is 62%. Chicago jack sits
    // at 62%; soulful and disco play almost straight.
    static const float swing[] { 0.45f, 0.2f, 0.5f, 0.3f, 0.72f, 0.6f, 0.75f, 0.35f, 0.3f, 0.45f, 0.9f, 0.3f, 0.1f };
    return category >= 0 && category < (int) std::size (swing) ? swing[category] : 0.2f;
}

int randomPatternIndex() noexcept
{
    return (int) drumPatterns().size();
}

int userPatternIndex() noexcept
{
    return randomPatternIndex() + 1;
}

} // namespace fgd

