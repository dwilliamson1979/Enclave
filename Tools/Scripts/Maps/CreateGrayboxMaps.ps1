# CreateGrayboxMaps.ps1
# Regenerates the graybox match maps (TDM_TwinBases, DM_Pinwheel) from create_graybox_maps.py
# using the editor headlessly. Close the editor first.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\Maps\CreateGrayboxMaps.ps1

param(
    [string]$EngineRoot = "D:\UnrealEngine\Versions\UE_5.8"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$projectFile = Join-Path $projectRoot "Enclave.uproject"
$script = (Join-Path $PSScriptRoot "create_graybox_maps.py") -replace '\\', '/'
$log = Join-Path $projectRoot "Saved\Automation\CreateGrayboxMaps.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null

$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$process = Start-Process -FilePath $editor -PassThru -Wait -ArgumentList @(
    "`"$projectFile`"", "-ExecutePythonScript=`"$script`"", "-unattended", "-nosplash", "-nosound", "-abslog=`"$log`""
)
$saved = @(Select-String -Path $log -Pattern "Graybox map saved").Count
$errors = Select-String -Path $log -Pattern "LogPython: Error" | Where-Object { $_.Line -notmatch "editor_toolset|init_unreal" }
$errors | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
if ($saved -ne 2 -or $errors) { Write-Host "Map generation FAILED ($saved of 2 saved). Log: $log" -ForegroundColor Red; exit 1 }
Write-Host "Maps generated. Log: $log"
