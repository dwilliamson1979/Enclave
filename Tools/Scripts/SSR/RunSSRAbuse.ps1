# RunSSRAbuse.ps1
# Milestone 3 abuse cases on a dedicated server (mid network condition by default):
# a cheating shooter falsifies timing evidence or fakes lag; we check what the
# server accepts and how hit registration holds up. Also a stamp bandwidth A/B.
# Writes summary.md / summary.json next to the sessions.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRAbuse.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRAbuse.ps1 -Only backtrack_100,forged

param(
    [int]$DurationSeconds = 90,
    [int]$LagMs = 25,
    [string[]]$Only = @()
)

$ErrorActionPreference = "Stop"

$cases = @(
    @{ Name = "baseline";      Timing = "Hybrid";       Shooter = "";                                                       ShooterLagMs = -1;  Server = "" },
    @{ Name = "backtrack_50";  Timing = "Hybrid";       Shooter = "Enclave.Debug.SSRTimingFault 1,Enclave.Debug.SSRTimingFaultMs 50";  ShooterLagMs = -1;  Server = "" },
    @{ Name = "backtrack_100"; Timing = "Hybrid";       Shooter = "Enclave.Debug.SSRTimingFault 1,Enclave.Debug.SSRTimingFaultMs 100"; ShooterLagMs = -1;  Server = "" },
    @{ Name = "backtrack_150"; Timing = "Hybrid";       Shooter = "Enclave.Debug.SSRTimingFault 1,Enclave.Debug.SSRTimingFaultMs 150"; ShooterLagMs = -1;  Server = "" },
    @{ Name = "forged";        Timing = "Hybrid";       Shooter = "Enclave.Debug.SSRTimingFault 2";                          ShooterLagMs = -1;  Server = "" },
    @{ Name = "future_50";     Timing = "Hybrid";       Shooter = "Enclave.Debug.SSRTimingFault 3,Enclave.Debug.SSRTimingFaultMs 50";  ShooterLagMs = -1;  Server = "" },
    @{ Name = "fake_lag_100";  Timing = "Hybrid";       Shooter = "";                                                       ShooterLagMs = 100; Server = "" },
    @{ Name = "bw_stamps_on";  Timing = "ServerAnchor"; Shooter = "";                                                       ShooterLagMs = -1;  Server = "Enclave.Debug.SSRReplicateStamps 1" },
    @{ Name = "bw_stamps_off"; Timing = "ServerAnchor"; Shooter = "";                                                       ShooterLagMs = -1;  Server = "Enclave.Debug.SSRReplicateStamps 0" }
)
# "powershell -File" passes a comma list as one string.
$Only = @($Only | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
if ($Only.Count -gt 0) { $cases = $cases | Where-Object { $Only -contains $_.Name } }

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$suiteName = "abuse_{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss")
$suiteDirectory = Join-Path $projectRoot "Saved\Automation\SSRSession\$suiteName"
New-Item -ItemType Directory -Force $suiteDirectory | Out-Null

# The first launch after a build hitches (measured: it skewed one matrix condition), so warm up first.
& (Join-Path $PSScriptRoot "RunSSRSession.ps1") -Dedicated -DurationSeconds 20 -SessionName "$suiteName\_warmup" | Out-Null

$rows = @()
foreach ($case in $cases)
{
    & (Join-Path $PSScriptRoot "RunSSRSession.ps1") -Dedicated -LagCompensation ServerSideRewind -TimingSource $case.Timing `
        -ServerLagMs $LagMs -ClientLagMs $LagMs -ShooterLagMs $case.ShooterLagMs -DurationSeconds $DurationSeconds `
        -SweepStartMs -75 -ExtraShooterCmds $case.Shooter -ExtraServerCmds $case.Server `
        -SessionName "$suiteName\$($case.Name)" | Out-Host

    $serverLog = Join-Path $suiteDirectory "$($case.Name)\server.log"
    $r = python (Join-Path $PSScriptRoot "analyze_timing_sweep.py") $serverLog --json | Out-String | ConvertFrom-Json
    $sa = $r.stampAcceptance
    $outPerClient = ($r.netstats | Measure-Object -Property serverOutBytesPerSec -Average).Average
    $fps = Select-String -Path (Join-Path $suiteDirectory "$($case.Name)\client1.log") -Pattern "avgFps=([\d.]+) worstFrameMs=([\d.]+)" -ErrorAction SilentlyContinue | Select-Object -Last 1
    $rows += [pscustomobject]@{
        case = $case.Name; timing = $case.Timing; rttMedianMs = if ($r.rttMs) { $r.rttMs.median } else { $null }
        authHitPct = $r.authoritative.clearHitPct; authMissPct = $r.authoritative.clearMissPct
        fallbacks = $r.authoritative.presentTimeFallbacks
        stampsSent = $sa.sent; stampsVerified = $sa.verified; stampsUsed = $sa.used; stampsRejected = $sa.rejected
        usedDeltaMinMs = if ($sa.usedDeltaMs) { $sa.usedDeltaMs.min } else { $null }
        usedDeltaMaxMs = if ($sa.usedDeltaMs) { $sa.usedDeltaMs.max } else { $null }
        serverOutBytesPerClientPerSec = if ($outPerClient) { [math]::Round($outPerClient) } else { $null }
        shooterFps = if ($fps) { "{0} (worst {1} ms)" -f $fps.Matches[0].Groups[1].Value, $fps.Matches[0].Groups[2].Value } else { $null }
    }
}

$rows | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $suiteDirectory "summary.json")
$markdown = @(
    "| Case | Timing | RTT median | Hit % | Miss % | Fallbacks | Stamps sent/verified/used/rejected | Used stamp age - anchor age (ms) | Server out B/s per client | Shooter FPS |",
    "|---|---|---|---|---|---|---|---|---|---|"
)
foreach ($r in $rows)
{
    $markdown += "| $($r.case) | $($r.timing) | $($r.rttMedianMs) | $($r.authHitPct) | $($r.authMissPct) | $($r.fallbacks) | $($r.stampsSent)/$($r.stampsVerified)/$($r.stampsUsed)/$($r.stampsRejected) | $($r.usedDeltaMinMs)..$($r.usedDeltaMaxMs) | $($r.serverOutBytesPerClientPerSec) | $($r.shooterFps) |"
}
$markdown | Set-Content -Encoding utf8 (Join-Path $suiteDirectory "summary.md")
$markdown | Write-Host
Write-Host "Summary: $suiteDirectory\summary.md"
