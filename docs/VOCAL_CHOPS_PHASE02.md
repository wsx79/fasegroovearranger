# Fase 02 — audit e correzioni mirate

Branch `Vocaltest`, base `358adb342f4dcb2097580065fdfa94e8e28d71f8`, versione
0.43.2 invariata. Vocal Studio rimane separato. Nessun campione vocale esterno è
stato scaricato: i fixture sono sintetizzati nei test e non vengono distribuiti.
Le verifiche storiche del report del 30 ottobre non sono test eseguiti in questa fase.

## 02A: inventario del codice esistente

| Requisito | Funzione già disponibile e percorso |
|---|---|
| Importazione e fallimento atomico | `readSampleAsset`, `Source/Sampler.cpp`: WAV/AIFF/FLAC/Ogg/MP3 tramite JUCE, massimo due canali PCM, NaN/Inf azzerati, fingerprint SHA256, preview di 8192 picchi. `AudioEngine::loadSample` decodifica/prepara prima della sostituzione e del checkpoint; fallimenti conservano la sorgente. |
| Dati condivisi e limiti | `Source/Sampler.h`: `SampleAsset` immutabile condiviso fra sound/voices; `AudioEngine::offlineCopy` condivide il PCM. `Source/core/StudioSettings.h`: limite per file 30 minuti/512 MiB decodificati. Nessun nuovo buffer PCM nelle correzioni. |
| Slice non distruttive | `detectSampleSlices`/`snapSamplePoint`, `Source/Sampler.cpp`; marker normalizzati, detector RMS con finestre di circa 5 ms e sensibilità, fino a 128 slice entro la mappatura MIDI. Il file originale non viene riscritto. |
| Editor, pad e MIDI | `Source/StudioWindow.cpp`: waveform/zoom, marker con drag/aggiunta/rimozione, zero crossing, tagli transitori o 4/8/16/32/64 parti uguali; banchi di 16 pad, tastiera, primo tasto slice. `WRITE SLICE MIDI` deriva il timing da durata sorgente e Source BPM. Nessun sequencer duplicato. |
| Pitch e durata | `SampleVoice`, `Source/Sampler.cpp`: resampling, transpose/fine/pitch wheel; sync con Signalsmith Stretch già incluso, Source BPM e BPM progetto indipendenti dall'intonazione. In modalità slice i tasti scelgono i tagli; transpose/fine cambiano il pitch. Le slice sono one-shot, quindi la durata MIDI non ne tronca l'audio. |
| Fade, reverse e loop | `SampleVoice`: fade in/out, ADSR e relative curve, reverse, loop con crossfade. Il loop interno del campione è disponibile in Classic/gate; Slices resta one-shot. I loop della timeline sono gestiti dallo scheduler esistente. |
| Pattern vocale | `Source/core/VocalChops.cpp`: parte opzionale, densità/gate/humanize, pattern deterministici e remapping delle slice; blocchi e loopSource conservati. Questo generatore non è modificato. |
| Persistenza e risorse mancanti | `samplerJson`/`readSampler` e collect/relink in `Source/StudioIO.cpp`; `vocalChops` opzionale in `Source/ProjectIO.cpp`. Default compatibili e nessun nuovo campo/formato. |
| Routing, anteprima, export | `Source/AudioEngine.cpp`, Studio e mixer esistenti: audition, monitor MIDI/audio, mute/solo, FX/automazione, seek, loop, sampler rate conversion, copie offline e stem. |
| Thread audio | `SampleSynth`: 32 voci e stretcher preparati prima dell'uso; blocchi stack di 256 frame. Settings copiati in dimensione fissa. Il callback usa ancora il lock interno JUCE preesistente; le modifiche non introducono lock, I/O, copie PCM o allocazioni. |

## Difetti riprodotti e intervento 02B circoscritto

