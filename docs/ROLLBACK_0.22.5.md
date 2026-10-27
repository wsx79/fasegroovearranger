# Studio 0.22.5: ripristino

Baseline 0.22.4: `5cb5929c1c482429db84db26345ab2366a893de1`.

L’accordatore è un osservatore dell’audio del campionatore: non modifica audio,
MIDI, radice, trasposizione, fine o formato dei progetti. Le letture sono transitorie.

Per tornare indietro reinstallare la 0.22.4 conservando progetti e campioni.
Per annullare il codice pubblicato: `git revert --no-edit v0.22.4..v0.22.5`.
Il report include revisioni, patch diretta/inversa e verifica del ripristino.
