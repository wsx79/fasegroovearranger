# Audit approfondito — Fase Groove Arranger 0.32.2

Nota 0.33.0: le voci A1, A2, A3, M1, M2, M3, M4, M5, H1 e i rifiuti silenziosi di import/rearrange sono state implementate. La decisione di prodotto è che la musica segue la guida. R7, Link e l'unificazione dei motori chitarra/lead restano fuori scope. Questo file è il verbale dell'audit, non la specifica riga per riga.

Analisi statica in sola lettura, scritta prima delle correzioni.

## 1. Snapshot

- Commit: `1c56122e59153172ff6b4e162d4d8e0ad5400580`
- Messaggio: `Release 0.32.2 fix verified audit regressions [release]` (5 nov 2026)
- Versione CMake: `0.32.2`
- Data analisi: 5 novembre 2026
- Perimetro: `Source/`, `Source/core/`, motori vendored in `ThirdParty/fasegroove{piano,drums,guitar,lead}`, senza rieseguire la CI

I fix dichiarati in 0.32.2 risultano presenti nel sorgente attuale e non sono riproposti come aperti: ritiro di `exportTask`, `resolveSectionHarmony` sui generatori, span audio implicito, split/delete transazionali, budget note, panic MIDI denso, flush PDC al seek, default `randomAutomation`, vita del popover accordi, coda parametri da `flushIdle`, hash plugin annullabile. `docs/AUDIT_0.32.1_RISCONTRO.md` resta il resoconto di quella tornata.

R7 (spezzare `ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.cpp`, ancora 2741 righe) resta in pausa. La dimensione complica la review, non è di per sé un difetto di runtime. Non è il passo successivo consigliato.

## 2. Sintesi

Il motore audio, il handoff lock-free e il salvataggio JSON sono molto più difesi delle release precedenti. I buchi rimasti sono di modello, non di stile.

Il problema più grave è che il menu sezione e il resize sul righello non usano le API che spostano davvero la musica. `resizeGuide`, `moveGuide` e `removeGuide` staccano ogni traccia (`freeTimeline`) e lasciano le clip ai beat assoluti, mentre la griglia delle sezioni si allunga, si riordina o si accorcia. Duplicate e «aggiungi sezione» seguono un’altra semantica. Dopo il primo stacco, `insertSection` sposta automazione e loop ma non le clip libere, e la rigenerazione di sezione può riscrivere il take che ora cade nella finestra nuova.

Il secondo buco è l’armonia di sezione. La generazione (piano, basso, archi, chitarra, lead, elettronica) passa da `resolveSectionHarmony`. Il menu EDIT CHORDS, i combo di tonalità/scala/progressione, NEW CHORDS, il pennello chitarra e i vocal chop no. Lo schermo può mostrare una progressione e scrivere un’altra.

Non è emerso un use-after-free nuovo sul callback audio, né un buco nel contratto host MIDI post-0.32.2. Restano finestre di PDC finché il thread di controllo non alloca la linea, e un dubbio sul lock del processore tenuto durante la distruzione dell’editor nativo.

## 3. Difetti

### Alta

**A1. Durata, sposta ed elimina sezione non muovono il MIDI**

- Dove: `Project::resizeGuide`, `moveGuide`, `removeGuide` in `Source/core/Timeline.cpp` (circa 74, 102–103). Chiamati dal menu in `MainComponent::showSectionMenu` (`Source/Menus.cpp`, id 3/4/5 e 401–528) e dal drag del righello in `ArrangementView::mouseDrag` (`resizeGuide` quando `resizeTrack < 0`).
- Evidenza: ogni funzione fa `detachTrack` su tutte le tracce, poi modifica solo `sections`. Non scala gli span, non sposta `position`, non rimappa l’automazione. `resizeSection` / `resizeSectionLoop` (`Project.cpp` 341–346, 484–490) fanno il remap di loop e automazione e, sulle tracce ancora allineate, la durata del clip segue `sections[i].bars`. Quei metodi non sono collegati al menu né al righello. `duplicateSection` invece copia e sposta anche le clip libere.
- Impatto: allungare l’intro da 8 a 16 battute materializza i clip ai beat vecchi e sposta solo l’etichetta della sezione successiva. Il main resta a beat 32, dentro l’intro allungato; la nuova finestra del main è vuota. Spostare o eliminare una sezione riordina la guida e lascia le note dove erano. Tutte le tracce diventano `freeTimeline`. Undo c’è, ma il gesto non fa quello che dice l’etichetta «Durata» / «Sposta» / «Elimina». I test in `TimelineChecks.h` fissano il comportamento «il guide edit non sposta l’audio» solo su clip già indipendenti: non coprono il brano ancora allineato aperto dal menu.
- Direzione: per il menu e il righello usare la semantica di `resizeSectionLoop` / `moveSection` / una `removeSection` che, sulle tracce allineate, cancelli il clip di quell’indice e sulle tracce libere tagli e chiuda il buco come fa `duplicateSection`. Tenere `resizeGuide` solo se un comando dice esplicitamente che si sposta la guida e non l’audio.

