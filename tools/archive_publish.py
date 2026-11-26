"""Publish a verified partial archive without replacing any destination."""
import os
from pathlib import Path


def publish(final, verify):
    final = Path(final)
    if final.suffix.lower() != '.zip':
        raise ValueError('expected ZIP destination')
    partial = final.with_name(final.name + '.partial')
    # Caller owns a trusted, quiescent directory. No competing writers.
    report = verify(partial)
    with partial.open('r+b') as handle:
        os.fsync(handle.fileno())
    try:
        os.link(partial, final)  # Atomic no-replace on supporting same-volume filesystems.
    except FileExistsError:
        if final.is_symlink() or not final.is_file() or not os.path.samefile(partial, final):
            raise
        verify(final)  # Recovery after link succeeded but partial was not removed.
    partial.unlink()
    return report


def create_run(root, selected, final, metadata):
    from evidence_archive import create, verify
    final = Path(final)
    if final.suffix.lower() != '.zip' or os.path.lexists(final):
        raise ValueError('destination already exists or is not ZIP')
    partial = final.with_name(final.name + '.partial')
    create(root, selected, partial, metadata=metadata)
    return publish(final, verify)
