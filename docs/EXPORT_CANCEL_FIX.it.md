# Annullamento export: blocco del message thread

Aggiornamento 12 novembre 2026: la correzione è ora verificata automaticamente
su **Windows nativo MSVC x64**, run 43184992493, codice `f98b706`. Vecchio
handshake bloccante riprodotto con timeout finito; nuovo wrapper PASS nelle
matrici interno 44/44, VST3 strumento 44/44 ed effetto 52/52. Il caso sul PC/plugin
dell'utente resta OPEN. Risultati e download in [verifica Windows](WINDOWS_CANCEL_VALIDATION.it.md).
Le sezioni seguenti conservano le evidenze e autorizzazioni storiche del fix.

## Evidenze e causa riprodotta

Base: `Vocaltest`, `26e2422e7519f5ef51c0a023ecc79072957ab529`.
Segnalazione originale: annullamento export/render su Windows blocca la DAW.

Percorso precedente:

1. `MainComponent::exportFile` sospende la riproduzione, rimuove il callback
   dispositivo e mantiene `ExportTask` di proprietà della finestra.
2. Il pulsante Cancel del dialogo JUCE termina lo stato modale.
3. `ThreadWithProgressWindow::timerCallback` sul message thread chiama
   `stopThread(15000)`: invia il flag **e attende sincronicamente** il worker.
4. `AudioEngine::exportAudio` controlla il flag tra blocchi, scansioni, scritture
   ed encoding. Alla fine/cancellazione, il Guard ripristina rate/buffer/PDC.
5. Il ripristino di un VST3 può richiedere `MessageManagerLock` in
   `prepareToPlay`. Il worker aspetta quindi il thread UI che lo sta aspettando.
6. Scaduto il timeout, JUCE tenta il kill; il backend Windows usa
   `TerminateThread`, senza garanzia di unwinding C++/rilascio dei lock o del
   Guard export. Il successivo riaggancio audio può rimanere bloccato.

Prova deterministica **prima della correzione: FAIL**: subclass di `ExportTask`
che, ricevuto il Cancel tramite il vero pulsante JUCE, richiede una chiamata al
message thread durante il cleanup. Il thread UI non la esegue prima della scadenza
del wait finito di 800 ms del probe. Nessun kill forzato è necessario nel
riproduttore: il wait finito evita di corrompere il processo di test.
Gli altri 17 controlli precedenti passavano. Il vecchio test chiamava soltanto
`signalThreadShouldExit`, senza chiudere il dialogo tramite il pulsante: non
attraversava il ramo `stopThread` responsabile.

Questo riproduce un difetto concreto del gestore, non prova una particolare
stack trace del plugin/PC dell'utente: la conferma manuale Windows resta pendente.

## Correzione minima e lifecycle

`RenderProgressTask` sostituisce solo il wrapper UI/worker usato da export e
Freeze, seguendo il polling cooperativo già presente in `MediaTask`, senza
modificare quest'ultimo o le funzioni vocali.

- Il pulsante invia immediatamente il flag, mostra l'annullamento e disabilita
  altri click. La chiusura del dialogo viene osservata dal timer senza attese.
- Il timer legge lo stato/progresso e continua a far dispatchare la UI; non
  chiama join/stop/kill. Progresso atomico, valore per la ProgressBar scritto
  solo dalla UI; messaggio protetto da lock breve e lettura UI con try-lock.
- Il job rimane di proprietà di MainComponent finché il worker non è uscito,
  inclusi distruttori/Guard, scrittura e ripristino del motore. Solo allora
  avvengono `threadComplete`, riaggancio dispositivo e riabilitazione editing.
- Riutilizzato `RetiredJobs` per eliminare i job terminati; nessun trasferimento
  del motore o nuovo host. La chiusura normale dell'app durante il job richiede
  cancellazione e conserva l'owner: riprovare la chiusura dopo il cleanup.
- Il job verifica anche la cancellazione prima di scrivere la mappa degli stem;
  gli eventuali file già completati negli stem conservano il comportamento
  storico. Le `future` degli stem vengono attese **dal worker**, non dalla UI.
- Gestito esplicitamente il mancato avvio del worker. Nessuna patch a JUCE.

`AudioEngine`, percorso real-time, Transparent/True Peak, writer/commit atomico,
plugin/PDC/routing, generatori, progetti e moduli vocali restano invariati.
Nessuna nuova dipendenza o distribuzione di binari. Non introdotta una nuova
architettura DSP; un solo wrapper comune per due job che avevano lo stesso bug.

## File modificati

- `Source/RenderProgressTask.h`: wrapper cooperativo comune a export/Freeze.
- `Source/Menus.cpp`: sostituzione dei due wrapper, controllo early-cancel e
  regressioni UI sotto `FGA_SELF_TESTS`.
- `Source/MainComponent.h/.cpp`: ownership tipizzata, hook solo di self-test e
  guard della chiusura normale mentre il job è attivo.
- `Source/UiAuditTests.cpp`: include del tipo completo per distruzione sicura.
- `Source/ArrangerTranslations.inc`: stato di annullamento in italiano.
- Stato, changelog e nota sul blocker della preview: documentazione soltanto.

## Test mirati

