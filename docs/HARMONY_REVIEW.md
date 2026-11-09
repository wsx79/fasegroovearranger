# Revisione armonia e rigenerazione — 9 novembre 2026

Base dell’analisi: 0.41.0, `ba61de9`; integrati anche gli aggiornamenti 0.42.0 fino a `bfbc0f7`, senza modifiche a core o test di questa revisione. Verifica del generatore locale, senza Composer IA.

## Difetti riprodotti

- Il controllo anti-ripetizione del piano confrontava il generatore tradizionale anche quando veniva suonato quello elettronico. Su 128 rigenerazioni controllate: solo 3 figure di accordi Electro e 50 ripetizioni immediate; lead Electro 8 figure e 13 ripetizioni immediate. In Techno: 6 figure di accordi, 28 ripetizioni immediate; lead 16 figure, 9 ripetizioni.
- L'accompagnamento elettronico di chitarra sceglieva l'accordo a inizio battuta, anche se l'attacco cadeva dopo un cambio a mezza battuta: 76 note incoerenti su 128 nella prova per ciascun genere elettronico.
- Nei drop Techno il seed poteva inserire una quarta non richiesta contro la terza del lead. La prova frigia riproduceva 304 note estranee su 2080.
- La generazione generale usava spesso cammini casuali tra gradi; Random Chords poteva tagliare e ruotare una frase armonica più lunga, perdendone la chiusura.

## Intervento

Progressioni complete del vocabolario di ciascun genere, con sviluppo A/B compatibile e frasi automatiche di 2/4/8 accordi nei generi tradizionali. Random Chords preferisce frasi della lunghezza richiesta e conserva gli accordi manuali. Techno mantiene pedali e vamp lenti; Electro amplia le progressioni minori. Ritmo, contorno melodico e risposta sono scelti separatamente. Le frasi elettroniche hanno chiusure riconoscibili e colori deboli risolti. Le note armoniche seguono l'accordo all'attacco e terminano prima del cambio successivo. Il controllo anti-ripetizione confronta le note realmente generate in un'anteprima limitata a quattro battute, mantenendo la durata reale della sezione.

Nessun cambiamento al formato progetto o ai motori vendorizzati. Il MIDI salvato non viene rigenerato all'apertura. APPLY mantiene riproducibili i nuovi take; gli scope esclusi e i blocchi vengono verificati.

## Tutti i 15 generi

Per genere: 24 nuove canzoni, ciascuna ripetuta dallo stesso stato e seed per verificare il determinismo; 64 rigenerazioni di clip per piano, basso, chitarra e lead. Totale: 720 generazioni complete e 3840 rigenerazioni locali. Le firme misurano attacchi quantizzati ai sedicesimi e intervalli, ignorando velocity, jitter e trasposizione.

| Genere | Piano | Basso | Chitarra | Lead | Ripetizioni immediate |
|---|---:|---:|---:|---:|---:|
| Piano House | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Soulful | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Deep House | 63/64 | 55/64 | 64/64 | 64/64 | 0 |
| Gospel | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Jackin House | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Piano Jack | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| UK Garage | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Afro House | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Funky & Disco | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| 90s House | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Neo Soul | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Chill | 62/64 | 55/64 | 64/64 | 64/64 | 0 |
| Dance Stabs | 64/64 | 64/64 | 64/64 | 64/64 | 0 |
| Techno | 34/64 | 64/64 | 33/64 | 61/64 | 0 |
| Electro House | 34/64 | 64/64 | 33/64 | 61/64 | 0 |

Ulteriori regressioni: cambi a mezza battuta e clip fuori griglia in tutte le 12 tonalità, note estranee nei drop, durata delle note ai cambi, anteprima fedele nei build, accordi manuali, scope, blocchi e APPLY. Le suite core includono anche i contratti armonici tra strumenti e la verifica indipendente dei MIDI esportati.

Questi risultati misurano coerenza e diversità, non sostituiscono la valutazione musicale all'ascolto. Non è una verifica dei suoni o del comportamento di plugin esterni.
