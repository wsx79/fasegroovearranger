# Arranger — correzioni audit 28 ottobre 2026

Base pubblicata: v0.23.5, commit `f04db0b8c3b3842cbc5c23c78bab173b10546493`.
Riferimento: `docs/bug da verificare.pdf`, audit del 27 ottobre, 20 rilievi.
Versione candidata: 0.23.7. Pubblicazione autorizzata dall’utente dopo la verifica locale. Esiti multipiattaforma disponibili nella CI del tag v0.23.7.

## Interventi e prove

| ID | Correzione | Verifica |
|---|---|---|
| C1 | COLLECT/RELINK cancellabili durante hash/copia; niente terminazione forzata, pulizia temporanei e rollback dei file creati. Il task viene distrutto soltanto dopo la sua uscita. | StudioIO: cancellazione durante copia, destinazione precedente conservata, niente media parziali. UI: cancellazione con trasporto disponibile e stato coerente anche se il commit era già terminato. |
| C2 | Worker AI ritirati in un proprietario che attende l'uscita cooperativa; callback con identità e SafePointer, watcher HTTP ricongiunto prima della distruzione. | EngineAudit: worker ritardato non distrutto prematuramente. Fixture HTTP/AI e cancellazione. |
| A1 | Il sampler possiede una copia delle impostazioni: non conserva più un puntatore al MixState ritirabile. | StudioEngine: modifica/liberazione del publisher, conservazione e riconfigurazione della copia. Audit audio. |
| A2 | Identità runtime stabile delle tracce; selezione Studio verificata a ogni modifica, anche con campo numerico focalizzato o chooser pendente. | UI: cancellazione della traccia precedente e della traccia attiva; nessuna scrittura alla traccia sbagliata né Undo spurio. |
| A3 | Autosave rimandato durante playback/registrazione/export; snapshot e scrittura in background quando inattivo. Gestione esplicita degli errori. | UI: nessuna scrittura durante playback; snapshot indipendente e dirty conservato dopo successiva modifica del progetto. |
| A4 | Studio non disabilita globalmente MainComponent. Vengono bloccate selettivamente le modifiche incompatibili; Stop e trasporto restano accessibili. | UI: registrazione, Stop principale, ripristino controlli e primo click dopo chiusura finestre. |
| M1 | Contatore MIDI Learn separato dall'indice circolare: il nono accordo non tronca la progressione. | UI: dieci acquisizioni, otto slot conservati. |
| M2 | Drop audio/MIDI sulla corsia sotto il puntatore; nessun seek involontario. Corsia vuota: clip creata al beat del drop. | Tre regressioni UI durante playback e su corsia vuota. |
| M3 | Annullare la chiusura o Salva con nome mantiene la registrazione in corso. Finalizzazione soltanto dopo la decisione effettiva. | UI: take e Undo conservati all'annullamento, take inclusa nel salvataggio accettato. |
| M4 | Chiusura, nuovo progetto, apertura e installazione aggiornamenti usano continuazioni asincrone; proseguono una volta sola dopo effettivo successo. | UI: annullamento, errore scrittura, primo salvataggio e navigazione senza dirty. |
| M5 | JSON limitato prima del parsing: 256 MiB, profondità 48, budget elementi. MIDI limitato a 64 MiB e budget residuo di 2 milioni di note. Caricamenti atomici; salvataggio respinge progetti oltre limite prima di sostituire il file. | IO: profondità, limiti, stringhe con delimitatori, BOM UTF-16, import oltre budget e destinazione conservata dopo salvataggio troppo grande. |
| M6 | Velocity crea Undo soltanto alla prima modifica reale. Gomma trascinabile, un checkpoint per gesto. | UI: click vuoto senza dirty, cancellazione multipla e ripristino con un Undo. |
| M7 | Percorsi relativi dei media confinati alla cartella del progetto/preset; rifiutato attraversamento con `..`, anche con separatori Windows. | IO: traversal e caricamento fallito senza alterare il progetto corrente. |
| M8 | Selezioni AI associate all'identità della traccia, risincronizzate prima della richiesta; lock correnti rispettati. | UI/IO: rimozione e reindicizzazione tracce, selezione bloccata o assente respinta. |
| B1 | Export delle numerose tracce manuali senza collisioni melodiche: secondo MIDI port quando necessario; export individuale su port 0. | IO: 32 tracce, 19 destinazioni melodiche port/canale distinte; percussioni GM condivise intenzionalmente. |
| B2 | Voicing Coach aggiorna la voce da applicare con le scelte A/B; pulsante esplicito APPLY A / APPLY B. | UI: scelta A dopo ascolto B e scelta B, etichetta e voicing corrispondenti. |
| B3 | Relink legacy per solo nome segnala contenuto non verificato; corrispondenze ambigue respinte. | StudioIO: nome unico segnalato e ambiguità senza falsa verifica. |
| B4 | Attesa callback audio con timeout e sblocco del mutex prima del join. Timeout CTest per impedire job indefiniti. | EngineAudit e CTest. |
| B5 | Asserzione esatta sulla cardinalità armonica; ordinamento verificato su tutti i 20.000 elementi senza soglia prestazionale dipendente dalla macchina. | Core e SampleBrowser. |
| B6 | CI Windows/macOS/Linux con rendering nativo, hosting VST e audit audio offline; dipendenze ffmpeg e verifica dei quattro motori alle revisioni fissate. | Wrapper eseguiti localmente Linux; verifica esatta dei quattro pin. Le esecuzioni remote Windows/macOS richiedono il push autorizzato. |

