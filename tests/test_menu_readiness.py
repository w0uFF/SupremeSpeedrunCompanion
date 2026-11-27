import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import menu_readiness as readiness


class ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def put(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def test_missing(self):
        report = readiness.observe(self.root)
        self.assertFalse(report['reference_menu_match'])
        self.assertEqual(report['loader_observation'], 'missing')
        self.assertTrue(all(item['status'] == 'missing' for item in report['menu'].values()))
        self.assertFalse(report['loader_gate_passed'])

    def test_reference_and_changed(self):
        expected = {name: hashlib.sha256(name.encode()).hexdigest() for name in readiness.MENU}
        for name in expected:
            self.put(name, name.encode())
        with patch.object(readiness, 'MENU', expected):
            report = readiness.observe(self.root)
            self.assertTrue(report['reference_menu_match'])
            self.assertFalse(report['runtime_verified'])
            self.assertFalse(report['native_registration_verified'])
            self.assertFalse(report['loader_provenance_verified'])
            self.assertEqual(report['engine_capabilities'], [])
            self.put(next(iter(expected)), b'changed')
            self.assertFalse(readiness.observe(self.root)['reference_menu_match'])

    def test_proxy_is_not_loader_provenance(self):
        self.put('winmm.dll', b'unknown proxy')
        report = readiness.observe(self.root)
        self.assertEqual(report['binaries']['winmm.dll']['variant'], 'unknown')
        self.assertEqual(report['loader_observation'], 'unreviewed_or_unavailable')
        self.assertFalse(report['loader_provenance_verified'])
        self.assertFalse(report['loader_gate_passed'])

    def test_unsupported_type_and_invalid_root(self):
        name = next(iter(readiness.MENU))
        (self.root / name).mkdir(parents=True)
        self.assertEqual(readiness.observe(self.root)['menu'][name]['status'], 'unsupported_type')
        with self.assertRaises(ValueError):
            readiness.observe(self.put('file', b'x'))

    def test_symlink(self):
        name = next(iter(readiness.MENU))
        target = self.put('target', b'not a menu file')
        link = self.root / name
        link.parent.mkdir(parents=True)
        try:
            link.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest('symlink creation unavailable')
        item = readiness.observe(self.root)['menu'][name]
        self.assertEqual(item, {'status': 'unsupported_link'})


if __name__ == '__main__':
    unittest.main()
