# Novità — Fase Groove Arranger

## 0.23.2

- Rinnova i pulsanti con icone vettoriali e aggiunge potenziometri volume/pan nell’inspector e master/densità/swing/feel nella home, conservando valori modificabili e Annulla.
- Apre l’inspector con doppio clic sul nome della traccia, con chiusura compatta e impaginazione più raccolta. Evidenzia Studio, chitarra solista e comandi mixer con icone coerenti.
- Prolunga il colore delle intestazioni lungo la sola linea iniziale delle sezioni. Aggiunge zoom e Adatta brano allineati a destra accanto ad Armonia.
- Sostituisce la scritta del mixer chiuso con un meter stereo del master, dB e memoria dei picchi; riaprendo il mixer tornano le strip.
- Un clic sulla posizione alterna battute e tempo trascorso con residuo. Il righello dell’arrangiamento segue la scelta; metrica, snap, MIDI e formato progetto restano invariati.
- Aggiorna guide IT/EN, schermate native e suggerimenti nelle sei lingue. Aggiunge verifiche native per comandi, Annulla, audio reale del master, zoom e conversione del tempo.

## 0.23.1

- Aggiorna le guide italiana e inglese all’Arranger attuale: campionatore, ascolto e accordatore, registrazione MIDI/audio, importazione take, render traccia, mandate del mixer e automazioni volume/pan/master/plugin.
- Sostituisce le schermate obsolete di arrangiamento, mixer, Piano, sampler e impostazioni con catture native. Corregge FAQ, ingresso audio, formati campione, migrazione e numero tracce; aggiunge accessi rapidi.
- Conserva impaginazione della famiglia, ricerca integrata e immagini ingrandibili. Elaborazione audio, generazione MIDI e formato progetto restano invariati.

## 0.23.0

- Rimuove il mutex globale dell’engine dal callback audio: le modifiche di controllo attendono una finestra quiescente. Prepara i plugin sostitutivi fuori da questa finestra e serializza/ripristina lo stato senza elaborazione concorrente.
- Registra MIDI su un tempo continuo attraverso i loop, compensa latenza di dispositivo/plugin/allineamento e conserva gli offset fra frammenti del callback. Segnala overflow senza panic globale e limita memoria/lavoro per eventi guitar/lead e sequenze MIDI troppo dense.
- Pubblica posizione, frequenza e loop in atomici; playhead, click e conteggio usano il metro dell’engine. Il formato attuale dei brani resta in 4/4.
- Applica subito il delay dei bus allo snap e avvolge gli indici di lettura. Conserva la dinamica float live, elimina valori non finiti su tutti i percorsi e limita soltanto gli export interi.
- Aggiunge registrazione WAV stereo da ingresso/mix in Studio, fine take/importazione/Annulla, trim non distruttivo della latenza, render traccia e feedback in sei lingue. Ingresso audio esplicito e ripristinato dopo la take. Import sampler fino a 30 minuti/512 MiB decodificati per file.
- Suddivide l’elaborazione dei plugin sui breakpoint di automazione con intervalli limitati sulle rampe, preservando tempi MIDI e playhead. Estende e mostra il limite PDC a 262143 frame, indicando i superamenti.
- Migra progetti precedenti con poche tracce o slot sparsi, aggiungendo slot muti e conservando note e assegnazioni dei plugin. Rifiuta ID duplicati ambigui e versioni future sconosciute. Aggiorna guide, privacy e regressioni native.

## 0.22.7

- Rifai Studio seguendo il disegno fornito: waveform unica a tutta larghezza, barra sorgente/preset e riga tagli/toggle compatte, cinque moduli su due righe, ordine manopola/valore/etichetta e tastiera più bassa.
- Rimuovi seconda waveform, slider separati di zoom/posizione e grafico aggiuntivo dell’inviluppo. Conserva rotella/pinch/fit, trim, caricamento drag-and-drop, tutti i parametri, ascolto/accordatore, slice suonabili e Annulla.
- Condividi palette piatta del PDF, Inter, manopole e chip fra le tab Studio e Bus e mandate. Mixer a quattro strip con fader in dB e meter verticali allineati, Pan, M/S e manopole dei ritorni.
- Aggiorna guide e prove native per navigazione sull’onda unica, controlli reali, geometria compatta, conversione dB, MIDI invariato e Annulla.

## 0.22.6

- Sposta gruppi, bus e mandate da Studio a un pannello dedicato del mixer.
- Aggiunge uscita M/GA/GB e pulsanti A/riverbero e B/delay sui canali. Nel pannello si regolano percentuali, livello/pan/mute/solo dei bus, nomi e parametri effetti.
- Conserva formato progetti e percorso audio. Test nativi per mandate reali, Annulla, MIDI invariato, canali compatti e rotella sicura; guide IT/EN e tooltip nelle sei lingue aggiornati.

