from dataclasses import replace
from contextlib import closing
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from offline_library import Library, Preferences, Run


class StorageTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.path = Path(temp.name) / 'library.db'
        self.lib = Library(self.path)
        self.addCleanup(lambda: self.lib.close())
        self.run = Run('1' * 32, 'wk6y97ed', 'a' * 64, 'test-v1', 'local', 'finished', 100, 120000000)

    def test_preferences_reopen(self):
        self.assertIsNone(self.lib.preferences())
        prefs = Preferences(True, 'rider', 'board', 'ask')
        self.lib.save_preferences(prefs)
        self.lib.close()
        self.lib = Library(self.path)
        self.assertEqual(self.lib.preferences(), prefs)
        with self.assertRaises(ValueError):
            self.lib.save_preferences(Preferences(True, 'rider', None, 'ask'))
        self.assertEqual(self.lib.preferences(), prefs)

    def test_immutable_run_and_separate_labels(self):
        self.lib.add_run(self.run)
        self.lib.set_label(self.run.run_id, 'Renamed run', True)
        with self.assertRaises(sqlite3.IntegrityError):
            self.lib.add_run(replace(self.run, igt_ms=1))
        self.assertEqual(self.lib.get_run(self.run.run_id), self.run)
        with self.assertRaises(sqlite3.IntegrityError):
            self.lib.set_label('9' * 32, 'Missing', False)
        self.lib.close()
        self.lib = Library(self.path)
        self.assertEqual(self.lib.history(), [self.run])
        self.assertEqual(self.lib.db.execute('SELECT name,favorite FROM labels').fetchone(), ('Renamed run', 1))

    def test_pb_comparability(self):
        self.lib.add_run(self.run)
        for i, changes in enumerate(({'origin': 'imported'}, {'profile_sha256': 'b' * 64},
                                     {'event_semantics': 'v2'}, {'category': 'other'},
                                     {'status': 'abandoned', 'igt_ms': None}, {'igt_ms': None}), 2):
            self.lib.add_run(replace(self.run, run_id=str(i) * 32, rta_ns=1, **changes))
        args = (self.run.category, self.run.profile_sha256, self.run.event_semantics)
        self.assertEqual(self.lib.best_local(*args, 'igt'), self.run)
        self.assertEqual(self.lib.best_local(*args, 'rta').run_id, '7' * 32)
        self.assertIsNone(self.lib.best_local('missing', *args[1:], 'igt'))
        with self.assertRaises(ValueError):
            self.lib.best_local(*args, 'arbitrary')
        self.assertEqual(len(self.lib.history(2)), 2)

    def test_invalid_times_and_ids(self):
        for changes in ({'igt_ms': -1}, {'rta_ns': True}, {'rta_ns': 1 << 63},
                        {'run_id': 'bad'}, {'status': 'restarted'}):
            with self.assertRaises(ValueError):
                self.lib.add_run(replace(self.run, **changes))
        self.assertEqual(self.lib.history(), [])

    def test_unknown_database_version(self):
        self.lib.db.execute('PRAGMA user_version=99')
        self.lib.close()
        with self.assertRaises(ValueError):
            Library(self.path)
        with closing(sqlite3.connect(self.path)) as db:
            self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0], 99)
        self.lib = sqlite3.connect(self.path)

    def test_foreign_database(self):
        other = self.path.with_name('other.db')
        with closing(sqlite3.connect(other)) as db, db:
            db.execute('CREATE TABLE foreign_table (value TEXT)')
        with self.assertRaises(ValueError):
            Library(other)


if __name__ == '__main__':
    unittest.main()
