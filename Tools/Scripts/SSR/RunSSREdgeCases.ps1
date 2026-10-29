# RunSSREdgeCases.ps1
# Milestone 3 edge cases on the SSR test range "open" lane (dedicated server, project
# defaults, mid condition): crouch spam, periodic target teleports, and a shooter
# lag switch. Writes summary.md / summary.json.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSREdgeCases.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\SSR\RunSSREdgeCases.ps1 -Only lag_switch

param(
    [int]$DurationSeconds = 120,
    [int]$LagMs = 25,
    [string[]]$Only = @()
)

$ErrorActionPreference = "Stop"

$cases = @(
    @{ Name = "baseline";    Server = ""; Shooter = ""; Target = "" },
    @{ Name = "crouch_spam"; Server = ""; Shooter = ""; Target = "Enclave.Debug.SSRTest.CrouchTogglePeriod 0.7" },
    @{ Name = "teleport";    Server = "Enclave.Debug.SSRTest.TeleportPeriod 2"; Shooter = ""; Target = "" },
    @{ Name = "lag_switch";  Server = ""; Target = "";
       Shooter = "Enclave.Debug.SSRTest.LagSwitchPeriod 3,Enclave.Debug.SSRTest.LagSwitchBurstMs 400,Enclave.Debug.SSRTest.LagSwitchBurstSeconds 0.5,Enclave.Debug.SSRTest.LagSwitchBaseMs $LagMs" }
)
# "powershell -File" passes a comma list as one string.
$Only = @($Only | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
if ($Only.Count -gt 0) { $cases = $cases | Where-Object { $Only -contains $_.Name } }

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..")).TrimEnd('\', '/')
$suiteName = "edge_{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss")
$suiteDirectory = Join-Path $projectRoot "Saved\Automation\SSRSession\$suiteName"
New-Item -ItemType Directory -Force $suiteDirectory | Out-Null
$common = @{ Dedicated = $true; Map = "/Game/Maps/Test/SSR_TestRange"; Layout = "open"; LagCompensation = "ServerSideRewind";
    ServerLagMs = $LagMs; ClientLagMs = $LagMs }

# The first launch after a build hitches, so warm up first.
& (Join-Path $PSScriptRoot "RunSSRSession.ps1") @common -DurationSeconds 20 -SessionName "$suiteName\_warmup" | Out-Null

$rows = @()
foreach ($case in $cases)
{
    & (Join-Path $PSScriptRoot "RunSSRSession.ps1") @common -DurationSeconds $DurationSeconds -SweepStartMs -75 `
        -ExtraServerCmds $case.Server -ExtraShooterCmds $case.Shooter -ExtraTargetCmds $case.Target `
        -SessionName "$suiteName\$($case.Name)" | Out-Host
    $dir = Join-Path $suiteDirectory $case.Name
    $r = python (Join-Path $PSScriptRoot "analyze_timing_sweep.py") (Join-Path $dir "server.log") --json | Out-String | ConvertFrom-Json
    $events = @(Select-String -Path (Join-Path $dir "*.log") -Pattern "SSREdge (CLIENT|SERVER)").Count
    $rows += [pscustomobject]@{
        case = $case.Name; events = $events
        rttMedianMs = if ($r.rttMs) { $r.rttMs.median } else { $null }
        rttMaxMs = if ($r.rttMs) { $r.rttMs.max } else { $null }
        hitPct = $r.authoritative.clearHitPct; hitN = $r.authoritative.clearHitN
        missPct = $r.authoritative.clearMissPct; missN = $r.authoritative.clearMissN
        fallbacks = $r.authoritative.presentTimeFallbacks; labeled = $r.labeledShots
    }
}

$rows | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $suiteDirectory "summary.json")
$markdown = @(
    "| Case | Edge events | Labeled shots | RTT median / max | Clear hits % | Clear misses % | Fallbacks |",
    "|---|---|---|---|---|---|---|"
)
foreach ($r in $rows)
{
    $markdown += "| $($r.case) | $($r.events) | $($r.labeled) | $($r.rttMedianMs) / $($r.rttMaxMs) | $($r.hitPct) (n=$($r.hitN)) | $($r.missPct) (n=$($r.missN)) | $($r.fallbacks) |"
}
$markdown | Set-Content -Encoding utf8 (Join-Path $suiteDirectory "summary.md")
$markdown | Write-Host
Write-Host "Summary: $suiteDirectory\summary.md"
