Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Set-TaAgentProcessEnvironment

$runtimeRoot = Get-TaAgentRuntimeRoot
$configDir = Join-Path $runtimeRoot "config"
$configPath = Join-Path $configDir "mcp_config.local.json"

if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
}

$renderDocScript = Join-Path (Get-TaAgentRepoRoot) "tools\start_renderdoc_mcp.ps1"
$unrealScript = Join-Path (Get-TaAgentRepoRoot) "tools\start_unreal_mcp.ps1"

$config = [ordered]@{
    mcpServers = [ordered]@{
        renderdoc = [ordered]@{
            command = "powershell.exe"
            args = @("-ExecutionPolicy", "Bypass", "-File", $renderDocScript)
        }
        "unreal-render" = [ordered]@{
            command = "powershell.exe"
            args = @("-ExecutionPolicy", "Bypass", "-File", $unrealScript)
        }
    }
}

$config | ConvertTo-Json -Depth 6 | Set-Content -Path $configPath -Encoding UTF8
Write-Host "[TAAgent] Local MCP config written to: $configPath"
