param(
    [string]$RenderDocExe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Set-TaAgentProcessEnvironment

$repoRoot = Get-TaAgentRepoRoot
$runtimeRoot = Get-TaAgentRuntimeRoot
$pythonExe = Get-TaAgentVenvPython
$renderDocPath = Find-RenderDocExecutable -PreferredPath $RenderDocExe

$renderDocProfileRoot = Join-Path $runtimeRoot "appdata"
$renderDocExtensionDir = Join-Path $renderDocProfileRoot "qrenderdoc\extensions"
$renderDocTemp = Join-Path $runtimeRoot "tmp\renderdoc"

foreach ($directory in @($renderDocExtensionDir, $renderDocTemp)) {
    if (-not (Test-Path $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
}

$env:RENDERDOC_EXTENSION_DIR = $renderDocExtensionDir
& $pythonExe (Join-Path $repoRoot "src\scripts\renderdoc\install_extension.py")

$renderDocEnv = @{
    APPDATA = $renderDocProfileRoot
    LOCALAPPDATA = $renderDocProfileRoot
    TEMP = $renderDocTemp
    TMP = $renderDocTemp
    RENDERDOC_MCP_IPC_DIR = (Join-Path $runtimeRoot "ipc\renderdoc")
}

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $renderDocPath
$startInfo.WorkingDirectory = [System.IO.Path]::GetDirectoryName($renderDocPath)
$startInfo.UseShellExecute = $false

foreach ($entry in $renderDocEnv.GetEnumerator()) {
    $startInfo.Environment[$entry.Key] = $entry.Value
}

[System.Diagnostics.Process]::Start($startInfo) | Out-Null
Write-Host "[TAAgent] RenderDoc started with local profile: $renderDocPath"
Write-Host "[TAAgent] Local extension directory: $renderDocExtensionDir"
