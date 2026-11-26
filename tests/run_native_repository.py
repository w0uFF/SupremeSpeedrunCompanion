"""Native adapter with independent SQLite inspection in a disposable private directory."""
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'épreuve'
    path.mkdir()
    db = path / 'runs.sqlite'
    def run(action, expected=0):
        r = subprocess.run([sys.argv[1], action, str(db)], capture_output=True, timeout=15)
        assert r.returncode == expected, (action, r.returncode, r.stderr)
        print(r.stdout.decode('ascii', errors='replace').strip())
    run('read', 3)
    assert not db.exists()
    run('create')
    original = db.read_bytes()
    run('create', 3)
    assert db.read_bytes() == original
    run('read')
    assert db.read_bytes() == original
    with sqlite3.connect(db) as conn:
        assert conn.execute('pragma application_id').fetchone()[0] == 0x53534352
        assert conn.execute('pragma user_version').fetchone()[0] == 1
        assert conn.execute('select igt_ms,rta_ns from runs where run_id=?', ('2'*32,)).fetchone() == (None, 0)
        assert conn.execute('select name,favorite from labels').fetchall() == [("Test ' é", 1)]
        assert conn.execute('select count(*) from runs').fetchone()[0] == 7
        assert conn.execute('select status,igt_ms from runs where run_id=?', ('6'*32,)).fetchone() == (1, None)
        try:
            conn.execute('update runs set igt_ms=0 where run_id=?', ('6'*32,))
        except sqlite3.IntegrityError:
            pass
        else:
            raise AssertionError('unfinished final IGT accepted by database')
        conn.execute('pragma user_version=99')
    conn.close()
    unsupported = db.read_bytes()
    run('read', 3)
    assert db.read_bytes() == unsupported
    with sqlite3.connect(db) as conn:
        conn.execute('pragma user_version=1')
        conn.execute('create table unrelated(x)')
    conn.close()
    unsupported = db.read_bytes()
    run('read', 3)
    assert db.read_bytes() == unsupported
print('Native WinSQLite library independently inspected; unsupported version/schema preserved')
