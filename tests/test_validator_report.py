"""Synthetic inert ZIP and injected verifier regressions; no game execution."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import evidence_archive as ea
import run_manifest
import ssc_profiles
from validator.report import inspect
from validate_package import validate, human, exit_code


class ValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.stage = self.root / 'stage'
        self.stage.mkdir()
        self.metadata = dict(run_id='a' * 32, session_id='b' * 32,
                             created_utc='2026-01-02T03:04:05Z', mod_build='fixture',
                             origin='local', recording_mode='manual', attempt_status='finished',
                             capture_status='finalized', igt_final_ms=0, rta_elapsed_ns=123,
                             clock_source='fixture', event_semantics_version='fixture',
                             track_id='Alpine', difficulty='Easy')

    def archive(self, members=None, metadata=None, schema=2, name='evidence.zip'):
        members = {'video.mp4': ('video', b'inert fixture bytes, not a video')} if members is None else members
        for filename, (_, data) in members.items():
            (self.stage / filename).write_bytes(data)
        path = self.root / name
        ea.create(self.stage, {n: r for n, (r, _) in members.items()}, path,
                  metadata=None if schema == 1 else (self.metadata if metadata is None else metadata))
        return path

    def read_only(self, path):
        before = path.read_bytes()
        paths = set(self.root.rglob('*'))
        report = validate(path)
        self.assertEqual(before, path.read_bytes())
        self.assertEqual(paths, set(self.root.rglob('*')))
        self.assertIs(report['eligibility_verified'], False)
        self.assertIs(report['complete_submission'], False)
        self.assertEqual(report['checks']['local_provenance'], 'not_established')
        self.assertEqual(report['checks']['gameplay_integrity'], 'not_assessed')
        return report

    def test_schema2_and_cli(self):
        path = self.archive(name='evidence-\u96ea.zip')
        report = self.read_only(path)
        verified = ea.verify(path, include_manifest=True)
        self.assertEqual(report['verification'], 'consistent')
        self.assertEqual(report['assessment'], 'insufficient_evidence')
        self.assertEqual(report['archive']['manifest_sha256'], verified['manifest_sha256'])
        self.assertEqual(report['archive']['members'][0]['sha256'], hashlib.sha256((self.stage / 'video.mp4').read_bytes()).hexdigest())
        self.assertEqual(report['claims']['origin'], 'local')  # claim only
        self.assertEqual(report['claims']['igt_final_ms'], 0)
        self.assertEqual(report['claims']['rta_elapsed_ns'], 123)
        self.assertFalse(report['claims']['replay_member_present'])
        self.assertEqual(report['findings'], [])  # missing optional replay is not a violation
        self.assertEqual(report['checks']['video_playback'], 'not_assessed')
        before = path.read_bytes()
        for flags in ([], ['--json']):
            result = subprocess.run([sys.executable, '-B', str(TOOLS / 'validate_package.py'), str(path), *flags],
                                    capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn(str(self.root), result.stdout)
            if flags:
                self.assertEqual(json.loads(result.stdout), report)
            else:
                self.assertIn('No competition eligibility established', result.stdout)
                self.assertIn('Manifest claims (not observed gameplay)', result.stdout)
        self.assertEqual(path.read_bytes(), before)

    def test_schema1_and_incomplete(self):
        path = self.archive(schema=1)
        report = self.read_only(path)
        self.assertEqual(report['verification'], 'consistent')
        self.assertIsNone(report['claims'])
        self.assertIsNone(report['archive']['manifest_sha256'])
        self.assertEqual(report['findings'], [{'code': 'unsupported_run_schema'}])
        self.assertEqual(exit_code(report), 3)
        metadata = dict(self.metadata, capture_status='incomplete', attempt_status='abandoned',
                        igt_final_ms=None, rta_elapsed_ns=None, track_id=None, difficulty=None)
        path = self.archive({'notes.txt': ('diagnostic', b'x')}, metadata, name='incomplete.zip')
        report = self.read_only(path)
        self.assertEqual(report['verification'], 'consistent')
        self.assertEqual(report['assessment'], 'insufficient_evidence')
        self.assertIsNone(report['claims']['igt_final_ms'])
        self.assertEqual({x['code'] for x in report['findings']}, {'no_nonempty_video',
            'attempt_or_capture_not_complete_claim', 'final_igt_missing', 'track_claim_missing'})

    def test_executable_looking_names(self):
        path = self.archive({'video.mp4': ('video', b'not video'),
                             'payload.EXE.txt': ('diagnostic', b'plain inert text'),
                             'driver.dll': ('diagnostic', b'plain inert text')})
        # Existing exact verifier accepts these names: this is a real source-backed gap.
        self.assertTrue(ea.verify(path)['integrity_matches_manifest'])
        report = self.read_only(path)
        self.assertEqual(report['verification'], 'consistent')
        self.assertEqual(report['assessment'], 'review_required')
        self.assertEqual({x['member'] for x in report['findings']}, {'payload.EXE.txt', 'driver.dll'})
        self.assertEqual(report['checks']['payload_safety'], 'not_established')
        self.assertEqual(exit_code(report), 3)
        path = self.archive({'video.mp4': ('video', b'not video'),
                             'opaque.bin': ('diagnostic', b'MZ inert signature marker')}, name='opaque.zip')
        report = self.read_only(path)
        self.assertEqual(report['assessment'], 'insufficient_evidence')
        self.assertEqual(report['checks']['payload_safety'], 'not_established')

    def test_settings_and_replay(self):
        snapshot = dict(schema=1, scope='attempt_settings_snapshot', run_id=self.metadata['run_id'],
                        session_id=self.metadata['session_id'], source='profile', settings=ssc_profiles.defaults())
        path = self.archive({'video.mp4': ('video', b'not video'), 'replay.bin': ('replay', b''),
                             'attempt-settings.json': ('diagnostic', ssc_profiles.canonical(snapshot))})
        report = self.read_only(path)
        self.assertEqual(report['claims']['settings_consistency'], 'matches_manifest')
        self.assertTrue(report['claims']['replay_member_present'])
        self.assertEqual(report['checks']['replay_playback'], 'not_assessed')
        self.assertEqual(report['checks']['reference_baseline'], 'not_assessed')
        raw = ea.verify(path, include_manifest=True)
        raw['attempt_settings_sha256'] = 'f' * 64
        self.assertEqual(inspect(lambda: raw)['verification'], 'malformed_report')

    def rewrite(self, path, change, name):
        with zipfile.ZipFile(path, 'r') as src:
            contents = {n: src.read(n) for n in src.namelist()}
        change(contents)
        target = self.root / name
        with zipfile.ZipFile(target, 'x', compression=zipfile.ZIP_STORED) as dst:
            for n, data in contents.items():
                dst.writestr(n, data)
        return target

    def test_invalid_and_unsupported(self):
        path = self.archive()
        bad = self.rewrite(path, lambda d: d.__setitem__('video.mp4', b'X' * len(d['video.mp4'])), 'bad.zip')
        self.assertEqual(self.read_only(bad)['verification'], 'invalid_package')
        def forge(d):
            m = json.loads(d['manifest.json'])
            m['policy_eligibility'] = 'approved'
            d['manifest.json'] = json.dumps(m, sort_keys=True, separators=(',', ':')).encode()
        forged = self.rewrite(path, forge, 'forged.zip')
        self.assertEqual(self.read_only(forged)['verification'], 'invalid_package')
        def unknown(d):
            m = json.loads(d['manifest.json'])
            m['schema'] = 99
            d['manifest.json'] = json.dumps(m).encode()
        unsupported = self.rewrite(path, unknown, 'future.zip')
        self.assertEqual(self.read_only(unsupported)['verification'], 'invalid_or_unsupported')
        tiny = self.root / 'tiny.zip'
        tiny.write_bytes(b'truncated')
        self.assertEqual(self.read_only(tiny)['verification'], 'invalid_package')
        # Known ZIP64 locator refusal; intentionally not a valid ZIP64 archive.
        locator = self.root / 'locator.zip'
        locator.write_bytes(b'PK\x06\x07' + b'\0' * 38)
        self.assertEqual(self.read_only(locator)['verification'], 'unsupported_format')
        self.assertEqual(exit_code(validate(locator)), 3)

    def test_unavailable_and_adapter_errors(self):
        self.assertEqual(validate(self.root / 'missing.zip')['verification'], 'unavailable')
        self.assertEqual(validate(self.stage)['verification'], 'unsupported_format')
        self.assertEqual(inspect(None)['verification'], 'unavailable')
        path = self.archive()
        calls = []
        def failing(p, *, include_manifest):
            calls.append((p, include_manifest))
            raise OSError('private path C:/private')
        report = validate(path, failing)
        self.assertEqual(calls, [(path, True)])
        self.assertEqual(report['verification'], 'unavailable')
        self.assertEqual(exit_code(report), 4)
        self.assertNotIn('private', human(report))
        with patch.dict(sys.modules, {'evidence_archive': None}):
            self.assertEqual(validate(path)['reason'], 'verifier_unavailable')
        self.assertEqual(validate(path, lambda *a, **k: None)['verification'], 'malformed_report')

    def test_malformed_report_fail_closed(self):
        raw = ea.verify(self.archive(), include_manifest=True)
        mutations = [lambda d: d.update(schema=True), lambda d: d.update(eligibility_verified=True),
                     lambda d: d.update(integrity_matches_manifest=1), lambda d: d.update(complete_submission=True),
                     lambda d: d.update(bytes_verified=0), lambda d: d.update(files_verified=2),
                     lambda d: d.update(run_id='f' * 32), lambda d: d.update(manifest_sha256='bad'),
                     lambda d: d['manifest'].update(scope='bad'), lambda d: d['manifest'].update(igt_final_ms=True),
                     lambda d: d['manifest'].update(policy_eligibility='approved'),
                     lambda d: d['manifest'].update(replay_available=True),
                     lambda d: d['manifest']['files'][0].update(path='../secret'),
                     lambda d: d['manifest']['files'][0].update(size=-1),
                     lambda d: d['manifest']['files'][0].update(role=[]),
                     lambda d: d['manifest'].update(files=d['manifest']['files'] * 65)]
        for change in mutations:
            candidate = copy.deepcopy(raw)
            change(candidate)
            report = inspect(lambda: candidate)
            self.assertEqual(report['verification'], 'malformed_report')
            self.assertEqual(report['assessment'], 'review_required')
            self.assertIsNone(report['claims'])
            self.assertIsNone(report['archive'])
            self.assertFalse(report['eligibility_verified'])
            self.assertEqual(exit_code(report), 3)
        self.assertEqual(inspect(lambda: raw)['verification'], 'consistent')


if __name__ == '__main__':
    unittest.main()
