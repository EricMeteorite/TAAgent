Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

function Find-BootstrapPython {
    if ($env:TAAGENT_BOOTSTRAP_PYTHON -and (Test-Path $env:TAAGENT_BOOTSTRAP_PYTHON)) {
        return @($env:TAAGENT_BOOTSTRAP_PYTHON)
    }

    $candidates = @(
        @{ Command = "py"; Args = @("-3.11") },
        @{ Command = "py"; Args = @("-3.10") },
        @{ Command = "py"; Args = @("-3") },
        @{ Command = "python"; Args = @() },
        @{ Command = "python3"; Args = @() }
    )

    foreach ($candidate in $candidates) {
        $commandInfo = Get-Command $candidate.Command -ErrorAction SilentlyContinue
        if (-not $commandInfo) {
            continue
        }

        try {
            $probeArgs = @()
            $probeArgs += $candidate.Args
            $probeArgs += @("-c", "import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)")
            & $candidate.Command @probeArgs *> $null
            if ($LASTEXITCODE -eq 0) {
                return @($candidate.Command) + $candidate.Args
            }
        }
        catch {
        }
    }

    throw "Python 3.10+ was not found. Install Python 3.10 or newer and run this script again."
}

Set-TaAgentProcessEnvironment

$repoRoot = Get-TaAgentRepoRoot
$runtimeRoot = Get-TaAgentRuntimeRoot
$venvPath = Join-Path $runtimeRoot ".venv"
$bootstrapPython = Find-BootstrapPython
$bootstrapCommand = $bootstrapPython[0]
$bootstrapArgs = @()
if ($bootstrapPython.Length -gt 1) {
    $bootstrapArgs = $bootstrapPython[1..($bootstrapPython.Length - 1)]
}

Write-Host "[TAAgent] Repo root: $repoRoot"
Write-Host "[TAAgent] Runtime root: $runtimeRoot"

if (-not (Test-Path (Join-Path $venvPath "Scripts\python.exe"))) {
    Write-Host "[TAAgent] Creating local virtual environment..."
    & $bootstrapCommand @bootstrapArgs -m venv $venvPath
}

$pythonExe = Join-Path $venvPath "Scripts\python.exe"

Write-Host "[TAAgent] Upgrading pip/setuptools/wheel inside the venv..."
& $pythonExe -m pip install --upgrade pip setuptools wheel

Write-Host "[TAAgent] Installing workspace dependencies..."
& $pythonExe -m pip install -e $repoRoot

Write-Host "[TAAgent] Installing Unreal MCP dependencies..."
& $pythonExe -m pip install -r (Join-Path $repoRoot "mcps\unreal_render_mcp\requirements.txt")

Write-Host "[TAAgent] Installing Unreal CLI package..."
& $pythonExe -m pip install -e (Join-Path $repoRoot "unreal\agent-harness")

Write-Host "[TAAgent] Generating local MCP config..."
& (Join-Path $PSScriptRoot "generate_local_mcp_config.ps1")

Write-Host "[TAAgent] Setup complete. Read docs/DEPLOYMENT_ZH-CN.md next."