L'utente ha autorizzato analisi e modifiche necessarie: questo intervento riusa il
sampler e le sue voci preallocate, senza cambiare architettura, editor o generatori.
Le funzionalità mancanti che richiedono interventi maggiori sono elencate sotto.

| Caso | Prima | Correzione e regressione |
|---|---|---|
| Detector con budget MIDI di una slice | Restituiva due marker al primo attacco. | Ritorno immediato dopo il marker iniziale quando il limite è uno. |
| Lettura fra slice a velocità frazionaria | Una slice completamente silenziosa leggeva la successiva: picco 0,5. | Bordi slice arrotondati al campione sorgente più vicino; interpolazione limitata al bordo del crop/slice e dell'asset. Picco del fixture ora zero. La correzione del reader è condivisa ma non cambia dati/editor di Vocal Studio. |
| Interruzione monofonica | Il voice stealing azzerava subito l'audio precedente: salto 0,6 anche con i fade predefiniti. | Breve rilascio di 2,5 ms usando una voce già allocata; le voci in rilascio non consumano il budget di polifonia suonabile. Totale sempre limitato a 32. Fixture con salto massimo 0,01125, anche in reverse e fra blocchi di dimensioni differenti. |
| Durata in sync e slice brevi | Il warmup consumava 1024 campioni della durata: ultimo frame 23999 anziché 25023; una slice di 240 campioni restava muta. | In Slices la durata parte dal clock di uscita dopo il warmup. Test: ultimo frame 25023, picco della slice breve 0,356506. Audio region e modalità legacy conservano il loro comportamento. |
| Panic MIDI CC120 | JUCE delegava a note-off con tail; le slice/one-shot lo ignoravano. | All Sound Off arresta subito le voci del canale, incluse quelle in rilascio. |

Il test sync ha impedito di consolidare una soluzione incompleta: il rilascio
va applicato sul clock di uscita. La prima correzione lasciava ancora un buco
silenzioso (ampiezza ponte 0, latenza 1024 campioni nel fixture). Il rilascio è ora
ritardato della stessa latenza: ampiezza ponte 0,6, senza quel buco. Anche il sampler
originale fallisce questa verifica: il confronto non attribuisce una nuova causa
indimostrata a difetti storici.

Le rampe rispettano i fade impostati dall'utente. Disabilitare gli attacchi/fade,
raggiungere il limite di tutte le 32 voci o inviare retrigger estremamente densi
può ancora produrre discontinuità. Nessuna promessa di assenza universale di click
o di qualità soggettiva: la misura riguarda i fixture descritti. La breve
sovrapposizione può alzare i picchi: lasciare margine di livello.

## Verifiche e confronto

Risultati finali Linux: build completa PASS; CTest **14/14 PASS** in 117,96 s;
UI completa predefinita PASS, incluso Studio (import/trim/Undo/routing/export);
regressione GUI Vocal Studio **22/22 PASS**. Nessun FAIL finale. Sono esclusi
l'input nativo opzionale, hosting VST esterno, dispositivi reali e ascolto.

