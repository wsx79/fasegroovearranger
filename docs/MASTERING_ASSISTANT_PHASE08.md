# Fase 08 — Mastering Assistant locale

## Ambito

Base `316d11ba2bf15ec00ec6e25aee0c80aeea55d6b4`, branch esclusivo Vocaltest,
working tree inizialmente pulita. Documento effettivo:
`phases/08_MASTERING_ASSISTANT.md` (intestazione interna «PHASE 07», ma indice e
nome file assegnano questa attività alla fase 08). Nessun avvio della fase 09.

Inventario mirato: catena master e host seriale, compressore, EQ delle tracce,
FFT/metri, player del browser, worker cooperativo, export Transparent e scanner
True Peak già presenti. Mancavano confronto reference normalizzato, target/QA
contestualizzati e un pannello mastering. Nessun nuovo bug DSP precedente
riprodotto; niente nuovo limiter, renderer, mixer o formato di progetto.

L'EQ nativo attuale è **delle tracce**: il suo editor clampa il destinatario a
una traccia valida. Non è stato presentato come EQ master né esteso nel DSP.
Per il master si riusa la catena insert esistente, dove l'utente può caricare
un proprio plugin EQ; il compressore nativo esistente resta disponibile lì.

## Funzioni e criteri della roadmap

| Requisito | Implementazione |
|---|---|
| Reference locale opzionale | Mix WAV + reference esplicita; misure e confronto senza rete/upload |
| A/B normalizzato | Estratti fino a 10 s, offset A/B indipendenti, LUFS misurati dopo fade di 5 ms, sole attenuazioni verso un livello comune compatibile con il ceiling |
| Target configurabili | LUFS opzionale (inizialmente disattivato, -30…-5), ceiling -12…-1 dBTP; sample rate/bit depth attesi facoltativi |
| Dinamica/headroom | Picchi, margine TP, RMS/crest factor; LUFS e confronto con reference. Non viene dichiarata una misura LRA |
| Suggerimenti tonali | Energie relative in sei bande riutilizzando FFT esistente; differenze >3 dB qualificate come dipendenti dal materiale |
| Preview reversibile | Player browser già esistente, Stop/chiusura/invalidation; nessuna modifica del progetto, dirty o Undo |
| Master/EQ/dynamics | Pulsanti Track EQ e Master inserts aprono gli editor esistenti; eventuali edit manuali conservano i loro Undo/bypass |
| Export QA | WAV/stem scelti: formato, rate/depth richiesti, durata, rischio clipping e ceiling; Export apre il menu esistente |
| Voci | Stem esplicitamente marcato: controllo del segnale non silenzioso; assenza/silenzio segnalati se richiesto |
| Oversampling/latency | Riutilizzato scanner TP16; verifica QA indipendente TP32; anteprima a rate coincidente, nessuna nuova PDC o offset |

Queste sono misure dei **file scelti**, non analisi live del progetto o del mix
prima del suo export. Il pannello non renderizza il progetto né applica preset
automatici. Per ricontrollare un cambiamento occorre esportare e selezionare il
nuovo WAV. I target del pannello non cambiano le preferenze storiche dell'export.

Un WAV già limitato/quantizzato non dimostra l'assenza di clipping precedente.
Lo stem marcato contiene o non contiene audio: non si identifica una cantante,
non si certificano intelligibilità/intonazione e **non si prova automaticamente
che quello stem sia incluso nel mix finale**. Questo controllo richiede ascolto.
Nessun codice dei moduli vocali congelati è stato modificato.

## Precisione, limiti e sicurezza

- Analisi locale a blocchi 4096, massimo 15 minuti/file e 32 stem. Lettura
  mono/stereo; gli stem devono avere lo stesso rate del mix e non superarne la
  durata. Devono essere allineati dall'utente: origine musicale, completezza
  della lista e somma effettiva nel mix non si deducono automaticamente.
- Mix mono misurabile, ma QA segnala che il master atteso è stereo. Stem mono
  ammessi. A/B richiede mix/reference stereo allo stesso sample rate; l'analisi
  tonale/loudness può confrontare rate differenti, senza abilitare quella preview.
- Target LUFS non universale. Si mostrano delta richiesto e gain uniforme massimo
  compatibile con il ceiling: un target irraggiungibile non abilita Loud e non
  viene promesso come ottenuto. Il percorso Transparent rimane identico.
- Scanner 16x di `OfflineExport.cpp` riusato tramite una funzione pubblica di
  **sola lettura**; codice della scansione, guard, encoding e Transparent invariati.
  Picchi mono/estratti adattati come reader, senza duplicare un oversampler/limiter.
