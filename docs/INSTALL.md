# Install FreshLab development builds

These packages contain FreshLab (instrument) and FreshLab FX (mixer effect),
the original WAV voices, standalone applications, and matching source.
They are unsigned development builds. FL Studio host testing is still required.

## Windows x64

1. Unzip the download. Copy both complete folders from `VST3/` to
   `C:\Program Files\Common Files\VST3`. Windows may ask for administrator permission.
2. Open FL Studio, then Options > Manage plugins > Find installed plugins.
3. Add FreshLab to the Channel rack or FreshLab FX to a Mixer insert.

Keep each entire `.vst3` folder together; copying only its DLL will not work.
The applications in `Standalone/` can be opened separately for an audio/MIDI check.

## macOS Intel and Apple Silicon

1. Unzip the download. In Finder choose Go > Go to Folder and enter
   `~/Library/Audio/Plug-Ins/VST3`. Create the VST3 folder if needed.
2. Copy both complete bundles from `VST3/` into that folder.
3. Rescan plugins in FL Studio and add FreshLab or FreshLab FX.

Optional Audio Units go from `AU/` into `~/Library/Audio/Plug-Ins/Components`.
The universal applications in `Standalone/` can be opened separately.
macOS may block unsigned applications/plugins. Signed and notarized distribution
requires a future build with the publisher's Apple signing credentials.

## Try the voices now

Drag the WAV files from `Voices/` or `Examples/Voices/` into FL Studio's
Sampler or Playlist. They work independently of the plugins. Original audio is
CC0 as described in `Voices/VOICE-LICENSE.txt`.

## Check in FL Studio

Verify plugin scanning, all engines, MIDI notes and pads, typed voice generation,
automation, project save/reopen, and audio export. Keep volume low for first tests.
`BUILD.json` identifies the exact source commit and records package checksums.
Full matching FreshLab and JUCE sources are in `Source/`; dependency licenses
and notices remain applicable.
