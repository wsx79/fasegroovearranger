> **Documento storico del backend rimosso.** Non descrive dipendenze o funzioni
> attive di Vocaltest. eCantorix/eSpeak e i relativi helper non sono più eseguiti
> né distribuiti. [Stato corrente di Vocal Studio](VOCAL_STUDIO.md).

# English guide: licences and platform gate (2026-11-10)

Approved: classic eCantorix/eSpeak formant synthesis for a subsequent real singer's
recording. No Qixuan, DiffSinger, neural vocoder, MBROLA bank, cloud or subscription.
No additional third-party source or binary is linked into JUCE. MIDI karaoke and
WAV are the external protocol. The repository application remains AGPL-3.0-or-later.

## Inspected source and Linux diagnostic packages

| Component | Version inspected | Terms / primary source |
|---|---|---|
| eCantorix | `069317157287ce1a6390c109d1a9310556bc0002` | GPL-3.0-or-later; [source notice](https://github.com/divVerent/ecantorix/blob/069317157287ce1a6390c109d1a9310556bc0002/ecantorix.pl), [COPYING](https://github.com/divVerent/ecantorix/blob/069317157287ce1a6390c109d1a9310556bc0002/COPYING) |
| eSpeak / English formant data | Debian `1.48.15+dfsg-3+b2` | GPL-3.0-or-later, Sonic portions LGPL-2.1+, PortAudio headers permissive; [official source/downloads](https://espeak.sourceforge.net/download.html), [package copyright](https://metadata.ftp-master.debian.org/changelogs/main/e/espeak/unstable_copyright) |
| SoX executable / libsox | Debian `14.4.2+git20190427-5+b3` | executable GPL-2.0-or-later, library LGPL-2.1-or-later, file exceptions; [upstream GPL](https://github.com/chirlu/sox/blob/master/LICENSE.GPL), [LGPL](https://github.com/chirlu/sox/blob/master/LICENSE.LGPL), [package copyright](https://metadata.ftp-master.debian.org/changelogs/main/s/sox/unstable_copyright) |
| Perl | local 5.40.1 | Artistic-1.0-Perl OR GPL-1.0-or-later; [perlartistic](https://perldoc.perl.org/perlartistic), [perlgpl](https://perldoc.perl.org/perlgpl) |
| MIDI | 0.84 | Perl terms; [distribution](https://metacpan.org/release/CONKLIN/MIDI-Perl-0.84), copyright in MIDI.pm and Debian package |
| URI::Escape (URI) | 5.30 | Perl terms; [distribution](https://metacpan.org/release/OALDERS/URI-5.30), copyright in Escape.pm and Debian package |
| Math::FFT | 1.36, compiled locally from CPAN | Perl terms in distribution LICENSE/META; [distribution](https://metacpan.org/release/SHLOMIF/Math-FFT-1.36). Preserve bundled Ooura FFT and arrays.c notices too. XS needs a compiler compatible with Perl. |
| FGA preparation script | `helpers/ecantorix/prepare.pl` | GPL-3.0-or-later, supplied as source with COPYING. Changed files carry modification date. |

Files in the actual extracted Linux packages and Math::FFT's LICENSE were read;
these are not assumptions based solely on repository badges. Config::Tiny, LMMS
and abcmidi are not loaded by the selected WAV route. Core Perl modules inherit
Perl distribution terms. No external voicebank or separate vocoder is needed.

## Four distinct uses

1. **Local use:** user installs trusted tools/data; personal local execution and
   modification do not themselves trigger GPL source distribution obligations.
   No text or audio leaves the machine. All components permit commercial use.
2. **Software distribution:** FGA's AGPL obligations remain. Process separation
   is an engineering boundary, **not an automatic GPL exemption**. Standard file
   interchange supports independent programs; assess the actual combined package
   under GPL §§5–6 and AGPL/GPL §13 before shipping. GPLv3 and AGPLv3 have an
   express compatibility provision, not a blanket waiver of either licence.
3. **Helper/data/runtime distribution:** none is bundled now. If later authorized,
   preserve copyright and licence texts; supply the corresponding modified source,
   exact upstream revision, this transformation/build scripts, dependency versions
   and required installation material. A URL alone is not always sufficient for
   binary distribution. Audit each actual Windows/macOS archive, DLL/dylib, XS
   module and optional codec; Linux package review does not approve them. Avoid
   unused codecs/MBROLA. SoX library relinking requirements and notices for bundled
   system/audio/codec libraries must be included in that package review.
4. **Generated audio, including commercial:** the guide is ordinary synthesized
   output; GPL does not automatically cover output unless it contains copyrighted
   program material ([GNU explanation](https://www.gnu.org/licenses/gpl-faq.html#GPLOutput),
   [GPLv3 §2](https://www.gnu.org/licenses/gpl-3.0.html#section2)). The selected
   formant route imposes no NC/output attribution clause identified in the inspected
   terms. Rights to lyrics/melody and the real singer's recording still apply.

This source-only integration does not authorize distributing any third-party
binary. A packaging review with the exact platform inventory remains necessary.

## Native platforms

Linux: real WAV generation with relocated dependencies and a helper path containing
spaces tested. Windows: POSIX shell expansion replaced by native argument-list
execution; binary Perl pipes and Windows process-tree supervision implemented,
**not compiled or run on Windows**. Native Perl/XS, eSpeak and SoX installations
need validation. Perl >=5.22 is enforced: official [open documentation](https://perldoc.perl.org/functions/open)
confirms list-form pipes on native Windows from that version; binary mode is explicit. macOS: uses POSIX spawn/process groups and native Perl tooling;
**not built or run on macOS**, including Apple Silicon and XS/toolchain setup.
No mandatory WSL/Bash; no unverified installers downloaded or bundled.

Preparation/setup: [helper instructions](https://github.com/wsx79/fasegroovearranger/blob/914822728be562047d0790607f63323d4aaa12ca/helpers/ecantorix/README.md).

## Vocaltest Preview Windows audit (2026-11-10)

Official download pages were read again: eSpeak offers 1.48.04 for Windows,
SoX 14.4.2 offers Win32 packages, and Strawberry Perl provides native Windows
Perl. No Windows installer/archive/DLL/XS module was downloaded or approved.
The Linux package versions in the table are not evidence for these Windows
payloads. WAV/raw and tempo/bend/remix/reverse/silence need actual runtime checks;
no optional compressed codec is required for the guide route.

Portable packaging now retains the verified GPL preparation **source** with
COPYING and its modification notice, plus the setup documentation. It still
excludes external interpreters, synthesis binaries, voice data, modules and
codecs. Review their exact archives before any redistribution; process separation
and commercial output use do not remove software/source obligations.
The unpublished local commit also prevents relying on the current SOURCE.txt URL
alone for FGA/JUCE/Shine corresponding source. No distributable preview was made.
See [Windows build, manual and source-distribution gates](VOCALTEST_PREVIEW_WINDOWS.it.md).

## Preview Actions con stack esterno escluso

Il job branch-only Vocaltest prepara sorgenti corrispondenti FGA/JUCE completi,
notice e istruzioni di rebuild/relink insieme al programma; non dipende più
soltanto dal vecchio URL locale. Lo stack eCantorix/Perl/eSpeak/SoX resta esterno:
nessun archivio Windows aggiuntivo è approvato o distribuito da questa CI.
La GUI senza helper e i test della DAW non provano il rendering inglese su Windows.
Per risultati reali del job e checklist vedere VOCALTEST_PREVIEW_WINDOWS.it.md.
