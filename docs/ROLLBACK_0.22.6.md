# Mixer 0.22.6: ripristino

Baseline 0.22.5: `8546c3bec7ac7f46bf6e7ab84defc4298f84f015`.

Il progetto continua a usare gli stessi gruppi e le stesse due mandate; non cambia
lo schema e non serve convertire i file dell’utente. Per il downgrade reinstallare
la 0.22.5 conservando progetti e campioni.

Per annullare il codice pubblicato: `git revert --no-edit v0.22.5..v0.22.6`.
Il report finale contiene revisione, patch diretta e inversa, compresi nuovi file
e immagini. Prima di applicare una patch al codice locale conservarne eventuali
modifiche successive e verificare con `git apply --check /percorso/rollback.patch`.