**A2. «Aggiungi sezione» e rearrange ignorano le clip già staccate**

- Dove: `Project::insertSection` (`Project.cpp` 440–456), chiamato da `MainComponent::addSongSection` (`Menus.cpp` 80–86). `rearrangeSong` (`Project.cpp` 915–964), pulsante in `UiAuditTests.cpp` circa 272.
- Evidenza: `insertSection` sposta ogni inviluppo (`envelopes()`, quindi anche volume/pan/parametri delle tracce libere) e i locator di loop, e inserisce un clip solo se `!freeTimeline`. Le `position` libere restano. Subito dopo, `generate(..., Scope::section)` (`generateNotes`, circa 1035) spezza i clip liberi sui bordi della sezione nuova e rigenera quelli il cui `sectionAtBeat(clipStart)` coincide con la sezione nuova. `duplicateSection` (Timeline.cpp 75–100) compensa proprio perché `insertSection` non sposta le clip libere; `addSongSection` non lo fa. Il commento in `addSongSection` dice che staccare tutto lasciava la sezione muta: il rimedio ha saltato le tracce già libere.
- `rearrangeSong` ricostruisce i clip solo se `!freeTimeline`. Le tracce manuali vengono staccate se erano allineate, poi escluse dal remap. Automazione di traccia libera e posizioni assolute restano sulla forma vecchia; l’automazione master segue la forma nuova.
- Impatto: dopo uno split, una traccia MIDI aggiunta (`addMidiTrack` nasce `freeTimeline`) o un A1, aggiungere una sezione disallinea note e automazione e può rigenerare il take che occupava il beat della sezione inserita. NEW ARRANGEMENT rimescola la guida e lascia il materiale staccato sulla timeline precedente. Il fallimento di `rearrangeSong` (budget, `resizedLoop`, tetto punti) esce senza messaggio (`if(!rearrangeSong(...))return`).
- Direzione: stesso spostamento temporale di `duplicateSection` (split sui bordi, shift delle `position >= end`, buco vuoto nella sezione nuova) prima della generate. In rearrange, o rifiutare con messaggio se esiste una traccia libera, o rimappare anche quelle con la stessa scala per sezione usata per l’automazione.

**A3. L’armonia di sezione è usata in generazione e ignorata dai controlli che la modificano**

- Dove:
  - Lettura corretta: `resolveSectionHarmony` (`Project.cpp` 124–127), `pianoEngineSettings` (128–130), `generateElectronicClip` (`ElectronicStyles.cpp` 57).
  - Scrittura globale: `applyChordMenuChoice` (`Menus.cpp` 66–71, `auto& s=project.settings`), `harmonyButton` / `keyBox` / `scaleBox` / `progressionBox` (`UiAuditTests.cpp` 345–349, 377), degree e RANDOM CHORDS (`InstrumentPanel.cpp` circa 238–264).
  - Pennello chitarra: `PianoRoll::mouseDown` (`MainComponent.cpp` 150) costruisce `bandChord` da `project.settings`, non dalla sezione del beat.
  - Striscia accordi del piano roll (`MainComponent.cpp` 99–100): i nomi arrivano da `cachedChordNames()` (che, se la sezione selezionata ha `harmony.enabled`, usa la sua progressione), ma il passo è `project.settings.chordSteps` e l’indice è `(clipStart+b)/chordBeats % names.size()`, senza fase di sezione.
