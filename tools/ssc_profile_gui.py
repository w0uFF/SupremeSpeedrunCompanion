"""Offline SSC-only profile editor prototype. Never opens the game or writes mods.json."""
import argparse
from pathlib import Path
import tempfile
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

import ssc_profiles as model

GROUPS = {
    'Recording': ('auto_record', 'game_audio', 'resolution', 'fps', 'bitrate_mbps', 'encoder'),
    'Overlay and player': ('input_overlay', 'igt_overlay', 'rta_overlay', 'overlay_opacity', 'remember_loadout'),
    'Storage and hotkeys': ('package_directory', 'retention', 'min_free_gib', 'auto_package',
                            'restart_key', 'record_key', 'overlay_key'),
}


class Editor:
    def __init__(self, root, store):
        self.root, self.store = root, store
        self.book = store.load()
        self.loaded = self.book['active']
        self.selected = tk.StringVar(root, self.loaded)
        self.status = tk.StringVar(root, 'Offline development prototype; no game effects.')
        self.active = tk.StringVar(root)
        self.values = {}
        root.title('SSC — Profile Editor (offline prototype)')
        outer = ttk.Frame(root, padding=16)
        outer.pack(fill='both', expand=True)
        ttk.Label(outer, text='Supreme Speedrun Companion', font=('TkDefaultFont', 15, 'bold')).pack(anchor='w')
        ttk.Label(outer, textvariable=self.active).pack(anchor='w', pady=(4, 12))
        top = ttk.Frame(outer)
        top.pack(fill='x')
        self.combo = ttk.Combobox(top, textvariable=self.selected, state='readonly', width=28)
        self.combo.pack(side='left', padx=(0, 8))
        self.combo.bind('<<ComboboxSelected>>', self.switch)
        for label, action in (('New', 'create'), ('Duplicate', 'duplicate'), ('Rename', 'rename'), ('Delete', 'delete')):
            ttk.Button(top, text=label, command=lambda a=action: self.safe(lambda: self.manage(a))).pack(side='left', padx=2)
        tabs = ttk.Notebook(outer)
        tabs.pack(fill='both', expand=True, pady=12)
        for title, fields in GROUPS.items():
            page = ttk.Frame(tabs, padding=12)
            page.columnconfigure(1, weight=1)
            tabs.add(page, text=title)
            for row, key in enumerate(fields):
                kind, default, allowed = model.FIELDS[key]
                label = key.replace('_', ' ').capitalize()
                var = tk.BooleanVar(root, default) if kind == 'boolean' else tk.StringVar(root, str(default))
                self.values[key] = var
                ttk.Label(page, text=label).grid(row=row, column=0, sticky='w', padx=(0, 12), pady=5)
                if kind == 'boolean':
                    widget = ttk.Checkbutton(page, variable=var)
                elif kind == 'dropdown':
                    widget = ttk.Combobox(page, textvariable=var, values=allowed, state='readonly')
                elif kind == 'number':
                    widget = ttk.Spinbox(page, textvariable=var, from_=allowed[0], to=allowed[1], increment=1)
                else:
                    widget = ttk.Entry(page, textvariable=var)
                    ttk.Button(page, text='Browse...', command=lambda: self.safe(self.browse)).grid(row=row, column=2, padx=(6, 0))
                widget.grid(row=row, column=1, sticky='ew', pady=5)
        bottom = ttk.Frame(outer)
        bottom.pack(fill='x')
        ttk.Button(bottom, text='Save changes', command=lambda: self.safe(self.save)).pack(side='left')
        ttk.Button(bottom, text='Set active profile', command=lambda: self.safe(self.activate)).pack(side='left', padx=6)
        ttk.Button(bottom, text='Import profile', command=lambda: self.safe(self.import_one)).pack(side='left', padx=6)
        ttk.Button(bottom, text='Export profile', command=lambda: self.safe(self.export_one)).pack(side='left')
        ttk.Label(outer, textvariable=self.status, wraplength=580).pack(fill='x', pady=(12, 0))
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.display(self.loaded)

    def display(self, name):
        self.loaded = name
        self.selected.set(name)
        self.combo.configure(values=tuple(self.book['profiles']))
        self.active.set('Active profile: ' + self.book['active'])
        for key, value in self.book['profiles'][name].items():
            self.values[key].set(value)

    def collect(self):
        values = {key: var.get() for key, var in self.values.items()}
        for key, (kind, _, _) in model.FIELDS.items():
            if kind == 'number':
                values[key] = int(values[key])
        return model.settings(values)

    def dirty(self):
        try:
            return self.collect() != self.book['profiles'][self.loaded]
        except (ValueError, tk.TclError):
            return True

    def permit_discard(self):
        return not self.dirty() or messagebox.askyesno('Unsaved changes', 'Discard unsaved changes?', parent=self.root)

    def switch(self, _event=None):
        name = self.selected.get()
        if self.permit_discard():
            self.display(name)
        else:
            self.selected.set(self.loaded)

    def apply(self, book, name):
        self.store.save(book, expected=self.book)
        self.book = self.store.load()
        self.display(name)
        self.status.set('Saved locally. Launcher and game configuration were not changed.')

    def save(self):
        self.apply(model.edit(self.book, 'replace', self.loaded, values=self.collect()), self.loaded)

    def activate(self):
        if self.dirty():
            raise ValueError('Save or discard edits before changing the active profile.')
        self.apply(model.edit(self.book, 'select', self.loaded), self.loaded)

    def manage(self, action):
        if not self.permit_discard():
            return
        if action == 'delete':
            if messagebox.askyesno('Delete profile', 'Delete the selected settings profile? No recordings are removed.', parent=self.root):
                self.apply(model.edit(self.book, 'delete', self.loaded), self.book['active'])
            return
        name = simpledialog.askstring('Profile name', 'New profile name:', parent=self.root)
        if name is None:
            return
        if action == 'create':
            book = model.edit(self.book, action, name, values=model.defaults())
        else:
            book = model.edit(self.book, action, self.loaded, target=name)
        self.apply(book, name)

    def export_one(self):
        if self.dirty():
            raise ValueError('Save or discard edits before exporting.')
        destination = filedialog.asksaveasfilename(parent=self.root, defaultextension='.json',
                                                   filetypes=[('SSC profile', '*.json')])
        if destination:
            model.export_profile(self.book, self.loaded, destination)
            self.status.set('Profile exported without the local package directory. Existing files are never replaced.')

    def import_one(self):
        if not self.permit_discard():
            return
        source = filedialog.askopenfilename(parent=self.root, filetypes=[('SSC profile', '*.json')])
        if source:
            target = simpledialog.askstring('Import profile', 'New profile name:', parent=self.root)
            if target is not None:
                self.apply(model.import_profile(self.book, source, target=target), target)

    def browse(self):
        path = filedialog.askdirectory(parent=self.root, mustexist=True)
        if path:
            self.values['package_directory'].set(model.validate_directory(path))

    def safe(self, action):
        try:
            action()
        except (OSError, ValueError, tk.TclError) as error:
            messagebox.showerror('SSC settings', str(error), parent=self.root)

    def close(self):
        if self.permit_discard():
            self.root.destroy()


