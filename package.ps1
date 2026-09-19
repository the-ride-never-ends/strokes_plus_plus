[CmdletBinding()]
param(
    [ValidateSet('NSIS64', 'ZIP', 'Both')]
    [string]$Format = 'NSIS64',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$sourceDirectory = $PSScriptRoot
$buildDirectory = Join-Path $sourceDirectory 'build-package'

function Find-CMake {
    if ($env:STROKES_CMAKE -and
        (Test-Path -LiteralPath $env:STROKES_CMAKE -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:STROKES_CMAKE).Path
    }
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $localCMake = Join-Path $env:USERPROFILE 'cmake-4.4.3-windows-x86_64\bin\cmake.exe'
    if (Test-Path -LiteralPath $localCMake -PathType Leaf) { return $localCMake }
    throw 'CMake was not found. Add it to PATH or set STROKES_CMAKE to cmake.exe.'
}

$cmake = Find-CMake
$toolDirectory = Split-Path -Parent $cmake
$cpack = Join-Path $toolDirectory 'cpack.exe'
$ctest = Join-Path $toolDirectory 'ctest.exe'
if (-not (Test-Path -LiteralPath $cpack -PathType Leaf)) {
    throw "CPack was not found beside CMake: $cpack"
}
if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
    throw "CTest was not found beside CMake: $ctest"
}

if (($Format -eq 'NSIS64' -or $Format -eq 'Both') -and
    -not (Get-Command makensis.exe -ErrorAction SilentlyContinue)) {
    throw 'NSIS is required to build the installer. Install NSIS, then run this script again.'
}

& $cmake -S $sourceDirectory -B $buildDirectory -A x64 `
    -DSTROKES_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

& $cmake --build $buildDirectory --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }

if (-not $SkipTests) {
    & $ctest --test-dir $buildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Release tests failed.' }
}

$generators = if ($Format -eq 'Both') { @('NSIS64', 'ZIP') } else { @($Format) }
foreach ($generator in $generators) {
    & $cpack --config (Join-Path $buildDirectory 'CPackConfig.cmake') `
        -C Release -G $generator -B (Join-Path $buildDirectory 'packages')
    if ($LASTEXITCODE -ne 0) { throw "$generator packaging failed." }
}

Write-Host "Packages written to $buildDirectory\packages"
