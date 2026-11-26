"""Real ZIP and native index retry with process exits at publication/insertion boundaries."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import archive_publish
import evidence_archive as ea
import index_archive as ix
from archive_status import native_status
from synthetic_admission import Context, Journal
from synthetic_journal_store import Store, ReviewRequired
from synthetic_recovery import observe, reconcile
from synthetic_retry import plan, execute

RUN='a'*32


def ports(exe, db):
    return (lambda i,r,o:native_status(exe,db,i,r,o),
            lambda r,o:ix.native_index(exe,db,r,o))


def child():
    exe,db,managed,private=map(Path,sys.argv[2:6])
    point=sys.argv[6]
    store=Store(private,RUN)
    lookup,insert=ports(exe,db)
    expected=plan(store,managed,lookup)
    if point=='after_link':
        assert expected.action=='retry_archive_publication'
        original=os.link
        def stop_after_link(source,destination):
            original(source,destination)
            os._exit(91)
        archive_publish.os.link=stop_after_link
    elif point=='after_insert':
        assert expected.action=='retry_index'
        original=insert
        def insert(report,origin):
            assert original(report,origin)=='inserted'
            os._exit(91)
    else:
        raise ValueError('invalid fixture point')
    execute(store,managed,lookup,insert,expected)
    raise AssertionError('process exit not reached')


def snapshot(directory):
    return {p.name:p.read_bytes() for p in directory.iterdir()}


def scenario(exe):
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        stage,managed,private=(root/n for n in ('stage','managed','journal'))
        for d in (stage,managed,private):
            d.mkdir()
        db,source=root/'runs.sqlite',root/'source.zip'
        subprocess.run([exe,'init',db],check=True,timeout=15)
        (stage/'clip.mp4').write_bytes(b'controlled synthetic data, not playable')
        metadata=dict(run_id=RUN,session_id='b'*32,created_utc='2026-01-02T03:04:05Z',
                      mod_build='T30-fixture',origin='synthetic',recording_mode='manual',
                      attempt_status='abandoned',capture_status='incomplete',
                      event_semantics_version='fixture')
        ea.create(stage,{'clip.mp4':'video'},source,metadata=metadata)
        report=ix.checked(source,'synthetic')
        context=Context(RUN,1,'c'*64)
        j=Journal(context)
        t=j.finish(context,'abandoned',False)
        v=t.verify(context,'abandoned','synthetic',report['manifest_sha256'],False)
        store=Store(private,RUN)
        for entry in (j,t,v):
            store.append(entry)
        lookup,insert=ports(exe,db)
        final=managed/('synthetic-'+RUN+'.zip')
        partial=final.with_name(final.name+'.partial')
        ix.copy_new(source,partial)
        initial=snapshot(private)
        db_initial=db.read_bytes()
        expected=plan(store,managed,lookup)
        assert expected.action=='retry_archive_publication'
        # A separate final copy is a conflict, even if its bytes match.
        final.write_bytes(source.read_bytes())
        conflict=snapshot(managed)
        try:
            execute(store,managed,lookup,insert,expected)
        except ReviewRequired:
            pass
        else:
            raise AssertionError('stale publication plan executed')
        assert snapshot(managed)==conflict and snapshot(private)==initial and db.read_bytes()==db_initial
        final.unlink()  # deliberate fixture reset
        def crash(point):
            result=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--child',
                                   str(exe),str(db),str(managed),str(private),point],timeout=30)
            assert result.returncode==91
        crash('after_link')
        assert os.path.samefile(final,partial) and snapshot(private)==initial and db.read_bytes()==db_initial
        alias=plan(store,managed,lookup)
        assert alias.action=='retry_archive_alias_cleanup'
        assert execute(store,managed,lookup,insert,alias)=='alias_cleaned'
        assert not partial.exists() and final.read_bytes()==source.read_bytes()
        assert snapshot(private)==initial and db.read_bytes()==db_initial
        reconcile(store,managed,lookup,observe(store,managed,lookup))
        published=snapshot(private)
        expected=plan(store,managed,lookup)
        assert expected.action=='retry_index'
        # Actual conflicting origin row must not be promoted or overwritten.
        conflict_db=root/'conflict.sqlite'
        subprocess.run([exe,'init',conflict_db],check=True,timeout=15)
        assert ix.native_index(exe,conflict_db,report,'imported')=='inserted'
        conflicting_lookup,conflicting_insert=ports(exe,conflict_db)
        conflict_bytes=conflict_db.read_bytes()
        assert plan(store,managed,conflicting_lookup) is None
        try:
            execute(store,managed,conflicting_lookup,conflicting_insert,expected)
        except ReviewRequired:
            pass
        else:
            raise AssertionError('conflicting index accepted')
        assert conflict_db.read_bytes()==conflict_bytes and snapshot(private)==published
        crash('after_insert')
        assert snapshot(private)==published and lookup(RUN,report,'synthetic')=='identical'
        assert plan(store,managed,lookup) is None
        indexed_bytes=db.read_bytes()
        reconcile(store,managed,lookup,observe(store,managed,lookup))
        assert observe(store,managed,lookup).action=='none'
        assert db.read_bytes()==indexed_bytes and final.read_bytes()==source.read_bytes()
        assert all((private/n).read_bytes()==b for n,b in published.items())
        print('T30 real archive-link and native-insert process interruption; separate explicit retries PASS')


if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='--child':
        child()
    else:
        scenario(Path(sys.argv[1]).resolve())
