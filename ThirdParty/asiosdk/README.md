# Steinberg ASIO SDK (interface headers only)

ASIO® is a registered trademark of Steinberg Media Technologies GmbH.

- Source: official Steinberg download `https://www.steinberg.net/asiosdk`
  → `ASIO-SDK_2.3.4_2025-10-15.zip`
  (SHA-256 `d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca`).
- Copyright (c) 2025 Steinberg Media Technologies GmbH.
- Licence: the SDK is dual licensed (proprietary Steinberg ASIO License **or**
  GNU General Public License version 3). Fase Groove Arranger uses it under the
  **GPLv3** option. See `LICENSE.txt` (Steinberg's licence text, unmodified) and
  `GPL-3.0.txt` (full GPLv3 text).
- Included files, copied unmodified: `common/asio.h`, `common/asiosys.h`,
  `common/iasiodrv.h`. They are only compiled on Windows, where JUCE's ASIO
  device type includes them (`JUCE_ASIO=1`). No driver, sample or host code from
  the SDK is used.

SHA-256 of the included headers:

```
93d2f80d49b4ac9c963514e7c1307ab5111d8104246192233745beb058534ea1  common/asio.h
f44a1ed0fd7c056ff047aeebc4c05a8c185f0585a24d8debc55c70107777dbef  common/asiosys.h
86ad4788452c74823653cf57d4bd557c48bb6542d7e6eb7fff80102c705a066d  common/iasiodrv.h
```

The application as a whole is AGPL-3.0-or-later. Section 13 of both the GNU AGPL v3
and the GNU GPL v3 permits combining code under the two licences: these headers keep
their GPLv3 terms, and the combined work is distributed with complete corresponding
source.

Trademark use follows the "Steinberg ASIO Usage Guidelines" shipped with the SDK
(version 2.3.4): the unmodified ASIO Compatible Logo (`Resources/asio-compatible-logo.png`,
Steinberg artwork) is shown in the audio device dialog where ASIO can be enabled, and the
attribution above appears in the app credits and the documentation.
