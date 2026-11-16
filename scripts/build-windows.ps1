param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build build --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if (-not $SkipTests) {
    ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
}
Write-Host 'Built plugins: build/FreshLab_artefacts/Release/VST3 and build/FreshLabFX_artefacts/Release/VST3'
Write-Host 'Copy both entire .vst3 folders into C:\Program Files\Common Files\VST3, then rescan in FL Studio.'
