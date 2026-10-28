# RunSSRSession.ps1
# Unattended three-process SSR shadow session: listen server + two remote clients
# (client 1 = shooter, client 2 = strafing target), with symmetric packet lag.
# Requires a Development Editor build (debug code compiled in). Opens three small game windows.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -RttMs 100
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -RttMs 50 -SweepStartMs -100 -SweepStepMs 25
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -LagCompensation ServerSideRewind -TimingSource Hybrid
#
# The server log is scored with analyze_timing_sweep.py at the end.

param(
    [int]$RttMs = 100,
    [int]$Scenario = 2,
    [int]$DurationSeconds = 120,
    [double]$SweepStepMs = 25,
    [double]$SweepStartMs = 0,
    [ValidateSet("Disabled", "ServerSideRewind")][string]$LagCompensation = "Disabled",
    [ValidateSet("Hybrid", "ServerAnchor", "EchoedStamp")][string]$TimingSource = "Hybrid",
    [string]$EngineRoot = "D:\UnrealEngine\Versions\UE_5.8"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$projectFile = Join-Path $projectRoot "Enclave.uproject"
$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$sessionDirectory = Join-Path $projectRoot ("Saved\Automation\SSRSession\{0}_rtt{1}_{2}_{3}" -f $timestamp, $RttMs, $LagCompensation, $TimingSource)
New-Item -ItemType Directory -Force $sessionDirectory | Out-Null

# Measured: a -PktLag on every process shows up about twice in PlayerState ping
# (PktLag=50 everywhere gave ~200 ms), so use a quarter of the target RTT.
# Always check the measured rttMs the analyzer prints, not this setting.
$lagMs = [int]($RttMs / 4)
$common = @("-game", "-windowed", "-ResX=640", "-ResY=360", "-nosound", "-nosplash", "-PktLag=$lagMs")

$serverCmds = @(
    "Enclave.Debug.SSRTimingSweep 1",
    "Enclave.Debug.SSRTimingSweepStepMs $SweepStepMs",
    "Enclave.Debug.SSRTimingSweepStartMs $SweepStartMs",
    "Enclave.Debug.SSRSelection 1",
    "Enclave.Debug.SSRTest.DurationSeconds $DurationSeconds",
    "Enclave.Debug.SSRTest.AutoQuit 1",
    "Enclave.Debug.SSRTest.AutoStart $Scenario"
) -join ","
$serverLog = Join-Path $sessionDirectory "server.log"
# The project path must come first, then the map URL / server address.
$serverArgs = @("`"$projectFile`"", "/Game/Maps/EnclaveMap?listen?WeaponHitDetectionMode=Capsule?LagCompensation=${LagCompensation}?SSRTimingSource=${TimingSource}") + $common +
    @("-WinX=0", "-WinY=40", "-ExecCmds=`"$serverCmds`"", "-abslog=`"$serverLog`"")

Write-Host "== SSR session rtt=${RttMs}ms scenario=$Scenario duration=${DurationSeconds}s lagComp=$LagCompensation timing=$TimingSource -> $sessionDirectory" -ForegroundColor Cyan
$server = Start-Process -FilePath $editor -ArgumentList $serverArgs -PassThru
Start-Sleep -Seconds 20

$clients = @()
foreach ($index in 1, 2)
{
    # Join order decides roles: the lower PlayerId (first client) is the shooter.
    $clientLog = Join-Path $sessionDirectory "client$index.log"
    $clientArgs = @("`"$projectFile`"", "127.0.0.1") + $common +
        @("-WinX=$(660 * $index)", "-WinY=40", "-abslog=`"$clientLog`"")
    $clients += Start-Process -FilePath $editor -ArgumentList $clientArgs -PassThru
    Start-Sleep -Seconds 8
}

# The server quits itself when the auto-started session ends; allow generous slack.
$deadline = (Get-Date).AddSeconds($DurationSeconds + 120)
while (-not $server.HasExited -and (Get-Date) -lt $deadline)
{
    Start-Sleep -Seconds 5
}
foreach ($process in @($server) + $clients)
{
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}

$started = Select-String -Path $serverLog -Pattern "SSRTest (START|AUTOSTART|STOP|START rejected)" | ForEach-Object { $_.Line.Trim() }
$started | ForEach-Object { Write-Host $_ }
python (Join-Path $PSScriptRoot "analyze_timing_sweep.py") $serverLog
