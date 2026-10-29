# CreateSSRTestMap.ps1
# Regenerates /Game/Maps/Test/SSR_TestRange from create_ssr_test_map.py using the editor headlessly.
# Close the editor first if it has the map open.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\CreateSSRTestMap.ps1

param(
    [string]$EngineRoot = "D:\UnrealEngine\Versions\UE_5.8"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$projectFile = Join-Path $projectRoot "Enclave.uproject"
$script = (Join-Path $PSScriptRoot "create_ssr_test_map.py") -replace '\\', '/'
$log = Join-Path $projectRoot "Saved\Automation\CreateSSRTestMap.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null

$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$process = Start-Process -FilePath $editor -PassThru -Wait -ArgumentList @(
    "`"$projectFile`"", "-ExecutePythonScript=`"$script`"", "-unattended", "-nosplash", "-nosound", "-abslog=`"$log`""
)
$saved = Select-String -Path $log -Pattern "SSR test map saved" -Quiet
$errors = Select-String -Path $log -Pattern "LogPython: Error" | Where-Object { $_.Line -notmatch "editor_toolset|init_unreal" }
$errors | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
if (-not $saved -or $errors) { Write-Host "Map generation FAILED. Log: $log" -ForegroundColor Red; exit 1 }
Write-Host "Map generated. Log: $log"
