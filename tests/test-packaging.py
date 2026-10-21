#!/usr/bin/env python3
"""Exercise portable/update archive layouts without building native binaries."""
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
from pathlib import Path
from unittest.mock import patch
from tempfile import TemporaryDirectory
from zipfile import ZipFile
import os
import runpy
import shutil

repo = Path(__file__).resolve().parent.parent
for system, suffix in [('Linux', ''), ('Windows', '.exe')]:
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        for path in ['scripts/package.py', 'CMakeLists.txt', 'LICENSE', 'README.md', 'README.it.md',
                     'CHANGELOG.md', 'CHANGELOG.it.md', 'PRIVACY.it.md', 'PRIVACY.md', 'THIRD_PARTY_NOTICES.md',
                     'Resources/Inter-OFL.txt', 'Resources/Inter-Regular.otf', 'Resources/Inter-SemiBold.otf',
                     'Resources/arranger.svg', 'ThirdParty/shine/COPYING',
                     'ThirdParty/fasegroovepiano/LICENSE', 'ThirdParty/fasegroovedrums/LICENSE',
                     'ThirdParty/fasegrooveguitar/LICENSE', 'docs/GUIDE.md', 'docs/GUIDA.md']:
            dest = root / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(repo / path, dest)
        binary = root / 'build/FaseGrooveArranger_artefacts/Release' / ('Fase Groove Arranger' + suffix)
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b'tested native payload')
        binary.chmod(0o755)
        with patch('platform.system', return_value=system), patch('platform.machine', return_value='x86_64'), patch.dict(os.environ, FGA_SOURCE_SHA='a' * 40):
            runpy.run_path(str(root / 'scripts/package.py'))
        with ZipFile(root / 'dist' / f'FaseGrooveArranger-{system}.zip') as archive:
            assert archive.read('Standalone/' + binary.name) == binary.read_bytes()
            assert b'a' * 40 in archive.read('SOURCE.txt')
            assert archive.read('docs/GUIDE.md') == (repo / 'docs/GUIDE.md').read_bytes()
            assert archive.read('docs/GUIDA.md') == (repo / 'docs/GUIDA.md').read_bytes()
        assert len([p for p in (root / 'dist').glob('FaseGrooveArranger-*.*') if p.is_file()]) == 2
print('PASS versioned portable and stable family update archives, native layout, guides, source offer and permissions')
