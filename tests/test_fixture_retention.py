import unittest
from fixture_lifecycle import WRITE_COUNTS
from fixture_retention import evidence, failure


def log():
    lines=['HDFaultGuiSample 0 123 0 1','HDLifecycleSample 0 123 1000 900 800 10']
    for i,count in enumerate(WRITE_COUNTS,1):
        lines.append(f'HDLifecycleBegin {i} {count}')
        lines.extend(f'HDStageResources {n} 123 {1000+i} 900 800 {20+n}' for n in range(10))
        lines.extend([f'HDFaultGuiSample {i} 123 7 4',f'HDLifecycleSample {i} 123 {1000+i} 900 800 29',f'HDLifecycleEnd {i}'])
    return '\n'.join(lines)


def failed():
    return '\n'.join(['HDWindowGeometry 1280 720 1280 720 1280 720 1280 720',
                     *(f'LiveFrame {i} {1000000000+i*400000}' for i in range(120)),
                     'HDFaultOutcome sink-write-failure failed 120 119 118 1 1 1 0 0',
                     'HDFaultArtifact 1 1234'])


class RetentionTests(unittest.TestCase):
    def test_stages(self):
        blocks,r,gui,stages=evidence(log())
        self.assertEqual(len(blocks),3)
        self.assertEqual(len(r),4)
        self.assertEqual(len(gui),4)
        self.assertTrue(all(len(s)==10 for s in stages))

    def test_invalid_stage(self):
        base=log()
        for bad in (base.replace('HDStageResources 8 123','HDStageResources 8 124'),
                    base.replace('HDStageResources 9 123','HDStageResources 8 123'),
                    base.replace('HDStageResources 5 123 1001','HDStageResources 5 123 999'),
                    base.replace('HDFaultGuiSample 2 123','HDFaultGuiSample 2 124')):
            with self.assertRaises(ValueError): evidence(bad)

    def test_exact_write_failure(self):
        f=failure(failed(),1234)
        self.assertEqual((f.written,f.discarded,f.pending),(118,1,1))
        for bad in (failed().replace('118 1 1','119 0 1'),failed().replace('failed 120','cancelled 120'),
                    failed()+'\nLiveCapture 120 120 0 118'):
            with self.assertRaises(ValueError): failure(bad,1234)


    def test_exact_finalize_failure(self):
        data=failed().replace('sink-write-failure failed 120 119 118 1 1','sink-finalize-failure failed 120 120 120 0 0')
        f=failure(data,1234,True)
        self.assertEqual((f.accepted,f.written,f.discarded,f.pending),(120,120,0,0))
        for bad in (data.replace('120 120 120 0 0','120 119 119 0 1'),
                    data.replace('failed 120','finalized 120'),data+'\nLiveHandoff 120 0 1 1 0 1 0 0'):
            with self.assertRaises(ValueError): failure(bad,1234,True)
        with self.assertRaises(ValueError): failure(data,1234)


if __name__=='__main__': unittest.main()
