# Fase Groove Arranger — Guida italiana

## 1. L'ecosistema

Fase Groove Arranger riunisce i motori originali di Fase Groove Piano, Drums e Guitar in una workstation MIDI standalone. Non serve installare i tre plugin. L'interfaccia riprende colori, carattere Inter, logo artista e linguaggio visivo delle app della famiglia.

Creato da Fabio 'Pitch' Serra / FS Pitch, DJ & Producer. I crediti dei motori originali restano Claude (Anthropic), AI coding assistant. L'integrazione dell'arranger è sviluppata con Codex (OpenAI), AI coding assistant. ECOSYSTEM apre crediti, licenze e privacy, con gli stessi collegamenti artista e donazioni delle altre app.

## Avvio dei pacchetti desktop

Scarica l'artifact del tuo sistema da GitHub Actions. Estrai lo ZIP dell'artifact e poi l'archivio di distribuzione contenuto: ZIP su Windows, tar.gz su macOS/Linux. Avvia Fase Groove Arranger.exe su Windows, il bundle .app su macOS oppure l'eseguibile su Linux. Gli archivi Unix conservano i permessi di esecuzione. Non serve una DAW esterna e non è richiesto il microfono. I pacchetti di sviluppo non hanno installer o firma di distribuzione.

## 2. Creare una canzone

Scegli un genere: Piano House, Soulful, Deep, Gospel, Jackin', Jackin' Piano, Garage & UK, Afro & Latin, Funky & Disco, '90s Piano House, Neo Soul & Broken, Chill & Pads oppure Dance Stabs. La scelta prepara tempo, swing, progressione e parti di basso/archi adatte al genere. Imposta tonalità, scala, estensione e voicing. GENERATE SONG scrive il MIDI di tutte le tracce sbloccate.

