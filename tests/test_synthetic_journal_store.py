import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from synthetic_admission import Context, Journal
from synthetic_journal_store import Store, ReviewRequired, encode, decode, MAX_BYTES


class StoreTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.ctx = Context('a'*32, 1, 'b'*64)
        self.j0 = Journal(self.ctx)
        self.j1 = self.j0.finish(self.ctx, 'abandoned', False)
        self.j2 = self.j1.verify(self.ctx, 'abandoned', 'synthetic', 'c'*64, False)
        self.j3 = self.j2.published('c'*64)
        self.j4 = self.j3.indexed('c'*64, 'identical')
        self.store = Store(self.root, self.ctx.run_id)

    def bytes(self):
        return {p.name:p.read_bytes() for p in self.root.iterdir()}

    def test_full_chain_and_reopen(self):
        for j in (self.j0, self.j1, self.j2, self.j3, self.j4):
            before = self.bytes()
            self.assertEqual(self.store.append(j), 'appended')
            self.assertEqual(Store(self.root, self.ctx.run_id).inspect().latest, j)
            self.assertEqual(self.store.append(j), 'identical')
            self.assertEqual(before, {k:self.bytes()[k] for k in before})
        self.assertFalse(self.store.inspect().latest.capture_complete)
        self.assertEqual(len(self.bytes()), 5)

    def test_strict_codec(self):
        for j in (self.j0, self.j1, self.j2, self.j3, self.j4):
            self.assertEqual(decode(encode(j)), j)
        raw = encode(self.j2)
        data = json.loads(raw)
        for update in (dict(origin='local'), dict(origin='imported'), dict(schema=True), dict(schema=2),
                       dict(generation=True), dict(capture_complete=None), dict(extra=1)):
            with self.assertRaises(ValueError):
                decode((json.dumps(dict(data, **update), sort_keys=True, separators=(',', ':')) + '\n').encode('ascii'))
        for bad in (raw[:-1], b'{"schema":1,'+raw[1:], b'x'*(MAX_BYTES+1)):
            with self.assertRaises(ValueError):
                decode(bad)

    def test_reject_conflicts(self):
        self.store.append(self.j0)
        before = self.bytes()
        for j in (self.j2, Journal(Context('a'*32, 2, 'b'*64)).finish(Context('a'*32,2,'b'*64), 'finished', True)):
            with self.assertRaises(ReviewRequired):
                self.store.append(j)
        self.assertEqual(self.bytes(), before)
        self.store.append(self.j1)
        final, partial = self.store._paths('terminal')
        partial.write_bytes(final.read_bytes())
        before = self.bytes()
        with self.assertRaises(ReviewRequired):
            self.store.inspect()
        self.assertEqual(self.bytes(), before)

    def test_wrong_filename_and_no_replace(self):
        self.store.append(self.j0)
        final, partial = self.store._paths('terminal')
        other = Context('f'*32, 1, 'b'*64)
        wrong = Journal(other).finish(other, 'abandoned', False)
        final.write_bytes(encode(wrong))
        before = self.bytes()
        with self.assertRaises(ReviewRequired):
            self.store.inspect()
        self.assertEqual(self.bytes(), before)
        final.unlink()  # fixture reset, not Store behavior
        def competitor(point):
            if point == 'after_flush':
                final.write_bytes(b'competing file')
        with self.assertRaises(FileExistsError):
            self.store.append(self.j1, competitor)
        self.assertEqual(final.read_bytes(), b'competing file')
        self.assertEqual(partial.read_bytes(), encode(self.j1))
        with self.assertRaises(ReviewRequired):
            self.store.inspect()

    def test_missing_predecessor(self):
        self.store.append(self.j0)
        self.store.append(self.j1)
        self.store._paths('recording')[0].unlink()  # deliberate corruption in disposable fixture only
        with self.assertRaises(ReviewRequired):
            self.store.inspect()

    def test_faults_in_subprocess(self):
        self.store.append(self.j0)
        code = '''import os,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from synthetic_journal_store import Store
s=Store(Path(sys.argv[2]),'a'*32)
j=s.inspect().latest
s.append(j.finish(j.context,'abandoned',False),lambda point: os._exit(91) if point==sys.argv[3] else None)
'''
        for fault in ('partial_write','after_flush','after_publish'):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as d:
                root=Path(d); store=Store(root,self.ctx.run_id);store.append(self.j0)
                result=subprocess.run([sys.executable,'-c',code,str(ROOT/'tools'),d,fault],timeout=10)
                self.assertEqual(result.returncode,91)
                final, partial=store._paths('terminal')
                if fault=='partial_write':
                    raw=partial.read_bytes()
                    with self.assertRaises(ReviewRequired): store.inspect()
                    with self.assertRaises(ReviewRequired): store.append(self.j1)
                    self.assertEqual(partial.read_bytes(),raw)
                    self.assertFalse(final.exists())
                else:
                    status=store.inspect()
                    self.assertEqual(status.pending,self.j1)
                    self.assertEqual(status.alias,fault=='after_publish')
                    self.assertEqual(status.latest,self.j1 if status.alias else self.j0)
                    self.assertEqual(store.append(self.j1),'identical' if status.alias else 'appended')
                    self.assertEqual(store.inspect().latest,self.j1)
                    self.assertFalse(partial.exists())


if __name__ == '__main__':
    unittest.main()
