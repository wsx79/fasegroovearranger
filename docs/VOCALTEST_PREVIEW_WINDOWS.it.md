# Vocaltest Preview — Windows x64 (preparazione, 2026-11-10)

**NON pronta per pubblicazione. Nessuno ZIP di preview generato.** Questo workspace
è Linux; non dispone di MSVC/Windows SDK, PowerShell o runtime Windows. Nessuna
build, prova GUI, installazione dipendenze o prova d'ascolto Windows è stata
eseguita. Wine/cross-compilazione non sostituirebbero i test nativi richiesti.
Il nome della preview non cambia la versione applicativa 0.43.2.

## Per chi proverà il programma

Queste istruzioni si applicheranno **solo a uno ZIP verificato**, non ancora disponibile.
Estrarre tutta la cartella in un percorso scrivibile e avviare
`Standalone\Fase Groove Arranger.exe`. Non servono WSL o Bash, né un abbonamento.
Selezionare il dispositivo audio nelle impostazioni. "Portabile" indica assenza
di installer: preferenze e cache possono usare il profilo utente; la cache guida
usa `FaseGrooveArranger/VocalGuideCache` nella directory dati dell'utente.

- **Vocal Studio:** menu aggiunta traccia > `Vocal Studio (English)`; oppure
  aprire `Vocal Studio (English)...` dal menu della clip MIDI manuale. È una voce
  guida formantica inglese da consegnare alla cantante, senza promessa di naturalezza.
- **Vocal Chops:** menu aggiunta traccia > `VOCAL CHOPS`, poi Studio/sampler.
  Carica registrazioni dell'utente e non richiede il motore della voce guida.
- Conservare il file originale e lavorare su una copia del progetto. WAV e
  campioni sono esterni: usare Collect portable project prima di spostarli.

### Configurazione voce guida (ancora da validare su Windows)

Un tecnico deve prima predisporre e inventariare i pacchetti nativi: Perl >=5.22
(con MIDI, URI::Escape, Math::FFT e relativa XS compatibile con il Perl installato),
eSpeak classico 1.48 con dati inglesi e SoX 14.4.2. Strawberry Perl include strumenti
per compilare XS, ma l'esito di Math::FFT su Windows non è stato provato qui.
Registrare versioni, URL ufficiali, SHA256 e licenze degli archivi effettivamente
usati. Non sostituire automaticamente con eSpeak NG o altre versioni non testate.

