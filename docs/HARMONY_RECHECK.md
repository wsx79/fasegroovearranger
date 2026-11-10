# Nuovo controllo armonico — 10 novembre 2026

Base: release **0.43.0**, commit `21c9119`. Correzioni preparate per **0.43.1**. Generatore locale dei 15 generi; Composer IA escluso. Nessuna modifica ai motori vendorizzati. Un campo opzionale per lo stile della take Strings conserva la compatibilità con i progetti precedenti.

## Difetti riprodotti e corretti

| Caso | Prima | Dopo |
|---|---|---|
| Clip libera attraverso un’armonia indipendente | Nel test Do/Fa♯: 12/12 note di piano e 4/4 di basso continuano con l’armonia precedente. | 0 note errate; la take viene separata al cambio armonico. |
| Rigenerazione di un loop già sfasato | Periodo 4 beat, fase 1,25: 16/27 note fuori dall’accordo nel test controllato. | 0/24; periodo e fase conservati, anche con origine della sorgente prima del beat zero. |
| Accordi nel righello | Sezione che inizia al beat 4, accordi di 16 beat: al beat 16 mostra Do mentre suona Sol. | Mostra Sol nella cella 16–32, seguendo la fase globale realmente suonata. |
| Micro-note del Lead | Nel test di 32 rigenerazioni Piano House: 16 frammenti inferiori a 1/128 di beat. Anche gli attacchi anticipati prima di una pausa possono produrre residui. | Collisioni ravvicinate accorpate e attacchi di confine gestiti prima del taglio; zero frammenti nel corpus finale. |
| Lane elettroniche ignorate | Con tutte le lane mute rimangono 4 note di chitarra e 18/36 di Lead nei test Techno/Electro. | Nessuna nota; rispettati mute, livelli e ottave. Combinare le ottave conserva intervalli e classe delle note ai limiti MIDI. |
| Frase di chitarra ripetuta | Deep: 4 ripetizioni consecutive su 80; ’90s: 3 su 80. Cambiare il fill finale nascondeva la ripetizione della parte riconoscibile. | Zero nei test. Si confronta l’ingresso e la frase effettivamente suonata, con estrazioni dal motore dello stesso genere. |
| Loop corti silenziosi | Le pause d’ingresso di una sezione lunga possono cancellare tutta la sorgente di una/due battute; la chitarra elettronica entra solo nella terza battuta, assente nella sorgente corta. | La sorgente esplicita conserva la frase prima della ripetizione; le sezioni lunghe e le pause DJ mantengono il proprio sviluppo. |
| Strings perse su Applica | Rigenerazione esplicita con stile globale Off: scrive un pad, ma Applica o un cambio armonico lo cancella. | La provenienza della take è salvata; Applica la conserva. Selezionare Off dopo un altro stile o generare un nuovo brano continua a spegnere le Strings. |

La separazione riguarda le parti generate editabili, compresi i Vocal chops con suono guida intonato. I blocchi, le altre clip e le tracce manuali restano protetti; le automazioni coprono l’intera selezione originale. Le referenze dei controlli dello strumento restano valide. Una rigenerazione di sezione conserva il MIDI esterno alla sua sezione; una clip senza loop rimane senza loop dopo la separazione automatica. L’apertura di un progetto non rigenera il MIDI salvato.

## Varietà realmente misurata

Per ogni genere: **80 rigenerazioni per Piano, Bass, Guitar e Lead**, totale **4.800 take**. Si confrontano attacchi quantizzati ai sedicesimi e intervalli, ignorando velocity e trasposizione. Il confronto parte dall’ingresso reale dello strumento, esclude micro-frammenti e il turnaround finale: un fill diverso da solo non basta. Una regressione modifica soltanto le velocity per verificare che la firma rimanga identica.

La chitarra elettronica mantiene volutamente un vocabolario percussivo più ristretto, con spazio fra le risposte. Non viene riempita di note casuali per aumentare i conteggi.

