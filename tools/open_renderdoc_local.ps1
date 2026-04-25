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
$systemRenderDocExtensionDir = $null
$systemRenderDocUiConfig = $null

 $systemAppData = [System.Environment]::GetFolderPath("ApplicationData")
 if ($systemAppData) {
    $systemRenderDocExtensionDir = Join-Path $systemAppData "qrenderdoc\extensions"
    $systemRenderDocUiConfig = Join-Path $systemAppData "qrenderdoc\UI.config"
}

function Ensure-RenderDocExtensionAutoLoad {
    param(
        [string]$UiConfigPath,
        [string]$PackageName
    )

    if (-not $UiConfigPath -or -not $PackageName) {
        return
    }

    $uiConfigDirectory = Split-Path -Parent $UiConfigPath
    if ($uiConfigDirectory -and -not (Test-Path $uiConfigDirectory)) {
        New-Item -ItemType Directory -Path $uiConfigDirectory -Force | Out-Null
    }

    $config = [pscustomobject]@{}
    if (Test-Path $UiConfigPath) {
        $raw = Get-Content -Path $UiConfigPath -Raw
        if ($raw.Trim()) {
            $config = $raw | ConvertFrom-Json
        }
    }

    $packages = @()
    if ($config.PSObject.Properties.Name -contains 'AlwaysLoad_Extensions') {
        $packages = @($config.AlwaysLoad_Extensions)
    }

    if ($packages -notcontains $PackageName) {
        $packages += $PackageName
        if ($config.PSObject.Properties.Name -contains 'AlwaysLoad_Extensions') {
            $config.AlwaysLoad_Extensions = $packages
        }
        else {
            $config | Add-Member -NotePropertyName AlwaysLoad_Extensions -NotePropertyValue $packages
        }

        $config | ConvertTo-Json -Depth 100 | Set-Content -Path $UiConfigPath -Encoding UTF8
        Write-Host "[TAAgent] Ensured RenderDoc auto-load includes: $PackageName"
    }
}

foreach ($directory in @($renderDocExtensionDir, $renderDocTemp)) {
    if (-not (Test-Path $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
}

$env:RENDERDOC_EXTENSION_DIR = $renderDocExtensionDir
& $pythonExe (Join-Path $repoRoot "src\scripts\renderdoc\install_extension.py")

if ($systemRenderDocExtensionDir -and $systemRenderDocExtensionDir -ne $renderDocExtensionDir) {
    $env:RENDERDOC_EXTENSION_DIR = $systemRenderDocExtensionDir
    & $pythonExe (Join-Path $repoRoot "src\scripts\renderdoc\install_extension.py")
    Write-Host "[TAAgent] Mirrored RenderDoc extension into system AppData for Windows UI discovery: $systemRenderDocExtensionDir"
}

Ensure-RenderDocExtensionAutoLoad -UiConfigPath $systemRenderDocUiConfig -PackageName "renderdoc_mcp_bridge"

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
Write-Host "[TAAgent] RenderDoc started with TAAgent runtime overrides: $renderDocPath"
Write-Host "[TAAgent] Local extension directory: $renderDocExtensionDir"
if ($systemRenderDocExtensionDir) {
    Write-Host "[TAAgent] System extension directory: $systemRenderDocExtensionDir"
}
