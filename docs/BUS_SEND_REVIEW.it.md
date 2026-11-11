# Ritorni A/B e sidechain — revisione approvata

## Ambito

Richiesta e approvazione esplicita dell'utente: ritorni A/B con catene modificabili,
riverbero iniziale in A, delay in B, compressore con sorgente sidechain selezionabile.
Base acquisita con fast-forward: fc856b532239036e89910ea474f7138ff9e98a0c;
preservata la grafica del compressore dei commit a411461/fc856b5.
Implementazione tecnica verificata localmente su Linux; validazione Windows e
funzionale/sonora ancora pendente. Nessun push, release o fase 07.
Vocal Studio/Chops e generatori non modificati. Nessuna dipendenza nuova:
wrapper nativi per JUCE e codice del progetto, con le licenze già applicabili.

## Utilizzo

1. Aprire **BUS E MANDATE**, ora con testo visibile nel mixer.
2. In **RITORNI A/B**, scegliere la traccia e dosare **MANDATA A/B**. I fader
   dei ritorni regolano quanto effetto torna al master; l'uscita della traccia
   rimane un controllo distinto.
3. Con **+** aggiungere riverbero, delay, compressore nativo o un proprio plugin.
   Il menu di ogni dispositivo permette di rimuoverlo o riordinarlo; il pulsante
   di alimentazione effettua il bypass e quello di apertura mostra l'editor.
   La catena usa gli otto slot esistenti, non un secondo host.
4. Per attenuare il riverbero sul colpo di cassa, mettere un compressore dopo il
   riverbero in A e scegliere **Kick** in **SORGENTE SIDECHAIN**. Per attenuare
   direttamente il basso, mettere il compressore sugli insert del basso.
   Provare soglia -30/-36 dB, rapporto 4:1, attacco 5 ms, rilascio 180/200 ms,
   makeup 0 dB; la riduzione dipende dal livello effettivo della sorgente.
5. **Segnale interno** usa l'ingresso del compressore. Una traccia selezionata
   alimenta invece il detector prima dei suoi insert e del fader. Mute/solo della
   traccia può escluderla; il mute del gruppo avviene dopo il prelievo.
6. Un ritorno senza dispositivi fa passare la mandata asciutta: non è silenzioso.
   Azzerare la mandata o il ritorno se non si vuole sommare quel segnale.

**GRUPPI 1/2** sono separati dai ritorni A/B: ricevono l'intera uscita di una
traccia e conservano il ducking storico. La vista mostra le tracce assegnate e
segnala un gruppo vuoto. Il ducking del gruppo non è un secondo compressore.
I pulsanti A/B del mixer mantengono il toggle storico; la regolazione continua
si trova nel pannello. Il difetto Windows dei pan rimane aperto, vedi sotto.

## Compatibilità e stato

I progetti storici mantengono il percorso fisso originale di riverbero/delay.
Non vengono convertiti all'apertura. La conversione è esplicita, arresta il
trasporto, riparte dai default degli effetti e supporta Undo; i valori originali
restano nel progetto/snapshot. Salvare una copia prima di convertire.

Le nuove catene e il compressore aggiornato usano il formato **7**. Un marker
esplicito protegge anche l'uso del compressore nuovo su un progetto che conserva
il routing storico. Le vecchie preview devono rifiutare questo formato, anziché
ignorare il sidechain e produrre un suono diverso. Un compressore storico v1 non
modificato non forza il cambio di formato. ID MIDI e formato dei dati precedenti
restano leggibili; il progetto attuale non promette apertura dei nuovi file sulle
vecchie build. Riordino e rinomina conservano gli ID degli insert.

## Percorso audio

- Due slot engine dedicati ai ritorni, distinti dalle tracce MIDI e dal master;
  stessi caricamento, editor, cache di stato, Undo, bypass e clonazione degli insert.
- Rendering delle sorgenti prima degli insert: la scelta del detector non dipende
  dall'ordine delle tracce. I buffer mono sono preparati fuori dal callback.
- Allineamento del detector alla latenza dell'ingresso effettivo del compressore.
  Se occorre ritardare l'audio, la latenza viene dichiarata alla PDC; un bypass
  nativo conserva tale allineamento. Buffer preparati sul lato controllo; nessuna
  loro allocazione nel callback. La sorgente è una scelta manuale non automatizzabile;
  i sei parametri storici conservano ID e comportamento.
- Ritorni in parallelo, poi master: massimi di latenza separati per tracce e ritorni,
  ritardo del percorso diretto e compensazione dell'export. Input/detector conservano
  la storia quando cambia solo l'identità della sorgente a latenza invariata.
- Code dichiarate di sorgenti, ritorni effettivamente usati e master sommate in
  serie, con il limite storico di 120 s. Dispositivi rimossi non lasciano stime
  del vecchio riverbero/delay; cambio di modalità non riproduce vecchi echi.
- Il delay nativo segue il BPM anche nella stima della coda prima del primo blocco.
- Modulo di staging/quantizzazione/True Peak **Transparent invariato**. Varia solo
  il segnale che gli viene consegnato e la durata/compensazione dei nuovi percorsi.

## Verifiche e limiti