Fonti ufficiali: [Strawberry Perl](https://strawberryperl.com/),
[eSpeak](https://espeak.sourceforge.net/download.html),
[SoX 14.4.2](https://sourceforge.net/projects/sox/files/sox/14.4.2/),
[moduli e condizioni](VOCAL_GUIDE_LICENSES.md).
Le pagine ufficiali consultate offrono eSpeak `setup_espeak-1.48.04.exe` e
SoX `sox-14.4.2-win32.exe`: la presenza del download **non** verifica contenuti,
DLL, licenze o funzionamento. Un tool Win32 separato può girare nativamente sotto
WoW64 su Windows x64; Perl e Math::FFT devono avere ABI/architettura compatibili.
Non scaricare/installare automaticamente questi pacchetti da questa procedura.

Preparare una copia fidata del sorgente eCantorix al commit
`069317157287ce1a6390c109d1a9310556bc0002`, seguendo
[helpers/ecantorix/README.md](../helpers/ecantorix/README.md). In PowerShell nativa,
con Perl nel PATH:

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

Adattare i percorsi reali. Le variabili valgono per questa sessione: avviare la
DAW dalla stessa PowerShell; non è necessario cambiare impostazioni di sistema.
Nel Vocal Studio selezionare il `ecantorix.pl` preparato. Serve la cartella completa
`espeak-data`, non solo il dizionario. Usare solo script fidati: vengono eseguiti
con i permessi dell'utente. Gli errori di dipendenza devono restare espliciti.

Il percorso guida usa WAV PCM/raw mono 16 bit e uscita guida a 22050 Hz. Verificare
con SoX i formati WAV/raw e gli effetti `tempo`, `bend`, `silence`, `reverse`,
`remix`. MP3, FFmpeg, LMMS, abcmidi, MBROLA, Config::Tiny e vocoder neurali non
servono per questa sintesi. DLL richieste dai tool devono essere censite, non
copiate alla cieca accanto alla DAW.

## Prova manuale (tutta NOT RUN su Windows)

1. Creare un progetto a 125 BPM, intro 4 battute, parte principale 8, outro 4.
   Aggiungere Vocal Studio e allungare la clip dall'origine a tutti i 64 beat.
2. Nel piano roll inserire C4 al beat 16 per 1 beat, D4 a 17 per 1, E4 a 18 per 2,
   G4 a 21 per 1, E4 a 22 per 2. Il beat 20–21 è pausa. Assegnare rispettivamente
   le parole inglesi **We / sing / here / We / stay** tramite Assign selected roll
   notes, poi Apply. Lasciare disattivato il canto nell'intro/outro.
3. Render English guide: verificare risposta UI, Cancel e nuovo render. Add guide
   audio take, Solo e ascolto: controllare BPM, note tenute, pausa e intro/outro
   silenziosi. Provare mute/solo, seek e loop. Aggiungere una nota in intro per
   provare separatamente l'override. [Procedura estesa](VOCAL_GUIDE_MANUAL_TEST.it.md).
4. Esportare il WAV separato per la cantante, riascoltarlo in un lettore e
   reimportarlo. Verificare Undo/Redo e salvataggio/riapertura dopo il caricamento.
5. Su una traccia Vocal Chops separata caricare un proprio WAV autorizzato (anche
   tramite browser/drop). Annullare un import; provare LIMITS sotto la dimensione
   del WAV: sorgente precedente e Undo devono restare invariati. Ripristinare il limite.
6. In Slices provare Auto slice, Beat/Bar/1/16/terzine e drag; impostare Source BPM
   corretto e provare Tempo Sync a 125 e 150 BPM. Snap off deve consentire tagli
   fuori griglia. Il cambio griglia/BPM non riscrive i marker già presenti.
7. Provare pad, WRITE SLICE MIDI, transpose/fine, reverse, fade, Undo/Redo, reopen
   e stem export. Ascoltare click, attacchi mancanti e disallineamenti; confrontare
   l'originale intatto. [Dettagli e limiti](VOCAL_CHOPS_IMPORT.md).
8. Ripetere con percorsi contenenti spazi e caratteri non ASCII, cambi dispositivo
   e frequenza, cancel/retry. Verificare da Task Manager che non restino processi
   helper. Registrare separatamente PASS/FAIL tecnico e giudizio d'ascolto.

## Per il manutentore: build e test nativi

Audit sorgente: i commit `358adb3`, `8cc5487`, `3f70446` sono antenati del branch
Vocaltest; HEAD iniziale `3f704467767599a99aef6186210efc9adc3ef380`.
Quest'ultimo non è stato trovato su GitHub dalla lettura API: non attribuire a
questa versione risultati CI di commit precedenti. Main non è stato modificato.

CMake include i file 01C/02, usa C++17, `/utf-8`, CRT MSVC statico, JUCE 8.0.9,
Shine statico e Signalsmith già presenti. Browser WebView non richiesto.
Il launcher guida usa CreateProcessW/Job Object e pipe Perl dirette, senza shell
POSIX. Queste sono verifiche di sorgente, **non un PASS di compilazione Windows**.

Su Windows 10/11 x64, predisporre Visual Studio 2022 Build Tools con C++ desktop
Windows SDK, CMake >=3.22, Git e Python 3 (per packaging). Aprire **x64 Developer
PowerShell**. Sono strumenti del manutentore; il tester non deve compilarli.
Il primo configure può scaricare JUCE per la build; l'uso della voce guida resta offline.
Con snapshot Vocaltest consolidato in un commit locale e working tree pulito,
dipendenze guida già verificate/configurate e Perl nel PATH:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-vocaltest-preview.ps1 -GuideHelper 'C:\Tools\English guide\ecantorix.pl'
```

Lo script è una ricetta **non eseguita qui su Windows**: blocca altri branch/host,
configura MSVC `-A x64`, build Release e CTest verbose, richiede il render eCantorix
reale e verifica PE32+ AMD64, dipendenze con dumpbin e tre harness GUI esistenti:
`--sample-import-test`, `--vocal-studio-test`, `--ui-test`. Arresta tutto al primo
errore e salva evidenze in `build/vocaltest-preview/validation-*`. Non crea ZIP né
pubblica nulla. Esaminare i log, non soltanto il titolo finale PASS. Hosting VST
esterno, dispositivi reali e ascolto richiedono verifiche aggiuntive.

## Gate legale e ZIP portabile (non ancora eseguito)

1. Conservare i risultati Windows reali e la scheda manuale con sistema, SHA del
   sorgente/binario, dipendenze, versioni e relativi hash. Esaminare l'output dumpbin:
   l'uso del CRT statico non prova l'assenza di ogni DLL esterna. Non includere DLL
   non di sistema senza verificare licenza, origine, architettura e necessità.
2. Non distribuire interpreti, moduli XS, eSpeak, dati, SoX o codec in questa preview.
   L'helper incluso è **solo il nostro prepare.pl sorgente**, GPL-3.0-or-later,
   con COPYING e data di modifica. La separazione in processi non esonera dalle
   licenze; il riesame dei pacchetti Windows effettivi resta aperto.
3. Per la DAW AGPL, JUCE AGPL, Shine LGPL e componenti MIT/OFL mantenere licenze e
   copyright. Preparare **sorgenti corrispondenti esatti**, inclusi motori vendorizzati,
   helper/modifiche, JUCE 8.0.9 effettivamente compilato con le sue notice e istruzioni
   per rebuild/relink di Shine. Una licenza nel pacchetto non sostituisce il sorgente.
   Il solo URL generato oggi da SOURCE.txt non basta: il commit è locale/non accessibile.
   Fornire il sorgente nello stesso pacchetto, oppure una modalità di accesso conforme
   verificata prima di distribuire. Nessun push è autorizzato da questa procedura.
4. **Solo dopo** i gate precedenti, preparare lo staging senza archivio:

   ```powershell
   python .\scripts\package.py --build-dir build/vocaltest-preview --stage-only
   ```

   Copiare i sorgenti corrispondenti e le istruzioni in `dist/package/Source`,
   sostituire `SOURCE.txt` con percorsi reali/SHA verificabili e aggiungere
   `PREVIEW.txt` con nome **Vocaltest Preview**, SHA, limiti e risultati Windows.
   Il packager include ora le guide vocali e prepare.pl/COPYING; non installa tool.
5. Dopo aver verificato inventario e sorgenti, da PowerShell:

   ```powershell
   Compress-Archive -Path .\dist\package\* -DestinationPath .\dist\FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip
   Get-FileHash .\dist\FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip -Algorithm SHA256
   ```

   Estrarre in una nuova cartella su Windows pulito e ripetere avvio e test manuali,
   con tool guida installati separatamente. Non usare file della build/PATH dello
   sviluppatore per mascherare dipendenze mancanti. Non pubblicare prima di questo
   controllo; non modificare main, tag o release.

## Risultati di questa sessione

- PASS: presenza dei tre commit, build Linux Release; CTest Linux **15/15** in
  **123,77 s**, con render eCantorix reale/cache, cinque regressioni Vocal Chops e nuovo import.
- PASS: GUI Linux completa, import GUI **11/11**, Vocal Studio GUI **22/22** con
  guida reale/import/export; verifica risorse/guide family e sintassi Python.
- PASS packaging: payload, source-only GPL/COPYING, assenza di runtime esterni,
  scelta build alternativa e stage-only senza ZIP, su fixture Linux/Windows.
  Il caso Windows simulato non esegue un binario PE e non certifica Windows.
- Windows: compilazione, script PowerShell, dipendenze/XS/codec, GUI, audio,
  installazione, estrazione dello ZIP e ascolto **NOT RUN**.
- Nessun binario Windows o ZIP di preview generato/distribuito. Fase 03 non iniziata.
