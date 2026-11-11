# Collaudo integrato 03–06 — Linux, 2026-11-11

Base `371d51059be730ee81cadbe711b29b266a4e56c4`, branch **Vocaltest**.
Cronologia verificata: `0bdd4ed` (03), `079565d` e `a46237b` (04), `e637d84`
(05), `371d510` (06) sono tutti antenati di HEAD. Directory inizialmente pulita;
nessun main, push, tag, merge, workflow dispatch o release. Fase 07 non avviata.
Vocal Studio/Chops rimangono congelati e non approvati funzionalmente.

## Ambito e riferimenti

Riutilizzati i target e gli harness engine/UI esistenti. Nessun nuovo motore,
feature, routing, formato di progetto o dipendenza. Audit e corpus storico
True Peak/THD+N non ripetuti: riferimenti [03](AUDIO_ENGINE_PHASE03.md),
[04](OFFLINE_EXPORT_MODES.md), [05](DYNAMICS_PHASE05.md),
[06](SIDECHAIN_PHASE06.md). Le nuove prove riguardano punti d'integrazione,
edit durante playback e verifiche sanitizer prima mancanti. Non sono un
collaudo hardware né un'approvazione qualitativa delle funzioni vocali.

## Bug riprodotti e correzioni circoscritte

**Edit sidechain.** Su portante DC 0,25, profondità 0,8 e sorgenti controllate,
24 combinazioni (44,1/48/96 kHz, blocchi 32/512, cambio sorgente/attack/curve/
release) riproducono un salto massimo di **0,182634**. Il vecchio test dei
controlli verificava i parametri e i valori stazionari, non questa discontinuità.

`MixerBuses` raccorda il guadagno dopo un edit per **10 ms**, con un offset che
parte dalla riduzione effettivamente applicata e torna linearmente a zero.
Gli edit ripetuti ripartono dal guadagno corrente. Riduzione limitata a 0–1,
stesso guadagno sui due canali. Nessun buffer aggiuntivo, lookahead o latenza;
solo stato fisso e primitive di smoothing già usate. Il raccordo è attivato
solo cambiando sorgente/timing/curva durante playback, non sui normali colpi del
detector. Edit a motore fermo e reset non lasciano raccordi pendenti.

Dopo la correzione: **24/24 PASS**, massimo passo **0,000434861**, più 200
edit ravvicinati con guadagno limitato e stereo link. Il riferimento storico
resta esatto campione per campione nelle 12 combinazioni preesistenti. È una
riduzione numerica del salto, **non** la prova d'ascolto o la garanzia universale
che non si sentano click: l'attacco istantaneo sui colpi rimane intenzionale.

**Contatore dei test.** ASan ha riprodotto `alloc-dealloc-mismatch` su una
allocazione STL `new(nothrow)` intercettata dal sanitizer e liberata dal `free`
del contatore globale. Completate soltanto le sostituzioni test-only nothrow
per usare lo stesso percorso new/free; regressione esplicita sul conteggio.
Nessun generatore modificato e nessun controllo ASan disabilitato. Il contatore
copre C++ ordinario/nothrow, non tutte le allocazioni allineate/C/driver.

**Stress Handoff.** Entrambe le corse sanitizer restavano in attesa nel test
concorrente: il producer terminava i `post()` e faceva `join`, mentre il consumer
non poteva adottare l'ultimo pending se lo slot retired era occupato. Il test
aveva smesso di chiamare `collect()` sul controllo, come invece richiesto dal
contratto. Aggiunta una riproduzione deterministica dell'interleaving
pending+retired, raccolta sul controllo fino allo svuotamento e deadline con
uscita del consumer prima del join. In questa correzione del test l'implementazione
produttiva di `Handoff` resta invariata. Le prime corse bloccate sono state terminate per questa
diagnosi: non sono conteggiate come PASS né descritte come dropout del motore.

**Race nel restore plugin/PDC.** TSan, con core/events JUCE instrumentati,
riproduce una lettura di `Instrument::delay` in `restoreHosted` /
`delayStorageMissing` concorrente alla scrittura del callback in
`refreshLatencies` (Source/AudioEngine.cpp). Il mutex di controllo non esclude
il callback per progetto: la scansione precedente era quindi priva della
sincronizzazione necessaria. Ora il controllo legge il flag atomico
`delayStorageNeeded` già pubblicato dal callback e prepara le linee solo sotto
il gate di controllo esistente. Aggiunta regressione di richiesta PDC a 256
campioni, restore e verifica delle capacità; riusato il restore concorrente del
test. Nessun nuovo lock/allocazione sul callback, nuova PDC o refactoring.

