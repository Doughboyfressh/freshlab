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

## Still outstanding

Native Windows x64 and macOS universal compilation, AU host loading,
installer packaging, signing/notarization, actual FL Studio scanning, live
playback/export, automation and cross-platform project reopening have not
been validated. Supplied native build scripts and the GitHub Actions matrix
are prepared, but no native workflow run occurred in this delivery.

Prolonged low-latency CPU performance, many simultaneous instances,
real-time glitch behavior during import/state replacement, host crash
recovery and production release readiness have not been certified.

This is a source prototype plus a directly usable original WAV pack.
Windows/Mac plugin binaries are not included. Linux standalone apps were
compiled, not tested against an audio device.

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
