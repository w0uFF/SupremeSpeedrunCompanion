"""Real ZIP and native index retry with process exits at publication/insertion boundaries."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch

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
    elif point=='after_alias_cleanup':
        assert expected.action=='retry_archive_alias_cleanup'
        original=Path.unlink
        def stop_after_unlink(path,*args,**kwargs):
            original(path,*args,**kwargs)
            if path==managed/('synthetic-'+RUN+'.zip.partial'):
                os._exit(91)
        Path.unlink=stop_after_unlink
    elif point in ('published_flush','indexed_publish'):
        decision=observe(store,managed,lookup)
        assert decision.action==('append_published' if point=='published_flush' else 'append_indexed')
        target='after_flush' if point=='published_flush' else 'after_publish'
        def stop_at(boundary):
            if boundary==target: os._exit(91)
        reconcile(store,managed,lookup,decision,stop_at)
        raise AssertionError('journal interruption not reached')
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


def scenario(exe, lost_response=False):
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
        if lost_response:
            assert execute(store,managed,index,alias)=='alias_cleaned'
        else:
            crash('after_alias_cleanup')
            assert plan(store,managed,index) is None
            assert observe(store,managed,lookup).action=='append_published'
            try:
                execute(store,managed,index,alias)
            except ReviewRequired:
                pass
            else:
                raise AssertionError('stale alias cleanup accepted')
        assert not partial.exists() and final.read_bytes()==source.read_bytes()
        assert snapshot(private)==initial and db.read_bytes()==db_initial
        if not lost_response:
            before=observe(store,managed,lookup)
            crash('published_flush')
            state=store.inspect()
            assert state.latest==v and state.pending==before.snapshot and not state.alias
            assert all((private/n).read_bytes()==b for n,b in initial.items())
            assert db.read_bytes()==db_initial and final.read_bytes()==source.read_bytes()
            assert plan(store,managed,index) is None
            assert observe(store,managed,lookup).action=='retry_journal'
            try:
                reconcile(store,managed,lookup,before)
            except ReviewRequired:
                pass
            else:
                raise AssertionError('stale journal decision accepted')
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
        if lost_response:
            # Read-only backend outage must not authorize publication or insertion.
            with patch('synthetic_index_port._lookup', side_effect=RuntimeError('fixture lookup outage')):
                assert plan(store,managed,index) is None
                try:
                    execute(store,managed,index,expected)
                except ReviewRequired:
                    pass
                else:
                    raise AssertionError('unavailable read accepted')
            assert db.read_bytes()==db_initial and snapshot(private)==published
            with patch('synthetic_index_port._insert', side_effect=RuntimeError('fixture before insert')):
                try:
                    execute(store,managed,index,expected)
                except RuntimeError as exc:
                    assert str(exc)=='fixture before insert'
                else:
                    raise AssertionError('failed insert reported success')
            assert db.read_bytes()==db_initial and snapshot(private)==published
            assert plan(store,managed,index)==expected
            # Real native insertion succeeds, but the adapter never receives its response.
            original=synthetic_index_port._insert
            def commit_then_fail(*args):
                assert original(*args)=='inserted'
                raise RuntimeError('fixture lost response after native commit')
            with patch('synthetic_index_port._insert',side_effect=commit_then_fail):
                try:
                    execute(store,managed,index,expected)
                except RuntimeError as exc:
                    assert str(exc)=='fixture lost response after native commit'
                else:
                    raise AssertionError('ambiguous commit reported success')
            assert snapshot(private)==published
            assert lookup(RUN,report,'synthetic')=='identical'
            committed=db.read_bytes()
            try:
                execute(store,managed,index,expected)
            except ReviewRequired:
                pass
            else:
                raise AssertionError('stale plan retried committed index')
            assert db.read_bytes()==committed
        else:
            crash('after_insert')
        assert snapshot(private)==published and lookup(RUN,report,'synthetic')=='identical'
        assert plan(store,managed,index) is None
        indexed_bytes=db.read_bytes()
        if not lost_response:
            before=observe(store,managed,lookup)
            assert before.action=='append_indexed'
            crash('indexed_publish')
            state=store.inspect()
            assert state.latest==before.snapshot and state.pending==before.snapshot and state.alias
            assert all((private/n).read_bytes()==b for n,b in published.items())
            assert db.read_bytes()==indexed_bytes and final.read_bytes()==source.read_bytes()
            assert plan(store,managed,index) is None
            assert observe(store,managed,lookup).action=='retry_journal'
        reconcile(store,managed,lookup,observe(store,managed,lookup))
        assert observe(store,managed,lookup).action=='none'
        assert db.read_bytes()==indexed_bytes and final.read_bytes()==source.read_bytes()
        assert all((private/n).read_bytes()==b for n,b in published.items())
        assert store.inspect().pending is None
        print('Native retry scenario PASS: '+('injected pre-insert outage and lost committed response' if lost_response else 'real process termination after native insert'))


if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='--child':
        child()
    else:
        exe=Path(sys.argv[1]).resolve()
        scenario(exe)
        scenario(exe,lost_response=True)
