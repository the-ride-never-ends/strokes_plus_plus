[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = 'build-vs2026',

    [string]$Generator = $env:CMAKE_GENERATOR,

    [string]$Architecture = 'x64',

    [switch]$Test,

    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSCommandPath
$minimumCMakeVersion = [version]'3.25'

function Get-CMakeVersion {
    param([Parameter(Mandatory)][string]$Path)

    $output = & $Path --version 2>&1
    $match = [regex]::Match(($output -join "`n"),
                            '(?m)^cmake version ([0-9]+(?:\.[0-9]+){1,3})')
    if ($LASTEXITCODE -ne 0 -or -not $match.Success) {
        throw "Unable to determine the CMake version from: $Path"
    }
    return [version]$match.Groups[1].Value
}

function Resolve-CMakeCandidate {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $version = Get-CMakeVersion -Path $resolvedPath
    if ($version -lt $minimumCMakeVersion) { return $null }
    return [pscustomobject]@{ Path = $resolvedPath; Version = $version }
}

function Find-CMake {
    if ($env:STROKES_CMAKE) {
        if (-not (Test-Path -LiteralPath $env:STROKES_CMAKE -PathType Leaf)) {
            throw "STROKES_CMAKE does not name a file: $env:STROKES_CMAKE"
        }
        $candidate = Resolve-CMakeCandidate -Path $env:STROKES_CMAKE
        if (-not $candidate) {
            throw "STROKES_CMAKE must point to CMake $minimumCMakeVersion or newer."
        }
        return $candidate.Path
    }

    $pathCommand = Get-Command cmake.exe -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($pathCommand) {
        try {
            $pathCandidate = Resolve-CMakeCandidate -Path $pathCommand.Source
            if ($pathCandidate) { return $pathCandidate.Path }
        } catch {
            Write-Verbose $_
        }
    }

    $candidates = [System.Collections.Generic.List[string]]::new()
    foreach ($programFiles in @($env:ProgramFiles,
                                 [Environment]::GetEnvironmentVariable('ProgramFiles(x86)'))) {
        if ($programFiles) {
            $candidates.Add((Join-Path $programFiles 'CMake\bin\cmake.exe'))
        }
    }
    if ($env:USERPROFILE) {
        Get-ChildItem -LiteralPath $env:USERPROFILE -Directory -Filter 'cmake-*-windows-*' `
                -ErrorAction SilentlyContinue |
            ForEach-Object { $candidates.Add((Join-Path $_.FullName 'bin\cmake.exe')) }
    }

    $compatible = foreach ($path in $candidates | Select-Object -Unique) {
        try {
            Resolve-CMakeCandidate -Path $path
        } catch {
            Write-Verbose $_
        }
    }
    $selected = $compatible | Sort-Object Version -Descending | Select-Object -First 1
    if ($selected) { return $selected.Path }

    throw "CMake $minimumCMakeVersion or newer was not found. Add it to PATH or set STROKES_CMAKE to cmake.exe."
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)][string]$Program,
        [Parameter(ValueFromRemainingArguments)][string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $Program $($Arguments -join ' ')"
    }
}

function Get-CMakeConfigureArguments {
    param(
        [Parameter(Mandatory)][string]$SourceDirectory,
        [Parameter(Mandatory)][string]$BuildDirectory,
        [string]$Generator,
        [string]$Architecture
    )

    $arguments = [System.Collections.Generic.List[string]]::new()
    $arguments.AddRange([string[]]@('-S', $SourceDirectory, '-B', $BuildDirectory))
    if ($Generator) { $arguments.AddRange([string[]]@('-G', $Generator)) }

    $effectiveGenerator = if ($Generator) { $Generator } else { $env:CMAKE_GENERATOR }
    if ($Architecture -and
        (-not $effectiveGenerator -or $effectiveGenerator -like 'Visual Studio*')) {
        $arguments.AddRange([string[]]@('-A', $Architecture))
    }
    return $arguments.ToArray()
}
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory
} else {
    Join-Path $repositoryRoot $BuildDirectory
}

function Test-ExecutableLocked {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    try {
        $stream = [System.IO.File]::Open($Path, 'Open', 'ReadWrite', 'None')
        $stream.Dispose()
        return $false
    } catch [System.IO.IOException] {
        return $true
    }
}

$requestedBuildPath = $buildPath
for ($attempt = 0; $attempt -lt 10; $attempt++) {
    $executable = Join-Path $buildPath "$Configuration\StrokesPlusPlus.exe"
    if (-not (Test-ExecutableLocked $executable)) { break }
    $buildPath = "$requestedBuildPath-staging$($attempt + 1)"
    Write-Host "The application is running from $executable; building in $buildPath instead."
}
if (Test-ExecutableLocked (Join-Path $buildPath "$Configuration\StrokesPlusPlus.exe")) {
    throw 'No unlocked build directory was found.'
}

$cmake = Find-CMake
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'

if (-not (Test-Path -LiteralPath (Join-Path $buildPath 'CMakeCache.txt'))) {
    Write-Host "Configuring Strokes++ in $buildPath"
    $configureArguments = Get-CMakeConfigureArguments -SourceDirectory $repositoryRoot `
        -BuildDirectory $buildPath -Generator $Generator -Architecture $Architecture
    Invoke-Checked -Program $cmake -Arguments $configureArguments
}

Write-Host "Building Strokes++ ($Configuration)"
Invoke-Checked -Program $cmake -Arguments @('--build', $buildPath, '--config', $Configuration, '--parallel')

if ($Test) {
    if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
        throw "CTest was not found beside CMake: $ctest"
    }
    Write-Host 'Running tests'
    Invoke-Checked -Program $ctest -Arguments @('--test-dir', $buildPath, '-C', $Configuration, '--output-on-failure', '-LE', 'performance')
}

if (-not $NoRun) {
    $executable = Join-Path $buildPath "$Configuration\StrokesPlusPlus.exe"
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "StrokesPlusPlus.exe was not produced at $executable"
    }
    Write-Host 'Starting Strokes++ in the notification area'
    Start-Process -FilePath $executable -WorkingDirectory $repositoryRoot -WindowStyle Hidden
}
