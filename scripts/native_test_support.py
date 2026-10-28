# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Native test paths and display setup, shared by the CI's three operating systems."""
from pathlib import Path
import os
import platform
import shutil
import subprocess
import sys


def build_root(value=None):
    root = Path(__file__).resolve().parent.parent
    if value:
        return Path(value).resolve()
    for name in ('build', 'build-app'):
        folder = root / name / 'FaseGrooveArranger_artefacts' / 'Release'
        binaries = [folder / 'Fase Groove Arranger', folder / 'Fase Groove Arranger.exe',
                    folder / 'Fase Groove Arranger.app' / 'Contents' / 'MacOS' / 'Fase Groove Arranger']
        if any(binary.is_file() for binary in binaries):
            return root / name
    return root / 'build'


def application(build):
    folder = build / 'FaseGrooveArranger_artefacts' / 'Release'
    system = platform.system()
    path = (folder / 'Fase Groove Arranger.app' / 'Contents' / 'MacOS' / 'Fase Groove Arranger'
            if system == 'Darwin' else folder / ('Fase Groove Arranger.exe' if system == 'Windows' else 'Fase Groove Arranger'))
    if not path.is_file():
        raise SystemExit(f'Native application not found: {path}')
    return path


def run_application(build, *arguments, timeout=300):
    command = [str(application(build)), *map(str, arguments)]
    if platform.system() == 'Linux' and not os.environ.get('DISPLAY'):
        if not shutil.which('xvfb-run'):
            raise SystemExit('Install xvfb or set DISPLAY before running native GUI tests.')
        command = ['xvfb-run', '-a', *command]
    subprocess.run(command, check=True, timeout=timeout)


def output_folder(name, requested=None):
    folder = Path(requested) if requested else Path(os.environ.get('RUNNER_TEMP', 'build/native-validation')) / name
    folder.mkdir(parents=True, exist_ok=True)
    return folder.resolve()


def verify(script, folder):
    subprocess.run([sys.executable, str(Path(__file__).parent / script), str(folder)], check=True, timeout=900)
