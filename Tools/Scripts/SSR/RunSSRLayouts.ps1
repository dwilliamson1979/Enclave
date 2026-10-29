# RunSSRLayouts.ps1
# Runs each SSR_TestRange lane with the clients it needs (dedicated server, project
# defaults: ServerAnchor, 60 Hz) and writes summary.md / summary.json.
# Generate the map first: Tools\Scripts\SSR\CreateSSRTestMap.ps1
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRLayouts.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSRLayouts.ps1 -Only crossing,crowd -JitterMs 15

param(
    [int]$DurationSeconds = 120,
    [int]$LagMs = 25,
    [int]$JitterMs = 0,
    [double]$LossPct = 0,
    [ValidateSet("Hybrid", "ServerAnchor", "EchoedStamp")][string]$TimingSource = "ServerAnchor",
    [string[]]$Only = @()
)

$ErrorActionPreference = "Stop"

# Scenario 2 = strafe, 4 = crouch-strafe, 5 = static target behind cover.
$layouts = @(
    @{ Name = "open";      Clients = 2; Scenario = 2 },
    @{ Name = "crossing";  Clients = 3; Scenario = 2 },
    @{ Name = "backstop";  Clients = 3; Scenario = 2 },
    @{ Name = "cover";     Clients = 2; Scenario = 2 },
    @{ Name = "halfcover"; Clients = 2; Scenario = 2 },
    @{ Name = "pillars";   Clients = 2; Scenario = 2 },
    @{ Name = "crowd";     Clients = 8; Scenario = 2 }
)
# "powershell -File" passes a comma list as one string.
$Only = @($Only | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
if ($Only.Count -gt 0) { $layouts = $layouts | Where-Object { $Only -contains $_.Name } }

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$suiteName = "layouts_{0}_{1}" -f (Get-Date -Format "yyyyMMdd-HHmmss"), $TimingSource
$suiteDirectory = Join-Path $projectRoot "Saved\Automation\SSRSession\$suiteName"
New-Item -ItemType Directory -Force $suiteDirectory | Out-Null
$common = @{ Dedicated = $true; Map = "/Game/Maps/Test/SSR_TestRange"; LagCompensation = "ServerSideRewind";
    TimingSource = $TimingSource; ServerLagMs = $LagMs; ClientLagMs = $LagMs; JitterMs = $JitterMs; LossPct = $LossPct }

# The first launch after a build hitches, so warm up first.
& (Join-Path $PSScriptRoot "RunSSRSession.ps1") @common -Layout open -DurationSeconds 20 -SessionName "$suiteName\_warmup" | Out-Null

$rows = @()
foreach ($l in $layouts)
{
    & (Join-Path $PSScriptRoot "RunSSRSession.ps1") @common -Layout $l.Name -Clients $l.Clients -Scenario $l.Scenario `
        -DurationSeconds $DurationSeconds -SweepStartMs -75 -SessionName "$suiteName\$($l.Name)" | Out-Host
    $dir = Join-Path $suiteDirectory $l.Name
    $r = python (Join-Path $PSScriptRoot "analyze_timing_sweep.py") (Join-Path $dir "server.log") --json | Out-String | ConvertFrom-Json
    $placed = Select-String -Path (Join-Path $dir "server.log") -Pattern "SSRTest LAYOUT layout=\S+ placed=(\d) bystandersInLayout=(\d+) bystandersParked=(\d+)" | Select-Object -Last 1
    $tick = Select-String -Path (Join-Path $dir "server.log") -Pattern "serverTickHz=([\d.]+)" | Select-Object -Last 1
    $fps = Select-String -Path (Join-Path $dir "client1.log") -Pattern "avgFps=([\d.]+) worstFrameMs=([\d.]+)" -ErrorAction SilentlyContinue | Select-Object -Last 1
    $rows += [pscustomobject]@{
        layout = $l.Name; clients = $l.Clients
        placed = if ($placed) { "{0} ({1} in lane, {2} parked)" -f $placed.Matches[0].Groups[1].Value, $placed.Matches[0].Groups[2].Value, $placed.Matches[0].Groups[3].Value } else { "no" }
        rttMedianMs = if ($r.rttMs) { $r.rttMs.median } else { $null }
        hitPct = $r.authoritative.clearHitPct; hitN = $r.authoritative.clearHitN
        missPct = $r.authoritative.clearMissPct; missN = $r.authoritative.clearMissN
        fallbacks = $r.authoritative.presentTimeFallbacks; otherTargetHits = $r.authoritative.otherTargetHits
        occludedPct = $r.authoritative.occludedPct; occludedN = $r.authoritative.occludedN
        serverTickHz = if ($tick) { [double]$tick.Matches[0].Groups[1].Value } else { $null }
        shooterFps = if ($fps) { [double]$fps.Matches[0].Groups[1].Value } else { $null }
    }
}

$rows | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $suiteDirectory "summary.json")
$markdown = @(
    "| Layout | Clients | Placed | RTT median | Clear hits % | Clear misses % | Behind cover, not hit % | Fallbacks | Hits on another character | Server Hz | Shooter FPS |",
    "|---|---|---|---|---|---|---|---|---|---|---|"
)
foreach ($r in $rows)
{
    $markdown += "| $($r.layout) | $($r.clients) | $($r.placed) | $($r.rttMedianMs) | $($r.hitPct) (n=$($r.hitN)) | $($r.missPct) (n=$($r.missN)) | $($r.occludedPct) (n=$($r.occludedN)) | $($r.fallbacks) | $($r.otherTargetHits) | $($r.serverTickHz) | $($r.shooterFps) |"
}
$markdown | Set-Content -Encoding utf8 (Join-Path $suiteDirectory "summary.md")
$markdown | Write-Host
Write-Host "Summary: $suiteDirectory\summary.md"
