# Compatibilità scanner VST3 — 1 novembre 2026

Base: v0.31.2 (ff18928). Intervento della 0.31.3; commit, push e release autorizzati dall’utente dopo la prova del progetto diagnostico.

Il report Windows dell'utente distingue SpaceControl 0xC0000005 (violazione di accesso), Scaler 3 0xC0000409 (fail-fast), WaveShell 16.0/16.7 con uscita 1 e WaveShell 17.1 interrotto. Questi esiti non dimostrano che i plugin siano difettosi o che manchi la cartella VST3. Ableton li riconosce come VST3 di sistema. L'utente conferma il prompt nativo Waves per la cartella dei componenti.

## Riscontri e modifiche

JUCE 8.0.9 risolve l'indice della classe prima di assegnare il contesto host alla factory. WaveShell può cambiare numero e ordine delle classi dopo setHostContext. Il problema è documentato da JUCE e corretto upstream il 3 settembre 2026: https://github.com/juce-framework/JUCE/commit/3fa28d0a14f1be97d49603c710afaa6b9b900cdb e https://forum.juce.com/t/69393.

L'adattamento locale in cmake/ScanWaveShell.cmake porta il principio della correzione su JUCE 8.0.9: costruisce il contesto prima della ricerca e lo mantiene identico fino all'inizializzazione del componente. Anche la scansione nativa assegna il contesto prima dell'enumerazione. Gli adattamenti sono verificati in configurazione; i sorgenti degli altri prodotti e le impostazioni Waves non vengono modificati (la dipendenza JUCE selezionata viene adattata durante il build).

Il processo scanner ora avvia il lavoro dal ciclo messaggi JUCE. Durante la scansione usa la cartella del modulo, ripristinandola al ritorno, come già avviene nel percorso di caricamento. Rimane un processo separato. Non si effettuano scansioni o caricamenti dei binari nel thread audio.

Le eccezioni gestibili dello scanner vengono indirizzate a un report/minidump per il singolo tentativo e archiviate in Plugins/Diagnostics, invece di confondersi con crash della DAW. Il report del catalogo conserva il codice esadecimale e l'eventuale modulo/stack. Alcuni fail-fast di Windows possono terminare il processo senza eseguire il crash handler: in quel caso rimane il codice di uscita, non viene inventata una causa. La policy cache 4 riprova una volta in background vecchi fallimenti e contenitori shell/multiclasse, conservando i plugin validi.

## Limiti e prova reale

La cartella richiesta nativamente da Waves contiene i componenti Plug-Ins V16/V17 e non è la cartella VST3 mostrata da Ableton. Fonte ufficiale: https://www.waves.com/support/locate-the-waves-plug-ins-folder. Non esiste qui una verifica di un'API pubblica per impostare quel percorso: non sono aggiunti valori di registro o variabili d'ambiente ipotetici. La correzione riguarda il protocollo host/factory, non modifica licenze o installazioni.

I plugin Waves, Scaler 3 e SpaceControl non sono installati nell'ambiente Linux di prova. Il nesso con i loro specifici crash va quindi confermato sul Windows dell'utente; un probe JUCE non certifica quei prodotti. Le prove native verificano scanner, factory contro manifest, ripristino directory, vecchi progetti, stato degli strumenti ed effetti. Log ed esiti sono in /workspace/reports/scanner-context-2026-11-01.

La patch inversa nella cartella delle evidenze annulla soltanto questo intervento rispetto a ff18928. Prima controllare con git apply --check; applicare solo senza conflitti e senza forzare modifiche successive. Le guide e le release già pubblicate restano disponibili.

## Ripristino del programma nativo

Il progetto fornito dall’utente contiene un programma nativo completo. Una copia diagnostica con il solo snapshot dei parametri host rimosso conserva il blocco nativo identico: l’utente ha confermato che il suono torna corretto anche premendo Play. La riproduzione dei parametri del controller dopo lo stato nativo alterava quindi il risultato. Il progetto originale non viene modificato né distribuito.

Il ripristino riconosce lo specifico formato verificato di programma completo e lascia autorevole il suo blocco nativo. Gli altri formati, inclusi quelli parziali e legacy, conservano il fallback dei parametri. Non sono aggiunti cambi di preset dopo il ripristino che cancellerebbero le regolazioni private. Un fixture sintetico controlla sia il programma completo sia il fallback parziale, senza includere dati privati del progetto.

## Ascolto nel piano roll

Inserire o selezionare note a trasporto fermo le fa ascoltare con il suono assegnato, volume, pan ed effetti della traccia. Il preascolto ha note-off automatico, non viene registrato e si interrompe cambiando selezione o avviando il trasporto.

Il pulsante ASCOLTA CLIP avvia soltanto la traccia e l’intervallo della clip selezionata, ripetendolo mentre si modifica il MIDI. FERMA CLIP arresta la riproduzione. Mute, solo, punti del loop e cronologia Annulla del progetto restano invariati. Il Play generale torna al mix completo. I test controllano isolamento audio, ritorno al loop, modifiche durante l’ascolto, ripristino del trasporto e pulsante UI reale.

## Verifica e ripristino

Le prove comprendono le sette suite CTest e i percorsi nativi di scanner, strumenti, effetti e piano roll. I risultati finali della versione sono conservati nella cartella delle evidenze `/workspace/reports/release-0.31.3`, insieme alla patch completa e inversa dalla base ff18928. Non sono inclusi il progetto dell’utente o i suoi blocchi di stato privati. La conferma Waves/Scaler/SpaceControl sul Windows reale resta necessaria.

Per annullare l’intervento usare il revert del commit della 0.31.3 da un albero pulito, previa autorizzazione. In alternativa verificare prima la patch inversa con `git apply --check`, senza forzare conflitti o cancellare modifiche successive. La 0.31.2 resta disponibile come precedente versione pubblicata.
