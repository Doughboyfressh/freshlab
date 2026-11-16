#!/usr/bin/env python3
"""Rebuild the audio previews from committed voices and the DSP test renderer."""
import argparse
from array import array
import json
from pathlib import Path
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
RATE = 44100


def write_voice_preview(destination):
    examples = json.loads((ROOT / "Examples/Voices/manifest.json").read_text())
    sources = [ROOT / "Examples/Voices" / item["file"] for item in examples["examples"]]
    sources += [ROOT / "Voices" / f"{style}_{word}.wav"
                for style in ("sub", "spark", "haze", "circuit")
                for word in ("yeah", "hey", "whoa", "go", "ah", "brr")]
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(destination), "wb") as output:
        output.setparams((2, 2, RATE, 0, "NONE", "not compressed"))
        for source in sources:
            with wave.open(str(source), "rb") as voice:
                channels = voice.getnchannels()
                if (voice.getframerate() != RATE or voice.getsampwidth() != 2
                        or channels not in (1, 2) or voice.getcomptype() != "NONE"):
                    raise ValueError(f"Expected 44.1 kHz mono/stereo PCM16: {source}")
                frames = voice.readframes(voice.getnframes())
            if channels == 1:
                mono = array("h")
                mono.frombytes(frames)
                if sys.byteorder != "little":
                    mono.byteswap()
                stereo = array("h", (sample for sample in mono for _ in range(2)))
                if sys.byteorder != "little":
                    stereo.byteswap()
                frames = stereo.tobytes()
            output.writeframes(frames)
            output.writeframes(bytes(round(RATE * .18) * 4))
    print(f"Wrote {destination} / {len(sources)} voices, typed examples first")


def make_previews(build_dir, output_dir, config):
    build_dir = Path(build_dir).resolve()
    output_dir = Path(output_dir).resolve()
    # CMake puts multi-config (Xcode/Visual Studio) executables under Release/.
    candidates = [folder / f"freshlab-dsp-tests{suffix}"
                  for folder in (build_dir / config, build_dir)
                  for suffix in ("", ".exe")]
    renderer = next((path for path in candidates if path.is_file()), None)
    if renderer is None:
        raise FileNotFoundError(f"Build freshlab-dsp-tests first: {build_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(renderer), str(output_dir / "dsp-preview.wav")], check=True)
    write_voice_preview(output_dir / "voice-preview.wav")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--output-dir", default="docs")
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    make_previews(args.build_dir, args.output_dir, args.config)