- Impatto: con «Independent section harmony» attiva, EDIT CHORDS, il cambio tonalità e NEW CHORDS riscrivono la song e rilanciano `applyInstrumentSettings` su tutte le parti armoniche. Il display può mostrare gli accordi di sezione. Il pennello e la striscia non coincidono con il MIDI che `generate` scriverebbe. Il popover del circolo (`showChordCirclePopover`, circa 1058–1074) è l’unico editor che rispetta `h.enabled`.
- Direzione: un solo accesso, `resolveSectionHarmony(project, selectedSection)` quando `harmony.enabled`, sia in lettura sia in scrittura, e la stessa origine per striscia e pennello (passo, conteggio, fase rispetto a `sectionStart`).

### Media

**M1. Mute durante la registrazione MIDI non rilascia le voci**

- Dove: `AudioEngine::applyMix` (`AudioEngine.cpp` 167–177). Il panic (CC64, note-off, clear di `liveHeld` / `livePedal`) è sotto `if(!i.mute && t.mute && !capture.active)`.
- Impatto: il mix zittisce la traccia (`audible` circa 429) ma plugin e guide continuano a tenere le note. All’unmute le voci tornano. Il contratto di 0.31.18 («il mute rilascia note e sustain») non vale mentre `capture.active`.
- Direzione: stesso rilascio anche in capture, senza cancellare gli eventi già accodati nel take.

**M2. I vocal chop non vedono l’armonia di sezione**

- Dove: `generateVocalChops` (`VocalChops.cpp` 51–53) chiama `chordAtBeat(p.settings, ...)`.
- Impatto: con harmony di sezione attiva, piano/basso/chitarra/lead/elettronica seguono la sezione; i chop interni restano sulla tonalità globale. I test in `ImprovementCoreTests` non coprono la traccia vocal.
- Direzione: `chordAtBeat(resolveSectionHarmony(p, guide), atLocale)`.

**M3. Rigenerare gli archi accende `stringsMode` su tutto il brano**

- Dove: `generateNotes` (`Project.cpp` 1104–1108). Se `stringsMode==0` e la richiesta non è l’intero brano, assegna `StringsMode::pad` a `p.settings`.
- Impatto: un REGENERATE di sezione o traccia archi, pensato per «far sentire qualcosa», resta nel JSON e cambia i generate successivi, anche elettronici che prima tenevano gli archi spenti.
- Direzione: pad solo nel `GrooveSettings` di quel render, oppure un flag di traccia, non `settings.stringsMode`.

**M4. Sampler in sync: la lunghezza di regione non è il tempo consumato dallo stretch**

- Dove: `SampleVoice::renderNextBlock` (`Sampler.cpp` 69 e 85). In `p.sync` lo stretch consuma `n*speed()` frame sorgente; `playedSource` avanza di `data->rate/getSampleRate()*speed()` per campione di uscita. Il gate di fine (`playedSource >= finish-begin`, riga 77) e i fade di regione usano `playedSource`.
- Impatto: se il file non è alla frequenza del device, la regione si chiude prima (o dura di più) di `audioDuration`. La latenza dello stretch è già esclusa dal contatore grazie al `continue` sotto `elapsed<lag`, ma i frame consumati nel chunk di latenza non rientrano in `playedSource`, quindi la coda è lunga circa `outputLatency`. Ipotesi sul rapporto esatto, bug di unità confermato dal confronto delle due formule.
- Direzione: far avanzare `playedSource` dei frame sorgente realmente letti (`input` dello stretch), non di `rate/sr*speed`.

**M5. PDC assente finché la linea non è allocata**

- Dove: `Instrument::applyDelay` (`AudioEngine.cpp` 120–122) ritorna se `capacity<=delay`. `refreshLatencies` (359–363) alza `delayStorageNeeded`; `prepareDelayLines` gira in `collectGarbage` sul thread di controllo.
- Impatto: dopo un cambio di latenza (plugin, freeze, sample rate) i blocchi successivi suonano senza allineamento, finché il timer UI non alloca. Il riscontro 0.32.2 lo descrive come bypass senza allocazione nel callback. È sicuro per la realtime, sbagliato per la fase. Non azzera il ring al volo: quello è già corretto.
- Direzione: tenere l’ultimo delay applicabile finché la linea nuova non è pronta, oppure silenziare solo i canali in ritardo, non farli passare in anticipo.

