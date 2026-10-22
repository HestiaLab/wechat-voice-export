[CmdletBinding()]
param(
    [string]$Preset = "ninja-debug",
    [switch]$SkipFormat,
    [switch]$FixFormat,
    [switch]$EnableTidy
)

$ErrorActionPreference = "Stop"

# PowerShell binds a stray bash-style `--flag` to the first positional
# parameter, which here is $Preset; that fails downstream with a confusing
# CMake error. Fail fast with an actionable message instead.
if ($Preset -like "-*") {
    [Console]::Error.WriteLine("Unknown option '$Preset'. PowerShell switches use a single dash: -EnableTidy, -SkipFormat, -FixFormat, -Preset <name>.")
    exit 2
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

Push-Location $RepoRoot
try {

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    # Under $ErrorActionPreference="Stop" on PowerShell 5.1, a native command
    # writing to stderr is surfaced as NativeCommandError and terminates the
    # script even when the command only returned a non-zero exit code. Drop to
    # Continue for the call and judge success by $LASTEXITCODE instead.
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Command @Arguments
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not $SkipFormat -and (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    $files = Get-ChildItem -Path include,src,tests -Recurse -File -Include *.c,*.h,*.cpp |
        Where-Object { $_.FullName -notlike "*\tests\vendor\*" } |
        Sort-Object FullName
    if ($files.Count -gt 0) {
        # Build the file list with `+=`, not an inline array literal. Binding
        # `@("-i", $files.FullName)` to a [string[]] parameter flattens the path
        # array into one space-joined string that clang-format rejects as
        # "Invalid argument". `+=` appends each path as its own argument.
        if ($FixFormat) {
            $formatArgs = @("-i")
            $formatArgs += $files.FullName
            Invoke-CheckedCommand -Command "clang-format" -Arguments $formatArgs
        } else {
            $formatArgs = @("--dry-run", "--Werror")
            $formatArgs += $files.FullName
            Invoke-CheckedCommand -Command "clang-format" -Arguments $formatArgs
        }
    }
} elseif (-not $SkipFormat) {
    Write-Host "Skipping clang-format: command not found."
}

Invoke-CheckedCommand -Command "cmake" -Arguments @("--preset", $Preset)
Invoke-CheckedCommand -Command "cmake" -Arguments @("--build", "--preset", $Preset)

if ($EnableTidy) {
    if (Get-Command clang-tidy -ErrorAction SilentlyContinue) {
        $buildDirectory = Join-Path "build" $Preset
        $compileCommands = Join-Path $buildDirectory "compile_commands.json"
        $generatedIncludeDirectory = Join-Path $buildDirectory "generated\include"
        $files = Get-ChildItem -Path src,tests -Recurse -File -Include *.c |
            Where-Object { $_.FullName -notlike "*\tests\vendor\*" } |
            Sort-Object FullName
        foreach ($file in $files) {
            if (Test-Path $compileCommands) {
                Invoke-CheckedCommand -Command "clang-tidy" -Arguments @(
                    $file.FullName,
                    "--warnings-as-errors=*",
                    "-quiet",
                    "-p",
                    $buildDirectory
                )
            } else {
                Invoke-CheckedCommand -Command "clang-tidy" -Arguments @(
                    $file.FullName,
                    "--warnings-as-errors=*",
                    "-quiet",
                    "--extra-arg=-std=c11",
                    "--",
                    "-Iinclude",
                    "-I$generatedIncludeDirectory"
                )
            }
        }
    } else {
        Write-Host "Skipping clang-tidy: command not found."
    }
}

Invoke-CheckedCommand -Command "ctest" -Arguments @("--preset", $Preset, "--output-on-failure")
} finally {
    Pop-Location
}
