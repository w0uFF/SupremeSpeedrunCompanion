"""Synthetic-only recovery coordination. No archive/index mutation or local authority."""
from dataclasses import dataclass
from synthetic_admission import Journal
from synthetic_journal_store import Store, ReviewRequired


@dataclass(frozen=True)
class Decision:
    action: str
    snapshot: Journal | None = None


def decide(state, status):
    """Consume a verified single-run archive/index inspection, not raw metadata."""
    j = state.latest
    if j is None or j.phase not in ('verified', 'published', 'indexed'):
        return Decision('review')
    if status['origin'] != 'synthetic' or status['run_id'] != j.context.run_id:
        return Decision('review')
    archive, index, digest = (status[k] for k in ('archive', 'index', 'manifest_sha256'))
    if index not in ('missing', 'identical'):
        return Decision('review')
    if archive == 'absent':
        if j.phase == 'verified' and index == 'missing' and digest is None and state.pending is None:
            return Decision('await_archive')
        return Decision('review')
    if digest != j.digest:
        return Decision('review')
    if archive == 'pending':
        if j.phase == 'verified' and index == 'missing' and state.pending is None:
            return Decision('retry_archive_publication')
        return Decision('review')
    if archive not in ('finalized', 'finalized_with_pending_alias'):
        return Decision('review')
    if j.phase == 'indexed' and index != 'identical':
        return Decision('review')
    if archive == 'finalized_with_pending_alias':
        return Decision('retry_archive_alias_cleanup') if state.pending is None else Decision('review')
    if state.pending is not None:
        p = state.pending
        if p.phase == 'published' and (state.alias or j.phase == 'verified'):
            expected = j if state.alias else j.published(j.digest)
        elif p.phase == 'indexed' and index == 'identical' and (state.alias or j.phase == 'published'):
            expected = j if state.alias else j.indexed(j.digest, 'identical')
        else:
            return Decision('review')
        return Decision('retry_journal', p) if p == expected else Decision('review')
    if j.phase == 'verified':
        return Decision('append_published', j.published(j.digest))
    if j.phase == 'published':
        if index == 'identical':
            return Decision('append_indexed', j.indexed(j.digest, 'identical'))
        return Decision('retry_index')
    return Decision('none')


def observe(store: Store, directory, lookup):
    """Inspect real archive bytes via existing verifier and injected read-only index port."""
    from archive_status import inspect
    try:
        state = store.inspect()
        j = state.latest
        if j is None or j.phase not in ('verified', 'published', 'indexed'):
            return Decision('review')

        def guarded(run_id, report, origin):
            if report is not None:
                m = report['manifest']
                if (report['manifest_sha256'] != j.digest or m['attempt_status'] != j.terminal
                        or (j.capture_complete and m['capture_status'] != 'finalized')):
                    return 'conflict'
            return lookup(run_id, report, origin)

        return decide(state, inspect(directory, store.run_id, 'synthetic', guarded))
    except (OSError, ValueError):
        return Decision('review')


def reconcile(store: Store, directory, lookup, expected: Decision, fault=lambda _: None):
    """Explicit one-step journal-only mutation; re-inspect under single-owner contract."""
    current = observe(store, directory, lookup)
    if current != expected or current.snapshot is None or current.action not in (
            'append_published', 'append_indexed', 'retry_journal'):
        raise ReviewRequired('recovery decision changed or not journal-only')
    return store.append(current.snapshot, fault)
