"""Actual inert ZIP fixtures; injected index, no native game or database access."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import archive_exchange_adapter as ax
import archive_publish as ap
import evidence_archive as ea
import index_archive as ia

RUN = '1' * 32
META = dict(run_id=RUN, session_id='2' * 32, created_utc='2026-01-01T00:00:00Z',
            mod_build='test', origin='local', recording_mode='manual',
            attempt_status='finished', capture_status='finalized')


class Index:
    def __init__(self):
        self.rows = {}
        self.calls = 0
        self.fail = None

    def lookup(self, run_id, report, origin):
        row = self.rows.get(run_id)
        if row is None:
            return 'missing'
        if report is None:
            return 'present'
        return 'identical' if row == ia.project(report, origin) else 'conflict'

    def insert(self, report, origin):
        self.calls += 1
        if self.fail == 'before':
            raise OSError('injected index failure')
        run_id = report['run_id']
        answer = self.lookup(run_id, report, origin)
        if answer == 'missing':
            self.rows[run_id] = ia.project(report, origin)
            answer = 'inserted'
        if self.fail == 'after':
            raise OSError('injected lost acknowledgement')
        return answer


class ExchangeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        self.managed = self.root / 'managed'
        self.managed.mkdir()
        self.stage = self.root / 'stage'
        self.stage.mkdir()
        (self.stage / 'clip.mp4').write_bytes(b'inert video fixture')
        self.source = self.root / 'source-\u00e9.zip'
        ea.create(self.stage, {'clip.mp4': 'video'}, self.source, metadata=META)
        self.original = self.source.read_bytes()
        self.repack = self.root / 'repacked.zip'
        with zipfile.ZipFile(self.source) as inp, zipfile.ZipFile(self.repack, 'w') as out:
            for name in inp.namelist():
                out.writestr(zipfile.ZipInfo(name, (2001, 1, 1, 0, 0, 0)), inp.read(name))
        self.other = self.repack.read_bytes()
        self.assertNotEqual(self.original, self.other)
        self.index = Index()
        self.adapter = ax.Adapter(self.managed, self.index.lookup, self.index.insert)
        self.expected = self.adapter.selected(self.source).identity
        self.assertIsNotNone(self.expected)
        self.assertEqual(self.adapter.selected(self.repack).identity.projection, self.expected.projection)
        self.final, self.partial = ax.pair_paths(self.managed / ('imported-' + RUN + '.zip'))
        self.dest = self.root / 'export-\u96ea.zip'
        self.dp = ax.pair_paths(self.dest)[1]

    def guard(self):
        return dict(expected_projection=self.expected.projection, expected_container=self.expected.container)

    def strict(self):
        return ia.admit(self.source, self.managed, 'imported', self.index.insert, **self.guard())

    def import_now(self):
        return self.adapter.admit_import(self.source, self.expected, self.adapter.managed(RUN, 'imported'))

    def export_now(self):
        return self.adapter.publish_exact(self.expected, self.dest, self.adapter.destination(self.dest, 'imported'))

    def assert_clean(self):
        self.assertFalse(self.final.exists())
        self.assertFalse(self.partial.exists())
        self.assertEqual(self.index.calls, 0)

    def test_guard_arguments_and_projection(self):
        for args in (dict(expected_projection=self.expected.projection),
                     dict(expected_container=self.expected.container),
                     dict(expected_projection=b'x', expected_container=self.expected.container),
                     dict(expected_projection=self.expected.projection, expected_container=(True, 'a'*64))):
            with patch.object(ia, 'checked', side_effect=AssertionError('must validate first')):
                with self.assertRaises(ValueError):
                    ia.admit(self.source, self.managed, 'imported', self.index.insert, **args)
        wrong = self.expected.projection.replace(b'unclassified', b'classified')
        with self.assertRaises(ValueError):
            ia.admit(self.source, self.managed, 'imported', self.index.insert,
                     expected_projection=wrong, expected_container=self.expected.container)
        self.assert_clean()

    def test_default_and_strict_dedup(self):
        with patch.object(ia, 'container_identity', side_effect=AssertionError('legacy must not hash container')):
            ia.admit(self.repack, self.managed, 'imported', self.index.insert)
            ia.admit(self.source, self.managed, 'imported', self.index.insert)
            ia.admit(self.final, self.managed, 'imported', self.index.insert)
        with self.assertRaises(ValueError):
            self.strict()
        self.assertEqual(self.final.read_bytes(), self.other)
        self.assertEqual(self.source.read_bytes(), self.original)
        self.assertEqual(self.index.calls, 3)

    def test_pending_conflict_and_exact_alias(self):
        self.partial.write_bytes(self.other)
        with self.assertRaises(ValueError):
            self.strict()
        self.assertEqual(self.partial.read_bytes(), self.other)
        self.partial.write_bytes(self.original)
        os.link(self.partial, self.final)
        self.assertEqual(self.strict()['index'], 'inserted')
        self.assertFalse(self.partial.exists())
        self.assertEqual(self.final.read_bytes(), self.original)
        with self.assertRaises(ValueError):
            ia.admit(self.final, self.managed, 'imported', self.index.insert, **self.guard())

    def test_before_and_during_copy(self):
        original_check = ia.checked_identity
        calls = []

        def check(path, origin):
            result = original_check(path, origin)
            calls.append(path)
            if len(calls) == 1:
                self.source.write_bytes(self.other)
            return result

        with patch.object(ia, 'checked_identity', side_effect=check):
            with self.assertRaises(ValueError):
                self.strict()
        self.assert_clean()
        self.source.write_bytes(self.original)
        copy = ia.copy_new

        def changed(source, partial):
            copy(source, partial)
            self.source.write_bytes(self.other)

        with patch.object(ia, 'copy_new', side_effect=changed):
            with self.assertRaises(ValueError):
                self.strict()
        self.assertFalse(self.final.exists())
        self.assertEqual(self.partial.read_bytes(), self.original)
        self.assertEqual(self.index.calls, 0)
        self.source.write_bytes(self.original)
        self.assertEqual(self.strict()['index'], 'inserted')

    def test_publication_and_final_guard(self):
        def change_pending(final, verify):
            self.partial.write_bytes(self.other)
            return ap.publish(final, verify)

        with patch.object(ia, 'publish', side_effect=change_pending):
            with self.assertRaises(ValueError):
                self.strict()
        self.assertFalse(self.final.exists())
        self.assertEqual(self.partial.read_bytes(), self.other)
        self.partial.write_bytes(self.original)

        def change_final(final, verify):
            report = ap.publish(final, verify)
            final.write_bytes(self.other)
            return report

        with patch.object(ia, 'publish', side_effect=change_final):
            with self.assertRaises(ValueError):
                self.strict()
        self.assertEqual(self.final.read_bytes(), self.other)
        self.assertEqual(self.index.calls, 0)

    def test_adapter_import_retry_and_index_conflict(self):
        before = self.adapter.managed(RUN, 'imported')
        self.assertEqual(before, ax.Slot('absent', index='missing'))
        self.index.fail = 'before'
        with self.assertRaises(OSError):
            self.import_now()
        self.assertEqual(self.final.read_bytes(), self.original)
        self.assertEqual(self.adapter.managed(RUN, 'imported').index, 'missing')
        self.index.fail = 'after'
        with self.assertRaises(OSError):
            self.import_now()
        self.index.fail = None
        self.assertEqual(self.import_now(), 'unchanged')
        self.assertEqual(self.index.calls, 2)
        with self.assertRaises(ValueError):
            self.adapter.admit_import(self.source, self.expected, before)
        self.index.rows[RUN] = b'conflicting row'
        with self.assertRaises(ValueError):
            self.import_now()
        self.assertEqual(self.source.read_bytes(), self.original)
        self.assertEqual(self.final.read_bytes(), self.original)

    def test_adapter_alias_and_orphan(self):
        self.partial.write_bytes(self.original)
        os.link(self.partial, self.final)
        self.assertEqual(self.adapter.managed(RUN, 'imported').state, 'alias')
        self.assertEqual(self.import_now(), 'completed')
        self.assertFalse(self.partial.exists())
        self.final.unlink()
        with self.assertRaises(ValueError):
            self.import_now()
        self.assertFalse(self.final.exists())
        self.assertEqual(self.index.calls, 1)

    def test_export_exact_conflicts_and_interruption(self):
        self.assertEqual(self.import_now(), 'completed')
        self.dest.write_bytes(self.other)
        with self.assertRaises(ValueError):
            self.export_now()
        self.assertEqual(self.dest.read_bytes(), self.other)
        self.dest.unlink()
        with patch.object(ap.os, 'link', side_effect=OSError('injected interruption')):
            with self.assertRaises(OSError):
                self.export_now()
        self.assertEqual(self.dp.read_bytes(), self.original)
        self.assertFalse(self.dest.exists())
        self.assertEqual(self.export_now(), 'completed')
        self.assertEqual(self.export_now(), 'unchanged')
        self.assertEqual(self.dest.read_bytes(), self.original)
        os.link(self.dest, self.dp)
        with self.assertRaises(ValueError):
            self.export_now()
        self.assertTrue(self.dp.exists())
        self.assertEqual(self.index.calls, 1)
        self.assertEqual(self.final.read_bytes(), self.original)

    def test_export_index_drift_and_corrupt_pending(self):
        self.import_now()
        self.dp.write_bytes(b'corrupt')
        with self.assertRaises(ValueError):
            self.export_now()
        self.assertEqual(self.dp.read_bytes(), b'corrupt')
        self.dp.unlink()
        copy = ia.copy_new

        def drift(source, partial):
            copy(source, partial)
            self.index.rows[RUN] = b'changed'

        with patch.object(ia, 'copy_new', side_effect=drift):
            with self.assertRaises(ValueError):
                self.export_now()
        self.assertFalse(self.dest.exists())
        self.assertEqual(self.dp.read_bytes(), self.original)
        self.assertEqual(self.final.read_bytes(), self.original)

    def test_protected_paths_and_hardlinks(self):
        self.import_now()
        for path in (self.source, self.dest, self.dp):
            protected = ax.Adapter(self.managed, self.index.lookup, self.index.insert, protected=(path,))
            if path == self.source:
                self.assertEqual(protected.selected(path).state, 'review')
            else:
                with self.assertRaises(ValueError):
                    protected.publish_exact(self.expected, self.dest, ax.Slot('absent'))
        with self.assertRaises(ValueError):
            self.adapter.publish_exact(self.expected, self.final, ax.Slot('absent'))
        os.link(self.final, self.dest)
        self.assertEqual(self.adapter.managed(RUN, 'imported').state, 'review')
        with self.assertRaises(ValueError):
            self.export_now()
        self.assertEqual(self.final.read_bytes(), self.original)
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_symlink_and_member_policy_unchanged(self):
        (self.stage / 'payload.exe').write_bytes(b'inert diagnostic, not executable')
        alternate = self.root / 'diagnostic.zip'
        ea.create(self.stage, {'clip.mp4': 'video', 'payload.exe': 'diagnostic'}, alternate, metadata=META)
        self.assertEqual(self.adapter.selected(alternate).state, 'finalized')
        self.assertFalse((self.root / 'payload.exe').exists())
        alias = self.root / 'alias.zip'
        try:
            alias.symlink_to(self.source)
        except (OSError, NotImplementedError):
            self.skipTest('symlink creation unavailable')
        self.assertEqual(self.adapter.selected(alias).state, 'review')
        folder = self.root / 'linked-folder'
        folder.symlink_to(self.stage, target_is_directory=True)
        self.assertEqual(self.adapter.destination(folder / 'out.zip', 'imported').state, 'review')


if __name__ == '__main__':
    unittest.main()
