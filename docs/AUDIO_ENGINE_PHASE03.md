# Fase 03 — audit Audio Engine, 2026-11-11

Branch `Vocaltest`, base `c1388eefc63ac10e43de701e55ff63e46e1174f4`.
La fase 03 è autorizzata dalla decisione attuale dell'utente. Vocal Studio e
Vocal Chops conservano implementazione e test tecnici storici, ma non hanno
approvazione funzionale/qualitativa: ulteriori interventi e revisione finale
sono rinviati al termine delle altre fasi. Nessun loro codice modificato.

## Audit dell'esistente

| Area | Implementazione riutilizzata e verifica |
| --- | --- |
| Callback | `Source/AudioEngine.cpp`, `audioDeviceIOCallbackWithContext`/`render`: segmentazione al buffer preparato e ai confini del loop; uscita durante manutenzione/export; sanitizzazione NaN/Inf. Nessuna modifica al callback. |
| Stati | `Source/AudioEngine.h`, `Handoff`: pubblicazione SPSC, migrazione dei riferimenti prima del ritiro, raccolta sul lato controllo; test deterministico di lifetime già presente. Aggiunto stress concorrente di 10.000 stati, senza distruzioni sul consumer audio e con conteggio completo del rilascio. |
| Plugin | `swapInstrument`, `capturePluginStates`, `restoreHosted`: preparazione/sostituzione sul controllo, try-lock per il processamento, recupero MIDI dopo sospensione. Test esistenti per snapshot lento, stato/programmi e note sostenute riutilizzati. |
| PDC | `prepareDelayLines`/`refreshLatencies`: latenza sorgente + insert, limite esplicito, allocazione differita sul controllo e svuotamento logico dopo seek/loop. Regressioni esistenti su impulsi, bypass, latenza dinamica e capacità mantenute. |
| Audio e voce guida | `AudioInput.cpp`, `AudioTake`, sampler e `offlineCopy` già gestiscono cattura, regioni, playback e copia privata. Riutilizzati i test esistenti; nessuna nuova integrazione vocale. |
| Export | `exportAudio` usa lo stesso grafo e file temporanei atomici; annullamento e ripristino RAII. Rafforzato il test della copia privata: il callback live deve avanzare durante l'export, non solo conservare `isPlaying`. |

Non è emersa la necessità di ricostruire questi componenti, cambiare schema
`.fga`, motori musicali, generatori o dipendenze.

## Difetto riprodotto e correzione minima

Durante un export alla frequenza del dispositivo, simulare un riavvio con buffer
512 → 1024. Il codice precedente aggiornava `deviceBlock`, poi il ripristino
aggiornava `blockSize` senza ripreparare plugin/buffer: decideva soltanto in base
al cambio di frequenza richiesto **all'inizio** dell'export. La nuova regressione
fallisce con `export completion/cancellation must reprepare plugins for a changed
device rate/buffer` (exit 1). Riprodotto lo stato incoerente, non un crash hardware.

Il ripristino ora confronta frequenza/buffer effettivi dell'export con quelli
attuali del dispositivo e riprepara plugin, DSP e PDC sotto il gate di controllo
prima di restituire il motore al callback. Le latenze input/output vengono
registrate anche quando il riavvio avviene durante l'export; il ritorno anticipato
precedente le lasciava obsolete. La preparazione dell'export resta invariata
durante il cambio. Nessuna allocazione/lock aggiunti al callback.

## Test locali

Ambiente: Linux x64, GCC 14, Release, JUCE 8.0.9. `AuditDevice` e plugin probe
sono fixture, non driver o VST commerciali. Le nuove verifiche sono incluse nel
target e nel test `audio_engine` esistenti, senza harness/dipendenze duplicati.

- Baseline `ctest --test-dir build/native -R '^audio_engine$'`: PASS, 11,83 s.
- Regressione nuova sul codice precedente: FAIL atteso, exit 1, messaggio sopra.
- Primo `--engine-audit` corretto: PASS, sei combinazioni di ripristino
  (solo buffer, sola frequenza, entrambi × completamento/annullamento).
- Matrice callback: PASS, 15 combinazioni 32/64/128/512/1024 frame ×
  44,1/48/96 kHz; loop con confine interno al blocco, seek/chase, mute/solo,
  24 pubblicazioni MIDI per configurazione, swap, stop e restart, campioni finiti.
- Stress Handoff concorrente: PASS, 10.000 stati rilasciati, dati integri e
  nessuna distruzione sul consumer audio.
- Build Release completa dell'applicazione e dei test: PASS. Rimangono warning
  non bloccanti (confronti floating point e API JUCE deprecate); nessun errore.
- Suite finale `ctest --test-dir build/native --output-on-failure -j2`: PASS,
  15/15, 64,11 s totali; `audio_engine` 16,97 s con i nuovi casi. Inclusi
  `project_io`, regressioni sample import/chops, PDC/plugin/audio take,
  MIDI/generatori esistenti e `vocal_guide` con helper locale configurato.
  Si sono eseguite le suite vocali esistenti per regressione, senza svilupparle
  né interpretarne il PASS come approvazione qualitativa.
  Il test della copia privata ora richiede avanzamento del callback live
  durante il progress dell'export: interleaving deterministico, non una misura
  di parallelismo o scheduling realtime del sistema operativo.

Log locali fuori repository: `/workspace/scratch/phase03-*.log`.
I tempi sono osservazioni di questo runner, non misure di latenza/CPU su hardware
utente. La durata baseline/finale non confronta prestazioni a carico uguale:
sono stati aggiunti rendering e stress test. Non è stata aggiunta elaborazione
per campione al percorso realtime.

## Limiti e verifica manuale ancora necessaria

Le nuove modifiche non sono state compilate/eseguite su Windows o macOS.
Il precedente artifact Windows riguarda un altro commit e non valida questa patch.
ASan/TSan, driver fisici, ASIO/CoreAudio, ascolto/click/dropout e plugin di terze
parti reali non sono stati verificati in questo intervento. Le suite automatiche
non costituiscono approvazione qualitativa delle funzioni vocali.

Su hardware reale: aprire una copia di un progetto esistente; provare play/stop,
seek, loop, mute/solo e sostituzioni plugin alle frequenze/buffer disponibili;
ascoltare le regioni WAV già presenti; esportare/annullare e verificare il
playback successivo, anche cambiando configurazione audio quando consentito.
Riaprire il progetto salvato e verificare note, regioni, routing e automazioni.
Non serve generare nuove voci né intervenire sui Vocal Chops sospesi.

Stato: audit e prima correzione della fase 03; collaudo reale ancora pendente.
Nessuna modifica architetturale da approvare in questa correzione. Nessuna fase
successiva avviata; nessun push, tag, workflow dispatch o release autorizzato.
