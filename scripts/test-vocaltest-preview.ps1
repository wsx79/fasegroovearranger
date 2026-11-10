# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
# Native Windows build/test recipe. NOT RUN on Windows in the Linux workspace.
# Never packages, installs dependencies, pushes or publishes.
param(
    [Parameter(Mandatory=$true)][string]$GuideHelper
)
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT' -or [IntPtr]::Size -ne 8 -or $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
    throw 'Run in a native x64 Visual Studio 2022 Developer PowerShell on Windows.'
}
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command exited with code $LASTEXITCODE" }
}
$branch = & git branch --show-current
if ($LASTEXITCODE -ne 0 -or $branch -ne 'Vocaltest') { throw 'Only branch Vocaltest is permitted.' }
$dirty = & git status --porcelain
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Use a clean locally committed Vocaltest snapshot; no push is required.' }
$sourceSha = & git rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot record source revision.' }
foreach ($sha in @('358adb3', '8cc5487', '3f70446')) {
    Invoke-Checked git @('merge-base', '--is-ancestor', $sha, 'HEAD')
}
$env:FGA_ECANTORIX = (Resolve-Path -LiteralPath $GuideHelper).Path
if (-not $env:FGA_ECANTORIX.EndsWith('.pl') -or
    -not (Get-Content -LiteralPath $env:FGA_ECANTORIX -Raw).Contains('FGA_PORTABLE_GUIDE_1')) {
    throw 'Use the trusted helper prepared with helpers/ecantorix/prepare.pl.'
}
# Process regression tests also invoke perl by name, even if FGA_PERL_BIN is set.
foreach ($tool in @('git', 'cmake', 'ctest', 'perl', 'dumpbin')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing native build/test tool: $tool" }
}
$build = Join-Path $root 'build\vocaltest-preview'
$evidence = Join-Path $build ('validation-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
Start-Transcript -Path (Join-Path $evidence 'build-and-tests.log') | Out-Null
try {
    Invoke-Checked cmake @('-S', '.', '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
        '-DFGA_BUILD_APP=ON', '-DFGA_BUILD_TESTS=ON', '-DFGA_SELF_TESTS=ON')
    Invoke-Checked cmake @('--build', $build, '--config', 'Release', '--parallel', '2')
    Invoke-Checked ctest @('--test-dir', $build, '-C', 'Release', '--output-on-failure', '-V')
    $engineLog = Get-Content -LiteralPath (Join-Path $build 'Testing\Temporary\LastTest.log') -Raw
    if (-not $engineLog.Contains('PASS real offline eCantorix rendering')) {
        throw 'Actual guide rendering was not exercised by CTest; a skipped render is not a PASS.'
    }
    $app = Join-Path $build 'FaseGrooveArranger_artefacts\Release\Fase Groove Arranger.exe'
    # Check actual PE machine/format, rather than trusting the directory name.
    $bytes = [System.IO.File]::ReadAllBytes($app)
    if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5a4d) { throw 'Not a PE executable.' }
    $pe = [BitConverter]::ToInt32($bytes, 60)
    if ($pe -lt 0 -or $pe -gt $bytes.Length - 26 -or
        [BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664 -or
        [BitConverter]::ToUInt16($bytes, $pe + 24) -ne 0x20b) { throw 'Expected native x64 PE32+.' }
    Invoke-Checked dumpbin @('/dependents', $app)
    foreach ($test in @('sample-import', 'vocal-studio', 'ui')) {
        $folder = Join-Path $evidence $test
        $process = Start-Process -FilePath $app -ArgumentList @("--${test}-test", ('"' + $folder + '"')) -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "$test test failed: $($process.ExitCode)" }
        $report = Get-Content -LiteralPath (Join-Path $folder "$test.txt") -Raw
        if (-not $report.StartsWith('PASS ') -or $report.Contains('FAIL ') -or $report.Contains('NOT RUN')) {
            throw "$test contains failed or skipped checks."
        }
    }
    $sha = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record source revision.' }
    $dirty = & git status --porcelain
    if ($LASTEXITCODE -ne 0 -or $dirty -or $sha -ne $sourceSha) { throw 'Source changed during native validation; repeat on one frozen snapshot.' }
    @('Vocaltest Preview - native automated checks PASS; manual/release gates remain open.',
      "Source HEAD: $sha", "Binary SHA256: $((Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash)",
      'Review DLL provenance, complete corresponding source, archive licences and manual listening before packaging.') |
        Set-Content -LiteralPath (Join-Path $evidence 'preview-validation.txt') -Encoding UTF8
    Write-Output "Native automated checks PASS. Evidence: $evidence"
} finally {
    Stop-Transcript | Out-Null
}
