# Fase 04 — Export & True Peak

Branch `Vocaltest`, base `0bdd4ed6857704e9bf44e875679ae6460d8082c3`.
La fase 03 è verificata tecnicamente su Linux; il suo audit non è stato ripetuto.
Restano pendenti Windows/macOS, hardware, plugin esterni, ascolto, dropout e
sanitizers. Vocal Studio/Chops rimangono sospesi e non approvati qualitativamente.

## Esistente e correzioni riprodotte

- `AudioEngine::exportAudio` gestisce già WAV stereo PCM 16/24 bit, frequenza
  selezionata, MP3 44,1 kHz, code, PDC, stems, file temporanei e copia privata.
  Riutilizzati pipeline e test, senza nuovi motori, formati o dipendenze runtime.
- `LoudnessMeter` aveva filtri generici diversi dal riferimento BS.1770: il nuovo
  test fallisce sul codice precedente a 100 Hz (−21,868 contro −21,825 LUFS).
  Il confronto analitico mostra uno scarto di circa −0,487 LU a 1.682 Hz.
  Ora i coefficienti a 48 kHz coincidono con le Tabelle 1/2 dell'Annex 1 ITU;
  adattamento bilineare alle altre frequenze. Cambia la misura, non l'audio.
- Il verificatore finale del guard poteva sottostimare i picchi. Regressione
  a 16x sul vecchio PCM: `hot-44100-16.wav` a −0,998012 dBTP contro il limite;
  ricostruzione indipendente del vecchio `dense-48000-24.wav`, con zeri prima e
  dopo: −0,813339 dBFS. La sola configurazione standard a 16x lascia errori
  su impulsi (−0,955980 dBFS) e un segnale denso a 96 kHz (−0,981763 dBFS).
  Corretto **solo il verificatore offline**: 16x, transizione del primo FIR
  0,01 anziché 0,05, stopband −100 dB e soglia −1,05 dBTP. L'eventuale
  correzione resta un guadagno uniforme sul file, preservando durata e metadati.
- Annullamento all'ultimo progress: prima l'export WAV restituiva successo e
  sovrascriveva il file esistente (FAIL riprodotto). Aggiunto il controllo prima
  della pubblicazione atomica WAV/MP3. Quattro casi guard on/off × WAV/MP3
  verificano annullamento, conservazione del file precedente e rilascio del motore.

Nessuna modifica al guard per campione, al suono con guard disattivato, al
callback, ai generatori, ai componenti vocali o allo schema `.fga`.

## Misure del guard esistente

Sinusoide 997 Hz in float, finestra stazionaria di un secondo, tre sample rate:

| Stimolo | Risultato prima della verifica/gain finale |
| --- | --- |
| Ampiezza 0,25, sotto soglia | THD+N circa 0,00003%; fondamentale 0,24997–0,24999 |
| Ampiezza 2, +6,02 dBFS, forte sovralivello | THD+N 26,27–26,29% |
| Impulso ampiezza 2 | Picco 0,715946; 90% dell'energia in 4 frame; allineamento entro 1 frame (−1 misurato) |

Il riferimento numerico del clamp PCM legacy ±0,999, sullo stesso tono ampiezza
2 a 48 kHz, dà 23,32% THD+N. Questi dati descrivono una saturazione forte,
non una prova d'ascolto. Il guard resta una protezione opzionale, non un limiter
trasparente. Sostituirlo richiederebbe una decisione architetturale separata;
non è stato sostituito né duplicato. La verifica finale modifica solo il livello.

## Verifica

Linux x64, GCC 14, JUCE 8.0.9, Release. Test mirati `--improvements`: PASS.

- Meter: quattro toni a 44,1/48/96 kHz; confronto con coefficienti ITU fissi
  a 48 kHz (tolleranza short-term 0,01 LU, integrated 0,025 LU; 0,1 LU alle
  altre frequenze), mono/stereo, silenzio breve, gate relativo e finestra 3 s.
  Restano i bin da 0,1 LU dell'integrated esistente; nessuna certificazione EBU.
- PCM: 36 combinazioni silenzio/17 frame/tono sotto soglia/tono sovralivello/
  impulso/mix denso × 44,1/48/96 kHz × 16/24 bit. PASS su header, durata,
  metadati e ricostruzione JUCE 16x.
- Riferimento indipendente: **36/36 PASS**, FFmpeg 7.1.5, sinc 32x,
  filtro 256 e zeri **prima e dopo** per la stessa condizione iniziale del meter.
  Picco massimo misurato **−1,042821 dBFS**. Senza zeri iniziali FFmpeg riflette
  il bordo: quel confronto non rappresenta lo stesso segnale zero-esteso.
- L'export WAV effettivo copre sei combinazioni sample rate/bit depth; restano
  i test di code, MP3 gapless, export privato durante playback, AudioTake e stem
  vocale con intro/outro silenziose già presenti. Nessun loro codice modificato.
- CTest finale: **15/15 PASS**, 146,00 s con compilazione concorrente;
  `audio_engine` 61,77 s. Inclusa regressione del vero helper/stem Linux;
  questo non approva qualità/funzionalità vocali. Sintassi dello script Python PASS.
  Build Release finale completa di applicazione e test: **PASS**; rimangono
  warning JUCE/deprecazioni/confronti float non bloccanti, nessun errore finale.

La suite mirata finale ha richiesto 9,284 s e 51.000 KiB di RSS massimo locale
(processo test completo). Non è una misura della CPU/latency del callback né un
confronto a carico uguale con la fase 03. Il verificatore più preciso costa più
CPU solo sul worker offline; buffer fissi, annullamento tra i blocchi, nessuna
nuova allocazione o elaborazione nel callback audio.

Riproduzione (dopo la normale build dei test; FFmpeg serve solo per QA):

```sh
FGA_EXPORT_AUDIT_DIR=/percorso/assoluto/segnali build/native/fga_engine_tests_artefacts/Release/fga_engine_tests --improvements
python3 scripts/verify-audio.py --export-peak /percorso/assoluto/segnali
ctest --test-dir build/native --output-on-failure
```

Log/fixture locali: `/workspace/scratch/phase04-*`, fuori repository. Il comando
indipendente produce `peak-reference.json`; non richiede FFmpeg nell'app o ZIP.
Fonti: [ITU-R BS.1770-5, Annex 1/2](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf),
[EBU Tech 3341](https://tech.ebu.ch/publications/tech3341).

## Limiti e gate

Per questa patch: Windows/macOS, hardware/driver reali, plugin esterni, ascolto,
dropout e sanitizers NON ESEGUITI. Il corpus di segnali non esaurisce tutte le
forme d'onda né sostituisce il collaudo hardware. Per MP3 il guard verifica il
PCM prima della codifica: i picchi del lossy decodificato non sono garantiti.

Non iniziata la fase 05. Nessun push/tag/release/workflow dispatch. Le correzioni
non richiedono una nuova architettura; un limiter più trasparente resta una
possibile scelta da approvare, alla luce delle misure e dei test d'ascolto mancanti.
