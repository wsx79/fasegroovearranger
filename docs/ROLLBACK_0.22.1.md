# Arranger 0.22.1: IA locale e ripristino

Baseline: release 0.22.0, commit `e4110f224b931b2cf37d0fc75493954a97233be9`.

Il compositore propone Ollama locale alla prima apertura dopo l’aggiornamento,
verifica il server e legge i modelli installati. Non chiede una chiave API per i
modelli locali. Mostra istruzioni per installazione, server spento e modelli
mancanti; accetta indirizzi senza schema e suffissi API comuni. Restano disponibili
i provider online con le proprie chiavi. Guide e privacy descrivono anche la
richiesta automatica dell’elenco modelli, che non contiene il contesto musicale.

Ollama e i suoi modelli non sono inclusi nell’installer: devono essere installati
sul computer dell’utente. Nessun servizio di inferenza gratuito viene promesso.
I test HTTP e della finestra usano risposte controllate per verificare il percorso
fino al MIDI; non misurano la qualità o la velocità di un vero modello locale.

Il formato dei progetti resta invariato rispetto alla 0.22.0. Per ripristinare
l’app, chiuderla e reinstallare il pacchetto 0.22.0 conservando progetti e campioni.
Le preferenze aggiornate sono compatibili; il modello locale rimane selezionato
finché non si cambia servizio. Le chiavi API non vengono salvate.

Per ripristinare il codice su una working tree pulita:

```sh
git fetch origin --tags
git switch -c rollback-ai-0.22.1
git revert --no-edit v0.22.0..v0.22.1
```

Ricompilare e ripetere i test. La patch inversa e gli esiti di verifica sono nel
report consegnato insieme alla release.
