from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from synthetic_admission import Context, Journal
from synthetic_journal_store import Store, ReviewRequired
from synthetic_recovery import Decision
from synthetic_retry import plan, execute
from synthetic_index_port import BoundSyntheticIndex


class RetryPlanTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.private = self.root/'journal'
        self.archive = self.root/'archive'
        self.private.mkdir()
        self.archive.mkdir()
        self.db=self.root/'runs.sqlite'
        self.db.write_bytes(b'mock db')
        self.bridge=self.root/'ssc_index.exe'
        self.bridge.write_bytes(b'mock bridge')
        self.index=BoundSyntheticIndex.bind(self.db,self.bridge)
        c = Context('a'*32, 1, 'b'*64)
        j = Journal(c)
        t = j.finish(c, 'abandoned', False)
        self.v = t.verify(c, 'abandoned', 'synthetic', 'c'*64, False)
        self.store = Store(self.private, c.run_id)
        for snapshot in (j,t,self.v):
            self.store.append(snapshot)

    def test_plan_binds_context_and_directories(self):
        with patch('synthetic_retry.observe', return_value=Decision('retry_archive_publication')):
            p = plan(self.store,self.archive,self.index)
            self.assertEqual(p.journal,self.v)
            self.assertEqual(p.index,self.index)
            self.assertEqual(p.journal_directory,self.private.resolve())
            self.assertEqual(p.archive_directory,self.archive.resolve())
            other=self.root/'other'
            other.mkdir()
            with self.assertRaises(ReviewRequired):
                execute(self.store,other,self.index,p)
            self.store.append(self.v.published(self.v.digest))
            with self.assertRaises(ReviewRequired):
                execute(self.store,self.archive,self.index,p)

    def test_refuse_nonexternal_and_stale_action(self):
        with patch('synthetic_retry.observe', return_value=Decision('retry_archive_publication')):
            p=plan(self.store,self.archive,self.index)
        for action in ('review','none','append_published','retry_index'):
            with patch('synthetic_retry.observe', return_value=Decision(action)):
                if action!='retry_index':
                    self.assertIsNone(plan(self.store,self.archive,self.index))
                with self.assertRaises(ReviewRequired):
                    execute(self.store,self.archive,self.index,p)

    def test_other_database_and_replacement_rejected(self):
        with patch('synthetic_retry.observe', return_value=Decision('retry_index')):
            p=plan(self.store,self.archive,self.index)
            other=self.root/'other'
            other.mkdir()
            db=other/'runs.sqlite'
            db.write_bytes(self.db.read_bytes())
            wrong=BoundSyntheticIndex.bind(db,self.bridge)
            self.assertNotEqual(plan(self.store,self.archive,wrong),p)
            with self.assertRaises(ReviewRequired):
                execute(self.store,self.archive,wrong,p)
            self.db.rename(self.root/'original-db')
            self.db.write_bytes(b'mock db')
            self.assertIsNone(plan(self.store,self.archive,self.index))
            with self.assertRaises(ReviewRequired):
                execute(self.store,self.archive,self.index,p)
            self.assertEqual(self.db.read_bytes(),b'mock db')
            self.assertEqual(db.read_bytes(),b'mock db')

    def test_pending_journal_requires_separate_reconciliation(self):
        def interrupt(point):
            if point=='after_flush':
                raise RuntimeError('fixture')
        with self.assertRaises(RuntimeError):
            self.store.append(self.v.published(self.v.digest),interrupt)
        with patch('synthetic_retry.observe', return_value=Decision('retry_archive_publication')):
            self.assertIsNone(plan(self.store,self.archive,self.index))


if __name__ == '__main__':
    unittest.main()
