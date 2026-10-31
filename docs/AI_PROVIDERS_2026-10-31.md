# Aggiornamento composizione IA — 31 ottobre 2026

Stato: aggiornamento preparato per la release **0.30.5**, pubblicazione autorizzata dall’utente. Base: `4cc939347eda02df7868d8d59977ffc5618c8595` (main).
Versione applicativa: **0.30.5**. La pubblicazione usa la CI e i quattro pacchetti della famiglia.

## Modifiche

- Aggiunti DeepSeek, Groq e OpenRouter Free. Gli ID di preferenza dei quattro
  servizi esistenti rimangono invariati: OpenAI 0, Anthropic 1, Gemini 2, Ollama 3;
  i nuovi sono rispettivamente 4, 5 e 6. Nessuna modifica al formato dei progetti.
- Aggiornati i suggerimenti alle famiglie documentate al 31 ottobre 2026:
  GPT-6.1/6, Claude 5.5/Fable 5.1, Gemini 3.8/3.5/4, DeepSeek Flash/V4 Pro,
  GPT-OSS e Qwen su Groq. I suggerimenti non garantiscono accesso o gratuità.
- AGGIORNA legge i cataloghi cloud ufficiali, con filtri per modelli di testo,
  deduplicazione e paginazione Gemini/Anthropic. L’ID cloud rimane modificabile;
  un errore di catalogo conserva i suggerimenti e il testo precedente. Un valore
  vuoto nelle preferenze torna al primo suggerimento. Ollama conserva il
  rilevamento dei modelli locali installati e non richiede una chiave.
- OpenRouter Free propone il router gratuito e varianti `:free` con prezzi
  dichiarati nulli per input/output. Rifiuta ID a pagamento; non configura
  ripieghi a pagamento o strumenti remoti. Il router resta selezionabile anche
  dopo l’aggiornamento delle varianti. Groq richiede fino a 4096 token di output,
  per ridurre la pressione sulle quote gratuite; DeepSeek usa output senza
  thinking per il compito JSON MIDI.
- Chiavi solo in memoria e separate per servizio; gli indirizzi cloud sono
  fissi HTTPS. Cursori di catalogo trattati come dati, mai indirizzi a cui inviare
  credenziali. Rete annullabile, reindirizzamenti disattivati, risposte limitate;
  errori online redatti e nessun catalogo parziale pubblicato dopo un errore.
- Istruzioni del servizio nell’anteprima, pulsante GUIDA IA e guida sopra il
  compositore. Aggiunta sezione dedicata IT/EN: configurazione per servizio,
  quote gratuite, esempi, errori, privacy e diritti di terzi. Aggiornati screenshot,
  README, architettura, privacy e licenze/attribuzioni incluse nell’app.

## Verifiche e limiti

Evidenze locali: `/workspace/reports/ai-providers-2026-10-31/`.

Comandi principali:

```sh
cmake --build build-app --target FaseGrooveArranger fga_io_tests -j3
ctest --test-dir build-app --output-on-failure
python3 scripts/test-ai.py --build-dir build-app --ui-output /percorso/evidenze
python3 scripts/verify-family.py
python3 ThirdParty/fasegroove-common/VerifyModule.py
python3 ThirdParty/fasegroove-common/CheckTranslations.py
python3 tests/test-packaging.py
git diff --check
```

I test coprono richieste/risposte dei sette servizi, isolamento delle credenziali,
origini HTTPS, modelli gratuiti, cataloghi paginati/duplicati/errati, annullamento,
errori di quota/credito e output troncati. Il server HTTP locale simula risposte
Ollama ma usa il trasporto reale; i cataloghi cloud sono verificati con risposte
simulate deterministiche. La UI nativa viene esercitata in italiano e inglese,
con layout 920×700 e 960×800, selezione dei modelli, aiuto e flusso locale fino
all’anteprima MIDI. I log riportano gli esiti effettivi.

Non sono disponibili chiavi personali dei sette servizi: nessuna inferenza cloud
reale o spesa API è stata eseguita. Il catalogo pubblico OpenRouter è stato
consultato senza autenticazione; i collegamenti ufficiali verificati sono
registrati in `official-links.json`. Build e UI locali: Linux; le compilazioni
Windows/macOS richiedono la CI prima della futura release. Il risultato musicale
rimane dipendente dal modello, dai suoi limiti e dall’account dell’utente.

## Ritorno indietro

Prima di pubblicare viene preparato un archivio locale con `changes.patch`,
`rollback.patch`, hash dei file ed evidenze. Per annullare solo queste modifiche
su un albero corrispondente alla versione verificata:

```sh
git apply --check /percorso/rollback.patch
git apply /percorso/rollback.patch
```

Il report esterno contiene il controllo di andata/ritorno delle patch su un
indice Git temporaneo. Non usare reset distruttivi sulle modifiche successive.
Dopo un eventuale commit pubblicato, preferire `git revert` di quel commit,
verificando i conflitti. Le nuove preferenze `aiModel4`–`aiModel6` possono restare
sul computer: una versione precedente le ignora. Tornando a una versione
precedente, riselezionare il servizio IA desiderato nelle preferenze.

Il ripristino del codice non annulla composizioni già applicate/salvate: per
quelle usare Annulla nella sessione o una copia precedente del progetto.

## Fonti e attribuzioni

L’articolo AIREITER indicato dall’utente è uno spunto introduttivo; endpoint,
modelli e disponibilità gratuita sono stati controllati sulle fonti ufficiali:

- https://aireiter.com/it/blog/best-free-ai-api
- https://developers.openai.com/api/docs/models
- https://developers.openai.com/api/docs/guides/latest-model
- https://platform.claude.com/docs/en/about-claude/models/overview
- https://platform.claude.com/docs/en/api/models/list
- https://ai.google.dev/gemini-api/docs/models
- https://ai.google.dev/gemini-api/docs/pricing
- https://ai.google.dev/api/models
- https://api-docs.deepseek.com/quick_start/pricing/
- https://api-docs.deepseek.com/api/list-models/
- https://api-docs.deepseek.com/guides/thinking_mode/
- https://console.groq.com/docs/models
- https://console.groq.com/docs/rate-limits
- https://console.groq.com/docs/structured-outputs
- https://openrouter.ai/docs/guides/routing/model-variants/free
- https://openrouter.ai/docs/api/reference/limits
- https://openrouter.ai/api/v1/models

Copyright, marchi, termini di servizio e licenze dei modelli rimangono dei
rispettivi titolari. Nessuna affiliazione implicita, modello distribuito o
contenuto dell’articolo copiato. Dettagli in THIRD_PARTY_NOTICES.md e nelle guide.