Imposta la durata desiderata (4–12 minuti nell'interfaccia, 5 di default) e premi NEW ARRANGEMENT. Questo ricostruisce forma e MIDI: usa UNDO per recuperare il lavoro precedente. Le tracce e i clip bloccati devono essere sbloccati prima di cambiare struttura. GENERATE SONG mantiene invece le sezioni e i blocchi esistenti.

Ogni genere ha una forma propria: Deep e Chill evolvono senza build da festival; Jackin' alterna groove, break brevi e ripartenze; Garage ha due drop e un taglio; Soulful/Gospel hanno risposte e riprese; Afro privilegia le percussioni e Disco le strofe/ritornelli. La durata si arrotonda alla frase musicale successiva. Intro e outro durano 16 o 32 battute secondo lo stile; le prime e le ultime 16 battute sono prive di piano, basso, archi e chitarra per consentire il mixaggio. Gli altri strati entrano/escono per frasi. Tempo e armonia sono controlli della DAW, condivisi da tutti gli strumenti.

La progressione è condivisa da piano, basso, archi e chitarra. EDIT CHORDS permette di impostare ogni slot come grado della scala oppure accordo maggiore, minore o dominante su una radice cromatica, e di scegliere da 1 a 8 accordi. PROGRESSION sceglie dalla libreria armonica originale. NEW CHORDS propone una nuova sequenza di gradi nella scala attuale. Le impostazioni nuove si applicano alla prossima generazione: non cambiano automaticamente le note che hai già modificato. Per applicare una nuova tonalità o progressione, rigenera insieme le parti interessate. Una traccia bloccata conserva il suo MIDI, quindi verifica l'armonia prima di bloccarla.

## 3. Tracce e sezioni

Le tracce sono ordinate così: Kick, Snare, Clap, Rim, Closed Hat, Open Hat, Ride, Shaker, Tambourine, Conga, Cowbell, Tom, Crash, Bass, Piano, Strings, Guitar. Ogni elemento ha MIDI, strumento, volume, pan, mute e solo propri. La chitarra resta una sola traccia con tutte le sue lane.

Il clic sul **nome della traccia** apre il pannello dello strumento. Il clic su un **clip** apre l'editor MIDI grande, che occupa l'area di lavoro: il piano roll non resta sotto l'arrangiamento. < ARRANGEMENT o Esc torna alla canzone. EDIT MIDI e INSTRUMENT PANEL consentono di passare tra i due editor. Il selettore di sezione cambia la parte da modificare. Il clic sull'intestazione di una sezione sposta il cursore al suo inizio.

Il pannello riprende marchio, superfici, colori e manopole della famiglia Fase Groove. Contiene groove, dinamica, swing, humanize, densità, variazioni/fill e parametri dello strumento. Il piano offre registro, gate, strum, direzione, movimento, range, top line, approcci e realism; basso e archi hanno i loro stili; la batteria espone soltanto il pezzo selezionato, nota MIDI e pocket. La chitarra offre let ring, room for band, push changes e il mixer delle 12 lane con livelli, ottave, mute, swing e anticipo/ritardo. I parametri sono salvati nel progetto; APPLY TRACK / APPLY CLIP riscrivono il MIDI rispettando i blocchi. TONE, REVERB, CHORUS, PUMP e PUMP RELEASE cambiano il suono guida interno; i suoni/VST personali conservano i propri controlli. Non ci sono LINK, tonalità o BPM duplicati nel pannello.

**GUITAR LEAD** nella schermata della chitarra attiva le risposte e gli spazi del piano di accompagnamento usando la logica del plugin originale. Aggiorna piano e chitarra rispettando i blocchi ed è annullabile. Disattivalo per tornare ai ruoli normali.

Il tasto destro sull'intestazione della sezione consente rinomina, duplica con MIDI, elimina, sposta, durata e funzione musicale. Cambiare la funzione prepara il prossimo rendering; cambiare la durata conserva le note che rientrano nella nuova lunghezza e tronca quelle oltre il limite. Se allunghi la sezione, rigenera per riempire il tratto nuovo. La duplicazione copia tutte le note e tutti i blocchi del segmento.

+ SECTION aggiunge una sezione Main dopo quella selezionata e ne genera il contenuto. Puoi avere da 1 a 64 sezioni, ciascuna da 1 a 128 battute, in 4/4. Il tasto destro sulla traccia consente rinomina e scelta della nota radice di un campione.

## 4. Rigenerare soltanto quello che serve

- GENERATE SONG: tutte le sezioni di tutte le tracce sbloccate.
- NEW TRACK: la sola traccia selezionata, lungo tutta la canzone.
- NEW SECTION: la sola sezione selezionata, su tutte le tracce sbloccate.
- NEW CLIP: soltanto la traccia selezionata nella sezione selezionata, per esempio il basso nel Drop.

LOCK TRACK protegge l'intera traccia. LOCK CLIP protegge soltanto la cella selezionata. I blocchi proteggono dalla generazione; le modifiche manuali restano disponibili. Le operazioni sono annullabili con UNDO/REDO. Ogni clip salva il proprio seed, così il progetto mantiene la variante generata senza generazioni imprevedibili alla riapertura.

DENSITY regola la quantità di note, SWING lo spostamento ritmico e FEEL la humanisation. Sono impostazioni per la generazione successiva. Il BPM cambia subito la velocità di riproduzione, senza modificare la posizione musicale delle note.

## 5. Editor MIDI grande

Piano, basso e archi mostrano soltanto le note della propria traccia. Gli elementi di batteria hanno una singola riga: l'editor del kick contiene soltanto il kick; le note importate/incollate vengono adattate alla sua nota MIDI. La chitarra apre tutte le 12 lane del VST: aggiungere un hit costruisce le note della relativa articolazione sull'accordo della battuta. Clic e trascinamento selezionano/spostano insieme le note dello stesso gesto; il tasto destro lo elimina. NOTES / LANES passa al piano roll per modificare anche ogni singola nota della chitarra. ZOOM cambia la scala orizzontale dell'editor.


- Clic in uno spazio vuoto: aggiungi una nota sulla griglia.
- Trascina la nota: cambia posizione e altezza.
- Trascina il bordo destro: modifica la durata.
- Alt + trascina verticalmente: cambia velocity.
- Shift + clic: aggiungi/rimuovi dalla selezione multipla.
- Tasto destro su una nota, DELETE o Backspace: elimina.
- Ctrl/Cmd + A con il piano roll attivo: seleziona tutte le note del clip.
- QUANTISE: allinea l'inizio delle note alla griglia 1/4, 1/8, 1/16 o 1/32.
- -12 / +12: trasponi di un'ottava.
- VEL: cambia la velocity delle note selezionate; senza selezione imposta quella delle nuove note.
- COPY / PASTE: copia le note selezionate, o l'intero clip se nessuna nota è selezionata, e incolla nella sezione attuale mantenendo i tempi relativi.

Quantise e trasposizione agiscono sulla selezione, oppure sull'intero clip quando non ci sono note selezionate. Le note oltre la durata della sezione vengono rimosse o accorciate. Le note sovrapposte sullo stesso pitch vengono unite/troncate per mantenere note-on e note-off coerenti su un singolo canale MIDI.

IMPORT MIDI e il trascinamento di un file .mid importano tutte le note PPQ nel clip selezionato, fino alla durata della sezione. Il MIDI importato sostituisce il clip; UNDO ripristina il contenuto precedente. Il tempo del progetto resta quello impostato nell'arranger. I file MIDI SMPTE non sono supportati.

## 6. Suoni, campioni e VST

Il suono iniziale è quello di guida delle app originali. Piano e basso possono usare Rhodes, Soulful Grand, House Piano, House Organ, Wurlitzer, Gospel Organ e Warm Pad. Gli archi usano la voce strings originale. La chitarra offre Clean Funk, Warm Jazz, Disco Chorus e Nylon. La batteria offre House 909, Deep 808, Disco Live e Lo-Fi Soul.

LOAD SAMPLE assegna un WAV/AIFF/FLAC/OGG personale, fino a 60 secondi. Per gli strumenti intonati la radice iniziale è C4/MIDI 60; per gli elementi di batteria è la loro nota General MIDI. Il tasto destro sulla traccia permette di cambiarla. Il sampler usa il campione per tutte le altezze, con trasposizione; non è un campionatore multisample.

LOAD VST3 / AU apre un plugin strumento nativo della tua macchina. Windows/Linux usano VST3; macOS supporta anche Audio Units. Dal menu scegli il bundle .vst3, non una cartella generica. Su macOS la voce Audio Units installate elenca gli strumenti registrati nel sistema. OPEN VST mostra l'interfaccia del plugin, oppure i suoi parametri generici. C'è uno strumento per traccia. Non sono presenti slot effetto o bridge per plugin a 32 bit.

Il motore generativo resta sempre quello interno Fase Groove: lo strumento assegnato riceve le note e produce il suono. I plugin devono essere installati, autorizzati e compatibili con la stessa architettura dell'app. I plugin vengono caricati nel processo dell'app: salva prima di provare un plugin nuovo. Non sono ridistribuiti plugin o librerie commerciali.

Lo stato del plugin viene salvato nel progetto. Campioni e plugin sono referenziati tramite percorso e non vengono incorporati nel file .fga. Se mancano, ricollegali con LOAD SAMPLE / LOAD VST3. L'esportazione richiede che gli strumenti mancanti siano risolti; puoi anche scegliere esplicitamente i suoni originali dal menu traccia.

## 7. Mixer e trasporto

PLAY o Spazio avvia/mette in pausa; STOP ferma e torna all'inizio. |< riporta il cursore all'inizio. Le note tenute vengono ricostruite quando riparti in mezzo a un accordo. LOOP attiva la regione corrente; LOOP SECTION imposta i limiti alla sezione selezionata. La regione iniziale è tutta la canzone.

GAIN e PAN regolano la traccia. MUTE esclude la traccia dal mix. Se una traccia è SOLO, suonano soltanto le tracce in solo. Il livello master regola tutte le uscite. I meter accanto alle tracce indicano il livello. Mantieni margine nel mix: la protezione finale limita i picchi a circa 0 dBFS e può distorcere se sovraccaricata.

AUDIO apre la scelta del dispositivo, della frequenza e del buffer. L'app usa soltanto l'uscita: non richiede microfono né registra audio. La latenza dei plugin non viene compensata in questa versione.

## 8. Progetti e autosave

SAVE salva il progetto .fga; SAVE AS crea una nuova copia. Il file comprende impostazioni musicali, struttura, note, blocchi, mixer e stati degli strumenti. Il salvataggio usa un file temporaneo e sostituzione atomica. Il progetto precedente resta conservato se la scrittura fallisce.

Ogni minuto, se ci sono modifiche, viene scritto un autosave locale nella cartella dati dell'app: FS Pitch/Fase Groove Arranger/autosave.fga. AUDIO → Apri cartella autosave mostra la cartella. AUDIO → Recupera autosave apre il progetto di recupero. L'autosave è una sola copia di recupero: salva separatamente le versioni che vuoi conservare.

UNDO/REDO conserva fino a 40 passaggi in memoria; la cronologia non viene salvata nel progetto. La chiusura con modifiche non salvate richiede di scegliere se scartarle. Salva dal pulsante SAVE prima di confermare la chiusura.

## 9. Esportazione

EXPORT offre MIDI multitraccia, singola traccia MIDI e tutte le tracce MIDI separate. I file contengono tempo, firma 4/4, metadati tonalità/scala, nomi delle tracce e marker di sezione, a 960 PPQ. La batteria usa il canale General MIDI 10; gli altri strumenti hanno canali separati. I suoni dei VST non sono contenuti nel MIDI.

WAV è stereo 24 bit alla frequenza del dispositivo. MP3 è stereo 320 kbps a 44.1 kHz, con encoder Shine integrato: non serve scaricare software. Entrambi possono esportare il mix, la traccia selezionata o tutti i 17 stem. Gli stem mantengono lo stesso inizio, la durata della canzone e tre secondi di coda, per essere allineati in una DAW.

Il mix rispetta mute e solo. L'export di una traccia singola/stem la include anche se è muta o altre tracce sono in solo; mantiene il suo gain, pan e livello master. Gli export multipli creano una nuova sottocartella datata. Le tracce vuote producono file silenziosi della stessa durata. La riproduzione viene fermata durante il rendering; una finestra mostra avanzamento e annullamento.

I plugin possono produrre take leggermente differenti fra render se usano campioni round-robin o casualità interna. MP3 è lossy e aggiunge ritardo/padding del codec: per riallineare stem e per mix professionali usa WAV. Non è implementata compensazione della latenza dei plugin.

## 10. Privacy, permessi e limiti attuali

Il programma funziona offline: nessun account, pubblicità, analytics o telemetria, nessun upload di musica. Legge i file scelti e scrive progetto, autosave ed export locali. I pulsanti esterni di crediti aprono il browser soltanto su tua azione. I plugin hanno condizioni e permessi propri. Leggi PRIVACY.it.md e THIRD_PARTY_NOTICES.md, disponibili anche da ECOSYSTEM.

Questa versione è una workstation per arrangiamento MIDI. Non include ancora registrazione audio/MIDI, editing audio, automazione, warp audio, catene effetti, bus/send, registrazione controller, time signature differenti da 4/4 o scansione plugin isolata. Non dichiara compatibilità con ogni VST: va verificata con gli strumenti effettivi. I pacchetti di sviluppo non sono firmati/notarizzati e non hanno installer.
