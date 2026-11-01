# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$setup = (Resolve-Path 'installer-output/FaseGrooveArranger-Windows-Setup.exe').Path
$folder = Join-Path $env:RUNNER_TEMP 'Fase Groove Arranger installer probe'
$exe = Join-Path $folder 'Fase Groove Arranger.exe'
$classes = 'HKLM:\Software\Classes'
$projectClass = Join-Path $classes 'FaseGrooveArranger.Project'
$extension = Join-Path $classes '.fga'
$expected = (Get-FileHash 'build/FaseGrooveArranger_artefacts/Release/Fase Groove Arranger.exe').Hash
function Install-Probe {
    $log = Join-Path $env:RUNNER_TEMP 'arranger-install.log'
    $p = Start-Process -FilePath $setup -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', "/DIR=`"$folder`"", "/LOG=`"$log`"") -Wait -PassThru
    if ($p.ExitCode -ne 0) { Get-Content $log; throw "Setup failed: $($p.ExitCode)" }
    if ((Get-FileHash $exe).Hash -ne $expected) { throw 'Installed executable differs from tested build' }
    $command = (Get-Item (Join-Path $projectClass 'shell\open\command')).GetValue('')
    if ($command -ne "`"$exe`" `"%1`"") { throw 'Project open command loses executable/project quoting' }
    $icon = (Get-Item (Join-Path $projectClass 'DefaultIcon')).GetValue('')
    if ($icon -ne "`"$(Join-Path $folder 'FaseGrooveArranger.ico')`",0") { throw 'Project icon is not the installed Arranger icon' }
    if (-not (Test-Path (Join-Path $folder 'FaseGrooveArranger.ico'))) { throw 'Project icon file missing' }
    if ($null -eq (Get-Item (Join-Path $extension 'OpenWithProgids')).GetValue('FaseGrooveArranger.Project')) { throw 'Project Open With registration missing' }
    foreach ($file in @('unins000.exe', 'FaseGrooveArranger.install.ini', 'SOURCE.txt', 'docs/GUIDE.md', 'docs/GUIDA.md', 'LICENSE')) {
        if (-not (Test-Path (Join-Path $folder $file))) { throw "Missing installer file: $file" }
    }
}
Install-Probe
if ((Get-Item $extension).GetValue('') -ne 'FaseGrooveArranger.Project') { throw 'Fresh .fga association missing' }
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
if (Test-Path $projectClass) { throw 'Uninstall left the project class registered' }
if ((Test-Path $extension) -and (Get-Item $extension).GetValue('') -eq 'FaseGrooveArranger.Project') { throw 'Uninstall left a dead default association' }
# An existing app's default must survive both installation and uninstall.
New-Item -Path $extension -Force | Out-Null
Set-Item -Path $extension -Value 'InstallerProbe.OtherProject'
Install-Probe
if ((Get-Item $extension).GetValue('') -ne 'InstallerProbe.OtherProject') { throw 'Installer replaced an existing default app' }
$p = Start-Process -FilePath $uninstall -ArgumentList '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART' -Wait -PassThru
if ($p.ExitCode -ne 0 -or (Get-Item $extension).GetValue('') -ne 'InstallerProbe.OtherProject') { throw 'Uninstall removed another app association' }
Remove-ItemProperty -Path $extension -Name '(default)'
if (-not (Test-Path $personal)) { throw 'Uninstall removed user project' }
Remove-Item -LiteralPath $folder -Recurse -Force
'PASS Windows installer: new install, silent upgrade, exact native executable, bundled guides/source, project icon/open command, association preservation, uninstall and user project preservation'
