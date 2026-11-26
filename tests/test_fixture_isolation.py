import unittest
from fixture_isolation import MODES, REFINEMENT_MODES, REFINEMENT_CYCLES, ONCE_COUNTS, evidence, once_evidence


def log(mode, cycles=3):
    state,accepted,written,discarded,exists,size=(('finalized',60,60,0,1,1200) if mode=='worker-write'
        else ('cancelled',0,0,0,1,80) if mode=='worker-idle' else ('none',0,0,0,0,0))
    rows=[f'IsolationBegin {mode}']
    for cycle in range(1,cycles+1):
        rows.append(f'IsolationSample {cycle} 0 123 1000 800 700 10 0 1')
        rows.append(f'IsolationOutcome {cycle} {state} {accepted} {written} {discarded} {exists} {size}')
        rows.extend(f'IsolationSample {cycle} {stage} 123 1000 800 700 {10+stage} 0 1' for stage in range(1,4))
    rows.append(f'IsolationEnd {mode} {cycles}')
    return '\n'.join(rows)


class IsolationTests(unittest.TestCase):
    def test_modes(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                rows,outcomes=evidence(log(mode),mode)
                self.assertEqual((len(rows),len(outcomes)),(12,3))

    def test_refinement_modes(self):
        for mode in REFINEMENT_MODES:
            with self.subTest(mode=mode):
                rows,outcomes=evidence(log(mode,REFINEMENT_CYCLES),mode,REFINEMENT_CYCLES)
                self.assertEqual((len(rows),len(outcomes)),(24,6))
        for mode,cycles in (('worker-write',6),('item-only',3),('pool',4)):
            with self.assertRaises(ValueError): evidence(log(mode,cycles),mode,cycles)
        with self.assertRaises(ValueError): evidence(log('pool',3),'pool',6)
        with self.assertRaises(ValueError): evidence(log('pool',6).replace('IsolationSample 6 3 123','IsolationSample 6 3 456'),'pool',6)

    def test_once_counts(self):
        for mode,counts in ONCE_COUNTS.items():
            with self.subTest(mode=mode):
                marker='WGCOnceCounts '+mode+' '+' '.join(map(str,counts))
                base=log(mode,6)+'\n'+marker
                rows,outcomes=once_evidence(base,mode)
                self.assertEqual((len(rows),len(outcomes)),(24,6))
                for bad in (log(mode,6),base+'\n'+marker,
                            base.replace(marker,marker.replace(' 1 ',' 2 ',1)),
                            marker+'\n'+log(mode,6)):
                    with self.assertRaises(ValueError): once_evidence(bad,mode)
        with self.assertRaises(ValueError): once_evidence(log('pool',6),'pool')

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
