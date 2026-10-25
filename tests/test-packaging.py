#!/usr/bin/env python3
"""Exercise the single family ZIP, payload allowlist and clean staging."""
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
                     'ThirdParty/fasegrooveguitar/LICENSE', 'ThirdParty/fasegroovelead/LICENSE', 'docs/GUIDE.md', 'docs/GUIDA.md']:
            dest = root / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(repo / path, dest)
        shutil.copytree(repo / 'docs', root / 'docs', dirs_exist_ok=True)
        binary = root / 'build/FaseGrooveArranger_artefacts/Release' / ('Fase Groove Arranger' + suffix)
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b'tested native payload')
        binary.chmod(0o755)
        (binary.parent / 'debug.pdb').write_text('not a runtime file')
        (root / 'dist/package').mkdir(parents=True)
        (root / 'dist/package/stale.txt').write_text('from a previous build')
        with patch('platform.system', return_value=system), patch('platform.machine', return_value='x86_64'), patch.dict(os.environ, FGA_SOURCE_SHA='a' * 40):
            runpy.run_path(str(root / 'scripts/package.py'))
        with ZipFile(root / 'dist' / f'FaseGrooveArranger-{system}.zip') as archive:
            assert archive.read('Standalone/' + binary.name) == binary.read_bytes()
            assert b'a' * 40 in archive.read('SOURCE.txt')
            assert archive.read('docs/GUIDE.md') == (repo / 'docs/GUIDE.md').read_bytes()
            assert archive.read('docs/GUIDA.md') == (repo / 'docs/GUIDA.md').read_bytes()
            names = set(archive.namelist())
            assert 'stale.txt' not in names and 'Standalone/debug.pdb' not in names
            assert 'docs/ARCHITECTURE.md' not in names and 'docs/VALIDATION.md' not in names
            assert 'Resources/Inter-Regular.otf' not in names and 'Resources/Inter-SemiBold.otf' not in names
            assert 'Resources/Inter-OFL.txt' in names and 'ThirdParty/shine/COPYING' in names
            assert 'ThirdParty/fasegroovelead/LICENSE' in names and 'docs/images/inspector.png' in names
            for image in ['logo.svg', 'options.png', 'screenshot.png', 'images/guide_piano.png', 'images/guide_lead.png']:
                assert f'docs/{image}' in names
            if system == 'Linux':
                assert archive.getinfo('Standalone/' + binary.name).external_attr >> 16 & 0o111
        assert len([p for p in (root / 'dist').glob('FaseGrooveArranger-*.*') if p.is_file()]) == 1
print('PASS one family ZIP per platform, explicit runtime payload, clean staging, linked guide images, licences, source offer and Unix permissions')
