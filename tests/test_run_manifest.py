from pathlib import Path
import sys
import tempfile
import unittest
import os
import zipfile
import json
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import archive_publish as ap
import run_manifest as rm
import evidence_archive as ea

META = dict(run_id='1'*32, session_id='2'*32, created_utc='2026-01-02T03:04:05Z', mod_build='test',
            origin='synthetic', recording_mode='manual', attempt_status='finished', capture_status='finalized')
FILES = [{'path':'video.mp4','role':'video','size':3,'sha256':'a'*64}]


class ManifestTests(unittest.TestCase):
    def test_metadata(self):
        result = rm.build(META, FILES)
        rm.validate(result)
        self.assertIsNone(result['igt_final_ms'])
        self.assertEqual(result['policy_eligibility'], 'unknown')
        self.assertFalse(result['replay_available'])
        for change in ({'policy_eligibility':'eligible'}, {'schema_version':2.0}, {'replay_available':0}):
            with self.assertRaises(ValueError):
                rm.validate(dict(result, **change))

    def test_invalid_metadata(self):
        for change in ({'run_id':'bad'}, {'created_utc':'2026-02-30T00:00:00Z'}, {'igt_final_ms':True},
                       {'attempt_status':'abandoned','igt_final_ms':10}, {'rta_elapsed_ns':10}, {'warnings':['\n']},
                       {'policy_eligibility':'eligible'}):
            with self.assertRaises(ValueError):
                rm.build(dict(META, **change), FILES)
        with self.assertRaises(ValueError):
            rm.build(META, [])
        result = rm.build(dict(META, igt_final_ms=0, rta_elapsed_ns=0, clock_source='test', event_semantics_version='v1'), FILES)
        self.assertEqual(result['rta_elapsed_ns'], 0)
        rm.validate(result)


class PublishTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.final = Path(tmp.name) / 'run.zip'
        self.partial = self.final.with_name('run.zip.partial')
        self.partial.write_bytes(b'validated fixture')

    @staticmethod
    def verify(path):
        if path.read_bytes() != b'validated fixture':
            raise ValueError('invalid candidate')
        return {'fixture_verified':True}

    def test_publish_and_conflict(self):
        self.final.write_bytes(b'other')
        with self.assertRaises(FileExistsError):
            ap.publish(self.final, self.verify)
        self.assertEqual(self.final.read_bytes(), b'other')
        self.assertTrue(self.partial.exists())
        self.final.unlink()
        self.assertTrue(ap.publish(self.final, self.verify)['fixture_verified'])
        self.assertEqual(self.final.read_bytes(), b'validated fixture')
        self.assertFalse(self.partial.exists())

    def test_recover_and_reject_invalid(self):
        self.partial.write_bytes(b'bad')
        with self.assertRaises(ValueError):
            ap.publish(self.final, self.verify)
        self.assertFalse(self.final.exists())
        self.partial.write_bytes(b'validated fixture')
        os.link(self.partial, self.final)
        ap.publish(self.final, self.verify)
        self.assertFalse(self.partial.exists())
        self.assertEqual(self.final.read_bytes(), b'validated fixture')

    def test_link_failure_retains_partial(self):
        with patch.object(ap.os, 'link', side_effect=OSError('fixture')):
            with self.assertRaises(OSError):
                ap.publish(self.final, self.verify)
        self.assertTrue(self.partial.exists())
        self.assertFalse(self.final.exists())


class ArchiveIntegration(unittest.TestCase):
    def test_roundtrip_and_schema1(self):
        with tempfile.TemporaryDirectory() as tmp:
            parent = Path(tmp)
            stage = parent / 'stage'
            stage.mkdir()
            (stage / 'video.mp4').write_bytes(b'abc')
            path = parent / 'run.zip'
            report = ap.create_run(stage, {'video.mp4': 'video'}, path, META)
            self.assertEqual(report, ea.verify(path))
            self.assertEqual(report['run_id'], META['run_id'])
            self.assertFalse(report['complete_submission'])
            self.assertFalse(path.with_name('run.zip.partial').exists())
            with zipfile.ZipFile(path) as archive:
                raw = archive.read('manifest.json')
            import hashlib
            self.assertEqual(report['manifest_sha256'], hashlib.sha256(raw).hexdigest())
            data = json.loads(raw)
            data['policy_eligibility'] = 'eligible'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('video.mp4', b'abc')
                archive.writestr('manifest.json', json.dumps(data, sort_keys=True, separators=(',', ':')))
            with self.assertRaises(ValueError):
                ea.verify(path)
            legacy = parent / 'legacy.zip'
            self.assertEqual(ea.create(stage, {'video.mp4': 'video'}, legacy)['schema'], 1)
            self.assertEqual(ea.verify(legacy)['schema'], 1)


if __name__ == '__main__':
    unittest.main()
