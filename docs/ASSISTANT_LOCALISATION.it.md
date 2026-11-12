# Mix/Master Assistant: localizzazione italiana

Base `dd9f5a8`, branch esclusivo Vocaltest. Segnalazione: finestre degli
assistenti completamente in inglese anche con DAW in italiano.

## Causa e correzione

Le fasi 07/08 costruivano direttamente in inglese pulsanti, titoli, istruzioni,
misure, errori e suggerimenti. Questi testi non passavano dal catalogo della DAW.

- Riutilizzati `tr`, catalogo prodotto e binding esistenti: nessun traduttore,
  servizio esterno o framework aggiunto. Aggiunte 150 voci italiane; le nuove
  voci nelle altre quattro lingue mantengono il precedente fallback inglese.
- Pulsanti, titoli, tooltip, note, caselle frequenza/bit, errori, QA e suggerimenti
  seguono l'italiano/inglese scelto nella DAW. LUFS, dBTP, WAV, stem ed altri
  termini tecnici restano riconoscibili. Nomi di file/tracce e numeri non vengono
  tradotti o sostituiti.
- Un cambio lingua conserva i file selezionati e invalida soltanto il report/
  ascolto temporaneo; premere Analizza per rigenerarlo nella lingua corrente.
  Un eventuale job attivo riceve annullamento cooperativo e non pubblica risultati
  della lingua precedente. Nessun join UI o cambiamento del wrapper Cancel.
- I testi di istruzione vanno a capo senza essere compressi orizzontalmente.
- Il codice delle due analisi differisce esclusivamente per chiamate di
  traduzione/include. Calcoli, soglie, guadagni, buffer e DSP sono invariati;
  nessuna nuova operazione nel callback audio, dipendenza o modifica dei progetti.

## Verifiche

- PASS build Release Linux, applicazione e test.
- PASS CTest mirati **3/3** in **14,16 s**: mix_assistant,
  mastering_assistant, native_render_gate.
- Nuove regressioni: misure/picchi/LUFS/trim identici cambiando lingua;
  campioni normalizzati A/B identici; errori italiani preservano nomi originali;
  ritorno all'inglese; preferenze dei test isolate da quelle dell'utente.
- PASS MASTER UI **23/23**, inclusi titolo/controlli italiani, report QA in
  italiano, cambio lingua con worker attivo, annullamento/riprova, riproduzione
  A/B, ownership, progetti/Undo/PDC preservati.
- Prima prova UI generale/MIX PASS, export/Cancel **44/44**; screenshot ispezionati.
  Nell’ispezione visiva intercettata una chiave inglese contenente un trattino
  Unicode: conversione esplicita `String::fromUTF8` per le due chiavi non ASCII
  e per il separatore grafico, con regressioni su intestazione italiana e errore
  di frequenza fuori intervallo. Riesecuzione finale UI/MIX e **44/44** export/Cancel
  PASS; nessun FAIL nei report. Screenshot italiano di entrambi ispezionato.
- PASS anche apertura iniziale in italiano e successivo ritorno all’inglese:
  i binding conservano chiavi inglesi canoniche, non il testo già tradotto.
- Durante lo sviluppo il nuovo controllo UI ha intercettato caselle vuote:
  `bindItems` aggiorna voci già esistenti, senza crearle. Ripristinata la
  popolazione iniziale e mantenuto il binding; nessuna build difettosa pubblicata.
- Nessun benchmark generale ripetuto: cambiamenti UI/test/testo, analisi numerica
  e campioni identici, nessuna modifica della memoria/latency del grafo audio.

## Distribuzione e limiti

La preview Windows nativa **f98b706** verificata in Actions 43184992493
**non contiene questa correzione successiva**. Nuova build/test Windows e macOS
NOT RUN per la localizzazione. Nessun push/tag/release o nuova fase.
La directory di lavoro e il commit finale sono verificati dopo i test.
Rilascio pubblico sempre **NO-GO**; invariati i gate di ascolto/hardware,
plugin commerciali, macOS/AU e approvazione funzionale/qualitativa dei moduli vocali.

L'utente ha nel frattempo confermato che Annulla funziona durante il rendering
sul proprio Windows: il caso effettivamente provato è accettato. Questa conferma
non estende automaticamente il collaudo manuale a tutti i plugin/scenari.
