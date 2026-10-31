# Plugin assenti e WaveShell — 31 ottobre 2026

Aggiornamento preparato per la release **0.30.6**, pubblicazione autorizzata dall’utente. Base: `f6282c92d8a8b0328e779d8ea3278050ac973fab` (release 0.30.5).

## Riscontri

La ricerca esistente include sottocartelle e collegamenti, con protezione dai
cicli. JUCE 8.0.9 enumera già più classi VST3 per contenitore: il supporto ai
WaveShell non è completamente assente.

Sono stati individuati limiti che possono far mancare intere famiglie o plugin
aggiunti successivamente:

- Un unico limite di 60 secondi per ogni file, anche per un WaveShell con molti
  plugin. Il processo viene terminato e il contenitore rimane fra gli scartati.
- La scansione manuale riutilizzava le liste dei file invariati, anche se i
  plugin dietro il contenitore erano cambiati. «Riprova errori» non aggiorna le
  liste già segnate come riuscite.
- Importando il solo KnownPluginList, i plugin dello stesso contenitore erano
  trattati come record separati; una scansione successiva poteva duplicarli.
- Gli scarti non conservavano il motivo; una risposta senza descrizioni valide
  poteva essere memorizzata come riuscita.

Questi sono riscontri sul codice e sui test. Senza installazione/catalogo del
Windows dell’utente non si può stabilire quale sia la causa effettiva dei suoi
Waves assenti o elencare tutti gli altri prodotti mancanti. I Waves commerciali
non sono installati nell’ambiente di verifica.

## Modifiche

- Avvio con scansione incrementale e cache, come prima. SCANSIONA manuale nel
  browser e nelle impostazioni esegue una verifica completa. RIPROVA ERRORI
  mantiene la funzione limitata agli scarti.
- WaveShell e contenitori già riconosciuti con più classi: fino a 10 minuti per
  file; file ordinari: 60 secondi. Annullamento e isolamento restano attivi.
- Le vecchie cache shell vengono rivalutate una volta con la nuova politica.
  L’XML rimane versione 2: attributi aggiuntivi `scanPolicy`/`issue`, ignorati
  dalle versioni precedenti. KnownPluginList e progetti conservano il formato.
- Raggruppamento corretto delle classi importate dal KnownPluginList.
- REPORT SCANSIONE apre una vista locale e salva ScanReport.txt quando possibile:
  cartelle configurate, plugin riconosciuti per contenitore e scarti con motivo
  (timeout, avvio fallito, annullamento/interruzione, codice uscita o risultato
  non valido). Le vecchie cause sconosciute vengono indicate come tali.
- Istruzioni aggiornate IT/EN e nuove stringhe tradotte nelle sei lingue.

## Verifica

La verifica nativa dello scanner usa un vero VST3 di prova e un manifest VST3
multiclasse sintetico, non un binario Waves. Riproduce una lista in cache
incompleta con wrapper invariato, verifica il recupero con scansione completa,
la migrazione, l’importazione del KnownPluginList senza duplicati, gli scarti e
il relativo report. Controlla i limiti configurati senza attendere dieci minuti.
La CI esegue questa verifica anche per le release Windows/macOS/Linux, oltre alle
scansioni notturne/su richiesta. Conserva le verifiche di cartelle annidate, collegamenti/cicli, rimozione,
aggiornamento, cache incrementale e ripetizione degli errori.

Log, screenshot, hash e patch: `/workspace/reports/plugin-scan-2026-10-31/`.
I log riportano gli esiti effettivi. La prova con Waves e licenze reali deve
avvenire sul computer dell’utente; Windows/macOS richiedono la CI prima di una
release. Commit, push e pubblicazione di questo aggiornamento sono stati autorizzati dall’utente.

## Formati e riferimenti

Questa build ospita VST3 nativi e, su macOS, Audio Units. Non ospita VST2 DLL,
AAX o CLAP e non offre un bridge per binari di architettura diversa.

Waves usa un WaveShell nella cartella VST3 per esporre i propri contenuti; le
cartelle Plug-Ins e i singoli .bundle non sono VST3 indipendenti. Per installazione
e formati fa riferimento la documentazione ufficiale:
https://www.waves.com/support/how-to-use-waves-plugins-when-using-custom-vst-folder

## Ripristino

Prima dell’eventuale commit, la cartella delle evidenze contiene changes.patch e
rollback.patch. Dalla repository verificare `git apply --check rollback.patch`
prima di applicarla. Dopo un eventuale commit pubblicato, preferire il suo
`git revert`, verificando le modifiche successive. La cache XML versione 2 resta
leggibile dalla versione precedente; non cancellare progetti o licenze Waves.
Il codice ripristinato non modifica strumenti, MIDI e assegnazioni già salvati.
