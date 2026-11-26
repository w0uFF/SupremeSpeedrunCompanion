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
from synthetic_index_port import BoundSyntheticIndex
import synthetic_index_port

RUN='a'*32


def ports(exe, db):
    return BoundSyntheticIndex.bind(db,exe)


def child():
    exe,db,managed,private=map(Path,sys.argv[2:6])
    point=sys.argv[6]
    store=Store(private,RUN)
    index=ports(exe,db)
    lookup=index.lookup
    expected=plan(store,managed,index)
    if point=='after_link':
        assert expected.action=='retry_archive_publication'
        original=os.link
        def stop_after_link(source,destination):
            original(source,destination)
            os._exit(91)
        archive_publish.os.link=stop_after_link
    elif point=='after_insert':
        assert expected.action=='retry_index'
        original=synthetic_index_port._insert
        def stop_after_insert(exe_path,db_path,report,origin):
            assert original(exe_path,db_path,report,origin)=='inserted'
            os._exit(91)
        synthetic_index_port._insert=stop_after_insert
    else:
        raise ValueError('invalid fixture point')
    execute(store,managed,index,expected)
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
        index=ports(exe,db)
        lookup=index.lookup
        final=managed/('synthetic-'+RUN+'.zip')
        partial=final.with_name(final.name+'.partial')
        ix.copy_new(source,partial)
        initial=snapshot(private)
        db_initial=db.read_bytes()
        expected=plan(store,managed,index)
        assert expected.action=='retry_archive_publication'
        # A separate final copy is a conflict, even if its bytes match.
        final.write_bytes(source.read_bytes())
        conflict=snapshot(managed)
        try:
            execute(store,managed,index,expected)
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
        alias=plan(store,managed,index)
        assert alias.action=='retry_archive_alias_cleanup'
        assert execute(store,managed,index,alias)=='alias_cleaned'
        assert not partial.exists() and final.read_bytes()==source.read_bytes()
        assert snapshot(private)==initial and db.read_bytes()==db_initial
        reconcile(store,managed,lookup,observe(store,managed,lookup))
        published=snapshot(private)
        expected=plan(store,managed,index)
        assert expected.action=='retry_index'
        # Actual conflicting origin row must not be promoted or overwritten.
        conflict_dir=root/'conflict'
        conflict_dir.mkdir()
        conflict_db=conflict_dir/'runs.sqlite'
        subprocess.run([exe,'init',conflict_db],check=True,timeout=15)
        conflicting_index=ports(exe,conflict_db)
        # Even two empty databases with identical read-only facts are different identities.
        other_bytes=conflict_db.read_bytes()
        assert plan(store,managed,conflicting_index).action=='retry_index'
        try:
            execute(store,managed,conflicting_index,expected)
        except ReviewRequired:
            pass
        else:
            raise AssertionError('different empty index accepted')
        assert conflict_db.read_bytes()==other_bytes
        # Replace the bound DB with byte-identical data while retaining its original inode.
        saved=db.with_name('original-db')
        db.rename(saved)
        db.write_bytes(saved.read_bytes())
        try:
            assert plan(store,managed,index) is None
            try:
                execute(store,managed,index,expected)
            except ReviewRequired:
                pass
            else:
                raise AssertionError('replacement index accepted')
            assert db.read_bytes()==saved.read_bytes()
        finally:
            db.unlink()
            saved.rename(db)
        index.check()
        assert ix.native_index(exe,conflict_db,report,'imported')=='inserted'
        conflict_bytes=conflict_db.read_bytes()
        assert plan(store,managed,conflicting_index) is None
        try:
            execute(store,managed,conflicting_index,expected)
        except ReviewRequired:
            pass
        else:
            raise AssertionError('conflicting index accepted')
        assert conflict_db.read_bytes()==conflict_bytes and snapshot(private)==published
        crash('after_insert')
        assert snapshot(private)==published and lookup(RUN,report,'synthetic')=='identical'
        assert plan(store,managed,index) is None
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
