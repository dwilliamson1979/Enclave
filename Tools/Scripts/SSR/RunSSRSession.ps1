# RunSSRSession.ps1
# Unattended three-process SSR session: a server plus two remote clients
# (client 1 = shooter, client 2 = strafing target) under emulated network conditions.
# Requires a Development Editor build (debug code compiled in). Opens two or three small game windows.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -RttMs 100
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -RttMs 50 -SweepStartMs -100 -SweepStepMs 25
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -LagCompensation ServerSideRewind -TimingSource Hybrid
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRSession.ps1 -Dedicated -ServerLagMs 40 -ClientLagMs 5 -JitterMs 10 -LossPct 2
#
# Network emulation is outgoing-only per process (-PktLag, -PktLagVariance, -PktLoss):
#   server -> clients uses ServerLagMs; clients -> server uses ClientLagMs
#   (ShooterLagMs overrides it for the shooter only, for fake-lag tests).
# -RttMs is a convenience that sets both to RttMs/4 (measured: PktLag on every
# process shows up about twice in PlayerState ping). Always report the measured
# rttMs the analyzer prints, not these settings.
#
# The server log is scored with analyze_timing_sweep.py at the end.

param(
    [int]$RttMs = 100,
    [int]$ServerLagMs = -1,
    [int]$ClientLagMs = -1,
    [int]$ShooterLagMs = -1,
    [int]$JitterMs = 0,
    [double]$LossPct = 0,
    [switch]$Dedicated,
    [int]$ServerTickRate = 0,
    [int]$Scenario = 2,
    [int]$DurationSeconds = 120,
    [double]$SweepStepMs = 25,
    [double]$SweepStartMs = 0,
    [ValidateSet("Disabled", "ServerSideRewind")][string]$LagCompensation = "Disabled",
    [ValidateSet("Hybrid", "ServerAnchor", "EchoedStamp")][string]$TimingSource = "ServerAnchor",
    [string]$ExtraServerCmds = "",
    [string]$ExtraShooterCmds = "",
    # Cap client frame rate (0 = uncapped). Uncapped, the focused client took the shared
    # GPU and the background shooter fell to ~30 FPS, which skewed scoring.
    [int]$ClientMaxFps = 60,
    [ValidateSet("Normal", "AboveNormal", "High")][string]$ClientPriority = "Normal",
    [string]$SessionName = "",
    [string]$EngineRoot = "D:\UnrealEngine\Versions\UE_5.8"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$projectFile = Join-Path $projectRoot "Enclave.uproject"
$editor = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
if ($SessionName -eq "")
{
    $SessionName = "{0}_rtt{1}_{2}_{3}" -f $timestamp, $RttMs, $LagCompensation, $TimingSource
}
$sessionDirectory = Join-Path $projectRoot "Saved\Automation\SSRSession\$SessionName"
New-Item -ItemType Directory -Force $sessionDirectory | Out-Null

if ($ServerLagMs -lt 0) { $ServerLagMs = [int]($RttMs / 4) }
if ($ClientLagMs -lt 0) { $ClientLagMs = [int]($RttMs / 4) }
# Fake-lag abuse case: only the shooter's uplink is slowed.
if ($ShooterLagMs -lt 0) { $ShooterLagMs = $ClientLagMs }

function Get-NetEmulationArgs([int]$LagMs)
{
    $netArgs = @()
    if ($LagMs -gt 0) { $netArgs += "-PktLag=$LagMs" }
    if ($JitterMs -gt 0) { $netArgs += "-PktLagVariance=$JitterMs" }
    if ($LossPct -gt 0) { $netArgs += "-PktLoss=$LossPct" }
    return $netArgs
}
$window = @("-game", "-windowed", "-ResX=640", "-ResY=360", "-nosound", "-nosplash")

$serverCmds = @(
    "Enclave.Debug.SSRTimingSweep 1",
    "Enclave.Debug.SSRTimingSweepStepMs $SweepStepMs",
    "Enclave.Debug.SSRTimingSweepStartMs $SweepStartMs",
    "Enclave.Debug.SSRSelection 1",
    "Enclave.Debug.SSRTest.DurationSeconds $DurationSeconds",
    "Enclave.Debug.SSRTest.AutoQuit 1",
    "Enclave.Debug.SSRTest.AutoStart $Scenario"
)
if ($ExtraServerCmds -ne "") { $serverCmds += $ExtraServerCmds.Split(",") }
$serverCmds = $serverCmds -join ","
$serverLog = Join-Path $sessionDirectory "server.log"
$mapUrl = "/Game/Maps/EnclaveMap?WeaponHitDetectionMode=Capsule?LagCompensation=${LagCompensation}?SSRTimingSource=${TimingSource}"
# The project path must come first, then the map URL / server address.
if ($Dedicated)
{
    $serverArgs = @("`"$projectFile`"", $mapUrl, "-server", "-nosound", "-unattended")
}
else
{
    $serverArgs = @("`"$projectFile`"", "$mapUrl?listen") + $window + @("-WinX=0", "-WinY=40")
}
$serverArgs += @(Get-NetEmulationArgs $ServerLagMs) + @("-ExecCmds=`"$serverCmds`"", "-abslog=`"$serverLog`"")
# 0 keeps the project default (Config/DefaultEngine.ini: 60 Hz).
if ($ServerTickRate -gt 0)
{
    $serverArgs += "-ini:Engine:[/Script/OnlineSubsystemUtils.IpNetDriver]:NetServerMaxTickRate=$ServerTickRate"
}

$serverKind = if ($Dedicated) { "dedicated" } else { "listen" }
if ($ServerTickRate -gt 0) { $serverKind += " ${ServerTickRate}Hz" }
Write-Host ("== SSR session {0} serverLag={1}ms clientLag={2}ms jitter=+/-{3}ms loss={4}% scenario={5} duration={6}s lagComp={7} timing={8} -> {9}" -f
    $serverKind, $ServerLagMs, $ClientLagMs, $JitterMs, $LossPct, $Scenario, $DurationSeconds, $LagCompensation, $TimingSource, $sessionDirectory) -ForegroundColor Cyan
$server = Start-Process -FilePath $editor -ArgumentList $serverArgs -PassThru
Start-Sleep -Seconds 20

$clients = @()
foreach ($index in 1, 2)
{
    # Join order decides roles: the lower PlayerId (first client) is the shooter.
    $clientLog = Join-Path $sessionDirectory "client$index.log"
    $lag = if ($index -eq 1) { $ShooterLagMs } else { $ClientLagMs }
    $clientArgs = @("`"$projectFile`"", "127.0.0.1") + $window + @(Get-NetEmulationArgs $lag) +
        @("-WinX=$(660 * $index)", "-WinY=40", "-abslog=`"$clientLog`"")
    $clientCmds = @()
    if ($ClientMaxFps -gt 0) { $clientCmds += "t.MaxFPS $ClientMaxFps" }
    if ($index -eq 1 -and $ExtraShooterCmds -ne "") { $clientCmds += $ExtraShooterCmds.Split(",") }
    if ($clientCmds.Count -gt 0) { $clientArgs += "-ExecCmds=`"$($clientCmds -join ",")`"" }
    $client = Start-Process -FilePath $editor -ArgumentList $clientArgs -PassThru
    if ($ClientPriority -ne "Normal") { try { $client.PriorityClass = $ClientPriority } catch { } }
    $clients += $client
    Start-Sleep -Seconds 8
}

# The server quits itself when the auto-started session ends; allow generous slack.
$deadline = (Get-Date).AddSeconds($DurationSeconds + 120)
# Server CPU over the whole run: total processor time / wall time (1.0 = one full core).
$serverStart = Get-Date
$serverCpu = [TimeSpan]::Zero
while (-not $server.HasExited -and (Get-Date) -lt $deadline)
{
    try { $serverCpu = $server.TotalProcessorTime } catch { }
    Start-Sleep -Seconds 5
}
$wallSeconds = ((Get-Date) - $serverStart).TotalSeconds
$cpuCores = if ($wallSeconds -gt 0) { [math]::Round($serverCpu.TotalSeconds / $wallSeconds, 3) } else { 0 }
"serverCpuSeconds=$([math]::Round($serverCpu.TotalSeconds, 1)) wallSeconds=$([math]::Round($wallSeconds, 1)) serverCpuCores=$cpuCores" |
    Set-Content -Encoding ascii (Join-Path $sessionDirectory "server_cpu.txt")
Write-Host "Server CPU: $cpuCores cores average (whole run, includes startup and map load)"
foreach ($process in @($server) + $clients)
{
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}

$started = Select-String -Path $serverLog -Pattern "SSRTest (START|AUTOSTART|STOP|START rejected)" | ForEach-Object { $_.Line.Trim() }
$started | ForEach-Object { Write-Host $_ }
python (Join-Path $PSScriptRoot "analyze_timing_sweep.py") $serverLog