## 0.22.5

- Accordatore di Studio: nota rilevata dall’audio del campione selezionato, scarto in cent e Hz durante ascolto, tastiera MIDI e brano.
- La lettura segue Trasposizione e Fine; resta separata dal tasto MIDI di comando. Silenzio e rumore non ricevono una nota inventata.
- Analisi fuori dal callback audio, tooltip nelle sei lingue e guide IT/EN per accordatura e limiti delle sorgenti.

## 0.22.4

- Ascolto isolato del campione in Studio con Play, Stop e ripetizione opzionale; le modifiche al campionatore si sentono subito, senza cambiare posizione del brano o MIDI.
- Indicazione della nota ascoltata, arresto al cambio campione/chiusura e tooltip tradotti.
- Separatore ASCII nel titolo di Studio per evitare il carattere corrotto.

## 0.22.3

- I meter di Studio senza segnale mostrano Silenzio, al posto dei simboli meno/infinito; i livelli attivi restano numerici in dB.
- Rotella e trackpad non modificano più involontariamente parametri, livelli dei bus, routing o selezioni di Studio. Restano zoom/scorrimento della forma d’onda e modifica esplicita con trascinamento o tastiera.
- Verifica nativa dei gesti anche sulle schede inattive: progetto e Annulla restano invariati. Guida al recupero di un kick attenuato senza sostituire MIDI o campione.

## 0.22.2

- Campionatore apribile dall’inspector, con menu limitato ai campioni; registrazione e bus conservano tutte le tracce.
- Studio riordinato in gruppi di controlli, tastiera sempre visibile, nomi delle note, evidenziazione di tasti/pad e meter dei bus.
- Zoom con rotella e pizzicamento sul puntatore, scorrimento orizzontale e doppio clic per inquadrare il campione.
- Taglio automatico sui transienti o in parti uguali, fette suonabili e ripristino default annullabile senza perdere file, MIDI o mix.
- Tooltip descrittivi tradotti nella finestra di Studio, secondo la preferenza globale; guide IT/EN e verifiche aggiornate.

## 0.22.1

- Ollama locale come scelta iniziale del compositore IA, senza chiave API.
- Rilevamento dei modelli installati, verifica annullabile della connessione, indirizzi normalizzati e istruzioni direttamente nell’anteprima.
- Errori del server e della memoria visibili; modelli cloud esclusi dall’elenco locale e contesto ridotto per contenere la memoria richiesta.
- Guide IT/EN e privacy aggiornate; test HTTP per rilevamento, richieste senza chiave ed errori.

## 0.22.0

- Studio: campionatore con forma d’onda, tagli, loop, inviluppo, filtro, preset portatili e sincronizzazione indipendente di tempo/altezza.
- Registrazione MIDI con ingresso e tastiera virtuale, conteggio, sovraincisione/sostituzione, quantizzazione e Annulla.
- Due gruppi e due ritorni condivisi, riverbero e delay sincronizzato; stesso percorso in ascolto ed esportazione.
- Raccolta verificata dei campioni e ricollegamento; progetti precedenti compatibili.


## 0.21.2 — 2026-10-26

- SHA e inventario completo del modulo comune verificati offline e contro la fonte canonica in CI; Lead incluso nel sync/check predefinito.
- Cataloghi verificati contro chiavi duplicate, anche su più righe; gli override intenzionali prodotto/comune restano supportati.
- Rimosse due righe di traduzione sovrascritte, preservando gli override comuni per il lookup senza distinzione di maiuscole; ripristinati i messaggi di caricamento completi in spagnolo, francese, tedesco e portoghese.

## 0.21.1 — 2026-10-26

- Corretto il cursore dell’estensione delle note del Piano in Armonia e scale: entrambi gli estremi appaiono nella posizione reale e seguono il trascinamento, con l’intervallo selezionato evidenziato. Il tema precedente disegnava un cursore singolo fermo, anche se i limiti MIDI cambiavano.
- Verifica nativa dei cursori visibili, dell’area cliccabile, del trascinamento di entrambi gli estremi e del MIDI risultante. Guide IT/EN aggiornate.

## 0.21.0 — 2026-10-25

- Pulsante ambra CREA CON IA con scintille nel tema della famiglia e compositore in finestra separata.
- OpenAI, Anthropic/Claude, Google Gemini e Ollama: scelta del modello, chiavi API solo in memoria e richieste in background annullabili.
- L’IA scrive vere note MIDI, motivi, transizioni e sezioni. Prima dell’anteprima si verificano dati, Lead monofonico, tracce e durata alla battuta più vicina.
- Nuovo brano oppure modifica delle tracce scelte, conservando strumenti e mix. La modifica mirata preserva altre tracce e clip bloccati, anche sulle timeline indipendenti. APPLICA permette ascolto, Annulla e protezione dalle modifiche intervenute durante l’attesa.
- Tempo IA da 30 a 300 BPM conservato nei progetti; la scelta di un genere classico ripristina i suoi limiti originali.
- Guide e privacy IT/EN aggiornate con configurazione, costi API, Ollama, contesto trasmesso e gestione delle chiavi. Test HTTP locali senza credenziali verificano richieste, errori e annullamento.

