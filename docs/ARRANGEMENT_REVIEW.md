# Revisione degli arrangiamenti — 9 novembre 2026

Intervento sul generatore locale dei 15 generi, dopo la revisione armonica; base `dde59d1`, con main `bfbc0f7` già integrato. Nessuna modifica al formato progetto o ai motori vendorizzati.

## Problema verificato

Per ogni genere, 24 rigenerazioni complete dallo stesso progetto cambiavano le durate ma producevano una sola sequenza di ruoli delle sezioni. Lo sviluppo di Lead e percussioni era limitato, soprattutto nei due generi elettronici. La scelta automatica dei fill poteva inoltre reintrodurre rullate fitte nei lift organici, anche usando il ruolo Main del motore.

## Comportamento risultante

- Quattro architetture per genere: classica, sviluppo esteso con ritorno conclusivo, versione compatta o risposta aggiuntiva al tema e anticipo breve del tema. Una rigenerazione completa evita la sequenza appena usata. Il progetto iniziale conserva la forma classica.
- Intro e outro automatici di 16/32 battute, ai veri estremi del brano. Prime e ultime otto battute ritmiche: cassa presente, assenza di Piano/Bass/Strings/Guitar/Lead e delle loro note tenute.
- Griglia di otto battute anche con cicli armonici dispari o lunghi. Intro/outro e break non sono costretti a contenere l’intero ciclo armonico: il giro continua attraverso i confini.
- Lead con pause e risposte; ride/tamburello entrano per frasi e aumentano la presenza nelle riprese. Le parti elettroniche ricevono lo stesso sviluppo di orchestrazione. Cassa e basso lasciano un beat di respiro prima dei drop nei build energici.
- Nei lift organici, i fill automatici usano piccoli turnaround di hat, conga, rim e percussioni. Restano disponibili i fill esplicitamente scelti e i pattern utente.
- Le forme riconosciute come personalizzate, le armonie locali, le transizioni impostate a mano e le parti manuali impediscono la sostituzione automatica della sequenza. Blocchi e scope restano rispettati. APPLY riproduce il take memorizzato.
- NUOVO ARRANGIAMENTO dispone i take esistenti senza rigenerarli e conserva la durata delle intro/outro factory. Quando la durata non cambia, mantiene le note esattamente, evitando anche ricostruzioni di loop inutili.

## Verifiche riproducibili

```sh
cmake --build BUILD
ctest --test-dir BUILD --output-on-failure
BUILD/fga_core_tests --arrangement-review CARTELLA_MIDI
python3 scripts/verify-midi.py CARTELLA_MIDI --club-only
BUILD/fga_engine_tests_artefacts/Debug/fga_engine_tests --arrangement-audit CARTELLA_AUDIO
```

La regressione dedicata controlla 24 nuove canzoni per ciascuno dei 15 generi (360 progetti), bookend DJ, griglia, Lead, pause prima dei drop, contrasto di attività MIDI fra break e ritorno, determinismo, APPLY, scope, strutture personalizzate, armonie locali e riuso dei take. La varietà è misurata sui ruoli, ignorando nomi e durate: da ogni forma classica iniziale risultano tre alternative distinte su 24 estrazioni; la classica torna disponibile nelle rigenerazioni successive.

Il lettore SMF indipendente controlla tutti i 15 export: 19 tracce, tempo, marker, coppie Note On/Off, durata positiva e assenza di note sospese. La struttura compatta può avere cinque sezioni, quindi non viene scartata per il precedente minimo fisso di sei marker.

L’audit audio usa un brano rigenerato con seed 42 per genere, i suoi MIDI reali, suoni guida e automazioni. Esporta sei finestre di otto battute: ingresso DJ, tema, break, ritorno, finale e uscita DJ (90 WAV). Controlla campioni finiti, audio presente, margine prima del clamp e RMS del ritorno superiore al break. Le finestre ritagliano i take, senza creare nuovi riff.

Le misure di attività MIDI e RMS verificano aspetti diversi dello sviluppo; non sono un punteggio artistico. I frammenti audio sono prove con i suoni guida e attacchi riallineati al loro inizio, non un ascolto continuo completo o una verifica di VST, campioni personali e hardware reale. La verifica locale è Linux/JUCE 8.0.9; macOS e Windows restano da verificare nei rispettivi ambienti.

## Risultati

Sette suite CTest superate in Release (20,92 s) e con ASan/UBSan e leak detection (187,52 s complessivi). La regressione degli arrangiamenti è separata dal resto del core per mantenere ciascuna prova entro il timeout CI. Suite nativa audio/trasporto/MP3: uscita 0; il log Debug contiene ancora assert JUCE già presenti nelle suite. Tutti i 90 frammenti audio hanno superato i controlli.

| Genere | Sequenze prima → dopo, su 24 | Ritorno rispetto al break, dB RMS | Picco massimo raw, dBFS |
|---|---:|---:|---:|
| Piano House | 1 → 3 | +14.29 | -5.72 |
| Soulful | 1 → 3 | +10.16 | -5.81 |
| Deep House | 1 → 3 | +7.77 | -5.08 |
| Gospel | 1 → 3 | +10.19 | -5.09 |
| Jackin' | 1 → 3 | +14.52 | -6.25 |
| Jackin' Piano | 1 → 3 | +15.73 | -6.28 |
| Garage & UK | 1 → 3 | +14.75 | -7.27 |
| Afro & Latin | 1 → 3 | +12.21 | -5.82 |
| Funky & Disco | 1 → 3 | +12.57 | -5.33 |
| '90s Piano House | 1 → 3 | +14.49 | -4.95 |
| Neo Soul & Broken | 1 → 3 | +11.39 | -8.46 |
| Chill & Pads | 1 → 3 | +12.37 | -10.37 |
| Dance Stabs | 1 → 3 | +16.41 | -6.08 |
| Techno | 1 → 3 | +16.91 | -6.22 |
| Electro House | 1 → 3 | +17.88 | -3.65 |

Il picco più alto misurato è 0,656652 (circa −3,65 dBFS), prima del clamp. Il confronto MIDI conferma un ritorno più attivo del break in tutti i 360 progetti controllati; il confronto audio riportato sopra usa seed 42 e non implica che ogni ripresa debba sempre essere più forte della precedente. Il finale può sviluppare la strumentazione senza imporre una crescita continua del volume.

I WAV e i report locali si trovano in `/workspace/scratch/arrangement-audio`; i 15 MIDI e le tabelle per frase in `/workspace/scratch/arrangement-midi`. Si rigenerano con i comandi sopra.
