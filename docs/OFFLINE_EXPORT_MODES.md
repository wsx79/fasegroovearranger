# Fase 04 — modalità di export offline

Base: `079565d544e8a5cdc3bb08d6e974e9d81ffede55`, branch `Vocaltest`.
Implementazione autorizzata soltanto offline; nessun cambiamento a callback,

## Transparent e ceiling

Il rendering viene scritto **una sola volta** in un temporaneo WAV IEEE float32,
senza clamp/limiter, conservando picchi oltre 0 dBFS. In questo modo non serve
rieseguire plugin potenzialmente non deterministici. `OfflineExport.cpp` riusa
il verificatore True Peak 16x con primo FIR a banda ampia; il vecchio scan inline
è stato condiviso, non duplicato. Si applica `min(1, ceiling/peak)` a entrambi
i canali, poi si scrive il PCM 16/24 bit e si ricontrolla il file quantizzato.
Margine di misura: 0,05 dB. Nessun aumento automatico del volume/target LUFS.

Memoria a blocchi da 4096 frame; temporanei RAII chiusi/rimossi su completamento,
errore e annullamento. La pubblicazione del WAV/MP3 resta atomica e controlla
l'annullamento anche dopo l'ultimo progress. Controllati letture, scritture,
flush, valori non finiti, rate/formato dello staging e target-directory.
Nessun file I/O o preparazione dei filtri introdotto nel callback.

Ceiling precedente: fisso a -1 dBTP. Il limite superiore resta **-1 dBTP**;
il menu consente -1/-2/-3/-6/-12, API valida da -12 a -1. Per MP3 si controlla
il PCM precodifica; non si garantisce il True Peak del lossy decodificato.

Nuove preferenze e `AudioExportOptions{}`: **Transparent**. Le preferenze
esplicite `exportLoudnessGuard=0/1` e la firma posizionale storica
`AudioExportOptions(rate,bits[,bool])` mantengono Legacy. Selezionando Transparent
si salva `exportPeakMode`, senza cancellare chiavi storiche. Legacy compare
solo per preferenze storiche/Legacy; per nuovi profili l'unica modalità esposta
è Transparent. Sample rate, bit depth e formato dei progetti sono invariati.

## Loud: candidato non ammesso

Il candidato di QA usa 4x FIR, lookahead 3 ms, attacco 0,5 ms, rilascio 100 ms,
massimo mobile e un inviluppo di guadagno comune stereo. Il segnale ritardato è
salvato in un piccolo ring; non contiene clipping istantaneo. Compensazione e
flush sono locali al file offline, senza interventi alla PDC di playback.
Segue la stessa analisi/quantizzazione/verifica di Transparent.

**Gate non superato:** residuo IMD massimo **0,198936%** sul test 60/7000 Hz,
contro criterio iniziale <=0,1%; sorgente float ~0,000035%. Il tono 997 Hz è
molto migliore del vecchio guard, ma questo non basta per ammettere il limiter.
`offlineLoudReady=false`: niente voce Loud nella UI, preferenze Loud non
ammesse e API `AudioEngine::exportAudio` che rifiuta la modalità. Il candidato
resta esercitabile dal solo percorso QA diretto. Nessuna promessa di qualità
d'ascolto o loudness costante. L'eventuale revisione richiede un nuovo intervento.

## Confronto numerico

Corpus: 2 s, 44,1/48/96 kHz; toni 997 Hz a drive 0/+1/+3/+7,18 dB rispetto
alla soglia storica 0,875, IMD 60/7000 Hz 4:1, impulsi, burst 80 Hz, materiale
complesso, near-Nyquist, 17 frame e silenzio. Canale R=0,37 L per verificare il
link stereo. THD+N/IMD: fit dei carrier e DC in una finestra coerente di 1 s,
con residuo della sorgente float separato (massimo ~0,000035%). Non si presenta
il contenuto armonico del materiale complesso come distorsione del limiter.

| Percorso | Picco max indipendente 32x | THD+N max, toni | IMD max |
| --- | ---: | ---: | ---: |
| Transparent | -1,044017 dBFS | 0,00004475% | 0,00004139% |
| Loud candidato | -1,084768 dBFS | 0,00009491% | 0,19893603% |
| Legacy | -1,042821 dBFS | 26,28642960% | 24,12283335% |

