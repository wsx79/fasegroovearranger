# Suoni e ruoli per genere — 0.4.5

La 0.4.4 lasciava quasi tutti gli strumenti sullo stesso suono guida, kit e livello. Il basso aveva anche il panorama per nota di una tastiera: le note basse finivano a sinistra. La 0.4.5 configura i motori originali della famiglia per ruolo e stile.

| Genere | Tastiera | Kit | Ruolo |
|---|---|---|---|
| Piano House | Soulful Grand | House 909 | tema di piano e risposte brevi |
| Soulful | Rhodes (FM) | Disco Live | accordi caldi, basso hold/push, chitarra jazz negli spazi |
| Deep | House Organ scuro | Deep 808 | basso sub, voicing rootless e archi alti |
| Gospel | Gospel Organ (Leslie) | Disco Live | voicing aperti, basso walking, risposte degli archi |
| Jackin’ | Wurlitzer | House 909 | gate corto, basso skip e riprese percussive |
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

Scegliere il genere applica la sua palette ai suoni interni sbloccati e prepara i parametri MIDI. Campioni/VST, percorsi, stato, guadagni e panorama personali restano assegnati. I progetti già salvati conservano timbri e mix finché non scegli nuovamente un genere; il miglioramento del basso guida centrato/filtrato riguarda invece il renderer interno. I nuovi brani partono con rapporti di traccia calibrati e master 0,55; i vecchi guadagni master restano salvati.

## Verifica e ascolto

I test controllano tredici palette distinte, basso asciutto/centrato, preservazione degli strumenti personali, scope, blocchi e struttura per genere. L'audit nativo renderizza i tredici brani interi e genera `preview-0.mp3` … `preview-12.mp3`: le prime otto battute del primo drop effettivo, più la coda. Su Soulful, Deep, UK e Disco controlla anche il basso stereo (L = R con il preset asciutto).

Picchi finiti, assenza di clipping e differenze spettrali non dimostrano da soli che una canzone piaccia. Le anteprime servono al confronto all'ascolto; i preset sono una base modificabile, non un master da distribuzione. Per confrontare con la 0.4.4 usa lo stesso passaggio e un volume percepito equivalente.
