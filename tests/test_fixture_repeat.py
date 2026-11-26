import unittest
from fixture_repeat import trial, summarize


def evidence(delta=0):
    lines = ['LivePerformance 600 23000000 86000000 6400 7300 7800 8500 15000',
             'LiveResources 1280 720 1280 720 116000000 42000000 13000000 83000 11059200',
             'LiveHandoff 600 0 225000000 222000000 3000000 1 0 0',
             'LiveDecode count=600 max_abs_delta_100ns=400']
    for i, frame in enumerate((1,120,240,360,480,600)):
        lines.append(f'LiveResourceSample {frame} {frame*37000} 115000000 114000000 {140000000+i*1000+delta*(i==5)} 1')
    return '\n'.join(lines)


class RepeatTests(unittest.TestCase):
    def test_signed_growth_and_ranges(self):
        a,b=trial(evidence()),trial(evidence(-10000))
        self.assertEqual(a.private_delta,5000)
        self.assertEqual(b.warm_private_delta,-6000)
        self.assertEqual(summarize([a,b])['warm_private_delta'],(-6000,4000))

    def test_missing_duplicate_and_out_of_bounds(self):
        base=evidence()
        for bad in (base.replace('LivePerformance','Other'),base+'\n'+base,
                    base.replace('1280 720 1280 720','1280 720 640 360'),
                    base.replace('11059200','11059201'),base.replace('1 0 0','1 1 0'),
                    base.replace('max_abs_delta_100ns=400','max_abs_delta_100ns=20001'),
                    base.replace('225000000 222000000 3000000','225000000 222000000 1'),
                    base.replace('LiveResourceSample 600','LiveResourceSample 601'),
                    base.replace('23000000','20000000')):
            with self.assertRaises(ValueError): trial(bad)

    def test_two_trials_required(self):
        t=trial(evidence())
        for cases in ([],[t],[t,t,t],[t,None]):
            with self.assertRaises(ValueError): summarize(cases)


if __name__=='__main__': unittest.main()
