import unittest
from fixture_isolation import MODES, evidence


def log(mode):
    state,accepted,written,discarded,exists,size=(('finalized',60,60,0,1,1200) if mode=='worker-write'
        else ('cancelled',0,0,0,1,80) if mode=='worker-idle' else ('none',0,0,0,0,0))
    rows=[f'IsolationBegin {mode}']
    for cycle in range(1,4):
        rows.append(f'IsolationSample {cycle} 0 123 1000 800 700 10 0 1')
        rows.append(f'IsolationOutcome {cycle} {state} {accepted} {written} {discarded} {exists} {size}')
        rows.extend(f'IsolationSample {cycle} {stage} 123 1000 800 700 {10+stage} 0 1' for stage in range(1,4))
    rows.append(f'IsolationEnd {mode} 3')
    return '\n'.join(rows)


class IsolationTests(unittest.TestCase):
    def test_modes(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                rows,outcomes=evidence(log(mode),mode)
                self.assertEqual((len(rows),len(outcomes)),(12,3))

    def test_refuses_corruption(self):
        base=log('worker-write')
        for bad in (base.replace('IsolationSample 2 1 123','IsolationSample 2 1 124'),
                    base.replace('IsolationSample 2 1','IsolationSample 2 2'),
                    base.replace('123 1000 800 700','123 799 800 700'),
                    base.replace('finalized 60 60','finalized 60 59'),
                    base.replace('finalized 60 60 0 1 1200','finalized 60 60 0 0 1200'),
                    base.replace('IsolationEnd worker-write 3','IsolationEnd worker-write 2'),
                    base+'\nIsolationUnexpected 1'):
            with self.assertRaises(ValueError): evidence(bad,'worker-write')

    def test_incomplete_not_finalized(self):
        with self.assertRaises(ValueError): evidence(log('worker-idle').replace('cancelled','finalized'),'worker-idle')


if __name__=='__main__': unittest.main()
