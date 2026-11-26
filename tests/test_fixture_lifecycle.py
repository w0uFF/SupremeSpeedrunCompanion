import unittest
from fixture_lifecycle import split,timeline,decoded,COUNTS


def log():
    lines=['HDLifecycleSample 0 123 1000 900 800 10']
    for i,n in enumerate(COUNTS,1):
        lines.extend([f'HDLifecycleBegin {i} {n}',f'clip {i}',f'HDLifecycleSample {i} 123 {1000+i} 900 800 10',f'HDLifecycleEnd {i}'])
    return '\n'.join(lines)


def capture(count):
    pts=[i*400000+(3000000 if i>=100 else 0) for i in range(count)]
    dur=[min(b-a,1000000) for a,b in zip(pts,pts[1:])]+[400000]
    end=pts[-1]+dur[-1]
    return '\n'.join([*(f'LiveFrame {i} {p+1000000000}' for i,p in enumerate(pts)),
                      f'LiveCapture {count} {count+1} 1 {count-1}',
                      f'LiveHandoff {count} 0 {end} {sum(dur)} {end-sum(dur)} 1 0 0'])


class LifecycleTests(unittest.TestCase):
    def test_valid_three_scopes(self):
        blocks,samples=split(log())
        self.assertEqual(blocks,('clip 1','clip 2','clip 3'))
        self.assertEqual([s.phase for s in samples],[0,1,2,3])

    def test_incomplete_or_identity_mismatch(self):
        base=log()
        for bad in (base.replace('Sample 2 123','Sample 2 456'),base.replace('Begin 2 1200','Begin 2 600'),
                    base.replace('HDLifecycleEnd 3',''),base.replace('Sample 1 123 1001','Sample 1 123 999'),
                    base.replace('clip 1','HDLifecycleIncomplete 1 77')):
            with self.assertRaises(ValueError): split(bad)

    def test_capture_and_decode(self):
        for n in (600,1200):
            data=timeline(capture(n),n)
            rows='\n'.join(f'TimedPTS {i} {p} {p+400}' for i,(p,d) in enumerate(data))
            evidence=rows+f'\nDecoded {n} H264 frames, 1280x720, monotonic timestamps and luma pattern verified'
            self.assertEqual(decoded(evidence,data),400)
            with self.assertRaises(ValueError): decoded(evidence.replace('TimedPTS 0 0 400','TimedPTS 0 0 20001'),data)
            with self.assertRaises(ValueError): decoded(rows,data)

    def test_bad_handoff(self):
        base=capture(600)
        for bad in (base.replace('LiveFrame 0 ','LiveFrame 2 '),base.replace('1 0 0','1 1 0'),
                    base.replace('LiveCapture 600 601 1','LiveCapture 600 601 0')):
            with self.assertRaises(ValueError): timeline(bad,600)


if __name__=='__main__': unittest.main()
