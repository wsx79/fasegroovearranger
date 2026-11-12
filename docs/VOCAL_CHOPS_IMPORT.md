# Vocal Chops — gate 02A approvato: import e snap

Branch `Vocaltest`, baseline `8cc548708394609a02182d64ad1609df52b4d322`.
Questo intervento completa i soli miglioramenti approvati della fase 02;
non avvia la fase 03. Le cinque correzioni DSP precedenti restano invariate.

## Importazione e ciclo di vita

- `SampleImportJob` (`Source/SampleImport.*`) prepara decode, SHA256, picchi,
  eventuali slice e le 32 voci del sampler su un worker. Decode a blocchi di
  16384 frame, fingerprint a blocchi limitati e picchi verificano l'annullamento.
  Detector e preparazione DSP terminano prima della pubblicazione: annullare
  scarta il risultato, senza terminare forzatamente il thread.
- `Source/SampleImportController.cpp` serve file chooser, browser/drop, preset,
  restore dei campioni e registrazioni AudioTake del pannello Studio esistente.
  I risultati sono pubblicati con release/acquire; il worker attende il passaggio
  di proprietà. Il timer UI convalida identità della traccia, sorgente/settings
  iniziali e frequenza dispositivo; solo il successo crea un checkpoint.
- Una richiesta nuova, Undo, restore, eliminazione della destinazione o modifica
  dei suoi parametri annullano/scartano il lavoro obsoleto. Le modifiche MIDI
  durante il normale import restano conservate. L'aggiunta di un AudioTake, che
  prepara una copia del progetto, è scartata se il progetto cambia nel frattempo.
- `AudioEngine::installPreparedSample` effettua il solo trasferimento sul thread
  di controllo con il protocollo EngineControlLock esistente: il callback non
  attende il controllo e può emettere silenzio durante la breve transazione.
  Nessun decode/hash/preparazione voci viene aggiunto al callback. La gestione
  degli editor/stati VST resta quella preesistente; tempi arbitrari dei plugin
  esterni non sono certificati da questi test.
- Il vecchio sampler e le risorse preparate/scartate sono ritirati dal worker,
  tramite RetiredJobs già presente. Il refresh della waveform precede il rilascio
  del vecchio sampler. Le risorse condivise restano vive finché esiste un owner.
- LOAD SAMPLE della DAW diventa CANCEL LOAD durante l'import; Studio aggiunge
  progressione, CANCEL LOAD e LIMITS. La griglia usa la waveform esistente;
  l'altezza minima è preservata usando lo scorrimento del pannello.
  nuovo restore asincrono dopo Redo, prima di esportare. L'API sincrona AudioEngine::loadSample resta

## Limiti e persistenza

LIMITS salva preferenze locali: file codificato 1–512 MiB, PCM/picchi per campione
1–512 MiB, durata 1–1800 secondi, totale 64–2048 MiB. Default ai massimi.
Valori non finiti/fuori intervallo sono normalizzati prima delle conversioni.
Il controllo avviene prima dell'allocazione del PCM; una prenotazione atomica
impedisce a import concorrenti di superare il budget.

Il totale conta il **PCM decodificato da file e i picchi**, inclusi asset in attesa,
vecchi campioni mantenuti durante la sostituzione e asset condivisi con render
ancora vivi. Non è un limite del RSS dell'app: DSP/stretcher, codec, allocator,
registrazioni live, preview e freeze hanno memoria aggiuntiva. Non viene aggiunta
una copia del PCM. In caso di rifiuto l'errore indica il limite; sorgente precedente,
MIDI e Undo restano invariati. Le prenotazioni cessano dopo la liberazione effettiva.

I progetti precedenti mantengono riferimenti e contenuti. Campioni oltre i limiti
attuali restano referenziati ma non caricati, con segnalazione di risorsa mancante;
nessuna cancellazione o riscrittura dell'originale. Abbassare i limiti non espelle
asset già caricati; la nuova policy si applica agli import successivi.

## Griglia musicale

Campo opzionale `SamplerSettings::sliceSnapBeats`, serializzato dal project/preset
serializer esistente in `Source/StudioIO.cpp`. Assente = 0, comportamento legacy.
Snap off consente il marker esatto; Zero crossing conserva l'opzione precedente.
Griglie: 1/32, 1/16 terzina, 1/16, 1/8 terzina, 1/8, beat, 2 beat,
1/2/4 battute (la battuta del progetto è di 4 beat).

`snapSlicePoint` usa origine nel trim start e passo sorgente
`gridBeats * 60 / projectBpm * speed / sampleSeconds`.
`speed` è il rapporto BPM progetto/Source BPM in Tempo Sync, altrimenti il
resampling dovuto a transpose/fine. Le slice selezionano il taglio via MIDI,
quindi la nota trigger non cambia la griglia. Pitch wheel realtime non incluso.
Aggiunta, drag e Auto slice usano questa funzione; normalizzazione elimina i
marker duplicati. Non si forza un drag fuori dai marker adiacenti né si aggiunge
una slice terminale vuota. Selezionare una griglia o cambiare BPM non riscrive
automaticamente i marker esistenti. Undo/Redo preserva impostazioni e MIDI.

## Verifica manuale