### Bassa

**B1. Coda di notifica parametri piena: il DSP è aggiornato, l’editor no**

- `applyParameterAutomation` (`AudioEngine.cpp` 293–302): se `(head+1)%capacity == tail` il `setValue` resta e la notifica si perde. Capacità 1024. Sotto molta automazione l’UI del plugin resta indietro.

**B2. Import MIDI e rearrange falliscono in silenzio**

- `importFile` (`MainComponent.cpp` 1011): `placeClip < 0` esce senza messaggio. Il paste di timeline dice «Clip edit exceeds the project limits.»
- Rearrange: vedi A2.

**B3. `generateNotes` può uscire a metà**

- `std::out_of_range` se scope/indici sono incoerenti (`Project.cpp` 1034) e `std::length_error` se il loop generato sfora (`1194`, e l’analogo in `VocalChops.cpp` 58). `MainComponent::regenerate` non cattura. I clip già riscritti nel giro restano modificati; l’undo è stato spinto prima, quindi un’eccezione non gestita perde anche quello. Caso limite (indice UI stantio o loop patologico), non il percorso normale.

**B4. Checkpoint plugin saltato mentre un plugin sta caricando**

- `checkpoint` (`MainComponent.cpp` 443): `if(!loadingPlugin) captureUndoPluginStates()`. `undo` ritorna subito se `loadingPlugin` (522). Un edit partito durante il load finisce in storia senza stato nativo fresco. Danno solo se l’utente modifica il progetto con l’editor aperto mentre un altro slot carica.

**B5. `presetSlots` e le triadi «dominanti»**

- `presetSlots` (`GrooveEngine.cpp` piano, 825–839): il kind 2 richiede `seventh==10`. Con estensione triade la settima non c’è, il grado resta −1 e lo slot diventa free chord. Le note poi passano da `chordForSlot`, quindi spesso suonano; il numerale e `manualChords` no. Bassa udibilità, incoerenza di modello.

**B6. Meta key del MIDI file è solo globale**

- `midiFile` (`Midi.cpp` 26–29) scrive key/scale di `p.settings`. Le sezioni con harmony propria esportano le note giuste e una meta signature della song.

**B7. Ricentro dell’editor nativo a 100 ms**

- `PluginWindow::showCentred` (`NativeWindows.h` 57–63) rifà `centreWithSize` dopo 100 ms. Il bug di primo frame Cocoa di 0.32.0 è coperto. Se l’utente sposta la finestra in quel decimo, il timer la riporta al centro.

## 4. Aree a rischio (ipotesi)

**H1. Lock del processore nel distruttore dell’editor.** `PluginWindow::~PluginWindow` e `closeButtonPressed` (`NativeWindows.h` 73–74) prendono `getCallbackLock()` e il distruttore chiama `clearContentComponent()` tenendo il lock. `juce::CriticalSection` non è ricorsiva. `closeButtonPressed` oggi non distrugge la finestra (la nasconde), quindi il doppio lock sul close non scatta. Se un editor, nel proprio distruttore, riprende lo stesso lock, la chiusura definitiva (cambio strumento, `closePluginEditor`) può deadlockare. Il callback audio usa `ScopedTryLock` e non si blocca. Da riprodurre con un plugin che locca nel distruttore dell’editor.

**H2. `renewForm` azzera i clip liberi delle tracce generate.** `Project.cpp` 867, solo su GENERATE SONG senza lock: `freeTimeline=false` e `clips.assign(sections.size(), {})`. È voluto per riallineare la forma, ma dopo A1 ogni traccia è libera: il generate successivo butta via il MIDI che il resize «aveva conservato», tracce manuali escluse. Combinazione pericolosa, non un secondo bug isolato.

