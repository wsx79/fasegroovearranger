# Studio 0.22.4: ripristino

Baseline 0.22.3: `1be4b16705174552760b7c29e21e8677fb903bb4`.

Il formato dei progetti non cambia. Play/Stop/Ripeti ascolto sono transitori: non
salvano impostazioni, non scrivono MIDI e non generano comandi Annulla.
L’ascolto usa una voce privata con i parametri attuali, prima del mixer traccia.
La riproduzione normale e l’export mantengono il loro percorso audio.

Per tornare alla versione precedente reinstallare la 0.22.3, conservando progetti
e campioni. Per annullare il codice pubblicato: `git revert --no-edit v0.22.3..v0.22.4`.
Il report contiene patch diretta e inversa e le revisioni per il ripristino.
