# Collaudo Windows delle fasi 03–06 — da eseguire

Questa checklist **non è un risultato PASS manuale**. La build Windows x64
nativa delle fasi 03–06 e i test automatici su Windows Server sono verificati in
[VOCALTEST_PREVIEW_WINDOWS.it.md](VOCALTEST_PREVIEW_WINDOWS.it.md).
Scaricare l'artifact verde di quella run e confrontare lo SHA `build_commit`
con `PREVIEW.json`. Il precedente artifact `0649e93` precede le fasi 03–06 e
non può validarle. Le prove sintetiche del runner non validano Windows 11,
ASIO/dispositivi reali o i propri plugin e non sostituiscono l'ascolto.

## Ritorni A/B: collaudo aggiuntivo

**Attenzione:** la successiva revisione locale dei livelli comuni, colori,
grafica e testo pan non è nell'artifact 2f7eb7a. Attendere una nuova build verificata
prima di validare queste modifiche su Windows; i PASS della preview restano storici.

- Impostare A al 62%; attivare A su Piano e Chitarra. Cambiare selezione: il livello
  comune e gli effetti devono restare uguali. B viola deve restare distinto da A turchese.
- Portare A a zero e di nuovo al 62%: attivazioni conservate. Provare Undo/Redo,
  disattivazione di una traccia e salvataggio/riapertura. Su file storici verificare
  avviso dei valori individuali, adozione esplicita e Undo esatto.
- Con automazione pan in play, osservare il numero sotto ogni manopola senza
  toccare A/B: deve aggiornarsi continuamente con il knob. Premendo A/B non deve
  comparire un salto dovuto a testo vecchio. Ripetere con DPI 100/125/150%, mixer
  compatto e normale, finestra ritorni aperta/chiusa e a trasporto fermo.
- Aprire i tre editor nativi; controllare grafica, valori digitati/drag/reset e
  dimensione minima. I diagrammi di riverbero/delay sono schemi dei parametri.

