"""Offline guarded archive port preparation; no native/UI binding or extraction."""
from dataclasses import dataclass
import os
from pathlib import Path
import re
import stat
import zipfile

import archive_status
import index_archive as ia
from archive_publish import publish
from baseline_snapshot import linked


@dataclass(frozen=True)
class Identity:
    run_id: str
    origin: str
    projection: bytes
    container: tuple


@dataclass(frozen=True)
class Slot:
    state: str
    identity: Identity | None = None
    index: str | None = None


def valid_id(run_id, origin):
    if (type(run_id) is not str or re.fullmatch('[0-9a-f]{32}', run_id) is None
            or origin not in ('imported', 'synthetic')):
        raise ValueError('explicit nonlocal run identity required')


def valid_identity(expected):
    if type(expected) is not Identity:
        raise ValueError('archive identity required')
    valid_id(expected.run_id, expected.origin)
    if not ia.validate_guard(expected.projection, expected.container):
        raise ValueError('explicit identity required')
    # No second projection parser: exact equality to the fresh checked identity
    # is required by the effect methods before any archive operation.


def path_shape(value):
    path = Path(value)
    if not path.is_absolute() or '..' in path.parts or len(str(path)) > 1024:
        raise ValueError('absolute bounded private path required')
    if os.name == 'nt':
        if re.fullmatch('[A-Za-z]:', path.drive) is None:
            raise ValueError('local drive path required')
        for part in path.parts[1:]:
            if (part.endswith((' ', '.')) or any(ord(c) < 32 or c in '<>:"|?*' for c in part)
                    or re.fullmatch(r'(?i:CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])',
                                    part.split('.')[0].rstrip(' '))):
                raise ValueError('unsafe private path component')
    return path


def real_directory(path):
    for part in (*reversed(path.parents), path):
        if linked(part) or not stat.S_ISDIR(part.lstat().st_mode):
            raise ValueError('linked or unsupported directory')


def file_info(path):
    real_directory(path.parent)
    try:
        info = path.lstat()
    except FileNotFoundError:
        return None
    if linked(path) or not stat.S_ISREG(info.st_mode):
        raise ValueError('linked or unsupported file')
    return info


def same_path(a, b):
    if os.path.normcase(str(a)) == os.path.normcase(str(b)):
        return True
    try:
        return os.path.samefile(a, b)
    except FileNotFoundError:
        return False


def pair_paths(final):
    return final, final.with_name(final.name + '.partial')


def observe_identity(path, origin):
    report, projection, container = ia.checked_identity(path, origin)
    return Identity(report['run_id'], origin, projection, container), report


