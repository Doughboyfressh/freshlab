# FreshLab 0.3 personal build

This work targets the current stable FL Studio on Windows and Mac. CI uses the
latest available GitHub Windows/macOS runners; that is not a claim of testing
inside a particular FL Studio release or on your hardware. Builds remain
unsigned ZIPs for personal use. Signing, notarization, publisher accounts and
public distribution installers are intentionally outside this scope.

## What changed

| Earlier limit | Personal-build change |
| --- | --- |
| Only electronic speech with limited spelling | An additional offline Installed system voice engine uses the default Windows/macOS speech voice and pronunciation rules. Original remains available. Only Speech speed affects the installed voice; other design controls apply to Original. |
| Creative grain harmony only | Live retains the quick creative layers. Studio uses pinned Signalsmith spectral shifting, formant compensation and adjustable vocal formant. |
| No pitch correction | Studio adds optional single-voice correction: amount, response, root and chromatic/major/minor/major-pentatonic scale. Unvoiced frames are rejected by a YIN-style detector. It is not polyphonic editing or a claim of artifact-free tuning. |
| Wet/dry delay mismatch and zero reported latency | Studio aligns dry/wet processing. Both quality modes report their fixed processing latency, including 16 distortion-FIR samples and the 3 ms limiter. Bypass preserves input samples with the same latency. Live’s creative wet delays are intentional effects. |
| Abrupt EQ and rack automation | EQ coefficients ramp over 15 ms; rack gains, sends, width, compressor threshold/ratio and delay time are smoothed. Discrete modes and note events are intentionally discrete. |
| Non-oversampled distortion and hard output ceiling | The distortion stage runs at 4x through interpolation/decimation FIRs. A stereo-linked 3 ms lookahead sample-peak limiter handles active output. It does not measure inter-sample peaks. |
| Fixed compressor and dynamic-EQ time constants | Compressor and dynamic-band attack/release are editable. |
| Fixed room | Room size, decay and damping are editable, with buffers allocated during preparation. |
| Millisecond-only delay | Optional host-tempo sync offers 1/32 through 1/2 notes, including triplet/dotted choices. Synced delay storage supports up to 8 seconds; the original unsynced 30–1500 ms parameter range is preserved for existing automation. Without host tempo, FX uses 120 BPM and the instrument uses Internal tempo. |
| No external sidechain | FreshLab FX has an optional mono/stereo sidechain input for its compressor. Enable the bus in the host and select External sidechain. The instrument does not expose this input. |
| No MPE or slides | Enable lower-zone MPE: member channels 2–16, channel 1 master, 48-semitone member bend (RPN bend-range overrides supported), channel pressure and CC74 timbre. Standard bend range is editable. Legato plus overlapping 808/synth notes provides portamento. FL-native slide-note events are not available to these VST3s. |
| Fixed 1240×940 editor | The editor scales between 868×658 and 1860×1410 with aspect ratio preserved. Width is stored in project/complete-sound state. |
| Sample import could make a callback fail its try-lock | Immutable audio sample snapshots use a hazard pointer. Copying, ownership changes and reclamation happen outside the audio callback. |
| Unknown CPU behavior | Core tests check for C++ heap allocations during a warmed full-rack processing run. Native CI saves synthetic 16-voice measurements at 44.1/48/96 kHz and 32/64/256-sample buffers. These measure the runner, not your interface or a full FL session. |
| Untested native plugin hosting | CI runs pinned pluginval 1.0.4 at level 5 for both VST3s and auval for both Mac AUs. The results and logs are attached to the run and successful ZIPs. Actual FL Studio scanning/playback/rendering still needs your installation. |

## Live versus Studio

Live keeps instrument playing responsive (about 3.4 ms processing latency at
44.1 kHz). Studio uses a longer analysis window for accurate pitch shifting;
its processing latency is about 120 ms at 44.1 kHz. Both are reported to the
host. Use Studio for rendered vocals/harmony/tuning. Formant and tuning
controls are enabled only in Studio. Change quality with transport stopped;
changes requested while host transport is playing wait until it stops.
For Studio monitoring, start with a 256-sample buffer or larger and check
underruns on your machine. The synthetic stress test can exceed a 32-sample
block’s deadline in Studio even when average CPU is modest; Live is the
mode intended for very small buffers.

The lookahead limiter’s ceiling defaults to -0.3 dBFS. It uses common gain
reduction for both channels, preserving stereo balance. Bypass can exceed
that ceiling because it deliberately returns the original samples.

## Voices and rights

Original’s synthesized output and the supplied original/example WAV bank
keep their CC0 dedication. Installed system voices are separate: their
quality, supported languages and output terms come from the installed
voice/provider. No cloud account, API key or paid speech service is used.
Use your OS speech settings to select/install a default voice. Generation
runs on a worker, with a 30-second subprocess timeout and temporary cleanup.

## Verification

```bash
cmake -S . -B build-core -DFRESHLAB_CORE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel 2
ctest --test-dir build-core --output-on-failure
python scripts/benchmark.py --build-dir build-core
```

For a full native build, run the five suites, then:

```bash
python scripts/validate_plugins.py --build-dir build
python scripts/benchmark.py --build-dir build
```

On headless Linux, prefix the validator with `xvfb-run -a`. On a Mac,
`--auval` additionally installs both components temporarily in the per-user
AU folder, tests them, then removes them. It refuses to overwrite an existing
component. CI also sets `FRESHLAB_TEST_SYSTEM_SPEECH=1` when running CTest to
exercise the installed-voice path on Windows and Mac.

`FreshLab-validation-Windows` / `FreshLab-validation-macOS` include CTest
JUnit, pluginval/AU logs and `performance.json`. A budget fraction of 1 means
processing took the entire audio block duration; scheduling outliers and
other plugins can affect your actual host budget. Use your own FL Studio
buffer setting as the final reference.

## Final personal FL Studio check

1. Copy the two complete VST3 bundles as described in INSTALL.md and scan
   them with verification enabled in the current stable FL Studio.
2. Play the 808, synth and voice sampler. Try Live first; check note timing,
   overlapping-note Legato, standard pitch bend and any MPE controller.
3. Put FreshLab FX on a vocal Mixer track. Try Studio harmony/formants and
   tuning, host-synced delay, room controls and an externally routed sidechain.
4. Save/reopen the project, export a WAV and compare it with playback. Check
   CPU/underruns at your normal audio-device buffer size. Compare bypass
   after allowing the latency and any crossfade to settle.
5. Try Installed system voice, then resize/reopen the editor. Keep Original
   selected if your OS has no usable system voice installed.

These steps are not marked complete automatically: this workspace has no
Windows/macOS FL Studio session or your audio interface.
