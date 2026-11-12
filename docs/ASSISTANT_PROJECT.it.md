# Assistenti: progetto corrente e applicazione delle proposte

Estensione approvata dall'utente dei flussi 07/08 sul solo Vocaltest, base
`c5b5dc0`. Nessuna nuova fase della roadmap. Implementazione tecnica verificata su Linux;
nessun GO al rilascio pubblico e nessuna autorizzazione a push/tag/release.

## Flusso

MIX ASSIST / MASTER ASSIST aprono la scheda **Progetto**. **Analizza progetto**
usa note, campioni, strumenti/insert caricati, automazioni, gruppi, sidechain,
ritorni A/B e master correnti. Non richiede un WAV esportato dall'utente.
La scheda **File WAV** conserva il precedente flusso di analisi esterna.
Nel nuovo flusso il riferimento WAV è facoltativo, selezionabile/rimovibile dal
pulsante Riferimento. Gli strumenti mancanti/effetti attivi non collegati sono
errori; non si certifica come completo un render con catena incompleta.

La riproduzione si ferma durante il lavoro offline. Il worker riusa le istanze
caricate, senza clonare/reinstallare strumenti o licenze; la UI rimane nel
polling cooperativo `RenderProgressTask` già usato per Cancel. MainComponent
mantiene l'ownership fino al ritorno del worker, poi ripristina callback audio,
editing e posizione del cursore. Chiusura/Annulla richiedono cancellazione, senza
join/kill sul message thread. Un plugin che non restituisce il controllo resta
un limite: non è introdotto isolamento in processi separati.

Il mix temporaneo è WAV float 32 **prima di Transparent/quantizzazione**, per
misurare i sovralivelli senza nasconderli. Le tracce vengono catturate nello
stesso passaggio dopo insert/EQ/PDC/fader/pan/automazione, prima di gruppi,
ritorni e master. I loro detector sono elaborati nel contesto completo: non sono
render isolati in solo. Per questo la somma delle tracce non coincide
necessariamente con un mix con bus/ritorni/elaborazioni non lineari.
Il taglio PDC delle tracce esclude la latenza aggiuntiva di ritorni/master;
primo impulso e lunghezze allineate sono coperti da regressione dedicata.

Cartelle temporanee private univoche; scrittura/lettura a blocchi. Limiti:
15 minuti inclusa coda, 8 GiB stimati e almeno 64 MiB liberi oltre al necessario.
Nessun intero brano caricato obbligatoriamente in RAM. I buffer di cattura
richiedono `tracce * 2 * blockSize * sizeof(float)` (128 KiB per 32 tracce/512),
oltre ai writer/analizzatori esistenti. Gli estratti A/B restano <=10 s ciascuno.
Il True Peak viene misurato sul mix; picchi/RMS delle tracce riusano il passaggio
FFT/misure già necessario, evitando verifiche True Peak ridondanti sui tap.
Cleanup dei cache su worker, anche quando la finestra viene chiusa durante il job.
Il normale export WAV/MP3/Transparent mantiene opzioni, protezione e file commit.

## Proposte selezionabili, A/B e Undo

La lista mostra destinatario, parametro, valore attuale/proposto e motivazione.
Checkbox indipendenti; nessuna modifica del progetto prima di Applica.

- Guadagno: attenuazione master per headroom/ceiling o target LUFS più basso;
  riduzioni prudenti delle tracce per i conflitti rilevati. Nessun boost automatico.
- EQ: alternativa selezionabile di campana sul basso, 100 Hz/-1,5 dB/Q 0,707,
  usando le bande dell'EQ di traccia esistente. Non un nuovo EQ sul master.
- Compressore nativo già presente: ratio 1,5/2 secondo crest factor, attack
  30 ms e release 180 ms come ipotesi prudenti. Non vengono cambiati threshold,
  knee, makeup o sorgente. Parametri automatizzati, insert bypassati e compressori
  con detector esterno sono esclusi; plugin proprietari non sono rimappati.
- Le proposte rispettano mute/solo e gruppi/ritorni udibili. I consigli soggettivi
  non traducibili in valori restano distinti dalle modifiche applicabili.

