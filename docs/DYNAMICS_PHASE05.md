# Fase 05 — dinamiche native

Base `a46237b`, branch Vocaltest. Audit mirato: erano presenti EQ nativo,
insert seriali VST3/AU su tracce/master, bypass, stato, automazione parametri,
PDC e meter. Il ducking esistente non è un compressore con threshold/ratio.
Mancava un compressore nativo. Limiter e clipper sono opzionali nella roadmap:
non aggiunti, senza evidenza che giustifichi nuovi percorsi o latenza.

Il compressore riusa AudioPluginInstance e la catena esistente: identificatore
interno `fga:native-compressor`, sei parametri stabili, stato XML nel contenitore
già serializzato dagli insert. Nessuna migrazione `.fga` o dipendenza nuova.
Factory interna anche per caricamento e copia offline; nessuna discovery o
installazione esterna richiesta. I progetti senza insert nativi non li attivano.

DSP separato dall'editor: threshold -60..0 dB, ratio 1..20, knee 0..24 dB,
attack 0,1..100 ms, release 10..1000 ms, makeup -12..24 dB. Default -18 dB,
3:1, knee 6 dB, 20/150 ms, makeup 0. Detector peak stereo-linked, curva in dB,
parametri e bypass smussati in 10 ms, zero lookahead/latency, memoria fissa.
Non è un limiter True Peak. Meter GR atomico, editor con i sei controlli;
accesso tramite `+` della catena insert, su traccia o master. Automazione tracce
riusata; l'automazione degli insert master rimane il limite preesistente della UI.

Bug riprodotto: ChannelStrip::configure copiava un vettore EQ sul lato audio.
Contatore C++: 1 allocazione prima della correzione, zero dopo. Ora configurazione
per riferimento, bande legacy su array e coefficienti locali; mantenuti ordine,
normalizzazione e limite 16 bande. Nessun algoritmo EQ o generatore riscritto.

Test nel target engine esistente, `--dynamics`/CTest `dynamics`: curva e knee,
tempi 63%/37%, 15 combinazioni 44,1/48/96 kHz e 32/64/128/512/1024 frame,
stereo, bypass graduale/unity, valori non finiti, allocazioni C++ misurate,
parametri/state concorrenti, stato/progetti, track/master, ordine insert,
automazione, cancellazione caricamento e clone/export offline. Il contatore
test-only non misura ogni allocazione C/driver del sistema.

Test UI dedicato `--dynamics-ui-test CARTELLA`, Linux/Xvfb: editor, parametri,
meter, Undo/Redo bypass, riapertura native track/master e export attraverso il
Transparent esistente. Screenshot e log in `/workspace/scratch/dynamics-*`.
Risultati: build Release PASS; CTest mirati **5/5 PASS**, 80,00 s; UI **12/12
PASS**. 997 Hz, ampiezza 0,5 e default: THD+N massimo **0,0236892%**, GR ~6,85
dB; ampiezza 0,01 sotto soglia: residuo float <=0,00002975%. Nessuna misura
sonora su registrazioni umane o ascolto ABX eseguita.

Benchmark locale, istanza attiva a 48 kHz: **1.024.000 frame stereo / 44,339 ms**,
solo stato DSP **296 byte**. Segnale DC e blocchi 512; non è una garanzia CPU
su hardware utente né misura della memoria completa di processor/editor/DAW.

Regressioni pertinenti: audio_engine (insert/lifecycle/routing), project_io,
eq_controls e improvement_core. Non ripetuti audit e corpus True Peak 32x della
fase 04: codice OfflineExport/Transparent invariato. Nessun intervento a Vocal
Studio/Chops o generatori musicali. Nessuna fase 06 avviata.

Windows/macOS, ascolti, hardware/driver, plugin esterni reali e sanitizers non
validati in questo intervento; prove numeriche/locali non equivalgono a collaudo
qualitativo. Nessuna nuova decisione architetturale necessaria per il compressore;
eventuali limiter/lookahead/sidechain richiedono un intervento separato.
