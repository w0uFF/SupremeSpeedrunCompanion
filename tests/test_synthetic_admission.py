import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from synthetic_admission import Context, Journal, recovery


class AdmissionTest(unittest.TestCase):
    def setUp(self):
        self.ctx = Context('a'*32, 1, 'b'*64)
        self.digest = 'c'*64
        self.recording = Journal(self.ctx)
        self.terminal = self.recording.finish(self.ctx, 'finished')
        self.verified = self.terminal.verify(self.ctx, 'finished', 'synthetic', self.digest)

    def test_terminal_and_identity(self):
        for context in (None, Context('a'*32, 2, 'b'*64), Context('a'*32, 1, 'd'*64)):
            with self.assertRaises(ValueError):
                self.recording.finish(context, 'finished')
            with self.assertRaises(ValueError):
                self.terminal.verify(context, 'finished', 'synthetic', self.digest)
        with self.assertRaises(ValueError):
            self.terminal.finish(self.ctx, 'finished')
        with self.assertRaises(ValueError):
            self.recording.finish(self.ctx, 'unknown')

    def test_origin_and_digest(self):
        for origin in ('local', 'imported', ''):
            with self.assertRaises(ValueError):
                self.terminal.verify(self.ctx, 'finished', origin, self.digest)
        with self.assertRaises(ValueError):
            self.terminal.verify(self.ctx, 'failed', 'synthetic', self.digest)
        with self.assertRaises(ValueError):
            self.terminal.verify(self.ctx, 'finished', 'synthetic', 'invalid')
        with self.assertRaises(ValueError):
            self.verified.published('d'*64)

    def test_recovery(self):
        d = self.digest
        j = self.verified
        self.assertEqual(recovery(j, d, None, 'missing'), 'retry_synthetic_publication')
        self.assertEqual(recovery(j, None, d, 'missing'), 'retry_synthetic_index')
        self.assertEqual(recovery(j, None, d, 'identical'), 'none')
        for pending, final, index in ((d,d,'missing'), (None,'d'*64,'missing'),
                                      (None,d,'conflict'), (None,d,'unavailable'),
                                      (None,None,'identical'), (d,None,'identical')):
            self.assertEqual(recovery(j,pending,final,index), 'review')
        self.assertEqual(recovery(None,None,d,'missing'), 'review')
        self.assertEqual(recovery(self.recording,None,d,'missing'), 'review')

    def test_index_and_invalid_phase(self):
        d = self.digest
        p = self.verified.published(d)
        self.assertEqual(p.published(d), p)
        for result in ('conflict', 'unavailable'):
            with self.assertRaises(ValueError):
                p.indexed(d, result)
        i = p.indexed(d, 'inserted')
        self.assertEqual(i.indexed(d, 'identical'), i)
        self.assertEqual(recovery(i,None,d,'missing'), 'review')
        self.assertEqual(recovery(i,None,d,'identical'), 'none')
        with self.assertRaises(ValueError):
            Journal(self.ctx,'indexed','finished',None)


if __name__ == '__main__':
    unittest.main()
