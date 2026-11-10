# Prova manuale della voce guida — 01C

Da eseguire nella DAW su Linux. **Questa procedura non è ancora un test di ascolto
superato. Windows, macOS e qualità sonora restano non validati.** La voce è una guida
sintetica in inglese destinata a una successiva registrazione con una cantante reale.

## Preparazione

Avvia la build del branch Vocaltest con Perl >=5.22, MIDI, URI::Escape, Math::FFT,
eSpeak classico e SoX installati localmente. Prepara una copia del sorgente eCantorix
con lo [script documentato](../helpers/ecantorix/README.md); non usare un eseguibile
o un modello trovato casualmente. Scegli solo script fidati. Nessun servizio online
è necessario. Le dipendenze non sono incluse nella DAW.

## Melodia e testo

1. Crea un progetto a **125 BPM**: intro di 16 battute, parte principale di 8 battute,
   outro di 16 battute. Aggiungi **Vocal Studio (English)** come traccia MIDI separata.
   Nella timeline estendi la clip vuota dall’origine fino alla fine del progetto
   (**160 beat**): la clip iniziale copre soltanto la prima sezione.
2. Nel piano roll esistente crea note senza sovrapposizioni. Nella prima battuta
   della parte principale usa questa frase (posizioni in beat rispetto alla clip,
   se la clip inizia all'origine della canzone):

   | Inizio | Durata in beat | Nota MIDI | Parola |
   |---:|---:|---|---|
   | 64 | 1 | 60 (Do4) | We |
   | 65 | 1 | 62 (Re4) | sing |
   | 66 | 2 | 64 (Mi4) | here |
   | 69 | 1 | 67 (Sol4) | We |
   | 70 | 2 | 64 (Mi4) | stay |

   Lascia una pausa fra beat 68 e 69. Puoi ripetere la frase entro gli 8 bar.
   Aggiungi anche due Do4 di un beat: al beat 0 nell'intro e al beat 96
   all'inizio dell'outro, così anche il test dell'override resta entro 60 secondi.
3. Apri Vocal Studio. Scrivi **“We sing here. We stay.”** nel testo inglese. Aggiungi
   una sillaba/parola per ogni nota della tabella, seleziona la nota nel piano roll
   e premi **Assign selected roll notes**. Assegna “We” anche alle note di prova.
   Il testo generale da solo non assegna automaticamente parole alle note.
4. Imposta la regione vocale per includere la clip e lascia **Allow singing in DJ
   intro / outro** disattivato. Seleziona **eCantorix / eSpeak English guide** e il
   percorso assoluto del `ecantorix.pl` preparato; premi **Apply text / settings**.

## Render, ascolto ed esportazione

5. Premi **Render English guide**. Durante l'attesa verifica che trasporto e
   interfaccia rispondano. Prova **Cancel render**, poi ripeti il render: non devono
   comparire take parziali o processi di sintesi rimasti attivi.
6. Quando pronto, premi **Add guide audio take**. La DAW aggiunge una traccia audio
   e mette in mute il MIDI sorgente. Usa il Solo della guida per ascoltarla con il
   metronomo. Verifica note, testo, pausa 68–69, note tenute e silenzio nell'intro/outro.
   Regola il volume prima dell'ascolto. Annota separatamente intelligibilità e qualità:
   il superamento dei test tecnici non garantisce una voce naturale.
7. Prova Undo/Redo dell'importazione. Cambia una parola o una nota e ripeti il
   render: la nuova take deve riflettere la modifica. Le vecchie take restano
   disponibili; silenziale prima di confrontare. Per verificare l'override,
   abilita il canto in intro/outro e genera una take separata.
8. Premi **Export added guide WAV for singer**, scegli codifica e destinazione.
   Il nome include BPM e tonalità. Riascolta il WAV in un lettore o reimportalo:
   deve partire dall'origine della canzone, con l'intro silenzioso e la voce allineata.
9. Salva e riapri il progetto. Usa **Collect portable project** prima di spostarlo:
   il WAV è un file esterno, non è incorporato nel JSON. Non cancellare la cache se
   contiene ancora take referenziate da progetti non raccolti.

Registra sistema/build, versione delle dipendenze, esito di ogni passo ed eventuali
errori. Per i test automatici e i problemi ancora aperti vedi
[VOCAL_GUIDE_VALIDATION.md](VOCAL_GUIDE_VALIDATION.md).