- A/B applica fade agli estremi degli estratti, misura e attenua, verifica TP
  finale e LUFS finali entro 0,1 LU; se il gating rende il match inaffidabile o
  un estratto è silenzioso/corto, confronto disabilitato con motivazione.
- Preview a trasporto fermo, monitoring disattivato e dopo le code; dispositivo
  allo stesso rate dei due file. Non si usa il ricampionamento lineare del browser
  per questo confronto. A/B riavvia l'estratto: **non** è un crossfade continuo.
  Il player è post-master: non elabora di nuovo i file con gli insert del progetto.
  Il livello storico del browser (0,8, circa -1,94 dB) è comune ai due lati;
  il LUFS mostrato del match è quello dei buffer prima di tale livello comune.
- Nessun accesso a file/FFT/decode/copia pesante nel callback o nel message thread.
  Anche la copia per il player viene preparata dal worker. Gli estratti residenti
  sono al massimo 2 × 10 s × 192 kHz × 2 × 4 = 30.720.000 byte; una copia pronta
  per il player aggiunge fino a 15.360.000 byte. Backend FFT/reader, vecchi handoff,
  progetto, GUI e processo sono extra: non è un limite RAM totale della DAW.
- Cancel cooperativo tramite `RenderProgressTask` e `RetiredJobs`, niente join UI.
  Chiusura annulla e stacca la callback; errori/eccezioni non producono un PASS.
  Letture filesystem in corso devono comunque restituire il controllo.
- Dimensione/data dei WAV controllate durante analisi e prima della preparazione
  della preview. Cambi a target, progetto o file invalidano il confronto.
- Un seriale di proprietà, incrementato solo nei controlli start/stop del player,
  evita che chiudere il pannello arresti un'anteprima successiva del browser.
  Nessuna operazione aggiunta al callback audio; getter read-only del monitoring.

Per rendere disponibili le energie medie si aggiunge un campo transitorio a
`MixMeasure`: sei somme per blocco e divisione finale, fuori callback. Misure,
suggerimenti e UI di MIX ASSIST sono preservati e coperti dalla sua regressione.
Nessun nuovo dato serializzato o migrazione; legacy e formato 7 restano invariati.

## Verifica

| Test | Esito |
|---|---|
| Build Release GCC 14, app/test/fixture VST3 | PASS |
| CTest fase 08: `mastering_assistant`, `mastering_peak32` | **2/2 PASS**, finale 17,76 s |
| CTest pertinenti: EQ, project IO, engine, MIX ASSIST, sidechain, dynamics, offline export, browser | **8/8 PASS**, 146,33 s |
| UI dedicata Xvfb: `--mastering-ui-test` | **17/17 PASS**, inclusi editor reali, Cancel/retry/chiusura e conservazione progetto/Undo/dirty/PDC |
| VST3 effetti nativi reali: `test-native-effects.py` | **79/79 PASS**, PCM/stato/ordine/bypass/Undo/export master |
| Windows/macOS, ascolto/hardware/dropout, vendor plugin, nuovi sanitizers | NOT RUN |

Comandi: `cmake --build build/native -j2`; CTest selezionati con
`DISPLAY=:116`, `--output-on-failure -j1`; app `--mastering-ui-test <cartella>`;
`python3 scripts/test-native-effects.py --build-dir build/native --output <cartella>`.
Nessun audit completo dei generatori o delle fasi vocali ripetuto. Il corpus
esteso della fase 04 non è stato rieseguito; il pertinente CTest offline esistente sì.

Fixture: reference opzionale, target disattivato/irraggiungibile, silenzio,
mono/stereo, stem/vocal mancanti o silenziosi, rate/depth incongruenti, duplicati,
input oltre limite, stem troppo lunghi, offset fuori brano, file mancanti/cambiati,
44,1/48/96/192 kHz, estratti limitati a 10 s (provato a 8 kHz), originali invariati,
Cancel e ripartenza. È stato ispezionato anche un WAV guida già renderizzato di
81,69 s/48 kHz, selezionato con `FGA_MIX_VOCAL_FIXTURE`, senza nuova sintesi o
approvazione dei moduli vocali.

Sul materiale equivalente i due estratti misurano **-20,0021 LUFS**, residuo
massimo tra i buffer normalizzati **1,04308e-7**. Il player conserva il suo gain
storico e non aggiunge offset campione/PDC (residuo test <1e-7 al rate coincidente).
Verificati endpoint dei fade e seriale di proprietà invariato durante callback.

