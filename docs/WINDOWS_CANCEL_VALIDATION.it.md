# Verifica Cancel e preview Windows aggiornata

Base: `c0b6358a0e563d4348afa7fa45a10430db855f80`, branch esclusivo
`Vocaltest`. La roadmap ufficiale termina alla fase 09: nessuna fase 10.
Autorizzati push fast-forward e artifact Actions per collaudo personale;
non autorizzati main, merge, tag o release. **Rilascio pubblico: NO-GO**.

## Codice e copertura

Il fix `9c4e812` è antenato della base: `RenderProgressTask` invia il flag e
continua il dispatch UI senza join/kill; l'owner resta vivo fino al ritorno del
worker e al ripristino audio/PDC. Causa e limiti in `EXPORT_CANCEL_FIX.it.md`.
Non è stata sostituita o modificata la correzione produttiva.

La matrice `verifyExports` esistente controlla il vero pulsante Cancel:
avvio immediato, render/staging, scaling/PCM, verifica finale, click ripetuti,
chiusura richiesta, stem, gruppo/sidechain/ritorni A/B, preservazione bytes e
assenza staging residuo, identità plugin/PDC e successivo WAV completo.
I gate del progresso sono esclusivamente self-test e impediscono che il Cancel
arrivi dopo la conclusione di un export breve.

Aggiunte mirate per questa verifica:

- Riproduttore finito del precedente wrapper JUCE: il worker richiede dispatch
  UI durante il cleanup, ma scade dopo 800 ms. Il test deve rilevare il mancato
  handshake; nessun kill forzato. Subito dopo, lo stesso handshake deve riuscire
  con il wrapper cooperativo. Riproduce il meccanismo di blocco, non la stack
  trace del plugin/PC originariamente segnalato.
- La stessa matrice ora gira anche con il VST3 **effetto** `Arranger Effect Probe`,
  oltre alla fixture strumento `Arranger Host Probe` e ai ritorni nativi. Ogni
  cancellazione conserva l'istanza dell'effetto. Plugin compilati dai sorgenti
  di test già presenti; non sono plugin commerciali.
- Il workflow già esistente `build.yml`, job `vocaltest-preview`, usa
  `windows-latest`, MSVC x64 Release, CTest e ricette UI/VST3 native. Ora conserva
  JUnit e richiede report Cancel completi per interno/strumento/effetto prima
  del packaging; produce `windows-cancel.json` nei diagnostics.

Nessuna modifica a DSP/export Transparent, routing/PDC produttivi, generatori,
progetti/schema, moduli vocali o dipendenze. Le nuove operazioni sono self-test,
fuori dal callback audio. Nessuna nuova misura prestazionale o promessa hardware.

## Verifiche locali

- Release Linux: PASS (build applicazione, secondo build senza lavoro residuo).
- CTest `native_render_gate`: PASS 1/1; test plumbing Python: PASS 2/2.
- YAML/Python embedded: PASS; gate report accetta fixture completa e rifiuta
  report vuoti/FAIL/scenari mancanti. Prove simulate, non Windows nativo.
- Risorse e fixture packaging/preview: PASS; nessuna equivalenza con avvio PE reale.
- Matrice VST3 effetto Linux: PASS effetti **79/79**, export/Cancel **52/52**,
  verificatore PCM/stato indipendente e identità effetto. Vecchio handshake bloccato
  riprodotto in modo finito; handshake corretto riuscito.
- Audit/suite generali Linux non ripetuti: restano le evidenze della fase 09.

## Windows nativo

