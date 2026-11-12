# Fase 07 — Mix Assistant locale

## Ambito e gate

Implementazione della fase `phases/07_MIX_ASSISTANT.md` sul solo Vocaltest.
L'intestazione interna del documento dice «PHASE 06»; file e indice ufficiale
lo assegnano alla fase 07. Dopo il preflight `6d56f17`, l'utente ha approvato
l'avanzamento lasciando aperta la validazione Windows/manuale del fix Cancel
`9c4e812`. Quella validazione **non** diventa PASS con questi test Linux.
Nessun avanzamento alla fase 08, push, merge, tag o release.

## Flusso implementato

Nel mixer, **MIX ASSIST** apre l'analisi di riferimenti WAV locali:

1. **Mix WAV**: scegliere il mix originale da esaminare.
2. Selezionare una traccia del progetto e **Add stem WAV** per associare uno
   stem già renderizzato. Le tracce interne Kick/Bass determinano quei ruoli;
   le altre sono confrontate come stem generici. Nessuna nuova sintesi vocale.
3. **Analyse** legge i file in un worker cooperativo, con progresso e Cancel.
4. Selezionare un suggerimento. **A: project** ripristina il mix del progetto;
   **B: suggestion** prova temporaneamente soltanto quella riduzione di gain.
   Nome del destinatario e variazione in dB sono visibili prima dell'approvazione.
5. **Apply selected suggestion** applica una sola proposta tramite mixer e Undo
   esistenti. Non esiste Apply All; le osservazioni senza intervento automatico
   non abilitano Apply. **Open existing EQ** riusa l'editor già presente.

**Sono misure dei file scelti, non del progetto corrente o del segnale live.**
Non viene creato un nuovo render, né clonato alcun plugin. Serve esportare o
fornire riferimenti corrispondenti al mix che si sta ascoltando; un WAV già
attenuato da Transparent o limitato non rivela il precedente sovralivello.
Gli stem devono partire dalla stessa origine temporale del mix; questa
corrispondenza musicale resta responsabilità dell'utente, non si deduce dal nome.

Nessuna rete, AI remota o caricamento di audio; niente nuove dipendenze/binari.
I file originali non sono scritti o incorporati nel progetto. Configurazione e
report dell'assistente sono temporanei, non nuovi campi del formato progetto.

## Misure, euristiche e limiti

- Picco campione e headroom in dBFS; RMS calcolato; LUFS integrati con il
  `LoudnessMeter` esistente. Non è una nuova misura True Peak o un target LUFS.
- Conteggio dei campioni quasi a fondo scala (`|x| >= 0.9999`): **rischio**,
  non prova di clipping precedente o promessa di recupero del segnale.
- Correlazione stereo `sum(LR)/sqrt(sum(LL)sum(RR))`; indefinita per mono/silenzio.
  Avviso di ascolto mono quando inferiore a -0,2. Nessuna correzione stereo.
- FFT Hann 4096 riutilizzata da `EqAnalyzer`, con potenze non smussate condivise
  con l'analizzatore grafico. Confronto in sei bande: 20/80/160/500/2000/6000/24000 Hz;
  Kick/Bass usa soltanto le prime due. Due stem devono essere contemporaneamente
  sopra -60 dBFS RMS; similarità coseno media >0,85 in almeno il 10% dei blocchi
  del mix. Pause disgiunte e bande distanti sono coperte dalle regressioni.
- Proposte prudenti: attenuazione master verso 3 dB di headroom campione o prova
  di -1,5 dB su Bass/secondo stem. Nessun boost, EQ automatico, modifica a note,
  arrangiamento, plugin o routing. Somiglianza spettrale **non** significa mix errato;
  dipende dal materiale e deve essere valutata con ascolto e confronto di volume.

Input limitati a un mix e 32 stem, WAV mono/stereo, 8–192 kHz, massimo 15 minuti,
uguale sample rate. Gli stem possono essere più corti (coda silenziosa), non più
lunghi del mix. Niente ricampionamento implicito; errori espliciti per file
mancanti/non WAV, NaN/Inf, ampiezze non analizzabili, binding duplicati/invalidi,
limiti superati e file cambiati per dimensione/data durante il job.

## Stato, memoria e thread

Il worker possiede reader, FFT e buffer; nessuna nuova chiamata nel callback audio.
Lettura a blocchi di 4096 frame; memoria dei campioni/FFT indipendente dalla
lunghezza del brano (circa 80 KiB per input nei buffer visibili, **oltre** a backend
FFT, reader, risultati e GUI). Confronti limitati a 496 coppie per 32 stem.
Il costo cresce con numero di file, durata e sample rate: non è garantito che
un computer sotto carico non abbia dropout mentre analizza.

Cancel segnala il worker, non fa join sul message thread. Chiusura annulla e
rimuove la callback prima del ritiro con `RetiredJobs`; completamento soltanto
dopo l'uscita del worker. Eccezioni/errori non producono un report dichiarato valido.
Le normali letture filesystem in corso devono comunque ritornare prima dell'uscita.

A/B pubblica una copia delle sole impostazioni già supportate tramite `updateMix`;
non modifica dati del progetto, dirty o Undo. A/chiusura/cambio suggerimento
ripristinano il progetto. Cambi di revisione UI o operazioni audio riservate
invalidano l'anteprima; dopo modifiche al progetto occorre analizzare nuovamente
prima di applicare altre proposte. Ogni proposta è applicabile una volta per analisi.
I destinatari usano gli ID transitori delle tracce esistenti. Apply conserva
checkpoint e `mixChanged`; niente formato 8, migrazione o modifica ai vecchi file.