**Contatore del plugin fittizio.** Dopo la correzione PDC, una corsa TSan
diagnostica senza arresto alla prima segnalazione ha isolato una race residua
solo nel test: `AuditPlugin::calls` veniva letto durante la scrittura audio.
Spostate le letture prima dell'avvio/dopo il join; mantenute le verifiche di
avanzamento del callback, latenza del controllo e assenza di snapshot. Il plugin
fittizio non è codice di produzione né un VST3 di terze parti. La corsa finale
usa nuovamente arresto al primo errore, senza suppression.

## Integrazione automatica

Il test sidechain estende la fixture di playback con campioni già esistente:
compressore nativo su traccia e master, GR attivo, source post-track/pre-group,
automazione threshold e pan della sorgente, save/load effettivo `.fga`, copia
privata, export Transparent, annullamento all'ultimo progress, conservazione
byte-per-byte del WAV preesistente, sovrascrittura 24→16 bit con ceiling diverso,
errore di destinazione e bypass live con adozione dei controlli.

- PASS: il render privato lascia avanzare il transport live. È interleaving
  deterministico di callback nel progress, non una prova di scheduling hardware.
- PASS: picco float pre-Transparent **1,90548**; WAV finale 48 kHz/16 bit,
  ceiling −3 dBTP: **−3,050041 dBFS**, FFmpeg indipendente sinc 32×, filtro 256,
  zeri prima/dopo, funzione QA già esistente. Verificata questa nuova fixture,
  senza ripetere l'intero corpus storico.
- PASS: **0 allocazioni C++ misurate** in playback combinato, adozione di nuovi
  controlli e bypass. Zero latenza algoritmica aggiunta; nessuna modifica PDC.
- PASS UI/Xvfb: mixer sidechain con Undo/Redo e MIDI invariato; compressore
  **12/12**, inclusi track/master, bypass, stato, riapertura ed export;
  export **17/17**, incluse preferenze Transparent, formato, annullamento ed errori.

## Sanitizers: copertura e limiti

GCC 14, ASan/UBSan e TSan nativi Linux; runtime prima verificati con una piccola
prova di avvio. Build QA separate fuori repo, senza cambiare il profilo Release.
Instrumentate dieci unità: entry point test, test sidechain/dynamics/EngineAudit,
AudioEngine, MixerBuses, NativeCompressor, OfflineExport, StudioIO e ProjectIO.
Per TSan instrumentati anche JUCE core/events: la prima corsa parziale segnalava
un lifetime del timer; la dinamica non lo riproduce ampliando la visibilità di
thread/refcount. Nessun codice JUCE modificato, nessuna suppression.

Risultati finali dei tre comandi per sanitizer riportati nella tabella conclusiva.
Altri moduli JUCE, libreria dei generatori e altri TU rimangono non instrumentati:
non è un PASS ASan/TSan dell'intera DAW. Le fixture del plugin e del dispositivo
non sono driver o VST3 esterni reali. I tempi dei sanitizers non sono prestazioni
Release né prove di dropout.

## Prestazioni e prove mancanti

Il nuovo raccordo usa solo stato fisso, senza ulteriore memoria PCM o allocazioni
al callback. Rimangono i due buffer detector della fase 06 (4.096 byte a blocco
512). Nessuna nuova latenza/PDC. I tempi locali finali sono riportati sotto;
nessuna estrapolazione a CPU, driver, progetti o plugin dell'utente.

Prima corsa con build/link e altri test UI concorrenti: callback combinato
512/44.100, mediana **2,087 ms**, p99 **13,634 ms**, massimo **15,405 ms** su 100
chiamate (budget audio 11,61 ms). Questo mostra che il container sotto carico
può superare la deadline; non dimostra un dropout su hardware né una regressione
DSP. La misura finale senza questi carichi va tenuta distinta. Non si promette
assenza di dropout e non si nascondono gli outlier della prima corsa.

**Windows x64 nativo: NOT RUN.** Questo executor è Linux e non dispone di MSVC,
Windows/ASIO o runner Windows collegato. Non è stato tentato uno pseudo-PASS con
cross-compilazione/Wine. I commit sono locali: il vecchio artifact Windows
`0649e93` non include queste fasi. Per un nuovo artifact serve successiva
approvazione al push e una build/test nativa dello SHA corretto.

