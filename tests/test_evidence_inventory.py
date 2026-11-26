from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import evidence_inventory as evidence


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / 'clip.mp4').write_bytes(b'abc')
        (self.root / 'timing.csv').write_bytes(b'')

    def test_known_hash_and_order(self):
        report = evidence.inventory(self.root, {'timing.csv': 'timing', 'clip.mp4': 'video'})
        self.assertEqual(report['files'][0], {'path': 'clip.mp4', 'size': 3, 'role': 'video',
                         'sha256': 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'})
        self.assertEqual(report['files'][1]['size'], 0)
        self.assertFalse(report['complete_package'])
        self.assertFalse(report['eligibility_verified'])
        self.assertEqual((self.root / 'clip.mp4').read_bytes(), b'abc')

    def test_unsafe_selection(self):
        for name in ('../clip.mp4', '/clip.mp4', 'a\\b', 'C:clip.mp4', 'CON.txt', 'x.', 'manifest.json', 'SHA256SUMS.txt'):
            with self.assertRaises(ValueError):
                evidence.inventory(self.root, {name: 'video'})
        with self.assertRaises(ValueError):
            evidence.inventory(self.root, {'clip.mp4': 'video', 'CLIP.MP4': 'video'})
        with self.assertRaises(ValueError):
            evidence.inventory(self.root, {'clip.mp4': 'unknown'})
        with self.assertRaises(OSError):
            evidence.inventory(self.root, {'missing.mp4': 'video'})

    def test_size_bound(self):
        with patch.object(evidence, 'MAX_BYTES', 2):
            with self.assertRaises(ValueError):
                evidence.inventory(self.root, {'clip.mp4': 'video'})

    def test_link(self):
        try:
            (self.root / 'link.mp4').symlink_to(self.root / 'clip.mp4')
        except (OSError, NotImplementedError):
            self.skipTest('symbolic link creation unavailable')
        with self.assertRaises(ValueError):
            evidence.inventory(self.root, {'link.mp4': 'video'})


if __name__ == '__main__':
    unittest.main()
