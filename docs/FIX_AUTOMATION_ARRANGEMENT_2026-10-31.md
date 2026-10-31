# Automazioni visibili, ripristino plugin e nuovo arrangiamento

Modifiche locali del 31 ottobre 2026. Base: `v0.30.6`, commit `f4e2885e55bf016be95a2dbee0b9488565a237cf`. Nessun commit, push o release eseguito per questo intervento.

## Modifiche

- **Volume:** inspector, fader mixer, master e controllo volume della vista strumento seguono il guadagno effettivo (base × curva) durante Play. Visualizzazione fino a circa +12 dB quando necessario; il volume di base mantiene il proprio limite. Aggiornati anche i numeri dipinti sotto i fader.
- **Pan e parametri:** pan nelle viste della traccia/mixer, tono/riverbero/chorus/pump dei suoni guida e cutoff/risonanza in Studio seguono la posizione audio. I parametri VST/AU notificano gli editor del plugin quando cambia il valore automatizzato. L’editor del produttore resta responsabile della propria visualizzazione.
- **Interazione:** aggiornamenti senza notifiche ai callback della DAW, senza modificare MIDI, curve, stato del progetto o cronologia. Trascinamenti e campi numerici in modifica hanno precedenza. Le regolazioni manuali modificano la base della variazione effettuata; non scrivono automazione. Pan e parametri sono automazioni assolute: mentre sono attive, il suono continua a seguire la curva. Stop/bypass ripristinano la visualizzazione della base.
- **Ripristino plugin:** programma selezionato prima dello stato nativo, poi soli parametri esposti che differiscono. Caricare il programma dopo il blocco nativo poteva cancellare regolazioni private. Conservati ripristino inattivo, supporto ai vecchi stati Base64 e programmi salvati separatamente. Nessuna modifica al formato progetto.
- **NUOVO ARRANGIAMENTO:** percorso dedicato che non chiama i generatori MIDI. Riutilizza le parti delle sezioni corrispondenti, compresi silenzi, note, velocity, seed e sorgenti dei loop. Adatta la durata ripetendo/accorciando i loop; mantiene BPM, armonia, suoni, effetti e mix. Trasporta e adatta le curve delle sezioni e del master. Tracce indipendenti e MIDI manuale restano alle posizioni assolute. Blocchi e limiti di risorse impediscono l’operazione senza modifiche parziali. Un solo Annulla ripristina il progetto. GENERA BRANO conserva il suo ruolo di nuova generazione.
- **Documentazione:** guide IT/EN, tooltip nelle sei lingue e changelog non pubblicato aggiornati.

## Verifica

- Build Release Linux dell’app e dei test.
- Sette test CTest: core, verifica MIDI, MP3, progetto, updater, audio engine, sample browser. Nuove regressioni per riuso letterale dei MIDI, tracce indipendenti/manuali, suoni, automazioni e blocchi.
- Regressione di un plugin con programma separato e regolazione privata nel blocco nativo: programma, regolazione privata e parametro esposto recuperati insieme. Listener dell’editor notificato dall’automazione.
- Prove native UI: volume/pan/master, avanzamento del callback audio, curve, seek, loop, Stop, bypass, cambio selezione, inserimento numerico, regolazione manuale, assenza di modifiche al progetto durante la lettura. Comando NUOVO ARRANGIAMENTO e relativo Annulla.
- Rendering nativo: 13 stems attivi, allineati a 305809 frame/44100 Hz, controllo silenzio mute, MP3 e rigenerazione musicale distinta.
- Host VST3 nativo: vecchi progetti 1/2/3 e versione 4, più uscite, riapertura editor/preset, stato inattivo, percorso completo codifica/destinazione/export WAV. Verifica effetti seriali, bypass, stato, alias e identità delle automazioni.
- Famiglia UI/manuali, traduzioni, modulo comune e packaging verificati. I motori vendorizzati non sono stati modificati.

I log e le evidenze complete si trovano in `/workspace/reports/automation-controls-2026-10-31/`. I tentativi intermedi falliti sono conservati separatamente dai risultati finali.

## Limiti e prova sul sistema reale

Triton non è installato nell’ambiente di verifica. Il difetto nell’ordine del ripristino è coperto da una regressione dedicata e dal vero host VST3, ma il comportamento di quel prodotto va confermato su Windows: scegliere un preset, modificare più regolazioni riconoscibili, salvare una copia del progetto, chiudere, riaprire e confrontare le regolazioni prima e dopo Play. Dati già assenti da un vecchio salvataggio non sono ricostruibili.

I MIDI indipendenti restano fermi anche quando cambia la guida delle sezioni. Le automazioni legate alle sezioni vengono adattate al nuovo arrangiamento; verificare musicalmente le transizioni dopo il comando. I plugin possono aggiornare il proprio editor con frequenze differenti. Le verifiche Windows/macOS di questa nuova modifica restano da eseguire dopo l’autorizzazione alla pubblicazione.

## Ripristino

Prima di provare, conservare una copia dei progetti. Nell’app, NUOVO ARRANGIAMENTO si annulla con un solo Annulla durante la sessione.

Per il codice, la cartella delle evidenze contiene `changes.patch`, `rollback.patch`, hash SHA256 e manifest con base e albero revisionato. La patch inversa riguarda solo questo intervento. Dal repository, prima verificare:

```sh
git apply --check /workspace/reports/automation-controls-2026-10-31/rollback.patch
```

Solo se il controllo riesce e si intende annullare l’intervento:

```sh
git apply /workspace/reports/automation-controls-2026-10-31/rollback.patch
```

Se altri cambiamenti successivi impediscono il controllo, fermarsi e confrontare i file; non forzare il ripristino. Dopo un eventuale commit pubblicato, preferire il revert del commit dell’intervento con l’approvazione richiesta. L’archivio della release v0.30.6 resta il riferimento binario precedente.
