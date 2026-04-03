param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-TaAgentRepoRoot
$pluginSpecs = @(Get-TaAgentProjectPluginSpecs)

function Copy-TaAgentPluginTree {
    param(
        [Parameter(Mandatory=$true)]
        [string]$SourceDir,

        [Parameter(Mandatory=$true)]
        [string]$DestinationDir
    )

    $excludeDirs = @("Binaries", "Intermediate", "DerivedDataCache", ".vs")
    $excludeFiles = @("*.pdb", "*.dll", "*.exe", "*.obj", "*.lib")

    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null

    Get-ChildItem -Path $SourceDir -Recurse | ForEach-Object {
        $relativePath = $_.FullName.Substring($SourceDir.Length + 1)
        $destPath = Join-Path $DestinationDir $relativePath

        $skip = $false
        foreach ($dir in $excludeDirs) {
            if ($relativePath -like "$dir\*" -or $relativePath -eq $dir) {
                $skip = $true
                break
            }
        }
        if ($skip) { return }

        foreach ($pattern in $excludeFiles) {
            if ($_.Name -like $pattern) {
                $skip = $true
                break
            }
        }
        if ($skip) { return }

        if ($_.PSIsContainer) {
            New-Item -ItemType Directory -Path $destPath -Force | Out-Null
        } else {
            $destDir = Split-Path $destPath -Parent
            if (-not (Test-Path $destDir)) {
                New-Item -ItemType Directory -Path $destDir -Force | Out-Null
            }
            Copy-Item -Path $_.FullName -Destination $destPath -Force
        }
    }
}

# ---------- Validate target project ----------
$resolvedProjectDir = [System.IO.Path]::GetFullPath($ProjectDir)
if (-not (Test-Path $resolvedProjectDir)) {
    throw "Project directory does not exist: $resolvedProjectDir"
}

# Find .uproject file to confirm this is a UE project
$uprojectFiles = @(Get-ChildItem -Path $resolvedProjectDir -Filter "*.uproject" -File)
if ($uprojectFiles.Count -eq 0) {
    throw "No .uproject file found in $resolvedProjectDir. Is this a valid Unreal project?"
}
$projectName = $uprojectFiles[0].BaseName
Write-Host "[TAAgent] Target project: $projectName ($resolvedProjectDir)"

# ---------- Destination ----------
$projectPluginsDir = Join-Path $resolvedProjectDir "Plugins"
if (-not (Test-Path $projectPluginsDir)) {
    New-Item -ItemType Directory -Path $projectPluginsDir -Force | Out-Null
}

# ---------- Copy all TAAgent project plugins (no Binaries / Intermediate) ----------
$deployedPluginDirs = @()
foreach ($pluginSpec in $pluginSpecs) {
    $pluginDst = Join-Path $projectPluginsDir $pluginSpec.Name
    if (Test-Path $pluginDst) {
        Write-Host "[TAAgent] Plugin already exists at $pluginDst"
        Write-Host "[TAAgent] Removing old copy..."
        Remove-Item -Recurse -Force $pluginDst
    }

    Write-Host "[TAAgent] Copying plugin source to $pluginDst ..."
    Copy-TaAgentPluginTree -SourceDir $pluginSpec.SourceDir -DestinationDir $pluginDst
    $deployedPluginDirs += $pluginDst
}

Write-Host ""
Write-Host "[TAAgent] Plugins deployed successfully."
Write-Host ("[TAAgent] Deployed plugins: {0}" -f (($pluginSpecs | ForEach-Object { $_.Name }) -join ", "))
Write-Host ""
Write-Host "[TAAgent] What happens next:"
Write-Host "  1. Open your project with Unreal Editor."
Write-Host "  2. UE will compile the plugins automatically on first load."
Write-Host "  3. UnrealMCP listens on 127.0.0.1:55557 (TCP, loopback only)."
Write-Host "  4. AssetValidation adds Niagara validation and overdraw analysis tools in the editor."
Write-Host ""
Write-Host "[TAAgent] To uninstall later, run:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir `"$resolvedProjectDir`""
Write-Host ""
Write-Host "[TAAgent] Or simply delete these directories:"
foreach ($pluginDir in $deployedPluginDirs) {
    Write-Host "  $pluginDir"
}
