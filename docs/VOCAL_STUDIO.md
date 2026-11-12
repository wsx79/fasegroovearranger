# Vocal Studio — conservato, disattivato

Per decisione dell'utente, il backend eCantorix/eSpeak è stato rimosso da Vocaltest.
Vocal Studio non compare nei menu o nella toolbar, non ha scorciatoie di apertura
e non può aprirsi o avviare rendering automaticamente. Etichette Vocal e sillabe
sovrapposte alle note non vengono mostrate, anche nei progetti vecchi. Non installare gli strumenti
del vecchio backend: non vengono più cercati, caricati o eseguiti.

## Codice conservato

Restano il modello `VocalClip`, sillabe/associazioni alle note, regioni e policy
intro/outro, persistenza, editor collegato al piano roll, preparazione MIDI,
scaffolding asincrono e importazione/export tramite le strutture audio esistenti.
L'editor continua a compilare ma il suo ingresso è chiuso da una costante interna,
non da una preferenza attivabile dall'utente. La funzione di rendering rimasta
restituisce un errore senza I/O, callback, worker o processi esterni. Un futuro
backend richiederà implementazione, verifiche e autorizzazione di riattivazione.

Sono rimossi launcher/process-tree handling, sintesi/postprocessing/cache del
backend, helper di preparazione e dipendenze di test/installazione esclusive.
Non è introdotto un motore sostitutivo. I vecchi report 01C/licenze sono storici.

## Compatibilità

Il formato dei progetti e `ProjectIO.cpp` non cambiano. I vecchi identificativi
di motore/voce e il percorso helper restano dati inerti per il round trip;
nessuna migrazione o cancellazione di testi, note, sillabe o riferimenti.
I WAV già generati e le cache dell'utente **non vengono cancellati**: possono
essere ancora referenziati dai progetti e continuano a usare il normale playback.
Vocal Chops, generatore audio preesistente, AI Composer, Lead MIDI, mixer,
routing e AudioTake non sono modificati.

## Verifica essenziale

Risultati correnti e commit in `02_STATUS.md`. Il CTest `vocal_studio_dormant`
verifica preparazione MIDI e render fail-closed; `project_io` comprende il round
trip dei metadati del motore rimosso. `--vocal-studio-test` è esclusivamente un
autotest: verifica il gate chiuso e l'editor riutilizzabile **fuori schermo**, senza
aprire Vocal Studio. Le ricette preview verificano che helper/runtime rimossi non
rientrino nel pacchetto. Questi test non sono un'approvazione funzionale vocale.

Verifica locale Linux conclusa: build Release app/engine/IO **PASS**; CTest mirati
**3/3 PASS** (`project_io`, `vocal_chops`, `vocal_studio_dormant`, 10,83 s);
autotest editor fuori schermo/gate chiuso **18/18**, import campioni invariato
**11/11**, due suite packaging **PASS** (fixture sintetiche, non binari Windows).
Audit mirato: nessun chiamante UI di `showVocalStudio`, nessuna variabile del
backend letta, nessun launcher/helper rimasto. Il test UI è eseguito anche con la
vecchia variabile helper impostata: resta ignorata. Un errore intermedio del
parser Python rifiutava la riga informativa dei tempi dell'import; corretto e
ricetta ripetuta PASS senza modificare l'import. Log in
`/workspace/scratch/vocal-retirement-*`. Windows/macOS/hardware non verificati
in questa modifica; nessuna dichiarazione di approvazione o nuova release.
