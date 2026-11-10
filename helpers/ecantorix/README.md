# Separate eCantorix guide helper

`prepare.pl` is GPL-3.0-or-later source, modified for FGA on 2026-11-10.
No engine, interpreter, voice data, module or executable is bundled. It transforms
user-supplied [eCantorix source](https://github.com/divVerent/ecantorix/tree/069317157287ce1a6390c109d1a9310556bc0002)
(commit pinned and input SHA-256 verified after CRLF normalization). The prepared copy retains upstream
copyright/license notices and COPYING. Keep this preparation source with it.

Run using **native Perl**, on any target OS (no Bash, patch utility or WSL):

```
perl helpers/ecantorix/prepare.pl "path/to/trusted/upstream" "path/to/new/helper"
```

Install native Perl **5.22 or later** with MIDI, URI::Escape and Math::FFT (XS extension; matching
compiler/toolchain required), classic eSpeak 1.48 and SoX 14.4.2. Use licensed
upstream/system packages; no automatic download or install occurs in FGA.
LMMS, abcmidi, MBROLA voices, neural models and Config::Tiny are **not required**
by this WAV route (the latter is mentioned for other upstream utilities).
`FindBin`, `Cwd`, `Getopt::Long`, `Digest::SHA`, `Exporter` and the preparation
script's File modules belong to Perl's core distribution.

Put native tools on PATH **before starting FGA**. Optional environment settings:
`FGA_PERL_BIN`, `FGA_ESPEAK_BIN`, `FGA_SOX_BIN` accept absolute executable paths;
`FGA_ESPEAK_DATA` identifies the complete `espeak-data` folder (not its parent).
This is useful for a relocated installation whose `--version` reports its compiled
system path. All arguments, including paths with spaces, are passed without a shell.
For example, in native Windows PowerShell:

```
$env:FGA_PERL_BIN = 'C:\Strawberry\perl\bin\perl.exe'
$env:FGA_ESPEAK_BIN = 'C:\tools\eSpeak\command_line\espeak.exe'
$env:FGA_SOX_BIN = 'C:\tools\sox\sox.exe'
$env:FGA_ESPEAK_DATA = 'C:\tools\eSpeak\espeak-data'
```

Choose the prepared `ecantorix.pl` in Vocal Studio. Only choose trusted scripts:
Perl code executes locally with the user's permissions when Render is clicked.
No control file or arbitrary effect expression is accepted from a project.
The adapter changes only five process invocation sites and binary stream modes;
upstream pitch/time synthesis remains intact. Custom SoX effects are unsupported.
The explicit `-t 24` cancels upstream's default -24 semitone transposition.

Linux real rendering and paths with spaces tested. **Windows and macOS are NOT
RUN**, including Perl XS installation and native binaries. Their launcher code
uses a Windows Job Object or POSIX process group for cancellation, respectively.
No compatibility certification or distributable installer is provided. See
[license/distribution review](../../docs/VOCAL_GUIDE_LICENSES.md).
