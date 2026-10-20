#!/usr/bin/env python3
"""Package the native app and its notices; tar preserves Unix executable modes."""
from pathlib import Path
import os
import platform
import shutil
import subprocess
import tarfile
import zipfile

root = Path(__file__).resolve().parent.parent
system = {'Darwin': 'macOS'}.get(platform.system(), platform.system())
native = root / 'build/FaseGrooveArranger_artefacts/Release'
if system == 'macOS':
    executable = native / 'Fase Groove Arranger.app/Contents/MacOS/Fase Groove Arranger'
    architectures = subprocess.check_output(['lipo', '-archs', str(executable)], text=True).split()
    arch = 'universal' if {'arm64', 'x86_64'}.issubset(architectures) else '-'.join(architectures)
else:
    arch = platform.machine()
name = f'FaseGrooveArranger-0.2.1-{system}-{arch}'
dist = root / 'dist'
stage = dist / name
stage.mkdir(parents=True, exist_ok=True)
native = root / 'build/FaseGrooveArranger_artefacts/Release'
assert native.is_dir(), f'Native build missing: {native}'
shutil.copytree(native, stage, dirs_exist_ok=True)
for file in ['LICENSE', 'README.md', 'PRIVACY.it.md', 'PRIVACY.md', 'THIRD_PARTY_NOTICES.md',
             'Resources/Inter-OFL.txt', 'Resources/Inter-Regular.otf', 'Resources/Inter-SemiBold.otf',
             'Resources/arranger.svg', 'ThirdParty/shine/COPYING',
             'ThirdParty/fasegroovepiano/LICENSE', 'ThirdParty/fasegroovedrums/LICENSE',
             'ThirdParty/fasegrooveguitar/LICENSE']:
    target = stage / file
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / file, target)
shutil.copytree(root / 'docs', stage / 'docs', dirs_exist_ok=True)
sha = os.environ.get('FGA_SOURCE_SHA') or os.environ.get('GITHUB_SHA') or subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
(stage / 'SOURCE.txt').write_text('Complete corresponding source and build instructions\n'
                               f'https://github.com/wsx79/fasegroovearranger/tree/{sha}\n', encoding='utf-8')
if system == 'Windows':
    archive = dist / f'{name}.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as out:
        for path in stage.rglob('*'):
            if path.is_file():
                out.write(path, path.relative_to(dist))
else:
    archive = dist / f'{name}.tar.gz'
    with tarfile.open(archive, 'w:gz') as out:
        out.add(stage, arcname=name)
    with tarfile.open(archive) as check:
        executables = [f for f in check.getmembers() if f.isfile() and f.mode & 0o111]
        assert executables, 'Executable permissions missing from distribution'
print(f'Packaged {archive.name}: {archive.stat().st_size:,} bytes')
