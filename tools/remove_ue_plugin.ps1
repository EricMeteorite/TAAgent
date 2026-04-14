param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectDir,

    [switch]$ForceCloseEditor
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

function Test-TaAgentCommandLineContainsPath {
    param(
        [string]$CommandLine,
        [string[]]$CandidatePaths
    )

    if ([string]::IsNullOrWhiteSpace($CommandLine)) {
        return $false
    }

    foreach ($candidatePath in $CandidatePaths) {
        if ([string]::IsNullOrWhiteSpace($candidatePath)) {
            continue
        }

        if ($CommandLine.IndexOf($candidatePath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            return $true
        }
    }

    return $false
}

function Get-TaAgentProjectEditorProcesses {
    param(
        [string]$ResolvedProjectDir,
        [string]$ResolvedUprojectPath
    )

    $candidatePaths = @($ResolvedProjectDir)
    if (-not [string]::IsNullOrWhiteSpace($ResolvedUprojectPath)) {
        $candidatePaths += $ResolvedUprojectPath
    }

    $matches = @()
    $editorProcesses = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" -ErrorAction SilentlyContinue
    foreach ($editorProcess in $editorProcesses) {
        $commandLine = [string]$editorProcess.CommandLine
        if (-not (Test-TaAgentCommandLineContainsPath -CommandLine $commandLine -CandidatePaths $candidatePaths)) {
            continue
        }

        $matches += [pscustomobject]@{
            ProcessId = [int]$editorProcess.ProcessId
            Name = [string]$editorProcess.Name
            CommandLine = $commandLine
        }
    }

    return $matches
}

function Write-TaAgentEditorProcessSummary {
    param(
        [object[]]$EditorProcesses
    )

    foreach ($editorProcess in $EditorProcesses) {
        Write-Host ("  PID {0} {1}" -f $editorProcess.ProcessId, $editorProcess.Name)
        Write-Host ("    {0}" -f $editorProcess.CommandLine)
    }
}

function Stop-TaAgentProjectEditorProcesses {
    param(
        [object[]]$EditorProcesses
    )

    foreach ($editorProcess in $EditorProcesses) {
        Write-Host ("[TAAgent] Stopping {0} (PID {1}) ..." -f $editorProcess.Name, $editorProcess.ProcessId)
        Stop-Process -Id $editorProcess.ProcessId -Force -ErrorAction Stop
    }

    foreach ($editorProcess in $EditorProcesses) {
        try {
            Wait-Process -Id $editorProcess.ProcessId -Timeout 15 -ErrorAction Stop
        }
        catch [System.ArgumentException] {
            # The process already exited between Stop-Process and Wait-Process.
        }
    }
}

$resolvedProjectDir = [System.IO.Path]::GetFullPath($ProjectDir)
$uprojectFile = Get-ChildItem -Path $resolvedProjectDir -Filter "*.uproject" -File -ErrorAction SilentlyContinue | Select-Object -First 1
$resolvedUprojectPath = if ($null -ne $uprojectFile) { $uprojectFile.FullName } else { $null }
$projectPluginsDir = Join-Path $resolvedProjectDir "Plugins"
$pluginSpecs = @(Get-TaAgentProjectPluginSpecs)
$removedPluginNames = @()
$foundAnyPlugin = $false

$editorProcesses = @(Get-TaAgentProjectEditorProcesses -ResolvedProjectDir $resolvedProjectDir -ResolvedUprojectPath $resolvedUprojectPath)
if ($editorProcesses.Count -gt 0) {
    if (-not $ForceCloseEditor) {
        Write-Host "[TAAgent] Unreal Editor is still running for this project, so plugin DLLs are locked."
        Write-Host "[TAAgent] Close the editor first, or rerun this script with -ForceCloseEditor."
        Write-Host "[TAAgent] Detected editor process(es):"
        Write-TaAgentEditorProcessSummary -EditorProcesses $editorProcesses
        exit 1
    }

    Write-Host "[TAAgent] Unreal Editor is still running for this project."
    Write-Host "[TAAgent] ForceCloseEditor was specified, so the script will stop the editor before uninstalling plugins."
    Stop-TaAgentProjectEditorProcesses -EditorProcesses $editorProcesses

    $remainingEditorProcesses = @(Get-TaAgentProjectEditorProcesses -ResolvedProjectDir $resolvedProjectDir -ResolvedUprojectPath $resolvedUprojectPath)
    if ($remainingEditorProcesses.Count -gt 0) {
        Write-Host "[TAAgent] ERROR: Unreal Editor is still running after the stop request."
        Write-TaAgentEditorProcessSummary -EditorProcesses $remainingEditorProcesses
        exit 1
    }
}

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

    try {
        Write-Host "[TAAgent] Removing plugin from: $pluginDir"
        Remove-Item -Recurse -Force $pluginDir -ErrorAction Stop
        $removedPluginNames += $pluginSpec.Name
    }
    catch [System.UnauthorizedAccessException] {
        Write-Host "[TAAgent] ERROR: A file under this plugin is still in use:"
        Write-Host "  $pluginDir"
        Write-Host "[TAAgent] Close Unreal Editor and retry, or rerun with -ForceCloseEditor."
        exit 1
    }
    catch [System.IO.IOException] {
        Write-Host "[TAAgent] ERROR: A file under this plugin is still in use:"
        Write-Host "  $pluginDir"
        Write-Host "[TAAgent] Close Unreal Editor and retry, or rerun with -ForceCloseEditor."
        exit 1
    }
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
