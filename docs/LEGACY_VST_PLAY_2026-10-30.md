# Vecchi progetti con strumenti VST — 30 ottobre 2026

Base: `c4dc1d4d9cd18ac1d48db7d253b01bb3b4709bee` (v0.30.1). Pubblicazione 0.30.2 autorizzata dall’utente: commit, push e release.

## Riscontro e modifica

Segnalazione: Windows, KORG M1, crash al primo Play dopo l’apertura di un vecchio progetto. Il log allegato riporta `memmove` e frame `GetPluginFactory`. Non contiene modulo e indirizzo dell’eccezione sufficienti a identificare con certezza il punto nel codice del plugin.

Nella 0.30 la configurazione introdotta per gli effetti disattivava anche le uscite secondarie degli strumenti. La correzione ripristina per gli strumenti il comportamento precedente alla 0.30; gli effetti mantengono il routing sul bus principale. L’arranger continua a usare la prima coppia di uscite dello strumento, allocando anche quelle secondarie. Non modifica MIDI, preset o dati dei progetti.

Il nuovo test nativo apre progetti in formato 1/2/3 con tre istanze VST3 multiuscita, stato nativo nel formato precedente e automazione dello strumento per indice. Usa l’apertura dell’app e il pulsante Play, guida la callback audio e controlla uscite, avanzamento, nuove note e segnale. Salva poi in formato 4 e ripete apertura e Play. Il controllo entra nella procedura di ogni release su Windows, macOS e Linux; prima il test degli strumenti era limitato alla suite estesa.

## Verifiche locali

- Test sulla configurazione 0.30.1, prima del fix: **FAIL**, perdita delle uscite secondarie dopo il caricamento.
- Stesso controllo dopo il fix: **PASS**. Apertura e Play dei formati 1/2/3 e del progetto aggiornato in formato 4 superati con tutte e tre le istanze.
- VST3 reale: MIDI, editor, rapporto di guadagno 3, stato ripristinato e clone per export 1:1: **PASS**.
- CTest: **7/7 PASS**. Effetti VST3 reali, catena, bypass, automazioni, stato ed export: **PASS**.
- Tema, font, guide, modulo condiviso, traduzioni e diff controllati.

Queste prove sono Linux con un plugin di test multiuscita. KORG M1 su Windows non è disponibile nell’ambiente: la regressione dei canali è riprodotta e corretta, ma la risoluzione dello specifico crash segnalato richiede una prova con quel plugin. Gli esiti dei binari Windows/macOS/Linux sono nella CI della release 0.30.2; la prova specifica con M1 sul Windows dell’utente resta necessaria.

## Ripristino

Log prima/dopo, manifest e patch sono nel report locale `/workspace/reports/legacy-vst-play-2026-10-30/`. `forward.patch` applica questa correzione alla base 0.30.1; `reverse.patch` riporta esattamente a quella base. Conservare prima eventuali modifiche successive, eseguire `git apply --check reverse.patch` e applicarla soltanto se il controllo riesce. Nessun reset forzato o riscrittura di commit/tag. Il ripristino rimuove anche test e documento aggiunti da questo intervento.

Dopo la pubblicazione, su checkout pulito, `git revert --no-commit v0.30.2` prepara il ripristino dei sorgenti alla 0.30.1. Controllare il diff e ricompilare prima di autorizzare commit e push del ripristino.
