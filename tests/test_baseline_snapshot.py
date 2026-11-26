import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import baseline_snapshot as baseline


class BaselineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'Saved_Data').mkdir()
        for name in baseline.CORE:
            (self.root / name).write_bytes(b'fixture')

    def test_paired_save_change(self):
        before = baseline.snapshot(self.root)
        (self.root / 'Saved_Data/Keyboard_Bindings.bin').write_bytes(b'new')
        after = baseline.snapshot(self.root)
        result = baseline.compare(before, after)
        self.assertEqual(result['core_observation'], 'unchanged')
        self.assertEqual(result['changes']['Saved_Data/Keyboard_Bindings.bin'], 'created')
        self.assertFalse(result['runtime_verified'])
        self.assertFalse(result['loader_gate_passed'])
        self.assertNotIn(str(self.root), json.dumps(after))
        self.assertEqual(after['files']['Supreme.exe']['sha256'], hashlib.sha256(b'fixture').hexdigest())

    def test_core_mutation_and_removal(self):
        before = baseline.snapshot(self.root)
        (self.root / 'Supreme.exe').write_bytes(b'other')
        (self.root / 'Main_Menu.dll').unlink()
        result = baseline.compare(before, baseline.snapshot(self.root))
        self.assertEqual(result['core_observation'], 'changed')
        self.assertEqual(result['changes']['Main_Menu.dll'], 'removed')

    def test_missing_and_bounded(self):
        (self.root / 'winmm.dll').unlink()
        with patch.object(baseline, 'MAX_BYTES', 3):
            report = baseline.snapshot(self.root)
            self.assertEqual(report['files']['Supreme.exe']['status'], 'too_large')
            self.assertEqual(baseline.compare(report, report)['core_observation'], 'incomplete')
        report = baseline.snapshot(self.root)
        self.assertEqual(baseline.compare(report, report)['changes']['winmm.dll'], 'absent_both')
        self.assertEqual(baseline.compare(report, report)['core_observation'], 'incomplete')

    def test_symlink(self):
        link = self.root / 'Saved_Data/Defaults.txt'
        try:
            link.symlink_to(self.root / 'Supreme.exe')
        except (OSError, NotImplementedError):
            self.skipTest('symbolic link creation unavailable')
        report = baseline.snapshot(self.root)
        self.assertEqual(report['files']['Saved_Data/Defaults.txt']['status'], 'unsupported_link')
        self.assertEqual(baseline.compare(report, report)['changes']['Saved_Data/Defaults.txt'], 'unknown')

    def test_malformed_snapshot(self):
        report = baseline.snapshot(self.root)
        for change in ({'schema': True}, {'scope': 'other'}, {'files': {}}, {'schema': 2}):
            with self.assertRaises(ValueError):
                baseline.compare(report, dict(report, **change))
        broken = copy.deepcopy(report)
        broken['files']['Supreme.exe']['sha256'] = 'z' * 64
        with self.assertRaises(ValueError):
            baseline.compare(report, broken)

    def test_report_bound(self):
        path = self.root / 'report.json'
        path.write_bytes(b' ' * (baseline.MAX_REPORT + 1))
        with self.assertRaises(ValueError):
            baseline.read_report(path)

    def test_no_game_files_modified(self):
        before = {p: p.read_bytes() for p in self.root.iterdir() if p.is_file()}
        baseline.snapshot(self.root)
        self.assertEqual(before, {p: p.read_bytes() for p in self.root.iterdir() if p.is_file()})


if __name__ == '__main__':
    unittest.main()
