#!/usr/bin/env python3
"""Validate native VST3s with pinned pluginval; optionally install/test Mac AUs."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
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
        if platform.system() != 'Darwin':
            raise RuntimeError('--auval requires macOS')
        # Explicit opt-in: auval discovers AUs through the per-user installation.
        components = Path.home() / 'Library/Audio/Plug-Ins/Components'
        components.mkdir(parents=True, exist_ok=True)
        for target, product, kind, code in [('FreshLab', 'FreshLab', 'aumu', 'Frsi'),
                                           ('FreshLabFX', 'FreshLab FX', 'aufx', 'Frfx')]:
            source = build / f'{target}_artefacts/Release/AU/{product}.component'
            destination = components / source.name
            if destination.exists():
                raise RuntimeError(f'Refusing to replace an existing AU: {destination}')
            try:
                subprocess.run(['ditto', str(source), str(destination)], check=True)
                passed = run_logged(['/usr/bin/auval', '-v', kind, code, 'Frsh'],
                                    reports / f'auval-{target}.txt', timeout=300) and passed
            finally:
                if destination.exists():
                    shutil.rmtree(destination)
    if not passed:
        raise RuntimeError(f'Native plugin validation failed; inspect {reports}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', default='build')
    parser.add_argument('--pluginval', help='Use an existing pluginval executable')
    parser.add_argument('--auval', action='store_true', help='Temporarily install per-user Mac AUs and run auval')
    args = parser.parse_args()
    validate(args.build_dir, args.pluginval, args.auval)
