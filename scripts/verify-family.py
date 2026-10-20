#!/usr/bin/env python3
"""Check the exact shared family assets and the two source manuals, offline."""
from pathlib import Path
import hashlib
import re

root = Path(__file__).resolve().parent.parent
# Identical Git blobs in Piano 026f045, Drums 7f1def6 and Guitar e571f8d.
expected = {
    'Source/FamilyTheme.h': 'd61097f13d49dd041e06364a62b130f95d4e38e6',
    'Resources/Inter-Regular.otf': '2d0bd1d645a9afaa4cf47afc4e1dc38698809f78',
    'Resources/Inter-SemiBold.otf': '52c84550ba03fa5543f14c5cb215b9b2c72946cb',
}
for path, sha in expected.items():
    data = (root / path).read_bytes()
    if path.endswith(".h"):
        data = data.replace(b"\r\n", b"\n")  # Git autocrlf on Windows; canonical Git blob is LF.
    actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    assert actual == sha, f'{path}: family asset has diverged ({actual})'
for name in ['GUIDA', 'GUIDE']:
    doc = root / 'docs' / f'{name}.md'
    text = doc.read_text(encoding='utf-8')
    assert len(re.findall(r'^## [0-9]+\.', text, re.M)) == 13, doc
    for target in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', text):
        if target.startswith(('https://', 'http://', '#', 'mailto:')):
            continue
        assert (doc.parent / target.split('#')[0]).exists(), (doc, target)
for name in ['README.it.md', 'CHANGELOG.md', 'CHANGELOG.it.md', 'PRIVACY.md', 'PRIVACY.it.md']:
    assert (root / name).is_file(), name
print('PASS exact family theme + Inter fonts, IT/EN Markdown manuals and local links')