class Adapter:
    """One serialized I/O owner; caller supplies trusted quiescent directories and
    paired read/insert callbacks to the SAME nonlocal index. Paths are resolved by
    the trusted composition root, not from manifests or arbitrary UI tokens.
    No hostile path-race, cross-process locking or crash-durability guarantee.
    """
    def __init__(self, directory, lookup, insert, *, protected=()):
        self.directory = path_shape(directory)
        if self.directory == self.directory.parent:
            raise ValueError('private managed subdirectory required')
        real_directory(self.directory)
        if not callable(lookup) or not callable(insert) or len(protected) > 64:
            raise ValueError('bound index and protected path list required')
        self.lookup = lookup
        self.insert = insert
        self.protected = tuple(path_shape(p) for p in protected)

    def _not_protected(self, path):
        if any(same_path(path, p) for p in self.protected):
            raise ValueError('protected path')

    def _external(self, value):
        path = path_shape(value)
        if path.suffix.lower() != '.zip':
            raise ValueError('explicit external ZIP path required')
        real_directory(path.parent)
        real_directory(self.directory)
        if any(same_path(parent, self.directory) for parent in path.parents):
            raise ValueError('external path overlaps managed archive directory')
        self._not_protected(path)
        return path

    def _managed_paths(self, run_id, origin):
        valid_id(run_id, origin)
        real_directory(self.directory)
        pair = pair_paths(self.directory / (origin + '-' + run_id + '.zip'))
        for path in pair:
            self._not_protected(path)
        return pair

    def _pair(self, final, pending, *, allow_alias):
        self._not_protected(final)
        self._not_protected(pending)
        a, b = file_info(final), file_info(pending)
        alias = a is not None and b is not None and same_path(final, pending)
        if alias:
            if not allow_alias or a.st_nlink != 2 or b.st_nlink != 2:
                raise ValueError('unsupported archive alias')
        elif any(x is not None and x.st_nlink != 1 for x in (a, b)):
            raise ValueError('unexpected archive hardlink')
        return a is not None, b is not None, alias

    def _lookup(self, run_id, report, origin):
        answer = self.lookup(run_id, report, origin)
        allowed = ('missing', 'identical', 'conflict') if report is not None else ('missing', 'present')
        if answer not in allowed:
            raise RuntimeError('bound index unavailable or malformed')
        return answer

    def _source(self, path, expected=None):
        path = self._external(path)
        info = file_info(path)
        if info is None:
            raise FileNotFoundError('selected archive missing')
        if info.st_nlink != 1:
            raise ValueError('selected archive aliases another file')
        identity, report = observe_identity(path, 'imported')
        if expected is not None and identity != expected:
            raise ValueError('selected archive changed')
        return path, identity, report

    def selected(self, path):
        try:
            _, identity, _ = self._source(path)
            return Slot('finalized', identity)
        except FileNotFoundError:
            return Slot('absent')
        except (ValueError, zipfile.BadZipFile, EOFError, RecursionError, TypeError):
            return Slot('review')
        except Exception:
            return Slot('unavailable')

    def managed(self, run_id, origin):
        try:
            final, pending = self._managed_paths(run_id, origin)
            a, b, alias = self._pair(final, pending, allow_alias=True)
            seen = []

            def lookup(i, report, o):
                seen.append(ia.project(report, o) if report is not None else None)
                return self._lookup(i, report, o)

            status = archive_status.inspect(self.directory, run_id, origin, lookup)
            if status['index'] == 'unavailable':
                return Slot('unavailable')
            expected_state = ('alias' if alias else 'review' if a and b else
                              'finalized' if a else 'pending' if b else 'absent')
            states = {'absent': 'absent', 'pending': 'pending', 'finalized': 'finalized',
                      'finalized_with_pending_alias': 'alias'}
            state = states.get(status['archive'], 'review')
            if state != expected_state or state == 'review' or len(seen) != 1:
                return Slot('review')
            if state == 'absent':
                return Slot(state, index=status['index'])
            identity, _ = observe_identity(final if a else pending, origin)
            if (identity.run_id != run_id or identity.projection != seen[0]
                    or (alias and observe_identity(pending, origin)[0] != identity)):
                return Slot('review')
            return Slot(state, identity, status['index'])
        except (ValueError, zipfile.BadZipFile, EOFError, RecursionError, TypeError):
            return Slot('review')
        except Exception:
            return Slot('unavailable')

    def destination(self, value, origin):
        try:
            if origin not in ('imported', 'synthetic'):
                raise ValueError('nonlocal destination required')
            final, pending = pair_paths(self._external(value))
            a, b, _ = self._pair(final, pending, allow_alias=False)
            if a and b:
                return Slot('review')
            if not a and not b:
                return Slot('absent')
            return Slot('finalized' if a else 'pending', observe_identity(final if a else pending, origin)[0])
        except (ValueError, zipfile.BadZipFile, EOFError, RecursionError, TypeError):
            return Slot('review')
        except Exception:
            return Slot('unavailable')

    def admit_import(self, source, expected, expected_slot):
        """Explicit guarded operation. Exceptions are uncertain to the controller;
        do fresh readback, never retry automatically. No local origin admitted.
        """
        valid_identity(expected)
        if expected.origin != 'imported':
            raise ValueError('ordinary import only')
        source, _, report = self._source(source, expected)
        current = self.managed(expected.run_id, expected.origin)
        row = self._lookup(expected.run_id, report, expected.origin)
        if (current != expected_slot or current.state not in ('absent', 'pending', 'finalized', 'alias')
                or (current.identity is not None and current.identity != expected)
                or row not in ('missing', 'identical')
                or current.index != ('missing' if row == 'missing' else 'present' if current.state == 'absent' else 'identical')):
            raise ValueError('managed slot or bound index conflict')
        if row == 'identical':
            if current.state == 'finalized':
                return 'unchanged'
            if current.state != 'alias':
                raise ValueError('indexed archive missing or pending; review required')
        # Last preflight on the same serialized owner, before shared admission.
        final, _ = self._managed_paths(expected.run_id, expected.origin)
        self._source(source, expected)
        if self.managed(expected.run_id, expected.origin) != current or self._lookup(expected.run_id, report, expected.origin) != row:
            raise ValueError('managed slot or index changed')

        def guarded_index(verified, origin):
            self._source(source, expected)
            if ia.project(verified, origin) != expected.projection:
                raise ValueError('index projection changed')
            if self.managed(expected.run_id, origin) != Slot('finalized', expected, row):
                raise ValueError('managed archive changed before insertion')
            ia.guarded_checked(final, origin, expected.projection, expected.container)
            now = self._lookup(expected.run_id, verified, origin)
            if now != row:
                raise ValueError('index changed before insertion')
            if now == 'identical':
                return 'identical'
            answer = self.insert(verified, origin)
            if answer not in ('inserted', 'identical') or self._lookup(expected.run_id, verified, origin) != 'identical':
                raise RuntimeError('index insertion not confirmed')
            return answer

        result = ia.admit(source, self.directory, 'imported', guarded_index,
                          expected_projection=expected.projection, expected_container=expected.container)
        if result['index'] not in ('inserted', 'identical') or self.managed(expected.run_id, 'imported') != Slot('finalized', expected, 'identical'):
            raise RuntimeError('import readback not confirmed')
        self._source(source, expected)
        return 'completed'

    def publish_exact(self, expected, destination, expected_slot):
        """Exact bytes only; no overwrite or export alias cleanup, no index writes."""
        valid_identity(expected)
        final, pending = pair_paths(self._external(destination))
        managed, _ = self._managed_paths(expected.run_id, expected.origin)

        def source_guard():
            if self.managed(expected.run_id, expected.origin) != Slot('finalized', expected, 'identical'):
                raise ValueError('managed source or index changed')
            return ia.guarded_checked(managed, expected.origin, expected.projection, expected.container)

        source_guard()
        current = self.destination(final, expected.origin)
        if (current != expected_slot or current.state not in ('absent', 'pending', 'finalized')
                or (current.identity is not None and current.identity != expected)):
            raise ValueError('export destination conflict or review required')
        if current.state == 'finalized':
            return 'unchanged'
        source_guard()
        if self.destination(final, expected.origin) != current:
            raise ValueError('export destination changed')
        if current.state == 'absent':
            ia.copy_new(managed, pending)

        def verify_pending(path):
            source_guard()
            if path != pending or self.destination(final, expected.origin) != Slot('pending', expected):
                raise ValueError('export pending changed or alias requires review')
            return ia.guarded_checked(path, expected.origin, expected.projection, expected.container)

        verify_pending(pending)
        publish(final, verify_pending)
        source_guard()
        if self.destination(final, expected.origin) != Slot('finalized', expected):
            raise RuntimeError('export readback not confirmed')
        return 'completed'
