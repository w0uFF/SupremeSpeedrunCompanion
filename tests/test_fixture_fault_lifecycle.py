import unittest
from fixture_lifecycle import FAULT_COUNTS
from fixture_fault_lifecycle import evidence, fault


def aborted(reason='user-cancel'):
    return '\n'.join(['HDWindowGeometry 1280 720 1280 720 1280 720 1280 720',
                      *(f'LiveFrame {i} {1000000000+i*400000}' for i in range(120)),
                      f'HDFaultOutcome {reason} cancelled 120 119 118 1 1 2 0 0',
                      'HDFaultArtifact 1 1234'])


def log():
    lines=['HDFaultGuiSample 0 123 2 1','HDLifecycleSample 0 123 1000 900 800 10']
    for i,n in enumerate(FAULT_COUNTS,1):
        lines.extend([f'HDLifecycleBegin {i} {n}',f'clip {i}',
                      f'HDFaultGuiSample {i} 123 2 1',f'HDLifecycleSample {i} 123 {1000+i} 900 800 10',
                      f'HDLifecycleEnd {i}'])
    return '\n'.join(lines)


class FaultLifecycleTests(unittest.TestCase):
    def test_valid_and_absent_artifact(self):
        f=fault(aborted(),'user-cancel',1234)
        self.assertEqual((f.accepted,f.written,f.discarded,f.pending),(119,118,1,1))
        f=fault(aborted('simulated-device-loss').replace('HDFaultArtifact 1 1234','HDFaultArtifact 0 0'),
                'simulated-device-loss',None)
        self.assertIsNone(f.artifact_bytes)

    def test_bad_fault_counters_or_success(self):
        base=aborted()
        for bad in (base.replace('119 118 1 1','119 117 1 1'),
                    base.replace('1 2 0 0','1 9 0 0'),
                    base.replace('1 2 0 0','1 2 1 0'),
                    base.replace('cancelled','finalized'),
                    base.replace('LiveFrame 119','LiveFrame 118'),
                    base+'\nLiveHandoff 120 0 1 1 0 1 0 0',
                    base+'\nHDFaultArtifact 1 1234',
                    base.replace('1280 720 1280 720','640 360 1280 720')):
            with self.assertRaises(ValueError): fault(bad,'user-cancel',1234)
        with self.assertRaises(ValueError): fault(base,'simulated-device-loss',1234)
        with self.assertRaises(ValueError): fault(base,'user-cancel',1235)

    def test_five_scope_identity_and_gui(self):
        blocks,r,gui=evidence(log())
        self.assertEqual(len(blocks),5)
        self.assertEqual(len(r),6)
        self.assertEqual(gui[-1],(5,123,2,1))

    def test_missing_or_wrong_gui_and_unexpected_fault(self):
        base=log()
        for bad in (base.replace('HDFaultGuiSample 3 123 2 1',''),
                    base.replace('HDFaultGuiSample 3 123','HDFaultGuiSample 3 124'),
                    base.replace('Begin 4 120','Begin 4 600'),
                    base.replace('clip 1','HDFaultOutcome user-cancel cancelled 120 119 118 1 1 2 0 0')):
            with self.assertRaises(ValueError): evidence(bad)


if __name__=='__main__': unittest.main()
