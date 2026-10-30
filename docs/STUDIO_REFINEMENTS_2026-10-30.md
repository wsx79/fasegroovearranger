# Studio e tracce vuote — 30 ottobre 2026

Intervento aggiuntivo alla preview FX, sulla base dell’utente 0.29.0 (`89e67e3`). Incluso nella pubblicazione 0.30.1 autorizzata dall’utente.

- Inviluppo nascosto inizialmente: il pulsante INVILUPPO AMPIEZZA sulla waveform mostra/nasconde curva, maniglie e sviluppo del volume. Non modifica l’inviluppo audio o la cronologia Undo.
- Selezionare Transienti o 4/8/16/32/64 parti applica subito i tagli. Taglio automatico resta disponibile per ripetere l’analisi.
- Le barre e i cartellini numerati si trascinano. I limiti dei vicini impediscono sovrapposizioni e cambi involontari dell’ordine; un gesto corrisponde a un solo Undo. Restano zoom, aggancio allo zero, aggiunta/rimozione manuale e mappatura delle voci.
- Inverti rispecchia anche la waveform: regione attiva in Classic/One-shot, singola fetta in Slices, come fa il renderer audio. Nessuna modifica distruttiva al file.
- Eliminati scritta e matita nelle celle vuote dell’arrangiamento. Il doppio clic continua a creare clip.
- Guide IT/EN e tooltip aggiornati. Protetto il clic sui marcatori se la traccia del campione è stata rimossa.

Verifiche Linux: compilazione Release, test nativi completi UI/audio/MP3, drag e singolo Undo dei marcatori, selezione immediata, inversione di regione/slice e controlli inviluppo superati. Anche il fit con doppio clic, zoom e conservazione dei dati restano verificati. Tema/font e dizionari di famiglia controllati. Gli esiti Windows/macOS della versione pubblicata sono nella CI della release.

## Ripristino

`reverse.patch` rimuove soltanto questo intervento Studio e mantiene la precedente implementazione FX. Verificata ricostruzione esatta del suo albero Git. Conservare modifiche successive, eseguire `git apply --check reverse.patch` e poi applicarla solo se il controllo riesce. `combined-reverse.patch` rimuove anche FX e torna al commit utente 0.29.0; `combined-forward.patch` ricostruisce l’intera preview partendo da quel commit. Nessun reset forzato. I progetti con il nuovo stato VST/FX restano in formato 4: mantenere una copia precedente per riaprire con l’app ufficiale 0.29.0.

Per annullare l’intera release pubblicata usare, su checkout pulito, `git revert --no-commit v0.29.0..v0.30.1` e verificare il risultato prima di un eventuale commit. Le patch incrementali citate sono nel report locale `/workspace/reports/studio-refine-2026-10-30/`.
