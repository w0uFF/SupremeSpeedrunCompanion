from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from synthetic_index_port import BoundSyntheticIndex, FileIdentity
from synthetic_journal_store import ReviewRequired


class BoundIndexTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.db=self.root/'runs.sqlite'
        self.exe=self.root/'ssc_index.exe'
        self.db.write_bytes(b'fixture')
        self.exe.write_bytes(b'bridge')
        self.port=BoundSyntheticIndex.bind(self.db,self.exe)
        self.report={'manifest':{'origin':'synthetic'}}

    def test_same_bound_paths_and_inplace_change(self):
        with patch('synthetic_index_port._lookup',return_value='missing') as lookup:
            self.assertEqual(self.port.lookup('a'*32,None,'synthetic'),'missing')
            lookup.assert_called_once_with(self.exe.resolve(),self.db.resolve(),'a'*32,None,'synthetic')
        def insert(exe,db,report,origin):
            self.assertEqual((exe,db),(self.exe.resolve(),self.db.resolve()))
            db.write_bytes(b'updated fixture')
            return 'inserted'
        with patch('synthetic_index_port._insert',side_effect=insert):
            self.assertEqual(self.port.insert(self.report,'synthetic'),'inserted')
        self.port.check()

    def test_origin_refusal_precedes_backend(self):
        with patch('synthetic_index_port._lookup') as lookup, patch('synthetic_index_port._insert') as insert:
            for origin in ('local','imported'):
                with self.assertRaises(ReviewRequired): self.port.lookup('a'*32,None,origin)
                with self.assertRaises(ReviewRequired): self.port.insert(self.report,origin)
            with self.assertRaises(ReviewRequired): self.port.insert({'manifest':{'origin':'imported'}},'synthetic')
            lookup.assert_not_called()
            insert.assert_not_called()

    def test_bridge_or_db_replacement_is_refused(self):
        for file in (self.exe,self.db):
            backup=file.with_suffix('.old')
            file.rename(backup)
            file.write_bytes(backup.read_bytes())
            with patch('synthetic_index_port._lookup') as lookup:
                with self.assertRaises(ReviewRequired): self.port.lookup('a'*32,None,'synthetic')
                lookup.assert_not_called()
            file.unlink()
            backup.rename(file)
        self.port.check()

    def test_post_operation_replacement_reports_review(self):
        def replace(*args):
            self.db.rename(self.root/'old-db')
            self.db.write_bytes(b'new')
            return 'missing'
        with patch('synthetic_index_port._lookup',side_effect=replace):
            with self.assertRaises(ReviewRequired): self.port.lookup('a'*32,None,'synthetic')
        self.assertEqual(self.db.read_bytes(),b'new')
        self.assertEqual((self.root/'old-db').read_bytes(),b'fixture')

    def test_bad_binding(self):
        wrong=self.root/'wrong.sqlite'
        wrong.write_bytes(b'fixture')
        with self.assertRaises(ReviewRequired): BoundSyntheticIndex.bind(wrong,self.exe)
        with self.assertRaises(ReviewRequired): FileIdentity.capture(self.root)
        with self.assertRaises(OSError): FileIdentity.capture(self.root/'missing')


if __name__=='__main__': unittest.main()