**H3. Grammatica di `randomProgression`.** L’overload a 4 argomenti (`Generators.cpp` 1176) delega al minore se `scaleIsMinor` (terza del modo = 3 semitoni) e al maggiore altrimenti; il gospel (genre 3) ha la sua camminata. Dorico e eolio usano le mosse minori. Lidico e misolidio condividono le mosse ioniche, poi `buildChord` le realizza nel modo: non escono dalla scala, ma la retorica è quella soulful, non quella del modo. Limite di vocabolario, non un off-by-one.

**H4. `randomPatternIndex()` non è casuale.** È la sentinella `patterns.size()`; il seed sta in `riffSeed` / `motifSeed`. Nessun secondo dado nascosto sui pattern di fabbrica.

**H5. Techno / Electro.** `applyElectronicHarmony` e `generateElectronicClip` usano `resolveSectionHarmony`, clamp 0–127, e i test in `HarmonyAuditChecks.h`. Nessun difetto nuovo letto in questo passaggio. Il lead elettronico è monofonico dopo `monophonicLead`. LINK di famiglia (`LinkClient`) non è collegato a `Source/`: l’arranger non pubblica né segue il clock Link. Assenza di funzione, non un crash.

**H6. Code di `assembleMidiTake`.** Tetti 8192 / 4096 segmenti senza `overflow`. Solo take estremi a cavallo di molti loop.

**H7. `Handoff::adopt` si ferma se `retired != nullptr`.** Nel GUI il timer chiama `collectGarbage`. Un host che pubblica stato senza collect lascia mix e schedule indietro. Non è il binario dell’app.

## 5. Suggerimenti (non implementati)

1. **S — Una sola porta per l’armonia di sezione.** Far passare menu, combo, pennello, striscia, vocal chop e export MIDI da `resolveSectionHarmony`. È il difetto che l’utente sente subito e il diff è locale. Copre A3 e M2.
2. **M — Una semantica di forma.** Collegare durata/sposta/elimina/aggiungi/rearrange allo stesso spostamento di clip, automazione e loop, con un messaggio se il budget rifiuta. `duplicateSection` è il modello già scritto. Copre A1 e A2. Prima di toccare `resizeGuide`, decidere se il guide overlay deve restare come comando separato: i test attuali lo richiedono.
3. **S — Mute in registrazione e messaggi sui rifiuti.** Rilasciare le voci anche con capture attivo (M1). Far dire a import e rearrange perché non hanno cambiato nulla (B2).
4. **S — `stringsMode` non globale** (M3) e contatore di regione del sampler allineato ai frame dello stretch (M4).
5. **S — Non aprire l’editor nativo tenendo il callback lock attraverso `clearContentComponent`** (H1). Prendere il lock solo per `removeListener`, distruggere l’editor dopo. Verificare con un plugin che locca nel distruttore.
6. **M — PDC: non anticipare la traccia mentre la linea cresce** (M5). Cambiamento piccolo ma va ascoltato su un plugin con latenza che cambia a caldo.
7. **L — Non unire a mano i due `GrooveEngine` di chitarra e lead.** Sono fork voluti (lane, fill, articolazioni). Il costo è che un fix di timing va portato due volte. Ha senso solo se si continua a patcharli spesso; non è un bug aperto.
8. **L — Link, solo se serve in studio.** Il client c’è in `fasegroove-common` e non è nell’arranger. Utile per suonare insieme ai plugin della famiglia; non sblocca i difetti sopra.

R7 resta in pausa. Non è in questa lista come passo operativo.

## 6. Prossimi passi (decisione umana)

Nessun fix è stato applicato. Ordine suggerito, da confermare:

1. Decidere la semantica del menu sezione: la musica segue la guida (consigliato, allineato a Duplicate) oppure la guida è solo un overlay e le etichette vanno cambiate. Senza questa scelta A1 si «corregge» nel verso sbagliato rispetto a `TimelineChecks.h`.
2. Poi A2, sullo stesso modello di `duplicateSection`, con un test: traccia staccata + MIDI manuale, insert in mezzo, rearrange.
3. Poi A3/M2, con un test di harmony di sezione su pennello chitarra, combo tonalità e vocal chop.
4. M1, M3, M4 sono piccoli e indipendenti.
5. H1 solo con un plugin reale che chiude l’editor sotto carico audio.

Non rilanciare la CI da questo audit: il tree di prodotto non è cambiato.
