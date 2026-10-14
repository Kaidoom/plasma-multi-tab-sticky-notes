#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Exercise a private KWin/Plasma Wayland session; never replace the live desktop."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile
import time
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--plugin-root', type=Path, default=Path('build/bin'))
parser.add_argument('--output', type=Path, default=Path('build/plasma-smoke'))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
output = args.output.resolve()
plugin_root = args.plugin_root.resolve()
assert (plugin_root / 'plasma/applets/dev.kaidoom.tabbednotes.so').is_file()

with tempfile.TemporaryDirectory(prefix='tabbednotes-plasma-') as directory:
    root = Path(directory)
    env = os.environ.copy()
    for kind in ('CONFIG', 'DATA', 'CACHE'):
        path = root / kind.lower()
        path.mkdir()
        env['XDG_' + kind + '_HOME'] = str(path)
    env['QT_PLUGIN_PATH'] = str(plugin_root)
    env['QT_QPA_PLATFORM'] = 'wayland'
    env['KDE_FULL_SESSION'] = 'true'
    (root / 'desktop').mkdir()
    (root / 'config/user-dirs.dirs').write_text('XDG_DESKTOP_DIR="' + str(root / 'desktop') + '"\n')
    socket = 'tabbednotes-test-' + uuid.uuid4().hex[:10]
    script = root / 'session.sh'
    script.write_text('#!/bin/sh\nprintf "%s\\n" "$DBUS_SESSION_BUS_ADDRESS" > '
                      + shlex.quote(str(root / 'bus')) + '\nexec kwin_wayland --virtual --width 1024 --height 768 '
                      '--no-lockscreen --no-global-shortcuts --no-kactivities --socket '
                      + shlex.quote(socket) + ' --exit-with-session /usr/bin/plasmashell\n')
    script.chmod(0o700)

    def call(path, method, *arguments):
        return subprocess.check_output(['gdbus', 'call', '--address', (root / 'bus').read_text().strip(),
                                        '--dest', 'org.kde.plasmashell', '--object-path', path,
                                        '--method', method, *map(str, arguments)], text=True, stderr=subprocess.PIPE, timeout=15)

    def js(code):
        return ast.literal_eval(call('/PlasmaShell', 'org.kde.PlasmaShell.evaluateScript', code))[0]

    def start(log_name):
        with (output / log_name).open('w') as log:
            process = subprocess.Popen(['dbus-run-session', '--', str(script)], env=env, stdout=log, stderr=log, start_new_session=True)
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise RuntimeError('Private Plasma session exited; see ' + str(output / log_name))
                try:
                    if int(js('print(desktops().length)')) > 0:
                        return process
                except (OSError, subprocess.SubprocessError, ValueError):
                    pass
                time.sleep(0.2)
            raise RuntimeError('Private Plasma session did not become ready')
        except BaseException:
            process.terminate()
            process.wait(timeout=15)
            raise

    def stop(process):
        call('/MainApplication', 'org.qtproject.Qt.QCoreApplication.quit')
        process.wait(timeout=30)
        # Session-activated helpers may still be flushing their configuration.
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        time.sleep(0.3)

    process = None
    try:
        process = start('initial.log')
        js('var d=desktops()[0]; var w=d.addWidget("dev.kaidoom.tabbednotes"); '
           'w.geometry=new QRectF(50,50,400,400); '
           'var p=panels()[0] || new Panel; p.addWidget("dev.kaidoom.tabbednotes");')
        time.sleep(1)
        stop(process)
        process = None
        config = root / 'config/plasma-org.kde.plasma.desktop-appletsrc'
        lines = config.read_text().splitlines()
        expected = []
        notes = root / 'data/plasma_tabbed_notes'
        for i, line in enumerate(lines):
            if not line.startswith('tabState='):
                continue
            state = json.loads(line[9:])
            state['tabs'][0]['title'] = 'First'
            (notes / state['tabs'][0]['id']).write_text('<p><b>Restart first Ω</b></p>')
            second = str(uuid.uuid4())
            (notes / second).write_text('<p><i>Restart second 日本語</i> <a href="https://kde.org">KDE</a></p>')
            state['tabs'].append(dict(id=second, title='Second', cursor=5, x=0, y=0))
            state['active'] = second
            state['tabs'].reverse()  # Restore a reordered strip, with selection identified by note ID.
            lines[i] = 'tabState=' + json.dumps(state, ensure_ascii=False, separators=(',', ':'))
            expected.append(state)
        assert len(expected) == 2, 'Desktop and panel applets must both save their configuration'
        config.write_text('\n'.join(lines) + '\n')
        process = start('restarted.log')
        states = json.loads(js('var states=[]; for(var c of desktops().concat(panels())) '
                              'for(var w of c.widgets()) if(w.type==="dev.kaidoom.tabbednotes") '
                              '{w.currentConfigGroup=["General"]; states.push(JSON.parse(w.readConfig("tabState")));} '
                              'print(JSON.stringify(states));'))
        assert sorted(states, key=lambda s: s['owner']) == sorted(expected, key=lambda s: s['owner'])
        time.sleep(0.5)
        call('/PlasmaShell', 'org.kde.PlasmaShell.grabContainmentImage', 'Virtual-0', 1024, 768, output / 'restarted-desktop.png')
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in notes.iterdir()}
        js('for(var c of desktops().concat(panels())) for(var w of c.widgets()) '
           'if(w.type==="dev.kaidoom.tabbednotes") w.remove();')
        stop(process)
        process = None
        after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in notes.iterdir()}
        assert before == after, 'Widget removal must preserve all notes'
        (output / 'result.json').write_text(json.dumps(dict(restored_applets=2, preserved_notes=len(after), states=states), indent=2, ensure_ascii=False))
        print('PASS: desktop and panel plugin discovery, full session restart, tab metadata restoration, and note retention after removal')
    finally:
        if process and process.poll() is None:
            try:
                stop(process)
            except (OSError, subprocess.SubprocessError):
                process.terminate()
                process.wait(timeout=15)
