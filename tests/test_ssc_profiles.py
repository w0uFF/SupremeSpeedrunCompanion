import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import ssc_profiles as s


class Profiles(unittest.TestCase):
    def test_manifest(self):
        manifest = s.read_json(ROOT / 'integrations/launcher/ssc/mod.json')
        self.assertEqual(manifest, s.launcher_manifest())
        self.assertEqual(manifest['rules'], [])
        self.assertFalse(manifest['configuration']['load_diagnostic'])
        self.assertEqual(manifest['dlls'], [{'path': 'SSCNative.dll', 'initialize': 'SSCInitialize', 'enable_if': 'load_diagnostic'}])
        self.assertEqual(len({p['id'] for p in manifest['parameters']}), len(manifest['configuration']))

    def test_precedence(self):
        book = s.initial()
        state = {'active': {'enabled': ['ssc'], 'configuration': {'ssc': {'profile_name': 'Speedrun Tryhard', 'auto_record': False}}}}
        before = s.canonical(state)
        a = s.resolve(book, state)
        self.assertTrue(a['settings']['auto_record'])
        self.assertFalse(a['diagnostic_requested'])
        self.assertEqual(s.canonical(state), before)
        state['active']['configuration']['ssc'].update(use_launcher_overrides=True, load_diagnostic=True, bitrate_mbps=12.0)
        b = s.resolve(book, state)
        self.assertFalse(b['settings']['auto_record'])
        self.assertEqual(b['settings']['resolution'], '720p')
        self.assertEqual(b['settings']['bitrate_mbps'], 12)
        self.assertTrue(b['diagnostic_requested'])
        self.assertFalse(b['native_capabilities'])
        self.assertNotEqual(a['local_snapshot_sha256'], b['local_snapshot_sha256'])
        state['active']['enabled'] = []
        self.assertFalse(s.resolve(book, state)['diagnostic_requested'])
        self.assertTrue(book['profiles']['Speedrun Tryhard']['auto_record'])

    def test_edit_lifecycle(self):
        original = s.initial()
        book = s.edit(original, 'duplicate', 'Training', target='Practice 2')
        book = s.edit(book, 'replace', 'Practice 2', values=dict(s.defaults(), fps='60'))
        book = s.edit(book, 'select', 'Practice 2')
        book = s.edit(book, 'rename', 'Practice 2', target='Evening')
        self.assertEqual(book['active'], 'Evening')
        self.assertEqual(book['profiles']['Evening']['fps'], '60')
        with self.assertRaises(ValueError):
            s.edit(book, 'delete', 'Evening')
        book = s.edit(book, 'select', 'Training')
        book = s.edit(book, 'delete', 'Evening')
        self.assertEqual(book, original)
        with self.assertRaises(ValueError):
            s.edit(book, 'create', 'training', values=s.defaults())

    def test_invalid_settings(self):
        for bad in ({'auto_record': 1}, {'fps': '120'}, {'bitrate_mbps': 40.5}, {'min_free_gib': True},
                    {'restart_key': 'F6', 'record_key': 'F6'}, {'unknown': 1}, {'package_directory': 'relative'},
                    {'package_directory': 'D:\\..\\evil'}, {'package_directory': 'D:\\NUL.txt'}, {'package_directory': 'D:\\'},
                    {'package_directory': '\\\\server\\share'}, {'package_directory': 'D:\\a\nkey=value'}):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                s.settings(dict(s.defaults(), **bad))
        self.assertEqual(s.settings(dict(s.defaults(), package_directory='D:/Supreme/épreuve'))['package_directory'], 'D:\\Supreme\\épreuve')

    def test_bad_schema_or_launcher(self):
        with self.assertRaises(ValueError):
            s.validate_book(dict(s.initial(), schema=2))
        for value in ({'profile_name': 'missing'}, {'load_diagnostic': 'true'}, {'unknown': True}):
            with self.assertRaises(ValueError):
                s.resolve(s.initial(), {'active': {'configuration': {'ssc': value}}})

    def test_persistence_and_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'profiles.json'
            store = s.ProfileStore(path)
            store.save(s.initial(), create=True)
            before = path.read_bytes()
            with self.assertRaises(FileExistsError):
                store.save(s.initial(), create=True)
            changed = s.edit(store.load(), 'select', 'All Tracks')
            with patch.object(s.os, 'replace', side_effect=OSError('fixture')):
                with self.assertRaises(OSError):
                    store.save(changed)
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(list(Path(directory).iterdir()), [path])
            store.save(changed)
            self.assertEqual(s.ProfileStore(path).load(), changed)
            path.write_text('{"schema":1,"schema":2}')
            broken = path.read_bytes()
            with self.assertRaises(ValueError):
                store.save(s.initial())
            self.assertEqual(path.read_bytes(), broken)

    def test_bounded_read(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'large.json'
            path.write_bytes(b' ' * (s.MAX_BYTES + 1))
            with self.assertRaises(ValueError):
                s.read_json(path)


if __name__ == '__main__':
    unittest.main()
