# Riscontro audit 0.30.9 — 1 novembre 2026

Base verificata: v0.31.3, commit c5d2faf0affce2ec67ae12876b6e623c7093f6d8. Intervento della 0.31.4; commit, push e release autorizzati dall’utente.

| Punto del report | Riscontro sulla base attuale | Intervento |
| --- | --- | --- |
| 1. Stato e anteprima IA al cambio lingua | La ritraduzione del testo visualizzato era già stata rimossa; il messaggio rimaneva però nella vecchia lingua fino al successivo evento. | Stato conservato come chiave originale con prefisso numerico separato. Aiuto, errori con aiuto e report dell’app vengono rigenerati; prompt, chiave API e testo esterno restano conservati. Aggiunte le traduzioni mancanti dei messaggi coinvolti. |
| 2. Delay inutili caricando un campione | Già corretto: loadSample prepara le linee soltanto con latenza del sampler o plugin presenti. | Nessun’altra modifica al DSP. |
| 3. Delay inutili nella copia offline | Già corretto: offlineCopy valuta sync del sampler, strumenti plugin ed effetti presenti. | Nessun’altra modifica al DSP. |
| 4. Notifiche host sul thread audio | Già corretto: durante render viene impostato il valore e accodata la notifica; flushParameterNotifications la consegna dal timer UI. | Conservato il percorso esistente. |
| 5. Lingua persistita dal self-test | Confermato anche in altri percorsi di verifica UI. | I comandi nativi di self-test usano un’identità di preferenze separata, così anche un’interruzione non modifica le preferenze reali. Il modulo condiviso rimane identico alla revisione fissata. |

## File progetto Windows

L’installer registra il tipo FaseGrooveArranger.Project, l’icona del prodotto, Apri con e il comando con eseguibile e percorso progetto quotati. L’opzione di associazione è selezionata inizialmente. Un default di un’altra applicazione o una scelta esplicita di Windows restano rispettati. La disinstallazione rimuove il default solo se punta ancora ad Arranger, non cancella i progetti e non elimina l’associazione a un’altra app.

L’app gestisce già apertura iniziale e invio a un’istanza esistente. La verifica UI aggiunta apre realmente un progetto dal comando di associazione con spazi nel nome ed estensione .FGA, confrontando il MIDI salvato. Le guide IT/EN descrivono installazione, Apri con e differenza rispetto al pacchetto ZIP.

## Verifiche

Build riuscita, sette suite CTest passate (20,99 s), verifica UI nativa completa passata e controlli famiglia/traduzioni passati. Evidenze in `/workspace/reports/audit-0.30.9-followup`. La prova di lingua controlla IT → FR → DE, prefissi numerici, errori esterni nell’aiuto e conservazione di prompt/chiave di sessione. I controlli famiglia e traduzioni verificano anche che il modulo condiviso non sia stato alterato.

Il test PowerShell dell’installer è esteso a icona, comando quotato, installazione/upgrade, pulizia dell’associazione e conservazione di default esterni e progetti. L’ambiente corrente è Linux: il test viene eseguito dalla CI Windows prima della pubblicazione dei pacchetti. Gli esiti della release e i relativi log vengono conservati nelle evidenze della 0.31.4. Non si dichiara eseguita su Linux una prova di Esplora file Windows.

## Ripristino

La cartella delle evidenze contiene il manifest e le patch completa/inversa rispetto a c5d2faf. Prima di applicare l’inversa usare `git apply --check rollback.patch`, da un albero senza modifiche successive incompatibili. Dopo un eventuale commit, preferire `git revert` del solo commit dell’intervento, previa autorizzazione. Nessun progetto dell’utente è incluso nelle patch. La release 0.31.3 già pubblicata rimane disponibile.
