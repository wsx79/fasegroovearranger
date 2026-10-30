# Vocal chops — intervento del 30 ottobre 2026

Base: release **0.27.1**, commit `4032051`. Versione 0.28.0: commit, push e pubblicazione autorizzati dall’utente dopo le verifiche locali. Gli esiti multipiattaforma sono nella CI della release.

## Utilizzo

Dal pulsante **+ MIDI** scegliere **VOCAL CHOPS**. È una parte opzionale separata dal Lead, con poche note sincopate e pause, adatta alle voci tagliate nella Jackin’ House. Prima di caricare la voce si sente il suono guida. **LOAD SAMPLE** prepara automaticamente le slice; **STUDIO** permette di rifinirle, tagliarle diversamente e regolare intonazione e inviluppo.

**DENSITY** regola il numero di note (circa due ogni quattro battute al valore iniziale), **GATE** la loro durata MIDI e **HUMANIZE** le piccole variazioni di tempo. In modalità Slices la lunghezza audio si modifica in Studio. Il dado e i comandi Rigenera cambiano la frase. Intro, break e outro lasciano più spazio. Si possono rinominare, modificare e cancellare le tracce vocali; Undo e blocchi funzionano come sulle altre parti.

La traccia usa uno slot MIDI aggiuntivo identificato dal campo opzionale `vocalChops`. Gli ID delle 18 parti esistenti, il Lead e le tracce MIDI manuali non cambiano. I progetti precedenti restano manuali dove previsto. La generazione del brano include le sole tracce vocali aggiunte, senza modificare il MIDI scritto a mano.

Cambiare numero o primo tasto delle slice rimappa il MIDI sbloccato, compresa la sorgente dei loop, conservando tempi, durate e velocity. Anche sostituzione della voce, preset e reset restano associati a tasti validi. I blocchi proteggono le note: sbloccare o correggere manualmente una parte congelata se si cambia la sua mappatura. Il contesto IA distingue la parte vocale e fornisce l’intervallo effettivo delle slice; note fuori mappatura vengono rifiutate senza alterare il progetto.

## Problemi corretti durante l’integrazione

| Percorso | Correzione |
|---|---|
| Rigenerazione di una sezione attraversata da clip indipendenti | `splitClip` sostituisce il progetto. La generazione ora riacquisisce la traccia dopo ogni taglio, senza conservare riferimenti invalidati. Coperto sia il percorso esistente sia quello vocale. |
| Menu destro → Rigenera traccia | Eliminato il checkpoint Undo duplicato: il comando delega al gestore di rigenerazione, che crea già il proprio checkpoint. |
| Studio → AUTO SLICE | Risolta l’identità della traccia prima di recuperare il suo campione, evitando l’asset di un indice precedente dopo cancellazione/compattazione delle tracce. |
| Test di hosting VST | La finestra viene già centrata asincronamente dall’app. Il test attende quella callback per massimo 500 ms, continuando a richiedere centramento, focus e riapertura corretti. Nessun comportamento del VST modificato per questo test. |

## Verifiche eseguite

- Compilazione Release Linux di applicazione e target test: riuscita.
- CTest: **7/7 PASS**; aggiunti casi per densità/monofonia, rigenerazione deterministica e variazioni, scope, blocchi, tasti slice, loop source, compattazione e persistenza del ruolo.
- AddressSanitizer con controllo perdite: tutti i test del core superati, inclusi i due tagli ai confini di sezione e la conservazione del MIDI esterno alla sezione.
- Test UI completo: aggiunta Vocal chops, controlli, rigenerazione/Undo, caricamento voce, mappatura, tooltip, disposizione e recupero dopo cancellazione superati. Export della parte campionata verificato udibile.
- Rendering e verifica audio indipendente: 13 stem allineati, mix muto silenzioso, MP3 e differenza udibile tra rigenerazioni superati.
- Hosting VST3 reale: nota MIDI, rapporto gain, ripristino stato, esportazione privata e riapertura editor superati.
- Test IO/IA aggiornati: round trip, progetti senza ruolo vocale, contesto dei tasti, rifiuto atomico di note fuori mappatura e accettazione di quelle valide superati.
- Guide IT/EN, tema/modulo di famiglia, 1761 chiavi prodotto in sei lingue e packaging verificati.

Le verifiche locali sopra riportate riguardano Linux; gli esiti Windows/macOS sono nella CI di pubblicazione della versione 0.28.0. Non viene dichiarata un’inferenza IA con un modello reale; i controlli IA sono locali sul contesto e sul compilatore MIDI.

## Ripristino

Le patch complete e inversa sono nel report di lavoro `/workspace/reports/vocal-chops-2026-10-30/`, con manifest e checksum. Prima di applicare `reverse.patch` conservare eventuali modifiche successive e usare `git apply --check`: in caso di errore fermarsi senza forzare. Le patch includono il nuovo generatore e questo report e sono verificate con ricostruzione esatta degli alberi Git su indice temporaneo.

Per ripristinare i sorgenti dopo la pubblicazione, da un checkout pulito preparare `git revert --no-commit v0.28.0`, verificare il diff e ricompilare prima di creare un commit di ripristino. Non riscrivere la cronologia. Per l’app pubblicata precedente conservare i progetti e reinstallare v0.27.1. Anche un eventuale commit/push/release di ripristino richiede autorizzazione.
