# Plugin, finestre ed export — 30–31 ottobre 2026

Base: `c993465b503980a218e5e132cecbc7cea9e35c86` (**v0.30.2**). Verifiche concluse il 31 ottobre, ora italiana. Pubblicazione **0.30.3 autorizzata dall’utente**: commit, push e release con le modalità già usate dalla famiglia, comprese guide e quattro pacchetti di distribuzione.

## Cosa cambia

- Un effetto appena caricato apre subito l’editor al centro e in primo piano. Il ripristino di un progetto non apre automaticamente gli editor. Più strumenti ed effetti possono restare visibili insieme: ogni finestra appartiene alla propria traccia e al proprio slot, si chiude manualmente e si ricentra alla riapertura. Sostituire/rimuovere un plugin chiude soltanto il relativo editor; riaprire un progetto chiude gli editor dei processori precedenti.
- Il pulsante tastiera sotto l’ingranaggio dell’inspector apre **solo lo strumento**, senza il menu. Tooltip con il nome del VST. Gli effetti si aprono dalla propria riga. L’ingranaggio della timeline continua ad aprire il motore interno quando sulla traccia sono caricati soltanto effetti. La chiusura dell’inspector è un piccolo segno senza il precedente riquadro, con feedback circolare al passaggio del mouse.
- **FX** nel mixer apre una finestra compatta con la stessa catena dell’inspector: bypass, editor, rinomina, riordino, aggiunta/rimozione e automazioni. La finestra conserva la propria traccia cambiando selezione e si chiude se quella traccia non esiste più. Menu e rinomina mantengono l’identità corretta anche cambiando destinazione della finestra.
- Il separatore nel browser usa una conversione UTF-8 esplicita: eliminato il carattere `Â` tra Effetto/Strumento e VST3/AU.
- Il bypass usa checkpoint e aggiornamenti del mix, senza acquisire lo stato nativo del plugin. Annulla/Ripristina del mix non serializza/ripristina il VST e conserva modifiche indipendenti fatte nel suo editor. Il salvataggio esplicito continua ad acquisire lo stato aggiornato.
- La scansione delle sottocartelle era già ricorsiva. Ora segue anche cartelle collegate, deduplica i percorsi risolti e interrompe i cicli. Restano scansione isolata, cache e gestione dei plugin falliti.
- L’export WAV offre **16/24 bit PCM**, **44,1/48/88,2/96/192 kHz**; conserva l’ultima codifica. Anche il bounce da Studio usa queste opzioni. L’ultima cartella di export è distinta da quella di salvataggio dei progetti. Preferenze nuove: `exportSampleRate`, `exportBitDepth`, `lastExportFolder`, `lastSaveFolder`.
- Nomi di **MIDI, WAV e MP3** con BPM e tonalità, coerente con la grafia della chiave nella UI; suffisso aggiunto una sola volta, ad esempio `_128bpm_Db`. I BPM decimali sono rappresentati fino al millesimo. Gli stem seguono l’ordine musicale. Le cartelle di export separato includono `Arrangiamento_<BPM>bpm_<chiave>.txt`, con intervalli delle sezioni al millisecondo. Le code audio sono distinte dalla durata della struttura.
- I WAV contengono anche metadati ACID BPM/metrica e un commento con tempo e tonalità. Sono **one-shot**, perché gli stem dell’intero brano includono code: non viene inventato un numero di beat che imporrebbe un loop errato. Il riconoscimento automatico dipende dalla DAW che importa; nome e TXT forniscono sempre il BPM.
- L’export audio sospende il callback del dispositivo e ferma l’ascolto. Usa **le istanze già caricate**, evitando di ricaricare strumenti, librerie e licenze. Mantiene il DSP nella modalità di riproduzione; alla stessa frequenza non reinizializza i processori per ogni file. Cambia e ripristina la preparazione solo se serve un’altra frequenza. Audio e comandi tornano disponibili dopo successo, annullamento ed errore; il trasporto resta fermo. Play/seek non interferiscono con il rendering. L’API interna `offlineCopy` resta disponibile e verificata, ma non è usata dal normale export UI.
- Guide IT/EN, traduzioni nelle sei lingue e changelog aggiornati. Tema/font condivisi conservati.

## Verifiche ed evidenze

Tutte eseguite su Linux, compilazione Release:

- **CTest 7/7 PASS**: anche intestazioni WAV a 44,1/48/96 kHz, 16/24 bit, contenuto audio, metadati, suffissi, BPM decimali, chiave bemolle, orari e annullamento senza WAV parziale. Una sonda verifica l’export dello stesso strumento, con/senza due insert, senza duplicazione, reinizializzazione alla stessa frequenza o cambio in modalità DSP offline.
- Hosting VST3 nativo: nota A4 misurata a circa 441 Hz, rapporto gain 3,000, stato ripristinato e apertura/Play dei progetti formato 1/2/3 e aggiornati a 4. Tre editor di strumenti visibili insieme; chiusura/riapertura indipendente. Uscite secondarie degli strumenti mantenute.
- Percorso UI di export con VST reale: WAV udibile 48 kHz/16 bit; tutti gli stem 44,1 kHz/24 bit; nomi con BPM/chiave e TXT esatto; MIDI leggibile; dialogo codifica funzionante; ripristino comandi/audio dopo completamento, annullamento e scrittura fallita; cartelle ricordate.
- Effetti VST3 nativi: autoapertura, editor indipendenti, catena/ordine/automazioni/stato/export, finestra FX in primo piano e centrata, destinazione indipendente dalla selezione. **16 cambi di bypass senza blocchi silenziati**, con editor aperto e sonda che impiega 20 ms a leggere lo stato. Verificato anche l’ingranaggio con motore interno più soli insert.
- Suite UI/audio completa: 13 stem attivi allineati a **305809 frame / 44100 Hz**, WAV/MP3, mix muto, variazione udibile tra rigenerazioni e controlli Studio/mixer/griglia.
- Scanner nativo: sottocartelle, link con ciclo senza duplicati, cache, aggiornamenti, quarantena/riprova, rimozione/ritrovamento. Modulo condiviso, tema, font, guide, traduzioni e `git diff --check`: PASS.

Log, rapporti test e immagini sono nel pacchetto `/workspace/reports/release-0.30.3/`. Le immagini mostrano opzioni WAV, inspector e finestra FX.

## Limiti da confermare

**Korg M1 su Windows non è disponibile in questo ambiente.** È stato corretto il percorso che duplicava il VST e cambiava modalità DSP, possibile causa del WAV vuoto, e sono state verificate le istanze esistenti con plugin di test. La risoluzione del caso specifico M1 richiede la prova dell’utente con la build Windows. Gli esiti Windows/macOS/Linux sono nella CI della release 0.30.3; resta necessaria la prova specifica con M1 sul Windows dell’utente.

La prova sul bypass riguarda la sospensione causata dall’acquisizione dello stato, con plugin di test senza latenza; non certifica ogni transitorio o buffer interno dei plugin commerciali. La modalità DSP di export mantiene le impostazioni di riproduzione, comprese le eventuali opzioni di qualità che il plugin distingue dalla modalità offline.

Su Windows/Linux sono supportati VST3, su macOS anche AU. I VST2 `.dll` non sono inclusi. È disponibile una cartella personalizzata oltre a quelle standard. Senza nomi e percorsi dei plugin mancanti non è possibile attribuire l’intera differenza rispetto ad Ableton alle cartelle collegate; il confronto specifico resta aperto perché l’utente non può attualmente fornire esempi.

## Tornare indietro

Il pacchetto esterno contiene `forward.patch`, `reverse.patch`, manifest con impronte dei file, evidenze e una copia di questo report. La patch inversa annulla **tutto questo intervento**, riportando esattamente alla base v0.30.2; non annulla modifiche precedenti già presenti in quella versione.

1. Conservare eventuali modifiche successive e i propri progetti/campioni.
2. Dalla repository controllare `git apply --check /workspace/reports/release-0.30.3/reverse.patch`.
3. Solo se il controllo riesce, applicare `git apply /workspace/reports/release-0.30.3/reverse.patch`.
4. Ricompilare l’app. `forward.patch` consente di riapplicare l’intervento alla base indicata.

Il ciclo base → patch → patch inversa → base è verificato con un indice Git temporaneo, senza toccare l’indice reale. Progetti e file audio non sono modificati dalle patch; le quattro preferenze nuove possono restare nel file impostazioni ed essere ignorate dalla versione precedente. Nessuna cronologia Git viene riscritta.

Dopo la pubblicazione, su checkout pulito, `git revert --no-commit v0.30.3` prepara il ripristino dei sorgenti alla 0.30.2. Controllare il diff e ricompilare prima di un eventuale commit e push di ripristino. Il pacchetto esterno `publication.json` registra commit, CI e asset effettivamente pubblicati.
