[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Debug',

    [string]$BuildDirectory = 'build-vs2026',

    [switch]$Test,

    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSCommandPath
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory
} else {
    Join-Path $repositoryRoot $BuildDirectory
}

function Find-CMake {
    if ($env:STROKES_CMAKE -and (Test-Path -LiteralPath $env:STROKES_CMAKE -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:STROKES_CMAKE).Path
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $installedCMake = Join-Path $env:USERPROFILE 'cmake-4.4.3-windows-x86_64\bin\cmake.exe'
    if (Test-Path -LiteralPath $installedCMake -PathType Leaf) {
        return $installedCMake
    }

    throw 'CMake was not found. Add it to PATH or set STROKES_CMAKE to cmake.exe.'
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)]
        [string]$Program,

        [Parameter(ValueFromRemainingArguments)]
        [string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $Program $($Arguments -join ' ')"
    }
}

$cmake = Find-CMake
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'

if (-not (Test-Path -LiteralPath (Join-Path $buildPath 'CMakeCache.txt'))) {
    Write-Host "Configuring Strokes++ in $buildPath"
    Invoke-Checked $cmake -S $repositoryRoot -B $buildPath -A x64
}

Write-Host "Building Strokes++ ($Configuration)"
Invoke-Checked $cmake --build $buildPath --config $Configuration --parallel

if ($Test) {
    if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
        throw "CTest was not found beside CMake: $ctest"
    }
    Write-Host 'Running tests'
    Invoke-Checked $ctest --test-dir $buildPath -C $Configuration --output-on-failure
}

if (-not $NoRun) {
    $executable = Join-Path $buildPath "$Configuration\GestureEngine.exe"
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "GestureEngine.exe was not produced at $executable"
    }
    Write-Host 'Starting Strokes++ in the notification area'
    Start-Process -FilePath $executable -WorkingDirectory $repositoryRoot -WindowStyle Hidden
}
