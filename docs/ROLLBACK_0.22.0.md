# Arranger 0.22.0: interventi e ritorno alla versione precedente

Baseline: `c4b18b02b4981ab6345f1a4d526ce1f576657306`, release 0.21.2.
Questa versione modifica soltanto Arranger. Il modulo comune e gli altri plugin
restano alle revisioni già pubblicate.

STUDIO aggiunge campionatore con forma d’onda, loop, inviluppo, filtro, slicing,
preset e sincronizzazione di tempo/altezza; registrazione MIDI con conteggio,
sustain, quantizzazione e Annulla; due gruppi con mandate a riverbero e delay;
raccolta portatile e ricollegamento dei campioni. Guide IT/EN e immagini sono
aggiornate insieme al codice. Le licenze Signalsmith sono incluse nei pacchetti. La pubblicazione attende
i controlli del modulo comune, del core e di tutte le compilazioni native.

La registrazione scrive note e velocity, convertendo il sustain in durata. Non
registra audio, pitch bend o automazioni dei controller. Il campionatore accetta
campioni fino a 60 secondi; la sincronizzazione non è un editor di tracce audio.

I test coprono audio e MIDI reali, sustain durante il conteggio e cambio ingresso,
clip nelle zone vuote, loop, preset, percorsi spostati, raccolta annullata, code
lunghe, copia privata del motore e pannelli nativi. I risultati delle compilazioni
sono nel workflow associato al commit della release. La latenza di un dispositivo
fisico richiede una prova sull’hardware effettivo.

## Ripristinare l’app

1. Chiudere Arranger e conservare una copia dei progetti e dei campioni attuali.
2. Reinstallare il pacchetto 0.21.2 del proprio sistema dalla
   [release precedente](https://github.com/wsx79/fasegroovearranger/releases/tag/v0.21.2).
3. Aprire il progetto salvato prima dell’aggiornamento. Il formato 3 e i percorsi
   relativi introdotti dalla nuova versione richiedono la 0.22.0; per trasferire
   note nuove alla versione precedente, esportarle prima in MIDI.

Non cancellare le cartelle `.media`: possono essere usate da nuovi progetti o preset.
Gli strumenti esterni devono restare installati.

## Ripristinare il codice

Su una working tree pulita, aggiornare i tag e creare un ramo di ripristino:

```sh
git fetch origin --tags
git switch -c rollback-studio-0.22.0
git revert --no-edit v0.21.2..v0.22.0
```

Il revert conserva la cronologia. Ricompilare e ripetere i test dopo il ripristino.
Il report consegnato insieme all’intervento include anche la patch completa e
quella inversa, collaudate in una worktree separata contro la baseline esatta.
