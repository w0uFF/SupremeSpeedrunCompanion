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


class HostedTextTests(unittest.TestCase):
    def setUp(self):
        self.url = 'https://example.invalid/runs/run.ssc.zip'
        self.options = dict(automatic_video=True, automatic_submission=True,
                            notes_limit=4096, description_limit=4096)

    def test_exact_default_and_modes(self):
        from hosted_text import compose, PersonalText
        a = compose(self.url, **self.options)
        self.assertEqual(a['description'], 'Uploaded automatically by Supreme Speedrun Companion (SSC)\n\nSupplementary evidence package:\n' + self.url)
        self.assertEqual(a['notes'], 'Submitted automatically by Supreme Speedrun Companion (SSC)\n\nSupplementary evidence package:\n' + self.url)
        self.assertFalse(a['dispatch_allowed'])
        self.assertFalse(a['metadata_verified'])
        p = PersonalText('New PB!', 'Race notes')
        easy = compose(self.url, saved=p, **self.options)
        advanced = compose(self.url, mode='advanced', advanced=p, **self.options)
        self.assertEqual({k:v for k,v in easy.items() if k!='mode'},
                         {k:v for k,v in advanced.items() if k!='mode'})
        self.assertEqual(advanced['notes'], 'New PB!\n\n' + a['notes'])
        self.assertEqual(advanced['description'], 'Race notes\n\n' + a['description'])
        self.assertEqual(compose(self.url, mode='advanced', saved=p, advanced=PersonalText(), **self.options)['notes'], a['notes'])

    def test_manual_truthful_attribution(self):
        from hosted_text import compose
        a = compose(self.url, **dict(self.options, automatic_video=False))
        self.assertTrue(a['description'].startswith('Recorded using '))
        self.assertTrue(a['notes'].startswith('Submitted automatically '))
        b = compose(self.url, **dict(self.options, automatic_submission=False))
        self.assertTrue(b['notes'].startswith('Recorded using '))
        self.assertTrue(b['description'].startswith('Uploaded automatically '))

    def test_utf8_limits_and_controls(self):
        from hosted_text import compose, PersonalText
        text = PersonalText('雪\r\nPB', 'a')
        a = compose(self.url, saved=text, **self.options)
        limit = len(a['notes'].encode('utf-8'))
        self.assertEqual(compose(self.url, saved=text, **dict(self.options, notes_limit=limit)), a)
        with self.assertRaises(ValueError):
            compose(self.url, saved=text, **dict(self.options, notes_limit=limit-1))
        for bad in ('a\x00b', '\ud800', 'x'*65537):
            with self.assertRaises(ValueError):
                compose(self.url, saved=PersonalText(bad, ''), **self.options)
        self.assertTrue(a['notes'].startswith('雪\nPB\n\n'))

    def test_invalid_links_modes_and_flags(self):
        from hosted_text import compose, PersonalText
        for bad in ('http://example.invalid/run', 'https://u:p@example.invalid/run',
                    'https://example.invalid/run?token=x', 'https://example.invalid/run#x',
                    'https://example.invalid/\n', 'https://example.invalid', 'https://example.invalid:80/run',
                    'https://example.invalid:bad/run'):
            with self.assertRaises(ValueError):
                compose(bad, **self.options)
        for changes in ({'mode':'other'}, {'advanced':PersonalText()}, {'saved':None},
                        {'automatic_video':1}, {'automatic_submission':None}, {'notes_limit':True}):
            with self.assertRaises(ValueError):
                compose(self.url, **dict(self.options, **changes))


if __name__ == '__main__':
    unittest.main()
