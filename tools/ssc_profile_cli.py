"""Offline profile editor and launcher projection, never writes mods.json."""
import argparse
import json
from ssc_profiles import ProfileStore, edit, initial, read_json, resolve


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('store', help='SSC-owned JSON path in an existing private directory')
    p.add_argument('action', choices=['init', 'show', 'resolve', 'create', 'replace', 'duplicate', 'rename', 'select', 'delete'])
    p.add_argument('name', nargs='?')
    p.add_argument('--target')
    p.add_argument('--values', help='JSON file containing a complete settings object')
    p.add_argument('--mods-state', help='Read-only copy of launcher mods.json')
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
    else:
        store.save(edit(store.load(), a.action, a.name, target=a.target, values=read_json(a.values) if a.values else None))


if __name__ == '__main__':
    main()
