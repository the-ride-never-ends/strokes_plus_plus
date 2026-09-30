[CmdletBinding()]
param(
    [ValidateSet('NSIS64', 'ZIP', 'Both')]
    [string]$Format = 'NSIS64',
    [string]$Generator = $env:CMAKE_GENERATOR,
    [string]$Architecture = 'x64',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$sourceDirectory = $PSScriptRoot
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
$buildDirectory = Join-Path $sourceDirectory 'build-package'

function Find-MakeNSIS {
    if ($env:STROKES_NSIS -and
        (Test-Path -LiteralPath $env:STROKES_NSIS -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $env:STROKES_NSIS).Path
    }
    $command = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($programFiles in @([Environment]::GetEnvironmentVariable('ProgramFiles(x86)'),
                                 $env:ProgramFiles)) {
        if (-not $programFiles) { continue }
        $candidate = Join-Path $programFiles 'NSIS\makensis.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw 'NSIS is required to build the installer. Install NSIS or set STROKES_NSIS to makensis.exe.'
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

if ($Format -eq 'NSIS64' -or $Format -eq 'Both') {
    $makensis = Find-MakeNSIS
    $env:PATH = (Split-Path -Parent $makensis) + ';' + $env:PATH
}

$configureArguments = Get-CMakeConfigureArguments -SourceDirectory $sourceDirectory `
    -BuildDirectory $buildDirectory -Generator $Generator -Architecture $Architecture
$configureArguments += '-DSTROKES_BUILD_TESTS=ON'
& $cmake @configureArguments
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
