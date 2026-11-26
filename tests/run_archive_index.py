"""Actual archive publication, simulated interruption and native WinSQLite readback."""
from pathlib import Path
import hashlib
import sqlite3
import subprocess
import sys
import tempfile
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import evidence_archive as ea
import index_archive as ix

with tempfile.TemporaryDirectory() as d:
    root = Path(d)
    stage = root / 'stage'
    managed = root / 'managed-épreuve'
    stage.mkdir()
    managed.mkdir()
    (stage / 'clip.mp4').write_bytes(b'fixture not playable')
    exe = Path(sys.argv[1])
    db = root / 'runs.sqlite'
    subprocess.run([exe, 'init', db], check=True, timeout=15)
    meta = dict(run_id='1'*32, session_id='2'*32, created_utc='2026-01-02T03:04:05Z',
                mod_build='test', origin='local', recording_mode='manual', attempt_status='finished',
                capture_status='finalized', igt_final_ms=0, event_semantics_version='test')
    source = root / 'source.zip'
    ea.create(stage, {'clip.mp4':'video'}, source, metadata=meta)
    original = source.read_bytes()
    index = lambda r,o: ix.native_index(exe, db, r, o)
    def failed(*args):
        raise RuntimeError('simulated index outage')
    final = managed / ('imported-' + meta['run_id'] + '.zip')
    partial = final.with_name(final.name+'.partial')
    partial.write_bytes(original)  # valid pending from interrupted process
    try:
        ix.admit(partial, managed, 'imported', failed)
    except RuntimeError:
        pass
    else:
        raise AssertionError('outage not propagated')
    assert final.read_bytes() == original and not partial.exists()
    assert ix.admit(final, managed, 'imported', index)['index'] == 'inserted'
    assert ix.admit(source, managed, 'imported', index)['index'] == 'identical'
    with zipfile.ZipFile(final) as z:
        digest = hashlib.sha256(z.read('manifest.json')).hexdigest()
    with sqlite3.connect(db) as conn:
        assert conn.execute('select origin,igt_ms,manifest_sha256 from runs').fetchall() == [(1,0,digest)]
    conn.close()
    # Same ID with different valid bytes: never overwrite existing archive/index.
    conflict = root/'conflict.zip'
    ea.create(stage, {'clip.mp4':'video'}, conflict, metadata=dict(meta, igt_final_ms=1))
    try:
        ix.admit(conflict, managed, 'imported', index)
    except ValueError:
        pass
    else:
        raise AssertionError('conflict accepted')
    assert final.read_bytes() == original and source.read_bytes() == original
    synthetic = root/'synthetic.zip'
    ea.create(stage, {'clip.mp4':'video'}, synthetic, metadata=dict(meta, run_id='3'*32, origin='synthetic'))
    assert ix.admit(synthetic, managed, 'synthetic', index)['index'] == 'inserted'
    # External import cannot silently overwrite prior fixture origin.
    assert ix.admit(synthetic, managed, 'imported', index)['index'] == 'conflict'
    invalid = root/'invalid.zip'
    invalid.write_bytes(b'not zip')
    try:
        ix.admit(invalid, managed, 'imported', index)
    except ValueError:
        pass
    else:
        raise AssertionError('invalid archive admitted')
    pending = managed/('imported-'+'4'*32+'.zip.partial')
    pending.write_bytes(b'interrupted copy')
    fourth = root/'fourth.zip'
    ea.create(stage, {'clip.mp4':'video'}, fourth, metadata=dict(meta,run_id='4'*32))
    try:
        ix.admit(fourth, managed, 'imported', index)
    except ValueError:
        pass
    else:
        raise AssertionError('invalid pending overwritten')
    assert pending.read_bytes() == b'interrupted copy'
    with sqlite3.connect(db) as conn:
        assert conn.execute('select origin from runs order by seq').fetchall() == [(1,), (2,)]
        assert conn.execute('select count(*) from runs where origin=0').fetchone()[0]==0
    conn.close()
    bad = ix.project(ix.checked(source,'imported'),'imported').replace(b'\nimported\n',b'\nlocal\n')
    assert subprocess.run([exe,'insert',db],input=bad,capture_output=True,timeout=10).returncode==3
print('Verified archive -> native index: pending recovery, retry, immutable digest, conflict and nonlocal origin checks passed')
