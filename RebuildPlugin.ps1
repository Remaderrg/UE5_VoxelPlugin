#Requires -Version 7.0
<#
.SYNOPSIS
    Clean and rebuild DG_VoxelPlugin via the sibling .uproject (Windows / macOS)

 Usage:
   pwsh ./RebuildPlugin.ps1

 Requires sibling VoxelCore plugin:
   <Project>/Plugins/VoxelCore

 Builds the module through UnrealBuildTool (sees sibling plugins),
 then copies Binaries from Intermediate when needed.

 Optional path override:
   $env:DG_UE_ENGINE_PATH = "C:\Program Files\Epic Games\UE_5.8\Engine"
   pwsh ./RebuildPlugin.ps1
#>

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ========== Settings ==========
$EngineVersion      = "5.8"
$EnginePathOverride = $env:DG_UE_ENGINE_PATH
$EditorTarget       = "WinterGameEditor"
# ==============================

function Write-Step([string]$Message) {
	Write-Host $Message
}

function Get-UbtScript([string]$EnginePath) {
	if ($IsWindows) {
		return Join-Path $EnginePath "Build\BatchFiles\Build.bat"
	}
	return Join-Path $EnginePath "Build/BatchFiles/Build.sh"
}

function Find-EnginePath {
	param(
		[string]$PreferredVersion,
		[string]$Override
	)

	if ($Override) {
		$enginePath = $Override.TrimEnd('\', '/')
		$buildBat = Get-UbtScript $enginePath
		if (-not (Test-Path -LiteralPath $buildBat)) {
			throw "Engine not found: $enginePath`nExpected: $buildBat"
		}
		return (Resolve-Path -LiteralPath $enginePath).Path
	}

	$candidates = [System.Collections.Generic.List[string]]::new()
	if ($PreferredVersion) {
		if ($IsWindows) { $candidates.Add("C:\Program Files\Epic Games\UE_$PreferredVersion\Engine") }
		if ($IsMacOS)   { $candidates.Add("/Users/Shared/Epic Games/UE_$PreferredVersion/Engine") }
	}

	$installRoots = @()
	if ($IsWindows) { $installRoots += "C:\Program Files\Epic Games" }
	if ($IsMacOS)   { $installRoots += "/Users/Shared/Epic Games" }

	foreach ($root in $installRoots) {
		if (-not (Test-Path -LiteralPath $root)) { continue }
		Get-ChildItem -LiteralPath $root -Directory -Filter "UE_*" |
			Sort-Object Name -Descending |
			ForEach-Object { $candidates.Add((Join-Path $_.FullName "Engine")) }
	}

	$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	foreach ($enginePath in $candidates) {
		if (-not $seen.Add($enginePath)) { continue }
		if (Test-Path -LiteralPath (Get-UbtScript $enginePath)) {
			return (Resolve-Path -LiteralPath $enginePath).Path
		}
	}

	$hint = if ($PreferredVersion) { "UE_$PreferredVersion" } else { "UE_*" }
	throw "Unreal Engine not found (expected $hint). Set DG_UE_ENGINE_PATH."
}

function Test-UnrealEditorRunning {
	return $null -ne (Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue)
}

function Remove-BuildArtifact {
	param([string]$BasePath, [string]$Label)
	if (Test-Path -LiteralPath $BasePath) {
		Remove-Item -LiteralPath $BasePath -Recurse -Force
		Write-Step "  - $Label removed"
	} else {
		Write-Step "  - $Label not found"
	}
}

function Find-Uproject {
	param([string]$PluginDir)
	$pluginsRoot = Split-Path -Parent $PluginDir
	$projectRoot = Split-Path -Parent $pluginsRoot
	$uprojects = @(Get-ChildItem -LiteralPath $projectRoot -Filter "*.uproject" -File)
	if ($uprojects.Count -eq 0) {
		throw "No .uproject next to Plugins (expected parent of $pluginsRoot)"
	}
	return $uprojects[0]
}

function Invoke-ProjectModuleBuild {
	param(
		[string]$EnginePath,
		[string]$UprojectPath,
		[string]$TargetName
	)

	$platform = if ($IsWindows) { "Win64" } else { "Mac" }
	$buildScript = Get-UbtScript $EnginePath

	Write-Step "  Build: $buildScript"
	Write-Step "  Target: $TargetName $platform Development"
	Write-Step "  Project: $UprojectPath"
	Write-Host ""

	if ($IsMacOS) {
		& bash $buildScript $TargetName $platform Development "-Project=$UprojectPath" -WaitMutex
	} else {
		& $buildScript $TargetName $platform Development "-Project=$UprojectPath" -WaitMutex
	}

	if ($LASTEXITCODE -ne 0) {
		throw "Build failed (exit code $LASTEXITCODE)."
	}
}

# --- main ---

$PluginDir = $PSScriptRoot
$platformLabel = if ($IsWindows) { "Windows" } elseif ($IsMacOS) { "macOS" } else { "Unknown" }

Write-Step "[DG_RebuildPlugin] Clean and rebuild DG_VoxelPlugin ($platformLabel)"
Write-Host ""

if (Test-UnrealEditorRunning) {
	throw "ERROR: Close Unreal Editor before rebuilding!"
}

$voxelCoreUplugin = Join-Path (Split-Path -Parent $PluginDir) "VoxelCore\VoxelCore.uplugin"
if (-not (Test-Path -LiteralPath $voxelCoreUplugin)) {
	$voxelCoreUplugin = Join-Path (Split-Path -Parent $PluginDir) "VoxelCore/VoxelCore.uplugin"
}
if (-not (Test-Path -LiteralPath $voxelCoreUplugin)) {
	throw "Sibling VoxelCore not found.`nExpected: $(Split-Path -Parent $PluginDir)/VoxelCore`nClone: git clone https://github.com/VoxelPlugin/VoxelCore.git"
}

$uproject = Find-Uproject -PluginDir $PluginDir
$enginePath = Find-EnginePath -PreferredVersion $EngineVersion -Override $EnginePathOverride

Write-Step "Plugin:    $PluginDir"
Write-Step "VoxelCore: $(Split-Path -Parent $voxelCoreUplugin)"
Write-Step "Uproject:  $($uproject.FullName)"
Write-Step "Engine:    $enginePath"
Write-Step "Version:   $EngineVersion"
Write-Host ""

Write-Step "[1/2] Removing plugin Intermediate and Binaries..."
Remove-BuildArtifact -BasePath (Join-Path $PluginDir "Intermediate") -Label "Intermediate"
Remove-BuildArtifact -BasePath (Join-Path $PluginDir "Binaries") -Label "Binaries"
Write-Host ""

Write-Step "[2/2] Building via .uproject (UBT, sibling VoxelCore)..."
Invoke-ProjectModuleBuild -EnginePath $enginePath -UprojectPath $uproject.FullName -TargetName $EditorTarget

Write-Host ""
Write-Step "Done. DG_VoxelPlugin rebuilt with VoxelCore dependency."
