#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Offline rendering through the user's installed system voice. No network,
// model downloads or shell interpolation of the phrase. Voice licenses belong
// to the OS/provider and are distinct from the original CC0 formant engine.
namespace fresh {
inline juce::String renderSystemSpeech(const juce::String& text,float speed,const juce::File& destination){
    using namespace juce;
    if(text.trim().isEmpty()||text.length()>100)return "Type between 1 and 100 characters.";
    const auto scratch=File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("freshlab-speech","",false);
    if(scratch.createDirectory().failed())return "Could not create a temporary speech folder.";
    struct Cleanup {File file;~Cleanup(){file.deleteRecursively();}}cleanup{scratch};
    const auto input=scratch.getChildFile("phrase.txt");if(!input.replaceWithText(text))return "Could not prepare speech text.";
    StringArray args;
   #if JUCE_MAC
    args={"/usr/bin/say","-f",input.getFullPathName(),"-o",destination.getFullPathName(),"--file-format=AIFF","--data-format=BEI16@44100","-r",String(int(175*std::clamp(speed,.6f,1.6f)))};
   #elif JUCE_WINDOWS
    const auto script=scratch.getChildFile("speak.ps1");
    const String source=R"PS(param([string]$TextFile,[string]$OutputFile,[int]$Rate)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Speech
$synth=[System.Speech.Synthesis.SpeechSynthesizer]::new()
try {
  $synth.Rate=$Rate
  $format=[System.Speech.AudioFormat.SpeechAudioFormatInfo]::new(44100,[System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen,[System.Speech.AudioFormat.AudioChannel]::Stereo)
  $synth.SetOutputToWaveFile($OutputFile,$format)
  $synth.Speak([System.IO.File]::ReadAllText($TextFile))
} finally { $synth.Dispose() }
)PS";
    if(!script.replaceWithText(source))return "Could not prepare the installed voice renderer.";
    args={"powershell.exe","-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",script.getFullPathName(),"-TextFile",input.getFullPathName(),"-OutputFile",destination.getFullPathName(),"-Rate",String(int(std::round(std::log2(std::clamp(speed,.6f,1.6f))*5)))};
   #else
    return "System voices are available in the Windows and Mac builds. Use Original on this platform.";
   #endif
    ChildProcess process;
    if(!process.start(args,ChildProcess::wantStdOut|ChildProcess::wantStdErr))return "Could not start the installed system voice.";
    if(!process.waitForProcessToFinish(30000)){process.kill();return "System speech timed out. Try a shorter phrase.";}
    if(process.getExitCode()!=0||destination.getSize()<44)return "System speech failed. Install an OS voice or select Original.";
    return {};
}
}