Il percorso del primo click introdotto in v0.23.5 resta coperto dalle regressioni delle finestre. Non è stato rimosso sulla sola base delle ipotesi del PDF.

## Compatibilità e limiti

Nessun cambio al formato del progetto: l'identità runtime non è serializzata. Guide IT/EN, changelog di preparazione e sei lingue aggiornati. Versione aggiornata a 0.23.7 per la pubblicazione autorizzata.

Le prove UI automatizzano le continuazioni di Salva con nome, non il selettore file nativo di ciascun sistema. Nessuna sessione fisica con driver Windows/macOS e nessuna esecuzione sanitizer viene dichiarata. Il limite dei percorsi relativi non è una sandbox per link simbolici; i percorsi assoluti restano supportati.

L'audit audio completo copre 13 generi, canzoni di circa 4–7 minuti, preview, MP3 decodificati indipendentemente e code audio. È stato eseguito prima dell'ultimo guard di salvataggio: audio e generatori non sono cambiati successivamente; IO e test nativi finali verificano anche quel guard.

## Ripristino

Per tornare all'app pubblicata, reinstallare v0.23.5 dopo aver conservato i progetti. Per tornare ai sorgenti precedenti seguire [ROLLBACK_0.23.7.md](ROLLBACK_0.23.7.md). Le patch complete del report locale includono anche i nuovi file, non soltanto quelli già tracciati. Non utilizzare reset o push forzati.

Per test e ripristino della versione pubblicata vedere [ROLLBACK_0.23.7.md](ROLLBACK_0.23.7.md). Le patch complete e i log locali sono conservati nel report di lavoro consegnato all’utente.

## Correzioni emerse dalla CI multipiattaforma

La candidata 0.23.6 non è stata rilasciata: Windows ha rilevato una cattura annidata non portabile a MSVC e macOS errori nei test nativi. Le catture sono state rese esplicite e la build Windows è successivamente passata. Un ulteriore test Windows ha isolato la barra di estensione note: JUCE notifica i due valori in modo asincrono. Il valore finale viene ora confermato prima della chiusura del gesto, evitando aggiornamenti tardivi e Undo aggiuntivi. Il test conserva i controlli sui pixel, sul MIDI e sui due Undo e verifica un rilascio immediato senza attendere notifiche.

I test macOS da terminale attivano esplicitamente l’app e attendono brevemente la consegna delle notifiche native di focus prima di iniettare il primo clic. Le verifiche di focus restano obbligatorie. I risultati finali della candidata 0.23.7 sono nella relativa esecuzione CI: non si dichiara un esito positivo prima del suo completamento.

Il test nativo Mac ha poi confermato un problema effettivo nel recupero del mouse: `getCurrentModifiersRealtime()` di JUCE su Cocoa aggiorna i modificatori della tastiera, ma conserva i pulsanti dell’ultimo evento mouse. La protezione dal rilascio spurio vedeva quindi ancora premuto un tasto il cui evento di rilascio era proprio quello perso. Il recupero legge ora i cinque pulsanti tramite CoreGraphics, mantenendo invariati i controlli sulla finestra proprietaria e sui gesti fisicamente ancora attivi. Il framework è collegato esplicitamente soltanto su Apple. Le verifiche native non sono state rimosse.

L’audit audio completo su ciascun sistema parte dopo il superamento dei controlli nativi rapidi di UI/render e hosting. Un errore in questi controlli continua a bloccare la release e ora rende disponibili i log senza attendere ulteriori rendering completi. I controlli audio restano obbligatori per ogni candidata pubblicabile.

La verifica del rilascio perso resta nel codice come prova opt-in (`FGA_RUN_NATIVE_INPUT=1`): un press sintetico consegnato al peer non viene attribuito in modo affidabile alla finestra, perché l'assegnazione dipende da quale peer possiede il puntatore e cambia con l'ordine delle finestre. La prova non blocca più la release; l'evidenza nativa dichiara lo skip e il resto della suite continua a girare. La take dello scenario Studio viene inoltre fermata subito dopo il controllo: durante una take la generazione è disabilitata per progetto, quindi il primo clic non può essere richiesto finché la take resta attiva.
