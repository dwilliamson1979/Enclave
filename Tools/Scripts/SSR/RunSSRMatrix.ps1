# RunSSRMatrix.ps1
# Runs RunSSRSession.ps1 across a matrix of network conditions on a dedicated server
# with authoritative SSR, then writes summary.md / summary.json next to the sessions.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRMatrix.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRMatrix.ps1 -Only mid,mid_jitter -TimingSource ServerAnchor
#
# Each condition takes about DurationSeconds + 90 s. Lag values are per-process outgoing lag.

param(
    [int]$DurationSeconds = 120,
    [ValidateSet("Hybrid", "ServerAnchor", "EchoedStamp")][string]$TimingSource = "ServerAnchor",
    [double]$SweepStartMs = -75,
    [double]$SweepStepMs = 25,
    [int]$ServerTickRate = 0,
    [string[]]$Only = @()
)

$ErrorActionPreference = "Stop"

$conditions = @(
    @{ Name = "low";        ServerLagMs = 5;  ClientLagMs = 5;  JitterMs = 0;  LossPct = 0 },
    @{ Name = "mid";        ServerLagMs = 25; ClientLagMs = 25; JitterMs = 0;  LossPct = 0 },
    @{ Name = "high";       ServerLagMs = 50; ClientLagMs = 50; JitterMs = 0;  LossPct = 0 },
    @{ Name = "mid_jitter"; ServerLagMs = 25; ClientLagMs = 25; JitterMs = 15; LossPct = 0 },
    @{ Name = "mid_loss";   ServerLagMs = 25; ClientLagMs = 25; JitterMs = 0;  LossPct = 2 },
    @{ Name = "asym_down";  ServerLagMs = 45; ClientLagMs = 5;  JitterMs = 0;  LossPct = 0 },
    @{ Name = "asym_up";    ServerLagMs = 5;  ClientLagMs = 45; JitterMs = 0;  LossPct = 0 },
    # A slow-rendering shooter (target stays at the default cap).
    @{ Name = "mid_shooter30fps"; ServerLagMs = 25; ClientLagMs = 25; JitterMs = 0; LossPct = 0; ShooterCmds = "t.MaxFPS 30" }
)
# "powershell -File" passes a comma list as one string.
$Only = @($Only | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
if ($Only.Count -gt 0) { $conditions = $conditions | Where-Object { $Only -contains $_.Name } }

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$tickLabel = if ($ServerTickRate -gt 0) { "${ServerTickRate}Hz" } else { "defaultHz" }
$matrixName = "matrix_{0}_{1}_{2}" -f (Get-Date -Format "yyyyMMdd-HHmmss"), $TimingSource, $tickLabel
$matrixDirectory = Join-Path $projectRoot "Saved\Automation\SSRSession\$matrixName"
New-Item -ItemType Directory -Force $matrixDirectory | Out-Null

# The first launch after a build hitches (measured: it skewed the first condition), so warm up first.
& (Join-Path $PSScriptRoot "RunSSRSession.ps1") -Dedicated -ServerTickRate $ServerTickRate -DurationSeconds 20 -SessionName "$matrixName\_warmup" | Out-Null

$rows = @()
foreach ($condition in $conditions)
{
    $sessionName = "$matrixName\$($condition.Name)"
    & (Join-Path $PSScriptRoot "RunSSRSession.ps1") -Dedicated -LagCompensation ServerSideRewind -TimingSource $TimingSource `
        -ServerLagMs $condition.ServerLagMs -ClientLagMs $condition.ClientLagMs -JitterMs $condition.JitterMs `
        -LossPct $condition.LossPct -DurationSeconds $DurationSeconds -SweepStartMs $SweepStartMs `
        -SweepStepMs $SweepStepMs -ServerTickRate $ServerTickRate -ExtraShooterCmds "$($condition.ShooterCmds)" `
        -SessionName $sessionName | Out-Host

    $serverLog = Join-Path $matrixDirectory "$($condition.Name)\server.log"
    $result = python (Join-Path $PSScriptRoot "analyze_timing_sweep.py") $serverLog --json | Out-String | ConvertFrom-Json
    $tick = Select-String -Path $serverLog -Pattern "serverTickHz=([\d.]+)" | Select-Object -Last 1
    $cpuLine = Get-Content (Join-Path $matrixDirectory "$($condition.Name)\server_cpu.txt") -ErrorAction SilentlyContinue
    $outPerClient = ($result.netstats | Measure-Object -Property serverOutBytesPerSec -Average).Average
    $fps = Select-String -Path (Join-Path $matrixDirectory "$($condition.Name)\client1.log") -Pattern "avgFps=([\d.]+) worstFrameMs=([\d.]+)" -ErrorAction SilentlyContinue | Select-Object -Last 1
    $best = $result.serverAnchor | Where-Object { $_.combinedPct -ne $null } | Sort-Object combinedPct -Descending | Select-Object -First 1
    $rows += [pscustomobject]@{
        condition = $condition.Name
        serverLagMs = $condition.ServerLagMs; clientLagMs = $condition.ClientLagMs
        jitterMs = $condition.JitterMs; lossPct = $condition.LossPct
        rttMedianMs = if ($result.rttMs) { $result.rttMs.median } else { $null }
        labeled = $result.labeledShots
        authHitPct = $result.authoritative.clearHitPct; authMissPct = $result.authoritative.clearMissPct
        authHitN = $result.authoritative.clearHitN; authMissN = $result.authoritative.clearMissN
        fallbacks = $result.authoritative.presentTimeFallbacks
        stampUsed = $result.authoritative.stampUsed; stampRejected = $result.authoritative.stampRejected
        echoHitPct = $result.echoedStamp.clearHitPct; echoMissPct = $result.echoedStamp.clearMissPct
        bestAnchorOffsetMs = $best.delayMs; bestAnchorCombinedPct = $best.combinedPct
        serverTickHz = if ($tick) { [double]$tick.Matches[0].Groups[1].Value } else { $null }
        serverCpuCores = if ($cpuLine -match "serverCpuCores=([\d.]+)") { [double]$Matches[1] } else { $null }
        serverOutBytesPerClientPerSec = if ($outPerClient) { [math]::Round($outPerClient) } else { $null }
        shooterFps = if ($fps) { "{0} (worst {1} ms)" -f $fps.Matches[0].Groups[1].Value, $fps.Matches[0].Groups[2].Value } else { $null }
        sweep = ($result.serverAnchor | ForEach-Object { "{0}:{1}" -f $_.delayMs, $_.combinedPct }) -join " "
    }
}

$rows | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $matrixDirectory "summary.json")
$markdown = @(
    "| Condition | Lag srv/cli ms | Jitter | Loss | RTT median | $TimingSource hit % | miss % | Fallbacks | Stamps used/rejected | Echo hit/miss % | Server Hz | Server CPU cores | Out B/s/client | Shooter FPS | Anchor sweep (d:combined %) |",
    "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"
)
foreach ($r in $rows)
{
    $markdown += "| $($r.condition) | $($r.serverLagMs)/$($r.clientLagMs) | +/-$($r.jitterMs) | $($r.lossPct)% | $($r.rttMedianMs) | $($r.authHitPct) (n=$($r.authHitN)) | $($r.authMissPct) (n=$($r.authMissN)) | $($r.fallbacks) | $($r.stampUsed)/$($r.stampRejected) | $($r.echoHitPct)/$($r.echoMissPct) | $($r.serverTickHz) | $($r.serverCpuCores) | $($r.serverOutBytesPerClientPerSec) | $($r.shooterFps) | $($r.sweep) |"
}
$markdown | Set-Content -Encoding utf8 (Join-Path $matrixDirectory "summary.md")
$markdown | Write-Host
Write-Host "Summary: $matrixDirectory\summary.md"
