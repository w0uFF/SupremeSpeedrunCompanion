import json
import math
import pathlib
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools'))
import native_xrefs
import replay_samples
from replay_header import BLOCK, MAX_BYTES


def fixture(positions):
    # Synthetic layout only; no proprietary replay bytes.
    data = bytearray(38 + len(positions) * BLOCK)
    data[:4] = b'\x05\0\0\0'
    data[24] = ord('X')
    last = len(positions) - 1
    for offset, value in ((4, last), (26, min(1, last)), (30, min(2, last)), (34, last)):
        struct.pack_into('<H', data, offset, value)
    for i, xyz in enumerate(positions):
        struct.pack_into('<3f', data, 38 + i * BLOCK + 44, *xyz)
    return data


class ReplaySamples(unittest.TestCase):
    def test_bounds_and_terminal(self):
        data = fixture([(1, -2, 3), (4, 5, 6), (-3, 0, 9), (999, 999, 999)])
        report = replay_samples.summarize(data)
        self.assertEqual(report['bounds_xyz'], {'min': [-3, -2, 3], 'max': [4, 5, 9]})
        self.assertEqual(report['finite_position_records'], 3)
        self.assertEqual(report['markers']['start_header']['xyz'], [4, 5, 6])
        self.assertEqual(report['markers']['checkpoint1_header']['xyz'], [-3, 0, 9])
        self.assertIsNone(report['markers']['finish_header']['xyz'])
        self.assertEqual(report['checkpoint1_elapsed_ms_candidate'], 10)
        self.assertFalse(report['geometry_crossings_verified'])
        self.assertFalse(report['header']['official_time_verified'])
        self.assertTrue(report['terminal_record_excluded'])

    def test_nonfinite(self):
        report = replay_samples.summarize(fixture([(0, 0, 0), (math.nan, 0, 0), (0, math.inf, 0), (0, 0, 0)]))
        self.assertEqual(report['finite_position_records'], 1)
        self.assertEqual(report['nonfinite_position_records'], 2)
        self.assertIsNone(report['markers']['start_header']['xyz'])
        self.assertIsNone(report['markers']['checkpoint1_header']['xyz'])
        json.dumps(report, allow_nan=False)

    def test_no_samples(self):
        for positions in ([(0, 0, 0)], [(math.nan, 0, 0), (0, 0, 0)]):
            report = replay_samples.summarize(fixture(positions))
            self.assertIsNone(report['bounds_xyz'])
            self.assertEqual(report['finite_position_records'], 0)

    def test_invalid_and_bounded_file(self):
        for data in (b'', fixture([(0, 0, 0)])[:-1], bytes(MAX_BYTES + 1)):
            with self.assertRaises(ValueError):
                replay_samples.summarize(data)
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'sample.dat'
            source = fixture([(0, 0, 0), (1, 2, 3)])
            path.write_bytes(source)
            replay_samples.summarize_file(path)
            self.assertEqual(path.read_bytes(), source)
            path.write_bytes(bytes(MAX_BYTES + 2))
            with self.assertRaises(ValueError):
                replay_samples.summarize_file(path)


class NativeReferences(unittest.TestCase):
    def test_disassembly_parser(self):
        text = '  401000:\t68 00 20 40 00\tpush   0x402000\n  401005:\tff 15 00 30 40 00\tcall DWORD PTR ds:0x403000\n  40100b:\t00\nDisassembly of section .text:\n'
        self.assertEqual([(va, refs) for va, _, refs in native_xrefs.parse_lines(text)],
                         [(0x401000, {0x402000}), (0x401005, {0x403000})])

    def test_compare(self):
        def report(rva, instruction):
            return {'scope': 'static_linear_sweep_candidates', 'references': [
                {'kind': 'string', 'instruction_rva': '0x10', 'target_rva': '0x20', 'name': 'ID_MAIN_MENU', 'instruction': instruction},
                {'kind': 'iat', 'instruction_rva': rva, 'target_rva': '0x30', 'name': 'example', 'instruction': instruction}]}
        comparison = native_xrefs.compare(report('0x15', 'call'), report('0x25', 'jmp'))
        self.assertEqual(comparison['string']['common'], 1)
        self.assertEqual(comparison['iat']['common'], 0)
        self.assertEqual(comparison['iat']['removed'], [('0x15', '0x30', 'example')])
        self.assertEqual(comparison['iat']['added'], [('0x25', '0x30', 'example')])

    def test_invalid_before_disassembler(self):
        with tempfile.TemporaryDirectory() as tmp, patch('native_xrefs.subprocess.run') as run:
            path = pathlib.Path(tmp) / 'invalid.exe'
            for data in (b'', bytes(64), bytes(native_xrefs.MAX_BYTES + 1)):
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    native_xrefs.scan(path)
            run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
