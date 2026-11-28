from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import archive_size_preflight as preflight
import evidence_archive as archive
import evidence_inventory as inventory
import attempt_settings_evidence as settings_evidence

META = dict(run_id='1'*32, session_id='2'*32, created_utc='2026-01-02T03:04:05Z', mod_build='test',
            origin='synthetic', recording_mode='manual', attempt_status='finished', capture_status='finalized')


class PreflightTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.parent = Path(tmp.name)
        self.root = self.parent / 'stage'
        self.root.mkdir()
        self.video = self.root / 'clip.mp4'
        self.video.write_bytes(b'abc')
        self.selected = {'clip.mp4': 'video'}

    def test_no_reads_and_exact_writer_manifest(self):
        for metadata in (None, dict(META, track_name='Épreuve', warnings=['测试'])):
            with patch.object(Path, 'open', side_effect=AssertionError('payload opened')):
                result = preflight.inspect(self.root, self.selected, metadata=metadata)
            self.assertEqual(result.state, 'within_size_limits')
            self.assertEqual(result.payload_bytes, 3)
            output = self.parent / ('legacy.zip' if metadata is None else 'schema2.zip')
            report = archive.create(self.root, self.selected, output, metadata=metadata)
            with zipfile.ZipFile(output) as z:
                self.assertEqual(z.getinfo(archive.MANIFEST).file_size, result.manifest_bytes)
            self.assertLessEqual(output.stat().st_size, result.container_upper_bytes)
            self.assertFalse(report['eligibility_verified'])
        self.assertEqual(self.video.read_bytes(), b'abc')

    def test_exact_payload_and_inventory_boundaries(self):
        # Small injected limits exercise boundaries without large allocation on Windows.
        with patch.object(archive, 'MAX_TOTAL', 3), patch.object(inventory, 'MAX_BYTES', 4):
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'within_size_limits')
            self.video.write_bytes(b'abcd')
            with patch.object(Path, 'open', side_effect=AssertionError('payload opened')):
                result = preflight.inspect(self.root, self.selected)
            self.assertEqual((result.state, result.payload_bytes), ('archive_payload_too_large', 4))
            self.video.write_bytes(b'abcde')
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'inventory_file_too_large')
        self.assertEqual(self.video.read_bytes(), b'abcde')
        self.assertEqual(list(self.parent.iterdir()), [self.root])

    def test_aggregate_and_zero(self):
        other = self.root / 'events.txt'
        other.write_bytes(b'xy')
        selection = dict(self.selected, **{'events.txt': 'diagnostic'})
        with patch.object(archive, 'MAX_TOTAL', 4):
            self.assertEqual(preflight.inspect(self.root, selection).state, 'archive_payload_too_large')
            other.write_bytes(b'')
            result = preflight.inspect(self.root, selection)
            self.assertEqual((result.state, result.payload_bytes), ('within_size_limits', 3))
        self.video.write_bytes(b'')
        self.assertEqual(preflight.inspect(self.root, selection).payload_bytes, 0)
        self.assertEqual(preflight.inspect(self.root, selection, metadata=META).state, 'invalid_metadata')

    def test_manifest_and_container_bounds(self):
        result = preflight.inspect(self.root, self.selected)
        with patch.object(archive, 'MAX_MANIFEST', result.manifest_bytes - 1):
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'manifest_too_large')
        with patch.object(archive, 'MAX_DIRECTORY', 1):
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'container_bound_unavailable')
        with patch.object(archive, 'MAX_TOTAL', 3), patch.object(archive, 'MAX_MANIFEST', result.manifest_bytes), patch.object(archive, 'CHUNK', 1):
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'container_bound_unavailable')
        self.assertEqual(preflight.inspect(self.root, self.selected, metadata={'bad': True}).state, 'invalid_metadata')

    def test_invalid_selection_and_unavailable(self):
        for selected in ({}, {'../clip.mp4': 'video'}, {'CLIP.MP4': 'video', **self.selected},
                         {'clip.mp4': []}, {'clip.mp4': 'unknown'}, {'manifest.json': 'diagnostic'},
                         {f'f{i}': 'diagnostic' for i in range(65)}):
            self.assertEqual(preflight.inspect(self.root, selected).state, 'invalid_selection')
        self.assertEqual(preflight.inspect(self.root, {'missing.mp4': 'video'}).state, 'unavailable')
        (self.root / 'directory').mkdir()
        self.assertEqual(preflight.inspect(self.root, {'directory': 'video'}).state, 'unsupported_source')
        self.assertEqual(preflight.inspect(self.video, self.selected).state, 'unsupported_source')
        with patch.object(Path, 'stat', side_effect=PermissionError('fixture')):
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'unavailable')

    def test_actual_sparse_oversize(self):
        # Actual sparse file, only POSIX. Never write GiB of payload; skip elsewhere.
        import os
        if os.name != 'posix':
            self.skipTest('POSIX sparse fixture only')
        with self.video.open('r+b') as handle:
            handle.truncate(archive.MAX_TOTAL + 1)
        before = self.video.stat()
        if getattr(before, 'st_blocks', 0) * 512 > 1024 * 1024:
            self.skipTest('filesystem did not provide sparse fixture')
        with patch.object(Path, 'open', side_effect=AssertionError('payload opened')):
            result = preflight.inspect(self.root, self.selected)
        self.assertEqual((result.state, result.payload_bytes), ('archive_payload_too_large', archive.MAX_TOTAL + 1))
        after = self.video.stat()
        self.assertEqual((before.st_size, before.st_mtime_ns, before.st_ino), (after.st_size, after.st_mtime_ns, after.st_ino))
        self.assertEqual(list(self.parent.iterdir()), [self.root])

    def test_actual_manifest_and_settings_limits(self):
        # Valid bounded Unicode warnings can exceed the serialized manifest limit.
        result = preflight.inspect(self.root, self.selected, metadata=dict(META, warnings=['🟦' * 256] * 31))
        self.assertEqual(result.state, 'manifest_too_large')
        self.assertGreater(result.manifest_bytes, archive.MAX_MANIFEST)
        settings = self.root / settings_evidence.NAME
        settings.write_bytes(b'x' * (settings_evidence.MAX_BYTES + 1))
        selected = dict(self.selected, **{settings_evidence.NAME: 'diagnostic'})
        self.assertEqual(preflight.inspect(self.root, selected, metadata=META).state, 'settings_member_too_large')
        self.assertEqual(preflight.inspect(self.root, selected).state, 'within_size_limits')

    def test_maximum_names_and_count(self):
        selected = dict(self.selected)
        for i in range(63):
            name = f'{i:02d}' + 'x' * 125
            (self.root / name).write_bytes(b'')
            selected[name] = 'diagnostic'
        result = preflight.inspect(self.root, selected, metadata=META)
        self.assertEqual(result.state, 'within_size_limits')
        output = self.parent / 'many.zip'
        archive.create(self.root, selected, output, metadata=META)
        with zipfile.ZipFile(output) as z:
            self.assertEqual(len(z.infolist()), 65)
            self.assertEqual(z.getinfo(archive.MANIFEST).file_size, result.manifest_bytes)
        self.assertLessEqual(output.stat().st_size, result.container_upper_bytes)

    def test_linked_source(self):
        import os
        if os.name != 'posix':
            self.skipTest('POSIX symlink fixture only')
        link = self.root / 'link.mp4'
        link.symlink_to(self.video)
        self.assertEqual(preflight.inspect(self.root, {'link.mp4': 'video'}).state, 'unsupported_source')
        root_link = self.parent / 'linked-stage'
        root_link.symlink_to(self.root, target_is_directory=True)
        self.assertEqual(preflight.inspect(root_link, self.selected).state, 'unsupported_source')
        self.assertEqual(self.video.read_bytes(), b'abc')

    def test_stale_observation_not_authority(self):
        result = preflight.inspect(self.root, self.selected)
        self.assertEqual(result.state, 'within_size_limits')
        self.video.write_bytes(b'longer')
        with patch.object(archive, 'MAX_TOTAL', 3):
            self.assertEqual(preflight.inspect(self.root, self.selected).state, 'archive_payload_too_large')
            with self.assertRaises(ValueError):
                archive.create(self.root, self.selected, self.parent / 'run.zip')
        self.assertFalse((self.parent / 'run.zip').exists())
        self.assertEqual(self.video.read_bytes(), b'longer')


if __name__ == '__main__':
    unittest.main()
