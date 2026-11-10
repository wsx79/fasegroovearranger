#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Source completeness/checksum/PE gate regressions; synthetic payload, not a Windows runtime test."""
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from preview_package import add_preview, check_x64, verify_preview


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.PIPE, text=True).strip()


def rejects(fn):
    try: fn()
    except ValueError: return
    raise AssertionError('Expected invalid payload to be rejected')


with TemporaryDirectory() as temp:
    base = Path(temp);root = base / 'app';juce = base / 'JUCE';stage = base / 'stage'
    for repo, licence in [(root, 'LICENSE'), (juce, 'LICENSE.md')]:
        repo.mkdir();(repo / licence).write_text('synthetic licence fixture')
        (repo / 'code.cpp').write_text('source fixture')
        if repo == root: (repo / 'VOCALTEST_QUICKSTART.it.md').write_text('synthetic manual')
        else:
            scanner = repo / 'modules/juce_audio_processors/format_types/juce_VST3PluginFormat.cpp'
            scanner.parent.mkdir(parents=True);scanner.write_text('upstream scanner fixture')
        git(repo, 'init');git(repo, 'add', '.')
        git(repo, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-m', 'fixture')
    (stage / 'Standalone').mkdir(parents=True)
    app = stage / 'Standalone/Fase Groove Arranger.exe'
    pe = bytearray(160);pe[:2]=b'MZ';struct.pack_into('<I',pe,60,80);pe[80:84]=b'PE\0\0'
    struct.pack_into('<H',pe,84,0x8664);struct.pack_into('<H',pe,104,0x20b);app.write_bytes(pe)
    rejects(lambda: check_x64(b'not executable'))
    wrong = bytearray(pe);struct.pack_into('<H',wrong,84,0x14c);rejects(lambda: check_x64(wrong))
    sha=git(root,'rev-parse','HEAD');add_preview(stage,root,juce,app,sha)
    for entry in ['LICENSE','THIRD_PARTY_NOTICES.md','helpers/ecantorix/COPYING','helpers/ecantorix/prepare.pl']:
        path=stage/entry;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('synthetic notice/source')
    archive=base/'preview.zip'
    def pack():
        with zipfile.ZipFile(archive,'w') as z:
            for p in stage.rglob('*'):
                if p.is_file():z.write(p,p.relative_to(stage))
    pack();verify_preview(archive)
    manifest=json.loads((stage/'PREVIEW.json').read_text());assert manifest['guide_render'].startswith('NOT RUN')
    scanner.write_text('actual configured scanner fixture')
    patched = base / 'patched-stage';patched.mkdir()
    add_preview(patched,root,juce,app,sha)
    with zipfile.ZipFile(patched/'Source/JUCE-source.zip') as src:
        assert src.read(scanner.relative_to(juce).as_posix()) == scanner.read_bytes()
        assert 'FGA_MODIFICATIONS.md' in src.namelist() and 'FGA_MODIFICATIONS.diff' in src.namelist()
    changed=json.loads((patched/'PREVIEW.json').read_text())['source']['JUCE']['modified_files']
    assert scanner.relative_to(juce).as_posix() in changed
    (juce/'code.cpp').write_text('unexpected source change')
    rejects(lambda: add_preview(base/'unknown-juce-stage',root,juce,app,sha))
    (juce/'code.cpp').write_text('source fixture')
    (stage/'sox.exe').write_text('unapproved runtime');pack();rejects(lambda: verify_preview(archive));(stage/'sox.exe').unlink()
    (stage/'Source/JUCE-source.zip').unlink();pack();rejects(lambda: verify_preview(archive))
    (root/'code.cpp').write_text('uncommitted changed source');rejects(lambda: add_preview(base/'other',root,juce,app,sha))
print('PASS preview PE64, exact source archives/licences/checksums, missing source, external runtime rejection; synthetic fixtures only')
