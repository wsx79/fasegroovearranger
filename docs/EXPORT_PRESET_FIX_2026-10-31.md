# Conferma export WAV e ripristino preset — 31 ottobre 2026

Base: `be8e58e54af9856efd0b7653eeec208d5edd6e0d` (**v0.30.3**). Pubblicazione **0.30.4 autorizzata dall’utente**: commit, push e release nelle modalità della famiglia, con guide e quattro pacchetti di distribuzione.

## Segnalazioni e correzioni

**Export WAV:** dopo ESPORTA nella finestra di codifica non appariva la scelta del percorso; la riproduzione continuava perché il rendering non era mai iniziato. Il callback avviava il file chooser mentre `ModalComponentManager` doveva ancora distruggere la finestra precedente. Ora vengono letti i valori, nascosta la finestra e rimandata la continuazione al ciclo di messaggi successivo, dopo la sua distruzione. Il percorso riprende quindi la normale sospensione del callback audio e il rendering sulle istanze caricate. Annullamento ed errore continuano a ripristinare i comandi.

I test precedenti aprivano la codifica e la annullavano, poi verificavano il rendering con una destinazione già fornita. Mancava la conferma attraverso il pulsante ESPORTA. La nuova prova attraversa il pulsante e la richiesta del percorso, verifica che la vecchia finestra sia stata distrutta e consegna la destinazione al vero codice di rendering. Solo l’interazione con il selettore file è sostituita nel test; WAV, sospensione, thread e ripristino sono reali.

**Plugin muti alla riapertura:** il ripristino nativo avveniva su un processore già attivato. Ora il componente viene disattivato, riceve lo stato e viene preparato nuovamente. Si aggiornano i buffer secondo i canali correnti; per gli strumenti si risincronizzano anche le note tenute. Gli editor restano associati alla stessa istanza. I checkpoint del mix e il bypass continuano a evitare ripristini nativi.

**Preset selezionato:** quando il plugin espone più programmi, il salvataggio conserva anche indice e nome del programma, insieme al chunk nativo e ai valori dei parametri. Alla riapertura viene ripristinato il programma mancante/differente e mantenuta la configurazione dei parametri. I campi aggiunti sono facoltativi dentro `FGAHOST1`: i salvataggi precedenti e i chunk nativi dei formati 1/2/3 restano leggibili. Non viene eseguito DSP per salvare lo stato.

La guida ricorda di salvare il progetto Arranger dopo la scelta del preset, anche quando il preset è stato salvato nell’editor del plugin.

## Evidenze

- Prima della correzione, il test di conferma WAV **fallisce** perché la richiesta del percorso avviene con la vecchia finestra ancora esistente.
- Prima della correzione, una sonda di ripristino **fallisce** perché riceve lo stato durante l’attivazione del processing.
- Dopo: compilazione Release Linux, **CTest 7/7 PASS**; ripetuto il test motore dopo l’aggiunta della prova che Play/seek restano inibiti per tutto il rendering.
- VST3 reale di prova con stato sensibile all’ordine di attivazione: apertura/Play dei formati 1/2/3 e aggiornamento a 4, stato e automazioni. Il test seleziona il preset non standard **Bright** e gain **0,37**, salva **prima di Play**, riapre e verifica programma e gain. Il WAV restituisce **880,4 Hz**, distinto dai **441,0 Hz** del preset standard.
- Conferma reale della codifica fino alla richiesta di destinazione, WAV udibile 48 kHz/16 bit, stem 44,1 kHz/24 bit e TXT, nomi MIDI, sospensione della riproduzione, annullamento ed errore: PASS.
- Catene di effetti VST3, ordine, automazioni, salvataggio/ripristino con editor aperto e 16 cambi di bypass senza blocchi silenziati: PASS.
- Suite UI/audio: 13 stem allineati a 305809 frame / 44100 Hz, WAV/MP3 e rigenerazioni udibilmente differenti: PASS. Modulo condiviso, tema/font, traduzioni, guide e diff controllati.

Korg M1 e il progetto originale dell’utente non sono disponibili nell’ambiente. Le regressioni sono riprodotte con sonde native e la correzione è verificata su Linux; il risultato specifico del preset M1 e la finestra nativa Windows richiedono la prova sul PC dell’utente. La prova automatica del selettore verifica la richiesta e la sequenza delle finestre, non l’interazione con la finestra di sistema Windows. Gli esiti Windows/macOS/Linux sono nella CI della release 0.30.4; resta necessaria la conferma specifica sul Windows dell’utente.

## Ripristino

Report, log prima/dopo, manifest e patch sono in `/workspace/reports/release-0.30.4/`.

`forward.patch` applica tutto questo intervento alla base v0.30.3; `reverse.patch` lo annulla senza rimuovere i miglioramenti della release precedente. Conservare eventuali modifiche successive, controllare `git apply --check /workspace/reports/release-0.30.4/reverse.patch` e applicare la patch solo se il controllo riesce. Ricompilare l’app dopo il ripristino. Il ciclo completo è verificato con un indice temporaneo; nessun commit/tag viene riscritto e nessun progetto o campione dell’utente viene modificato.

Dopo la pubblicazione, su checkout pulito, `git revert --no-commit v0.30.4` prepara il ripristino dei sorgenti alla 0.30.3. Controllare il diff e ricompilare prima di autorizzare un eventuale commit e push di ripristino. Il manifest esterno `publication.json` registra commit, CI e asset effettivamente pubblicati.
