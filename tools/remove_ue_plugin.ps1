param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

$resolvedProjectDir = [System.IO.Path]::GetFullPath($ProjectDir)
$projectPluginsDir = Join-Path $resolvedProjectDir "Plugins"
$pluginSpecs = @(Get-TaAgentProjectPluginSpecs)
$removedPluginNames = @()
$foundAnyPlugin = $false

if (-not (Test-Path $projectPluginsDir)) {
    Write-Host "[TAAgent] Plugins folder not found at $projectPluginsDir - nothing to remove."
    exit 0
}

foreach ($pluginSpec in $pluginSpecs) {
    $pluginDir = Join-Path $projectPluginsDir $pluginSpec.Name
    if (-not (Test-Path $pluginDir)) {
        continue
    }

    $foundAnyPlugin = $true
    $upluginFile = Get-ChildItem -Path $pluginDir -Filter "*.uplugin" -File | Select-Object -First 1
    if ($null -eq $upluginFile) {
        Write-Host "[TAAgent] WARNING: $pluginDir exists but does not contain a .uplugin file."
        Write-Host "[TAAgent] Skipping removal to avoid deleting unknown content."
        continue
    }

    $content = Get-Content $upluginFile.FullName -Raw
    $namePattern = '"(FriendlyName|Name)"\s*:\s*"' + [regex]::Escape($pluginSpec.Name) + '"'
    if ($content -notmatch $namePattern) {
        Write-Host "[TAAgent] WARNING: $($upluginFile.FullName) does not look like the expected TAAgent plugin '$($pluginSpec.Name)'."
        Write-Host "[TAAgent] Skipping removal."
        continue
    }

    Write-Host "[TAAgent] Removing plugin from: $pluginDir"
    Remove-Item -Recurse -Force $pluginDir
    $removedPluginNames += $pluginSpec.Name
}

if (-not $foundAnyPlugin) {
    Write-Host "[TAAgent] No TAAgent plugins were found under $projectPluginsDir - nothing to remove."
    exit 0
}

if ($removedPluginNames.Count -gt 0) {
    Write-Host "[TAAgent] Removed plugins: $($removedPluginNames -join ', ')"
    Write-Host ""
} else {
    Write-Host "[TAAgent] No plugins were removed because all matches failed safety checks."
    exit 1
}

# Check if Plugins folder is now empty and clean up
$remaining = Get-ChildItem -Path $projectPluginsDir -ErrorAction SilentlyContinue
if ($null -eq $remaining -or $remaining.Count -eq 0) {
    # Only remove Plugins/ if it was created by us and is now empty
    # Don't remove - the user may have other uses for this folder
    Write-Host "[TAAgent] The Plugins folder is now empty. You can remove it manually if you wish:"
    Write-Host "  $projectPluginsDir"
}

Write-Host "[TAAgent] Uninstall complete. No TAAgent artifacts remain in your project."
Write-Host "[TAAgent] Note: if UE added plugin entries to your .uproject file, you can"
Write-Host "  safely remove the TAAgent-related entries from the Plugins array if present."
