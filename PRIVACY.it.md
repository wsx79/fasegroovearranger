# Privacy e permessi — Fase Groove Arranger

Versione 0.22.0 · 26 ottobre 2026

Fase Groove Arranger è un'app desktop dell'ecosistema FS Pitch, creato da Fabio 'Pitch' Serra. Il codice sorgente e il contatto del progetto sono disponibili su https://github.com/wsx79/fasegroovearranger.

## Dati e connessioni

L'app elabora musica sul dispositivo. Non richiede account, non raccoglie dati personali per un servizio remoto e non contiene analytics, telemetria, pubblicità, cookie o caricamento automatico di file. La verifica delle release GitHub è disattivabile. I motori C++ integrati generano musica sul dispositivo. La composizione IA facoltativa si collega al servizio scelto soltanto quando premi COMPONI.

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

L’app non richiede amministratore, rubrica, calendario, posizione, camera o accesso cloud. Solo il setup Windows richiede i permessi di amministratore per installare o aggiornare in Program Files. I permessi effettivamente mostrati dal sistema possono variare in base al packaging, al dispositivo audio e agli strumenti caricati.

Questa informativa descrive il comportamento del codice dell'arranger. Non sostituisce le informative dei plugin o dei siti esterni. Per richieste sul progetto usa il repository; evita di inserire dati privati nelle issue pubbliche.

## Lingue, tooltip e MIDI

Lingua e attivazione dei tooltip sono preferenze locali salvate sul computer. LEARN legge le note della tastiera MIDI soltanto dopo che scegli un ingresso MIDI nella sua finestra; quando la chiudi, il callback viene rimosso e viene ripristinata l’attivazione precedente del dispositivo. Non registra audio e non usa il microfono. I preset personali restano file locali.

## Aggiornamenti GitHub

Come nelle altre app della famiglia, verifica e download automatici sono attivi inizialmente e si disattivano in OPZIONI → IMPOSTAZIONI. VERIFICA ORA avvia una verifica manuale. L’updater richiede i metadati pubblici delle release, il changelog italiano da raw.githubusercontent.com quando opportuno e il pacchetto corretto tramite i download/CDN GitHub. GitHub riceve i normali dati di connessione (IP e user-agent); non vengono caricati progetti, MIDI, campioni, stato dei plugin o percorsi locali.

I pacchetti vengono scaricati e preparati in `Downloads/Fase Groove Arranger updates`, oppure in Documents se Downloads non esiste. Lo SHA-256 viene verificato con i metadati della release. Serve INSTALLA ORA prima di eseguire setup o sostituzione portatile. Il setup può richiedere i permessi di amministratore Windows; la copia portatile richiede accesso in scrittura alla cartella dell’app. Se la sostituzione fallisce vengono ripristinati i file precedenti; nelle cartelle protette resta l’installazione manuale. Un helper temporaneo locale può riaprire l’app dopo la chiusura. Progetti, campioni, esportazioni, autosave e preferenze non vengono sostituiti dall’updater o cancellati dalla disinstallazione. I download degli aggiornamenti si eliminano dal file manager quando non servono più.

Lingua, dimensione, tooltip, modalità del circolo, preferenza aggiornamenti e versione ignorata rimangono nelle impostazioni locali. La rete su Linux usa curl di sistema; se manca, il lavoro musicale offline resta disponibile.

La ricerca dei plugin legge le cartelle standard e quelle personali selezionate, incluse le sottocartelle. Percorsi, descrizioni degli strumenti, impronte dei metadati dei file e stato dei controlli falliti sono conservati localmente nella cartella dati dell’utente, sotto `Fase Groove Arranger/Plugins`. L’app non carica questo catalogo su servizi esterni. Per verificare un plugin nuovo/aggiornato viene avviata una copia separata dell’app; codice, licenze e comportamento di rete del plugin restano di competenza del produttore.

## Composizione IA facoltativa

CREA CON IA supporta OpenAI, Anthropic, Google Gemini e un server Ollama di cui scegli l’indirizzo. Ogni pressione di COMPONI invia richiesta scritta, nomi/ruoli delle tracce, nomi e durate delle sezioni, tonalità, scala, accordi e un estratto limitato del MIDI usato come contesto musicale. Il servizio online riceve normali dati di connessione e la propria chiave API per l’autenticazione; applica le sue condizioni su privacy, conservazione, modelli e fatturazione. Ollama sul tuo computer elabora localmente; un indirizzo remoto riceve il contesto su quel server.

Il contesto musicale non contiene audio, campioni, percorsi locali, catalogo plugin, stati serializzati di strumenti/effetti o chiavi API. Il provider scelto riceve la chiave soltanto in un header HTTPS, con reindirizzamenti disattivati. Le chiavi restano nella memoria del processo per la sessione: non vengono salvate in progetti, preferenze, file temporanei, log o argomenti di processi esterni. La connessione usa HTTPS nativo su Windows/macOS e libcurl su Linux. Si salvano soltanto preferenze di servizio/modello e indirizzo Ollama. Chiudere la finestra annulla una richiesta in corso; il servizio potrebbe averla già elaborata o fatturata. Non partono richieste automatiche e non si passa automaticamente a un altro provider.

Le risposte sono convalidate come dati musicali limitati, senza eseguire codice. L’anteprima viene inserita soltanto con APPLICA; Annulla ripristina il progetto precedente. Il MIDI e l’arrangiamento diventano normali dati di progetto, salvati e autosalvati localmente come la musica scritta a mano.

## Registrazione e raccolta in Studio

Studio apre soltanto l’ingresso MIDI scelto per ascoltare e registrare note. Note, velocity e durate ricavate dal sustain restano nel progetto locale; ingresso e preferenze di registrazione sono salvati localmente. Studio non registra il microfono. Raccolta e preset portatili copiano i campioni scelti nella destinazione selezionata e ne verificano il contenuto sul dispositivo. Sincronizzazione e intonazione vengono elaborate localmente, senza caricare audio in rete.
