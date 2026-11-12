# Fase 09 — Gate di qualità e rilascio

**Verdetto: NO-GO per distribuzione/release.** La verifica Windows reale del fix
funzionalmente/qualitativamente. Ascolto, hardware e piattaforme non disponibili
non sono sostituibili con test sintetici Linux.

Base locale `23be496ed1b5618d3a235da5995293b078d89a8e`, repository
`wsx79/fasegroovearranger`, branch esclusivo Vocaltest, directory inizialmente
pulita. Documento ufficiale `phases/09_RELEASE_GATE.md` (intestazione interna
«PHASE 08», mentre file e indice definiscono fase 09). Nessuna nuova funzione
musicale, fase 10, modifica a main, push, merge, tag, release o workflow remoto.
Il gate è una valutazione di disponibilità al rilascio, non un'autorizzazione.

## Interventi circoscritti

### Copertura del runner

La ricetta `scripts/test-native-render.py` non chiamava il test di MASTER ASSIST.
Una regressione mocked ha dimostrato che il runner poteva concludere senza
verificare quella UI, anche se il report corrispondente era assente/FAIL/NOT RUN.
Aggiunta la chiamata nativa esistente e verifica di report non vuoto, con tutte
le righe PASS. Il nuovo `native_render_gate` è registrato in CTest: due metodi,
nove sottocasi, incluse simulazioni del plumbing Linux/Windows/macOS. Queste
simulazioni **non** sono build o esecuzioni native Windows/macOS.

I workflow esistenti già richiamano la ricetta prima del packaging: nessun
workflow duplicato e nessun avvio Actions. Nessun player, EQ, limiter o framework
di test musicale nuovo.

### Regressione focus UI riprodotta

Il ripristino dopo la chiusura di un editor VST terminava al primo recupero del
focus. Una successiva notifica nativa poteva spostarlo sulla cornice della DAW,
lasciando il componente principale senza focus pur con peer attivo. L'errore è
stato riprodotto più volte su Xvfb; diagnostica: finestra abilitata/visibile,
foreground, nessun blocco modale, focus JUCE sulla cornice.

La traccia temporanea ha mostrato i callback iniziali fermarsi al primo successo.
La regressione ora introduce una perdita ritardata di 250 ms dopo la chiusura,
attende che avvenga e conserva l'asserzione esatta sul componente principale.
Un secondo caso assegna esplicitamente il focus con la tastiera a un TextEditor.
**Entrambi FAIL prima della correzione.**

`restoreFocusAfterHide` continua a osservare l'intervallo già esistente e limitato
(100 ms iniziali + al massimo 20 tentativi da 50 ms); non termina al primo
successo. Rispetta clic successivi, altri controlli scelti con la tastiera,
finestre riaperte, modalità e background. Nessun timeout allungato o asserzione
indebolita; tracing temporaneo rimosso. Solo message thread, SafePointer esistenti,
nessuna modifica al callback, DSP o trasporto. Questa nuova correzione del focus
non è validata su Windows/macOS né con mouse/DPI/hardware reali.

### Fixture export/Freeze

Il test nativo usava l'intera canzone predefinita: nel primo fallimento il WAV
misurava 380,7049375 s, 48 kHz, 109.643.294 byte. Il job superava l'attesa di 60 s;
il Freeze richiesto mentre exportTask era ancora attivo veniva correttamente
rifiutato. Due FAIL del test erano quindi conseguenza di un carico non limitato,
non prova di deadlock o di un difetto Freeze nel prodotto.

La fixture ora usa una battuta a 120 BPM con una nota esplicita e ripristina il
progetto originale dopo la prova. Stesso VST3 reale, stessi controlli di istanza,
sospensione, MIDI e Undo; stessi timeout. Non modificati generazione/arrangiamento
reali, Freeze, renderer, Transparent o il fix Cancel. I primi fallimenti sono
conservati nei log, non cancellati da una dichiarazione di successo retroattivo.

## Matrice di verifica