| Genere | Piano | Bass | Guitar | Lead | Ripetizioni immediate |
|---|---:|---:|---:|---:|---:|
| Piano House | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Soulful | 80/80 | 80/80 | 80/80 | 79/80 | 0 |
| Deep House | 75/80 | 72/80 | 67/80 | 79/80 | 0 |
| Gospel | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Jackin' | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Jackin' Piano | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Garage & UK | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Afro & Latin | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Funky & Disco | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| '90s Piano House | 80/80 | 80/80 | 51/80 | 78/80 | 0 |
| Neo Soul & Broken | 80/80 | 80/80 | 80/80 | 80/80 | 0 |
| Chill & Pads | 71/80 | 72/80 | 80/80 | 80/80 | 0 |
| Dance Stabs | 80/80 | 80/80 | 68/80 | 79/80 | 0 |
| Techno | 37/80 | 80/80 | 8/80 | 75/80 | 0 |
| Electro House | 36/80 | 80/80 | 8/80 | 76/80 | 0 |

Il confronto con il core originale 0.43.0 trova **7 ripetizioni immediate e 156 micro-note** nello stesso corpus; dopo le correzioni entrambe le misure sono zero. Sono risultati di campionamento riproducibili, non una garanzia che qualsiasi futura estrazione sia unica.

Il confronto aggiuntivo della chitarra usa fino a otto battute e sedici candidati. Nei loop più lunghi già sfasati rimane la normale estrazione, senza accorciare o reinterpretare la sorgente per forzare il confronto.

## Coerenza e rendering

- **5.625 casi di loop**: 15 generi, Piano/Bass/Strings/Guitar/Lead, 12 tonalità, inizi allineati/spostati e tre fasi, più Dorico/Eolio/Frigio. Le note seguono l’armonia corrente; periodo/fase, APPLY e materiale escluso sono verificati.
- **150 loop corti** su tutti i generi e cinque strumenti: sorgenti di una/due battute udibili e riproducibili su Applica. Prove sullo stile Strings Off, cambio armonico, nuovo brano e salvataggio/riapertura.
- Prove aggiuntive su Vocal chops intonati, controlli delle lane, limite MIDI, blocchi, riproducibilità e automazioni dell’intera selezione.
- **15 confronti audio prima/dopo**: 30 WAV di otto battute, con i MIDI memorizzati, i suoni guida e le automazioni reali. Salvati anche `.fga` e `.mid`. Tutti finiti e udibili; picco massimo dopo i fix **0.4655014**, circa **-6.64 dBFS**, prima del clamp.
- Lettore SMF indipendente: tutti i 30 export validi, 19 tracce, marker e coppie Note On/Off senza note sospese.
- Suite core completa: **8/8** in Release e **8/8** con AddressSanitizer, UndefinedBehaviorSanitizer e leak detection (222,67 s), senza segnalazioni. Suite native di persistenza/importazione/IA e motore audio/trasporto/MP3: entrambe uscita 0. Verificati modulo condiviso, traduzioni, guide e packaging.
- I test audio Debug contengono assert JUCE preesistenti su stream/buffer: l’uscita 0 della suite non dimostra l’assenza di ogni problema JUCE.

I conteggi verificano coerenza e varietà; RMS e picchi verificano il rendering. La qualità artistica va confrontata anche ascoltando gli esempi. Questi sono frammenti con suoni guida, non prove di VST commerciali o di hardware reale. Verifica locale Linux/JUCE 8.0.9.

## Riproduzione

```sh
cmake --build BUILD
ctest --test-dir BUILD --output-on-failure
BUILD/fga_core_tests --harmony-recheck
BUILD/fga_engine_tests_artefacts/Debug/fga_engine_tests --harmony-recheck-audio CARTELLA
```

Le regressioni sono in `tests/HarmonyRecheckChecks.h`; l’audit nativo in `Source/ArrangementAudit.h`. I confronti locali sono in `/workspace/scratch/harmony-audio-before` e `/workspace/scratch/harmony-audio-fixed`; prove e misure in `/workspace/scratch/harmony-proof-*.log` e `/workspace/scratch/harmony-043-primary-*.log`.
