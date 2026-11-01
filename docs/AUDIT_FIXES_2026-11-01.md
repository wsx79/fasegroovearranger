# Verifica e interventi sul report del 1 novembre 2026

Base: main 7171481, versione 0.31.1. Report PDF letto come analisi da verificare, non come prova di esecuzione. Commit, push e release 0.31.2 successivamente autorizzati dall’utente.

- B1 confermato: eliminato il limite fisso del lead elettronico; intro/outro seguono la politica comune di ingresso scalata sulla durata.
- B2/B3 confermati: swing solo sui sedicesimi dispari, quinta dall'accordo effettivo; stesso helper armonico di Techno/Electro, inclusi accordi liberi.
- B4 confermato: esempio IA e fixture Techno usano ID 13, preservando ID 12 Dance Stabs.
- B5/B6: eliminati accessi non protetti a front() e al plugin nel self-test. Il test registra comunque FAIL quando mancano le note/editor.
- B7: i generatori elettronici definiscono già le pause delle percussioni; esclusa la seconda potatura di shapeBandRoles, mantenendo la politica comune intro/outro.
- B8/varietà: timing Electro con swing udibile, jitter di pochi millisecondi; basso con otto celle, lead con rotazioni, frase/risposta e sette intervalli, otto progressioni Electro. Hat, clap, open hat, ride e pickup kick variano con il motivo. Il kick in quarti resta l'ancora del genere. Techno mantiene la base armonica minimale; nessun DSP rumble aggiunto sotto l'etichetta di variazione MIDI.
- Manutenzione: helper unici per accordo, armonia e feel elettronici; enum con ID stabili; controlli statici sulle tabelle principali. Audit v0.13 annotato come storico senza rompere collegamenti.

I motori vendorizzati, widget condivisi e updater non sono stati sostituiti: le copie incorporano adattamenti documentati e una migrazione va coordinata tra prodotti e validata nelle rispettive repo. insertGuide resta una API pubblica coperta da test; l'assenza di un chiamante UI non basta a giustificarne la rimozione. R7 resta in pausa come indicato nel PDF. Non si modifica il thread audio.

Le modifiche musicali riguardano la generazione successiva: i progetti conservano le note salvate. Verifiche e patch inversa sono registrate in /workspace/reports/audit-2026-11-01. Per il ripristino usare prima git apply --check sulla patch inversa e applicarla solo senza conflitti; non forzare sopra ulteriori modifiche. Per il risultato musicale usare una copia del progetto e Annulla durante la sessione.

## Verifiche eseguite

Build Release Linux dell'app e dei target di test riuscita. Sette suite CTest superate: core, MIDI, MP3, progetto, updater, audio engine e sample browser. Le nuove regressioni misurano varietà delle posizioni ritmiche con humanize spento, ingresso Techno nelle intro di 16 battute, swing che lascia fermi gli on-beat e quinta diminuita effettivamente generata.

Prova nativa UI ed export riuscita: 13 stems attivi e allineati, controllo mute, WAV/MP3 e rigenerazioni udibilmente diverse. Famiglia grafica, guide e traduzioni verificate. Non è una certificazione estetica dei nuovi motivi: ascoltarli nella propria produzione resta utile. Windows/macOS e pubblicazione restano da eseguire dopo l'autorizzazione. Nessun codice dei motori vendorizzati è stato modificato.
