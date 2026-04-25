Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-TaAgentRepoRoot {
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
}

function Get-TaAgentProjectPluginSpecs {
    $pluginsRoot = Join-Path (Get-TaAgentRepoRoot) "plugins\unreal\UnrealMCP\RenderingMCP\Plugins"
    if (-not (Test-Path $pluginsRoot)) {
        throw "TAAgent plugin source root was not found: $pluginsRoot"
    }

    $pluginSpecs = @()
    foreach ($pluginDirectory in Get-ChildItem -Path $pluginsRoot -Directory) {
        $upluginFile = Get-ChildItem -Path $pluginDirectory.FullName -Filter "*.uplugin" -File | Select-Object -First 1
        if ($null -eq $upluginFile) {
            continue
        }

        $pluginSpecs += [pscustomobject]@{
            Name = [System.IO.Path]::GetFileNameWithoutExtension($upluginFile.Name)
            SourceDir = $pluginDirectory.FullName
            UpluginPath = $upluginFile.FullName
        }
    }

    if ($pluginSpecs.Count -eq 0) {
        throw "No TAAgent Unreal project plugins were found under: $pluginsRoot"
    }

    return $pluginSpecs
}

function Get-TaAgentRuntimeRoot {
    return Join-Path (Get-TaAgentRepoRoot) ".taagent-local"
}

function Get-TaAgentVenvPython {
    $pythonPath = Join-Path (Get-TaAgentRuntimeRoot) ".venv\Scripts\python.exe"
    if (-not (Test-Path $pythonPath)) {
        throw "Local virtual environment not found: $pythonPath. Run tools/setup_local.ps1 first."
    }
    return $pythonPath
}

function Initialize-TaAgentRuntimeDirectories {
    $runtimeRoot = Get-TaAgentRuntimeRoot
    $directories = @(
        $runtimeRoot,
        (Join-Path $runtimeRoot "appdata"),
        (Join-Path $runtimeRoot "config"),
        (Join-Path $runtimeRoot "ipc\renderdoc"),
        (Join-Path $runtimeRoot "logs"),
        (Join-Path $runtimeRoot "pip-cache"),
        (Join-Path $runtimeRoot "tmp"),
        (Join-Path $runtimeRoot "tmp\ue_cli")
    )

    foreach ($directory in $directories) {
        if (-not (Test-Path $directory)) {
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
        }
    }
}

function Set-TaAgentProcessEnvironment {
    Initialize-TaAgentRuntimeDirectories

    $runtimeRoot = Get-TaAgentRuntimeRoot
    $tempRoot = Join-Path $runtimeRoot "tmp"
    $env:TAAGENT_ROOT = Get-TaAgentRepoRoot
    $env:TAAGENT_RUNTIME_ROOT = $runtimeRoot
    $env:PIP_CACHE_DIR = Join-Path $runtimeRoot "pip-cache"
    $env:PIP_DISABLE_PIP_VERSION_CHECK = "1"
    $env:FASTMCP_CHECK_FOR_UPDATES = "off"
    $env:TEMP = $tempRoot
    $env:TMP = $tempRoot
    $env:UE_CLI_TEMP_DIR = Join-Path $runtimeRoot "tmp\ue_cli"
    $env:RENDERDOC_MCP_IPC_DIR = Join-Path $runtimeRoot "ipc\renderdoc"
}

function Find-RenderDocExecutable {
    param(
        [string]$PreferredPath
    )

    function Resolve-RenderDocCandidate {
        param(
            [string]$Candidate
        )

        if (-not $Candidate) {
            return $null
        }

        $trimmed = $Candidate.Trim().Trim('"')
        if (-not $trimmed) {
            return $null
        }

        $trimmed = $trimmed.TrimEnd('\', '/')

        if (Test-Path $trimmed -PathType Container) {
            foreach ($name in @('qrenderdoc.exe', 'renderdocui.exe')) {
                $child = Join-Path $trimmed $name
                if (Test-Path $child -PathType Leaf) {
                    return $child
                }
            }
        }

        if (Test-Path $trimmed -PathType Leaf) {
            return $trimmed
        }

        return $null
    }

    $candidates = @()
    if ($PreferredPath) {
        $candidates += $PreferredPath
    }
    if ($env:RENDERDOC_EXE) {
        $candidates += $env:RENDERDOC_EXE
    }
    $candidates += @(
        "C:\Program Files\RenderDoc\qrenderdoc.exe",
        "C:\Program Files\RenderDoc\renderdocui.exe"
    )

    foreach ($candidate in $candidates) {
        $resolved = Resolve-RenderDocCandidate -Candidate $candidate
        if ($resolved) {
            return [System.IO.Path]::GetFullPath($resolved)
        }
    }

    throw "RenderDoc executable was not found. Pass -RenderDocExe or set RENDERDOC_EXE."
}