## 0.20.0 — 2026-10-25

- Clic sui nomi delle sezioni per ascoltare dal punto scelto. Clic su clip e griglia seleziona e modifica senza spostare la riproduzione; il righello numerato mantiene cursore e loop.
- Annulla conserva il clip selezionato quando il suo intervallo è invariato, indipendentemente dal cursore di ascolto.
- Durata MIDI visibile e modificabile dal bordo destro anche nei Drums. I colpi scritti a mano usano le durate del generatore, compresi due beat per il Crash.
- I campioni Drums completano la coda naturale, sia durante l’ascolto sia nell’export audio. Stop continua a silenziarli subito.
- Pannelli strumento ed editor VST restano in primo piano fino alla chiusura manuale; alla riapertura tornano al centro dello schermo.
- Estensioni del piano applica subito il cambiamento dal selettore principale, come nel pannello originale. Entrambi i controlli sono verificati sul MIDI prodotto.
- Guide della famiglia IT/EN aggiornate.

## 0.19.0 — 2026-10-25

- Elimina le tracce MIDI scritte a mano dall’inspector o dal menu traccia; Annulla ripristina note, routing, strumenti ed effetti. Le tracce generate restano protette.
- Inspector richiudibile con una piccola icona: nessuna fascia vuota; riapertura dalla barra o con I.
- + SECTION propone Main, Intro, Breakdown, Build-up, Drop e Outro.
- Mixer con nome della traccia sopra il fader e sorgente sonora sotto. Menu Piano/Bass/Strings contenuti nei riquadri colorati.
- Rigenerazione con zoom automatico sul brano oppure su durata e registro del clip.

## 0.18.0 — 2026-10-25

- Pannelli strumento in finestre separate e ridimensionabili, con ascolto e modifiche condivisi.
- + MIDI aggiunge tracce scritte a mano, fino a 32 totali; supporta piano roll, sample/plugin, effetti, automazioni, salvataggio e stem. La generazione preserva le note manuali.
- Rinomina dal nome nell’inspector/mixer o dal menu traccia; APRI VST apre l’editor dello strumento o effetto assegnato.
- Ottava generale Lead relativa alle singole corsie, condivisa con il motore Lead VST3/Standalone.
- Rigenerazioni del piano con attacchi ritmici più distinti, senza contare dinamica e umanizzazione come nuovi pattern. Take salvati e parti bloccate restano riproducibili.
- Tonalità complete nella barra, comprese sigle come Db.

## 0.17.0 — 2026-10-25

- Motore originale Fase Groove Lead integrato come diciottesima traccia: generazione monofonica, armonia e durata accordi condivise, dodici corsie, quattro synth guida, sample/plugin, automazioni e stem MIDI/audio.
- I progetti precedenti a diciassette tracce si aprono invariati, con Lead vuoto aggiunto in fondo. ID e MIDI delle tracce esistenti restano stabili.
- Inspector ridisegnato in riquadri compatti per traccia, suono, mix e clip, con selettore sorgente su una riga, etichette chiare ed editor plugin contestuale. Comandi, undo, chiusura e ridimensionamento restano funzionanti.

## 0.16.2 — 2026-10-25

- La finestra salva / scarta alla chiusura, i messaggi e la finestra di rinomina seguono il tema dell'Arranger invece di quello standard.

## 0.16.1 — 2026-10-25

- Loop nel righello: trascina i bordi per ridimensionarlo (agganciato alla griglia, almeno un beat), trascina il centro per spostarlo, trascina sul righello vuoto per disegnarne uno nuovo; un annulla per gesto, un clic sposta ancora il cursore.

## 0.16.0 — 2026-10-25

- Nuovo aspetto da DAW professionale: trasporto centrato con LCD (posizione, tempo, metro, tonalità), barra a icone con suggerimenti, righello con marker di sezione colorati, clip con anteprima delle note, intestazioni traccia con icona dello strumento e meter stereo, mixer con scala dB, meter stereo con picco e pan a manopola, inspector a sezioni, tastiera del piano roll ridisegnata. Da fermo non ridisegna nulla.
- Aggiornamento e traduzioni dal modulo comune FaseGroove (stessi controlli e comportamento); due traduzioni corrette (pt "OPÇÕES", it "Blocca A").

## 0.15.3 — 2026-10-24

- L'accompagnamento di piano ora si compila dal motore di Piano stesso (un solo file condiviso); MIDI generato invariato.

## 0.15.2 — 2026-10-24

