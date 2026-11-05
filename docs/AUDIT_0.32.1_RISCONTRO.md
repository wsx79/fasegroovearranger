# Riscontro audit 0.32.1 — correzioni 0.32.2

Base verificata: `4d1c38b634ba0b10e752b967f0096b3950a188a1`.
Il documento ricevuto era una revisione statica senza esecuzione delle suite.
I risultati sotto distinguono i problemi riprodotti, gli interventi difensivi e
le indicazioni che richiedono una precisazione. Esiti in `VALIDAZIONE_0.32.2.txt`.

| Voce | Riscontro e correzione |
| --- | --- |
| 1 | Confermato. Il completamento dell'export ritira `exportTask` tramite `RetiredJobs`. La prova VST esegue ora export → freeze → unfreeze. |
| 2 | Confermato. `resolveSectionHarmony` è condiviso dai generatori, dal posizionamento delle frasi e dal pedal CC. Chitarra e lead usano accordi, tonalità, scala e periodo effettivi. Verifica anche con una sezione che inizia fuori dal periodo armonico. |
| 3 | Confermato. Le regioni usano la durata effettiva della clip anche con `span == 0`. Resize e riarrangiamento conservano un singolo trigger audio, senza trasformarlo in una serie di Note On. |
| 4 | Confermato. Lo split valida i bordi dopo il detach. Il detach rende esplicita la durata prima di tagliare una regione sovrapposta. Un taglio nella parte nascosta viene rifiutato senza modificare il progetto. |
| 5 | Confermato. Delete lavora su una copia e commette soltanto dopo un detach riuscito. |
| 6 | Confermato. Nuovo brano IA sostituisce soltanto le tracce abilitate; le altre vengono conservate a posizioni assolute, con audio, automazioni e contenuto delle clip sovrapposte, anche quando non udibile. La durata richiesta governa la nuova forma: i take esclusi più lunghi restano integri e possono prolungare il brano complessivo. |
| 7 | Confermato. Il rilascio delle voci precede l'accodamento del panic. La prova con processor MIDI verifica che spariscano anche le note dal vivo su altri canali. |
| 8 | Confermato. Ogni seek/loop invalida il contenuto utile del PDC in tempo costante. Si leggono solo campioni scritti dopo il salto; non serve azzerare il ring nel callback. La memoria viene preparata sul thread di controllo, solo per delay positivi e con capacità proporzionata al ritardo; le linee a latenza zero vengono liberate. |
| 9 | Confermato. Il default di lettura di `randomAutomation` è falso. Save normalizza e valida una copia prima della scrittura atomica. La prova riapre un progetto con valori non finiti iniziali. |
| 10 | Il distruttore di MainComponent esisteva già in `UiAuditTests.cpp`, ma il rischio del popover era reale. Owner ora protetto da SafePointer, dismissione esplicita, rilascio audition e controlli sugli indici. La prova invalida la selezione e poi scollega l'owner. |
| 11 | Difetto latente confermato nel controllo del ritorno. Un avvio fallito del thread ripristina scanning/current. La normale scansione e la cancellazione sono verificate; non viene simulato l'esaurimento dei thread del sistema operativo. |
| 12 | Confermato. I campioni non finiti dei plugin vengono sanificati prima del DSP successivo e del meter. Il meter ha ulteriori controlli su energia/livello e limita il valore prima della conversione intera. |
| 13 | Confermato. Insert/duplicate, split e applicazione IA rispettano il budget di 2 milioni di note, contando anche loopSource. I rifiuti conservano il progetto originale. |

## Voci da verificare

- MIDI non conforme: i data byte dei messaggi di canale vengono mascherati prima dell'indicizzazione. Regressione con nota/velocity `0xff`.
- Browser plugin: la scelta viene risolta tramite identità stabile (`row.key`), non tramite un indice diventato obsoleto dopo un rescan. Una voce rimossa richiede una nuova selezione.
- IA di sezione: un fallimento di `placeClip` produce un errore esplicito, senza assegnare un risultato parziale. La cancellazione viene propagata anche alla compilazione IA.
- JSON: conversioni di array, seed e versione limitate prima del cast; una versione enorme viene rifiutata senza modificare la destinazione.
- Hash plugin: cancellazione controllata durante la lettura e la canonicalizzazione. Un digest parziale non viene memorizzato; prova con interruzione e successivo hash completo.

## Notifiche dei parametri VST

La coda verso il message thread era già presente nel rendering normale. La
verifica dei chiamanti ha però trovato il percorso `flushIdle`, eseguito dal
callback a trasporto fermo, fuori da quella protezione. Ora usa la stessa coda.
Il test verifica che il valore DSP cambi subito, che i listener non vengano
notificati nel callback e che la notifica arrivi con il flush sul message thread.

## Limiti e suggerimenti di progetto

I cambi dinamici di latenza che richiedono una linea più grande segnalano la
preparazione al thread di controllo; durante l'attesa resta il bypass sicuro
senza allocazioni nel callback. Il limite massimo dichiarato del PDC non cambia.

Non vengono introdotte nuove grammatiche armoniche, modulazioni o un diverso
contratto di time-scaling delle curve MIDI: sono proposte evolutive, distinte dai
fix verificati. Formato e compatibilità dei progetti restano invariati.
