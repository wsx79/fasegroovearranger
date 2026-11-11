# Vocaltest Preview Windows x64 — Actions

## Prerelease di test: livelli comuni A/B, pan e nuovo design

**Pubblicata**: [Vocaltest Preview](https://github.com/wsx79/fasegroovearranger/releases/tag/preview-vocaltest-2026-11-11-839f6e8).
**[Download diretto ZIP Windows x64](https://github.com/wsx79/fasegroovearranger/releases/download/preview-vocaltest-2026-11-11-839f6e8/FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip)**, **42.201.127 byte** (42,2 MB).
La stable Latest resta **v0.43.2**; main invariato. Tag di test **preview-vocaltest-2026-11-11-839f6e8**,
commit binario **839f6e8398ea16d04c6759846226ff32c36f3698** sul branch Vocaltest.

**SUCCESS** [build Windows 43102064454](https://github.com/wsx79/fasegroovearranger/actions/runs/43102064454), MSVC 19.51.36260.0 x64 Release
su Windows Server 2025. CTest **18/18**, **164,58 s**, zero test falliti;
ritorni **33/33**, compressore **16/16**, guard pan **11.737** con il nuovo test
di repaint parziale. UI/WAV/MP3, VST3 fixture, dati vocali/import campioni e avvio
EXE estratto PASS. Nessuna correzione al codice/build necessaria su Windows.
Download indipendente dell'artifact e del **link pubblico della release PASS**,
hash identici; PE AMD64, risorse, sorgenti completi app/JUCE/licenze e import
soltanto dalle 21 DLL di sistema verificati. Nessun helper vocale runtime,
modello/voicebank, codec o DLL esterna aggiunta.

ZIP SHA256: `08d713953fb472c25d8a98189badea5003faff6e997b2b38be5a03210f8d51c8`.
EXE: **17,473,024 byte**, SHA256 `8fb1078950950706a180d860c765d815aa8d47c91aba7b010e7eb7734124e097`.
Checksum allegato alla release; registro [JSON](VOCALTEST_PREVIEW_RESULT.json).
Estrarre in una cartella nuova e avviare **Standalone\Fase Groove Arranger.exe**.
Livello A 62% comune a tutte le tracce con A attivo, A turchese/B viola;
[checklist manuale](WINDOWS_AUDIO_CHECKLIST.it.md), [report](BUS_SEND_REVIEW.it.md).

La pubblicazione ha richiesto un passaggio separato: upload locale 401, poi job
temporaneo [43106621631](https://github.com/wsx79/fasegroovearranger/actions/runs/43106621631)
con caricamento e riscaricamento degli asset **PASS**, ma ultimo publish **FAIL 403**.
Completato il solo publish tramite API locale autorizzata, poi verificato il
file pubblico. Non è un fallimento della build/test nativi. Rimosso il job
operativo temporaneo, ripristinato il workflow preview originale con permessi
read-only. I successivi commit documentali non cambiano il binario del tag.

**NOT RUN:** clic/DPI reali Windows 11, ascolto/hardware/dropout, vendor plugin,
sintesi eCantorix reale, input sintetico opzionale, nuovi sanitizers e macOS.
Il fix pan è tecnicamente verificato anche su Windows, non ancora confermato
con mouse/hardware dell'utente. Salvare copie: formato 7 e adozione esplicita dei
livelli comuni sui file storici con Undo. Vocali congelati; fase 07 non avviata.

## Archivio: prima preview ritorni A/B — 2026-11-11

**SUCCESS** [run 43089785681](https://github.com/wsx79/fasegroovearranger/actions/runs/43089785681), build **`2f7eb7ade201209e698f765f8524171ae7ed7a72`**.
Push normale su `origin/Vocaltest`, autorizzato dall'utente; nessuna divergenza,
nessun force push, main/tag/release. Riutilizzato il workflow esistente, senza
correzioni di compilazione, codice o test. MSVC **19.51.36260.0 x64 Release**,
Windows Server 2025 / windows-2025-vs2026, Visual Studio 18 2026; nessun WSL.

- **PASS** build; CTest **18/18**, **181,65 s**, **0 FAIL**.
- **PASS** ritorni A/B **24/24**, compressore **16/16**, gruppi/sidechain;
  aggiunta/ordine/rimozione/bypass, sorgente Kick, Undo/Redo e persistenza formato 7.
- **PASS** guard pan **11.736 controlli** anche su Windows. Usa
  `Button::triggerClick`: **non riproduce né risolve il difetto riferito dall'utente
  con clic reali/DPI**; resta da verificare manualmente.
- **PASS** UI/WAV/MP3, export UI **17/17**, VST3 effetti **79 asserzioni**
  e verifiche PCM indipendenti, strumenti VST3/focus (fixture del progetto).
- **PASS** dati/UI vocali **14**, import campioni **11**, EXE estratto **11**;
  non validano sintesi esterna né qualità delle funzioni vocali congelate.

Scaricare [**Vocaltest-Preview-Windows-x64**](https://github.com/wsx79/fasegroovearranger/actions/runs/43089785681/artifacts/13890261519),
**41.884.527 byte** (circa **41,9 MB**), scadenza **2026-12-11 20:10 UTC**.
ZIP interno **FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip**,
**41.884.309 byte**, SHA256
`a7ed19a0d59fb382646f82cf5282a02a5126e55b35ecd9be028ce026ef6beb51`.
EXE **17.450.496 byte**, SHA256
`f4b344bf6b26eacbdf1114c55fe5ff4d3e789790eaced589848aa67bef08eef7`.
Dati completi: [VOCALTEST_PREVIEW_RESULT.json](VOCALTEST_PREVIEW_RESULT.json).

Download indipendente **PASS**: digest Actions, ZIP/PE AMD64, hash risorse/EXE,
sorgenti app/JUCE completi e licenze/adattamenti esatti. Confrontati anche i
sorgenti critici dei nuovi ritorni e del formato progetto (CRLF normalizzati
solo per il confronto testuale). Import normali e ritardati verificati:
**21 DLL di sistema, nessuna DLL aggiuntiva necessaria**. Inclusi manuali, risorse,
licenze, sorgenti e istruzioni build/relink; nessun runtime eCantorix, Perl,
eSpeak, SoX, FFmpeg, codec/modello/voicebank o altro binario esterno.

Per provare: estrarre l'archivio Actions e lo ZIP portabile interno, poi avviare
**Standalone\Fase Groove Arranger.exe** conservando tutte le cartelle.
Creare un nuovo progetto, aprire **Bus e mandate → Ritorni A/B**: A riverbero,
B delay; regolare mandate e ritorni. Aggiungere un compressore e scegliere Kick
come sorgente, abbassando la soglia. Confrontare bypass e meter GR.
[Checklist pratica](WINDOWS_AUDIO_CHECKLIST.it.md) e
[guida alle nuove catene](BUS_SEND_REVIEW.it.md). Usare **copie** dei progetti:
i nuovi dati richiedono formato 7; quelli vecchi mantengono il routing storico
fino a conversione esplicita a trasporto fermo.

**NOT RUN: 5 ambiti Windows**: Windows 11/ascolto/hardware/dropout/clic reali;
plugin vendor; sintesi eCantorix reale; input sintetico opzionale (SKIP esplicito);
sanitizers Windows. macOS e qualità restano pendenti. Il candidato Loud continua
a fallire intenzionalmente il gate IMD e non è esposto: non è un FAIL della suite.
Nessun audit Linux ripetuto. Un successivo commit documentale `[skip ci]` registra
questo risultato; il binario verificato resta **2f7eb7a**. Fase 07 non iniziata.

## Archivio: preview fasi 03–06 precedente al redesign — 2026-11-11

**SUCCESS** [run 43040178202](https://github.com/wsx79/fasegroovearranger/actions/runs/43040178202), build **`353ae3d04be740b2d861c90f6b7e2a0a2a3a4af1`**.
Runner Windows Server 2025 / windows-2025-vs2026, Visual Studio 18 2026,
MSVC 19.51.36260.0 **x64 Release**, senza WSL o cross-compilazione Linux.

- **PASS** build completa; CTest **18/18**, 126,38 s; nessun FAIL finale.
- **PASS** UI compressore **14/14**, menu Native/Plugin e annullamento asincrono;
  mixer/sidechain; UI export **17/17**; startup, WAV/MP3 e interfaccia nativa.
- **PASS** VST3 effetti: **79 asserzioni**, più verifica PCM e stati; strumenti
  VST3 e regressioni focus, con fixture native del progetto, non plugin commerciali.
- **PASS** Vocal Studio dati/UI **14** e sample import **11**; non equivalgono
  a sintesi reale o approvazione funzionale/qualitativa delle fasi vocali congelate.
- **PASS** pacchetto estratto: avvio dell'EXE Windows e sample UI **11**.

Artifact [**Vocaltest-Preview-Windows-x64**](https://github.com/wsx79/fasegroovearranger/actions/runs/43040178202/artifacts/13865383703):
**41.795.309 byte**, scadenza **2026-12-11 14:57 UTC**.
Dentro: **FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip**, **41.795.091 byte**,
SHA256 `7202efd3267879e0384ce2369e08b88a2cbb70d6e92f89d5af244e9b42ad8f76`.
EXE: **17.317.376 byte**, SHA256
`2fc33af410d2fb07a34d82dd8a9f77300ac1595f15665b1573a12e11f40c06fa`.
Dati completi: [VOCALTEST_PREVIEW_RESULT.json](VOCALTEST_PREVIEW_RESULT.json).

Download indipendente PASS: digest degli artifact, ZIP/PE AMD64, hash EXE e
sorgenti, risorse/licenze, istruzioni build/relink e adattamenti JUCE effettivi.
Confrontati i sorgenti critici col commit: sola differenza CRLF/LF della
conversione Git Windows, non modifiche al codice. Gli hash degli archivi e
file JUCE modificati sono verificati sui byte esatti. Import table: soltanto
le 21 DLL di sistema Windows già previste, nessuna DLL da installare/bundlare.

Contenuto: `Standalone/Fase Groove Arranger.exe`, manuali/risorse, licenze,
`Source/` con sorgenti completi app/JUCE e istruzioni. Il solo adattatore GPL
è sorgente con COPYING; **nessun runtime eCantorix, Perl, eSpeak, SoX, FFmpeg,
DLL aggiuntiva, codec esterno, vocoder o voicebank distribuito**.

### Errori incontrati e correzioni

Il push iniziale fast-forward include i sei commit fino a `ffaa0ec6`; nessuna
divergenza remota. Riutilizzato il workflow esistente e aggiunti i due harness UI
già presenti. La run 43029786229 è stata cancellata dalla sostituzione del workflow.
La run 43030410827 ha passato build e 18 CTest, ma fallito 5 asserzioni VST3:
il vecchio test premeva `+` aspettandosi il browser, invece del nuovo menu.
Corretto **solo il codice self-test** per selezionare davvero Plugin tramite
l'azione di accessibilità JUCE; preservate tutte le asserzioni e aggiunti i due
percorsi del menu al test nativo. La run 43035822163 ha passato build/18 CTest,
ma rilevato 1 asserzione sul ripristino dopo Escape: sostituita la pausa fissa
30 ms con attesa limitata sullo stato reale, senza forzare l'apertura o cambiare
il comportamento produttivo. Verifica locale limitata al menu: 14/14 PASS;
nessun audit Linux ripetuto. Nessuno ZIP programma dalle run fallite.
La run finale sopra supera anche i controlli VST3 e il packaging.

### Prove ancora mancanti e avvio

**NOT RUN: 5 ambiti**, distinti dai 18 CTest passati: collaudo manuale Windows 11,
driver/hardware/ascolto/dropout; plugin di terze parti; sintesi reale esterna;
regressione opzionale input sintetico su peer (`window-focus.txt` SKIP esplicito);
sanitizers Windows. Il CTest vocale passa il piano MIDI, non la sintesi assente;
anche render/import effettivi dell'editor sono NOT RUN. I sanitizers mirati
precedenti restano risultati Linux. Fasi 01/02 congelate senza approvazione
qualitativa; Loud resta non esposto per gate IMD.

Aprire la run verde e scaricare l'artifact del programma, non quello diagnostico.
Estrarre l'archivio Actions e lo ZIP portabile interno; conservare tutte le
cartelle. Avviare **Standalone\Fase Groove Arranger.exe**, scegliere il dispositivo
audio e aprire una copia del progetto. Non servono WSL, Bash o compilatori.
[Checklist Windows 03–06](WINDOWS_AUDIO_CHECKLIST.it.md) e
[istruzioni voce guida opzionale](../VOCALTEST_QUICKSTART.it.md).

La build appartiene al commit `353ae3d`; l'eventuale successivo commit `[skip ci]`
aggiorna soltanto questo registro/stato/documentazione. Non è un secondo binario.
Nessun main, merge/PR, tag o GitHub Release; fase 07 non iniziata.

## Archivio: prima preview precedente alle fasi 03–06

Il workflow esistente `.github/workflows/build.yml` ora include un job dedicato
**Vocaltest Preview Windows x64**, avviato dai push o dispatch di **Vocaltest**.
Main non viene modificato. La famiglia di job release è esclusa da questo branch;
permessi contents:read, nessun tag o GitHub Release.

**Consultare la run Actions della specifica revisione per i risultati nativi.**
Il primo tentativo si è fermato sul fixture di permessi Unix eseguito su NTFS:
corretto l'harness affinché quel controllo resti su POSIX, mantenendo i test Windows.
Il secondo tentativo ha superato build e CTest 15/15 (80,76 s), poi ha rilevato
FFmpeg assente nella verifica MP3. Riutilizzato il passo di installazione QA già
presente nel workflow standard: FFmpeg non entra nello ZIP. Risultati definitivi
e run di riferimento sono riportati in 02_STATUS.md. La terza run ha superato
anche UI/WAV/MP3 ed effetti VST3, rilevando poi il caso di focus logico perso con
peer nativo già focalizzato e processo non foreground. Correzione UI minima;
la stessa asserzione è passata nella run 42917662978, insieme a tutti i test
nativi. Il gate dei sorgenti ha poi intercettato gli adattamenti CMake legittimi di
JUCE: il packaging è stato corretto per includerli effettivamente e viene ripetuto.

## Risultato verificato

**PASS** [run 42920754927](https://github.com/wsx79/fasegroovearranger/actions/runs/42920754927),
commit di build `0649e93d915f762f29e85adf427bfe08ded164d5`.
Windows Server 2025 / windows-2025-vs2026, MSVC 19.51.36260.0, Release x64.
CTest **15/15** (80,76 s), UI/avvio/WAV/MP3, VST3 effetti/strumenti, regressione
focus, import asincrono **11** e Vocal Studio senza helper **14**: tutti PASS.
Anche eseguibile estratto, packaging e upload sono PASS. Scaricato e ricontrollato
l'archivio: digest Actions, PE64, risorse, sorgenti/licenze e checksum confermati;
import table dell'EXE con sole DLL di sistema Windows, senza runtime aggiuntivi.

[Scarica l'artifact](https://github.com/wsx79/fasegroovearranger/actions/runs/42920754927/artifacts/13811964799),
41.692.092 byte per lo ZIP portabile interno, disponibile fino al 10 dicembre 2026.
[Dettagli riproducibili e SHA256](https://github.com/wsx79/fasegroovearranger/blob/Vocaltest/docs/VOCALTEST_PREVIEW_RESULT.json).
Resta una preview per test manuali: sintesi esterna e Windows 11/hardware/ascolto
non sono PASS. I precedenti blocchi (fixture Unix su NTFS, decoder QA assente,
recupero focus e inclusione degli adattamenti JUCE) sono risolti nella run verde;
nessuna asserzione nativa è stata saltata per ottenere il risultato.

## Gate automatici

- Runner `windows-latest`, MSVC x64, Release; C++17, CRT statico e JUCE 8.0.9 come
  previsto dal CMake del progetto. Host/effect probe VST3 originali solo per test.
- Risorse/guide e fixture packaging, CTest verbose, UI/avvio, WAV/MP3, VST3
  effetti/strumenti con gli harness esistenti; test specifici Vocal Studio e import.
- Nessun motore musicale o formato progetto modificato per preparare la preview.
- ZIP solo dopo tutti i gate; verifica PE32+ AMD64, risorse, checksum, presenza dei
  sorgenti e assenza dei runtime esterni non approvati. Esecuzione del test import/UI
  anche dall'eseguibile estratto. Upload del programma soltanto al successo.
- I log diagnostici possono essere caricati dopo un errore: non sono una build
  scaricabile funzionante. La CI usa i tool del runner per QA, senza copiarli nello ZIP.

## Artifact e sorgenti

Pagina: https://github.com/wsx79/fasegroovearranger/actions/workflows/build.yml

Nome artifact **Vocaltest-Preview-Windows-x64**; contiene
**FaseGrooveArranger-Vocaltest-Preview-Windows-x64.zip**, conservato per 30 giorni.
Scaricare dalla sezione Artifacts di una run verde del branch Vocaltest; può
servire il login GitHub. Il doppio archivio è il contenitore standard di Actions.

Lo ZIP portabile contiene `Standalone/Fase Groove Arranger.exe`, guide, icona,
licenze, helper **solo sorgente** prepare.pl/COPYING, `LEGGIMI-PRIMA.it.md`,
PREVIEW.json con SHA e limiti, SOURCE.txt e Source/ con archivi Git dei sorgenti
esatti FGA e JUCE compilati, rispettive notice e istruzioni rebuild/relink Shine.
I source ZIP sono completi per i file tracciati, non semplici URL a commit locali;
modifiche inattese o submodule non censiti vengono rifiutati. Gli adattamenti
JUCE già applicati dal CMake del progetto sono inclusi nello snapshot effettivo,
con diff, notice datata e hash dei file modificati, invece di copiare solo HEAD upstream. Il verificatore
controlla checksum e integrità degli archivi prima dell'upload.

## Sintesi vocale e limiti

**Rendering eCantorix Windows NOT RUN.** Il workflow non installa né distribuisce
Perl/XS, eSpeak, SoX, dati vocali, DLL aggiuntive, codec, modelli o voicebank.
Il test dell'editor senza helper prevede 14 controlli e una voce esplicita
NOT RUN per la sintesi esterna: non equivale ai 22 controlli Linux con helper reale.
Versioni, hash, licenze e funzionamento degli archivi Windows effettivi restano
una verifica separata prima di qualsiasi futura redistribuzione. La separazione
in processi non è un'esenzione GPL. [Termini e fonti](VOCAL_GUIDE_LICENSES.md).

I test numerici/sintetici della CI non certificano ascolto, dispositivi reali,
prestazioni realtime, tutti i VST di terzi o avvio su Windows 11 dell'utente.
La versione applicativa resta 0.43.2; nessuna fase 03 avviata.

## Prova dell'utente

Aprire **LEGGIMI-PRIMA.it.md** nello ZIP. La stessa guida è consultabile su
[GitHub](https://github.com/wsx79/fasegroovearranger/blob/Vocaltest/VOCALTEST_QUICKSTART.it.md).
Contiene configurazione nativa senza WSL/Bash e checklist Windows 11 per avvio,
progetto esistente/save/reopen, mixer/audio, testo inglese/note/render/import/export,
Vocal Chops/slicing/snap/Undo/Redo, crash e blocchi. Tutte le prove manuali e
l'ascolto sono **PENDING** finché l'utente non registra gli esiti.

La precedente ricetta `scripts/test-vocaltest-preview.ps1` rimane disponibile
per una macchina con stack guida già configurato; non viene presentata come
eseguita o superata dal workflow senza quei componenti.
