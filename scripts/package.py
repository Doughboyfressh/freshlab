#!/usr/bin/env python3
"""Package complete native bundles; preserve macOS permissions and symlinks."""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile


def package(build_dir, output_dir):
    root = Path(__file__).resolve().parents[1]
    build_dir = Path(build_dir).resolve()
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    system = platform.system()
    if system not in ("Windows", "Darwin"):
        raise RuntimeError("Native packages must be built on Windows or macOS.")
    version = "0.2.0"
    label = "Windows-x64" if system == "Windows" else "macOS-universal"
    name = f"FreshLab-{version}-{label}"
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True
    ).strip()
    juce = build_dir / "_deps" / "juce-src"
    if not (juce / "LICENSE.md").is_file():
        raise RuntimeError("The pinned JUCE source and license must accompany the package.")

    with tempfile.TemporaryDirectory(prefix="freshlab-package-") as temp:
        stage = Path(temp) / name
        stage.mkdir()
        for target, product in (("FreshLab", "FreshLab"), ("FreshLabFX", "FreshLab FX")):
            artefacts = build_dir / f"{target}_artefacts" / "Release"
            bundles = [("VST3", f"{product}.vst3")]
            bundles.append(("Standalone", f"{product}.exe" if system == "Windows" else f"{product}.app"))
            if system == "Darwin":
                bundles.append(("AU", f"{product}.component"))
            for kind, filename in bundles:
                source = artefacts / kind / filename
                if not source.exists():
                    raise RuntimeError(f"Missing build output: {source}")
                destination = stage / kind / filename
                destination.parent.mkdir(parents=True, exist_ok=True)
                if source.is_dir():
                    shutil.copytree(source, destination, symlinks=True)
                else:
                    shutil.copy2(source, destination)
        for folder in ("Voices", "Examples"):
            shutil.copytree(root / folder, stage / folder)
        previews = stage / "Previews"
        previews.mkdir()
        shutil.copytree(root / "docs" / "ui", previews / "ui")
        for filename in ("voice-preview.wav", "dsp-preview.wav"):
            shutil.copy2(root / "docs" / filename, previews / filename)
        for filename in ("README.md", "LICENSE"):
            shutil.copy2(root / filename, stage / filename)
        notices = stage / "Notices"
        notices.mkdir()
        for filename in ("THIRD-PARTY.md", "JUCE-8.0.12-LICENSE.md", "VALIDATION.md", "INSTALL.md"):
            shutil.copy2(root / "docs" / filename, stage / filename)
        shutil.copy2(juce / "LICENSE.md", notices / "JUCE-LICENSE.md")
        source_dir = stage / "Source"
        source_dir.mkdir()
        subprocess.run(
            ["git", "archive", "--format=zip", f"--output={source_dir / 'FreshLab-source.zip'}", revision],
            cwd=root, check=True,
        )
        # Include the exact framework source, including bundled SDK notices.
        shutil.make_archive(str(source_dir / "JUCE-8.0.12-source"), "zip", juce)
        checksums = {}
        for path in sorted(stage.rglob("*")):
            if path.is_file() and not path.is_symlink():
                checksums[path.relative_to(stage).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
        (stage / "BUILD.json").write_text(json.dumps({
            "version": version, "platform": label, "commit": revision,
            "signed": False, "fl_studio_validated": False, "sha256": checksums,
        }, indent=2) + "\n", encoding="utf-8")
        archive = output_dir / f"{name}.zip"
        if system == "Darwin":
            subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(stage), str(archive)], check=True)
        else:
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as result:
                for path in sorted(stage.rglob("*")):
                    if path.is_file():
                        result.write(path, path.relative_to(stage.parent))
        with zipfile.ZipFile(archive) as result:
            if result.testzip() is not None:
                raise RuntimeError("Package ZIP integrity check failed.")
        print(archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--output-dir", default="dist")
    args = parser.parse_args()
    package(args.build_dir, args.output_dir)
