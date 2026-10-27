# Audit 0.23.0: ripristino locale

Baseline pubblicata 0.22.7: `11307e9ea0c02dda6c7a0f9d3605e187ddd3ede8`.
Le modifiche sono locali; commit, push, tag e release richiedono il via libera
esplicito dell’utente. Nessun progetto personale viene modificato dalle prove.

Il report include patch diretta e inversa con sorgenti, prove, guide e immagini.
Verificare dalla radice del repository:
`git apply --check /percorso/rollback.patch`.
Conservare eventuali modifiche successive prima di applicare la patch inversa.
I progetti mantengono gli schemi 1–3; WAV registrati ed export rimangono sul disco.
Annullare l’importazione di una take rimuove la traccia, conservando il WAV.
La 0.22.7 non carica nel sampler file oltre 60 secondi: i WAV lunghi restano validi,
ma dopo il rollback vanno usati con una versione che supporta tali durate.
