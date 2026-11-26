import struct
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import evidence_archive as ea


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.parent = Path(tmp.name)
        self.root = self.parent / 'stage'
        self.root.mkdir()
        (self.root / 'clip.mp4').write_bytes(b'abc')
        self.path = self.parent / 'run.zip'

    def create(self):
        return ea.create(self.root, {'clip.mp4': 'video'}, self.path)

    def rewrite(self, edit):
        with zipfile.ZipFile(self.path) as z:
            members = [(i.filename, z.read(i)) for i in z.infolist()]
        with zipfile.ZipFile(self.path, 'w') as z:
            for name, data in edit(members):
                z.writestr(name, data)

    def test_roundtrip_exclusive(self):
        report = self.create()
        self.assertEqual(report, ea.verify(self.path))
        self.assertEqual(report['bytes_verified'], 3)
        self.assertFalse(report['eligibility_verified'])
        before = self.path.read_bytes()
        with self.assertRaises(FileExistsError):
            self.create()
        self.assertEqual(self.path.read_bytes(), before)
        self.assertEqual((self.root / 'clip.mp4').read_bytes(), b'abc')

    def test_digest_mismatch(self):
        self.create()
        self.rewrite(lambda members: [(n, b'xyz' if n == 'clip.mp4' else d) for n, d in members])
        with self.assertRaises(ValueError):
            ea.verify(self.path)

    def test_extra_and_traversal(self):
        for name in ('../escape', 'CLIP.MP4', 'extra.txt'):
            if self.path.exists():
                self.path.unlink()
            self.create()
            self.rewrite(lambda members: members + [(name, b'x')])
            with self.assertRaises(ValueError):
                ea.verify(self.path)
        self.assertFalse((self.parent / 'escape').exists())

    def test_manifest_duplicate_key(self):
        self.create()
        self.rewrite(lambda members: [(n, b'{"schema":1,"schema":1}' if n == ea.MANIFEST else d) for n, d in members])
        with self.assertRaises(ValueError):
            ea.verify(self.path)

    def test_limits_and_cleanup(self):
        with patch.object(ea, 'MAX_TOTAL', 2):
            with self.assertRaises(ValueError):
                self.create()
        self.assertFalse(self.path.exists())
        with patch.object(ea, 'verify', side_effect=ValueError('fixture')):
            with self.assertRaises(ValueError):
                self.create()
        self.assertFalse(self.path.exists())
        with self.assertRaises(ValueError):
            ea.create(self.root, {'clip.mp4': 'video'}, self.root / 'out.zip')

    def test_zip64_footer_rejected(self):
        self.create()
        raw = self.path.read_bytes()
        signature, disk, start, disk_count, count, size, offset, comment = struct.unpack('<4s4H2IH', raw[-22:])
        extension = (struct.pack('<4sQ2H2I4Q', b'PK\x06\x06', 44, 45, 45, 0, 0,
                                 count, count, size, offset)
                     + struct.pack('<4sIQI', b'PK\x06\x07', 0, len(raw) - 22, 1))
        footer = struct.pack('<4s4H2IH', signature, disk, start, disk_count, count,
                             size + len(extension), offset, comment)
        self.path.write_bytes(raw[:-22] + extension + footer)
        with zipfile.ZipFile(self.path) as archive:
            self.assertEqual(archive.read('clip.mp4'), b'abc')
        with self.assertRaisesRegex(ValueError, 'ZIP64 central directory'):
            ea.verify(self.path)

    def test_compressed_rejected(self):
        self.create()
        with zipfile.ZipFile(self.path) as z:
            members = [(i.filename, z.read(i)) for i in z.infolist()]
        with zipfile.ZipFile(self.path, 'w', compression=zipfile.ZIP_DEFLATED) as z:
            for name, data in members:
                z.writestr(name, data)
        with self.assertRaises(ValueError):
            ea.verify(self.path)


if __name__ == '__main__':
    unittest.main()
