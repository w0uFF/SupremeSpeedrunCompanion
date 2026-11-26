"""Private synthetic-only admission state model; no filesystem, native bridge or local authority."""
from dataclasses import dataclass, replace
import re


def _hex(value, length):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{'+str(length)+r'}', value) is not None


@dataclass(frozen=True)
class Context:
    run_id: str
    generation: int
    settings: str

    def __post_init__(self):
        if not _hex(self.run_id, 32) or type(self.generation) is not int or not 0 < self.generation < 2**63 or not _hex(self.settings, 64):
            raise ValueError('invalid synthetic context')


@dataclass(frozen=True)
class Journal:
    context: Context
    phase: str = 'recording'
    terminal: str | None = None
    digest: str | None = None
    capture_complete: bool | None = None

    def __post_init__(self):
        if not isinstance(self.context, Context):
            raise ValueError('missing synthetic context')
        if self.phase == 'recording' and self.terminal is None and self.digest is None and self.capture_complete is None:
            return
        if self.terminal not in ('finished', 'restarted', 'abandoned', 'failed') or type(self.capture_complete) is not bool:
            raise ValueError('invalid terminal')
        if self.phase == 'terminal' and self.digest is None:
            return
        if self.phase in ('verified', 'published', 'indexed') and _hex(self.digest, 64):
            return
        raise ValueError('invalid synthetic journal phase')

    def finish(self, context, terminal, capture_complete):
        if context != self.context or self.phase != 'recording':
            raise ValueError('stale or duplicate terminal')
        return replace(self, phase='terminal', terminal=terminal, capture_complete=capture_complete)

    def verify(self, context, terminal, origin, digest, capture_complete):
        if self.phase != 'terminal' or context != self.context or terminal != self.terminal or origin != 'synthetic' or type(capture_complete) is not bool or capture_complete != self.capture_complete:
            raise ValueError('synthetic evidence mismatch')
        return replace(self, phase='verified', digest=digest)

    def published(self, digest):
        if self.phase not in ('verified', 'published') or digest != self.digest:
            raise ValueError('publication mismatch')
        return replace(self, phase='published')

    def indexed(self, digest, result):
        if self.phase not in ('published', 'indexed') or digest != self.digest or result not in ('inserted', 'identical'):
            raise ValueError('index mismatch')
        return replace(self, phase='indexed')


def recovery(journal, pending, final, index):
    """Pure decision only. Inputs are verified digests, None for confirmed absence, or invalid/unknown status; no repair."""
    if not isinstance(journal, Journal) or journal.phase not in ('verified', 'published', 'indexed'):
        return 'review'
    digest = journal.digest
    if index not in ('missing', 'identical') or any(x is not None and x != digest for x in (pending, final)):
        return 'review'
    if pending is not None and final is not None:  # alias status not proven here
        return 'review'
    if final == digest:
        if journal.phase == 'indexed' and index != 'identical':
            return 'review'
        return ('none' if journal.phase == 'indexed' else 'reconcile_synthetic_journal') if index == 'identical' else 'retry_synthetic_index'
    if pending == digest and journal.phase == 'verified' and index == 'missing':
        return 'retry_synthetic_publication'
    return 'review'