**B** genera il render reale della proposta, poi A/B usa il player già esistente
con estratti normalizzati. **Confronta riferimento** seleziona la coppia A/B
mix/riferimento; disattivandolo si torna a originale/proposta. Sono coppie
separate a volume allineato. Servono file stereo e dispositivo allo stesso sample
rate; non viene aggiunta conversione implicita. Se l'inizio è silenzioso si usa la prima finestra
attiva; nessuna nota/posizione del progetto viene spostata.
Per il solo flusso Progetto la normalizzazione può affinare fino a otto volte
l'attenuazione misurata quando cambia il gating LUFS. Restano gli stessi limiti
True Peak, tolleranza 0,1 LU e soglia -60 LUFS; il flusso File WAV mantiene il
comportamento precedente. Estratti non validi non sono riprodotti come A/B valido.

**Applica suggerimenti** verifica il render proposto se non è già in cache,
valida identità/valori/revisione/automazioni e applica le sole righe selezionate
con un checkpoint completo. Un Undo ripristina il batch e i parametri reali
Native; Redo lo riapplica. L'anteprima modifica temporaneamente i parametri
Native soltanto nel render esclusivo e li ripristina anche su errore/Cancel.
Modifiche a progetto, impostazioni o lingua invalidano i risultati; i report
sono istantanee, non analisi live. Nessun nuovo campo nel formato progetto.

Nessuna promessa di LUFS/ceiling o qualità estetica ottimale del mix pre-export:
le non linearità del master richiedono di misurare nuovamente dopo Apply.
Vengono mostrati anche TP/LUFS reali della proposta. Transparent protegge il
normale export; Loud resta non disponibile. Le sorgenti vocali caricate vengono
riprodotte normalmente, senza nuova sintesi o approvazione dei moduli vocali.

## Verifiche

Ambiente locale: Linux x86_64, GCC 14, JUCE 8.0.9, Ninja/Release con self-test.
Nessun nuovo pacchetto o binario di terzi incluso.

| Verifica | Risultato |
| --- | --- |
| Build Release completa | PASS |
| CTest completo | PASS 23/23, 276,72 s |
| CTest mirati dopo protezione plugin sostituito | PASS 4/4, 36,52 s |
| UI Progetto MIX | PASS 24/24 (prima del solo fix al titolo di avanzamento condiviso) |
| UI Progetto MASTER, titolo avanzamento italiano incluso | PASS 25/25 |
| Flusso precedente MASTER File WAV | PASS 23/23 |
| Ricetta nativa UI + PCM/MP3/MIDI/routing | PASS; export/Cancel 46/46 |
| VST3 effetti effettivamente caricati | PASS 79/79; export/Cancel 54/54 |
| VST3 strumento effettivamente caricato | PASS; editor/focus/Freeze 15/15, legacy 44/44, export/Cancel 46/46 |
| A/B conservati, True Peak indipendente FFmpeg 32× | PASS 4/4; volume indipendente PASS 2 coppie |
| Sicurezza callback, routing/PDC/automazioni/bypass/riapertura/Transparent | PASS; suite sidechain e misure ripetute senza allocazioni C++ ordinarie |

La fixture `assistant_project` copre mix float >0 dBFS (nessun clipping nascosto),
lunghezze/PDC/impulso iniziale, detector reale anche se il tap della sorgente non
è richiesto (picco tap 0,466422 con sorgente, 1,6 mutata), Transparent separato,
Cancel ripetuto/cleanup/riprova e rifiuto di directory non vuote senza toccarne i
file. Verifica proposte gain/EQ/compressore, parametri realmente impostati e
ripristinati, rifiuto di automazioni attive e plugin sostituiti con meno parametri.
La UI verifica anche progetto/storia invariati prima di Applica, un solo Undo,
Redo, invalidazione dei risultati e chiusura durante il worker. Screenshot MIX e
MASTER italiani ispezionati. Le prove VST3 aggiunte conservano identità delle
istanze caricate, gruppi/ritorni e Cancel del nuovo capture.

QA indipendente sui WAV 24 bit delle **anteprime normalizzate**, a 44,1 kHz:

| Coppia | TP32 A | TP32 B | LUFS FFmpeg A / B |
| --- | ---: | ---: | ---: |
| MIX originale/proposta | -1,357892 dBTP | -10,593511 dBTP | -27,9 / -27,9 |
| MASTER originale/proposta | -1,357301 dBTP | -9,922868 dBTP | -27,9 / -27,9 |

