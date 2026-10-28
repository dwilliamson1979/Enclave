# BuildAndTest.ps1
# Compiles EnclaveEditor (Development, Win64) and runs headless automation tests.
#
# Usage (from anywhere):
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\BuildAndTest.ps1
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\BuildAndTest.ps1 -TestFilter Enclave.Combat.SSR
#   powershell -ExecutionPolicy Bypass -File Tools\Scripts\BuildAndTest.ps1 -SkipBuild
#
# Exit code is 0 only when the build succeeds and every test passes.
# Logs and the JSON report go to Saved\Automation\.
# Can run while the editor is open, as long as Live Coding is not holding the module.

param(
    [string]$TestFilter = "Enclave",
    [string]$EngineRoot = "D:\UnrealEngine\Versions\UE_5.8",
    [switch]$SkipBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..")).TrimEnd('\', '/')
$projectFile = Join-Path $projectRoot "Enclave.uproject"
$outputDirectory = Join-Path $projectRoot "Saved\Automation"
$reportDirectory = Join-Path $outputDirectory "Report"
$testLog = Join-Path $outputDirectory "TestRun.log"

New-Item -ItemType Directory -Force $outputDirectory | Out-Null

if (-not $SkipBuild)
{
    Write-Host "== Building EnclaveEditor Win64 Development" -ForegroundColor Cyan
    & (Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat") EnclaveEditor Win64 Development "-Project=$projectFile" -WaitMutex
    if ($LASTEXITCODE -ne 0)
    {
        Write-Host "BUILD FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
        if ($LASTEXITCODE -eq 6)
        {
            Write-Host "If UBT reported Live Coding is active: close the editor, or compile there with Ctrl+Alt+F11." -ForegroundColor Yellow
        }
        exit $LASTEXITCODE
    }
}

if ($SkipTests)
{
    exit 0
}

Write-Host "== Running automation tests: $TestFilter" -ForegroundColor Cyan
$editorCmd = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$editorArgs = @(
    "`"$projectFile`"",
    "-ExecCmds=`"Automation RunTests $TestFilter;Quit`"",
    "-unattended", "-nopause", "-nullrhi", "-nosplash", "-nosound",
    "-stdout", "-FullStdOutLogOutput",
    "-ReportExportPath=`"$reportDirectory`""
)
$process = Start-Process -FilePath $editorCmd -ArgumentList $editorArgs -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $testLog

# Summarize results from the log; the editor exit code alone does not catch "no tests found".
$results = Select-String -Path $testLog -Pattern "Test Completed\. Result=\{(\w+)\} Name=\{[^}]*\} Path=\{([^}]*)\}"
$passed = @($results | Where-Object { $_.Matches[0].Groups[1].Value -eq "Success" })
$failed = @($results | Where-Object { $_.Matches[0].Groups[1].Value -ne "Success" })

foreach ($result in $failed)
{
    Write-Host ("FAIL  {0}" -f $result.Matches[0].Groups[2].Value) -ForegroundColor Red
}
Write-Host ("{0} passed, {1} failed. Log: {2}" -f $passed.Count, $failed.Count, $testLog)

if ($results.Count -eq 0)
{
    Write-Host "NO TESTS RAN for filter '$TestFilter'" -ForegroundColor Red
    exit 2
}
if ($failed.Count -gt 0 -or $process.ExitCode -ne 0)
{
    exit 1
}
exit 0
