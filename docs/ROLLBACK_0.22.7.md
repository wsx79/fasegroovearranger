# Studio e mixer 0.22.7: ripristino locale

Baseline 0.22.6: `d721545f9969500ac4c3301994d47bfac7a080dd`.

Le modifiche sono locali: nessun commit, push, tag o release viene effettuato
senza il via libera dell’utente. L’ultima release pubblicata resta la 0.22.6.
Non cambiano motore audio, parametri, formato progetti o MIDI.
Il nuovo Source/StudioTheme.h riguarda soltanto Studio e Bus e mandate;
il tema globale e quello condiviso con i plugin restano invariati.

Il report contiene patch diretta e inversa, incluse documentazione e immagini.
Dalla radice del repository verificare con `git apply --check /percorso/rollback.patch`.
Applicare `git apply /percorso/rollback.patch` solo dopo aver conservato eventuali
modifiche successive sui file interessati. La patch inversa ripristina la baseline
e rimuove questo documento locale.
