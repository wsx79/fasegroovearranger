# Catena effetti e automazioni — 30 ottobre 2026

Base: `89e67e3b3bad3d5840956f15d4bc50b7908fe3ed` (v0.29.0). Pubblicazione della versione 0.30.0 autorizzata dall’utente, insieme ai miglioramenti Studio.

## Utilizzo

- **EFFETTI → +** nell’inspector carica un effetto VST3 (anche AU su macOS). Sono disponibili **otto insert in serie per traccia**. Il browser mostra gli effetti; gli strumenti restano nel loro selettore.
- Ogni riga offre bypass, editor originale e menu. Doppio clic sul nome per assegnare un soprannome; il menu conserva il nome del plugin e permette di ripristinarlo. La presa a sinistra riordina la catena; sono disponibili anche Sposta su/giù. **FX** nel mixer porta al riquadro della traccia. Le finestre degli effetti usano centramento, chiusura manuale e primo piano già previsti per gli strumenti.
- **A**, il pulsante delle automazioni nel riquadro effetti e il titolo della corsia aprono il selettore. Scegli traccia/master, suono, strumento oppure una specifica istanza numerata di effetto; cerca il parametro e fai doppio clic o premi Mostra automazione. Le corsie già aggiunte sono riutilizzate; il gruppo Automazioni esistenti permette di richiamarle e rimuoverle. Fino a **32 corsie parametri per traccia**, oltre alle curve di volume/pan/master.
- Suoni guida: tono, riverbero, chorus e pump. Campioni: frequenza e risonanza del filtro. Le automazioni del filtro attivano il filtro durante la riproduzione, lasciando intatte le impostazioni del campionatore. Pitch e mappatura delle slice restano nei controlli Studio.
- Tasto destro nella corsia mantiene bypass, cancellazione punti/curva, generazione casuale, blocco e automazione alla rigenerazione. Il valore dei parametri usa il testo/unità esposti dal plugin; la frequenza del filtro mostra Hz/kHz.

## Architettura e correzioni

Le istanze hanno ID persistenti 1–8 separati dall’ordine, dal nome e dall’ID del plugin. Lo slot 0 identifica lo strumento, -1 i controlli nativi. Parametri e curve conservano anche l’ID del parametro fornito dal plugin: la ricerca all’apertura non dipende solo dalla sua posizione. Rinominare o riordinare mantiene i destinatari; eliminare o sostituire con un plugin diverso rimuove soltanto le curve dell’istanza interessata. Ricollegare lo stesso plugin conserva stato e curve. Undo ripristina assegnazione, stato e automazioni.

L’intero percorso audio, compreso l’export privato, usa la catena. Si sommano le latenze degli insert abilitati e si mantiene il limite di compensazione già esistente. Gli insert mono ricevono la somma attenuata di L/R e la loro uscita torna su entrambi i lati; le code dichiarate dei plugin entrano nell’export, con un limite di 120 secondi. Un effetto rimosso o un caricamento superato da un cambio di sorgente/progetto non può riapparire alla fine di una callback asincrona.

Il browser carica con un clic o con Invio: navigare con le frecce non deve aggiungere istanze. Cambiare traccia mentre si conclude una rinomina conserva la corretta identità della traccia e dell’effetto. L’inspector non duplica il comando di apertura degli effetti nel riquadro dello strumento.

**Stato con trasporto fermo.** Il componente VST3 può non avere ancora elaborato le modifiche del suo controller. La sola serializzazione nativa può quindi salvare valori precedenti, anche dopo aver invalidato la cache. Il nuovo involucro `FGAHOST1:` conserva blob nativo e valori normalizzati dei parametri automatizzabili, identificati per ID; il ripristino riapplica quei valori. Lo usa anche la clonazione per export, senza eseguire DSP per salvare. I blob precedenti restano leggibili. Il salvataggio esplicito acquisisce uno stato fresco; le modifiche strutturali con editor aperto pure. I gesti di volume/pan continuano a riusare la cache. La distruzione programmatica dell’editor invalida la cache come la chiusura manuale.

## Compatibilità

I vecchi progetti migrano il singolo insert nello slot 1. Catene, nuovi stati VST/AU, più di otto corsie parametri o automazioni native richiedono il **formato progetto 4**; conservare una copia del progetto precedente per poterlo riaprire con v0.29.0. Gli altri progetti mantengono il formato precedente dove possibile. Gli ID delle tracce, la generazione MIDI e gli strumenti interni restano quelli esistenti.

