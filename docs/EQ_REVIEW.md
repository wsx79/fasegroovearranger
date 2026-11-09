# Revisione EQ parametrico

Base: 0.42.0. La revisione mantiene le sei forme e le sedici bande del DSP esistente.

## Interfaccia e impostazioni

- Grafico con assi distinti per guadagno EQ (dB) e analizzatore (dBFS). Parametri permanenti sotto il grafico; selezione esplicita di tutte le bande, comprese quelle sovrapposte o escluse.
- Ingresso/uscita, solo ingresso, solo uscita o analizzatore spento; risposta veloce/media/lenta e scala EQ ±6/12/24 dB. Le preferenze sono salvate come impostazioni dell'interfaccia.
- Doppio clic sul grafico: aggiunge una banda. Doppio clic su un nodo: azzera il suo guadagno senza eliminare altre bande. Rotella: Q; Maiusc: regolazione fine. F/G/Q: apre la relativa casella. Canc: elimina la banda selezionata.
- Frequenza, guadagno e Q accettano inserimento manuale. Sono supportati `2.5k`, `2,5 kHz`, `2500 Hz`, `-6 dB`; testo invalido e valori fuori intervallo non cambiano il progetto. Il guadagno è disabilitato per passa-alto, passa-basso e notch.
- Undo per trascinamenti, rotella, tipo, attivazione, inversione e reset. La semplice selezione non crea una voce Undo. Menu asincroni protetti dalla distruzione del componente e dal cambio di stato.
- Azzerare l'EQ neutralizza i guadagni ed esclude i filtri di taglio e notch. La curva EQ esclusa viene conservata al salvataggio.

## Correzioni funzionali

1. Il grafico e l'audio usano la stessa implementazione dei coefficienti e il sample rate effettivo; non è più assunto 48 kHz.
2. L'analizzatore Hann a 4096 campioni è calibrato in ampiezza e combina la potenza dei due canali. Un segnale stereo in opposizione di fase resta visibile. In assenza di campioni nuovi la traccia decade.
3. I buffer condivisi fra audio e interfaccia usano campioni atomici e letture limitate alla capacità fissa, mantenendo l'allocazione fuori dal percorso audio.
4. Quattro bande personalizzate non vengono più riconvertite implicitamente nei controlli legacy. Una modifica del pannello compatto interviene sulla prima banda del tipo richiesto e mantiene le altre.
5. La proprietà opzionale `eqBypassed` conserva le curve escluse. I lettori precedenti mantengono l'EQ spento; risalvare da una vecchia versione non conserva questa nuova proprietà. Le versioni di progetto già supportate restano leggibili.

## Verifica

- `eq_controls`: parsing, intervalli, isolamento delle bande compatte, risposta dei filtri e reset neutro; eseguito anche con ASan/UBSan.
- `--eq-ui-test`: interazioni sul componente JUCE, Undo, inserimento numerico, cambi traccia, layout minimo/normale e schermate italiane/inglesi. Confronto fra curva e audio per tutte le sei forme a 44,1/48/96 kHz; controllo spettro stereo e decadimento.
- `--eq-spectrum`: rendering e lettura concorrente dei tap stereo, limiti del buffer e corrispondenza ingresso/uscita.
- Regressione di persistenza dell'EQ escluso nel percorso reale JSON del progetto; suite audio/IO e core/MIDI.
- Il flusso CI di rendering nativo esegue anche il controllo dell'equalizzatore su Linux e Windows. Le verifiche eseguite in questa sessione sono su Linux.
