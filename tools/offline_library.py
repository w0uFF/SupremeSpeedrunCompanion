"""Experimental standalone SQLite store, not linked to the game or official evidence schema."""
from dataclasses import dataclass
import re
import sqlite3

MAX_INT = (1 << 63) - 1


def text(value, limit=128):
    if (not isinstance(value, str) or not 1 <= len(value) <= limit
            or any(ord(c) < 32 or ord(c) == 127 for c in value)):
        raise ValueError('invalid text field')
    return value


def identifier(value, size):
    if not isinstance(value, str) or re.fullmatch('[0-9a-f]{%d}' % size, value) is None:
        raise ValueError('invalid hex identifier')
    return value


def time_value(value):
    if value is not None and (type(value) is not int or not 0 <= value <= MAX_INT):
        raise ValueError('invalid time')
    return value


@dataclass(frozen=True)
class Preferences:
    remember: bool
    rider: str | None
    board: str | None
    recording: str

    def validate(self):
        if type(self.remember) is not bool or self.recording not in ('automatic', 'ask', 'training'):
            raise ValueError('invalid preferences')
        if (self.rider is None) != (self.board is None):
            raise ValueError('partial loadout')
        if self.rider is not None:
            text(self.rider)
            text(self.board)


@dataclass(frozen=True)
class Run:
    run_id: str
    category: str
    profile_sha256: str
    event_semantics: str
    origin: str
    status: str
    igt_ms: int | None
    rta_ns: int | None

    def validate(self):
        identifier(self.run_id, 32)
        text(self.category)
        identifier(self.profile_sha256, 64)
        text(self.event_semantics)
        if self.origin not in ('local', 'imported') or self.status not in ('finished', 'abandoned', 'restarted', 'failed'):
            raise ValueError('invalid run classification')
        time_value(self.igt_ms)
        time_value(self.rta_ns)
        if self.status != 'finished' and self.igt_ms is not None:
            raise ValueError('unfinished run has a final IGT')


class Library:
    """Owner-thread storage adapter; caller owns path and closes explicitly."""
    def __init__(self, path):
        self.db = sqlite3.connect(path)
        try:
            self.db.execute('PRAGMA foreign_keys=ON')
            with self.db:
                self.db.execute('BEGIN IMMEDIATE')
                version = self.db.execute('PRAGMA user_version').fetchone()[0]
                if version == 0:
                    if self.db.execute("SELECT 1 FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'").fetchone():
                        raise ValueError('not an empty library')
                    self.db.execute('CREATE TABLE preferences (id INTEGER PRIMARY KEY CHECK(id=1), remember INTEGER NOT NULL, rider TEXT, board TEXT, recording TEXT NOT NULL)')
                    self.db.execute('CREATE TABLE runs (run_id TEXT PRIMARY KEY NOT NULL, category TEXT NOT NULL, profile_sha256 TEXT NOT NULL, event_semantics TEXT NOT NULL, origin TEXT NOT NULL, status TEXT NOT NULL, igt_ms INTEGER, rta_ns INTEGER)')
                    self.db.execute('CREATE TABLE labels (run_id TEXT PRIMARY KEY REFERENCES runs(run_id), name TEXT NOT NULL, favorite INTEGER NOT NULL)')
                    self.db.execute('CREATE INDEX comparable ON runs(category, profile_sha256, event_semantics, origin, status)')
                    self.db.execute('PRAGMA user_version=1')
                elif version != 1:
                    raise ValueError('unsupported library version')
                # Reject missing tables/columns without modifying an existing database.
                for query in ('SELECT id,remember,rider,board,recording FROM preferences LIMIT 0',
                              'SELECT run_id,category,profile_sha256,event_semantics,origin,status,igt_ms,rta_ns FROM runs LIMIT 0',
                              'SELECT run_id,name,favorite FROM labels LIMIT 0'):
                    self.db.execute(query)
        except BaseException:
            self.db.close()
            raise

    def close(self):
        self.db.close()

    def save_preferences(self, prefs):
        prefs.validate()
        with self.db:
            self.db.execute('INSERT OR REPLACE INTO preferences VALUES (1,?,?,?,?)',
                            (int(prefs.remember), prefs.rider, prefs.board, prefs.recording))

    def preferences(self):
        row = self.db.execute('SELECT remember,rider,board,recording FROM preferences WHERE id=1').fetchone()
        if row is None:
            return None
        if row[0] not in (0, 1):
            raise ValueError('invalid stored remember flag')
        prefs = Preferences(bool(row[0]), *row[1:])
        prefs.validate()
        return prefs

    def add_run(self, run):
        run.validate()
        with self.db:
            self.db.execute('INSERT INTO runs VALUES (?,?,?,?,?,?,?,?)',
                            (run.run_id, run.category, run.profile_sha256, run.event_semantics,
                             run.origin, run.status, run.igt_ms, run.rta_ns))

    def get_run(self, run_id):
        identifier(run_id, 32)
        row = self.db.execute('SELECT * FROM runs WHERE run_id=?', (run_id,)).fetchone()
        if row is None:
            return None
        run = Run(*row)
        run.validate()
        return run

    def set_label(self, run_id, name, favorite):
        identifier(run_id, 32)
        text(name, 256)
        if type(favorite) is not bool:
            raise ValueError('invalid favorite')
        with self.db:
            self.db.execute('INSERT OR REPLACE INTO labels VALUES (?,?,?)', (run_id, name, int(favorite)))

    def best_local(self, category, profile_sha256, event_semantics, method):
        """Provisional local PB only; caller specifies comparison policy, not leaderboard eligibility."""
        text(category)
        identifier(profile_sha256, 64)
        text(event_semantics)
        field = {'igt': 'igt_ms', 'rta': 'rta_ns'}.get(method)
        if field is None:
            raise ValueError('unknown timing method')
        query = ('SELECT run_id FROM runs WHERE category=? AND profile_sha256=? AND event_semantics=? '
                 "AND origin='local' AND status='finished' AND " + field + ' IS NOT NULL ORDER BY '
                 + field + ',run_id LIMIT 1')
        row = self.db.execute(query, (category, profile_sha256, event_semantics)).fetchone()
        return self.get_run(row[0]) if row else None

    def history(self, limit=100):
        if type(limit) is not int or not 1 <= limit <= 1000:
            raise ValueError('invalid history limit')
        rows = self.db.execute('SELECT run_id FROM runs ORDER BY rowid DESC LIMIT ?', (limit,)).fetchall()
        return [self.get_run(row[0]) for row in rows]
