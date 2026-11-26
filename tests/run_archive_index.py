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
import archive_status as status
import os

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
    lookup=lambda i,r,o:status.native_status(exe,db,i,r,o)
    probe=lambda i=meta['run_id'],o='imported':status.inspect(managed,i,o,lookup)
    before=db.read_bytes()
    assert probe()['archive']=='absent' and probe()['index']=='missing'
    partial.write_bytes(original)  # valid pending from interrupted process
    assert probe()['archive']=='pending' and probe()['action']=='retry_publication'
    assert db.read_bytes()==before
    try:
        ix.admit(partial, managed, 'imported', failed)
    except RuntimeError:
        pass
    else:
        raise AssertionError('outage not propagated')
    assert final.read_bytes() == original and not partial.exists()
    assert probe()['archive']=='finalized' and probe()['action']=='retry_index'
    assert db.read_bytes()==before
    assert ix.admit(final, managed, 'imported', index)['index'] == 'inserted'
    before=db.read_bytes()
    assert probe()['index']=='identical' and probe()['action']=='none'
    os.link(final,partial)
    assert probe()['archive']=='finalized_with_pending_alias' and probe()['action']=='retry_alias_cleanup_and_index'
    assert partial.exists() and os.path.samefile(partial,final)
    assert ix.admit(final,managed,'imported',index)['index']=='identical'
    assert not partial.exists()
    partial.write_bytes(original)
    assert probe()['archive']=='separate_pending_conflict' and probe()['action']=='review'
    assert partial.read_bytes()==original
    partial.unlink()
    assert db.read_bytes()==before
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
    assert probe('4'*32)['archive']=='review_required'
    assert probe('3'*32)['index']=='conflict' and probe('3'*32)['action']=='review'
    absent_db=root/'missing'/'runs.sqlite'
    unavailable=status.inspect(managed,meta['run_id'],'imported',
        lambda i,r,o:status.native_status(exe,absent_db,i,r,o))
    assert unavailable['index']=='unavailable' and unavailable['action']=='review' and not absent_db.exists()
    # Orphaned index is visible without reading any archive, and no repair is attempted.
    moved=root/'temporarily-moved.zip'
    final.rename(moved)
    assert probe()['archive']=='absent' and probe()['index']=='present' and probe()['action']=='review'
    moved.rename(final)
    assert probe()['index']=='identical'
    with sqlite3.connect(db) as conn:
        assert conn.execute('select origin from runs order by seq').fetchall() == [(1,), (2,)]
        assert conn.execute('select count(*) from runs where origin=0').fetchone()[0]==0
    conn.close()
    bad = ix.project(ix.checked(source,'imported'),'imported').replace(b'\nimported\n',b'\nlocal\n')
    assert subprocess.run([exe,'insert',db],input=bad,capture_output=True,timeout=10).returncode==3
print('Verified archive -> native index: pending recovery, retry, immutable digest, conflict and nonlocal origin checks passed')
