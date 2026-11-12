# SPDX-License-Identifier: AGPL-3.0-or-later
"""Source-complete Windows preview payload; called only after native CI tests."""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import shutil
import struct
import subprocess
import zipfile


def check_x64(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Windows preview requires an actual PE executable')
    at = struct.unpack_from('<I', data, 60)[0]
    if at + 26 > len(data) or data[at:at+4] != b'PE\0\0' or struct.unpack_from('<H', data, at+4)[0] != 0x8664 or struct.unpack_from('<H', data, at+24)[0] != 0x20b:
        raise ValueError('Windows preview requires native AMD64 PE32+')


def add_preview(stage, root, juce, app, sha):
    check_x64(app.read_bytes())
    juce = Path(juce).resolve()
    if not (juce / 'LICENSE.md').is_file():
        raise ValueError('Exact compiled JUCE source/licence required')
    source = stage / 'Source'
    source.mkdir(parents=True)
    revisions = {}
    for name, repo in [('FaseGrooveArranger', root), ('JUCE', juce)]:
        revision = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
        if name == 'FaseGrooveArranger' and revision != sha:
            raise ValueError('Source SHA differs from tested checkout')
        changed = subprocess.check_output(['git', '-C', str(repo), 'diff', 'HEAD', '--name-only', '-z']).decode().split('\0')
        changed = [path for path in changed if path]
        adaptations = {'modules/juce_audio_processors/format_types/juce_VST3PluginFormat.cpp',
                       'modules/juce_events/native/juce_Messaging_linux.cpp'}
        if changed and (name != 'JUCE' or not set(changed).issubset(adaptations)):
            raise ValueError('Unexpected source changes: ' + name + ': ' + ', '.join(changed))
        if b'160000 ' in subprocess.check_output(['git', '-C', str(repo), 'ls-files', '--stage']):
            raise ValueError('Submodule sources need an explicit complete-source review')
        archive = source / (name + '-source.zip')
        modified = {}
        if name == 'JUCE':
            # CMake applies the project's existing WaveShell/host-context fixes.
            # Archive actual compiled bytes, not the unpatched upstream Git tree.
            paths = subprocess.check_output(['git', '-C', str(repo), 'ls-files', '-z']).decode().split('\0')
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as out:
                for relative in filter(None, paths):
                    path = repo / relative
                    if path.is_symlink() or not path.is_file():
                        raise ValueError('Review source link/missing file: ' + relative)
                    out.write(path, relative)
                date = subprocess.check_output(['git', '-C', str(root), 'show', '-s', '--format=%cs', 'HEAD'], text=True).strip()
                out.writestr('FGA_MODIFICATIONS.md', f'JUCE adapted by FaseGroove Arranger build scripts, {date}.\n'
                    f'Application revision: {sha}; upstream JUCE revision: {revision}.\n'
                    'Original copyright/licence notices are retained. See the included patch and application cmake/ScanWaveShell.cmake and cmake/LinuxEventLoop.cmake.\n'
                    'Archive files contain the exact working-tree source used by this build.\n')
                out.writestr('FGA_MODIFICATIONS.diff', subprocess.check_output(['git', '-C', str(repo), 'diff', 'HEAD', '--binary']))
            modified = {p: hashlib.sha256((repo / p).read_bytes()).hexdigest() for p in changed}
        else:
            subprocess.run(['git', '-C', str(repo), 'archive', '--format=zip', 'HEAD', '--output', str(archive)], check=True)
        revisions[name] = {'commit': revision, 'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(), 'modified_files': modified}
    (source / 'BUILD.txt').write_text('Extract both source ZIPs into separate directories.\n'
        'Build with MSVC x64, CMake >=3.22 and Python 3:\n'
        'cd FaseGrooveArranger\n'
        'cmake -S . -B build -A x64 -DJUCE_DIR=../JUCE\n'
        'cmake --build build --config Release\n'
        'ctest --test-dir build -C Release --output-on-failure\n'
        'The application source includes vendored Shine; rebuild/relink with modified sources is permitted.\n', encoding='utf-8')
    # git archive has no top-level prefix: extract each ZIP into the named directory.
    (stage / 'SOURCE.txt').write_text('Complete corresponding source is included in Source/.\n'
        'Extract FaseGrooveArranger-source.zip into FaseGrooveArranger/, JUCE-source.zip into JUCE/.\n'
        f'Application revision: {sha}\nSee Source/BUILD.txt and original licence notices.\n', encoding='utf-8')
    (stage / 'PREVIEW.json').write_text(json.dumps({'name': 'Vocaltest Preview', 'source': revisions,
        'exe_sha256': hashlib.sha256(app.read_bytes()).hexdigest(),
        'manual_windows_11_audio': 'PENDING'}, indent=2), encoding='utf-8')
    shutil.copy2(root / 'VOCALTEST_QUICKSTART.it.md', stage / 'LEGGIMI-PRIMA.it.md')


def verify_preview(archive):
    with zipfile.ZipFile(archive) as z:
        required = {'Standalone/Fase Groove Arranger.exe', 'LEGGIMI-PRIMA.it.md', 'PREVIEW.json',
            'SOURCE.txt', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'Source/FaseGrooveArranger-source.zip', 'Source/JUCE-source.zip', 'Source/BUILD.txt'}
        if not required.issubset(z.namelist()) or z.testzip() is not None:
            raise ValueError('Preview payload incomplete/corrupt')
        check_x64(z.read('Standalone/Fase Groove Arranger.exe'))
        manifest = json.loads(z.read('PREVIEW.json'))
        if hashlib.sha256(z.read('Standalone/Fase Groove Arranger.exe')).hexdigest() != manifest['exe_sha256']:
            raise ValueError('Preview binary checksum mismatch')
        for name in z.namelist():
            part = PurePosixPath(name.lower())
            if part.suffix == '.dll' or (part.suffix == '.exe' and name != 'Standalone/Fase Groove Arranger.exe'):
                raise ValueError('Unreviewed external runtime in preview: ' + name)
        for name, licence in [('FaseGrooveArranger', 'LICENSE'), ('JUCE', 'LICENSE.md')]:
            data = z.read('Source/' + name + '-source.zip')
            if hashlib.sha256(data).hexdigest() != manifest['source'][name]['sha256']:
                raise ValueError('Corresponding source checksum mismatch')
            with zipfile.ZipFile(io.BytesIO(data)) as src:
                if licence not in src.namelist() or src.testzip() is not None:
                    raise ValueError('Corresponding source/licence missing: ' + name)
                for path, checksum in manifest['source'][name].get('modified_files', {}).items():
                    if hashlib.sha256(src.read(path)).hexdigest() != checksum:
                        raise ValueError('Compiled source mismatch: ' + path)
