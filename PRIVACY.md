# Privacy and permissions — Fase Groove Arranger

Version 0.22.4 · 26 October 2026

Fase Groove Arranger is a desktop application in Fabio 'Pitch' Serra's FS Pitch ecosystem. Source and project contact: https://github.com/wsx79/fasegroovearranger.

The application processes music locally. It has no accounts, ads, analytics, telemetry, cookies or automatic uploads. Its integrated C++ engines generate music on your device. Optional AI composition connects to the service you choose only when you press COMPOSE. Source dependencies may be downloaded while building the software; the traditional music functions run offline.

Source-code, artist and donation links open the browser only when clicked. The linked services have their own terms and privacy policies. Native VST3/AU plugins run in the application's process and can access resources or services according to their own behaviour; their privacy practices are separate.

The app reads selected projects, MIDI files, samples and plugins. It writes selected projects/exports, temporary render files and a periodic local autosave under `FS Pitch/Fase Groove Arranger` in the user application-data folder. Project files store MIDI notes, musical settings, track names, external file paths and plugin state. Paths and plugin state may reveal local system or personal information: review them before sharing projects. External samples are not automatically embedded.

You can delete projects, exports and autosaves with your file manager. Removing a portable executable does not delete those documents. Undo history is kept in memory and ends when the process closes.

Permissions are limited to audio output, access to chosen files and local application data, loading native instrument code, and opening requested browser links. The application opens no microphone/audio input and does not record the room. The running app requires no administrator account, camera, location, contacts, calendar or cloud account. The optional Windows setup requests administrator access to install or upgrade in Program Files; the app itself runs with the normal user account. System prompts can depend on packaging, audio devices and third-party plugins.

This notice describes the arranger's source code, not the behaviour of external plugins or websites. For project requests use the repository, keeping private data out of public issues.

## Languages, tooltips and MIDI

Language and tooltip preferences are saved locally on this computer. LEARN reads keyboard notes only after you choose a MIDI input in its window. Closing it removes the callback and restores the device’s previous enabled state. It does not record audio or use the microphone. Personal presets remain local files.

## GitHub updates

As in the family apps, automatic release checking/downloading is enabled initially and can be disabled in OPTIONS → SETTINGS. CHECK NOW performs a manual check. The updater requests the public GitHub release metadata, the Italian changelog from raw.githubusercontent.com when appropriate, and the matching release package through GitHub's download/CDN services. GitHub receives ordinary connection data (IP and user-agent); no project, MIDI, sample, plugin state or local file path is uploaded.

Packages are downloaded and prepared under `Downloads/Fase Groove Arranger updates`, or Documents if Downloads is unavailable. SHA-256 is verified against release metadata. INSTALL NOW is required before the Windows setup or portable replacement runs. The setup may request Windows administrator permission; portable replacement needs write access to the app folder. If replacement fails, old files are restored; protected folders fall back to manual installation. A local temporary helper may reopen the app after it closes. Projects, samples, exports, autosaves and preferences are not replaced by the updater or deleted by uninstalling. Delete update downloads with your file manager when no longer needed.

Language, size, tooltips, circle mode, update preference and skipped version are stored locally. Linux networking uses system curl; if it is missing, offline music work remains available.

Plugin discovery reads the standard plugin folders and the custom folders you select, including subfolders. Search paths, instrument descriptions, file metadata fingerprints and failed-scan status are stored locally under your user application-data folder in `Fase Groove Arranger/Plugins`. The app does not upload this catalogue. Discovery starts a separate copy of the app to inspect each new/changed plugin; plugin code and its own licensing/network behaviour still belong to the plugin vendor.

## Optional AI composition

AI COMPOSER supports OpenAI, Anthropic, Google Gemini and an Ollama server whose address you choose. Each COMPOSE action sends your written request, available track names/roles, section names/lengths, key, scale, chord names and limited MIDI excerpts used as musical context. Online providers receive ordinary connection metadata and your API key for authentication. Their own privacy, retention, billing and model policies apply. An Ollama server on your computer processes context locally; a remote Ollama address receives it on that server.

No audio, samples, local file paths, plugin catalogue, serialised plugin/effect state or API keys are included in the musical context. The selected online provider receives its key only in an HTTPS header, with redirects disabled. Keys remain in process memory for the session; they are not saved to projects, preferences, temporary files, logs or subprocess arguments. Generation uses native HTTPS on Windows/macOS and libcurl on Linux. Only provider/model preferences and the Ollama address are saved locally. Closing the composer cancels a pending request; the provider may already have processed or billed it. Opening the composer with Ollama selected, selecting Ollama or pressing CHECK sends a model-list request to the configured server without musical context. No composition request starts automatically and there is no automatic fallback to another provider.

Responses are validated as bounded musical data, never executed as code. A preview is applied only when you press APPLY; Undo restores the preceding project. The resulting MIDI and arrangement become ordinary project data and are saved/autosaved locally in the same way as manual music.

## Studio recording and sample collection

Studio opens only the selected MIDI input for note monitoring and recording. Notes, velocity and sustain-derived durations stay in the local project; MIDI input selection and recording preferences are saved locally. Studio does not record microphone audio. Sample collection and portable presets copy chosen sample files to the selected destination and verify their content locally. Tempo and pitch processing runs on the device, without uploading audio.
