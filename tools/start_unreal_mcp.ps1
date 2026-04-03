param(
    [string]$UnrealHost = "127.0.0.1",
    [int]$Port = 55557
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Set-TaAgentProcessEnvironment

$env:UE_HOST = $UnrealHost
$env:UE_PORT = "$Port"

$repoRoot = Get-TaAgentRepoRoot
$pythonExe = Get-TaAgentVenvPython
$serverRoot = Join-Path $repoRoot "mcps\unreal_render_mcp"

Push-Location $serverRoot
try {
    & $pythonExe "server.py"
}
finally {
    Pop-Location
}
