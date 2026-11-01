#!/usr/bin/env python3
"""Validate diagnostic output without crashing the application or using user logs."""
from pathlib import Path
import platform
import tempfile
from native_test_support import build_root, run_application

with tempfile.TemporaryDirectory(prefix='arranger-diagnostic-') as folder:
    report=Path(folder)/'crash.txt'
    run_application(build_root(),'--crash-report-test',report)
    text=report.read_text(encoding='utf-8-sig')
    assert text.startswith('Fase Groove Arranger ') and ' crash\n' in text
    if platform.system()=='Windows':
        dump=report.with_suffix('.dmp')
        assert 'Thread: ' in text and 'Minidump: crash.dmp' in text
        with dump.open('rb') as f: assert f.read(4)==b'MDMP'
    print('PASS crash report version, native metadata and Windows minidump')
