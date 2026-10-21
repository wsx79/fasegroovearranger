# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$setup = (Resolve-Path 'installer-output/FaseGrooveArranger-Windows-Setup.exe').Path
$folder = Join-Path $env:RUNNER_TEMP 'Fase Groove Arranger installer probe'
$exe = Join-Path $folder 'Fase Groove Arranger.exe'
$expected = (Get-FileHash 'build/FaseGrooveArranger_artefacts/Release/Fase Groove Arranger.exe').Hash
function Install-Probe {
    $log = Join-Path $env:RUNNER_TEMP 'arranger-install.log'
    $p = Start-Process -FilePath $setup -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/DIR=`"$folder`"", "/LOG=`"$log`"") -Wait -PassThru
    if ($p.ExitCode -ne 0) { Get-Content $log; throw "Setup failed: $($p.ExitCode)" }
    if ((Get-FileHash $exe).Hash -ne $expected) { throw 'Installed executable differs from tested build' }
    foreach ($file in @('unins000.exe', 'FaseGrooveArranger.install.ini', 'SOURCE.txt', 'docs/GUIDE.md', 'docs/GUIDA.md', 'LICENSE')) {
        if (-not (Test-Path (Join-Path $folder $file))) { throw "Missing installer file: $file" }
    }
}
Install-Probe
$personal = Join-Path $folder 'my-song.fga'
Set-Content -LiteralPath $personal -Value 'user project must remain'
# Simulate an older binary at the same installation path, then run the same
# silent upgrade entry point used by the updater, without relaunching in CI.
Set-Content -LiteralPath $exe -Value 'old application'
Install-Probe
if ((Get-Content $personal) -ne 'user project must remain') { throw 'Upgrade changed user project' }
$uninstall = Join-Path $folder 'unins000.exe'
$p = Start-Process -FilePath $uninstall -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -Wait -PassThru
if ($p.ExitCode -ne 0 -or (Test-Path $exe)) { throw 'Uninstall failed' }
if (-not (Test-Path $personal)) { throw 'Uninstall removed user project' }
Remove-Item -LiteralPath $folder -Recurse -Force
'PASS Windows installer: new install, silent upgrade, exact native executable, bundled guides/source, uninstall and user project preservation'
