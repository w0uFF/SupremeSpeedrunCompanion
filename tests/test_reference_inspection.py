import pathlib
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import replay_header
import reference_assets


def sample():
    data = bytearray(38 + 4*109)
    data[:4] = b'\x05\0\0\0'
    data[24] = ord('X')
    for offset, value in ((4,3),(26,1),(30,2),(34,3)):
        struct.pack_into('<H',data,offset,value)
    return data


class Replay(unittest.TestCase):
    def test_candidate(self):
        report = replay_header.inspect(sample())
        self.assertEqual(report['candidate_displayed_ms'],20)
        self.assertFalse(report['official_time_verified'])
    def test_nonzero_last_block(self):
        data = sample(); data[-1] = 1
        self.assertFalse(replay_header.inspect(data)['zero_last_block'])
    def test_invalid(self):
        for data in (b'', sample()[:-1], bytes(100), sample() + bytes(109), bytes(replay_header.MAX_BYTES+1)):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                replay_header.inspect(data)
    def test_order(self):
        data = sample(); struct.pack_into('<H',data,30,4)
        with self.assertRaises(ValueError): replay_header.inspect(data)
    def test_bounded_name(self):
        data = sample(); data[24:280] = b'X'*256
        with self.assertRaises(ValueError): replay_header.inspect(data)


class Assets(unittest.TestCase):
    def test_partial(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            path = root/'Data/Levels/Alpine/Tracks/Easy'
            (path/'Day').mkdir(parents=True)
            (path/'Resource_Config.txt').touch()
            report = reference_assets.inspect(root)
            self.assertEqual(len(report['tracks']),9)
            self.assertEqual(report['tracks'][0]['weather_directories'],['day'])
            self.assertTrue(report['tracks'][0]['resource_config'])
            self.assertFalse(report['tracks'][1]['resource_config'])
            self.assertEqual(report['native_capabilities'],[])


if __name__ == '__main__': unittest.main()
