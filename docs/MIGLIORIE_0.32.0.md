# Migliorie 0.32.0

Base recuperabile: tag `v0.31.21`, commit `4d024b26c469726098dc8786b5b3e515b4f89066`.
Release 0.32.0: pubblicazione automatica subordinata alle verifiche CI su Linux, macOS e Windows.

| PDF | Accesso e comportamento |
| --- | --- |
| M-01 | Menu della clip → **Converti in regione audio**. Conversione esplicita per sample one-shot con un unico trigger sulla nota radice; i progetti legacy non vengono riclassificati automaticamente. |
| M-02 | Regioni audio: micro-rampa di 2,5 ms, fade indipendenti trascinando i quadratini ai bordi superiori. I tagli mantengono i fade esterni e creano bordi interni con declick. |
| M-03 | Punto nell'header delle clip con feel locale; passando sul titolo si leggono Density/Swing/Feel. Menu clip → **Ripristina feel globale**. |
| M-04 | Inizializzazione preventiva di sample rate, ADSR e filtri; limiti dei buffer nel rendering del sampler. |
| M-05 | Circolo delle quinte: toggle **Audition**, rilascio automatico e alla chiusura, punteggi percentuali delle transizioni sugli slot. |
| M-06 | Menu sezione → **Armonia indipendente**; **Ruota accordi sezione** oppure modifica nel circolo. Le sezioni senza override continuano a usare l'armonia globale. |
| M-07 | Menu della sezione che precede un Drop: silenzio di Kick/Bass negli ultimi due beat, rullata accelerata, crash invertito per singolo colpo sui sample. Le opzioni rispettano scope e blocchi. |
| M-08 | Studio → **Estrai groove** dai transienti/slice già individuati. Il template conserva scostamenti e accenti su quattro beat. Menu clip → **Rimuovi groove estratto**. |
| M-09 | Bus & Send → **Sidechain** per Group A/B. Detector dall'audio effettivamente udibile del Kick, non dal clock; rilascio 180 ms. |
| M-10 | Inspector → **EQ / HP**: high-pass 20–400 Hz e Low/Mid/High ±18 dB. EQ disabilitato trasparente, nessun plugin o latenza aggiuntiva. |
| M-11 | Inspector o menu traccia → **Freeze / Unfreeze** per VST/AU. Bounce privato in background di strumento e insert; istanze originali conservate e sospese. Gain, pan, send ed EQ nativo restano disponibili. |
| M-12 | Mixer: LUFS Short-Term/Integrated con K-weighting e gating. Menu Export: guard opzionale a -1 dBTP, oversampling FIR 4x e controllo finale del WAV prima dell'eventuale codifica MP3. |
| M-13 | MIDI editor: selettore **Note fantasma** per sovrapporre un'altra traccia allineata al tempo assoluto. |
| M-14 | MIDI editor: selettore Velocity/CC64/CC1/Pitch Bend; disegno nella corsia inferiore, persistenza, playback ed export. I tagli conservano il valore iniziale del frammento destro. |
| M-15 | Sample Browser → **Sync**: stima BPM/intonazione e preview con time-stretch e trasposizione aggiornati al progetto. |
| M-16 | Studio: **Note → MIDI** dopo l'analisi **Note**; crea una nuova traccia con note stimate limitate al trim del campione, senza alterare il file sorgente. |
| M-17 | AI Composer: **Solo sezione selezionata** e prompt rapidi; l'applicazione preserva le altre sezioni, comprese le porzioni esterne delle timeline indipendenti e i blocchi. |
| M-18 | Timeline in `ArrangementView.cpp`, audit visivi in `UiAuditTests.cpp`; export parallelo limitato a due motori privati per stem interni/sampler senza insert ospitati. Tracce con plugin mantengono il percorso seriale. |

## Compatibilità e limiti funzionali

- Lo schema 6 viene scritto solo quando vengono utilizzati i nuovi dati persistenti; restano leggibili gli schemi 1–5. Un progetto con dati di schema 6 richiede questa o una versione successiva.
- Freeze è una cache di sessione: riaprendo il progetto si ritrovano gli strumenti originali. Un cambio di BPM invalida la cache e riattiva i plugin, evitando di riprodurre il bounce a un'intonazione diversa.
- L'analisi spettrale e la stima BPM/intonazione della preview sono indicative. Materiale percussivo senza altezza affidabile viene sincronizzato nel tempo senza una trasposizione arbitraria.
- Il guard verifica il true peak del PCM con oversampling 4x; una codifica lossy può modificare i picchi del segnale decodificato. Non viene applicata una normalizzazione automatica a un target LUFS.
- Lo zero di latenza riguarda l'EQ nativo. Il guard di export ha ritardo FIR compensato; i plugin mantengono la loro compensazione di latenza.

## Verifica

Gli esiti effettivi sono raccolti in `VALIDAZIONE_0.32.0.txt`; il completamento delle singole funzioni non implica che siano state provate su hardware audio/MIDI reale, AU/macOS o Windows in questa sessione Linux.