La prima implementazione descritta in [BUS_SEND_REVIEW.it.md](BUS_SEND_REVIEW.it.md) è inclusa
nella build **2f7eb7a**, [run 43089785681](https://github.com/wsx79/fasegroovearranger/actions/runs/43089785681).
Build e controlli automatici Windows PASS; i seguenti controlli umani rimangono
da eseguire. Gli artifact precedenti non contengono questo redesign.

- Creare un nuovo progetto: in **Bus e mandate → Ritorni A/B** verificare A con
  riverbero, B con delay. Nella nuova revisione dosare i livelli comuni e usare A/B nel mixer.
- Aggiungere un compressore dopo il riverbero in A, scegliere Kick come sorgente,
  abbassare la soglia e controllare riduzione/ascolto. Provare anche un compressore
  sugli insert del basso. Verificare mute/solo, source change, bypass e Undo/Redo.
- Rimuovere riverbero/delay mantenendo gli altri effetti; provare catena vuota
  (passaggio asciutto), riordino e salvataggio/riapertura. I nuovi dati usano formato 7:
  conservare una copia apribile con la preview precedente.
- Aprire un vecchio progetto e confrontare il suono prima di convertire i ritorni.
  La conversione arresta la riproduzione e riparte dai default; provare Undo.
- Fermare il trasporto, annotare i pan e premere A/B su più canali; ripetere in play
  distinguendo pan base e automazione. Se cambiano, registrare traccia, valore prima/
  dopo e stato dell'automazione. Ripetere con il mixer compatto e al DPI usato.
- Con plugin che dichiarano latenza, verificare allineamento di dry/ritorni,
  detector e transienti, export WAV, code, annullamento e sovrascrittura.

Annotare: SHA della build, versione Windows 11, CPU/RAM, interfaccia e versione
driver, frequenza/buffer, plugin e relative versioni, esito e passaggi per
riprodurre problemi. Usare una copia di un progetto esistente. Vocal Studio e
Vocal Chops rimangono congelati e senza approvazione funzionale/qualitativa.

1. **Avvio e progetto.** Estrarre tutto lo ZIP, avviare l'EXE, scegliere il proprio
   dispositivo audio. Aprire il progetto, ascoltare e controllare mixer, routing,
   note e automazioni. Salvare con un nuovo nome; chiudere e riaprire l'app.
   Verificare che progetto e impostazioni audio tornino correttamente.
2. **Transport e dispositivo.** Provare play/stop, seek, loop e mute/solo.
   Iniziare con 48 kHz/512 frame; ripetere ai buffer 64/128 e alle frequenze
   44,1/96 kHz, se realmente supportati dal driver. Annotare click, note bloccate,
   interruzioni o crash. Provare il riavvio del dispositivo quando consentito.
3. **Compressore.** Dal `+` degli insert inserire il compressore nativo su una
   traccia e sul master. Provare threshold/ratio, knee, attack/release e makeup;
   controllare GR e bypass. Verificare Undo/Redo, automazione dei parametri sulla
   traccia, ordine insert e riapertura del progetto. L'automazione degli insert
   master resta una limitazione della UI preesistente.
4. **Sidechain.** In Bus & Send mandare un suono sostenuto a GRUPPO 1; scegliere
   Kick come sorgente, depth circa 0,5, attack 20 ms, release 180 ms, curve 1.
   In loop cambiare ripetutamente sorgente, attack 0/200 ms, release 10/2000 ms e
   curve 0,25/4, anche prima che finisca la transizione precedente. Ascoltare
   separatamente eventuali click al momento dell'edit e il pumping normale.
   L'attacco istantaneo storico sui colpi non è stato eliminato. Ripetere con
   GRUPPO 2, con compressori attivi e bypassati, poi Undo/Redo e save/reopen.
5. **Routing della sorgente.** Il mute/solo della traccia può escluderla dal
   detector. Il mute del suo gruppo avviene dopo il prelievo e **non** elimina
   il trigger. Per il ducking storico dei gruppi, le sorgenti sono le 18 tracce generate con ID stabili; il nuovo compressore può selezionare le tracce presenti fino ai 32 ID.
   Lo stem della sola traccia target non include una sorgente su un'altra traccia.
6. **Export.** Selezionare Transparent e ceiling -1 o -3 dBTP; provare WAV
   16/24 bit a 44,1/48/96 kHz. Controllare durata, pause, code, canali e formato
   del file. Confrontare con il playback compensando il volume: Transparent può
   attenuare uniformemente l'intero brano, quindi un livello minore è previsto.
   Per una misura numerica usare un meter True Peak indipendente, se disponibile;
   il solo ascolto non misura il ceiling. Loud rimane indisponibile.
7. **Annullamento/errori.** Esportare su una copia di un WAV già esistente;
   annullare in punti diversi. Il file precedente deve rimanere integro e la DAW
   deve poter riprodurre/esportare ancora. Ripetere con sovrascrittura riuscita e
   con una cartella senza permesso di scrittura. Controllare messaggi, ripristino
   di controlli e assenza di temporanei residui. Provare anche MP3: il suo picco
   decodificato non è garantito dal ceiling PCM.
8. **Integrazione e plugin reali.** Durante l'export tramite copia privata,
   verificare che il playback continui. Se possibile cambiare dispositivo/buffer
   durante l'export e controllare il playback successivo. Provare gli strumenti e
   gli effetti VST3 regolarmente installati, inclusi bypass e plugin con latenza.
   Nessun test automatico con una fixture sostituisce questa prova.
9. **Sessione reale.** Riprodurre in loop per almeno 15 minuti con automazioni,
   entrambi i gruppi e insert attivi. Annotare CPU/memoria, eventuali contatori di
   dropout del driver, rumori, blocchi e perdita di stato. Salvare e riaprire.
   Separare un'impressione d'ascolto da una misura o da un errore riproducibile.

Per ogni punto registrare **PASS / FAIL / NON ESEGUITO**, configurazione, ora e
passaggi. Un PASS su un dispositivo non valida tutti i driver o plugin.
Le prove Windows, hardware, ascolto e stabilità prolungata restano pendenti
finché questa checklist non viene realmente eseguita. Nessuna fase 07 autorizzata.
