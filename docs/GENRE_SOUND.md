# Suoni e ruoli per genere — 0.4.7

La 0.4.4 lasciava quasi tutti gli strumenti sullo stesso suono guida, kit e livello. Il basso aveva anche il panorama per nota di una tastiera: le note basse finivano a sinistra. La 0.4.6 configura i motori originali della famiglia per ruolo e stile.

| Genere | Tastiera | Kit | Ruolo |
|---|---|---|---|
| Piano House | Soulful Grand | House 909 | tema di piano e risposte brevi |
| Soulful | Rhodes (FM) | Disco Live | accordi caldi, basso hold/push, chitarra jazz negli spazi |
| Deep | House Organ scuro | Deep 808 | basso sub, voicing rootless e archi alti |
| Gospel | Gospel Organ (Leslie) | Disco Live | voicing aperti, basso walking, risposte degli archi |
| Jackin’ | Piano acustico a chops | House 909 | hook di due battute, basso elettrico corto/swingato, niente archi automatici, chitarra percussiva a battute alterne |
| Jackin’ Piano | House Piano (M1) | House 909 | triadi corte, attacco brillante |
| Garage & UK | Rhodes tagliato | Deep 808 | basso 2-step, stabs brevi e shuffle |
| Afro & Latin | Wurlitzer scuro | Disco Live | nylon, shaker/conga in rilievo, call/response |
| Funky & Disco | Wurlitzer | Disco Live | chitarra Disco Chorus SOLISTA, archi disco |
| ’90s Piano House | House Piano (M1) | House 909 | triadi e ottava alta, piano in primo piano |
| Neo Soul & Broken | Rhodes scuro | Lo-Fi Soul | chitarra jazz, feel morbido e accordi estesi |
| Chill & Pads | Warm Pad | Lo-Fi Soul | attacco lungo, nylon e archi larghi |
| Dance Stabs | House Piano (M1) | House 909 | triadi superiori corte e batteria incisiva |

Le famiglie vicine possono condividere un sintetizzatore originale: non sono tredici librerie campionate. Timbro, inviluppo MIDI (gate), voicing, estensione, fraseggio, effetti, kit e rapporti di livello differenziano le palette. I suoni rimangono sintetizzati dai motori Piano/Drums/Guitar, senza campioni non autorizzati o nuovi strumenti esterni.

Il basso guida usa la stessa sintesi del Piano con uscita centrata; un passa-basso segue il controllo TIMBRO. Riverbero e chorus del basso partono spenti. Il filtro non viene applicato a campioni o VST personali. Gli archi restano l'ensemble originale, con registro, fraseggio e timbro diversi.

Nel corpo del brano le chitarre di accompagnamento rispondono al piano e gli archi/percussioni entrano per frasi. La cassa, il backbeat e l'hat chiuso non vengono troncati dall'orchestrazione aggiuntiva. Le finestre DJ di intro/outro restano dentro la canzone. Le pause composte sono note assenti nel MIDI, non un'automazione volume nascosta. Pattern utente e parti bloccate sono conservati; le note scritte a mano cambiano solo quando rigeneri il relativo clip.

Scegliere il genere applica la sua palette ai suoni interni sbloccati e prepara i parametri MIDI. Campioni/VST, percorsi, stato, guadagni e panorama personali restano assegnati. I progetti già salvati conservano timbri e mix finché non scegli nuovamente un genere; il miglioramento del basso guida centrato/filtrato riguarda invece il renderer interno. I nuovi brani partono con rapporti di traccia calibrati e master 0,45; i vecchi guadagni master restano salvati.

## Verifica e ascolto

I test controllano tredici palette distinte, basso asciutto/centrato, preservazione degli strumenti personali, scope, blocchi e struttura per genere. L'audit nativo renderizza i tredici brani interi e genera `preview-0.mp3` … `preview-12.mp3`: le prime otto battute del primo drop effettivo, più la coda. Su Soulful, Deep, UK e Disco controlla anche il basso stereo (L = R con il preset asciutto).

Picchi finiti, assenza di clipping e differenze spettrali non dimostrano da soli che una canzone piaccia. Le anteprime servono al confronto all'ascolto; i preset sono una base modificabile, non un master da distribuzione. Per confrontare con la 0.4.4 usa lo stesso passaggio e un volume percepito equivalente.

La 0.4.6 corregge inoltre il bilanciamento della batteria: il motore Drums applica già guadagni diversi per cassa, snare e piatti. I livelli di traccia tengono conto di questa calibrazione, evitando una seconda forte attenuazione della parte alta del kit. Il master dei nuovi progetti è 0,45 per lasciare margine anche alla codifica MP3. Le anteprime vengono decodificate e controllate anche come true peak.

## Jackin’ — correzione 0.4.7

Il profilo precedente lasciava gli archi su stabs anche in tutti i drop e il basso su un organo filtrato con gate 0,55. La sola etichetta Jackin’ e lo swing non bastavano a dare un’identità all’insieme. Ora la selezione automatica del piano usa quattro hook a chops della libreria originale: Disco Jack Chops, Jack Chops, Chicago Stab Jack e Jack Stab Skips. Il gate del piano è 0,18, lo strum è zero e l’ambiente è quasi asciutto. Il piano usa Soulful Grand; il basso usa il Wurlitzer originale più aperto, gate 0,30 e la linea Funky Octave Skip già presente nel motore, con ottave e ghost note, nel registro basso voice-led originale (non il registro della tastiera).

Gli archi partono spenti (si possono riattivare), la chitarra risponde a battute alterne con Chicago Chank della libreria originale, scratch e accordi palm muted. Cassa in quattro, snare/clap e hat conservano il generatore Drums e lo swing del genere. Jackin’ Piano e gli altri undici generi non cambiano. Pattern espliciti, note editate e blocchi conservano le regole esistenti.

Riferimenti di produzione consultati, senza importare campioni, MIDI o audio:
- [Attack: Simple Jackin’ House](https://www.attackmagazine.com/technique/beat-dissected/simple-jackin-house/) — accenti, hat aperto, shaker e shuffle.
- [BandLab: How to make a jackin’ house beat](https://blog.bandlab.com/how-to-make-a-jackin-house-beat-on-bandlab/) — radici funk/disco, basso ripetitivo e fondamento ritmico.
- [Scott Diaz / Loopmasters: Jackin House Essentials](https://www.loopmasters.com/genres/93-Bass-House/products/1640-Jackin-House-Essentials) — basso funky e riff filtrati/tagliati.

Questa è una traduzione compositiva per i motori interni, non una ricostruzione dei dischi degli artisti. L’audit esporta anche il brano Jackin’ completo e tre anteprime separate (batteria, basso e tastiera), per valutare il groove e l’interazione oltre ai controlli tecnici.

Il riferimento indicato dall’utente è **Da Funk Junkies**. La [presentazione ufficiale di 3rd Room Records](https://3rdroomrecords.com/da-funk-junkies) descrive un Jackin’ intriso di disco, basso corposo e chitarre propulsive. Da qui la scelta compositiva di ottave/ghost del basso, piano tagliato e chank ritmico, anziché un organo sub con archi sovrapposti. È un orientamento di stile; non viene dichiarata una corrispondenza sonora verificata ai master dell’artista.
