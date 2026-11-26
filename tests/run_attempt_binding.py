"""C++ immutable binding -> independently checked Python schema-2 archive."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import attempt_settings_evidence as ase
import archive_publish as ap
import evidence_archive as ea
import ssc_profiles as s

result = subprocess.run([sys.argv[1]], capture_output=True, timeout=10)
assert result.returncode == 0, result.stderr
raw = result.stdout
meta = dict(run_id='1'*32, session_id='2'*32, created_utc='2026-01-02T03:04:05Z',
            mod_build='test', origin='synthetic', recording_mode='automatic',
            attempt_status='finished', capture_status='finalized')
expected = dict(schema=1, scope='attempt_settings_snapshot', run_id=meta['run_id'],
                session_id=meta['session_id'], source='launcher',
                settings=dict(s.defaults(), auto_record=True, fps='60'))
assert raw == s.canonical(expected)
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    stage = root / 'stage'
    stage.mkdir()
    (stage / 'clip.mp4').write_bytes(b'synthetic video bytes, not a playable recording')
    settings_file = stage / ase.NAME
    settings_file.write_bytes(raw)
    path = root / 'attempt.zip'
    selection = {'clip.mp4': 'video', ase.NAME: 'diagnostic'}
    report = ap.create_run(stage, selection, path, meta)
    assert report == ea.verify(path)
    assert report['attempt_settings_consistent'] and not report['eligibility_verified']
    assert report['attempt_settings_sha256'] == hashlib.sha256(raw).hexdigest()
    with zipfile.ZipFile(path) as archive:
        manifest = archive.read('manifest.json')
    assert hashlib.sha256(manifest).hexdigest() == report['manifest_sha256']
    valid = path.read_bytes()
    # Rehash a mismatched settings member and its manifest: structural check still rejects it.
    for bad in (dict(expected, run_id='3'*32), dict(expected, session_id='4'*32),
                dict(expected, settings=dict(expected['settings'], package_directory='D:\\Private')),
                dict(expected, schema=2), dict(expected, source='unknown')):
        settings_file.write_bytes(s.canonical(bad))
        target = root / 'bad.zip'
        try:
            ea.create(stage, selection, target, metadata=meta)
        except ValueError:
            pass
        else:
            raise AssertionError('inconsistent settings accepted')
        assert not target.exists()
    settings_file.write_bytes(raw)
    try:
        ea.create(stage, dict(selection, **{ase.NAME: 'timing'}), root/'wrong-role.zip', metadata=meta)
    except ValueError:
        pass
    else:
        raise AssertionError('wrong role accepted')
    assert path.read_bytes() == valid
    legacy = ea.create(stage, {'clip.mp4': 'video'}, root/'old.zip', metadata=meta)
    assert 'attempt_settings_consistent' not in legacy
print('C++ immutable settings -> schema-2 archive: canonical bytes, identity, privacy and manifest hash verified; eligibility remains unverified')