| Verifica | Risultato e ambito |
|---|---|
| Build Release GCC 14, app/test/VST3 fixture | PASS |
| CTest disponibili | **22/22 PASS**: 21 suite esistenti (210,15 s) + nuova native_render_gate (0,23 s). Dopo il fix solo UI, ricette native rieseguite; engine/DSP invariati |
| Risorse e packaging | PASS `verify-family.py`, packaging/preview fixture (licenze/sorgenti/hash, rifiuto runtime esterni); PE sintetici, non runtime Windows |
| Ricetta UI/render finale | PASS (144,98 s): startup, EQ, piano, UI generale/MIX ASSIST, MASTER ASSIST 17/17, MIDI, WAV/MP3 e 12 stem attivi allineati |
| VST3 effetti reali di test | **79/79 PASS** (28,20 s), PCM/stato/ordine/bypass/Undo/master |
| VST3 strumento reale di test | PASS (37,65 s), **15/15** editor/focus/Freeze; progetti legacy 1/2/3 e upgrade 4 multi-output, preset/automazione, PCM indipendente |
| Export UI, interno e VST3 | **43/43 PASS ciascuno**, compresi actual-button Cancel, preservazione file, cleanup/PDC, errori/sovrascrittura e ripartenza |
| Dati vocali/helper mancante | **14/14 UI** + **11/11 import**; sintesi reale NOT RUN, qualità non approvata |
| Staging Linux finale | PASS hash del binario, avvio MASTER ASSIST 17 e import 11; nessun ZIP. Staging precedente ripristinato |
| ASan/UBSan scoped | **3/3 PASS**: MIX, MASTER, EngineAudit; 15 TU applicativi instrumentati |
| TSan scoped | **3/3 PASS** sulle stesse prove; 15 TU + JUCE core/events instrumentati |
| ASan/UBSan UI scoped | **1/1 PASS**, VST3/focus/Freeze con MainComponent e UiAuditTests instrumentati; export esteso già provato nel normale Release |

Le esecuzioni finali normali hanno tutti i controlli superati. I FAIL iniziali
hanno portato alle correzioni documentate sopra, non a retry senza spiegazione,
soppressioni o aumento dei timeout. La regressione focus deliberata fallisce
prima e passa dopo. Nessun cambiamento architetturale o nuovo percorso audio.

I sanitizers sono binari QA separati, ricavati dai comandi della build corrente,
`-O1 -g1 -fno-omit-frame-pointer`, Address+Undefined oppure Thread. Leak detection
e halt-on-error abilitati, nessuna suppression. Nel binario GUI soltanto due TU
sono instrumentati; altre parti app/JUCE/core e VST3 restano non instrumentate.
Non è una prova sanitizer completa dell'applicazione o dei plugin commerciali.
Le corse UI temporizzate, la baseline Release e le compilazioni sanitizer sono
state eseguite in sequenza, senza compilatori concorrenti ai test export.

Baseline locale, non comparazione prima/dopo o promessa hardware: costruzione
finestra **154,036 ms**, processo startup 0,846 s / **56.464 KiB RSS**. Suite
sidechain: 32,078 s / **178.340 KiB RSS**, comprendenti fixture/render/export;
il solo kernel mixer/2 gruppi elabora **1.024.000 frame stereo in 123,317 ms**.
Misurate zero allocazioni C++ ordinarie nelle regressioni callback, PDC/latency
native invariata. Non comprende tutte le allocazioni C/allineate/driver e non
misura dropout o latenza fisica. La correzione aggiunge polling limitato nel
message thread, non lavoro nel callback audio.

Localizzazione: risorse, guide e layout della UI principale in sei lingue PASS.
MIX/MASTER ASSIST hanno diagnostica inglese; traduzione completa/revisione
linguistica dei nuovi pannelli non certificata. Rimane una verifica/rifinitura
per il collaudo finale richiesto dall'utente, non un nuovo intervento in questa fase.

Evidenze locali: `/workspace/scratch/release-phase09/`, con log CTest/build,
`native-final-results.json`, cartelle `native-*-final`, `vocal-preview-final`,
`focus-delay-before`/`focus-after`, `baseline.json`, `sanitizer-results.json`,
comandi/oggetti sanitizer e `linux-stage-final`. Manifest persistente:
`docs/RELEASE_GATE_PHASE09.json`, con base, hash dei file modificati e limiti.
Gli artifact Windows storici da 839f6e8 sono precedenti a Cancel/assistenti e
non chiudono nessuno dei gate attuali.

