import pathlib
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
from replay_compare import compare
from replay_header import BLOCK


def fixture(start, checkpoint, finish):
    recording = finish + 2
    data = bytearray(38 + (recording + 1) * BLOCK)
    data[:4] = b'\x05\0\0\0'
    data[24] = ord('X')
    for offset, value in ((4, recording), (26, start), (30, checkpoint), (34, finish)):
        struct.pack_into('<H', data, offset, value)
    return data


class ComparisonTests(unittest.TestCase):
    def test_splits(self):
        result = compare(fixture(2, 5, 10), fixture(4, 6, 12), same_track_confirmed=True)
        self.assertEqual(result['second_minus_first'], {'start_to_cp1': -1, 'cp1_to_finish': 1, 'start_to_finish': 0})
        self.assertFalse(result['official_time_verified'])
        self.assertFalse(result['identity_verified_by_tool'])

    def test_refusal(self):
        data = fixture(2, 5, 10)
        with self.assertRaises(ValueError):
            compare(data, data)
        with self.assertRaises(ValueError):
            compare(data, b'', same_track_confirmed=True)


if __name__ == '__main__':
    unittest.main()
