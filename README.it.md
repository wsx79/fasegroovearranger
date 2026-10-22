<p align="center"><img src="Resources/arranger.png" alt="Fase Groove Arranger" width="160"></p>

<h1 align="center">Fase Groove Arranger</h1>

<p align="center">
  <b>Il brano completo con i motori di Piano, Drums e Guitar, in un'unica app standalone.</b><br>
  di <b>Fabio 'Pitch' Serra</b> · Windows, macOS &amp; Linux · gratuito e open source
</p>

<p align="center"><a href="README.md">🇬🇧 English</a> · <b>🇮🇹 Italiano</b></p>

<p align="center">
  <a href="docs/GUIDA.md"><b>📖 Guida di utilizzo</b></a> ·
  <a href="CHANGELOG.it.md">🆕 Novità</a> ·
  <a href="#compilare-e-scaricare">⬇️ Download</a> ·
  <a href="#ecosistema-e-crediti">☕ Sostieni il progetto</a> ·
  <a href="PRIVACY.it.md">Privacy</a> · <a href="LICENSE">Licenza</a>
</p>

---

**[Scarica l’ultima release](https://github.com/wsx79/fasegroovearranger/releases/latest).** I pacchetti verificati per Windows, macOS universal e Linux sono nella sezione **Assets**. È un’applicazione iniziale di sviluppo.

Fase Groove Arranger **crea MIDI** per un brano completo negli stessi 13 generi della famiglia. Scegli genere, tonalità, giro e groove; ascolta con i suoni guida, modifica le note e assegna i tuoi campioni o strumenti VST3/AU. I motori originali sono già inclusi: non occorre installare i singoli plugin.

![Fase Groove Arranger](docs/images/guide_arrangement.png)

La vista DAW comprende inspector laterale, mixer a 17 canali con master, righello e locatori di loop, clip indipendenti con divisione al cursore, taglia/copia/incolla, duplicazione, eliminazione, spostamento e selezione multipla, adattamento dell’intero brano e scorrimento in riproduzione. La struttura in alto resta una guida modificabile: le sue modifiche conservano le posizioni MIDI.

Lo zoom segue il puntatore (rotella sul righello o Ctrl/Cmd+rotella); Maiusc+rotella scorre in orizzontale. Z inquadra la selezione e Ctrl/Cmd+L la mette in loop. Nell’editor MIDI trovi anche spostamento con le frecce, passi di ottava, taglia, duplica e incolla al cursore.

## Funzioni

- 17 tracce: Kick, Snare, Clap, Rim, Closed Hat, Open Hat, Ride, Shaker, Tambourine, Conga, Cowbell, Tom, Crash, Basso, Piano, Archi e Chitarra. La chitarra conserva le sue 12 parti in una sola traccia.
- Arrangiamenti secondo genere e groove: Intro, Main, Breakdown, Build-up, Drop e Outro. La durata deriva dalle frasi musicali; le sezioni restano modificabili.
- Generazione di brano, traccia, sezione o singolo clip; blocchi di generazione, annulla/ripeti e MIDI salvato nel progetto.
- Pannelli strumenti e piano roll grande separati: ogni batteria mostra solo lo strumento scelto, la chitarra tutte le sue parti. Il piano mantiene i controlli estesi, DNA, Morph, Voicing Coach, Varianti e circolo delle quinte.
- Disegno, spostamento, durata e dinamica delle note, selezione multipla, quantizzazione, trasposizione e copia/incolla. Importazione MIDI PPQ.
- Suoni guida originali, campioni personali e uno strumento VST3 per traccia; Audio Units su macOS. Editor e stato del plugin salvati nel progetto.
- Progetti `.fga`, salvataggio atomico e autosave locale; volume, pan, mute, solo, meter e loop di brano o sezione.
- Export MIDI multitraccia o separato, WAV stereo 24 bit e MP3 stereo 320 kbps, mix completo o singoli stem. L'encoder MP3 è incluso.
- Sei lingue e tooltip, pannello OPZIONI con IMPOSTAZIONI / INFO, dimensione interfaccia e verifica aggiornamenti GitHub disattivabile.
- Guide in italiano e inglese, privacy, licenze e crediti della famiglia.

## Primo brano

1. Scegli genere, tonalità, giro e groove, poi **NUOVO ARRANGIAMENTO**.
2. Clicca il nome per selezionare la traccia; l’ingranaggio ambra accanto a Solo apre i parametri dello strumento. Doppio clic sul clip per il piano roll.
3. Usa **NUOVA TRACCIA**, **NUOVA SEZIONE** o **NUOVO CLIP** per un'altra take. Blocca ciò che vuoi conservare.
4. Scegli un suono guida, un campione o **CARICA VST3 / AU**. **APRI VST** apre i suoi controlli.
5. Salva il progetto `.fga`, poi usa **ESPORTA** per MIDI, WAV o MP3.

I parametri di generazione valgono per la prossima generazione. Il MIDI già scritto resta modificabile; il tempo cambia subito la riproduzione.

## Compilare e scaricare

CMake 3.22+, compilatore C++17 e JUCE **8.0.9** (scaricato in fase di build, oppure `-DJUCE_DIR=/percorso/JUCE`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j2
ctest --test-dir build -C Release --output-on-failure
```

Scarica da [Releases](https://github.com/wsx79/fasegroovearranger/releases/latest): **FaseGrooveArranger-Windows-Setup.exe** usa lo stesso installer italiano/inglese della famiglia, con menu Start e disinstallazione. Ci sono gli ZIP portatili per Windows, macOS universale e Linux, uno ZIP per piattaforma. **OPZIONI → IMPOSTAZIONI → VERIFICA ORA** scarica l’aggiornamento; **INSTALLA ORA** esegue il setup o sostituisce la copia portatile scrivibile e riapre l’app. Progetti e preferenze restano. La 0.7.0 e le precedenti richiedono una prima installazione manuale. Dettagli nella [guida](docs/GUIDA.md#1-installazione). Il setup Windows non è firmato; macOS usa la firma ad hoc/Developer ID opzionale della famiglia. Build di sviluppo anche in Actions → Artifacts.

Per provare i soli motori: `bash scripts/test-core.sh` oppure CMake con `-DFGA_BUILD_APP=OFF`.

## Guide e privacy

- [Guida italiana](docs/GUIDA.md) · [English guide](docs/GUIDE.md)
- [Privacy e permessi](PRIVACY.it.md) · [Privacy](PRIVACY.md)
- [Architettura e provenienza](docs/ARCHITECTURE.md) · [Confronto UI famiglia](docs/FAMILY_ALIGNMENT.md)
- [Licenze di terze parti](THIRD_PARTY_NOTICES.md) · [Verifiche](docs/VALIDATION.md)

Questa versione è una workstation MIDI in sviluppo. Registrazione/editing audio, automazioni di parametri diversi dal volume traccia, audio warping, catene effetti, bus/send, registrazione dei controller MIDI, compensazione della latenza dei plugin e scansione isolata non sono implementati. I plugin girano nel processo dell'app e richiedono l'architettura corretta; la compatibilità va provata con i plugin effettivi.

I suoni guida sono sintetizzati, non librerie commerciali campionate. Per la produzione finale puoi usare i tuoi strumenti. WAV è senza perdita; MP3 aggiunge il ritardo/padding del codec.

## Ecosistema e crediti

[Fase Groove Piano](https://github.com/wsx79/fasegroovepiano) · [Fase Groove Drums](https://github.com/wsx79/fasegroovedrums) · [Fase Groove Guitar](https://github.com/wsx79/fasegrooveguitar)

Creato da **Fabio 'Pitch' Serra / FS Pitch — DJ & Producer**. Assistenza allo sviluppo: **Claude (Anthropic)** e **Codex (OpenAI)**. JUCE, font Inter (SIL OFL) e componenti elencati negli avvisi inclusi. Software libero **AGPL-3.0-or-later**.

[Ko-fi](https://ko-fi.com/fabios1979) · [PayPal](https://paypal.me/fabios79) · [Spotify](https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP)

[Arrangiamenti da club: fonti, scelte musicali e verifica](docs/CLUB_ARRANGEMENT.md).
