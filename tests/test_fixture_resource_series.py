import unittest
from fixture_resource_series import expected_frames, parse


class ResourceTests(unittest.TestCase):
    def lines(self):
        return [f'LiveResourceSample {frame} {frame*40000} {1000+i*100} 900 {300+i*10} {min(i,2)}'
                for i,frame in enumerate(expected_frames(600))]

    def test_valid_and_growth(self):
        samples=parse('\n'.join(self.lines()),600,1600,2)
        self.assertEqual(len(samples),6)
        self.assertEqual(samples[-1].private_bytes-samples[0].private_bytes,50)
        self.assertEqual(samples[-1].frame,600)

    def test_missing_or_extra(self):
        lines=self.lines()
        for invalid in (lines[1:],lines+[lines[-1]],lines[:2]+lines[3:]):
            with self.assertRaises(ValueError): parse('\n'.join(invalid),600,1600,2)

    def test_bounds_and_order(self):
        for replacement in ('LiveResourceSample 120 1 1100 900 310 1',
                            'LiveResourceSample 120 4800000 999 900 310 1',
                            'LiveResourceSample 120 4800000 1100 1200 310 1',
                            'LiveResourceSample 120 4800000 1100 900 310 9',
                            'LiveResourceSample 120 4800000 1100 900 -1 1'):
            lines=self.lines();lines[1]=replacement
            with self.assertRaises(ValueError): parse('\n'.join(lines),600,1600,2)
        with self.assertRaises(ValueError): parse('\n'.join(self.lines()),600,1200,2)

    def test_count_limits(self):
        for count in (0,119,601,1320,True):
            with self.assertRaises(ValueError): expected_frames(count)


if __name__=='__main__': unittest.main()
