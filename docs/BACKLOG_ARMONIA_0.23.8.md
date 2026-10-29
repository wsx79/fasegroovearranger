# Armonia e arrangiamento — stato e piano

Aggiornato: 29 ottobre 2026.

## Fatto

| Voce | Dove | Commit |
| --- | --- | --- |
| Sezione nuova generata subito (non più muta) | `Source/Menus.cpp` | `5a2ab80` |
| NEW CHORDS conserva lunghezza e accordi scelti a mano, pesca dal vocabolario del genere | `Source/core/Project.cpp` | `5d7975d` |
| Forma allineata al phrase (ciclo armonico + groove) | `Source/core/Project.cpp` | `5d7975d` |
| Fill armonico sull'ultima battuta di ogni sezione | `Source/core/Project.cpp` | `5d7975d` |
| Accordo tenuto rilasciato al cambio successivo | `fasegroovepiano` `aece955` (motore condiviso, ri-pinnato) | `5538e4c` |
| Crash sul downbeat della sezione che segue un fill, con test di riproducibilità | `Source/core/Project.cpp`, `tests/RegenerationChecks.h` | `cd0ff7e` |

## Provato e annullato

- **Accordo unico piano/chitarra/lead**: palette estesa (6/9, Sus, 11ths, 13ths, 7#9) nei due motori vendored e mappatura unica in `Project.cpp`; compilava e i test passavano, ma all'ascolto il risultato non è piaciuto. Annullato senza toccare i repo dei motori: si riparte solo se si decide una direzione diversa.
- **Fold dell'ottava del lead**: il fold a 96 viola il contratto coperto dal test `arranger_core`, "global lead octave = +12 esatto su ogni nota" (`tests/CoreTests.cpp:520`). Revert upstream in `fasegroovelead` `31af807`, pin invariato. Va ripreso solo dopo aver deciso se il contratto cambia; in alternativa si limita la somma ottava globale + ottava lane nell'arranger (Source), senza toccare il motore.

## Da fare, in quest'ordine

1. **Stessa qualità di accordo per piano, chitarra e lead.**
   Oggi il piano usa `sgk::buildChord` (11 estensioni) mentre chitarra e lead ricevono `std::min(extension,2)` su un enum di tre soli valori: con sus, dominante alterata, 11ths/13ths il piano e la chitarra suonano accordi diversi (nona minore contro nona naturale).
   Progetto: estendere `ChordColour` in `fasegrooveguitar` e `fasegroovelead` con `sixNine`, `sus`, `elevenths`, `thirteenths`, `sharpNine` (i `ChordType` esistono già tranne `7#9`), mappare in `Project.cpp` l'indice di estensione al colore corrispondente, aggiornare `chordColourName`/`numChordColours`. Va fatto upstream nei due motori, con i loro test, poi ri-pin.
   Rischio: alto (cambia il suono di ogni brano) — richiede ascolto A/B.

2. **Registri separati** per piano, archi, chitarra e lead: oggi stanno tutti intorno a 55-84 e gli archi raddoppiano il piano all'unisono. È una scelta di mix (bande: basso ≤47, piano 48-79, chitarra ≥67, archi sopra o sotto il piano, lead ≥79): va ascoltata prima di spedire.

3. **Pulizia**: normalizzare i fine riga di `ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.cpp` (il re-pin ha gonfiato il diff) e rigenerare la patch registrata.

## Regole

- Un commit per voce, prefisso `fix motore:` per le modifiche ai motori condivisi.
- Nessuna patch locale ai motori: si modifica il repo del motore e si ri-pinna con `sync-engines.py --record --synced`.
- Ogni voce si verifica con `ctest` (arranger_core) e con lo smoke test, più un ascolto A/B dello stesso seed.
