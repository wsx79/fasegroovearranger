# Credits, licences and third-party notices

## Fase Groove ecosystem

Copyright (C) 2026 Fabio 'Pitch' Serra / FS Pitch. Application and original generation engines: **GNU Affero General Public License version 3 or later**, full text in `LICENSE` and in the original source folders.

Creator: **Fabio 'Pitch' Serra — DJ & Producer**. Development assistance: **Claude (Anthropic)** and **Codex (OpenAI) — AI coding assistants**. The original FS PITCH artist logo, Inter theme and artist/donation links are retained. Source provenance and licences for each component are listed below.

Original sources, copied with copyright and licence notices:

- https://github.com/wsx79/fasegroovepiano at `026f0451d683f25520409f8dd273ff5a4496cad0`
- https://github.com/wsx79/fasegroovedrums at `7f1def6a4e4cb72ed887f7754171affec12af001`
- https://github.com/wsx79/fasegrooveguitar at `e571f8d02621c65adeb35207214771571499fc65`

- https://github.com/wsx79/fasegroovelead at `07b33a3689b9986ae372c64cc70ceeaa47ff938b` (v0.1.0)

The vendored engines are kept in step with later plugin commits by `scripts/sync-engines.py`: the commit of each last sync is in `ThirdParty/ENGINE_VERSIONS`, the Arranger's own changes in `ThirdParty/ENGINE_PATCHES.md`.

See `ThirdParty/*/LICENSE`, original notices and `docs/ARCHITECTURE.md`. The original third-party documentation is retained for provenance; this app's privacy and guide are the documents in the repository root and `docs/`.

## JUCE

JUCE 8.0.9 — https://github.com/juce-framework/JUCE. Copyright JUCE contributors / Raw Material Software. JUCE is available under AGPLv3 and commercial terms. This source distribution uses the open-source AGPL option. JUCE includes its own platform/component notices, available in the fetched/local JUCE source. Keep the corresponding source and notices when distributing binaries.

## Inter

Inter Regular and SemiBold — Copyright The Inter Project Authors. **SIL Open Font License 1.1**, full text in `Resources/Inter-OFL.txt`. Fonts are embedded without modification.

## Shine MP3 encoder

Shine — https://github.com/toots/shine at `ab5e3526b64af1a2eaa43aa6f441a7312e013519`. Original work/history credits include Gabriel Bouvigne, Pete Everett, Patrick Roberts and the Shine contributors; see `ThirdParty/shine/README.md`, `README.old` and source files. The library's full licence is the **GNU Library General Public License version 2**, in `ThirdParty/shine/COPYING`. Its source is included; it is linked into the AGPL application. The complete application source and build instructions allow rebuilding/relinking with a modified library. Do not remove these notices or restrict the user's applicable library modification/relinking rights.

Shine is a simple fixed-point encoder without a psychoacoustic model. The app uses 320 kbps stereo for sharing; WAV is the lossless production format.

## Trademarks and user content

VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units, macOS and Windows belong to their respective owners. Naming a format does not imply affiliation or endorsement.

No proprietary plugin binary, commercial sample library or third-party musical recording is bundled. Users provide and license their own instruments and samples. Generated MIDI/audio is not required to be AGPL merely because it was produced by this application; rights in any user-supplied content continue to apply. The software is provided without warranty as set out in its licences.

The six-language UI dictionaries and CircleOfFifths component are adapted from those same family snapshots, under their original AGPL notices. The guitar step engine has an Arranger-specific shared harmonic-duration adapter; the 16-step default retains the plugin behaviour.

Arranger 0.4.5 adds an optional centred-output constructor argument to the vendored Piano `GuideVoice`, used only by its dedicated bass track. Original callers retain keyboard spread by default. The synthesis algorithms remain the original family engines; Arranger applies its own genre presets, mix levels, musical layer scheduling and built-in bass low-pass after synthesis.

Arranger 0.7.2 adds an optional `bassLineOverride` to the vendored Piano settings, renderer and resolved grid. Empty retains the original selected library figure. Arranger develops that figure for explicitly regenerated musical takes; synthesis is unchanged.

## Signalsmith Stretch 1.4.0 and Signalsmith Linear 0.6.4

The sampler uses Signalsmith Stretch for independent tempo and pitch changes.
Copyright Geraint Luff / Signalsmith Audio, MIT license. Sources, version pins and
full license texts are in `ThirdParty/signalsmith-stretch` and
`ThirdParty/signalsmith-linear`. No network service is involved.

## Optional AI services and model rights

Arranger independently integrates the HTTP APIs of OpenAI, Anthropic (Claude),
Google (Gemini / AI Studio), DeepSeek, Groq (GroqCloud), OpenRouter and Ollama.
Their names, logos, services, model names and other protected material remain the
property of their respective rights holders. No affiliation, sponsorship or
endorsement is implied. No provider logo, API key, proprietary SDK or model
weights are bundled by this integration. The application's AGPL licence does
not relicense those services or models. Accounts, API access, availability,
quotas, charges and rights in generated output are subject to the applicable
provider/model terms, not guaranteed by Arranger.

Groq and OpenRouter can serve models created by third parties. For example,
GPT-OSS is by OpenAI and Qwen is by the Qwen/Alibaba project. Other model authors
and licences are identified in the selected model's official catalogue/model
card. Ollama downloads also carry their own model licences. Review those terms
for your intended use; local inference does not transfer ownership of a model.

Official service terms and privacy references:

| Service | Terms / policies | Privacy / data handling |
|---|---|---|
| OpenAI | [Terms / policies](https://openai.com/policies/services-agreement/) | [Privacy / data](https://openai.com/policies/privacy-policy/) |
| Anthropic / Claude | [Terms / policies](https://www.anthropic.com/legal/commercial-terms) | [Privacy / data](https://www.anthropic.com/legal/privacy) |
| Google / Gemini | [Terms / policies](https://ai.google.dev/gemini-api/terms) | [Privacy / data](https://policies.google.com/privacy) |
| DeepSeek | [Terms / policies](https://cdn.deepseek.com/policies/en-US/deepseek-terms-of-use.html) | [Privacy / data](https://cdn.deepseek.com/policies/en-US/deepseek-privacy-policy.html) |
| Groq / GroqCloud | [Terms / policies](https://groq.com/terms-of-use/) | [Privacy / data](https://groq.com/privacy-policy/) |
| OpenRouter | [Terms / policies](https://openrouter.ai/terms) | [Privacy / data](https://openrouter.ai/privacy) |
| Ollama | [Terms / policies](https://ollama.com/terms) | [Privacy / data](https://ollama.com/privacy) |

Free cloud API tiers are limited service offers, not licences to unlimited
inference; their terms can change. OpenRouter also involves the chosen upstream
provider and its data policies. Setup, current model discovery, free-tier
limitations and official pricing links are documented in the dedicated AI
section of `docs/GUIDA.md` and `docs/GUIDE.md`, included in the application.
The AIREITER article https://aireiter.com/it/blog/best-free-ai-api is credited as
the user's introductory reference; provider documentation was checked directly
on 2026-10-31. The guide text and integration are original; no article text or
third-party model weights were copied.
