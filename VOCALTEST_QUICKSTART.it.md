# Vocaltest Preview — Windows x64

Preview di prova, versione applicativa 0.43.2. Lo ZIP viene caricato soltanto dopo
build e test automatici Windows riusciti. Ascolto, dispositivo reale e prove
manuali su Windows 11 sono ancora da eseguire. Il rendering eCantorix Windows
**NON è stato verificato da questo workflow**: i suoi strumenti non sono inclusi.

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

## Voce guida inglese: strumenti esterni opzionali

Vocal Chops funziona con i propri WAV e non richiede questi strumenti. Vocal Studio
usa una voce sintetica semplice per guidare una successiva cantante reale.
Non genera una voce neurale naturale e non richiede servizi cloud.

La configurazione esterna è **ancora da validare su Windows**. Usare le fonti
ufficiali, registrare versioni e hash; non copiare DLL da siti casuali.

1. Installare [Strawberry Perl x64](https://strawberryperl.com/) (Perl >=5.22),
   poi i moduli MIDI, URI::Escape e Math::FFT tramite CPAN. Le versioni sorgente
   già esaminate sono MIDI-Perl 0.84, URI 5.30 e Math-FFT 1.36. Math::FFT contiene
   un modulo compilato XS: deve corrispondere al Perl installato; usare il suo
   toolchain. Se l'installazione fallisce, registrare l'errore: la voce guida non
   è pronta. [Licenze e versioni](docs/VOCAL_GUIDE_LICENSES.md).
2. Installare separatamente [eSpeak classico 1.48](https://espeak.sourceforge.net/download.html)
   con `espeak-data` completo e [SoX 14.4.2](https://sourceforge.net/projects/sox/files/sox/14.4.2/).
   Sono componenti nativi Windows anche nelle varianti Win32 eseguite su x64.
   Non sostituire automaticamente eSpeak con eSpeak NG. Nessun codec MP3 è necessario
   per la guida WAV; verificare formati raw/WAV ed effetti tempo/bend di SoX.
3. Scaricare i [sorgenti eCantorix fissati](https://github.com/divVerent/ecantorix/archive/069317157287ce1a6390c109d1a9310556bc0002.zip),
   estrarli, quindi aprire PowerShell nella cartella della preview. Adattare i percorsi:

   ```powershell
   perl .\helpers\ecantorix\prepare.pl 'C:\Tools\ecantorix-upstream' 'C:\Tools\English guide'
   $env:FGA_PERL_BIN = 'C:\Strawberry\perl\bin\perl.exe'
   $env:FGA_ESPEAK_BIN = 'C:\Tools\eSpeak\command_line\espeak.exe'
   $env:FGA_ESPEAK_DATA = 'C:\Tools\eSpeak\espeak-data'
   $env:FGA_SOX_BIN = 'C:\Tools\SoX\sox.exe'
   & $env:FGA_PERL_BIN -MMIDI -MURI::Escape -MMath::FFT -e 'print qq(modules OK\n)'
   & $env:FGA_ESPEAK_BIN --version
   & $env:FGA_SOX_BIN --help
   & '.\Standalone\Fase Groove Arranger.exe'
   ```

   Avviare dalla stessa PowerShell: queste variabili valgono solo nella sessione.
   In Vocal Studio scegliere `C:\Tools\English guide\ecantorix.pl`. Eseguire soltanto
   script fidati: hanno i permessi del proprio utente. Non è necessaria alcuna
   modifica al sistema per la separazione in processi; le licenze restano applicabili.

## Checklist manuale Windows 11

Annotare versione Windows, SHA da PREVIEW.json, dispositivo audio, versioni dei
componenti e PASS/FAIL/NOT RUN per ciascun punto. Se qualcosa fallisce, allegare
passi per riprodurlo e diagnostica senza credenziali o registrazioni private.

- [ ] Avvio senza crash, scelta dispositivo, riproduzione audio e meter/mixer.
- [ ] Apertura di una copia di un progetto esistente, salvataggio, chiusura e riapertura;
      MIDI, campioni, livelli, mute/solo e riferimenti restano corretti.
- [ ] Aggiungere **Vocal Studio (English)** dal menu aggiunta traccia. A 125 BPM,
      scrivere “We sing here”, creare C4/D4/E4 con una pausa e una nota tenuta,
      assegnare le sillabe alle note dal piano roll e applicare le impostazioni.
- [ ] Render English guide, annullamento e nuovo render; UI/trasporto rispondono,
      gli errori di strumenti mancanti sono chiari. Importare la take, ascoltare
      note/pausa e silenzio nell'intro/outro; Undo/Redo e Export added guide WAV for singer.
      [Prova dettagliata con melodia](docs/VOCAL_GUIDE_MANUAL_TEST.it.md).
- [ ] Aggiungere una traccia separata **VOCAL CHOPS**, caricare un proprio WAV anche
      con drag/drop, provare Cancel Load e LIMITS senza perdere sorgente e Undo.
- [ ] Auto slice, snap Beat/Bar/1/16 e Snap off con taglio manuale, a 125 e 150 BPM;
      pad, WRITE SLICE MIDI, pitch, sync, loop, mixer ed esportazione stem.
- [ ] Undo/Redo, save/reopen e Collect portable project preservano originali ed edit.
- [ ] Percorsi con spazi/accenti, cambio dispositivo, cancel/retry: nessun blocco,
      crash o processo helper rimasto attivo. Separare qualità d'ascolto e test tecnici.

I sorgenti corrispondenti e le notice sono in **Source/**; istruzioni in SOURCE.txt.
Non sono inclusi Perl, eSpeak, SoX, voicebank, modelli neurali o DLL di terzi.