def smoke():
    with tempfile.TemporaryDirectory() as directory:
        store = model.ProfileStore(Path(directory) / 'profiles.json')
        store.save(model.initial(), create=True)
        root = tk.Tk()
        root.withdraw()
        try:
            editor = Editor(root, store)
            assert len(editor.values) == 18
            editor.values['fps'].set('60')
            editor.values['auto_record'].set(True)
            assert editor.dirty()
            editor.save()
            assert not editor.dirty() and store.load()['profiles']['Training']['auto_record']
            editor.apply(model.edit(editor.book, 'duplicate', 'Training', target='Evening'), 'Evening')
            editor.activate()
            assert store.load()['active'] == 'Evening'
            root.update_idletasks()
        finally:
            root.destroy()
    print('Offline profile GUI widget creation and save/duplicate/activate smoke passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('store', nargs='?', type=Path)
    parser.add_argument('--smoke', action='store_true')
    args = parser.parse_args()
    if args.smoke:
        if args.store:
            parser.error('--smoke uses its own private store')
        smoke()
        return
    if not args.store:
        parser.error('existing SSC profile store required; initialize with ssc_profile_cli.py')
    root = tk.Tk()
    Editor(root, model.ProfileStore(args.store))
    root.mainloop()


if __name__ == '__main__':
    main()
