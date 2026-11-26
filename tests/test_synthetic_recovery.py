import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from synthetic_admission import Context, Journal
from synthetic_journal_store import Store, ReviewRequired
from synthetic_recovery import decide


class RecoveryTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.ctx = Context('a'*32, 1, 'b'*64)
        self.d = 'c'*64
        self.j = Journal(self.ctx)
        self.t = self.j.finish(self.ctx, 'abandoned', False)
        self.v = self.t.verify(self.ctx, 'abandoned', 'synthetic', self.d, False)
        self.p = self.v.published(self.d)
        self.i = self.p.indexed(self.d, 'identical')
        self.store = Store(self.root, self.ctx.run_id)
        for j in (self.j, self.t, self.v):
            self.store.append(j)

    def facts(self, archive='finalized', index='missing', **extra):
        return dict(dict(run_id=self.ctx.run_id, origin='synthetic', archive=archive,
                         index=index, manifest_sha256=self.d if archive!='absent' else None), **extra)

    def decision(self, archive='finalized', index='missing', **extra):
        return decide(self.store.inspect(), self.facts(archive, index, **extra))

    def test_ordered_reconciliation(self):
        self.assertEqual(self.decision('absent').action, 'await_archive')
        self.assertEqual(self.decision('pending').action, 'retry_archive_publication')
        self.assertEqual(self.decision('finalized_with_pending_alias').action, 'retry_archive_alias_cleanup')
        self.assertEqual(self.decision(index='identical').snapshot, self.p)
        self.store.append(self.p)
        self.assertEqual(self.decision().action, 'retry_index')
        self.assertEqual(self.decision(index='identical').snapshot, self.i)
        self.store.append(self.i)
        self.assertEqual(self.decision(index='identical').action, 'none')
        self.assertEqual(self.decision().action, 'review')

    def test_conflicts_preserved(self):
        before = {p.name:p.read_bytes() for p in self.root.iterdir()}
        for facts in (self.facts(index='conflict'), self.facts(index='unavailable'),
                      self.facts('pending','identical'), self.facts('absent','present'),
                      self.facts('separate_pending_conflict'), self.facts('review_required'),
                      self.facts(manifest_sha256='d'*64), self.facts(origin='local'),
                      self.facts(run_id='e'*32)):
            self.assertEqual(decide(self.store.inspect(), facts).action, 'review')
        self.assertEqual(before, {p.name:p.read_bytes() for p in self.root.iterdir()})

    def test_actual_process_interruption(self):
        code = '''import os,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from synthetic_journal_store import Store
s=Store(Path(sys.argv[2]),'a'*32)
j=s.inspect().latest
s.append(j.published(j.digest),lambda point: os._exit(91) if point==sys.argv[3] else None)
'''
        for point in ('partial_write','after_flush','after_publish'):
            with self.subTest(point=point), tempfile.TemporaryDirectory() as d:
                s=Store(Path(d),self.ctx.run_id)
                for j in (self.j,self.t,self.v): s.append(j)
                result=subprocess.run([sys.executable,'-c',code,str(ROOT/'tools'),d,point],timeout=10)
                self.assertEqual(result.returncode,91)
                if point=='partial_write':
                    with self.assertRaises(ReviewRequired): s.inspect()
                else:
                    decision=decide(s.inspect(),self.facts(index='identical'))
                    self.assertEqual(decision.action,'retry_journal')
                    self.assertEqual(decision.snapshot,self.p)
                    self.assertEqual(decide(s.inspect(),self.facts(manifest_sha256='d'*64)).action,'review')
                    s.append(decision.snapshot)
                    self.assertEqual(decide(s.inspect(),self.facts(index='identical')).snapshot,self.i)

    def test_index_pending_requires_identical(self):
        self.store.append(self.p)
        def fault(point):
            if point=='after_flush': raise RuntimeError('fixture')
        with self.assertRaises(RuntimeError): self.store.append(self.i, fault)
        self.assertEqual(self.decision().action,'review')
        self.assertEqual(self.decision(index='identical').snapshot,self.i)


if __name__ == '__main__':
    unittest.main()
