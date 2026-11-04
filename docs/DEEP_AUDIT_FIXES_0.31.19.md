# Fix del deep-audit 0.31.19

Base: `98e9d0b16ba400713267281488dcb9045161a63c` (0.31.19). Il branch citato nell’audit non era disponibile sul remoto; `main` conteneva lo stesso commit. Lavoro sul branch `fix/deep-audit-0.31.19`.

## Correzioni

| ID | Implementazione e regressione |
| --- | --- |
| C1 | `Handoff::adopt` migra i riferimenti prima di pubblicare lo stato ritirato. Il test forza una raccolta concorrente durante la migrazione e controlla la durata del vecchio oggetto; resta lo stress sul vero motore. Nessuna attesa aggiunta al percorso audio. |
| C2 | Stato MIDI desiderato a capacità fissa per processor/slot, con note dei 16 canali, sustain, controller, pitch wheel e channel pressure. Dopo lock o sospensione vengono rilasciate esplicitamente le voci e ricostruite le note premute o sostenute, prima degli eventi del nuovo blocco. Anche il percorso idle effettua il recupero. Test di Note On/Off e pedal release persi, canali, sospensione e idle; il test host include un VST3 caricato realmente. |
| C3 | Offset e durata sorgente in secondi per clip registrato. Le suddivisioni conservano l’audio corretto e le voci terminano al bordo. Seek e modifiche durante Play inseguono la regione giusta. Il messaggio interno è letto senza costruire messaggi MIDI allocati; gli insert ricevono normali note MIDI. Test con WAV a 220/880 Hz, confronto del segnale prima/dopo il taglio, spostamento, copia, sovrascrittura ed editing durante Play. |
| C4 | Limiti coerenti tra modello, caricamento, inserimento, spostamento e ridimensionamento UI: niente clamp a 512 beat. Il test inserisce un WAV completo di sei minuti a 120 BPM e verifica clip e nota di 720 beat. I limiti di memoria dei file audio restano quelli del sampler. |
| C5 | Feel effettivo persistente per take. Una rigenerazione aggiorna i potenziometri e i take selezionati; gli esclusi conservano i loro valori anche al successivo APPLICA. Le variazioni globali esplicite aggiornano il parametro scelto. I generatori standard, elettronici e Vocal chops usano lo stesso contratto. Le timeline indipendenti vengono suddivise prima di assegnare il feel; anche i crash di sezione rispettano lo scope e APPLICA. |
| C6 | IMPORT MIDI musicale usa il percorso dei cambi armonici, preserva i blocchi e crea un solo checkpoint. La regressione aziona il vero callback del selettore file e controlla MIDI, blocchi e Annulla/Ripristina. |
| O1 | Decisione dell’utente: conservare periodo e fase originali. La fase viene mantenuta nei frammenti, nelle estensioni e nel salvataggio. Test del loop di quattro beat tagliato al beat sei: successive note del frammento sinistro a 0, 4, 8; fase corretta anche nel frammento destro. |

## Compatibilità dei file

Lo schema 5 conserva feel, regioni audio, loop con fase e clip lunghe. Il caricamento degli schemi 1–4 rimane supportato. I progetti che usano i nuovi dati richiedono questa build o una successiva: le build precedenti rifiutano lo schema 5, evitando di eliminare silenziosamente dati necessari alla riproduzione.

I take continuano a usare il campione assegnato alla traccia. Gli offset non modificano il WAV originale. I generici clip sample/MIDI dei vecchi progetti non vengono riclassificati automaticamente come registrazioni: le take prive di metadati di regione richiedono una conversione esplicita, che questa patch non applica automaticamente. Il ripristino della durata già persa in progetti precedentemente salvati con clip troncate non può ricostruire automaticamente i dati di editing mancanti: il file audio originale conserva comunque la registrazione completa.

## Verifica

Le regressioni sono integrate nelle suite. Esecuzioni mirate:

```sh
build/native/fga_core_tests --deep-audit
build/native/fga_engine_tests_artefacts/Debug/fga_engine_tests --deep-audit
```

`--piano-tools-test <cartella>` esegue la prova mirata del callback IMPORT MIDI e degli strumenti musicali. `--ui-test` include la stessa prova; `--host-smoke-test` include la perdita di Note On/Off per un callback con VST3 reale. `scripts/verify-host.py` controlla anche il nuovo risultato.

Gli esiti finali sono registrati in [DEEP_AUDIT_VALIDATION_0.31.19.txt](DEEP_AUDIT_VALIDATION_0.31.19.txt). Il timeout del test audio è 300 secondi, per includere gli export completi con DSP Debug non ottimizzato. Ambiente Linux con JUCE 8.0.9, export offline e display virtuale. Core strumentato con ASan, UBSan e `_GLIBCXX_ASSERTIONS`; sorgenti e test del motore strumentati con ASan/UBSan, collegati ai moduli JUCE ordinari. I messaggi di assert JUCE Debug restano visibili nei log (anche ADSR/filter, contenitori e buffer audio); il successo delle suite non equivale all’assenza di assert. Hardware audio/MIDI reale, macOS/AU, Windows e plugin commerciali non verificati in questa sessione.