## Compatibilità, privacy e distribuzione

Nessuna migrazione/schema nuovo o modifica ai generatori, automazioni, mixer,
bus/mandate/gruppi, sidechain, compressore, PDC, MIX ASSIST, MASTER ASSIST o
Transparent. La modifica del prodotto è limitata al ripristino UI del focus.
Le prove dei dati vocali/missing-helper non riattivano lo sviluppo dei due moduli
congelati e non approvano la qualità del canto.

Riutilizzati `THIRD_PARTY_NOTICES.md`, `PRIVACY.md`, le licenze vendorizzate e
`docs/VOCAL_GUIDE_LICENSES.md`: helper GPL solo sorgente, COPYING/README;
aggiunto/distribuito. Separazione in processi non equivale a esenzione GPL.
I test packaging rifiutano runtime esterni non autorizzati e sorgenti/checksum
incompleti. Le verifiche sintetiche PE64 non sono prove di runtime Windows.

Staging Linux locale solo per collaudo, senza nuovo ZIP né pubblicazione; eventuale
staging precedente preservato e ripristinato. La prova runtime locale non dimostra
portabilità verso altre distribuzioni Linux o disponibilità del corresponding
source per la redistribuzione. Il pacchetto Windows completo dello snapshot
corrente non è stato generato o verificato. Nessun risultato di vecchi artifact
Windows viene trasferito a questo codice.

## Gate aperti e prossime autorizzazioni

| Gate | Stato |
|---|---|
| Cancel render su Windows, clic reali e VST3/bus/PDC | NOT RUN, bloccante |
| Build/runtime Windows corrente e relativo pacchetto/source/DLL | NOT RUN |
| macOS corrente, AU reali, firma/notarizzazione | NOT RUN |
| Hardware audio, dropout, ascolto, mouse/DPI e plugin commerciali | NOT RUN |
| App/GUI/driver/plugin interamente sotto sanitizers | NOT RUN; distinguere dalle prove scoped |
| Distribuzione di eventuali stack vocali/modelli esterni | Non autorizzata; nessun binario nuovo incluso |

Per chiudere i gate servono test nativi dello snapshot corrente e collaudo umano,
poi decisione esplicita sulle funzioni vocali. L'attuale divieto di push impedisce
ad Actions di consumare queste modifiche locali: non è una richiesta di aggirarlo.
Nessuna decisione architetturale necessaria per le correzioni eseguite. Qualsiasi
push, pubblicazione o fase ulteriore richiede nuova autorizzazione.

## Checklist Windows pratica

1. Usare una build nativa MSVC x64 **del nuovo commit**, non la vecchia preview.
2. Aprire/riprodurre/salvare/riaprire copie di progetti legacy e formato 7; controllare
   plugin/preset, automazioni, MIDI, mixer, pan, A/B, gruppi e sidechain.
3. Aprire/chiudere/riaprire un editor VST3, premere Space, poi scegliere un campo
   con Tab e digitare: il focus non deve restare sulla cornice o essere sottratto.
4. Freeze/Unfreeze con VST3, preset e routing; controllare note, Undo e latenza.
5. Export Transparent WAV/MP3/stem: Cancel immediato, durante il job e vicino al
   termine, con VST3 e A/B, ripetizioni e nuovo export. UI reattiva, vecchio file
   preservato, temporanei ripuliti, audio/PDC ripristinati. Registrare crash/blocchi.
6. MIX ASSIST e MASTER ASSIST: WAV/stem/reference, misure e preview, Stop/chiusura,
   target, Undo e rianalisi. Il confronto è su file, non sul progetto live.
7. Verificare recupero con asset/helper mancanti senza installare modelli incerti.
   Qualità vocale e inclusione della voce nel mix richiedono ascolto, non metadata.
8. Ascoltare con hardware reale, più rate/buffer e plugin abituali; annotare CPU,
   memoria, click, dropout e variazioni di volume. I benchmark locali non li provano.
