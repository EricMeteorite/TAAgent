Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Set-TaAgentProcessEnvironment

$repoRoot = Get-TaAgentRepoRoot
$pythonExe = Get-TaAgentVenvPython
$serverRoot = Join-Path $repoRoot "mcps\renderdoc_mcp"

Push-Location $serverRoot
try {
    & $pythonExe -m mcp_server.server
}
finally {
    Pop-Location
}