`verifyExports`, riusato da `--export-ui-test` e dal test nativo host, include
il probe di deadlock e sette casi con gate **solo di self-test** sul progresso.
Il gate garantisce che il click non arrivi dopo la conclusione di un job breve;
una chiamata UI ritardata deve ancora essere eseguita mentre il worker è attivo.

| Caso | Strumenti interni Linux | VST3 reale Linux |
|---|---|---|
| Immediatamente dopo launch, prima del render | PASS | PASS |
| Render float e scrittura staging | PASS | PASS |
| Scaling offline/scrittura PCM | PASS | PASS |
| Verifica finale prima della pubblicazione atomica | PASS | PASS |
| Cancel ripetuti, chiusura richiesta durante il job | PASS | PASS |
| Stem e future sul worker | PASS | PASS |
| Gruppo, sidechain e ritorni A/B con effetti caricati | PASS | PASS |

Per ciascun caso: bytes precedenti conservati, niente staging residuo nella
cartella, stessa istanza plugin/PDC; sugli stem sono ammessi soltanto nomi di
WAV/mappa completati. Lo zero non viene usato come falso segnale di conclusione:
si attende l'effettiva fine del job. Infine un nuovo render produce un WAV
completo a 48 kHz/24 bit. I self-test non cambiano il percorso del build senza
self-test: il callback/gate di prova è escluso con `FGA_SELF_TESTS=0`.

- **PASS Release Linux**, app e fixture native `ArrangerHostProbe`/
  `ArrangerEffectProbe`, derivate dai sorgenti già esistenti.
- **PASS UI export 43/43** isolato; **PASS 43/43 con VST3 realmente caricato**
  tramite `scripts/test-native-host.py`, più Freeze/Unfreeze e controlli stato,
  program, multi-output, riapertura/PCM. Non sono plugin commerciali.
- **PASS effetti VST3**: harness esistente e 18 file PCM indipendenti/stati.
- Probe locale: circa **27–35 ms** dal click al completamento con la UI attiva;
  è una misura del probe, non un limite garantito per plugin/disco/hardware.
- **PASS CTest 18/18**, seriale, **188,95 s**, incluse regressioni engine,
  PDC, sidechain, dinamica e Transparent/annullamento/sovrascrittura.

Esiti intermedi non nascosti: il primo caso bus usava per errore `A/B` nel nome
della fixture, generando una sottocartella inesistente: corretto solo il nome di
prova e verificata la creazione del file precedente. In un run il vecchio test
18-stem ha superato la sua soglia di 30 s: 41 PASS/2 FAIL sul completamento
stem; tutti i nuovi scenari Cancel passavano. Retry isolato **43/43 PASS**,
senza cambiare deadline o DSP. Nessuna causa di regressione prestazionale è
attribuita a quel singolo timeout; nessuna garanzia di throughput/dropout.
Il primo avvio su un vecchio socket DISPLAY non attivo non è un test applicativo:
Xvfb ripristinato su `:114`, poi riproduzione e test effettuati su quel display.

Log fuori repository: `/workspace/scratch/export-cancel-review/`: `before-display`
(riproduttore FAIL), `after-probe`, `matrix-internal-retry`, `native-vst3`,
`native-effects`, `ctest-final.log`, `build-*.log`. Nessuna nuova modifica al
workflow: il test resta agganciato al percorso export UI già usato da Actions.

## Main, Windows e limiti

Main letto senza checkout/modifiche: commit
`9a4aefb9d2b0b67aee3d2eaf708a12dd24605aa6`. La base/costruttore di ExportTask
sono identici a quelli di Vocaltest prima del fix (`ThreadWithProgressWindow`,
15000 ms), con lo stesso JUCE 8.0.9: **il medesimo meccanismo rischioso è presente**.
Non compilata/eseguita l'intera DAW di main; il runtime main è **NOT RUN**.
Nessun backport, merge o modifica di main effettuato.

**Windows nativo della modifica: NOT RUN.** Il workflow MSVC esiste, ma il runner
può usare soltanto il codice disponibile su GitHub. L'utente autorizza solo
commit locali e vieta push: non è stata pubblicata la correzione né rilanciata
la vecchia build spacciandola per verifica del fix. Il precedente PASS Windows
non copriva il pulsante Cancel. Necessari futuro build/test nativi e collaudo
manuale del PC/plugin dell'utente prima di chiudere il gate Windows.

**NOT RUN:** mouse/hardware/ascolto Windows, macOS, plugin vendor e nuovi
sanitizers del percorso UI. Cancellazione cooperativa: un plugin/driver che non
ritorna da una propria chiamata può ritardare la fine del worker. Non viene
terminato forzatamente; l'isolamento in processi separati non fa parte del fix.
Nessun nuovo benchmark CPU/RAM/latency; nessun buffer o latenza DSP aggiunto.

Collaudo manuale Windows: avviare un export lungo con il plugin usato nel caso
segnalato, premere Cancel subito, a metà e quasi alla fine; ripetere con ritorni
A/B e stem, più click e chiusura app richiesta durante l'annullamento. Controllare
ridisegno/click UI, fine effettiva del worker, riproduzione successiva, file
precedente e assenza di `_temp...wav`, quindi un nuovo render valido. Registrare
SHA, plugin/versione, rate/buffer e punto preciso dell'annullamento.

Stato: fix locale e verifiche Linux; gate Windows/manuale **ancora aperto**.
Nessun push, merge, tag o release; fase 07 non avviata.