**NOT RUN:** macOS, ascolto/ABX, hardware reale, dropout prolungati e VST3 di
terze parti (nessuno installato trovato nei percorsi del progetto/utente/sistema).
Il DSP Transparent/NativeCompressor, i generatori e i componenti vocali non sono stati modificati.
Loud resta escluso dal gate IMD. Checklist umana:
[WINDOWS_AUDIO_CHECKLIST.it.md](WINDOWS_AUDIO_CHECKLIST.it.md).

## Risultati conclusivi

| Verifica | Esito |
| --- | --- |
| Release applicazione e test | **PASS**, GCC 14/JUCE 8.0.9, warning preesistenti non bloccanti |
| CTest pertinenti, seriali | **4/4 PASS, 58,17 s**: project_io 4,96 s; audio_engine 38,36 s; sidechain 12,97 s; dynamics 1,87 s |
| UI mixer/compressore/export | **PASS**, Undo/Redo e MIDI; **12/12** compressore; **17/17** export isolato |
| ASan/UBSan mirati | **PASS** sidechain integrato, dynamics e EngineAudit; nessuna segnalazione finale nei rispettivi ambiti |
| TSan mirato, core/events inclusi | **PASS** dynamics, EngineAudit e sidechain integrato; nessuna segnalazione finale |
| Fixture integrata FFmpeg 32× | **PASS**, −3,050041 dBFS a ceiling −3 dBTP |
| Windows nativo, macOS, hardware/ascolto, VST3 reali | **NOT RUN**, restano pendenti |

Memoria PCM: nessun nuovo buffer rispetto alla fase 06. Raccordo e cache dei
controlli sono stato fisso. Mixer locale: **1.024.000 frame stereo / 155,358 ms**
(two groups active, one non-unit curve), non un confronto normalizzato con il
vecchio benchmark. Callback combinato Release, 100 chiamate da 512 frame a
44,1 kHz, senza build/sanitizers concorrenti: mediana **2,227 ms**, p99
**4,434 ms**, massimo **8,062 ms**, budget rappresentato **11,61 ms**. Non
costituisce garanzia hardware o esclusione di dropout sotto altri carichi.

Una corsa UI export concorrente ai sanitizers aveva **15/17 PASS**, fallendo
completamento/struttura degli stem. Ripetuta isolata, su cartella nuova, con
codice e timeout **30 s invariati**: **17/17 PASS**. Non riprodotto il problema
in isolamento; resta evidenza di sensibilità al carico della prova temporizzata,
non viene nascosto aumentando i limiti o presentato come bug DSP risolto.
Gli altri 14 CTest e il corpus storico completo non sono stati rieseguiti.

Riproduzione del controllo Release: `cmake --build build/native -j2`, poi
`ctest --test-dir build/native -R '^(sidechain|project_io|audio_engine|dynamics)$'
--output-on-failure -j1`. UI con display/Xvfb: `--sidechain-ui-test`,
`--dynamics-ui-test`, `--export-ui-test`, ciascuno con cartella di evidenza nuova.
Eseguire le prove temporizzate senza compilazioni/sanitizers concorrenti.

Ricetta sanitizer usata: comandi dei TU estratti da `ninja -C build/native -t
commands fga_engine_tests`, stessi define/include della build, aggiunti
`-O1 -g1 -fno-omit-frame-pointer` e `-fsanitize=address,undefined` oppure
`-fsanitize=thread`; oggetti e binari QA separati, altri moduli riutilizzati
senza instrumentazione come dichiarato. Comandi `--sidechain`, `--dynamics`,
`--engine-audit`; ASAN_OPTIONS `detect_leaks=1:halt_on_error=1`, UBSAN_OPTIONS
`halt_on_error=1:print_stacktrace=1`, TSAN_OPTIONS
`halt_on_error=1:history_size=7`. Nessun detector disabilitato e nessuna
suppression. Script di orchestrazione locale `build-sanitizers.py` nella
cartella dei log; non fa parte del prodotto.

Nessuna modifica architetturale richiesta da queste correzioni. Per provare il
nuovo SHA su Windows serve un ambiente Windows nativo; se si usa Actions sul
branch remoto, il push dei commit locali richiede nuova autorizzazione.
Fase 07 e pubblicazione **non autorizzate**.

Log e fixture fuori repository: `/workspace/scratch/mix-integration/`; binary QA,
comandi di build e liste dei TU in `sanitizers/`; risultati CTest finali anche in
`build/native/Testing/Temporary/LastTest.log`. Le fixture sonore non sono incluse
nel commit né pubblicate.
