#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise the real isolated scanner with a native instrument on each platform."""
from pathlib import Path
import os
import platform
import subprocess

system = platform.system()
base = Path('build/FaseGrooveArranger_artefacts/Release')
if system == 'Windows':
    app = base / 'Fase Groove Arranger.exe'
elif system == 'Darwin':
    app = base / 'Fase Groove Arranger.app/Contents/MacOS/Fase Groove Arranger'
else:
    app = base / 'Fase Groove Arranger'
probes = list(Path('build/ArrangerHostProbe_artefacts/Release/VST3').glob('*.vst3'))
assert app.is_file() and len(probes) == 1, (app, probes)
out = Path(os.environ['RUNNER_TEMP']) / 'plugin-catalog'
command = [str(app.resolve()), '--plugin-catalog-test', str(probes[0].resolve()), str(out.resolve())]
if system == 'Linux':
    command = ['xvfb-run', '-a', *command]
subprocess.run(command, check=True, timeout=300)
report = (out / 'plugin-catalog.txt').read_text()
assert report.startswith('PASS'), report
print(report)
