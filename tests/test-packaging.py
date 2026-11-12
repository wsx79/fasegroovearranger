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
import sys

repo = Path(__file__).resolve().parent.parent
# NTFS does not provide the Unix execute mode required by a Linux payload.
# Keep that regression on POSIX; Windows still exercises its real package shape.
systems = [('Windows', '.exe')] if os.name == 'nt' else [('Linux', ''), ('Windows', '.exe')]
for system, suffix in systems:
    with TemporaryDirectory() as tmp:
        root = Path(tmp)
        for path in ['scripts/package.py', 'CMakeLists.txt', 'LICENSE', 'README.md', 'README.it.md',
                     'CHANGELOG.md', 'CHANGELOG.it.md', 'PRIVACY.it.md', 'PRIVACY.md', 'THIRD_PARTY_NOTICES.md',
                     'Resources/Inter-OFL.txt', 'Resources/Inter-Regular.otf', 'Resources/Inter-SemiBold.otf',
                     'Resources/arranger.svg', 'Resources/arranger-project.ico', 'ThirdParty/shine/COPYING',
                     'ThirdParty/signalsmith-stretch/LICENSE.txt', 'ThirdParty/signalsmith-linear/LICENSE.txt',
                     'ThirdParty/fasegroovepiano/LICENSE', 'ThirdParty/fasegroovedrums/LICENSE',
                     'ThirdParty/fasegrooveguitar/LICENSE', 'ThirdParty/fasegroovelead/LICENSE', 'docs/GUIDE.md', 'docs/GUIDA.md',
                     'ThirdParty/asiosdk/LICENSE.txt', 'ThirdParty/asiosdk/GPL-3.0.txt', 'ThirdParty/asiosdk/README.md']:
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
        with patch('platform.system', return_value=system), patch('platform.machine', return_value='x86_64'), \
                patch.dict(os.environ, FGA_SOURCE_SHA='a' * 40), patch.object(sys, 'argv', ['package.py']):
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
            for guide in ['VOCAL_CHOPS_PHASE02.md',
                          'VOCAL_CHOPS_IMPORT.md', 'VOCAL_CHOPS_IMPORT_PERFORMANCE.json',
                          'VOCALTEST_PREVIEW_WINDOWS.it.md']:
                entry = 'docs/' + guide
                assert archive.read(entry) == (repo / entry).read_bytes()
            assert not any(name.lower().endswith(('.dll', '/perl.exe', '/sox.exe', '/espeak.exe')) for name in names)
            assert 'ThirdParty/fasegroovelead/LICENSE' in names and 'docs/images/inspector.png' in names
            assert all('ThirdParty/asiosdk/' + n in names for n in ('LICENSE.txt', 'GPL-3.0.txt', 'README.md'))
            for dependency in ['signalsmith-stretch', 'signalsmith-linear']:
                assert f'ThirdParty/{dependency}/LICENSE.txt' in names
            for image in ['logo.svg', 'options.png', 'screenshot.png', 'images/guide_piano.png', 'images/guide_lead.png', 'images/guide_sampler.png', 'images/guide_recording.png', 'images/guide_buses.png']:
                assert f'docs/{image}' in names
            if system == 'Windows':
                assert archive.read('Resources/arranger-project.ico') == (repo / 'Resources/arranger-project.ico').read_bytes()
            else:
                assert 'Resources/arranger-project.ico' not in names
            if system == 'Linux':
                assert archive.getinfo('Standalone/' + binary.name).external_attr >> 16 & 0o111
        assert len([p for p in (root / 'dist').glob('FaseGrooveArranger-*.*') if p.is_file()]) == 1
        # Staging a separately tested build must not emit a preview/ZIP early.
        archive_path = root / 'dist' / f'FaseGrooveArranger-{system}.zip'
        archive_path.unlink()
        alternate = root / 'build/preview/FaseGrooveArranger_artefacts/Release' / binary.name
        alternate.parent.mkdir(parents=True)
        alternate.write_bytes(b'other native build fixture')
        with patch('platform.system', return_value=system), patch.dict(os.environ, FGA_SOURCE_SHA='b' * 40), \
                patch.object(sys, 'argv', ['package.py', '--stage-only', '--build-dir', 'build/preview']):
            try:
                runpy.run_path(str(root / 'scripts/package.py'))
            except SystemExit as result:
                assert result.code == 0
            else:
                raise AssertionError('stage-only must finish before archive generation')
        assert not list((root / 'dist').glob('*.zip'))
        assert (root / 'dist/package/Standalone' / binary.name).read_bytes() == alternate.read_bytes()
        assert b'b' * 40 in (root / 'dist/package/SOURCE.txt').read_bytes()
print('PASS platform packaging fixtures, licences, removed-helper exclusion, source pointer, clean staging, alternate build and stage-only; NOT native executable validation')
