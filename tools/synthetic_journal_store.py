"""Bounded append-only synthetic journal fixture, never a local-attempt authority.

A trusted single owner supplies an existing private directory. No automatic repair,
archive verification, native repository access, migration, or power-loss guarantee.
"""
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import stat
from typing import Callable

from synthetic_admission import Context, Journal

PHASES = ('recording', 'terminal', 'verified', 'published', 'indexed')
MAX_BYTES = 1024


class ReviewRequired(ValueError):
    """Preserve the evidence; the caller must review the inconsistency."""


def encode(journal: Journal) -> bytes:
    if not isinstance(journal, Journal):
        raise ValueError('synthetic journal required')
    data = dict(schema=1, origin='synthetic', run_id=journal.context.run_id,
                generation=journal.context.generation, settings=journal.context.settings,
                phase=journal.phase, terminal=journal.terminal, digest=journal.digest,
                capture_complete=journal.capture_complete)
    return (json.dumps(data, sort_keys=True, ensure_ascii=True, separators=(',', ':')) + '\n').encode('ascii')


def decode(raw: bytes) -> Journal:
    if not isinstance(raw, bytes) or not 0 < len(raw) <= MAX_BYTES:
        raise ValueError('journal byte limit')
    try:
        data = json.loads(raw.decode('ascii'))
        if (not isinstance(data, dict) or set(data) != {'schema', 'origin', 'run_id', 'generation',
                'settings', 'phase', 'terminal', 'digest', 'capture_complete'} or
                type(data['schema']) is not int or data['schema'] != 1 or data['origin'] != 'synthetic'):
            raise ValueError('unsupported synthetic journal')
        journal = Journal(Context(data['run_id'], data['generation'], data['settings']),
                          data['phase'], data['terminal'], data['digest'], data['capture_complete'])
        # Enforces canonical bytes, rejects duplicate keys, alternate encodings and extra data.
        if encode(journal) != raw:
            raise ValueError('noncanonical synthetic journal')
        return journal
    except (UnicodeError, TypeError, ValueError, RecursionError) as exc:
        raise ValueError('invalid synthetic journal') from exc


def _regular(path: Path) -> bool:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if not stat.S_ISREG(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
        raise ReviewRequired('nonregular journal path')
    return True


def _read(path: Path) -> Journal | None:
    if not _regular(path):
        return None
    try:
        with path.open('rb') as handle:
            return decode(handle.read(MAX_BYTES + 1))
    except (OSError, ValueError) as exc:
        raise ReviewRequired('unreadable or invalid journal snapshot') from exc


def _successor(previous: Journal | None, current: Journal) -> bool:
    try:
        if previous is None:
            return current == Journal(current.context)
        if previous.phase == 'recording':
            return current == previous.finish(current.context, current.terminal, current.capture_complete)
        if previous.phase == 'terminal':
            return current == previous.verify(current.context, current.terminal, 'synthetic',
                                              current.digest, current.capture_complete)
        if previous.phase == 'verified':
            return current == previous.published(current.digest)
        if previous.phase == 'published':
            return current == previous.indexed(current.digest, 'identical')
    except ValueError:
        return False
    return False


@dataclass(frozen=True)
class Inspection:
    latest: Journal | None
    pending: Journal | None = None
    alias: bool = False


class Store:
    """Only fixed paths for a caller-selected synthetic run; no directory scan."""
    def __init__(self, directory: Path, run_id: str):
        self.directory = Path(directory)
        if not isinstance(run_id, str) or re.fullmatch('[0-9a-f]{32}', run_id) is None:
            raise ValueError('synthetic run ID required')
        info = self.directory.lstat()
        if not stat.S_ISDIR(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('existing private directory required')
        self.run_id = run_id

    def _paths(self, phase):
        number = PHASES.index(phase)
        final = self.directory / f'synthetic-{self.run_id}-{number}-{phase}.json'
        return final, final.with_name(final.name + '.partial')

    def inspect(self) -> Inspection:
        latest = pending = None
        alias = False
        for phase in PHASES:
            final, partial = self._paths(phase)
            committed, staged = _read(final), _read(partial)
            for candidate in (committed, staged):
                if candidate is not None and (candidate.context.run_id != self.run_id or candidate.phase != phase):
                    raise ReviewRequired('journal filename identity mismatch')
            if committed is not None:
                if pending is not None or not _successor(latest, committed):
                    raise ReviewRequired('broken journal phase chain')
                latest = committed
                if staged is not None:
                    if committed != staged or not os.path.samefile(final, partial):
                        raise ReviewRequired('separate pending conflict')
                    pending, alias = staged, True
            elif staged is not None:
                if pending is not None or not _successor(latest, staged):
                    raise ReviewRequired('invalid pending sequence')
                pending = staged
        return Inspection(latest, pending, alias)

    def append(self, desired: Journal, fault: Callable[[str], None] = lambda _: None) -> str:
        """Explicit append/retry only; injected fault points leave owned staging for review."""
        if not isinstance(desired, Journal) or desired.context.run_id != self.run_id:
            raise ValueError('journal identity mismatch')
        state = self.inspect()
        final, partial = self._paths(desired.phase)
        if state.pending is not None:
            if desired != state.pending:
                raise ReviewRequired('different pending journal')
            if state.alias:
                # Exactly the same-file alias of the latest verified snapshot, not another copy.
                partial.unlink()
                return 'identical'
        elif state.latest == desired:
            return 'identical'
        elif not _successor(state.latest, desired):
            raise ReviewRequired('invalid journal transition')
        else:
            payload = encode(desired)
            with partial.open('xb') as handle:
                split = len(payload) // 2
                handle.write(payload[:split])
                handle.flush()
                fault('partial_write')
                handle.write(payload[split:])
                handle.flush()
                os.fsync(handle.fileno())
            fault('after_flush')
        if _read(partial) != desired:
            raise ReviewRequired('pending bytes mismatch')
        # Flush an existing exact pending snapshot again before no-replace publication.
        with partial.open('r+b') as handle:
            os.fsync(handle.fileno())
        os.link(partial, final)
        fault('after_publish')
        partial.unlink()
        return 'appended'
