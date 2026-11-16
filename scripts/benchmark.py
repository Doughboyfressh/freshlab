#!/usr/bin/env python3
"""Run the native single-instance synthetic CPU benchmark and save JSON."""
import argparse
import json
import math
import os
import platform
from datetime import datetime, timezone
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build')
args = parser.parse_args()
build = Path(args.build_dir).resolve()
executable = next((path for directory in (build / 'Release', build)
                   for suffix in ('', '.exe')
                   if (path := directory / f'freshlab-benchmark{suffix}').is_file()), None)
if executable is None:
    raise FileNotFoundError('Build freshlab-benchmark first')
report = build / 'reports/performance.json'
report.parent.mkdir(parents=True, exist_ok=True)
subprocess.run([str(executable), str(report)], check=True, timeout=180)
data = json.loads(report.read_text())
data['environment'] = {'system': platform.platform(), 'machine': platform.machine(),
                       'logical_cpus': os.cpu_count(), 'generated_at': datetime.now(timezone.utc).isoformat(),
                       'warmup_seconds': .25, 'measurement_seconds': 1.0}
report.write_text(json.dumps(data, indent=2) + '\n')
assert len(data['results']) == 27
for row in data['results']:
    for key in ('average_budget_fraction', 'p99_budget_fraction', 'max_budget_fraction'):
        assert math.isfinite(row[key]) and row[key] >= 0, (key, row)
print(f'Wrote {report}')
print('Worst average audio-budget fraction:', max(row['average_budget_fraction'] for row in data['results']))
