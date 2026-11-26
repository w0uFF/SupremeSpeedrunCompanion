"""Offline profile editor and launcher projection, never writes mods.json."""
import argparse
import json
from ssc_profiles import ProfileStore, edit, initial, read_json, resolve, import_profile, export_profile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('store', help='SSC-owned JSON path in an existing private directory')
    p.add_argument('action', choices=['init', 'show', 'resolve', 'create', 'replace', 'duplicate', 'rename', 'select', 'delete', 'export', 'import'])
    p.add_argument('name', nargs='?')
    p.add_argument('--target')
    p.add_argument('--values', help='JSON file containing a complete settings object')
    p.add_argument('--mods-state', help='Read-only copy of launcher mods.json')
    p.add_argument('--file', help='Single-profile import/export file')
    a = p.parse_args()
    store = ProfileStore(a.store)
    if a.action == 'init':
        store.save(initial(), create=True)
    elif a.action == 'show':
        print(json.dumps(store.load(), indent=2))
    elif a.action == 'resolve':
        if not a.mods_state:
            p.error('--mods-state required')
        print(json.dumps(resolve(store.load(), read_json(a.mods_state)), indent=2))
    elif a.action == 'export':
        if not a.file or not a.name:
            p.error('export requires name and --file')
        export_profile(store.load(), a.name, a.file)
    elif a.action == 'import':
        if not a.file:
            p.error('import requires --file')
        current = store.load()
        store.save(import_profile(current, a.file, target=a.target), expected=current)
    else:
        current = store.load()
        store.save(edit(current, a.action, a.name, target=a.target, values=read_json(a.values) if a.values else None), expected=current)


if __name__ == '__main__':
    main()
