# Privacy and permissions — Fase Groove Arranger

Version 0.3.0 · 20 October 2026

Fase Groove Arranger is a desktop application in Fabio 'Pitch' Serra's FS Pitch ecosystem. Source and project contact: https://github.com/wsx79/fasegroovearranger.

The application processes music locally. It has no accounts, ads, analytics, telemetry, cookies, automatic uploads or built-in update checks. Its integrated C++ engines generate music on your device and do not send data to AI services. Source dependencies may be downloaded while building the software; the compiled application runs offline.

Source-code, artist and donation links open the browser only when clicked. The linked services have their own terms and privacy policies. Native VST3/AU plugins run in the application's process and can access resources or services according to their own behaviour; their privacy practices are separate.

The app reads selected projects, MIDI files, samples and plugins. It writes selected projects/exports, temporary render files and a periodic local autosave under `FS Pitch/Fase Groove Arranger` in the user application-data folder. Project files store MIDI notes, musical settings, track names, external file paths and plugin state. Paths and plugin state may reveal local system or personal information: review them before sharing projects. External samples are not automatically embedded.

You can delete projects, exports and autosaves with your file manager. Removing a portable executable does not delete those documents. Undo history is kept in memory and ends when the process closes.

Permissions are limited to audio output, access to chosen files and local application data, loading native instrument code, and opening requested browser links. The application opens no microphone/audio input and does not record the room. It requires no administrator account, camera, location, contacts, calendar or cloud account. System prompts can depend on packaging, audio devices and third-party plugins.

This notice describes the arranger's source code, not the behaviour of external plugins or websites. For project requests use the repository, keeping private data out of public issues.

## Languages, tooltips and MIDI

Language and tooltip preferences are saved locally on this computer. LEARN reads keyboard notes only after you choose a MIDI input in its window. Closing it removes the callback and restores the device’s previous enabled state. It does not record audio or use the microphone. Personal presets remain local files.
