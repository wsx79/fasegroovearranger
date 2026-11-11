# Fase 07 — Mix Assistant: verifica preliminare

## Stato effettivo

Preflight eseguito sul solo branch Vocaltest da `9c4e8129daba8d7e415c11e707dc83608da15647`,
working tree inizialmente pulita. Nessuna funzionalità della fase implementata.
Il documento effettivo è `phases/07_MIX_ASSISTANT.md`: l'intestazione interna
riporta «PHASE 06», ma il file e l'indice ufficiale lo assegnano alla fase 07.
Non è una richiesta di estendere nuovamente il sidechain.

La richiesta di avvio prescrive prima la verifica del fix Cancel e l'assenza di
problemi bloccanti aperti. Il fix è presente e verificato tecnicamente su Linux:
Release, CTest 18/18, export UI 43/43 interno e 43/43 con VST3 reale, effetti
VST3 e Freeze/Unfreeze. Sono risultati storici del fix, non test ripetuti oggi.
Il report `docs/EXPORT_CANCEL_FIX.it.md` lascia esplicitamente aperto il gate
Windows/manuale. Non è possibile certificare la risoluzione sul Windows segnalato
con quei soli risultati Linux. Nessun nuovo bug è stato riprodotto nel preflight.

Il workflow Windows esiste, ma non può compilare il commit locale non pubblicato.
Il divieto corrente di push è rispettato: nessun avvio della vecchia build come
surrogato della verifica del fix, nessuna pubblicazione remota del codice.

## Inventario mirato e lacune

| Requisito della fase | Esistente da riusare | Lavoro non ancora implementato |
|---|---|---|
| Headroom/clipping/loudness | Meter engine, `LoudnessMeter`, pipeline export | Analisi contestualizzata e suggerimenti |
| Analisi spettrale | `EqAnalyzer` FFT 4096, trace 128 punti; cattura pre/post nell'engine | Mascheramento e confronto kick/bass |
| Stereo | Meter dei due canali e buffer di cattura | Misura di correlazione e interpretazione prudente |
| Interventi mixer/EQ | `EqSettings`, editor EQ, mixer corrente | Proposte individuali con spiegazione e approvazione |
| A/B e Undo | `checkpointMix`, `mixChanged`, snapshot progetto | Sessione di anteprima reversibile e Undo verificato |
| Lavoro fuori callback | `offlineCopy`, export e worker cooperativi esistenti | Orchestrazione analisi locale con annullamento |

Non è emerso un Mix Assistant già integrato nelle ricerche mirate. Non duplicare
EQ, mixer o host; non usare la trace grafica smussata come prova definitiva di
mascheramento. Un eventuale clone plugin può fallire: `offlineCopy` documenta
questo limite, quindi la strategia di analisi deve gestirlo senza cambiare
silenziosamente suoni o preset.

Accettazione prevista dal documento: misure locali deterministiche, nessun
upload/AI remota automatica, suggerimenti etichettati come tali, approvazione di
ogni modifica, anteprima A/B e Undo, analisi pesante fuori callback e fixture
comprendenti voci già renderizzate. Quest'ultimo punto non richiede di modificare
i due moduli vocali congelati. Progetti legacy/formato 7 e motori preservati.

## Gate e prossima decisione

Prima dell'implementazione serve chiarire il prerequisito aperto: completare la
verifica Windows del fix (richiede renderlo disponibile al runner o un altro
ambiente Windows autorizzato), oppure approvare esplicitamente l'avanzamento
fase 07 con quel gate ancora pendente. Nessuna nuova architettura è stata scelta
né implementata; non è richiesta una decisione architetturale del prodotto ora.

Test nuovi Release/regressioni/CPU/memoria/allocazioni: NOT RUN, nessun codice
modificato. Windows/macOS/manuale del fix restano NOT RUN, non PASS impliciti.
Nessuna variazione di DSP, routing, progetti, CPU/memoria o thread audio introdotta.
Nessun push, merge, tag o release; fase 08 non avviata.
