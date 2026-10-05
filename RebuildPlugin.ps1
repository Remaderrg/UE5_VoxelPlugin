#Requires -Version 7.0
<#
.SYNOPSIS
    Очистка и пересборка DG_VoxelPlugin через WinterGame.uproject (Windows / macOS)

 Запуск:
   pwsh ./RebuildPlugin.ps1

 Требует sibling-плагин VoxelCore:
   WinterGame/Plugins/VoxelCore

 Собирает модуль через UnrealBuildTool (видит sibling plugins),
 затем копирует Binaries из Intermediate при необходимости.

 Переопределение путей (необязательно):
   $env:DG_UE_ENGINE_PATH = "C:\Program Files\Epic Games\UE_5.8\Engine"
   pwsh ./RebuildPlugin.ps1
#>

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ========== Настройки ==========
$EngineVersion      = "5.8"
$EnginePathOverride = $env:DG_UE_ENGINE_PATH
$EditorTarget       = "WinterGameEditor"
# ==============================

function Write-Step([string]$Message) {
	Write-Host $Message
}

function Find-EnginePath {
	param(
		[string]$PreferredVersion,
		[string]$Override
	)

	if ($Override) {
		$enginePath = $Override.TrimEnd('\', '/')
		$buildBat = if ($IsWindows) {
			Join-Path $enginePath "Build\BatchFiles\Build.bat"
		} else {
			Join-Path $enginePath "Build/BatchFiles/Build.sh"
		}
		if (-not (Test-Path -LiteralPath $buildBat)) {
			throw "Движок не найден: $enginePath`nОжидался: $buildBat"
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
		$buildBat = if ($IsWindows) {
			Join-Path $enginePath "Build\BatchFiles\Build.bat"
		} else {
			Join-Path $enginePath "Build/BatchFiles/Build.sh"
		}
		if (Test-Path -LiteralPath $buildBat) {
			return (Resolve-Path -LiteralPath $enginePath).Path
		}
	}

	$hint = if ($PreferredVersion) { "UE_$PreferredVersion" } else { "UE_*" }
	throw "Unreal Engine не найден (ожидался $hint). Задайте DG_UE_ENGINE_PATH."
}

function Test-UnrealEditorRunning {
	return $null -ne (Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue)
}

function Remove-BuildArtifact {
	param([string]$BasePath, [string]$Label)
	if (Test-Path -LiteralPath $BasePath) {
		Remove-Item -LiteralPath $BasePath -Recurse -Force
		Write-Step "  - $Label удалена"
	} else {
		Write-Step "  - $Label не найдена"
	}
}

function Find-Uproject {
	param([string]$PluginDir)
	$pluginsRoot = Split-Path -Parent $PluginDir
	$projectRoot = Split-Path -Parent $pluginsRoot
	$uprojects = @(Get-ChildItem -LiteralPath $projectRoot -Filter "*.uproject" -File)
	if ($uprojects.Count -eq 0) {
		throw "Не найден .uproject рядом с Plugins (ожидался parent of $pluginsRoot)"
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
	$buildScript = if ($IsWindows) {
		Join-Path $EnginePath "Build\BatchFiles\Build.bat"
	} else {
		Join-Path $EnginePath "Build/BatchFiles/Build.sh"
	}

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
		throw "Ошибка сборки (exit code $LASTEXITCODE)."
	}
}

# --- main ---

$PluginDir = $PSScriptRoot
$platformLabel = if ($IsWindows) { "Windows" } elseif ($IsMacOS) { "macOS" } else { "Unknown" }

Write-Step "[DG_RebuildPlugin] Очистка и пересборка DG_VoxelPlugin ($platformLabel)"
Write-Host ""

if (Test-UnrealEditorRunning) {
	throw "ОШИБКА: Закройте Unreal Editor перед пересборкой!"
}

$voxelCoreUplugin = Join-Path (Split-Path -Parent $PluginDir) "VoxelCore\VoxelCore.uplugin"
if (-not (Test-Path -LiteralPath $voxelCoreUplugin)) {
	$voxelCoreUplugin = Join-Path (Split-Path -Parent $PluginDir) "VoxelCore/VoxelCore.uplugin"
}
if (-not (Test-Path -LiteralPath $voxelCoreUplugin)) {
	throw "Не найден sibling VoxelCore.`nОжидался: $(Split-Path -Parent $PluginDir)/VoxelCore`nКлонируй: git clone https://github.com/VoxelPlugin/VoxelCore.git"
}

$uproject = Find-Uproject -PluginDir $PluginDir
$enginePath = Find-EnginePath -PreferredVersion $EngineVersion -Override $EnginePathOverride

Write-Step "Плагин:    $PluginDir"
Write-Step "VoxelCore: $(Split-Path -Parent $voxelCoreUplugin)"
Write-Step "Uproject:  $($uproject.FullName)"
Write-Step "Движок:    $enginePath"
Write-Step "Версия:    $EngineVersion"
Write-Host ""

Write-Step "[1/2] Удаление Intermediate и Binaries плагина..."
Remove-BuildArtifact -BasePath (Join-Path $PluginDir "Intermediate") -Label "Intermediate"
Remove-BuildArtifact -BasePath (Join-Path $PluginDir "Binaries") -Label "Binaries"
Write-Host ""

Write-Step "[2/2] Сборка через .uproject (UBT, sibling VoxelCore)..."
Invoke-ProjectModuleBuild -EnginePath $enginePath -UprojectPath $uproject.FullName -TargetName $EditorTarget

Write-Host ""
Write-Step "Готово. DG_VoxelPlugin пересобран вместе с зависимостью VoxelCore."
