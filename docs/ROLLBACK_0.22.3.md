# Studio 0.22.3: ripristino

Baseline 0.22.2: `4a67a255e3590ada0c3e5a340faa4aebadefa71f`.

Cambia soltanto l’interfaccia: meter senza simboli matematici e gesti di scorrimento
che non modificano i controlli. Il DSP, il formato progetto e i parametri già
salvati restano invariati. L’aggiornamento non ripristina automaticamente le
impostazioni del kick: usare Annulla o il reset del campionatore se appropriato.

Per tornare all’app precedente chiudere Arranger e reinstallare la 0.22.2,
conservando progetti e campioni. Per ripristinare il codice su una working tree
pulita:

```sh
git fetch origin --tags
git switch -c rollback-studio-0.22.3
git revert --no-edit v0.22.2..v0.22.3
```

Il report allegato contiene revisioni, verifiche e patch inversa collaudata.
