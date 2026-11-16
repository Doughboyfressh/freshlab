#!/usr/bin/env python3
"""Validate native VST3s with pinned pluginval; optionally install/test Mac AUs."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import plistlib
import time
import shutil
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = {
    'Windows': ('Windows', 'c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab'),
    'Darwin': ('macOS', '3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f'),
    'Linux': ('Linux', 'c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352'),
}


def validator(build):
    system = platform.system()
    label, expected = ASSETS[system]
    folder = build / 'validation-tools'
    folder.mkdir(parents=True, exist_ok=True)
    archive = folder / f'pluginval-{label}-1.0.4.zip'
    if not archive.exists():
        url = f'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_{label}.zip'
        with urllib.request.urlopen(url, timeout=120) as response:
            archive.write_bytes(response.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise RuntimeError('pluginval archive checksum mismatch')
    destination = folder / label
    destination.mkdir(exist_ok=True)
    if system == 'Darwin':
        subprocess.run(['ditto', '-x', '-k', str(archive), str(destination)], check=True)
    else:
        with zipfile.ZipFile(archive) as source:
            source.extractall(destination)
    name = 'pluginval.exe' if system == 'Windows' else 'pluginval'
    executable = next(p for p in destination.rglob(name) if p.is_file())
    if system != 'Windows':
        executable.chmod(executable.stat().st_mode | 0o111)
    return executable


def run_logged(command, log, timeout=600):
    with log.open('w', encoding='utf-8') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        print('\n'.join(log.read_text(encoding='utf-8', errors='replace').splitlines()[-50:]))
        return False
    print(f'PASS: {log.name}')
    return True


def refresh_au_registry():
    # VST3 validation may have primed the registrar before these AUs existed.
    # Restart only the current user's registrar; launchd starts it on demand.
    subprocess.run(['/usr/bin/killall', 'AudioComponentRegistrar'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)


def validate_audio_units(build, reports):
    if platform.system() != 'Darwin':
        raise RuntimeError('--auval requires macOS')
    components = Path.home() / 'Library/Audio/Plug-Ins/Components'
    components.mkdir(parents=True, exist_ok=True)
    plans = []
    for target, product, kind, code in [('FreshLab', 'FreshLab', 'aumu', 'Frsi'),
                                       ('FreshLabFX', 'FreshLab FX', 'aufx', 'Frfx')]:
        source = build / f'{target}_artefacts/Release/AU/{product}.component'
        destination = components / source.name
        if destination.exists():
            raise RuntimeError(f'Refusing to replace an existing AU: {destination}')
        with (source / 'Contents/Info.plist').open('rb') as stream:
            info = plistlib.load(stream)
        descriptions = {(c.get('type'), c.get('subtype'), c.get('manufacturer'))
                        for c in info.get('AudioComponents', [])}
        identity = (kind, code, 'Frsh')
        if identity not in descriptions:
            raise RuntimeError(f'AU metadata mismatch in {source}: {descriptions}')
        plans.append((target, source, destination, identity))
    installed = []
    try:
        # Install both before discovery; keep them installed through both tests.
        for target, source, destination, identity in plans:
            installed.append(destination)
            subprocess.run(['ditto', str(source), str(destination)], check=True)
            print(f'Installed for AU validation: {destination} ({identity})')
        refresh_au_registry()
        expected = {p[3] for p in plans}
        for attempt in range(20):
            scan = subprocess.run(['/usr/bin/auval', '-a'], capture_output=True, text=True, timeout=30)
            listing = scan.stdout + scan.stderr
            (reports / 'auval-components.txt').write_text(listing, encoding='utf-8')
            found = {tuple(line.split()[:3]) for line in listing.splitlines() if len(line.split()) >= 3}
            if expected.issubset(found):
                break
            if attempt == 19:
                raise RuntimeError(f'AU registrar did not discover {expected - found}; inspect auval-components.txt')
            time.sleep(1)
        passed = True
        for target, source, destination, identity in plans:
            passed = run_logged(['/usr/bin/auval', '-v', *identity],
                                reports / f'auval-{target}.txt', timeout=300) and passed
        return passed
    finally:
        for destination in installed:
            if destination.exists():
                shutil.rmtree(destination)
        if installed:
            refresh_au_registry()


def validate(build_dir, pluginval=None, auval=False):
    build = Path(build_dir).resolve()
    reports = build / 'reports'
    reports.mkdir(parents=True, exist_ok=True)
    executable = Path(pluginval).resolve() if pluginval else validator(build)
    passed = True
    for target, product in [('FreshLab', 'FreshLab'), ('FreshLabFX', 'FreshLab FX')]:
        bundle = build / f'{target}_artefacts/Release/VST3/{product}.vst3'
        passed = run_logged([str(executable), '--strictness-level', '5', '--random-seed', '24680',
                             '--timeout-ms', '120000', '--validate', str(bundle),
                             '--output-dir', str(reports)], reports / f'pluginval-{target}.txt') and passed
    if auval:
        passed = validate_audio_units(build, reports) and passed
    if not passed:
        raise RuntimeError(f'Native plugin validation failed; inspect {reports}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', default='build')
    parser.add_argument('--pluginval', help='Use an existing pluginval executable')
    parser.add_argument('--auval', action='store_true', help='Temporarily install per-user Mac AUs and run auval')
    args = parser.parse_args()
    validate(args.build_dir, args.pluginval, args.auval)
