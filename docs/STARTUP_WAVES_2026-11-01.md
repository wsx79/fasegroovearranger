# Avvio e plugin Waves — 1 novembre 2026

Intervento locale successivo ai fix dell'audit, conservati. Commit, push e release 0.31.2 successivamente autorizzati dall’utente. Segnalazione reale: Ableton riconosce i VST3 Waves, Arranger mostra soltanto Sync VX; installazione indicata dall'utente come «23.6.26».

## Avvio

Nel codice ho verificato tre costi evitabili: il browser ricostruiva i duplicati per ogni produttore, la deduplicazione confrontava ogni tipo con tutti i precedenti e poteva leggere/hashare interi file VST sul thread grafico; il costruttore del catalogo riscriveva inoltre KnownPlugins.xml anche senza modifiche.

Ora il browser prepara una sola lista canonica e conta i produttori senza ricostruirla. La deduplicazione raggruppa prima per identità del tipo e confronta i binari solo all'interno del gruppo, preservando classi diverse nello stesso contenitore. La UI non calcola hash mancanti: usa BinaryHashes.xml, con chiavi legate a percorso, dimensione e modifica del file. Il worker completa gli hash fuori dal lock del catalogo e dal thread grafico, salvandoli atomicamente. Durante un controllo può restare temporaneamente visibile una copia non ancora verificata. Il caricamento iniziale del catalogo non riscrive la lista.

La regressione verifica che una richiesta di hash non presente nella cache restituisca subito sul thread UI e che il worker consenta poi la deduplicazione corretta. Le prove di grandi cataloghi registrano separatamente il costo della ricostruzione ripetuta e quello di una passata unica, controllando che i conteggi coincidano.

`--startup-benchmark <file>` è una diagnostica compilata solo con FGA_SELF_TESTS: misura la costruzione della finestra fino a setVisible e riporta il numero di plugin caricati. FGA_BENCHMARK_PLUGIN_CACHE seleziona una cache di prova soltanto in questa modalità. I risultati con 0/2000 plugin e tre ripetizioni sono in startup-times.json. Non sono una misura dell'avvio completo su Windows: non includono il caricamento del processo e non quantificano driver, filesystem o plugin dell'utente. Le prime misure con un processo GUI concorrente sono state scartate e non sono usate nelle conclusioni.

## Waves

JUCE 8.0.9 termina findAllTypesForFile appena trova descrizioni nel moduleinfo.json. Un manifest del solo wrapper può quindi nascondere la factory reale. L'adattamento in cmake/ScanWaveShell.cmake forza l'enumerazione nativa per i nomi WaveShell, mantenendo il percorso rapido degli altri plugin. Si applica in configurazione alla dipendenza JUCE selezionata e fallisce esplicitamente se il punto previsto cambia in una futura versione, invece di saltare silenziosamente la correzione.

La policy cache passa a 3: contenitori WaveShell e cache multiclasse precedenti vengono ricontrollati una volta in background. RESCAN mantiene la scansione completa; nessun file Waves viene spostato o modificato.

La regressione usa un vero probe VST3 rinominato WaveShell, con un manifest che pubblicizza una classe sintetica inesistente: il risultato deve essere la sola classe della factory nativa. Restano i test multiclasse da manifest, sottocartelle, link, fallimenti, retry, migrazione cache e copie identiche/diverse. Non equivale a un test di Waves installato/licenziato: i binari dell'utente non sono disponibili qui. Dopo l'aggiornamento verificare gli altri effetti Waves e, se ancora assenti, consultare REPORT SCANSIONE per percorso, numero di classi ed eventuali errori del contenitore. Il caso descritto dall'utente esclude la semplice differenza VST2/VST3.

## Evidenze e ripristino

Log di build, scanner nativo, test, rendering e misure: /workspace/reports/startup-waves-2026-11-01. Guide IT/EN aggiornate. Windows/macOS reali restano da verificare dopo l'autorizzazione alla pubblicazione.

La patch inversa incrementale nella stessa cartella preserva i fix precedenti dell'audit. Verificare prima `git apply --check rollback.patch` dalla repo, usando il percorso completo al file, e applicare solo se non vi sono conflitti. Non forzare sopra modifiche successive. Il manifest registra gli alberi e gli hash e la verifica di andata/ritorno su indice temporaneo. BinaryHashes.xml è una cache locale: il codice precedente la ignora e non cambia il formato dei progetti.

Esiti finali Linux: build Release riuscita, 7/7 suite CTest superate, scanner isolato e migrazione cache superati, controllo hash freddo escluso dal thread UI superato, interfaccia e rendering WAV/MP3 con 13 stems superati. Le misure di conteggio su 2.000 plugin/100 produttori mostrano circa 90 ms per la ricostruzione ripetuta (già con la nuova deduplicazione) contro meno di 1 ms per la passata unica. Questo confronto riguarda quel lavoro del catalogo, non un fattore di accelerazione dell'avvio complessivo. Le misure della finestra con 2.000 descrizioni verificate dalla diagnostica sono nell'ordine di 60 ms su questo ambiente Linux; non vanno estese al Windows dell'utente.