Gli insert sono per traccia. Non sono aggiunti insert esterni a gruppi/master né ingressi sidechain audio esterni. I plugin operano nel processo dell’app e devono corrispondere alla sua architettura; i plugin commerciali specifici richiedono una prova effettiva.

## Verifiche e distribuzione

Gli esiti finali, i log, le immagini native e il pacchetto di prova sono raccolti in `/workspace/reports/effects-chain-2026-10-30/`. Le prove sono locali Linux: Windows e macOS richiedono la CI successiva alla pubblicazione dei sorgenti autorizzata dall’utente. La CI include una verifica specifica degli effetti sui tre sistemi senza richiedere l’intera suite notturna.

## Ripristino

Nella cartella del report, `forward.patch` e `reverse.patch` rappresentano l’intervento completo rispetto al commit base. La ricostruzione avanti/indietro viene verificata su un indice Git temporaneo, senza alterare l’indice del lavoro. Conservare eventuali modifiche successive e controllare prima `git apply --check /percorso/reverse.patch`; applicarla solo se il controllo riesce. Non usare reset forzati. La patch elimina anche i nuovi file introdotti da questo intervento.

Per l’app precedente reinstallare v0.29.0 e usare la copia precedente dei progetti. Non tentare di aprire un file in formato 4 con la versione precedente. Il commit/tag della release non viene riscritto; un eventuale commit, push o rilascio di ripristino richiede a sua volta il via libera.

## Modifiche dell’utente integrate

La base è stata aggiornata al commit dell’utente `89e67e3` del 30 ottobre, già rilasciato come v0.29.0. Sono preservati i pulsanti dimensionati sul testo tradotto, le etichette lunghe in inspector/mixer, titoli di clip e sezioni, tooltip dei nomi completi, armonia e testi delle azioni disabilitate. I file del tema e dei controlli condivisi dall’app corrispondono esattamente a quel commit; le due sovrapposizioni nei changelog mantengono sia la sezione 0.29.0 dell’utente sia le nuove voci della versione 0.30.0. Il rollback finale torna alla base 0.29.0 e conserva queste modifiche. Una copia precedente all’integrazione è nel report esterno.

Anche le nuove righe effetti e i nomi nel selettore automazioni seguono la preferenza dell’utente: prima riducono la scala del testo, con tooltip che conserva nome completo e plugin originale.

Le curve attive riapplicano il valore anche dopo modifiche manuali nel VST, richiamo di stato o riattivazione del bypass. I parametri discreti seguono gli stati dichiarati dal plugin, invece di inviare ripetutamente valori intermedi.

## Esiti locali finali

Verifiche ripetute sulla base dell’utente v0.29.0:

- Compilazione Release e CTest **7/7 PASS**. Include insert mono, ordine/ID non contigui, bypass, somma delle latenze, automazioni separate, rimozione selettiva, controlli nativi e filtro dei campioni.
- VST3 effetto reale: più istanze, riordino, rinomina/Undo, ricollegamento, rimozione/Undo, editor in primo piano, nomi conclusi al cambio traccia, ricerca e riuso delle corsie: PASS.
- Salvataggio a editor aperto e trasporto fermo, ripristino dei valori ancora in coda e relativo export privato: PASS.
- Verifica PCM indipendente di dieci export: tutti i campioni corrispondono al risultato previsto; il bypass produce silenzio.
- VST3 strumento reale: nota 441 Hz, rapporto gain 3, ripristino e clone privato 1:1, riapertura/centramento editor: PASS.
- Interfaccia completa e audio: 13 stem allineati, WAV/MP3, mix muto silenzioso e differenza udibile tra rigenerazioni: PASS.
- AddressSanitizer con controllo perdite: tutti i test del core superati. Non è una verifica ASan dei plugin commerciali o dell’intera GUI.
- Tema/font condivisi, guide IT/EN, localizzazione, sintassi del workflow e packaging controllati.

Le immagini del report mostrano plugin di prova, usati per misurare il percorso audio. Le immagini e il pacchetto locali precedono la pubblicazione. Gli esiti Windows/macOS e Linux della versione 0.30.0 sono nella CI della release.

Per annullare l’intera versione pubblicata, da un checkout pulito preparare `git revert --no-commit v0.30.0`, controllare il diff e ricompilare prima di autorizzare un commit di ripristino. Il commit dell’utente 0.29.0 resta nella cronologia.
