# Validation — FreshLab 0.2.0 source prototype

Checked November 15, 2026 (America/Los_Angeles), on Linux x86_64 with GCC
14.2, C++17, Release configuration and pinned JUCE 8.0.12.

## Build and test results

Both instrument and effects shared-code targets, both Linux VST3 bundles,
and both Linux standalone application targets compile and link. JUCE's VST3
manifest helper loads both bundles and creates their module metadata. This
is development verification, not FL Studio host validation.

All four CTest suites pass:

| Suite | Main coverage |
| --- | --- |
| freshlab-dsp | Original oscillator/sampler/FX stability at 22.05, 44.1, 48, 96 and 192 kHz; tuning, envelopes, sustain, reverse, one-shot, panic, pitch bend and bypass |
| freshlab-extensions | Rule-based speech, deterministic output, rejection of bad/overlong text, designer timbre changes, stereo spread, speed; sequencer repeats/swing/looping; harmonizer interval on steady tones; dynamic EQ reduction; layered/drawn-pitch 808 stability |
| freshlab-instrument | Sample-accurate MIDI and MIDI pads; typed voice generation/export; WAV format; all 24 factory voices; host-synced sequence onset and transport stop; saved voices/EQ/effects/complete patches; parameter, pattern, pad and branding recall; first note after preset loading; logo import and persistence |
| freshlab-fx | Exact stereo bypass, audio processing, new vocal-layer recall, EQ and effects sound files, branding/logo recall, malformed input handling and editor creation |

The core suites count approximately 1.9 million assertions, mostly per-sample
finite/bounded-output checks. This is not a count of separate test cases.
The core and extension suites also pass AddressSanitizer and
UndefinedBehaviorSanitizer. LeakSanitizer is disabled because this runtime
uses tracing that it cannot support; leaks were not tested.

Nine instrument editor pages and four applicable FX pages are rendered and
visually inspected. `docs/ui/` contains software renders of the actual JUCE
editors, not images of FL Studio. Expanded UI dimensions are 1240 by 940.

## Audio assets

- Original bank: 24 distinct, normalized 44.1 kHz, 16-bit mono WAVs, with
  quiet boundaries. Hashes and durations are in Voices/manifest.json.
- Typed-phrase examples: eight distinct 44.1 kHz, 16-bit stereo WAVs from the
  same C++ speech engine used by the plugin. Their phrases, settings and
  hashes are in Examples/Voices/manifest.json.
- Updated preview: stereo WAV, with typed examples followed by the original
  bank. The original 808/synth demo is also included.
- File structure, format, nonzero audio and bounds are checked. No listening
  judgment about human naturalness, pronunciation quality or mix quality is
  claimed. The generated speech is intentionally electronic.

## Native build recovery — November 16, 2026 (UTC)

The [first native run](https://github.com/Doughboyfressh/freshlab/actions/runs/43812760134)
compiled Windows x64 and universal macOS targets. Windows passed all four
CTest suites and produced the `FreshLab-Windows` development package.
Mac passed the core DSP and extension suites, but stopped before packaging:
the instrument and FX suites used exact float equality for recalled gains.
Fused multiply-add in range normalization can return -3.000000477 instead
of -3 dB, or -0.999999046 instead of -1 dB.

Continuous parameter assertions now use a 0.00001 absolute tolerance with
actual/expected diagnostics. Integer and choice assertions remain exact.
The FX recall test now changes harmony, doubler and dynamic EQ controls
before restoring and checking them. The latest native results are in
[Actions](https://github.com/Doughboyfressh/freshlab/actions).

The missing UI and audio previews are regenerated from the committed source
and voice bank, and tracked in `docs/ui/`, `docs/voice-preview.wav` and
`docs/dsp-preview.wav`. CI writes its own editor/audio previews and uploads
them separately; failed builds retain CTest diagnostic logs. Native packages
include the committed previews alongside the source archive.

The recovery was checked locally on Linux x86_64 with GCC 14.2, Release and
JUCE 8.0.12: all four CTest suites pass under Xvfb, with nine instrument and
four FX renders at 1240 by 940. Both audio previews are stereo 44.1 kHz PCM16;
the voice preview is 27.33 seconds and the DSP demo is 12 seconds. All 32
source WAV hashes still match their manifests, and all 32 preview segments
match the source audio exactly (mono bank files are duplicated to stereo).

## Still outstanding

AU host loading, installer packaging, signing/notarization, actual FL Studio
scanning, live playback/export, automation and cross-platform project
reopening have not been validated. Native compilation and automated test
results are recorded separately in Actions as described above.

Prolonged low-latency CPU performance, many simultaneous instances,
real-time glitch behavior during import/state replacement, host crash
recovery and production release readiness have not been certified.

This repository is a source prototype plus a directly usable original WAV
pack. Native development binaries are distributed as Actions artifacts,
rather than checked into Git. Linux standalone apps were compiled, not
tested against an audio device.

## Reproduce

```bash
cmake -S . -B build-core -DFRESHLAB_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Full processor integration tests require a JUCE/native build; headless Linux
also needs a display such as Xvfb. Setting FRESHLAB_SNAPSHOT_DIR during CTest
writes the editor renders. The runtime-generated sound and image files used
by integration tests are confined to a temporary test directory and removed.