Verifica indipendente FFmpeg **32x 6/6 PASS**, 44,1/48/96 kHz, padding zero
esplicito ai bordi, FIR 1024/phase 10/cutoff 1. Il tono intersample con sample peak
circa -3,10 dBFS ricostruisce **+0,000683 dBTP** (TP16 circa +0,000801 dBTP):
il rischio è rilevato anche senza campioni PCM a fondo scala. Gli estratti
normalizzati risultano **-1,049978 / -1,049978 / -1,049982 dBTP** con ceiling -1.
È QA numerica su fixture, non una certificazione per qualsiasi contenuto.
`mastering_peak32` viene registrato quando Python/FFmpeg sono disponibili; i due
CTest di fase devono essere eseguiti insieme, perché il primo produce il corpus.

Misure locali in processi dedicati, `FGA_MASTERING_PROFILE=1`: l'unico probe
AudioEngine del test è escluso per misurare l'analisi/file IO senza il costo del
motore; tutte le altre asserzioni della fixture restano attive. Fixture base:
**14,510 s wall / 14,496 s CPU / 9.916 KiB RSS**; aggiungendo il WAV guida lungo:
**21,007 s / 20,825 s / 9.040 KiB RSS**. Sono intere suite di fixture (più file,
writer/IO WAV e FFT/TP ripetuti), non costo di una singola analisi,
non massimo a 192 kHz × 10 s e non confronto RAM/CPU prima-dopo della DAW.
La variazione RSS fra processi non dimostra un risparmio di memoria. Il test
normale senza quel flag comprende invece il probe player/latency, ed è PASS.
Le regressioni callback esistenti misurano zero allocazioni C++ ordinarie per
EQ, compressore, sidechain e return. Nessun nuovo buffer/FFT o operazione nel
callback; costo di proprietà +8 byte atomici per engine, aggiornati fuori audio.
Nessuna promessa di assenza di dropout durante un'analisi pesante: va collaudata
sulla propria macchina.

Un errore di compilazione iniziale della nuova UI (`juce::File = {}` ambiguo)
è stato corretto con costruzione esplicita. Uno screenshot ha riprodotto testo
mojibake nei nuovi literal; corretti in ASCII e nuova UI verificata. Non vengono
rivendicate correzioni a DSP, limiti o bug Windows preesistenti.

Evidenze fuori repository: `/workspace/scratch/master-phase08/` con build,
`mastering-final.log`, `regressions.log`, `regressions-details.log`,
`ui-final/mastering-ui.txt`, screenshot, `native-effects/`, `performance.json`.
Corpus/verifica indipendente in `build/native/mastering-qa/verification32.json`.
Nessun nuovo binario, WAV o componente esterno aggiunto al repository.

## Prova pratica Windows da completare

1. Aprire una copia di un vecchio progetto/formato 7, ascoltare e salvare/riaprire.
2. Esportare con il menu esistente, preferibilmente Transparent e ceiling scelto;
   aprire MASTER ASSIST nel mixer e selezionare quel WAV.
3. Aggiungere stem allineati e, se presente, marcare quello vocale. Attivare
   Expect vocal stem; provare anche assenza e silenzio.
4. Selezionare eventuali target LUFS/rate/depth e leggere separatamente misure,
   CHECK di QA e raccomandazioni soggettive. Nessun target universale è obbligatorio.
5. Scegliere una reference locale stereo, stesso rate del mix; regolare gli offset
   su porzioni musicali comparabili. Fermare trasporto, monitoring e altre preview,
   attendere le code e impostare il dispositivo al rate dei file.
6. Provare A, B e Stop, verificando volume comparabile, transienti/stereo e code.
   Il confronto riparte, non segue automaticamente la posizione dell'altro brano.
7. Aprire Track EQ/Master inserts, editare manualmente e provare bypass/Undo/Redo.
   Riesportare e rianalizzare: il vecchio WAV non rappresenta i nuovi cambiamenti.
8. Avviare/annullare/ripetere analisi e chiudere durante il job. Provare poi la
   preview del browser e chiudere MASTER ASSIST: non deve fermare l'altra preview.
9. Verificare realmente Cancel durante export con VST3 e A/B: vecchio file,
   temporanei, ripristino audio/PDC e nuovo rendering. Il fix Windows è ancora aperto.

Windows x64/macOS nativi, mouse/DPI, ascolto, hardware/dropout, plugin commerciali
e nuovi sanitizers **NOT RUN**. La CI Windows esistente non può compilare questi
commit locali non pubblicati; nessun push/dispatch di codice vecchio spacciato
per verifica. Il gate Windows del fix Cancel `9c4e812` resta esplicitamente aperto.
Nessuna dipendenza/binario aggiunta al prodotto; FFmpeg già disponibile è usato
solo come verifica indipendente opzionale nei test, mai distribuito qui.