Ricostruzione indipendente con `adelay=2048S:all=1,apad=pad_len=2048,aresample=1411200:filter_size=1024:phase_shift=10:cutoff=1:exact_rational=1`,
lettura float64 a blocchi; loudness indipendente FFmpeg `ebur128=peak=true`.
Non sono valori del mix applicato o del file finale: l'A/B attenua gli estratti.
Il misuratore del progetto mantiene il gate 0,1 LU; la lettura indipendente è
arrotondata a 0,1 LU. Non è una valutazione della qualità all'ascolto.

Durante lo sviluppo: UI A/B inizialmente fallita per gating LUFS e falsa
invalidazione dal refresh di fine job; corretti con raffinamento solo attenuante
e revisione aggiornata anche in errore. I TP ridondanti sui 18 tap avevano superato
la deadline della fixture: eliminati, senza aumentare le deadline. La prima
ricetta effetti ha fallito due controlli perché l'export dei 18 stem non era
terminato entro 30 s (16 completati), durante altre misure CPU/QA32. Ripetizione
**isolata**, senza modificare codice/deadline: PASS, 18 stem e span fra mtime
23,451 s. Questo identifica la scadenza superata, non dimostra una causa DSP;
resta sensibilità al carico del runner. Errori iniziali conservati come FAIL di
sviluppo, non conteggiati come PASS né corretti allentando i gate.

## Prestazioni e limiti delle misure

- Nuovo capture locale: mix + 2 tap, stereo float/block 512: 2266,65 ms; buffer
  capture 8192 byte. Tempo della fixture, inclusi renderer/scrittura, non prestazione
  garantita su altri progetti. Allocazioni ammesse sul worker, non nel callback.
- Quattro CTest mirati: massimo RSS dei processi figli 56.932 KiB (~55,6 MiB),
  raccolto con `resource.getrusage(RUSAGE_CHILDREN)`; include le fixture/verificatori,
  non rappresenta l'intera DAW con librerie/plugin caricati.
- Mediana callback integrato 512/44100 prima 2,033 ms; dopo due corse 2,764 e
  1,992 ms (p99 5,225 / 3,681 ms). Fixture ritorni 51.200 frame prima 149,948 ms,
  dopo 143,619 / 190,630 ms. Kernel mixer, non misura del nuovo ramo engine:
  1.024.000 frame prima 144,728 ms, dopo 159,659 / 122,337 ms.
- Variazioni del runner impediscono un confronto statistico o una promessa CPU:
  i risultati vanno riportati insieme, senza attribuire i tempi migliori al codice.
  Tutte le prove di allocazione restano a zero per le allocazioni C++ ordinarie;
  nessuna nuova latenza/PDC, nessuna I/O/analisi/file/attesa nel callback. La sola
  aggiunta real-time è la condizione capture nulla; buffer e lavoro sono offline.
- Cache A/B: fino a quattro estratti stereo da 10 s (circa 14,65 MiB a 48 kHz),
  oltre a motore, report e plugin già esistenti. Nessuna stima per plugin arbitrari.

Comandi riutilizzati: `cmake --build build/native --config Release`,
`ctest --test-dir build/native --output-on-failure`, i CTest mirati
`assistant_project|mix_assistant|mastering_assistant|mastering_peak32`,
`scripts/test-native-render.py`, `scripts/test-native-effects.py`,
`scripts/test-native-host.py` con `--build-dir build/native`, e
`fga_engine_tests --sidechain`. Ricette UI eseguite con Xvfb locale, scadenze
originali. Log, XML, screenshot e WAV di prova in
`/workspace/scratch/assistant-project/` (evidenza locale, non asset distribuiti).

Windows/macOS/AU aggiornati, ascolto/hardware/dropout, plugin commerciali e nuovi
sanitizers NOT RUN. Il precedente Cancel Windows su f98b706 e la conferma utente
restano evidenze storiche valide, non collaudo Windows di questa nuova estensione.

## Collaudo manuale

Aprire una copia del progetto, scegliere Progetto e Analizza senza caricare WAV.
Verificare livelli/soglie e destinatari. Selezionare poche modifiche, provare A/B,
confronto riferimento facoltativo e Applica. Controllare i parametri nel mixer,
EQ e Native Compressor; un Undo/Redo deve ripristinare/riapplicare tutto.
Provare salvataggio/riapertura, progetto con sidechain/mandate e plugin reali,
Annulla nelle varie fasi, chiusura durante il job e successiva nuova analisi.
La preview Windows f98b706 non contiene questa estensione né la localizzazione
successiva; serve un nuovo artifact autorizzato prima del collaudo sul PC utente.