AudioEngine, Transparent/True Peak, PDC, compressore, sidechain, bus/mandate,
generatori e moduli vocali non modificati. L'unica estrazione dal codice precedente
è la FFT non smussata di `EqAnalyzer`; algoritmo della trace grafica invariato.

## Verifica Linux

| Verifica | Esito |
|---|---|
| Build nativa Release GCC 14, app + test + fixture VST3 | PASS |
| CTest completo finale | **19/19 PASS**, 191,81 s |
| UI generale finale/Xvfb e nuovo assistente | PASS: A/B senza variazioni dati/Undo/dirty; Apply master/traccia, Undo/Redo; invalidazione; worker Cancel/retry/errore/chiusura |
| Export UI in isolamento | **43/43 PASS**, inclusi Cancel, retry, stato/PDC, overwrite e 18 stem |
| VST3 effetto nativo reale di test | PASS script `test-native-effects.py`, stato/ordine/automazione/bypass/Undo e PCM verificato indipendentemente |
| Windows/macOS, hardware/ascolto, plugin commerciali, nuovi sanitizers | NOT RUN |

Comandi: `cmake --build build/native -j2`; `DISPLAY=:115 ctest --test-dir
build/native --output-on-failure -j1`; app `--ui-test` (nel controllo finale con
`FGA_SKIP_EXPORT_UI_TEST=1`, perché export verificato separatamente),
`--export-ui-test`; `scripts/test-native-effects.py --build-dir build/native`.
Il test `mix_assistant` è registrato in CTest. `FGA_MIX_VOCAL_FIXTURE` consente
il controllo opzionale di un WAV vocale già renderizzato: eseguito qui sul
file locale `english-guide_122bpm_A.wav` (48 kHz stereo, 81,69 s), associato a
un AudioTake del progetto fixture e riaperto tramite JSON. Nessuna sintesi nuova,
modifica ai moduli vocali o valutazione della qualità della voce.

Fixture deterministiche: silenzio, tono 0,8, antiphase, PCM a fondo scala,
voce armonica sintetica, kick/bass simultanei/disgiunti, bande separate/simili,
44,1/48/96 kHz e mono. Verificati NaN float, rate incongruenti, file mancanti o
cambiati, stem duplicati/oltre durata, input oltre limite, Cancel e nuovo tentativo,
hash WAV e progetto invariati, salvataggio/riapertura del riferimento vocale.
Il tono 0,8 dà circa -1,94 dBFS e proposta master -1,06 dB; correlazioni +1/-1;
LUFS mono inferiori allo stereo duplicato di 3,0103 dB (tolleranza 0,01).

Un primo test UI **FAIL** ha avuto 41/43 controlli export PASS: i 18 stem non
avevano terminato entro il timeout esistente di 30 s, mentre CTest girava in
parallelo. È avvenuto prima di aprire Mix Assistant; lo stesso timeout era già
registrato nel report Cancel. Tutti i casi Cancel passavano. Ripetizione isolata
**43/43 PASS**, nessun timeout aumentato né correzione export rivendicata.
Un primo FAIL della nuova fixture dipendeva da ID di test hardcoded in collisione:
il binding è ora ottenuto dalle tracce reali; non era un bug dei progetti.

Misure locali del processo test completo, **non** del callback o del solo worker:
fixture brevi 0,290 s wall / 0,283 s CPU, picco RSS 10.368 KiB; aggiungendo progetto
con guida già renderizzata (mix + stem, 81,69 s) 2,389 s / 2,377 s CPU, RSS
10.496 KiB. Comprendono generazione/IO fixture, JSON/hash e più analisi; non sono
un benchmark DAW prima/dopo né un massimo di memoria per 33 file. La durata del
WAV non viene caricata per intero. Nessuna latenza DSP nuova; regressioni audio
esistenti misurano zero allocazioni C++ ordinarie nel callback (EQ, compressore,
sidechain, return e integrazione). Il profilo sidechain locale registra 1.024.000
frame stereo in 136,70 ms: workload del test, non prova di assenza di dropout.

Log/screenshot fuori repository: `/workspace/scratch/mix-phase07/`, inclusi
`ctest-final.log`, `ui-final/mix-assistant-ui.txt`, `ui-final/mix-assistant.png`,
`export-isolated/export-ui.txt`, `native-effects/` e `performance-final.json`.
Nessun nuovo bug DSP preesistente riprodotto. La validazione funzionale e sonora
resta distinta dal PASS tecnico Linux; nessuna decisione architetturale pendente.

## Collaudo pratico da completare

1. Salvare una copia di un progetto legacy/formato 7; ascoltare il mix A.
2. Aprire MIX ASSIST; scegliere mix originale e stem allineati, compresa una voce
   già renderizzata. Leggere le misure e confrontare con meter indipendenti.
3. Scegliere una proposta: ascoltare A/B a volume comparabile. Verificare che
   master/traccia, routing, pan, compressore e sidechain restino quelli attesi.
4. Apply: controllare il solo gain indicato, Undo/Redo, salvataggio e riapertura.
   Una nuova modifica al progetto deve disattivare una vecchia anteprima/proposta.
5. Avviare e annullare Analyse, ripetere, chiudere la finestra durante l'analisi;
   verificare ripartenza, reattività e playback con il proprio hardware/plugin.
6. Esportare Transparent e annullare realmente con il mouse, anche con VST3 e
   A/B attivi; controllare WAV precedente, file temporanei e nuovo export.

Windows x64 nativo/macOS, mouse/DPI reali, ascolto/qualità dei suggerimenti,
hardware/dropout e plugin commerciali restano NOT RUN per questa fase. Il workflow
MSVC esiste ma non può consumare questi commit locali senza push autorizzato.
Nessun nuovo sanitizer dichiarato eseguito. Vocals congelati e non approvati.
