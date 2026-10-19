# Privacy e permessi — Fase Groove Arranger

Versione 0.1.0 · 19 ottobre 2026

Fase Groove Arranger è un'app desktop dell'ecosistema FS Pitch, creato da Fabio 'Pitch' Serra. Il codice sorgente e il contatto del progetto sono disponibili su https://github.com/wsx79/fasegroovearranger.

## Dati e connessioni

L'app elabora musica sul dispositivo. Non richiede account, non raccoglie dati personali per un servizio remoto e non contiene analytics, telemetria, pubblicità, cookie o caricamento automatico di file. Non effettua controlli aggiornamenti o connessioni di rete proprie durante l'uso ordinario. La generazione avviene nei motori C++ integrati; non invia prompt o musica a servizi AI.

Le pagine di codice sorgente, Spotify e donazioni si aprono nel browser soltanto quando premi il relativo pulsante. Tali servizi applicano le proprie condizioni e informative. Le dipendenze possono essere scaricate da GitHub durante la compilazione da sorgente; questo non avviene durante l'esecuzione dell'app già compilata.

## File locali

L'app legge progetti, MIDI, campioni e plugin che scegli. Scrive progetti, file temporanei di rendering, esportazioni e una copia periodica di autosave nella cartella dati utente `FS Pitch/Fase Groove Arranger`. Nessuno di questi file viene caricato automaticamente.

I progetti contengono note, impostazioni musicali, nomi delle tracce, percorsi locali di campioni/plugin e stato serializzato degli strumenti. Percorsi e stati plugin possono contenere informazioni del tuo sistema o impostazioni personali. Prima di condividere un progetto, verifica anche i diritti sui campioni e i dati contenuti nello stato dei plugin. I campioni esterni non vengono incorporati automaticamente.

Puoi eliminare i progetti/esportazioni dal file manager e cancellare l'autosave dalla cartella dati dell'app. La disinstallazione di un eseguibile portatile non cancella automaticamente quei documenti. La cronologia undo resta nella memoria del processo e termina alla chiusura.

## Permessi

- Uscita audio: necessaria per ascolto e anteprima. L'app non apre ingressi audio, non usa il microfono e non registra l'ambiente.
- File: lettura dei file scelti; scrittura delle destinazioni di salvataggio/esportazione e dell'autosave locale.
- Plugin nativi: il caricamento di VST3/AU esegue codice del plugin nel processo dell'app. Plugin e relativi servizi possono avere rete, file e licenze proprie, fuori dal controllo dell'arranger.
- Browser: usato soltanto per collegamenti esterni scelti dall'utente.

Non sono richiesti amministratore, rubrica, calendario, posizione, camera o accesso cloud. I permessi effettivamente mostrati dal sistema possono variare in base al packaging, al dispositivo audio e agli strumenti caricati.

Questa informativa descrive il comportamento del codice dell'arranger. Non sostituisce le informative dei plugin o dei siti esterni. Per richieste sul progetto usa il repository; evita di inserire dati privati nelle issue pubbliche.
