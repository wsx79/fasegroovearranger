// Original family control explanations, reused by the standalone arranger.
#include "UiText.h"
namespace fga {
juce::String controlTip(const juce::String& key){
    if(key.equalsIgnoreCase("GROOVE"))return tr("The rhythm of both hands, grouped by genre");
    if(key.equalsIgnoreCase("REGISTER"))return tr("Moves the right hand up or down the keyboard");
    if(key.equalsIgnoreCase("STRUM"))return tr("Rolls the chord notes like a pianist instead of hitting them all together");
    if(key.equalsIgnoreCase("STRUM DIRECTION"))return tr("Direction of the roll: up, down, or alternating (up on the beat, down off it). Click to change");
    if(key.equalsIgnoreCase("SWING"))return tr("Delays the off-beat 16ths for a shuffled, human feel");
    if(key.equalsIgnoreCase("GATE"))return tr("How long the chords ring: from short stabs to long chords");
    if(key.equalsIgnoreCase("VELOCITY"))return tr("How hard the notes are played");
    if(key.equalsIgnoreCase("HUMANIZE"))return tr("Small timing and dynamics variations, like a real player");
    if(key.equalsIgnoreCase("DENSITY"))return tr("Random riffs only: from sparse stabs to busy chops");
    if(key.equalsIgnoreCase("TONE"))return tr("Brightness of the guide sound");
    if(key.equalsIgnoreCase("REVERB"))return tr("Reverb on the guide sound");
    if(key.equalsIgnoreCase("CHORUS"))return tr("Widens the guide sound");
    if(key.equalsIgnoreCase("PUMP"))return tr("Sidechain depth: ducks the guide sound like a kick sidechain");
    if(key.equalsIgnoreCase("PUMP RELEASE"))return tr("How fast the sound comes back after each duck");
    if(key.equalsIgnoreCase("FILL EVERY"))return tr("How often a fill leads into the next chord");
    if(key.equalsIgnoreCase("FILL STYLE"))return tr("What the fill plays");
    if(key.equalsIgnoreCase("TOP LINE"))return tr("The melody the top notes of the chords draw across the progression");
    if(key.equalsIgnoreCase("APPROACH"))return tr("Gospel slide: on the last 16th before a chord change, play the next chord a half step above or below");
    if(key.equalsIgnoreCase("RANGE LOW"))return tr("The chords' range: no chord note goes below the left thumb or above the right one (notes outside move by an octave)");
    if(key.equalsIgnoreCase("RANGE HIGH"))return tr("The chords' range: no chord note goes below the left thumb or above the right one (notes outside move by an octave)");
    if(key.equalsIgnoreCase("PROGRESSION"))return tr("Ready-made progressions, grouped by style (they also set the scale)");
    if(key.equalsIgnoreCase("OPTIONS"))return tr("Language, interface size, tips and credits");
    if(key.equalsIgnoreCase("NEW FEEL"))return tr("Another humanization of the same groove (repeatable: saved with the project)");
    if(key.equalsIgnoreCase("HUMANIZE STYLE"))return tr("How Humanize is shared: Even, Pianist (the top note sings), Laid-back bass, Tight bass. Click to change");
    if(key.equalsIgnoreCase("MORPH"))return tr("Morph the groove step by step towards another one (locked parts stay)");
    if(key.equalsIgnoreCase("DNA"))return tr("Musical DNA: variations by intention (more soulful, funkier, more minimal, more energy, more sophisticated)");
    if(key.equalsIgnoreCase("A/B"))return tr("Voicing Coach: compare two voicings of your progression");
    if(key.equalsIgnoreCase("VARIANTS"))return tr("Chord variations: more soulful, gospel, deep or chromatic versions of your progression, to hear and keep");
    if(key.equalsIgnoreCase("EDIT GRID"))return tr("The rhythm grid: click the steps to write your own groove");
    if(key.equalsIgnoreCase("APPLY TRACK"))return tr("Apply the prepared settings to this track; locked clips keep their notes.");
    if(key.equalsIgnoreCase("APPLY CLIP"))return tr("Apply the prepared settings to this track; locked clips keep their notes.");
    if(key.equalsIgnoreCase("NEW TRACK"))return tr("Try another take of this track, keeping the rest of the song.");
    if(key.equalsIgnoreCase("EDIT MIDI"))return tr("Open the full MIDI editor for the selected clip.");
    if(key.equalsIgnoreCase("SAVE"))return tr("Save the song and all instrument settings in a project.");
    if(key.equalsIgnoreCase("SAVE AS"))return tr("Save the song and all instrument settings in a project.");
    if(key.equalsIgnoreCase("SOLISTA"))return tr("Enable the guitar soloist and the piano accompaniment.");
    if(key.equalsIgnoreCase("LOCK TRACK"))return tr("Lock this part against regeneration; manual note editing remains available.");
    if(key.equalsIgnoreCase("LOCK CLIP"))return tr("Lock this part against regeneration; manual note editing remains available.");
    if(key.equalsIgnoreCase("PLAY"))return tr("Start or pause playback.");
    if(key.equalsIgnoreCase("STOP"))return tr("Stop playback and return to the beginning.");
    if(key.equalsIgnoreCase("NEW ARRANGEMENT"))return tr("Rebuild the song form for the chosen genre and duration.");
    if(key.equalsIgnoreCase("NEW CLIP"))return tr("Generate new notes only in the selected part; locked parts stay.");
    if(key.equalsIgnoreCase("NEW SECTION"))return tr("Generate new notes only in the selected part; locked parts stay.");
    if(key.equalsIgnoreCase("GENERATE SONG"))return tr("Generate new notes only in the selected part; locked parts stay.");
    if(key.equalsIgnoreCase("AUDIO"))return tr("Choose the audio output and buffer size.");
    if(key.equalsIgnoreCase("Pedal & Expression CC"))return tr("Pedal sustains notes between chord changes; expression rises in build-ups. Included in VST playback and MIDI export.");
    if(key.equalsIgnoreCase("Riff seed"))return tr("Zero follows the stored clip seed; another value gives a repeatable feel or riff.");
    if(key.equalsIgnoreCase("Humanize seed"))return tr("Zero follows the stored clip seed; another value gives a repeatable feel or riff.");
    if(key.equalsIgnoreCase("Sidechain Source"))return tr("Tempo follows the chosen rate; Kick MIDI ducks on notes in the kick track.");
    if(key.equalsIgnoreCase("Sidechain Rate"))return tr("How often the tempo sidechain ducks: every beat, 8th, half bar or 16th");
    if(key.equalsIgnoreCase("MOVEMENT"))return tr("Voicing movement: some hits play the chord an inversion up or down so the riff moves instead of repeating the same chord");
    if(key.equalsIgnoreCase("Top +8va"))return tr("Double the top note of every chord an octave higher (bright house piano)");
    if(key.equalsIgnoreCase("VARIATION EVERY"))return tr("Every 4 or 8 bars the last bar plays a variation of the groove, so the loop never feels static. Evolve: over 8 or 16 bars the same groove grows - as written, then ghost notes, then a variation, then a fill into the next round");
    if(key.equalsIgnoreCase("EVOLVE"))return tr("Every 4 or 8 bars the last bar plays a variation of the groove, so the loop never feels static. Evolve: over 8 or 16 bars the same groove grows - as written, then ghost notes, then a variation, then a fill into the next round");
    if(key.startsWithIgnoreCase("Inversion"))return tr("Change the inversion of this chord, from three steps down to three steps up.");
    return tr(key)+". "+tr("Settings apply on the next generation; the MIDI already written stays editable.");
}

void translateControls(juce::Component& c){
    auto& properties=c.getProperties();
    auto text=[&](const juce::String& current,const juce::String& name){
        auto source=name+"Source",last=name+"Last";
        if(!properties.contains(source)||current!=properties[last].toString())properties.set(source,current);
        auto translated=tr(properties[source].toString());properties.set(last,translated);return translated;
    };
    juce::String key;
    if(auto* button=dynamic_cast<juce::Button*>(&c)){key=button->getButtonText();button->setButtonText(text(key,"button"));key=properties["buttonSource"].toString();}
    else if(auto* box=dynamic_cast<juce::ComboBox*>(&c)){
        for(int i=0;i<box->getNumItems();++i){auto id=box->getItemId(i);box->changeItemText(id,text(box->getItemText(i),"item"+juce::String(id)));}
    }
    else if(auto* label=dynamic_cast<juce::Label*>(&c))label->setText(text(label->getText(),"label"),juce::dontSendNotification);
    if(properties.contains("fgaControl"))key=properties["fgaControl"].toString();
    if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(&c)){
        auto current=tip->getTooltip();
        if(properties.contains("fgaTip"))tip->setTooltip(tr(properties["fgaTip"].toString()));
        else if(key.isNotEmpty())tip->setTooltip(controlTip(key));
        else if(current.isNotEmpty())tip->setTooltip(text(current,"tip"));
    }
    if(dynamic_cast<juce::ComboBox*>(&c)||dynamic_cast<juce::Slider*>(&c)||dynamic_cast<juce::TextEditor*>(&c))return;
    for(auto* child:c.getChildren())translateControls(*child);
}
}