**PASS nativo Windows x64**, MSVC **19.51.36260.0**, codice
`f98b7063c197dc432b969699240d6eda1c5584da`.
[Run completo](https://github.com/wsx79/fasegroovearranger/actions/runs/43184992493),
[artifact scaricabile](https://github.com/wsx79/fasegroovearranger/actions/runs/43184992493/artifacts/13928718840).
Il push da `26e2422` a questo commit è stato fast-forward, solo origin/Vocaltest;
comprende il fix Cancel e tutti i cinque commit precedenti fino a c0b6358.
Nessuna modifica a main, merge, tag o release; il job di pubblicazione è SKIPPED.

| Verifica nativa | Esito |
|---|---|
| Build MSVC Release x64 | PASS, eseguibile prodotto |
| CTest | **22/22 PASS**, 0 FAIL, 197,44 s; nessuna suite skipped |
| Export/Cancel, strumenti interni | **44/44 PASS** |
| Export/Cancel, VST3 strumento reale di test | **44/44 PASS** |
| Export/Cancel, VST3 effetto reale di test | **52/52 PASS** |
| Effetti VST3 ordine/stato/automazione/Undo/master | **79/79 PASS** |
| Host/editor/focus/Freeze-Unfreeze | **15/15 PASS** |
| Compressore UI | **16/16 PASS** |
| Sidechain/routing UI e Undo/Redo | PASS report nativo |
| MASTER ASSIST UI | **17/17 PASS** |
| UI generale/MIX ASSIST, EQ, piano, WAV/MP3, MIDI | PASS ricetta nativa |
| EXE estratto dal pacchetto: avvio/import/UI | **11/11 PASS** |
| Dati vocali/helper mancante | **14 PASS**, render reale NOT RUN; import **11 PASS** |

Tutti e tre i report Cancel contengono la riproduzione **finita del vecchio
handshake bloccante** e il successo del nuovo wrapper. Tutti i sette scenari
controllati passano, con dispatch UI mentre il worker è attivo, ritorno del
worker, conservazione WAV precedente, niente staging residuo, PDC/istanze
preservati e successivo export completo. Il meccanismo è quindi riprodotto e
la correzione verificata **anche su Windows nativo**. Il caso concreto sul PC e
sul plugin originariamente segnalato rimane da confermare manualmente.

Tempo click → completamento nel solo probe: interno **39,33 ms**, strumento
**16,38 ms**, effetto **58,21 ms**. Non sono tempi massimi garantiti per plugin,
disco o hardware. Benchmark isolato CTest del runner: **1.024.000 frame stereo
in 138,488 ms**, zero allocazioni C++ ordinarie nel callback misurato. Nessun
confronto prima/dopo o certificazione hardware; nessuna nuova elaborazione DSP.

Artifact **Vocaltest-Preview-Windows-x64**, **42.345.839 byte** (archivio esterno).
ZIP interno **FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip**,
**42.345.621 byte**. SHA-256 ZIP interno:
`4c0bc9d49576f9171b1d666111f29ab3d6abd2d0f09a70ac2d4d535c5a08d2c2`.
Download effettuato, CRC, payload PE AMD64, manifest e hash dei sorgenti/licenze
verificati nuovamente sul file scaricato. SHA-256 artifact esterno coincidente
con il digest GitHub. Contiene EXE, risorse/manuali, sorgenti app/JUCE e notice.
Il manifest riporta esattamente il commit del codice testato.

Nota accesso: `gh run download` da questo workspace ha ricevuto HTTP 403 dal
redirect Azure; il download mediante il connettore GitHub è riuscito e il file
ha superato tutti i controlli. Questo 403 locale non è un fallimento della build
o del pacchetto. I link artifact richiedono eventualmente login GitHub e sono
soggetti alla conservazione Actions di 30 giorni. Diagnostics separati:
`Vocaltest-Windows-diagnostics`, con JUnit, report/PNG/WAV e windows-cancel.json.
Dati numerici e hash in `WINDOWS_CANCEL_VALIDATION.json`.

Il report finale viene conservato in un commit locale di sola documentazione;
il remoto e l'artifact restano sul codice esatto della build verificata.

Il packaging esistente verifica EXE AMD64 PE32+, risorse e checksum, sorgenti
corrispondenti dell'applicazione e JUCE effettivamente compilato, licenze/notice
e assenza runtime esterni non approvati. Dopo estrazione il runner avvia l'EXE
voicebank, codec esterni o DLL di terzi. QA ffmpeg resta soltanto sul runner.

## Gate umani e residui

La verifica automatica non certifica tutti i VST3. Un plugin che non ritorna da
una propria chiamata può ritardare il worker; il fix evita join/kill UI e non
isola plugin in processi separati. Restano **NOT RUN** il PC Windows/plugin del
caso originale, interazione fisica/ascolto/hardware/dropout, plugin commerciali,
macOS/AU e approvazione funzionale/qualitativa dei moduli vocali congelati.
Restano validi gli altri limiti/sanitizers parziali della valutazione fase 09.

Per scaricare: aprire il run verde del branch Vocaltest, Artifacts,
`Vocaltest-Preview-Windows-x64` (login GitHub eventualmente richiesto); estrarre
prima l'artifact, poi lo ZIP interno. Avviare
`Standalone/Fase Groove Arranger.exe`, configurare il dispositivo audio e usare
copie dei progetti. Checklist prioritaria in `VOCALTEST_QUICKSTART.it.md`:
Cancel subito/metà/fine, VST3 utente, A/B/stem, integrità WAV precedente,
riproduzione e nuovo export dopo l'annullamento. Registrare SHA/plugin/rate/buffer.
