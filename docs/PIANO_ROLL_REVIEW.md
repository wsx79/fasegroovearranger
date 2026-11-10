# Piano roll — revisione 0.43.2

## Comportamento

- All’avvio e rientrando dall’arrangiamento è attiva **Selezione**. Cambiare un’opzione mentre l’editor è già aperto conserva lo strumento corrente.
- Si ricordano scala evidenziata, griglia, traccia di riferimento, corsia velocity/CC e vista note della chitarra, anche ricreando il piano roll. È ricordata anche la visibilità delle note attive.
- **NOTE TRACCIA** nasconde la rappresentazione delle note e delle velocity della parte attiva, senza silenziarla o cambiare il progetto. La griglia diventa una vista di confronto; i comandi diretti di modifica vengono disabilitati finché le note non vengono mostrate di nuovo.
- Le note della traccia di riferimento sono limitate all’intervallo della clip analizzata. L’inquadratura tiene conto del loro registro; nascondendo la parte attiva, si adatta alla sola traccia di riferimento.
- La scala evidenziata segue tonalità e modo effettivi di ogni sezione attraversata. La tastiera a sinistra mostra il contesto all’inizio della clip.

## Accordi dopo la modifica delle note

Le etichette del piano roll leggono il MIDI della clip corrente: non riscrivono gli accordi del progetto né rigenerano gli altri strumenti. Le celle seguono i confini armonici assoluti della timeline, anche se la clip inizia a metà di una cella.

L’analisi considera note realmente simultanee, con intervalli semiaperti: una nota che termina quando la successiva comincia non viene sovrapposta artificialmente. Include anche i rilasci; prima erano controllati soltanto gli attacchi. Una settima che diventa una triade durante la cella non viene più riconosciuta come un unico accordo di settima per tutta la durata.

Sono riconosciuti set completi di triadi, settime, sus2/sus4, add9, none, 6/9, undicesime e tredicesime, con raddoppi d’ottava e rivolti. Il riconoscimento non inventa radici o terze mancanti. Alcuni set di note ammettono più interpretazioni: la nota più bassa disambigua quando identifica una radice; altrimenti l’analisi rimane prudente.

Gli attacchi sfalsati di un accordo non devono farlo apparire come due accordi diversi: le configurazioni fugaci inferiori a 1/16 di beat, ridotte proporzionalmente nelle celle molto brevi, sono escluse dal confronto. Due accordi persistenti diversi o materiale non riconoscibile mantengono l’indicazione armonica prevista, in colore attenuato; le etichette riconosciute dalle note hanno invece colore pieno. Una melodia monofonica non viene sommata in un falso accordo. Le etichette sono quindi un aiuto contestuale, non una trascrizione armonica universale.

## Verifiche

Le regressioni del core coprono tutte le dodici fondamentali per gli accordi estesi, gli attacchi sfalsati, i rilasci, le note di passaggio e i cluster non riconoscibili. Il controllo nativo usa vere schermate del piano roll e note esattamente sovrapposte per verificare che la traccia di riferimento venga scoperta e che MIDI e Undo restino identici.

Comando nativo riproducibile: `FaseGrooveArranger --piano-roll-test CARTELLA` (build con `FGA_SELF_TESTS=ON`). Il controllo è incluso nelle verifiche native Linux/Windows della pipeline. Le preferenze dei self-test sono isolate da quelle dell’utente.

Risultati locali Linux/JUCE 8.0.9: **8/8** test core in Release e **8/8** con ASan/UBSan e leak detection, senza segnalazioni; **11/11** verifiche del piano roll nativo; suite completa dell’interfaccia superata (editor, gesture, Undo, pannelli, strumenti musicali e routing). Traduzioni, modulo condiviso, guide e packaging verificati. I log dell’ambiente Xvfb contengono avvisi X11/BadAtom già presenti; non sono prove su hardware o plugin commerciali.
