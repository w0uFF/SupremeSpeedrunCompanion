from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ssc_profiles as s

class Exchange(unittest.TestCase):
    def test_roundtrip_and_privacy(self):
        with tempfile.TemporaryDirectory() as root:
            dest = Path(root) / 'share.json'
            book = s.edit(s.initial(), 'replace', 'Training', values=dict(s.defaults(),
                          package_directory='D:\\Private\\Runs', fps='60'))
            s.export_profile(book, 'Training', dest)
            original = dest.read_bytes()
            self.assertNotIn(b'Private', original)
            with self.assertRaises(FileExistsError):
                s.export_profile(book, 'Training', dest)
            self.assertEqual(dest.read_bytes(), original)
            with self.assertRaises(ValueError):
                s.import_profile(book, dest, target='training')
            imported = s.import_profile(book, dest, target='Shared')
            self.assertEqual(imported['active'], 'Training')
            self.assertEqual(imported['profiles']['Shared']['fps'], '60')
            self.assertEqual(imported['profiles']['Shared']['package_directory'], '')
            self.assertEqual(book['profiles']['Training']['package_directory'], 'D:\\Private\\Runs')
            self.assertNotIn('Shared', book['profiles'])

    def test_invalid_import(self):
        with tempfile.TemporaryDirectory() as root:
            file = Path(root) / 'share.json'
            book = s.initial()
            for payload in (s.canonical(book), b'{"schema":2}', b'{"schema":1,"schema":1}', b' '*(s.MAX_BYTES+1)):
                file.write_bytes(payload)
                with self.assertRaises(ValueError):
                    s.import_profile(book, file)
                self.assertEqual(file.read_bytes(), payload)
            self.assertEqual(book, s.initial())

    def test_stale_editor(self):
        with tempfile.TemporaryDirectory() as root:
            store = s.ProfileStore(Path(root) / 'profiles.json')
            original = s.initial()
            store.save(original, create=True)
            changed = s.edit(original, 'select', 'All Tracks')
            store.save(changed, expected=original)
            before = store.path.read_bytes()
            with self.assertRaisesRegex(ValueError, 'state changed'):
                store.save(s.edit(original, 'select', 'Low Performance'), expected=original)
            self.assertEqual(store.path.read_bytes(), before)

if __name__ == '__main__':
    unittest.main()
