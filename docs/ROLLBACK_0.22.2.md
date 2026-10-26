# Studio 0.22.2: modifiche e ripristino

Baseline: release 0.22.1, commit `50cfee0459a93897691b1a4be644d3298b909fb3`.

Questa versione cambia l’interfaccia di Studio, lo slicing automatico e l’accesso
al campionatore dall’inspector. Il motore espone al pannello le voci attive per
mostrare le note effettivamente in riproduzione, comprese le code one-shot.
Restano disponibili le registrazioni sulle tracce non campionate: il filtro del
menu si applica alla sola scheda campionatore. Cambiare scheda non sposta una take.

Il reset riguarda soltanto il campionatore della traccia, conserva file, MIDI e
mix ed è annullabile. I tagli in parti uguali usano gli stessi marcatori già salvati
nei progetti: il formato progetto resta 3. Ora i campioni sulle tracce Drums
conservano i pitch MIDI anche fuori dalla modalità Tagli, così reset e
registrazione non li uniformano. La 0.22.1 può riaprire i progetti, ma potrebbe
uniformare questi pitch: conservare il progetto precedente ed esportare il MIDI
prima di un downgrade se si utilizzano campioni Drums suonati cromaticamente.

Per tornare all’app precedente, chiudere Arranger e reinstallare la 0.22.1,
conservando progetti e cartelle dei campioni. Per ripristinare il codice su una
working tree pulita:

```sh
git fetch origin --tags
git switch -c rollback-studio-0.22.2
git revert --no-edit v0.22.1..v0.22.2
```

Ricompilare e ripetere i controlli. Il report include revisioni esatte, test e
patch inversa collaudata. I test audio e MIDI usano i callback del motore; la
compatibilità delle tastiere MIDI fisiche richiede una prova sull’hardware.