- Piano più vario: ogni brano nuovo (GENERA BRANO, cambio di genere) prende uno dei voicing e dei colori del genere, e i riff casuali rispondono alla prima battuta con un colpo anticipato o un pick-up (motore Piano 0.31.3).
- Motori inclusi allineati ai plugin con scripts/sync-engines.py; le modifiche proprie dell'Arranger sono in ThirdParty/ENGINE_PATCHES.md.

## 0.15.1 — 2026-10-24

- Ogni brano ha già le automazioni: anche i progetti aperti le scrivono con GENERA BRANO e quando rigeneri traccia, sezione o clip (si spengono dal menu della corsia).
- Le corsie automatizzate sono in blu: titolo della corsia, menu delle corsie, volume e pan nell'inspector.

## 0.15.0 — 2026-10-24

- Automazioni oltre il volume: pan della traccia, volume master e parametri dei plugin, disegnate e modificate come le corsie del volume, annullabili, salvate e incluse negli export.
- Automazioni casuali (dado): sweep nei build-up, salite, movimenti di volume, pan e parametri dei plugin che seguono le sezioni; si rigenerano quando rigeneri brano, traccia, sezione o clip, rispettando i blocchi. Attive di default solo nei progetti nuovi.
- CARICA CAMPIONE nell'inspector apre e chiude il browser campioni; il browser si può ridimensionare e ricorda la dimensione.
- Motori Drums e Guitar aggiornati (snare nell'Afro, groove casuali di chitarra sempre udibili).
- Tema, circolo delle quinte e logo FaseGroove ora vengono dal modulo comune.

## 0.14.4 — 2026-10-24

- Più leggero: mixer, pannello strumento, timeline e piano roll ridisegnano solo ciò che cambia; le manopole del pannello strumento rigenerano la parte quando il gesto si ferma invece che a ogni scatto; le traduzioni si applicano solo al cambio lingua; meno copie e ricalcoli del progetto a ogni modifica; le impostazioni dei plugin restano ferme quando sono nascoste.
- Motori Drums e Guitar aggiornati a 0.20.1 e 0.10.1. La musica generata non cambia.
- Rimossi residui inutilizzati (traduzioni doppie, controlli mai creati, il vecchio campo dei minuti del brano; i progetti vecchi si aprono ancora).

## 0.14.3 — 2026-10-24

- Loop: il pulsante loop, L, il loop di sezione/clip e Ctrl+L spengono di nuovo il loop; un doppio clic sul righello o il menu contestuale tolgono la zona. Annullabile e salvato nel progetto.
- CARICA CAMPIONE nell'inspector apre il browser campioni interno per quella traccia.
- Le tracce mostrano lo strumento usato: il plugin, il campione o il suono interno.
- L'inspector è rifatto come un'unica colonna stabile.
- GENERA BRANO segue ogni volta la forma del genere attuale; le modifiche sulla timeline non la bloccano più; tracce e clip bloccate restano.
- Le modifiche nel pannello strumento rigenerano la parte; quelle dell'armonia rigenerano anche basso, archi e chitarra (blocchi rispettati).
- Editor MIDI: pulsante RIGENERA (annullabile). Matita, selezione e gomma sono icone.

## 0.14.2 — 2026-10-23

- Il browser campioni considera nascosti i file e le cartelle che iniziano con un punto su ogni piattaforma, Windows compreso.

## 0.14.1 — 2026-10-23

- Corretto il build Windows del browser campioni.

## 0.14.0 — 2026-10-23

- Browser campioni (BROWSER, tasto B): le tue cartelle di campioni e i preferiti, salvati; si riapre nell'ultima cartella. Percorso, avanti/indietro/su, ricerca, filtro per tipo, ordinamento per nome, data, dimensione o durata, informazioni sul file, ascolto con autopreview opzionale, caricamento con doppio clic, Invio, LOAD o trascinando su una traccia. Le cartelle si scansionano in background.
- Interfaccia: una sola toolbar (strumenti di modifica a sinistra; GUITAR LEADS, INSPECTOR, BROWSER e MIXER a destra); il transport mostra la posizione, LOOP e CLICK (metronomo). La timeline ha tracce a righe alternate, linee di battuta e di sezione, interruttori A/M/S più chiari e un indicatore del cursore; da ferma non si ridisegna più. Tolti i pulsanti di modifica doppi (gli stessi comandi sono nel menu della clip e da tastiera).
- Motori aggiornati a Piano 0.31, Drums 0.20 e Guitar 0.10; piano e basso, su tracce separate, non si tagliano più le note.
- Riproduzione: compensazione della latenza dei plugin; caricare un plugin o un campione tocca solo la sua traccia; gli export si generano su una copia privata, quindi la riproduzione continua; le modifiche durante la riproduzione fanno ripartire solo le note cambiate; niente clic ai giri del loop e ai salti; le note dei plugin si chiudono a stop, salto e loop; i plugin mono suonano su entrambi i lati.
- Export MP3 senza silenzio iniziale; un .fga si apre da riga di comando o da una seconda apertura; alla chiusura chiede di salvare; il recupero dell'autosalvataggio non fa più sovrascrivere il file con SALVA; annulla ripristina gli stati giusti dei plugin. Metronomo con C, loop con L.

## 0.13.0 — 2026-10-22

- I groove di batteria seguono il genere del brano. Charleston aperti, cassa garage, rim deep e hat jackin' escono da quello stile. Lo shuffle Jackin' è al 62%. Il jack resta: cassa in 4/4, clap o rullante su 2 e 4, charleston aperto in levare, accento di charleston chiuso in shuffle e shaker.
- I groove di chitarra pescano le due battute dal ritmo del genere: stab in levare, stab dance corte, ottavi gospel, spazio rilassato, sedicesimi funky o jackin'. In ogni battuta resta un accento che si sente.
- I riff di piano seguono di nuovo il genere del brano. Non c'è un nuovo menu genere. House, piano house anni '90 e dance stab tengono le figure classiche. Gli altri stili hanno riff propri, e un seed nuovo scrive ancora una figura diversa. Il dado degli accordi gospel cammina di grado. NUOVO RIFF tiene voicing e colore del genere. Gli stab degli archi si riscrivono col riff nuovo. CHITARRA SOLISTA usa ancora il piano roll di quattro battute. I segmenti bloccati della griglia restano.

## 0.12.1 — 2026-10-22

- La batteria Jackin' House resta un jack su Genera brano, rigenera, casuale, variazioni e sezioni: cassa in 4/4, clap o rullante su 2 e 4, charleston aperto in levare, sedicesimi shuffle circa 56–62% e shaker sincopato. Il tempo resta fra 120 e 127. Il beat non diventa two-step, broken o garage.
- Rigenerare il brano non porta più il Jackin' a 130 BPM né a uno shuffle terzinato. Cowbell e tom restano nel fraseggio.
- Le figure di piano tornano ai riff house classici, come in FaseGroove Piano 0.29.0. Il genere non sceglie più ritmo, variazione, fill, accento o progressione del piano. Rigenerare piano o stabs d'archi scrive ogni volta una figura diversa. Un genere salvato si apre ancora e il piano lo ignora. CHITARRA SOLISTA tiene il piano roll di quattro battute. Generi di chitarra e batteria invariati.

## 0.12.0 — 2026-10-22

- Gli archi suonano una parte sull'armonia in ogni genere che li prevede. Pad e linee di accordi seguono la progressione in strofa, ritornello e break, ed entrano in intro e outro invece di essere tolti dall'arrangiamento.
- Rigenerare la traccia archi, una sezione o un clip scrive sempre delle note. Il Jackin' nasce ancora senza un tappeto di archi; rigenerare la traccia archi aggiunge un pad. Una traccia o un clip bloccati restano com'erano, e la parte si salva con il brano.

## 0.11.0 — 2026-10-22

- Browser dei plugin nello stesso stile del browser dei groove FaseGroove: produttori a sinistra, plugin a destra, ricerca, preferiti e recenti. I motori FaseGroove sono il primo gruppo. Un clic carica il plugin; doppio clic o Invio chiude il browser.
- All’avvio la scansione copre le cartelle VST3 predefinite e una cartella personale, anche nelle sottocartelle. L’elenco noto è salvato come XML KnownPluginList di JUCE. I file invariati non vengono riletti. Le scansioni fallite restano in quarantena. RESCAN e RIPROVA ERRORI restano nelle impostazioni plugin.
- Ogni traccia può caricare uno strumento VST3/AU e un effetto in insert. Il progetto salva entrambi i plugin e il loro stato.
- I groove casuali di chitarra restano udibili. Ogni battuta tiene un accento che si sente, i sedicesimi semplici non scendono sotto la curva degli accenti e l’umanizzazione non azzera un colpo. I ghost restano più bassi. Le intro funky, jackin' e jackin' piano non sono più solo uno scratch fantasma.

## 0.10.0 — 2026-10-22

- I riff casuali, le variazioni, i break e i build-up del piano restano nel genere del brano. Fill Mix, accenti e progressioni casuali seguono gospel, jackin', garage, afro, funky, neo soul e chill. I groove della famiglia house tengono le figure esistenti. Li guida il genere del brano: il pannello non aggiunge un secondo menu genere.
- I pannelli motore non ripetono più il menu sezione né il volume di traccia, e il piano non salva più i preset standalone `.fgapreset`. Tempo, tonalità, trasporto ed export restano nell’arranger. I parametri salvati non cambiano.
- I groove casuali di chitarra, i turnaround, le sezioni e i fill Mix restano nello stile. Solo il pattern casuale usa voicing e lick del genere; i preset restano sulle forme classiche. Uno slide sul battere non viene più tagliato, e l’umanizzazione non riveste più ogni colpo della quarta battuta.
- Fill, turnaround e accenti dei piatti seguono il genere. Garage, neo soul e chill tengono kick e hat nelle sezioni, e il chill non aggiunge più uno shaker in sedicesimi. Un riff seed nuovo aggiorna i turnaround dei preset, e Jackin' Piano non prende più il flam di Jackin'.
- I fill del piano non leggono più una scala vuota, e il comping con la chitarra solista chiude le note ancora aperte a fine clip.

## 0.9.0 — 2026-10-22

- GENERA BRANO estrae anche un tempo nel genere, densità e swing dello stile, e nuove durate di sezione da otto battute. L’ordine delle sezioni resta. I blocchi congelano la forma; le note armoniche bloccate tengono tonalità e progressione.
- NUOVI ACCORDI scrive l’armonia nelle tracce sbloccate. La quantizzazione tiene attacchi e finali dentro il clip. Swing, densità e umanizzazione aggiornano subito l’arrangiamento. Taglia e incolla non staccano più la timeline se la modifica non entra.
- Duplica una sezione della guida con le note. Umanizza le note dal menu del clip o con il tasto H. Rimossi helper dei motori che l’arranger non chiamava.
- GENERA BRANO sceglie tonalità, progressione del genere e figura di basso diverse. Le note armoniche bloccate mantengono l’armonia attuale; la rigenerazione parziale resta nell’armonia del brano.
- La rigenerazione lascia preset e Riff seed fissi, combina frasi pianistiche del genere, varia il ritmo del basso e crea nuovi pattern di chitarra Jackin’. APPLICA conserva parametri espliciti e versioni salvate; restano blocchi, strumenti, mix e note fuori ambito.
- Test dei ritmi di piano/basso/chitarra senza humanizzazione sui tredici generi, inclusi preset e seed fissi; aggiornati messaggi e guide IT/EN.

- Distribuzione come Piano/Guitar/Drums: uno ZIP per piattaforma e setup Windows, solo file di esecuzione, guide con risorse collegate e licenze. Prove CI raccolte in un solo artifact separato.
- Il clic sul nome seleziona la traccia; l’ingranaggio ambra accanto a Solo apre il motore. CHITARRA SOLISTA disponibile nella barra principale, con blocchi e ANNULLA.
- Rimossi i comandi pianistici ignorati dalle modalità indipendenti di basso/archi, i controlli guida inutili con campioni/plugin e i doppioni di routing/batteria. Le modalità e il suono assegnato aggiornano i controlli disponibili.

## 0.8.0 — 2026-10-21

- Catalogo degli strumenti salvato e diviso per produttore, disponibile subito dal selettore dei plugin. Cartelle VST3 standard di sistema/utente e percorsi personali configurabili; ricerca ricorsiva senza duplicati.
- Scansione automatica al primo avvio e a ogni avvio successivo, in background. I VST3 invariati usano la cache; rilevati aggiornamenti e rimozioni, conservate le assegnazioni dei progetti. Audio Units registrate disponibili su macOS.
- Verifica dei binari in un processo separato, con limite di tempo e quarantena persistente dei controlli falliti/interrotti. Gestione cartelle, scansione, annullamento e riprova nelle sei lingue dell’interfaccia.
- Test nativi per sottocartelle e percorsi sovrapposti, catalogo salvato, cache, aggiornamenti, quarantena/riprova, rimozione e nuova scoperta. Conservati stile famiglia, disposizione delle impostazioni e caricamento manuale.

## 0.7.2 — 2026-10-21

- La rigenerazione crea una nuova versione musicale di brano, traccia, sezione o clip, invece di cambiare soprattutto l’umanizzazione dello stesso riff. Traccia/brano mantengono un motivo coerente tra sezioni e la batteria un seed comune.
- Variazioni della linea di basso scelta con risposte su fondamentale/quinta/ottava e anticipi, rispettando accenti e note tenute. Salvataggio, duplicazione e APPLICA conservano la versione musicale.
- Corretto RND LANE che riutilizzava la versione precedente nei pannelli strumenti. APPLICA TRACCIA/CLIP applica i parametri senza estrarre un nuovo riff; preset fissi, griglie manuali e seed espliciti restano vincoli voluti. Conservati strumenti assegnati, mix, automazione, blocchi e clip fuori ambito.
- Test di diversità musicale sui tredici generi, isolamento degli ambiti, riproducibilità e salvataggio del progetto.

## 0.7.1 — 2026-10-21

- Setup Windows della famiglia: wizard italiano/inglese, logo Arranger, menu Start, collegamento desktop facoltativo e disinstallazione, adattato all’app standalone.
- Aggiornamenti come nelle altre app: download del pacchetto corretto, INSTALLA ORA / PIÙ TARDI / IGNORA, setup silenzioso, sostituzione portatile con ripristino e riapertura automatica.
- ZIP stabili per Windows/macOS/Linux accanto agli archivi con versione; progetti, campioni e preferenze conservati. Verifica SHA-256 GitHub prima dell’installazione.
- Prove native di integrità/ripristino, installazione/upgrade/disinstallazione Windows reali e documentazione di installazione/privacy IT/EN aggiornata.

## 0.7.0 — 2026-10-21

- Zoom con rotella centrato sul puntatore in arrangiamento ed editor MIDI, zoom da tastiera, scorrimento sui due assi e trascinamento con il tasto centrale nell’arrangiamento.
- Zoom e loop della selezione; la navigazione manuale sospende SEGUI.
- Note MIDI: spostamento con frecce, passi di ottava, taglia, duplicazione e incolla al cursore con controllo dei bordi della frase e Undo.
- Scorciatoie protette durante i trascinamenti; eliminati i passi Undo vuoti della selezione note e la doppia voce nella generazione su una traccia vuota; la generazione conserva il cursore.
- I gruppi incollati o duplicati nella timeline restano interamente selezionati per le modifiche successive.
- Tooltip in sei lingue, verifiche native e guide Markdown IT/EN aggiornati.

## 0.6.0 — 2026-10-21

- Clip indipendenti nella timeline: spostamento/copia tra tracce, divisione al cursore, taglia/copia/incolla, duplicazione, eliminazione e selezione multipla sulla stessa traccia, con griglia e Undo.
- La corsia Intro/Main/Break/Build-up/Drop/Outro diventa una guida modificabile: bordi, inserimento, rimozione e riordino lasciano al loro posto clip e automazione.
- I clip indipendenti conservano editor MIDI completi, ridimensionamento dei loop, controlli dei motori ed export WAV/MP3/MIDI. I progetti modificati usano il formato .fga 2; il formato 1 resta leggibile.

## 0.5.2 — 2026-10-21

- Ogni traccia è ridimensionabile in altezza dal bordo inferiore del nome. Altezze indipendenti e salvate, doppio clic per ripristinare, Esc per annullare e corsie di automazione allineate. MIDI e posizione di riproduzione restano invariati.

## 0.5.1 — 2026-10-21

- Trascina il bordo del clip per ridimensionare il singolo loop MIDI, oppure il bordo dell’intestazione per l’intera sezione su tutte le tracce. La sorgente si conserva accorciando e si ripete riallungando, senza stirare le note; salvata in .fga e modificabile nel piano roll. Le parti coperte della stessa traccia non raddoppiano le note. Supportati Undo ed Esc.
- Inspector ridimensionabile trascinando il bordo destro; mixer ridimensionabile trascinando l’intestazione. Le dimensioni vengono ricordate; doppio clic sulle rispettive aree ripristina quelle predefinite.
- Freccia nell’intestazione del Track Inspector per richiudere il pannello laterale in una barra stretta e recuperare larghezza per le tracce; il comando resta accessibile mentre si scorrono i controlli.
- Pulsante ABBASSA MIXER nell’intestazione: richiude i fader in una barra sottile e recupera spazio per le tracce; RIAPRI MIXER ripristina l’altezza precedente. Restano disponibili MIXER/F3.
- Clic in qualsiasi punto del clip per posizionare esattamente la riproduzione; Play/Spazio parte da lì, durante l’ascolto il salto è immediato. Funziona anche dopo lo scorrimento orizzontale.
- I righelli di arrangiamento e MIDI mantengono le frazioni di battuta. Il clic oltre la fine del loop permette di ascoltare dal punto scelto; tornando nel loop il ciclo riprende.
- Verifiche native per clic in pausa/durante l’ascolto, scorrimento, uscita/rientro nel loop e note MIDI già iniziate.

## 0.5.0 — 2026-10-21

- Vista DAW con inspector laterale, mixer a 17 canali e master, livelli in dB, pan, mute/solo e indicatore di sovraccarico.
- Righello delle battute, locatori di loop con Maiusc-trascina, selezione dei clip, doppio clic per l’editor MIDI, scambio/copia con trascinamento, adattamento brano e scorrimento durante la riproduzione.
- Correzioni: loop persistente e aggiornato con le sezioni; automazione spostata/scalata correttamente al cambio di durata; meter azzerati in pausa; movimenti delle selezioni MIDI limitati come gruppo; drop di file sulla traccia di destinazione; Save As fallito conserva il percorso attivo; cronologia separata fra progetti; modifiche da tastiera ai cursori annullabili.
- I fader aggiornano il mix senza riordinare o rigenerare l’intero MIDI a ogni movimento. Guide IT/EN, tooltip e sei lingue aggiornati; tema, font e pannelli originali invariati.

- Transizioni di 10 ms su volume e pan durante la riproduzione, per evitare salti bruschi quando si muovono i fader.

- Gestione delle uscite audio disattivate senza accessi a puntatori nulli; i campioni di batteria usano la mappatura MIDI della traccia come nota radice iniziale.

## 0.4.7 — 2026-10-20

- Jackin’: hook a chops dalla libreria originale, basso Wurlitzer più articolato, archi spenti di default e chitarra ritmica a battute alterne nel riferimento funky/disco indicato. Gli altri generi restano invariati.
- Audit con Jackin’ completo e anteprime separate di batteria, basso e tastiera; regressioni su quattro seed e possibilità di riattivare gli archi.

## 0.4.6 — 2026-10-20

- Bilanciamento della batteria corretto tenendo conto dei guadagni già applicati dal motore originale: snare, clap e piatti non vengono attenuati due volte.
- Master dei nuovi progetti a 0,45 e verifica true peak delle tredici anteprime MP3, oltre al controllo PCM dei brani completi.

## 0.4.5 — Timbri per genere e bilanciamento della band

- Tredici palette dei motori guida originali: suoni, kit, timbro, voicing, gate, effetti, livelli e ruoli degli strumenti specifici per stile.
- Basso guida al centro, con passa-basso collegato al timbro; campioni e VST personali non attraversano questa elaborazione.
- Risposte piano/chitarra e ingressi di archi/percussioni per frasi, riducendo la sovrapposizione continua. Conservati pattern utente, blocchi, assegnazioni e MIDI modificabile.
- Audit dei brani completi, tredici anteprime MP3 da otto battute e controllo del basso stereo.

## 0.4.4 — Guida della famiglia Fase Groove

- Ripresa dalla guida Piano la struttura in nove capitoli, con cambio lingua, logo e schermate centrati, separatori, tabelle, ricette, FAQ e chiusura con artista e musica.
- Conservate nelle sezioni corrispondenti tutte le istruzioni Arranger per strumenti, note, forme per genere, suoni, automazione ed export.
- Schermate aggiornate dalla 0.4.3 verificata. Il lettore offline interpreta immagini centrate, corsivi, sottotitoli ed elenchi del Markdown originale.

## 0.4.3 — Automazione volume delle tracce

- Corsia volume opzionale per tutte le 17 tracce: punti trascinabili, griglia/tempi liberi, curve, bypass, cancellazione e annulla/ripeti. Nascosta e disattivata all’avvio; nascondere non disattiva l’ascolto.
- Guadagno audio dopo lo strumento, continuo per campione: motori interni, campioni e VST/AU; stessa elaborazione in ascolto ed export WAV/MP3 di mix e tracce. MIDI invariato.
- Inviluppi salvati nel progetto con campi opzionali; vecchi progetti senza automazione attiva. I punti seguono inserimento, duplicazione, eliminazione e spostamento delle sezioni.
- Rimosse le rampe extra alle velocity aggiunte dall’arranger in intro/outro e lift organici; conservate articolazione ed espressione dei motori originali.
- Controlli e tooltip in sei lingue, guide Markdown aggiornate; verifiche del motore, persistenza, gesti UI e curva audio.

[English](CHANGELOG.md)

## 0.4.2 — 20 ottobre 2026

- Corretti picchi in clipping nei nuovi mix guida: 6 dB di margine aggiuntivo sul master, misura dei picchi prima dell’encoder e rifiuto di audio non finito.

- Arrangiamenti da club: passaggi progressivi per il mixaggio dentro intro/outro, range BPM per genere e lift soulful/organici che mantengono il groove.
- Motivo coerente fra sezioni, frasi regolari per i DJ e continuità armonica anche con numeri dispari di accordi.
- Corretti sconfinamenti inferiori a un tick nell’outro, groove factory del vecchio genere e normalizzazione incoerente nell’export MIDI/audio.
- Audit MIDI/audio completo dei tredici generi, MP3 Soulful di riferimento e note sulle fonti musicali.

## [0.4.1] — In sviluppo

- Pannello OPZIONI con schede IMPOSTAZIONI / INFO ripreso da Guitar: geometria, pulsanti e crediti della famiglia.
- Dimensione interfaccia e verifica release GitHub disattivabile; audio, guida, privacy e licenze nello stesso pannello.
- IconButton originali per trasporto, annulla/ripeti, frecce e dado; proporzioni e gruppi dei pannelli strumenti allineati agli editor di riferimento.
- Branding separato come nelle altre app e nuovo simbolo originale a tracce/clip per Arranger.
- README bilingue e guide Markdown; eliminata l'esportazione HTML aggiunta all'Arranger.

## [0.4.0] — Versione di sviluppo precedente

- Motori Piano, Drums e Guitar standalone, arrangiamento multitraccia, editor MIDI e export MIDI/WAV/MP3.
- Pannelli estesi, strumenti musicali Piano, hosting VST3/AU e progetti `.fga`.
