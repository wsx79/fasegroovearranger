# Arrangiamenti da club: fonti, scelte e verifica

Fase Groove Arranger 0.4.2 · 20 ottobre 2026

## Fonti lette

- [Phil Morse — Rock The Dancefloor!, capitoli sul beatmixing](https://www.digitaldjtips.com/app/uploads/delightful-downloads/2017/07/Rock-The-Dancefloor-by-Phil-Morse.pdf), pp. 113–120 della numerazione stampata: allineamento delle frasi e cambiamenti sul primo movimento. Da qui la griglia di frasi regolari per i passaggi DJ, senza ridurre ogni genere a una sola forma.
- [Adam Douglas / Attack Magazine — Joe Smooth, Promised Land](https://www.attackmagazine.com/technique/deconstructed/joe-smooth-promised-land/): analisi di una struttura house con ingresso percussivo, sviluppo a strofe/ritornelli, risposte e cambi di strumentazione. È un esempio concreto contro l'idea che ogni pezzo debba usare lo stesso build/drop da festival. Non vengono copiate note o melodie del brano.
- [Beatportal — guida alla produzione Afro House](https://www.beatportal.com/articles/647491-step-by-step-guide-to-creating-an-afro-house-track-keinemusik-black-coffee-caiiro-alex-wann-style): riferimento per BPM 115–125, centralità delle percussioni, basso essenziale, sviluppo progressivo e break atmosferici. I tempi indicativi dell'articolo non vengono trasformati in durate fisse nell'app.
- [The DJ Revolution — Electronic Dance Music BPMs](https://www.thedjrevolution.com/electronic-dance-music-bpm/): confronto dei range tipici di house, soulful, deep, afro, disco e UK garage. È una guida editoriale, non uno standard vincolante per tutta la musica esistente.

Le durate e le soglie implementate sotto sono scelte di Arranger basate su questi riferimenti e sui suoi motori. Non sono regole universali né una trascrizione dei brani analizzati.

## Tempo e identità dei tredici stili

| Categoria presente nei motori | Range operativo BPM | Centro | Forma / carattere |
|---|---:|---:|---|
| Piano House | 120–128 | 124 | Tema pianistico, break armonico, lift e ripresa |
| Soulful | 118–125 | 122 | Strofa, risposta, break e ritornello; lift musicale |
| Deep | 115–125 | 120 | Groove ipnotico, atmosfera, evoluzione e ritorno |
| Gospel | 118–125 | 123 | Strofa/risposta e ripresa corale; famiglia soulful |
| Jackin' | 124–130 | 126 | Break brevi, workout percussivo e ripartenze |
| Jackin' Piano | 124–130 | 126 | Jack pianistico, taglio breve, build e variazione |
| Garage & UK | 130–135 | 132 | Shuffle/two-step, break di basso e più drop |
| Afro & Latin | 115–125 | 122 | Intro percussiva, call/response e ritorno organico |
| Funky & Disco | 112–124 | 120 | Strofe, ritornelli e break di archi |
| '90s Piano House | 120–130 | 126 | Tema, breakdown pianistico, build e ripresa anthem |
| Neo Soul & Broken | 92–112 | 98 | Strofa, chorus e bridge su ritmo spezzato |
| Chill & Pads | 90–115 | 100 | Warm-up, paesaggi armonici e ritorno morbido |
| Dance Stabs | 124–130 | 128 | Tema a stab, tensione, drop e reprise |

I sottostili Piano/Gospel/Jackin'/Stabs restringono deliberatamente la famiglia house al carattere dei motori. Neo Soul & Broken e Chill & Pads sono categorie ibride già presenti nei plugin: i loro range sono una scelta da warm-up/downtempo dell'app, non una definizione di tutto il broken beat o di tutta la chill music.

Alla scelta del genere, il centro viene adattato leggermente a densità e seed e resta nel range. Il controllo BPM e la normalizzazione dei progetti usano lo stesso range: Soulful non genera una canzone a 140 BPM. Progetti precedenti con un tempo fuori range vengono ricondotti al limite più vicino, senza riscrivere le note MIDI già salvate.

## La forma e i passaggi per il DJ

- Intro e outro sono sezioni effettive del brano: non ci sono clip di mixaggio aggiunte prima o dopo la canzone.
- Le forme sono distinte per genere. Deep e Chill non ricevono un build standard a rullate; Soulful, Gospel, Jackin' e Afro mantengono il groove nei lift. Gli stili più energici possono usare il build originale dei motori.
- L'ingresso parte con il riferimento ritmico; entrano poi altri elementi percussivi, armonici e basso in ordine diverso per stile. L'outro effettua il processo inverso. La prima e l'ultima frase da otto battute rimangono ritmiche. Le intro/outro sono normalmente di 16 o 32 battute secondo il profilo, con spazio aggiuntivo per motivi lunghi.
- Gli accenti, swing, voicing, strumenti e pattern sono quelli dei motori originali. La DAW ne dispone ingressi/uscite e sviluppo lungo il brano. Le note rimangono modificabili.
- La durata deriva da `battute × 240 / BPM` in 4/4. Non si allungano automaticamente i brani fino a raggiungere cinque minuti. Motivi lunghi, cicli armonici e materiale più rarefatto possono richiedere più frasi.
- I confini delle sezioni generate restano su multipli di otto battute. Quando un ciclo armonico e un motivo richiederebbero una frase comune eccessiva, il giro continua attraverso il confine invece di creare sezioni da 35 battute o interrompere arbitrariamente la progressione. Le sezioni aggiunte/modificate manualmente possono avere altre lunghezze.

## Bug corretti

1. Il seed del clip poteva scegliere un altro preset pianistico/chitarristico a ogni sezione. Ora il motivo appartiene alla canzone; i clip ricevono variazioni e interpretazioni proprie.
2. Le intro/outro dei plugin erano ripetute come loop statici. Gli ingressi e le uscite ora si sviluppano dentro la sezione; il basso può entrare tardi e poi lasciare spazio al mixaggio.
3. Il fallback delle progressioni dispari perdeva la griglia delle frasi DJ. Corrette la suddivisione e la fase armonica condivisa anche da Piano/Bass/Strings/Guitar.
4. Note anticipate vicinissime al taglio dell'outro potevano essere riallungate dalla durata minima MIDI. Ora non sconfinano nell'ultima frase ritmica.
5. Cambiare genere poteva lasciare selezionato un vecchio groove factory incompatibile. Si torna al groove automatico del nuovo genere, conservando i pattern scritti dall'utente.
6. Il rendering completo ha rilevato clipping in sette mix guida predefiniti, non visibile nel test breve. I nuovi brani partono con 6 dB di margine in più sul master (0,8 → 0,4), mantenendo i rapporti fra gli strumenti. I livelli dei progetti già salvati non vengono cambiati. L’audit misura anche il picco prima della conversione/clamp, non soltanto il WAV.
7. Gli export potevano normalizzare le note ma usare tempo, tonalità o lunghezza del progetto non normalizzato per metadati e rendering. Ora usano lo stesso progetto validato; indici di stem non validi sono rifiutati.

Blocchi di tracce/clip, rigenerazione isolata, editing MIDI e gli strumenti esistenti rimangono disponibili. Queste modifiche non riprogettano l'interfaccia.

## Verifica riproducibile

- `bash scripts/test-core.sh`: tutti i generi, frasi e passaggi ritmici, limiti BPM, variazioni, blocchi, scope e casi dispari. Produce MIDI completi e tabelle di attività per ogni frase in `build/core/club-*`.
- `python3 scripts/verify-midi.py build/core`: lettore SMF indipendente, diciotto tracce, marker, note on/off abbinate e assenza di note sospese.
- L'app nativa con `--club-audit CARTELLA` rende tutti e tredici i brani completi, controllando campioni finiti, headroom e assenza di intere frasi accidentalmente mute. Salva progetti `.fga`, MIDI, un report e un MP3 Soulful completo con i suoni guida.
- `python3 scripts/verify-club-audio.py CARTELLA`: controlla durata calcolata/progetto e decodifica il riferimento MP3 con FFmpeg. L'audit nativo usa i suoni integrati; i VST e i campioni personali richiedono il normale ascolto e bilanciamento nel progetto dell'utente.

Le misure verificano struttura, integrità e rendering. La valutazione artistica del brano va fatta anche ascoltandolo; l'MP3 di riferimento serve a questo confronto.

## Aggiornamento 0.4.3: volume indipendente dalle note

Le rampe di velocity aggiunte dall’arranger alle parti melodiche di intro/outro e ai lift organici sono rimosse. Restano l’articolazione originale dei motori e gli ingressi/uscite degli strumenti per frasi. Il volume può ora essere disegnato nella corsia di automazione audio di ciascuna traccia, inizialmente nascosta e in bypass, senza riscrivere il MIDI. La forma e i BPM seguono la stessa politica 0.4.2 documentata sopra.
