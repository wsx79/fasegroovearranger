# Novità — Fase Groove Arranger

## Non pubblicato — 2026-10-22

- GENERA BRANO sceglie tonalità, progressione del genere e figura di basso diverse. Le note armoniche bloccate mantengono l’armonia attuale; la rigenerazione parziale resta nell’armonia del brano.
- La rigenerazione lascia preset e Riff seed fissi, combina frasi pianistiche del genere, varia il ritmo del basso e crea nuovi pattern di chitarra Jackin’. APPLICA conserva parametri espliciti e versioni salvate; restano blocchi, strumenti, mix e note fuori ambito.
- Test dei ritmi di piano/basso/chitarra senza humanizzazione sui tredici generi, inclusi preset e seed fissi; aggiornati messaggi e guide IT/EN.

- Distribuzione come Piano/Guitar/Drums: uno ZIP per piattaforma e setup Windows, solo file di esecuzione, guide con risorse collegate e licenze. Prove CI raccolte in un solo artifact separato.
- Il clic sul nome seleziona la traccia; l’ingranaggio ambra accanto a Solo apre il motore. CHITARRA SOLISTA disponibile nella barra principale, con blocchi e ANNULLA.
- Rimossi i comandi pianistici ignorati dalle modalità indipendenti di basso/archi, i controlli guida inutili con campioni/plugin e i doppioni di routing/batteria. Le modalità e il suono assegnato aggiornano i controlli disponibili.

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
