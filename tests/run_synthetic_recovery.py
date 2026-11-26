"""Actual archive verification, native index readback and process-death coordination."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import evidence_archive as ea
import index_archive as ix
from archive_status import native_status
from synthetic_admission import Context, Journal
from synthetic_journal_store import Store, ReviewRequired
from synthetic_recovery import observe, reconcile

RUN = 'a'*32


def crash_child():
    exe, db, source, managed = map(Path, sys.argv[2:6])
    point = sys.argv[6]

    def interrupted(report, origin):
        if point == 'after_index':
            if ix.native_index(exe, db, report, origin) != 'inserted':
                raise RuntimeError('expected first synthetic row')
        elif point != 'after_archive':
            raise RuntimeError('invalid interruption point')
        os._exit(91)

    ix.admit(source, managed, 'synthetic', interrupted)
    raise RuntimeError('interruption not reached')


def journal_bytes(root):
    return {p.name:p.read_bytes() for p in root.iterdir()}


def scenario(exe, point):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        stage, managed, private = (root/n for n in ('stage','managed','journal'))
        for d in (stage, managed, private):
            d.mkdir()
        db, source = root/'runs.sqlite', root/'source.zip'
        subprocess.run([exe,'init',db],check=True,timeout=15)
        (stage/'clip.mp4').write_bytes(b'not playable: controlled synthetic fixture')
        meta = dict(run_id=RUN,session_id='b'*32,created_utc='2026-01-02T03:04:05Z',
                    mod_build='T29-fixture',origin='synthetic',recording_mode='manual',
                    attempt_status='abandoned',capture_status='incomplete',
                    event_semantics_version='fixture')
        ea.create(stage,{'clip.mp4':'video'},source,metadata=meta)
        report = ix.checked(source,'synthetic')
        context = Context(RUN,1,'c'*64)
        j = Journal(context)
        t = j.finish(context,'abandoned',False)
        v = t.verify(context,'abandoned','synthetic',report['manifest_sha256'],False)
        store = Store(private,RUN)
        for snapshot in (j,t,v):
            store.append(snapshot)
        lookup = lambda i,r,o:native_status(exe,db,i,r,o)
        assert observe(store,managed,lookup).action=='await_archive'
        before = journal_bytes(private)
        child = subprocess.run([sys.executable,str(Path(__file__).resolve()),'--child',
                                str(exe),str(db),str(source),str(managed),point],timeout=30)
        assert child.returncode==91 and journal_bytes(private)==before
        final = managed/('synthetic-'+RUN+'.zip')
        partial = final.with_name(final.name+'.partial')
        assert final.read_bytes()==source.read_bytes() and not partial.exists()
        db_bytes = db.read_bytes()
        decision = observe(store,managed,lookup)
        assert decision.action=='append_published'
        # A stale decision must not modify the journal after archive evidence changes.
        partial.write_bytes(b'invalid competing pending')
        assert observe(store,managed,lookup).action=='review'
        try:
            reconcile(store,managed,lookup,decision)
        except ReviewRequired:
            pass
        else:
            raise AssertionError('stale decision accepted')
        assert journal_bytes(private)==before and db.read_bytes()==db_bytes
        assert partial.read_bytes()==b'invalid competing pending'
        partial.unlink()  # fixture resets deliberate corruption; not coordinator behavior
        assert reconcile(store,managed,lookup,decision)=='appended'
        assert db.read_bytes()==db_bytes
        assert all((private/n).read_bytes()==b for n,b in before.items())
        next_step = observe(store,managed,lookup)
        if point=='after_archive':
            assert next_step.action=='retry_index'
            assert ix.native_index(exe,db,ix.checked(final,'synthetic'),'synthetic')=='inserted'
            next_step = observe(store,managed,lookup)
        assert next_step.action=='append_indexed'
        db_bytes = db.read_bytes()
        assert reconcile(store,managed,lookup,next_step)=='appended'
        assert observe(store,managed,lookup).action=='none'
        assert db.read_bytes()==db_bytes and final.read_bytes()==source.read_bytes()
        # Journal digest binding alone cannot override independently verified terminal fields.
        wrong = root/'wrong-journal'
        wrong.mkdir()
        wrong_store=Store(wrong,RUN)
        w=j.finish(context,'finished',True)
        for snapshot in (j,w,w.verify(context,'finished','synthetic',v.digest,True)):
            wrong_store.append(snapshot)
        assert observe(wrong_store,managed,lookup).action=='review'
        # Read-only compare of a different origin would conflict; never request local.
        assert native_status(exe,db,RUN,ix.checked(final,'synthetic'),'imported')=='conflict'
        assert db.read_bytes()==db_bytes
        print('Actual subprocess interruption',point,'reconciled; journal-only writes; native synthetic index identical')


if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='--child':
        crash_child()
    else:
        executable=Path(sys.argv[1]).resolve()
        for boundary in ('after_archive','after_index'):
            scenario(executable,boundary)
