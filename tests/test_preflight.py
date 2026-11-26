import hashlib
import pathlib
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from preflight import inspect


def sha(s):
    return hashlib.sha256(s).hexdigest()


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.profile = {'id': 'synthetic', 'engine_files': ['engine.dll'], 'files': {
            'engine.dll': {'reference': sha(b'core')},
            'Supreme.exe': {'original': sha(b'game'), 'keyboard_v01': sha(b'patch')},
            'Display_Config.exe': {'bundled': sha(b'old'), 'display_v180': sha(b'new')},
            'winmm.dll': {'display_v180': sha(b'proxy')}}}
        self.write('engine.dll', b'core')
        self.write('Supreme.exe', b'game')
        self.write('Display_Config.exe', b'old')

    def write(self, name, data):
        (self.root / name).write_bytes(data)

    def check(self, expected):
        before = {p.name: p.read_bytes() for p in self.root.iterdir()}
        result = inspect(self.root, self.profile)
        self.assertEqual(expected, result['combination'])
        self.assertEqual([], result['engine_capabilities'])
        self.assertEqual('blocked_pending_loader_gate', result['native_integration'])
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.root.iterdir()})

    def test_reference(self):
        self.check('supremelight_reference')

    def test_display(self):
        self.write('Display_Config.exe', b'new')
        self.write('winmm.dll', b'proxy')
        self.check('display_v180_original')
        self.write('Supreme.exe', b'patch')
        self.check('display_v180_keyboard_v01')

    def test_mixed_and_unknown(self):
        self.write('winmm.dll', b'proxy')
        self.check('unknown_or_incomplete')
        self.write('Display_Config.exe', b'new')
        self.write('Supreme.exe', b'foreign')
        self.check('unknown_or_incomplete')

    def test_missing_engine(self):
        (self.root / 'engine.dll').unlink()
        self.check('unknown_or_incomplete')

    def test_modified_engine(self):
        self.write('engine.dll', b'changed')
        self.check('unknown_or_incomplete')

    def test_missing_proxy(self):
        self.write('Display_Config.exe', b'new')
        self.check('unknown_or_incomplete')

    def test_unknown_proxy(self):
        self.write('Display_Config.exe', b'new')
        self.write('winmm.dll', b'other')
        self.check('unknown_or_incomplete')


if __name__ == '__main__':
    unittest.main()
