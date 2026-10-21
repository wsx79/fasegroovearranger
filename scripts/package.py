#!/usr/bin/env python3
"""Package the native app and its notices; tar preserves Unix executable modes."""
from pathlib import Path
import os
import re
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
version = re.search(r'project\(FaseGrooveArranger VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text())[1]
name = f'FaseGrooveArranger-{version}-{system}-{arch}'
dist = root / 'dist'
stage = dist / name
stage.mkdir(parents=True, exist_ok=True)
native = root / 'build/FaseGrooveArranger_artefacts/Release'
assert native.is_dir(), f'Native build missing: {native}'
shutil.copytree(native, stage, dirs_exist_ok=True)
for file in ['LICENSE', 'README.md', 'README.it.md', 'CHANGELOG.md', 'CHANGELOG.it.md', 'PRIVACY.it.md', 'PRIVACY.md', 'THIRD_PARTY_NOTICES.md',
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

# Stable family asset names are used by the in-app updater. Keep the versioned
# portable archives too, so existing manual installations retain their workflow.
update_stage = dist / f'update-{system}'
if update_stage.exists():
    shutil.rmtree(update_stage)
(update_stage / 'Standalone').mkdir(parents=True)
app_name = 'Fase Groove Arranger' + ('.app' if system == 'macOS' else '.exe' if system == 'Windows' else '')
source_app = native / app_name
if source_app.is_dir():
    shutil.copytree(source_app, update_stage / 'Standalone' / app_name, symlinks=True)
else:
    shutil.copy2(source_app, update_stage / 'Standalone' / app_name)
for path in stage.iterdir():
    if path.name == app_name:
        continue
    if path.is_dir():
        shutil.copytree(path, update_stage / path.name, symlinks=True)
    else:
        shutil.copy2(path, update_stage / path.name)
stable = dist / f'FaseGrooveArranger-{system}.zip'
if stable.exists():
    stable.unlink()
if system == 'macOS':
    subprocess.run(['/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', str(update_stage), str(stable)], check=True)
else:
    with zipfile.ZipFile(stable, 'w', zipfile.ZIP_DEFLATED) as out:
        for path in sorted(update_stage.rglob('*')):
            if path.is_file():
                out.write(path, path.relative_to(update_stage))
with zipfile.ZipFile(stable) as check:
    assert f'Standalone/{app_name}' + ('/Contents/Info.plist' if system == 'macOS' else '') in check.namelist()
    assert 'SOURCE.txt' in check.namelist() and 'docs/GUIDE.md' in check.namelist()
print(f'Packaged family update {stable.name}: {stable.stat().st_size:,} bytes')
