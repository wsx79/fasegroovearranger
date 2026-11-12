#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Native runner coverage/report-gate tests, mocked: no native platform PASS implied."""
from pathlib import Path
import runpy
import sys
from tempfile import TemporaryDirectory
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class NativeRenderGateTests(unittest.TestCase):
    def run_recipe(self, report, system='Linux'):
        with TemporaryDirectory() as tmp:
            folder = Path(tmp)
            support = types.ModuleType('native_test_support')
            support.build_root = lambda value=None: folder
            support.output_folder = lambda name, requested=None: folder
            support.verify = lambda *args: None
            calls = []

            def run_application(build, flag, output, timeout=300):
                calls.append(flag)
                output.mkdir(parents=True, exist_ok=True)
                names = {'--startup-splash-test': 'startup.txt', '--eq-ui-test': 'eq-checks.txt',
                         '--piano-roll-test': 'piano-roll.txt', '--mastering-ui-test': 'mastering-ui.txt'}
                if flag in names:
                    value = report if flag == '--mastering-ui-test' else 'PASS fixture\n'
                    if value is not None:
                        (output / names[flag]).write_text(value, encoding='utf-8')

            support.run_application = run_application
            with patch.dict(sys.modules, {'native_test_support': support}), \
                 patch.object(sys, 'argv', ['test-native-render.py']), \
                 patch('platform.system', return_value=system), patch.dict('os.environ'):
                runpy.run_path(str(ROOT / 'scripts/test-native-render.py'), run_name='__main__')
            self.assertIn('--mastering-ui-test', calls)
            self.assertIn('--smoke-test', calls)

    def test_mastering_is_checked_on_each_native_runner(self):
        for system in ('Linux', 'Windows', 'Darwin'):
            with self.subTest(system=system):
                self.run_recipe('PASS analysis\nPASS Cancel\n', system)

    def test_missing_or_incomplete_mastering_cannot_pass(self):
        for report in (None, '', 'FAIL analysis\n', 'NOT RUN mastering\n',
                       'PASS analysis\nFAIL Cancel\n', 'PASS analysis\nNOT RUN preview\n'):
            with self.subTest(report=report):
                with self.assertRaises((RuntimeError, FileNotFoundError)):
                    self.run_recipe(report)


if __name__ == '__main__':
    unittest.main()
