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

IN ATTESA del run sul nuovo commit pubblicato. Nessun PASS Windows anticipato.
Solo se tutti i gate riescono viene caricato l'artifact
`Vocaltest-Preview-Windows-x64`, contenente
`FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip`.

Il packaging esistente verifica EXE AMD64 PE32+, risorse e checksum, sorgenti
corrispondenti dell'applicazione e JUCE effettivamente compilato, licenze/notice
e assenza runtime esterni non approvati. Dopo estrazione il runner avvia l'EXE
per il test di import/UI. Non vengono inclusi eCantorix, Perl, eSpeak, SoX,
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
