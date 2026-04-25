param(
    [ValidateSet("triggerImmediateCapture", "triggerDelayedCapture", "queueCap", "cycleActiveWindow")]
    [string]$ButtonName = "triggerImmediateCapture",
    [switch]$Json
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Set-TaAgentProcessEnvironment

$repoRoot = Get-TaAgentRepoRoot
$pythonExe = Get-TaAgentVenvPython
$scriptPath = Join-Path $repoRoot "src\scripts\renderdoc\trigger_live_capture.py"

$arguments = @($scriptPath, "--button-name", $ButtonName)
if ($Json) {
    $arguments += "--json"
}

& $pythonExe @arguments
exit $LASTEXITCODE