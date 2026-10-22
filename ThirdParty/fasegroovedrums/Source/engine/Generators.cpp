// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace fgd
{

namespace
{
    /** Small, fast and repeatable (same seed = same beat on every computer). */
    struct Random
    {
        explicit Random (std::uint32_t seed) noexcept : state (seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL) {}

        std::uint32_t next() noexcept
        {
            state += 0x9E3779B97F4A7C15ULL;
            auto z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return (std::uint32_t) ((z ^ (z >> 31)) >> 32);
        }

        int nextInt (int n) noexcept           { return n > 0 ? (int) (next() % (std::uint32_t) n) : 0; }
        float nextFloat() noexcept             { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
        bool chance (float probability) noexcept { return nextFloat() < probability; }

        template <typename T, size_t N>
        T pick (const T (&items)[N]) noexcept { return items[nextInt ((int) N)]; }

        std::uint64_t state;
    };

    std::string compact (const char* s)
    {
        std::string out;
        for (; *s != 0; ++s)
            if (*s != ' ')
                out += *s;
        return out;
    }

    void soften (std::string& line)
    {
        for (auto& c : line)
            if (c == 'X')
                c = 'x';
    }

    enum class Kick { fourFloor, twoStep, broken, halfTime };

    // The genres, in grooveCategories() order.
    enum Genre { pianoHouse = 0, soulful, deep, gospel, jackin, jackinPiano, garage, afro, funky, nineties, neoSoul, chill, dance };

    /** Jackin' house after the dice, a section variation or a preset: open hat on every
        offbeat, a loud closed hat on the swung "a" (the accent curve makes that 16th the
        punch), and a syncopated shaker when that layer is thin. Kick and backbeat stay. */
    void reinforceJackinHouse (DrumPattern& p)
    {
        const int len = p.length();
        for (int i = 0; i < len; ++i)
        {
            if (i % 4 == 2)
            {
                p[Lane::openHat][(size_t) i] = 'x';
                p[Lane::closedHat][(size_t) i] = '.';
            }
            else if (i % 4 == 3)
            {
                // The swung accent is a closed hat. An open hat on the same step
                // would choke it, so the shuffle never reached the MIDI.
                if (p[Lane::closedHat][(size_t) i] != 'X' && p[Lane::closedHat][(size_t) i] != 'f')
                    p[Lane::closedHat][(size_t) i] = 'x';
                p[Lane::openHat][(size_t) i] = '.';
            }
        }
        int odd = 0;
        for (int i = 1; i < len; i += 2)
            odd += isHit (p[Lane::shaker][(size_t) i]) ? 1 : 0;
        if (odd < 8)
        {
            static const char steps[] = "g.xx..x.g.xx..xg";
            for (int i = 0; i < len; ++i)
                p[Lane::shaker][(size_t) i] = steps[i % 16];
        }
    }
}

//==============================================================================
const char* sectionName (int section) noexcept
{
    static const char* names[] { "Main", "Intro", "Break", "Build-up", "Drop", "Outro" };
    return section >= 0 && section < numSections ? names[section] : "";
}

double sectionVelocityScale (Section section) noexcept
{
    switch (section)
    {
        case Section::intro:     return 0.85;
        case Section::breakdown: return 0.75;
        case Section::buildup:   return 0.95;
        case Section::drop:      return 1.08;
        case Section::outro:     return 0.85;
        case Section::main:
        default:                 return 1.0;
    }
}

bool sectionAllowsFills (Section section) noexcept
{
    return section == Section::main || section == Section::drop;
}

DrumPattern applySection (const DrumPattern& source, Section section)
{
    DrumPattern p = source;
    const int len = p.length();
    auto any = [&p] (Lane l) { return std::any_of (p[l].begin(), p[l].end(), isHit); };
    // Two-step, broken beat and chill keep the kick and the hats they arrived with.
    const int genre = categoryIndex (source.category);
    const bool keepFeel = genre == garage || genre == neoSoul || genre == chill;

    switch (section)
    {
        case Section::intro:
        case Section::outro:
        {
            // Only the kick and the hats (and the shaker), a little softer.
            for (int l = 0; l < numLanes; ++l)
            {
                const auto lane = (Lane) l;
                if (lane == Lane::kick || lane == Lane::closedHat || lane == Lane::openHat || lane == Lane::shaker)
                    soften (p.lanes[(size_t) l]);
                else
                    p.lanes[(size_t) l].assign ((size_t) len, '.');
            }

            if (! any (Lane::kick) && ! keepFeel)
                for (int i = 0; i < len; i += 4)
                    p[Lane::kick][(size_t) i] = 'x';
            break;
        }

        case Section::breakdown:
        {
            // No kick, no backbeat: the hats and the percussion carry it, softly.
            for (auto lane : { Lane::kick, Lane::snare, Lane::clap, Lane::tom, Lane::crash })
                p[lane].assign ((size_t) len, '.');
            for (auto& line : p.lanes)
                soften (line);

            if (std::none_of (p.lanes.begin(), p.lanes.end(), [] (const std::string& l) { return std::any_of (l.begin(), l.end(), isHit); }))
                for (int i = 0; i < len; i += 2)
                    p[Lane::shaker][(size_t) i] = 'x';
            break;
        }

        case Section::buildup:
        {
            // A snare roll that doubles up (quarters, 8ths, 16ths). House puts the kick on every
            // beat and hats on the 8ths; two-step, broken and chill keep their own kick and hats.
            const std::string keptKick = p[Lane::kick], keptClosed = p[Lane::closedHat], keptOpen = p[Lane::openHat];
            for (auto& line : p.lanes)
                line.assign ((size_t) len, '.');

            for (int i = 0; i < len; ++i)
            {
                const double at = (double) i / (double) len;
                const int every = at < 0.5 && len > 16 ? 4 : at < 0.75 ? 2 : 1;
                if (i % every == 0)
                    p[Lane::snare][(size_t) i] = i % 4 == 0 ? 'X' : 'x';
                if (! keepFeel && i % 4 == 0)
                    p[Lane::kick][(size_t) i] = 'X';
                if (! keepFeel && i % 2 == 0)
                    p[Lane::closedHat][(size_t) i] = 'x';
            }
            if (keepFeel)
            {
                p[Lane::kick] = keptKick;
                p[Lane::closedHat] = keptClosed;
                p[Lane::openHat] = keptOpen;
            }
            break;
        }

        case Section::drop:
        {
            // House is lifted: the open hat on every offbeat when no open hat plays, and a 16th
            // shaker when neither shaker nor tambourine does (the engine adds a crash every 8
            // bars). Two-step, broken and chill stay on the feel they came in with.
            if (! keepFeel && ! any (Lane::openHat))
                for (int i = 2; i < len; i += 4)
                {
                    p[Lane::openHat][(size_t) i] = 'x';
                    p[Lane::closedHat][(size_t) i] = '.';
                }
            if (! keepFeel && ! any (Lane::shaker) && ! any (Lane::tambourine))
                p[Lane::shaker].assign ((size_t) len, 'x');
            break;
        }

        case Section::main:
        default:
            break;
    }

    // Main and drop keep the jack even when a section variation moved the hats.
    // Breakdown and the snare-roll build stay as the section wrote them.
    if ((section == Section::main || section == Section::drop) && genre == jackin)
        reinforceJackinHouse (p);

    if (section != Section::main)
        p.name = source.name + " / " + sectionName ((int) section);

    return p;
}

//==============================================================================
namespace
{
    // One-bar cells and how common each is in house drum programming (the weight). Hats are
    // written plainly: the accent groove makes the offbeat sing and the other 16ths soft.
    struct Cell { const char* steps; int weight; };

    const Cell hatCells[] {
        { "..x. ..x. ..x. ..x.", 39 }, { "x.x. x.x. x.x. x.x.", 16 }, { "..x. ..x. ..x. ..xx", 15 }, { "..x. x.x. ..x. x.x.", 9 },
        { "..x. ..x. ..xx ..x.", 7 }, { "..x. ..xx ..x. ..x.", 5 }, { "..xx ..x. ..x. ..x.", 5 }, { ".xxx .xxx .xxx .xxx", 4 },
        { "..xx x.x. ..x. ..x.", 3 } };
    const Cell jackHatCells[] {
        { "..x. ..xg ..x. ..x.", 6 }, { "..x. .gx. ..x. .gxx", 5 }, { "g.x. g.x. g.x. g.xg", 4 }, { "..xg ..x. ..xg ..x.", 4 },
        { "x.xg x.x. x.xg x.xx", 3 }, { "..x. x.xg ..x. x.x.", 3 }, { "..x. ..x. ..x. ..x.", 2 } };
    const Cell busyHatCells[] { { "xxxx xxxx xxxx xxxx", 3 }, { "x.xx x.xx x.xx x.xx", 1 }, { ".xxx .xxx .xxx .xxx", 1 } };
    const Cell underOpenHatCells[] { { "x... x... x... x...", 3 }, { "xx.x .x.x xx.x .x.x", 2 }, { "x..x x..x x..x x..x", 1 } };   // never on the offbeat
    const Cell openAccentCells[] {
        { ".... .... .... .x..", 18 }, { ".x.. .... .... ....", 8 }, { ".... .... .... ...x", 7 }, { ".x.. .... .... .x..", 6 },
        { "...x ...x ...x ...x", 5 } };
    const Cell clapCells[] {
        { ".... X... .... X...", 60 }, { ".... X..x .... X...", 8 }, { ".... Xg.. .... Xx..", 8 }, { ".... X... .... X..x", 5 },
        { ".... X... .... X.x.", 4 }, { ".... X... ..x. X...", 2 } };
    const Cell jackSnareCells[] {
        { ".... x... .... x...", 5 }, { "g... x... g... x...", 4 }, { ".... x..x .... x...", 4 }, { "...g x... ...g x...", 3 },
        { ".... x... ..g. x..x", 3 } };
    const Cell shakerCells[] {
        { "..xx ..x. ..xx ..x.", 12 }, { "x..x ..x. ..x. x...", 7 }, { "xxxx xxxx xxxx xxxx", 6 }, { "..xx ..xx ..xg ..xx", 5 },
        { "g.gx g.xx ..xx g.xx", 5 }, { "..x. ..x. ..x. ..x.", 4 }, { "xgx. xgxx xgx. xgxx", 4 }, { "g.xg xgxg g.xg xgxg", 3 } };
    const Cell percCells[] {
        { "x... .... .... ....", 8 }, { ".... .... .... ...x", 8 }, { "...x ...x ...x ...x", 6 }, { ".... .... .xxx x...", 4 },
        { "..xx g... ..xx g..g", 4 }, { "..x. .x.. .... ....", 3 }, { "..xx g... ..xx ...g", 3 }, { ".... ..x. .... x.x.", 3 } };
    const Cell lowPercCells[] {
        { "xx.x x.xx .... ....", 4 }, { ".... .... .... ...x", 4 }, { ".... .x.. .... ....", 4 }, { "..x. .xx. .... ....", 4 },
        { ".... .... ..xx ...g", 3 }, { ".... .... .... ..xx", 3 } };

    template <size_t N>
    const char* pickCell (Random& r, const Cell (&cells)[N])
    {
        int total = 0;
        for (const auto& c : cells) total += c.weight;
        int x = r.nextInt (total);
        for (const auto& c : cells)
            if ((x -= c.weight) < 0)
                return c.steps;
        return cells[0].steps;
    }

    /** What each genre leans on: the chance of each layer (scaled by the density). */
    struct GenreStyle
    {
        float openOffbeat;   // the 909 open hat on every offbeat (then the closed hat goes around it)
        float busyHats;      // 16th hats
        float openAccent;    // a single open hat accent
        float shaker, tambourine;
        float percussion;    // a sparse conga / rim / cowbell line
        float lowPercussion; // a low conga / tom line
        float clapVariation; // a clap pick-up or ghost
        float snare;         // a snare with (or instead of) the clap
        Lane percussionLane;
    };

    GenreStyle styleOf (int genre)
    {
        switch (genre)
        {
            case pianoHouse:  return { 0.95f, 0.05f, 0.35f, 0.5f,  0.1f,  0.25f, 0.15f, 0.25f, 0.1f, Lane::conga };
            case soulful:     return { 0.95f, 0.3f,  0.3f,  0.55f, 0.35f, 0.6f,  0.3f,  0.3f,  0.15f, Lane::conga };
            case deep:        return { 0.95f, 0.0f,  0.3f,  0.4f,  0.0f,  0.55f, 0.3f,  0.3f,  0.0f, Lane::rim };
            case gospel:      return { 0.5f,  0.1f,  0.2f,  0.1f,  0.85f, 0.2f,  0.1f,  0.35f, 0.8f, Lane::conga };
            case jackin:      return { 0.0f,  0.0f,  0.5f,  0.2f,  0.05f, 0.7f,  0.45f, 0.55f, 0.7f,  Lane::cowbell };
            case jackinPiano: return { 0.5f,  0.0f,  0.35f, 0.35f, 0.15f, 0.35f, 0.15f, 0.4f,  0.1f, Lane::rim };
            case garage:      return { 0.0f,  0.85f, 0.15f, 0.15f, 0.0f,  0.55f, 0.2f,  0.2f,  0.85f, Lane::rim };
            case afro:        return { 0.35f, 0.0f,  0.2f,  0.9f,  0.0f,  0.8f,  0.65f, 0.15f, 0.0f, Lane::conga };
            case funky:       return { 0.95f, 0.35f, 0.3f,  0.1f,  0.5f,  0.25f, 0.2f,  0.3f,  0.6f, Lane::cowbell };
            case nineties:    return { 0.8f,  0.05f, 0.2f,  0.2f,  0.35f, 0.1f,  0.05f, 0.25f, 0.6f, Lane::conga };
            case neoSoul:     return { 0.0f,  0.15f, 0.15f, 0.25f, 0.0f,  0.2f,  0.05f, 0.1f,  0.7f, Lane::rim };
            case chill:       return { 0.05f, 0.0f,  0.25f, 0.55f, 0.05f, 0.15f, 0.05f, 0.05f, 0.15f, Lane::rim };
            case dance:       return { 0.85f, 0.0f,  0.1f,  0.0f,  0.2f,  0.0f,  0.0f,  0.1f,  0.6f, Lane::conga };
            default:          return { 0.2f,  0.0f,  0.2f,  0.4f,  0.0f,  0.3f,  0.1f,  0.2f,  0.2f, Lane::rim };
        }
    }
}

DrumPattern makeRandomPattern (std::uint32_t seed, float density, int genre)
{
    // A beat is built as house producers build it: the kick, the clap on 2 and 4, ONE hat line
    // (mostly on the offbeat), and only a few other layers, each from the patterns that groove
    // in house music. Fewer layers at low density. Bar 2 repeats bar 1 with a small answer at its end.
    Random r (seed);
    density = std::clamp (density, 0.0f, 1.0f);
    genre = std::clamp (genre, 0, (int) grooveCategories().size() - 1);
    const auto style = styleOf (genre);
    const float more = 0.35f + 1.1f * density;   // how much the optional layers come in

    DrumPattern p;
    p.name = "Random beat";
    p.category = grooveCategories()[(size_t) genre];
    for (auto& line : p.lanes)
        line.assign ((size_t) randomBeatLength, '.');

    auto setBar = [&p] (Lane lane, const char* cell)   // one bar, written into both bars
    {
        const auto c = compact (cell);
        for (int i = 0; i < 16 && i < (int) c.size(); ++i)
            p[lane][(size_t) i] = p[lane][(size_t) (i + 16)] = c[(size_t) i];
    };
    auto setLoop = [&p] (Lane lane, const char* line)  // both bars
    {
        const auto c = compact (line);
        for (int i = 0; i < randomBeatLength && i < (int) c.size(); ++i)
            p[lane][(size_t) i] = c[(size_t) i];
    };
    auto pickOf = [&r] (std::initializer_list<const char*> cells) { return *(cells.begin() + r.nextInt ((int) cells.size())); };

    // ---- Kick (always on the grid) and backbeat ----
    const bool garageBeat = genre == garage, brokenBeat = genre == neoSoul, chillBeat = genre == chill;
    if (garageBeat && r.chance (0.92f))
        setBar (Lane::kick, pickOf ({ "X... .... ..x. ....", "X... .... ..X. ...x", "X... ...x ..x. ...." }));
    else if (brokenBeat)
        setLoop (Lane::kick, pickOf ({ "X... .... ..x. .... X... ...x ..x. ....", "X... ..x. .x.. .... X... ..x. ...x ..x.",
                                      "X... ...x ..x. .... X... .... ..x. .x.." }));
    else if (chillBeat && r.chance (0.4f))
        setBar (Lane::kick, "X... .... ..x. ....");
    else
        setBar (Lane::kick, genre == deep || chillBeat ? "x... x... x... x..." : "X... X... X... X...");
    if (genre != deep && ! chillBeat && ! brokenBeat && ! garageBeat && r.chance (0.15f * more))
        p[Lane::kick][30] = 'x';                                  // a kick pick-up into the next loop
    else if (genre == jackin && r.chance (0.35f))
        p[Lane::kick][31] = 'g';                                  // a ghost kick into the next loop (on the grid)

    const bool halfTime = chillBeat && p[Lane::kick][4] == '.';
    const char* clapCell = halfTime ? ".... .... X... ...." : r.chance (style.clapVariation * more) ? pickCell (r, clapCells) : ".... X... .... X...";
    if (brokenBeat || garageBeat)
        setBar (Lane::snare, halfTime ? ".... .... X... ...." : pickOf ({ ".... X... .... X...", ".... X..g .... X...", ".... X... ...g X..." }));
    else if (chillBeat)
        setBar (r.chance (0.5f) ? Lane::rim : Lane::clap, halfTime ? ".... .... x... ...." : ".... x... .... x...");
    else
    {
        setBar (Lane::clap, clapCell);
        if (genre == deep)
        {
            soften (p[Lane::clap]);
            setBar (Lane::rim, ".... x... .... x...");   // the rim with the clap, not instead of it
        }
        else if (genre == jackin && r.chance (style.snare))
        {
            // The 909 snare under the jack clap: soft on the beats or loud swung pick-ups,
            // and bar 2 turns round into the next loop.
            setBar (Lane::snare, pickCell (r, jackSnareCells));
            const auto turn = compact (pickOf ({ "..g. x..x", "..x. x.xx", ".... XgXX", "..g. x.x." }));
            for (int i = 0; i < 8; ++i)
                if (turn[(size_t) i] != '.')
                    p[Lane::snare][(size_t) (24 + i)] = turn[(size_t) i];
        }
        else if (r.chance (style.snare))
            setBar (Lane::snare, genre == funky && r.chance (0.5f) ? ".... X... ..g. X..." : ".... x... .... x...");
    }

    // ---- ONE hat line ----
    const bool openOffbeat = genre != jackin && ! garageBeat && ! brokenBeat && r.chance (style.openOffbeat);
    if (garageBeat)
        setBar (Lane::closedHat, pickOf ({ "x.xx x.xx x.xx x.xx", "..xx ..xx ..xx ..xx", "x.xx x.x. x.xx x.x." }));
    else if (brokenBeat)
        setBar (Lane::closedHat, pickOf ({ "x.xg x.xg x.xg x.xg", "x.x. x.xx x.x. x.x.", "x.xx x.x. x.xx x.x." }));
    else if (genre == jackin)
    {
        // Offbeat open hat, and swung 16ths in the closed hat (they sit off the offbeat).
        setBar (Lane::openHat, "..x. ..x. ..x. ..x.");
        setBar (Lane::closedHat, pickOf ({ "x..g x..g x..g x..g", "x..g ..xg x..g ..xg", "x.xg x..g x.xg x..x" }));
    }
    else if (openOffbeat)
    {
        setBar (Lane::openHat, "..x. ..x. ..x. ..x.");
        if (r.chance (0.6f * more))
            setBar (Lane::closedHat, pickCell (r, underOpenHatCells));
    }
    else if (genre == jackin)
        setBar (Lane::closedHat, pickCell (r, jackHatCells));   // the offbeat with ghosts and pick-ups around it
    else
        setBar (Lane::closedHat, r.chance (style.busyHats * more) ? pickCell (r, busyHatCells) : pickCell (r, hatCells));

    // A single open hat accent (with a closed hat line only).
    if (! openOffbeat && genre != jackin && r.chance (style.openAccent * more))
    {
        const auto accent = compact (pickCell (r, openAccentCells));
        for (int i = 0; i < 16; ++i)
            if (accent[(size_t) i] == 'x')
                for (int at : { i, i + 16 })
                    p[Lane::openHat][(size_t) at] = 'x', p[Lane::closedHat][(size_t) at] = '.';
    }

    // ---- Shaker or tambourine ----
    if (r.chance (style.tambourine * more))
        setBar (Lane::tambourine, r.chance (0.3f) ? ".... x... .... x..." : pickCell (r, shakerCells));
    else if (r.chance (style.shaker * more))
        setBar (Lane::shaker, pickCell (r, shakerCells));

    // ---- A soft 16th layer, where the swing and the accents make the groove (most house beats
    //      have one: shaker, tambourine or soft hats between the offbeats) ----
    auto sixteenths = [&p]
    {
        int odd = 0;
        for (int l = 1; l < numLanes; ++l)
            for (int i = 1; i < randomBeatLength; i += 2)
                odd += isHit (p.lanes[(size_t) l][(size_t) i]) ? 1 : 0;
        return odd;
    };
    if (! brokenBeat && ! garageBeat && ! chillBeat && genre != dance && sixteenths() < 8 && r.chance (0.55f + 0.45f * density))
        setBar (genre == gospel || genre == funky || genre == nineties ? Lane::tambourine : Lane::shaker,
                genre == soulful ? "xxxx xxxx xxxx xxxx" : pickOf ({ "..xx ..x. ..xx ..x.", "..xx ..xx ..xg ..xx", "g.xg xgxg g.xg xgxg", "xgx. xgxx xgx. xgxx" }));

    // ---- At most one sparse percussion line, maybe a low one ----
    if (genre == afro && r.chance (0.6f))
        setLoop (Lane::rim, "x... ..x. .... x... .... x... x... ....");   // the son clave
    else if (r.chance (style.percussion * more))   // (on the congas when the rim already plays the backbeat)
        setBar (std::any_of (p[style.percussionLane].begin(), p[style.percussionLane].end(), isHit) ? Lane::conga : style.percussionLane,
                pickCell (r, percCells));
    if (r.chance (style.lowPercussion * more))
        setBar (genre == afro || genre == deep || genre == jackin ? Lane::tom : Lane::conga, pickCell (r, lowPercCells));

    // ---- Bar 2 answers at its very end ----
    if (r.chance (0.3f + 0.4f * density))
        switch (r.nextInt (3))
        {
            case 0:  if (p[Lane::openHat][29] == '.' && ! openOffbeat) p[Lane::openHat][29] = 'x', p[Lane::closedHat][29] = '.'; break;
            case 1:  if (p[Lane::clap][31] == '.' && p[Lane::clap][28] != '.') p[Lane::clap][31] = 'x'; break;
            default: if (p[Lane::conga][31] == '.') p[Lane::conga][31] = 'x'; break;
        }

    // ---- What sets the genre apart (its own dice, so the layers above stay as they were) ----
    Random accent (seed * 2246822519u + 3266489917u);
    if (genre == afro)
    {
        // Three against four: a dotted-8th figure on the congas (or the shaker), and with the
        // clave the clap often gives way.
        if (accent.chance (0.5f))
            setBar (! std::any_of (p[Lane::conga].begin(), p[Lane::conga].end(), isHit) ? Lane::conga : Lane::shaker, "x..x ..x. .x.. x..x");
        if (std::any_of (p[Lane::rim].begin(), p[Lane::rim].end(), isHit) && accent.chance (0.4f))
            p[Lane::clap].assign ((size_t) randomBeatLength, '.');
    }
    else if (genre == deep && accent.chance (0.5f))
    {
        // Soft rim ghosts off the grid's strong points.
        for (int at : { 3, 11, 19, 27 })
            if (p[Lane::rim][(size_t) at] == '.')
                p[Lane::rim][(size_t) at] = 'g';
    }
    else if (garageBeat && accent.chance (0.6f))
    {
        // The garage snare's ghost on the last 16th, into the next bar.
        for (int at : { 15, 31 })
            if (p[Lane::snare][(size_t) at] == '.')
                p[Lane::snare][(size_t) at] = 'g';
    }

    if (genre == jackin)
        reinforceJackinHouse (p);

    normalisePattern (p, randomBeatLength);
    return p;
}

DrumPattern makeVariation (const DrumPattern& source, std::uint32_t seed)
{
    Random r (seed * 2654435761u + 17u);
    DrumPattern p = source;
    const int len = p.length();
    if (len < 16)
        return p;

    const int bar = len - 16;   // the last bar plays the turnaround
    auto at = [&p, bar] (Lane lane, int step) -> char& { return p[lane][(size_t) (bar + step)]; };
    const bool fourFloor = at (Lane::kick, 0) != '.' && at (Lane::kick, 4) != '.' && at (Lane::kick, 8) != '.' && at (Lane::kick, 12) != '.';
    const int genre = categoryIndex (p.category);

    // Chill often leaves the bar alone, and never piles on a second idea.
    if (genre == chill)
    {
        if (r.chance (0.4f))
            return p;
        at (Lane::openHat, 14) = 'x';
        at (Lane::closedHat, 14) = '.';
        return p;
    }

    auto houseMove = [&] (int which)
    {
        switch (which)
        {
            case 0:   // open hat lift into the next bar
                at (Lane::openHat, 14) = 'X';
                at (Lane::closedHat, 14) = '.';
                at (Lane::closedHat, 15) = '.';
                break;
            case 1:   // kick pick-up
                if (fourFloor) at (Lane::kick, 15) = 'x';
                else           at (Lane::kick, 14) = 'x';
                break;
            case 2:   // ghost / flam on the backbeat
                if (std::any_of (p[Lane::clap].begin(), p[Lane::clap].end(), isHit))
                    at (Lane::clap, 12) = 'f';
                at (Lane::snare, 15) = 'g';
                break;
            case 3:   // snare drag into the bar
                at (Lane::snare, 14) = 'g';
                at (Lane::snare, 15) = 'x';
                break;
            default:  // a tom at the very end
                at (Lane::tom, 15) = 'x';
                break;
        }
    };

    // House (and an unnamed grid) keeps the original five moves and the same dice.
    const int table = genre == garage ? 1 : genre == afro ? 2 : genre == neoSoul ? 3 : 0;
    const int choices = table == 0 ? 5 : 4;
    const int moves = 1 + r.nextInt (2);
    for (int m = 0; m < moves; ++m)
    {
        const int which = r.nextInt (choices);
        if (table == 0)
            houseMove (which);
        else if (table == 1)   // garage: a ghost into the one. No tom, no open-hat lift.
        {
            switch (which)
            {
                case 0:  at (Lane::snare, 15) = 'g'; break;
                case 1:  at (Lane::rim, 15) = 'x'; break;
                case 2:  at (Lane::snare, 14) = 'g'; at (Lane::snare, 15) = 'x'; break;
                default: at (Lane::closedHat, 15) = 'g'; break;
            }
        }
        else if (table == 2)   // afro: the congas answer. No tom, no open hat.
        {
            switch (which)
            {
                case 0:  at (Lane::conga, 15) = 'x'; break;
                case 1:  at (Lane::conga, 14) = 'g'; at (Lane::conga, 15) = 'x'; break;
                case 2:  at (Lane::shaker, 15) = 'g'; break;
                default: at (Lane::rim, 14) = 'x'; break;
            }
        }
        else                    // neo soul: a late snare. No kick pick-up on the broken kick.
        {
            switch (which)
            {
                case 0:  at (Lane::snare, 14) = 'g'; at (Lane::snare, 15) = 'x'; break;
                case 1:  at (Lane::snare, 15) = 'g'; break;
                case 2:  at (Lane::rim, 14) = 'g'; break;
                default: at (Lane::closedHat, 15) = 'g'; break;
            }
        }
    }
    return p;
}

DrumPattern makeSectionVariation (const DrumPattern& source, std::uint32_t seed)
{
    Random r (seed * 747796405u + 2891336453u);
    DrumPattern p = source;
    const int len = p.length();
    auto hits = [] (const std::string& line) { return std::any_of (line.begin(), line.end(), isHit); };

    // The closed hats: a few 16th ghosts added or taken away.
    auto& hats = p[Lane::closedHat];
    if (hits (hats))
        for (int n = 2 + r.nextInt (3); n > 0; --n)
        {
            const int i = r.nextInt (len);
            if (p[Lane::openHat][(size_t) i] == '.' && i % 4 != 0)
                hats[(size_t) i] = hats[(size_t) i] == '.' ? 'g' : '.';
        }

    // A new answer on a percussion line that plays (or a light one where none does).
    static constexpr Lane percussion[] { Lane::rim, Lane::conga, Lane::tambourine, Lane::cowbell, Lane::shaker };
    bool moved = false;
    for (auto lane : percussion)
        if (hits (p[lane]) && ! moved && r.chance (0.7f))
        {
            auto& line = p[lane];
            for (int bar = 0; bar < len / 16; ++bar)
            {
                const int from = bar * 16 + r.nextInt (16), to = bar * 16 + r.nextInt (16);
                if (to % 4 != 0)
                    std::swap (line[(size_t) from], line[(size_t) to]);
            }
            moved = true;
        }
    if (! moved)
    {
        const auto lane = r.chance (0.5f) ? Lane::rim : Lane::shaker;
        for (int bar = 0; bar < len / 16; ++bar)
            p[lane][(size_t) (bar * 16 + r.pick ({ 3, 7, 11, 14 }))] = 'g';
    }

    // Ghost notes on the snare now and then.
    if (r.chance (0.5f))
        p[Lane::snare][(size_t) (r.nextInt (len / 16) * 16 + r.pick ({ 7, 10, 15 }))] = 'g';

    // And the turnaround of the last bar.
    return makeVariation (p, seed + 7);
}

DrumPattern applySegmentOrder (const DrumPattern& source, const std::array<int, numSegments>& order)
{
    const int len = source.length();
    if (len % numSegments != 0 || order == std::array<int, numSegments> { 0, 1, 2, 3 })
        return source;

    DrumPattern p = source;
    const int segment = len / numSegments;
    for (int k = 0; k < numSegments; ++k)
    {
        const int from = std::clamp (order[(size_t) k], 0, numSegments - 1);
        for (int l = 0; l < numLanes; ++l)
            for (int i = 0; i < segment; ++i)
                p.lanes[(size_t) l][(size_t) (k * segment + i)] = source.lanes[(size_t) l][(size_t) (from * segment + i)];
    }
    return p;
}

void copySegment (DrumPattern& into, const DrumPattern& from, int segment)
{
    const int len = into.length();
    if (from.length() != len || len % numSegments != 0)
        return;
    const int size = len / numSegments;
    segment = std::clamp (segment, 0, numSegments - 1);
    for (int l = 0; l < numLanes; ++l)
        for (int i = segment * size; i < (segment + 1) * size; ++i)
            into.lanes[(size_t) l][(size_t) i] = from.lanes[(size_t) l][(size_t) i];
}

//==============================================================================
const char* fillStyleName (int style) noexcept
{
    static const char* names[] { "Snare roll", "Tom run", "Clap build", "Hat lift", "Drop out", "Conga run", "Mix",
                                 "Kick build", "Rim shuffle", "Open hat run", "Clap stutter", "Percussion break", "Custom" };
    return style >= 0 && style < numFillStyles ? names[style] : "";
}

int fillStartStep (FillStyle style) noexcept
{
    switch (style)
    {
        case FillStyle::snareRoll:
        case FillStyle::clapBuild:
        case FillStyle::rimShuffle:
        case FillStyle::openHatRun:
        case FillStyle::percBreak: return 8;
        case FillStyle::custom:    return 0;   // from its first written step (see the engine)
        case FillStyle::tomRun:
        case FillStyle::hatLift:
        case FillStyle::dropOut:
        case FillStyle::congaRun:
        case FillStyle::mix:
        default:                   return 12;
    }
}

} // namespace fgd

