import json
from pathlib import Path
import sys
import unittest
from decimal import Decimal

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from submission_preview import preview, MAX_MS


class PreviewTests(unittest.TestCase):
    def test_exact_millisecond_conversion(self):
        for ms in (0, 1, 123456, MAX_MS):
            report = preview('aaaaaaaa', 'bbbbbbbb', 'https://example.invalid/video', '2026-01-02', ms)
            payload = json.loads(report['body_json'], parse_float=Decimal)['run']
            self.assertEqual(payload['times']['ingame'] * 1000, ms)
            self.assertFalse(report['metadata_verified'])
            self.assertFalse(report['dispatch_allowed'])
            self.assertEqual(set(payload), {'category', 'platform', 'date', 'times', 'video', 'emulated'})

    def test_explicit_level(self):
        report = preview('aaaaaaaa', 'bbbbbbbb', 'https://example.invalid/v', '2026-01-02', 100, level='cccccccc')
        self.assertEqual(json.loads(report['body_json'])['run']['level'], 'cccccccc')

    def test_invalid_input(self):
        defaults = dict(category='aaaaaaaa', platform='bbbbbbbb', video='https://example.invalid/v', run_date='2026-01-02', igt_ms=100)
        for changes in ({'category': 'bad'}, {'level': 'bad'}, {'platform': None}, {'run_date': '20260102'},
                        {'video': 'http://example.invalid/v'}, {'video': 'https://user:pass@example.invalid/v'},
                        {'video': 'https://example.invalid/\n'}, {'igt_ms': True}, {'igt_ms': -1}, {'igt_ms': MAX_MS + 1}):
            with self.assertRaises(ValueError):
                preview(**dict(defaults, **changes))


if __name__ == '__main__':
    unittest.main()