A 48 kHz, materiale complesso: Transparent **-14,048 LUFS / crest 12,478 dB**;
Loud **-11,288 LUFS / crest 9,384 dB**; Legacy **-8,210 LUFS / crest 6,541 dB**.
Il candidato guadagna ~2,76 LU su questo materiale, modificando la dinamica;
non è un target generalizzabile. Sul tono forte: -3,504 / -3,603 / -1,265 LUFS.
I livelli riflettono anche lo sbilanciamento stereo del corpus, non stereo dual-mono.

Transparent conserva ogni campione entro 2e-7 dal riferimento moltiplicato per
un solo gain; errore di rapporto stereo massimo 1,383e-7. Nel candidato è
2,027e-7. Legacy può arrivare a **0,015807** sui sovralivelli densi a causa del
clamp PCM separato: è misurato come confronto storico, non usato come criterio
di accettazione dei nuovi modi. Il percorso storico è stato preservato.

## Test e prestazioni locali

Linux x64, GCC 14, JUCE 8.0.9, Release; nessuna dipendenza runtime aggiunta.
FFmpeg è lo strumento QA già previsto dal progetto, mai incluso nell'app.

- Nuovo test nel target `fga_engine_tests`, CTest `offline_export`: 99 casi di
  confronto + PCM16 ai tre rate con ceiling -3; uguaglianza del segnale,
  stereo, metadati/durata, staging con headroom, export completo e sovrascrittura.
  Annullamento in render/analisi/scrittura/verifica/pubblicazione WAV/MP3,
  errori/ripristino e assenza di temporanei residui.
- Riferimento FFmpeg sinc 32x, zeri prima/dopo, filtro 256: **103/103 PASS**,
  inclusi PCM16 -3 dBTP ed export reale sovrascritto -3 dBTP.
- UI export Linux/Xvfb: 17/17 PASS (preferenze nuove/storiche, ceiling,
  dialogo WAV 48 kHz/16 bit, MIDI/stem, errore/annullamento/ripristino).
- Build Release completa: **PASS**. Suite CTest seriale finale: **16/16 PASS**,
  202,97 s, stessi timeout. Sintassi del verificatore Python PASS.
  Nella precedente corsa concorrente 14/16 PASS e timeout su armonia/helper;
  retry isolato con limiti invariati: PASS, 6,70 s e 27,33 s. Nessun motore
  modificato né timeout aumentato per nascondere il problema.

Benchmark isolato del **solo helper console**, 48 kHz stereo float → PCM24:
2 s processati in **0,340 s**, 120 s in **17,555 s** (CPU 17,482 s), RSS massimo
**8704 KiB in entrambi i casi**. Il file float da 120 s è 46.080.104 byte;
il PCM è 34.560.104 byte. Memoria indipendente dalla durata; il dato non è
l'RSS/CPU dell'intera DAW né un benchmark hardware generalizzabile. Staging
float aggiuntivo ~23 MB/min a 48 kHz, oltre all'output; RAM e lavoro sono a blocchi.

Riproduzione, dopo la build:

```sh
FGA_OFFLINE_EXPORT_AUDIT_DIR=/percorso/assoluto/qa build/native/fga_engine_tests_artefacts/Release/fga_engine_tests --offline-export
python3 scripts/verify-audio.py --offline-modes /percorso/assoluto/qa
ctest --test-dir build/native --output-on-failure -j1
FGA_OFFLINE_BENCH_SECONDS=120 FGA_OFFLINE_EXPORT_AUDIT_DIR=/percorso/assoluto/bench build/native/fga_engine_tests_artefacts/Release/fga_engine_tests --offline-export
```

CLI UI: applicazione `--export-ui-test CARTELLA` (desktop/Xvfb). Non eseguire
la ricostruzione FFmpeg pesante contemporaneamente ai test temporizzati.
Log/CSV/JSON/WAV locali fuori repository: `/workspace/scratch/offline-*`.

Windows/macOS, ascolti/ABX, hardware/plugin esterni, dropout e sanitizers NON
ESEGUITI per questo intervento. Il corpus non è esaustivo di ogni forma d'onda.
Il vecchio artifact Windows non contiene queste modifiche. Fasi 01/02 ancora
sospese senza approvazione qualitativa; fase 05 non avviata. Nessun push/tag/release.