CTest `vocal_chops`, tramite il test runner esistente
`fga_engine_tests --vocal-chops-test`, include i casi sopra, file mancanti/vuoti/non
audio, sostituzione fallita, rendering sincro di 8 battute a 125 BPM, esattamente
32 trigger nello scheduler, seek/loop, cambio a 150 BPM, salvataggio/riapertura,
immutabilità della sorgente ed esportazione di uno stem non silenzioso
(nessuna valutazione d'ascolto).

Confronto prestazioni: stessi test, stessi input sintetici, stesso compilatore e
oggetti; il solo sampler originale viene compilato da HEAD in una directory di
lavoro esterna e collegato al medesimo runner. Tre esecuzioni alternate per variante,
metriche e log in `/workspace/scratch/02-vocal-chops/performance.json`. La baseline
fallisce intenzionalmente le nuove regressioni. Tempo del fixture sincro, CPU del
processo e picco RSS sono misure locali indicative, non una certificazione realtime.
Mediane finali locali prima/dopo: fixture 8-bar 258.87 / 285.78 ms
(+10.4%); CPU totale runner 4752.52 / 4622.31 ms; picco RSS
48016 / 48236 KiB. Il fixture include la preparazione del synth e la
sovrapposizione delle voci; la CPU totale include tutti i test, quindi non è una
misura del costo del solo callback. Il confronto a tre esecuzioni risente del rumore
di scheduling/cache; il rilascio ora corretto mantiene inoltre più audio effettivo.
Nessun aumento del tetto di 32 voci e nessuna copia PCM aggiunta. Questi numeri non
certificano dispositivi reali o stress estremo.
PCM sorgente: 384032 byte; buffer di confronto 8-bar stereo: 5898240 byte per output.

## Limiti e interventi maggiori da pianificare

- Il caricamento da UI è ancora sincrono (`MainComponent::loadSampleFile`): decode,
  hash, picchi e detector possono bloccare l'interfaccia con file grandi. Sono fuori
  dal callback audio, ma non esiste ancora un job di importazione annullabile.
- Il limite di memoria è per file, non globale fra tutte le tracce; le copie offline
  condividono il PCM, ma importazioni indipendenti possono duplicarlo.
- Lo snap è a zero crossing sul canale sinistro; non esiste uno snap musicale dei
  marker a beat/bar né una ricerca multicanale. Tagli uguali e WRITE SLICE MIDI già
  usano il tempo sorgente; non sono uno snap musicale interattivo.
- Loop interno di ogni slice e scelta di un render offline di qualità superiore
  non sono implementati. Lo stretch usa il processore già disponibile, senza nuove
  dipendenze o modelli. Nessuna sintesi testo→canto viene aggiunta a Vocal Chops.
- Windows/macOS, ASan/TSan, dispositivo reale, stress estremo e ascolto di voci reali
  non sono validati qui. Qualità dei tagli/pitch/formanti va ascoltata su materiale
  autorizzato; niente voce protetta scaricata come fixture.

La fase 02A richiede una proposta/approvazione prima di modifiche maggiori:
prossimi candidati sono importazione asincrona con budget globale e snap musicale.
La fase 03 non è iniziata. Licenze esistenti di JUCE/Signalsmith e degli eventuali
campioni forniti dall'utente restano applicabili; nessuna dipendenza aggiunta.

## Prova manuale ancora da eseguire

1. Aggiungere VOCAL CHOPS, caricare una propria registrazione autorizzata e indicare
   il Source BPM corretto. Confermare che un caricamento fallito conservi la voce precedente.
2. Usare AUTO SLICE e rifinire i marker nel pannello Studio esistente; mantenere
   fade in/out brevi e non nulli, polifonia 1 per la prova dei retrigger.
3. Usare pad/tastiera e WRITE SLICE MIDI; modificare il pattern nel piano roll.
   In Slices la durata MIDI non tronca la one-shot: accorciare la slice tramite marker.
4. Provare 8 battute a 125 BPM, BPM Sync, transpose/fine, reverse e cambio a 150 BPM;
   ascoltare tagli, seek e loop. Provare anche Panic/All Sound Off sul canale giusto.
5. Salvare/riaprire, fare Undo/Redo e Collect portable project; esportare lo stem
   con la funzione esistente e confrontarlo con il playback e con la sorgente originale.
6. Registrare esito di ascolto e sistema: i test numerici non certificano qualità
   di voci reali né comportamento su Windows/macOS.

Coordinate principali del sampler verificato: `Source/Sampler.cpp:10` (juce::Result readSampleAsset), `Source/Sampler.cpp:23` (double snapSamplePoint), `Source/Sampler.cpp:25` (std::vector<double> detectSampleSlices), `Source/Sampler.cpp:41` (class SampleVoice), `Source/Sampler.cpp:181` (void SampleSynth::handleMidiEvent), `Source/Sampler.cpp:199` (void SampleSynth::noteOn).
