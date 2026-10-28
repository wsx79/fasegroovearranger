# Backlog armonia e arrangiamento — 0.23.8

Questo lavoro è **separato dalla UI** e va nella seconda release.

Regole:

- ogni voce è un commit a sé, con messaggio che inizia con `harmony:`;
- nessun commit di questo backlog tocca file di sola UI (tema, layout, testi, pannelli);
- `git revert <sha>` di un singolo commit deve ripristinare il comportamento musicale precedente **senza** toccare la UI;
- ogni voce va verificata con lo stesso seed e lo stesso progetto, confrontando l'export WAV prima/dopo (e con un ascolto A/B), non solo a orecchio;
- il seed resta invariato: a parità di seed, prima/dopo devono cambiare solo le note previste dalla voce.

## Voci previste

Ordine consigliato: prima i difetti oggettivi (1, 2, 3, 6, 10), poi le scelte musicali (4, 5, 7, 8, 9).

1. **Una sola descrizione dell'accordo per slot.** Piano usa `sgk::buildChord` (estensione completa), chitarra e lead ricevono l'estensione ridotta a `{triadi, settime, none}` — con sus/alterata/soul il piano e la chitarra suonano accordi diversi (nona minore contro nona naturale). Riferimenti: `Source/core/Project.cpp:886`, `:903`, `fasegroovepiano/Theory.cpp:205-260`, `fasegroovelead/Lead.h:125`.
2. **Fill e crash al cambio sezione.** Ogni clip è renderizzata da sola, quindi `afterSectionFill` è sempre falso e il crash sul primo beat della sezione successiva non arriva mai. Riferimenti: `Project.cpp:917`, `fasegroovedrums/GrooveEngine.cpp:735-739`, `:877-890`.
3. **ADD SECTION non deve creare una sezione muta.** `insertGuide` stacca tutte le tracce e non crea clip; la sezione resta silenziosa. Riferimenti: `Source/Timeline.cpp:70`, `MainComponent.cpp:62-63`, `Project.cpp:414-429`.
4. **`varyForm` deve rispettare il `phrase`** calcolato da `arrangeSong`, altrimenti il drop parte su un accordo arbitrario (progressione da 3 battute, drop da 32). Riferimenti: `Project.cpp:581-593`, `:710-715`, `:812`.
5. **Fill armonici alla fine di ogni sezione** (oggi ogni 16 battute fisse), allineati ai fill della batteria. Riferimenti: `GenreSound.h:38`, `GrooveEngine.cpp:1657`, `:1664-1672`.
6. **Gli accordi tenuti non devono suonare oltre il cambio** sulla griglia a ½ battuta (`DurationFor` non clampa allo slot successivo). Riferimenti: `GrooveEngine.cpp:1626-1641`, `Generators.cpp:167`, `Comping.cpp:162-166`.
7. **Registri separati** per piano, archi, chitarra, lead (oggi tutti intorno a 55-84, archi in unisono col piano). Riferimenti: `Theory.cpp:787`, `GrooveEngine.cpp:2401`, `Guitar.cpp:407`, `Lead.cpp:201-224`.
8. **Lead: fold dell'ottava invece del clamp** (con +2 globale e +2 di lane la melodia collassa su una nota pedale). Riferimenti: `Lead.cpp:224-225`, `Guitar.cpp:394`.
9. **NEW CHORDS**: mantenere il numero di accordi, non cancellare gli accordi liberi e attingere al vocabolario del genere (oggi solo diatonico e massimo 4 slot). Riferimenti: `Project.cpp:480-483`, `Generators.cpp:1107-1179`.
10. **Pennello della lane chitarra**: usare lo slot assoluto come la striscia accordi, non quello locale al clip. Riferimenti: `MainComponent.cpp:524` contro `:486`.

## Note di verifica

- clash semitonici: nessuna coppia di note a distanza 1 o 2 semitoni tra piano/chitarra/lead nello stesso slot;
- transizioni: presenza di fill sull'ultima battuta e crash sul downbeat della sezione seguente;
- forma: lunghezza di ogni sezione multipla del `phrase`;
- registro: nessun unisono esatto piano/archi, lead sopra il comping;
- determinismo: stesso seed ⇒ stesse note, e diff limitato alla voce in prova.
