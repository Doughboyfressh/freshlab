# Dependencies

FreshLab's original source uses the MIT license in ../LICENSE. Its original
WAV samples use CC0 as described in ../Voices/VOICE-LICENSE.txt.

The plugin wrapper and UI require JUCE 8.0.12, fetched from the official
juce-framework/JUCE repository at commit
29396c22c93392d6738e021b83196283d6e4d850. JUCE is dual-licensed under AGPLv3
and its commercial JUCE license. The FreshLab MIT license does not relicense
JUCE. Choose the appropriate JUCE license before distributing a combined
plugin; the voice pack can be used separately without JUCE.

Pinned framework license:
https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md

JUCE's plugin wrapper includes third-party VST3 SDK code with its own notices.
Preserve the applicable upstream notices in binary distributions. Framework
sources and their notices can be obtained from the pinned repository. No
proprietary FL Studio SDK, Image-Line assets, speech-model weights, downloaded
audio, or commercial samples are included in FreshLab's source archive.
