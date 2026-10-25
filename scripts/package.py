#!/usr/bin/env python3
"""Build one family ZIP per platform from an explicit runtime payload."""
from pathlib import Path
import os
import platform
import re
import shutil
import subprocess
import zipfile

root = Path(__file__).resolve().parent.parent
system = {'Darwin': 'macOS'}.get(platform.system(), platform.system())
native = root / 'build/FaseGrooveArranger_artefacts/Release'
app_name = 'Fase Groove Arranger' + ('.app' if system == 'macOS' else '.exe' if system == 'Windows' else '')
source_app = native / app_name
assert source_app.exists(), f'Native app missing: {source_app}'
dist = root / 'dist'
stage = dist / 'package'
# Never carry a previous build's files into a new distribution.
if stage.exists():
    shutil.rmtree(stage)
(stage / 'Standalone').mkdir(parents=True)
if source_app.is_dir():
    shutil.copytree(source_app, stage / 'Standalone' / app_name, symlinks=True)
else:
    shutil.copy2(source_app, stage / 'Standalone' / app_name)

files = {'LICENSE', 'README.md', 'README.it.md', 'CHANGELOG.md', 'CHANGELOG.it.md',
         'PRIVACY.it.md', 'PRIVACY.md', 'THIRD_PARTY_NOTICES.md',
         'Resources/Inter-OFL.txt', 'ThirdParty/shine/COPYING', 'docs/images/inspector.png',
         'ThirdParty/fasegroovepiano/LICENSE', 'ThirdParty/fasegroovedrums/LICENSE',
         'ThirdParty/fasegrooveguitar/LICENSE', 'ThirdParty/fasegroovelead/LICENSE'}
# Include the user manuals and their local links/images, not the development
# documentation, source assets or duplicate embedded fonts.
pending = [root / 'docs/GUIDE.md', root / 'docs/GUIDA.md']
while pending:
    path = pending.pop().resolve()
    relative = path.relative_to(root).as_posix()
    if relative in files:
        continue
    assert path.is_file(), f'Missing manual resource: {relative}'
    files.add(relative)
    if path.suffix.lower() == '.md':
        text = path.read_text(encoding='utf-8')
        for link in re.findall(r'(?:!?\[[^\]]*\]\(([^)]+)\)|src="([^"]+)")', text):
            target = next(part for part in link if part).split('#')[0]
            if target and not re.match(r'^[a-zA-Z]+:', target):
                pending.append(path.parent / target)
for file in sorted(files):
    target = stage / file
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / file, target)
sha = os.environ.get('FGA_SOURCE_SHA') or os.environ.get('GITHUB_SHA') or subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
(stage / 'SOURCE.txt').write_text('Complete corresponding source and build instructions\n'
                               f'https://github.com/wsx79/fasegroovearranger/tree/{sha}\n', encoding='utf-8')
archive = dist / f'FaseGrooveArranger-{system}.zip'
if archive.exists():
    archive.unlink()
if system == 'macOS':
    subprocess.run(['/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', str(stage), str(archive)], check=True)
else:
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as out:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                out.write(path, path.relative_to(stage))
with zipfile.ZipFile(archive) as check:
    entry = f'Standalone/{app_name}' + ('/Contents/Info.plist' if system == 'macOS' else '')
    assert entry in check.namelist()
    if system == 'Linux':
        assert check.getinfo(entry).external_attr >> 16 & 0o111, 'Executable permissions missing'
    assert 'SOURCE.txt' in check.namelist() and 'docs/GUIDE.md' in check.namelist()
print(f'Packaged {archive.name}: {archive.stat().st_size:,} bytes')