Log e screenshot locali: `/workspace/scratch/bus-send-review`.
| Verifica | Risultato Linux |
| --- | --- |
| Build Release completa | PASS |
| CTest seriale completo | 18/18 PASS, 179,62 s |
| Ultima correzione reset dei rack: audio_engine/sidechain/dynamics | 3/3 PASS, 106,82 s |
| Ultima precisione delay: sidechain | PASS, 40,46 s |
| UI generale Xvfb | PASS, 67,49 s |
| UI ritorni, parametri, Undo/Redo, stato, sorgente | 24/24 PASS |
| UI compressore e dimensione minima | 16/16 PASS |
| Detector: 12 combinazioni rate/latenza, bypass e switch a pari latenza | residuo massimo 0, zero allocazioni C++ ordinarie |
| Delay: 72 combinazioni rate/BPM/divisione | campione iniziale/interpolazione corretti, zero allocazioni C++ ordinarie |
| Sorgente esterna misurata | 15,9863 dB GR, 15,9863 dB attesi |
| PDC parallela dry/A/B | impulso allineato campione per campione; 53 + 137 = 190 frame dichiarati |
| Code seriali e modalità | sorgente 5 s + ritorno 10 s; rimozione elimina stime storiche; nessun vecchio eco alla conversione |
| WAV e ripristino | clonazione privata, cancel tardivo effettivamente raggiunto, file precedente integro, overwrite 48 kHz/24 bit PASS |

Il delay ora inizializza il tempo corretto al primo blocco, anche per divisioni
corte: il normale smoothing dei cambi successivi non ritarda il primo eco.
L'editor accetta la virgola decimale e conserva le divisioni digitate senza la
quantizzazione del vecchio slider normalizzato; visualizza tre decimali per i beat.
I rack FX senza destinazione MIDI non accumulano controller inutilizzati ai loop.

### Prestazioni locali

Workload comune prima/dopo: 100 callback stereo da 512 frame a 44,1 kHz,
senza compilazioni/test concorrenti. Binario precedente conservato con SHA256
`8051f3084a55d34796ec2d9b10ca6e54ed3bb1a8c0804951185a7d2c896c257a`.
La grafica intermedia del compressore non altera questo workload DSP.

- Coppia senza campionamento RSS: mediana 2,145 → 2,113 ms;
  p99 4,328 → 3,756 ms; massimo 5,067 → 5,763 ms.
- Coppia con campionamento RSS: mediana 2,925 → 2,958 ms;
  p99 7,331 → 5,082 ms; massimo 8,522 → 6,588 ms.
- RSS nel punto comune: 95.516 → 98.564 KiB, +3.048 KiB (circa +3,2%).
  Il picco delle suite complete nel probe era invece 95.812 → 191.520 KiB:
  la suite nuova include molte fixture aggiuntive e non isola il consumo della DAW.
- Rendering nativo con ritorni: 51.200 frame misurati in circa 150–160 ms nelle
  prime prove; zero allocazioni C++ ordinarie nel tratto misurato e latenza aggiunta
  zero con soli dispositivi nativi a latenza zero. Il routing fra plugin con
  latenze diverse può richiedere ritardi dichiarati alla PDC.

Non emerge un peggioramento CPU significativo nel workload comune delle due
prove locali (mediana circa -1,5% / +1,1%). Non sono misure generalizzabili:
nessun driver reale, misura di dropout o confronto prestazionale Windows.
I log `benchmark.json` e `memory-common.json` conservano entrambi i confronti.
Storage nominale dei detector: 32 × blocco × 4 byte; le linee di compensazione
crescono con la latenza e sono preparate fuori dal callback. Due rack riusano
anche le riserve dell'host. Il delay nativo mantiene una linea stereo di circa
8 s: circa 3,08 MB a 48 kHz/blocco 512, oltre al percorso storico conservato.
Le capacità possono essere trattenute per il riuso; nessuna stima del consumo
complessivo della DAW è dedotta da queste sole dimensioni.

**Pan A/B:** l'utente conferma un cambiamento visuale su Windows. Non riprodotto
su Linux. Il test ora usa `Button::triggerClick`, verifica tutti i canali, pan
salvati, curve, valori e pixel, a trasporto fermo/in riproduzione, con Undo/Redo:
11.736 confronti PASS. Non equivale al clic nativo Windows o alla riproduzione
con hardware. Nessuna causa o correzione del difetto è dichiarata.

Aggiornamento Windows: **2f7eb7a**, [run 43089785681](https://github.com/wsx79/fasegroovearranger/actions/runs/43089785681),
MSVC x64 Release **PASS**, CTest **18/18** (181,65 s), ritorni **24/24**,
compressore **16/16**, guard pan **11.736 confronti**, UI/export/VST3 fixture e
avvio del pacchetto estratto **PASS**. Nessuna correzione richiesta dal runner;
workflow riutilizzato. Artifact e download verificati, sorgenti/licenze inclusi:
[VOCALTEST_PREVIEW_WINDOWS.it.md](VOCALTEST_PREVIEW_WINDOWS.it.md).
Il guard Windows usa ancora `Button::triggerClick`, non clic OS: il problema
originale rimane aperto. **NOT RUN**: macOS, ascolto, Windows 11/hardware/dropout,
vendor plugin con il nuovo routing e sanitizers della nuova estensione.
La vecchia preview non contiene questa revisione; usare lo SHA sopra.

Sorgenti: tracce presenti fino ai 32 ID stabili; nessun ingresso detector da
ritorno/master, rete o bus sidechain di plugin esterni. L'automazione degli insert
su ritorni/master non è esposta. Cambi di routing che modificano la latenza possono
richiedere una transizione: non si promette assenza di click/dropout su ogni plugin.
Le misure locali non sono una garanzia su driver, CPU o progetti dell'utente.