1. Aggiungere VOCAL CHOPS e caricare un proprio WAV autorizzato, anche dal browser
   o tramite drop. Durante l'import muovere/selezionare la UI e provare CANCEL LOAD:
   la sorgente precedente deve restare utilizzabile. Ripetere e attendere il successo.
2. In Studio > LIMITS impostare un limite file inferiore alla dimensione del WAV:
   riprovare e controllare errore, sorgente e Undo invariati. Ripristinare il limite.
3. A 125 BPM scegliere Beat o 1/16, aggiungere con Shift-click e trascinare marker;
   provare Source BPM/Tempo Sync, quindi 150 BPM e nuovi tagli. I tagli già presenti
   restano dove erano. Snap off deve permettere tagli fra le posizioni di griglia.
4. Provare pad, WRITE SLICE MIDI, Undo/Redo, salvataggio/riapertura e stem export.
   Ascoltare fade/retrigger e confrontare l'originale, che non deve cambiare.

## Risultati della verifica

Risultati finali locali; non estendere le misure di fixture sintetici a hardware
o file arbitrari.

Ambiente: Linux x86_64, kernel 6.18.57, 3 CPU virtuali Intel Xeon Platinum 8573C,
limite container 8 GiB; GCC 14, JUCE 8.0.9, build Release. Nessuna dipendenza nuova.

- **PASS** build completa e `git diff --check`.
  include anche il render reale/cache della guida precedente.
- **PASS** `sample_import`: cancellazione decode/hash, restituzione delle
  prenotazioni, limiti file/PCM/durata/totale, due richieste concorrenti entro
  budget, validazione preset, passaggio del synth preparato, frequenza dispositivo
  cambiata senza sostituzione/checkpoint, griglia a 125/150 BPM, sync, pitch,
  terzine/battute/origine trim e default legacy/round trip.
- **PASS** `vocal_chops`: tutti i cinque bug precedenti, one-shot/sync, 32 trigger
  in 8 battute, seek/loop, MIDI/persistenza/sorgente intatta e stem non silenzioso.
- **PASS** GUI completa (`--ui-test`), inclusi Studio, controlli griglia/off/Undo,
  browser/drop sulla traccia corretta senza fermare o spostare il playback,
  registrazione e import AudioTake, raccolta, export e layout minimo.
- **PASS 11/11** GUI `--sample-import-test`: ritorno asincrono, heartbeat della
  UI, checkpoint unico/MIDI conservato, Undo/Redo, nessun fallback interno mentre
  il sample restore è in attesa, errore limite, annullamento e risultati obsoleti.
- **PASS 22/22** GUI `--vocal-studio-test`, inclusi render reale/import/Undo/Redo/export.

Le prime prove GUI hanno rilevato attese sincrone obsolete nell'harness di
browser/drop/Redo e un'altezza waveform ridotta dalla nuova riga. L'harness ora
attende l'import effettivo; l'altezza minima originale è ripristinata, senza
allentare le asserzioni. Le esecuzioni finali sopra non contengono FAIL.

**NOT RUN** Windows/macOS, input nativo opzionale, hosting VST esterno,
ASan/TSan, dispositivo audio reale, stress estremo e ascolto di voci reali.
Le verifiche numeriche non certificano qualità sonora né latenze realtime
su altri sistemi. Il vecchio FAIL storico della cache 01C non riprodotto resta
senza causa provata: non viene dichiarato risolto da questo intervento.

## Prestazioni prima/dopo

Tre esecuzioni alternate del runner `--vocal-chops-test`, prima dal binario
conservato a baseline 8cc5487 e dopo dal nuovo build; test VocalChopsTests invariato.
Nessuna build/suite parallela durante le misure. Dati grezzi, SHA256 dei binari,
ambiente e metodo: [VOCAL_CHOPS_IMPORT_PERFORMANCE.json](VOCAL_CHOPS_IMPORT_PERFORMANCE.json).

| Mediana locale | Prima | Dopo |
|---|---:|---:|
| Fixture sync 8 battute, inclusa preparazione synth | 273,663 ms | 278,702 ms (+1,8%) |
| CPU user+system dell'intero runner (`wait4`) | 4407,237 ms | 4457,145 ms (+1,1%) |
| Picco RSS del runner | 47664 KiB | 47692 KiB (+28 KiB) |
| Latenza del fixture sync | 1024 campioni | 1024 campioni |

Non emerge un peggioramento sensibile in questo carico. I tempi variano con
scheduling/cache; CPU del runner e tempo del fixture non isolano il callback.
Il limite di 32 voci, il PCM condiviso e la latenza DSP non sono stati aumentati.

Tre processi GUI separati su WAV sintetico stereo 48 kHz/32 bit di 8 secondi:
preparazione sincrona di riferimento **58,271 ms**, dispatch asincrono **0,529 ms**,
completamento import **91,161 ms** (mediane). La richiesta torna subito e la UI
elabora messaggi mentre il worker lavora; il completamento include preparazione,
polling e pubblicazione. Il riferimento sincrono usa lo stesso decoder/DSP attuale
sul thread chiamante, non il vecchio eseguibile: misura il lavoro spostato fuori
UI, non una velocizzazione del decoder. Non sono misure di file massimi, codec
arbitrari o garanzie di latenza su dispositivi reali.
