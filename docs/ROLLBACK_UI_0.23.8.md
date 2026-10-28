# Rollback del restyling UI (0.23.8)

Il restyling è **un solo commit**, separato da armonia/arrangiamento (che non è ancora stato toccato):

    commit 91fbd96  Restyle the arranger: legible captions, one primary action, fitted instrument panels

## Annullare tutto il restyling

    git revert 91fbd96

Riporta indietro solo questi sette file, senza toccare logica musicale, build o release:

    Source/ArrangerTranslations.inc
    Source/DawTheme.h
    Source/DawWorkspace.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/Translations.inc
    Source/UiText.cpp

## Annullare solo una parte

    # solo il tema (contrasti, dimensioni, scrollbar)
    git checkout 91fbd96~1 -- Source/DawTheme.h

    # solo la barra dei comandi (GENERATE split, AI Composer, caret)
    git checkout 91fbd96~1 -- Source/MainComponent.cpp Source/MainComponent.h

    # solo il mixer
    git checkout 91fbd96~1 -- Source/DawWorkspace.cpp

    # solo le etichette accorciate
    git checkout 91fbd96~1 -- Source/Translations.inc Source/ArrangerTranslations.inc

`git checkout 91fbd96~1 -- <file>` ripristina la versione precedente del singolo file; dopo, `git checkout -- <file>` annulla il ripristino.

## Cosa resta fuori

- **Armonia e arrangiamento**: nessun file musicale è incluso in `91fbd96`; le modifiche previste sono in `docs/BACKLOG_ARMONIA_0.23.8.md` e saranno commit separati con prefisso `harmony:`.
- **Test e CI**: commit `97973df`, `2e7df25`, `c20c521`, `9d11d08`, `1df7fcc`, già nella release 0.23.7.
- **Report di audit** (`docs/report/`, `scripts/report/`): non tracciati, non fanno parte del restyling.

## Verifica dopo il revert

1. `cmake --build build --config Release --target FaseGrooveArranger`
2. `--smoke-test`: `interface.png` e `window-focus.txt` presenti, nessun `FAIL` nei file di evidenza di Windows/Linux.
3. Controllo visivo: barre, mixer, piano roll, pannello strumento tornano alla versione 0.23.7.
