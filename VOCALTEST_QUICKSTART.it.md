# Vocaltest Preview — Windows x64

Preview di prova, versione applicativa 0.51.1. Lo ZIP viene caricato soltanto dopo
build e test automatici Windows riusciti. Ascolto, dispositivo reale e prove
dall'applicazione.

## Scaricare e avviare

1. Aprire [GitHub Actions](https://github.com/wsx79/fasegroovearranger/actions/workflows/build.yml),
   scegliere una run **verde** del branch **Vocaltest** e scorrere fino ad Artifacts.
   Può essere richiesto l'accesso gratuito a GitHub.
2. Scaricare **Vocaltest-Preview-Windows-x64**. Estrarre l'archivio dell'artifact,
   poi **FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip** contenuto al suo interno.
   L'artifact dei diagnostics contiene log, non il programma da installare.
3. Conservare tutte le cartelle estratte. Avviare **Standalone\Fase Groove Arranger.exe**
   e scegliere il dispositivo audio nelle impostazioni. Non servono WSL, Bash o
   compilatori. Preferenze/cache usano il profilo utente. Non è una release firmata:
   verificare provenienza e SHA, senza disabilitare antivirus o protezioni globali.
4. Provare su una copia dei propri progetti; non cancellare campioni/cache ancora
   referenziati. Per spostare il progetto usare Collect portable project.

## Priorità di collaudo: Cancella durante export

Questa preview è un artifact per test personali: **NO-GO al rilascio pubblico**.
Gli esiti del runner non sostituiscono la prova con il proprio dispositivo/plugin.

1. Aprire una copia del progetto e scegliere export WAV **Transparent**. Provare
   prima senza plugin, poi con il VST3 del caso segnalato e con ritorni A/B,
   mandate e gruppi attivi. Annotare SHA da PREVIEW.json e versione del plugin.
2. Premere **Cancella** subito, a metà e verso la fine di un export lungo;
   ripetere l'annullamento e provare anche gli stem. La finestra deve continuare
   a ridisegnarsi; attendere il completamento del cleanup senza chiudere il processo.
3. Verificare riproduzione ed editing dopo l'annullamento, poi un nuovo export
   completo. Provare la sovrascrittura di un WAV di prova: annullando devono
   restare intatti i bytes del file precedente e non devono restare WAV temporanei.
4. Registrare PASS/FAIL/NOT RUN, durata dell'attesa, rate/buffer, plugin/versione
   e passi per riprodurre eventuali blocchi. Un plugin che non restituisce il
   controllo può ritardare il worker: non viene terminato forzatamente.

Vocal Chops resta separato e senza approvazione qualitativa definitiva.

## Checklist manuale Windows 11

Annotare versione Windows, SHA da PREVIEW.json, dispositivo audio, versioni dei
componenti e PASS/FAIL/NOT RUN per ciascun punto. Se qualcosa fallisce, allegare
passi per riprodurlo e diagnostica senza credenziali o registrazioni private.

- [ ] Avvio senza crash, scelta dispositivo, riproduzione audio e meter/mixer.
- [ ] Apertura di una copia di un progetto esistente, salvataggio, chiusura e riapertura;
      MIDI, campioni, livelli, mute/solo e riferimenti restano corretti.
- [ ] Grafica rinnovata di EQ, compressore, riverbero, delay, bus e mandate.
- [ ] Aggiungere una traccia separata **VOCAL CHOPS**, caricare un proprio WAV anche
      con drag/drop, provare Cancel Load e LIMITS senza perdere sorgente e Undo.
- [ ] Auto slice, snap Beat/Bar/1/16 e Snap off con taglio manuale, a 125 e 150 BPM;
      pad, WRITE SLICE MIDI, pitch, sync, loop, mixer ed esportazione stem.
- [ ] Undo/Redo, save/reopen e Collect portable project preservano originali ed edit.
- [ ] Percorsi con spazi/accenti, cambio dispositivo, cancel/retry: nessun blocco,
      crash. Separare qualità d'ascolto e test tecnici.

I sorgenti corrispondenti e le notice sono in **Source/**; istruzioni in SOURCE.txt.
Non sono inclusi helper vocali, voicebank, modelli neurali o DLL di terzi.
