"""Bind a synthetic fixture index to one existing database and one native bridge.

Filesystem identity detects ordinary path substitution, not hostile races or trust.
No native repository schema change, local admission, or automatic database creation.
"""
from dataclasses import dataclass
from pathlib import Path
import stat
from synthetic_journal_store import ReviewRequired


@dataclass(frozen=True)
class FileIdentity:
    path: Path
    device: int
    inode: int

    @classmethod
    def capture(cls, path):
        source = Path(path)
        info = source.lstat()
        if (not stat.S_ISREG(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400
                or not info.st_ino):
            raise ReviewRequired('regular file with available identity required')
        resolved = source.resolve(strict=True)
        actual = resolved.stat()
        if (actual.st_dev, actual.st_ino) != (info.st_dev, info.st_ino):
            raise ReviewRequired('file identity changed during binding')
        return cls(resolved, info.st_dev, info.st_ino)

    def check(self):
        if FileIdentity.capture(self.path) != self:
            raise ReviewRequired('bound file identity changed')


@dataclass(frozen=True)
class BoundSyntheticIndex:
    database: FileIdentity
    bridge: FileIdentity

    @classmethod
    def bind(cls, database, executable):
        db = FileIdentity.capture(database)
        if db.path.name != 'runs.sqlite':
            raise ReviewRequired('explicit private runs.sqlite path required')
        return cls(db, FileIdentity.capture(executable))

    def check(self):
        self.database.check()
        self.bridge.check()

    def lookup(self, run_id, report, origin):
        if origin != 'synthetic':
            raise ReviewRequired('synthetic index only')
        self.check()
        try:
            return _lookup(self.bridge.path, self.database.path, run_id, report, origin)
        finally:
            self.check()

    def insert(self, report, origin):
        if origin != 'synthetic' or report['manifest']['origin'] != 'synthetic':
            raise ReviewRequired('synthetic index only')
        self.check()
        try:
            return _insert(self.bridge.path, self.database.path, report, origin)
        finally:
            self.check()


def _lookup(executable, database, run_id, report, origin):
    from archive_status import native_status
    return native_status(executable, database, run_id, report, origin)


def _insert(executable, database, report, origin):
    from index_archive import native_index
    return native_index(executable, database, report, origin)
