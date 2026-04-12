param(
    [string]$Config = "Release",
    [string]$RepoRoot = "",
    [string]$MayaInstallRoot = "C:\Program Files\Autodesk\Maya2022",
    [string]$MayaDevkitRoot = "D:\_Code_Here\Maya\Autodesk_Maya_2022_5_Update_DEVKIT_Windows\devkitBase",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

function Get-FileVersionString {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    $versionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    if ($null -eq $versionInfo) {
        return ""
    }

    if (-not [string]::IsNullOrWhiteSpace($versionInfo.ProductVersion)) {
        return $versionInfo.ProductVersion
    }

    if (-not [string]::IsNullOrWhiteSpace($versionInfo.FileVersion)) {
        return $versionInfo.FileVersion
    }

    return ""
}

function Get-PythonVersionString {
    param([string]$MayapyPath)

    if (-not (Test-Path -LiteralPath $MayapyPath)) {
        return ""
    }

    try {
        $result = & $MayapyPath -c "import platform,sys; print(platform.python_version()); print(sys.version.replace('\n', ' '))" 2>$null
        if ($LASTEXITCODE -ne 0 -or $null -eq $result) {
            return ""
        }

        return ($result | Where-Object { $_ -and $_.Trim().Length -gt 0 }) -join " | "
    }
    catch {
        return ""
    }
}

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
    $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory "..\.."))
}

$pluginRoot = Join-Path $RepoRoot "dcc_plugin"
$buildDir = Join-Path $pluginRoot "build"
$mayaExe = Join-Path $MayaInstallRoot "bin\maya.exe"
$mayapyExe = Join-Path $MayaInstallRoot "bin\mayapy.exe"
$pluginBinary = Join-Path $pluginRoot ("bin\" + $Config + "\maya_dmx.mll")
$melDir = Join-Path $pluginRoot "src\mel"
$batchRegressionScript = Join-Path $pluginRoot "tools\MayaBatchRegression.py"
$interactiveValidationScript = Join-Path $pluginRoot "tools\MayaInteractiveValidation.py"
$batchWrapper = Join-Path $pluginRoot "RunMayaBatchRegression.bat"
$interactiveWrapper = Join-Path $pluginRoot "RunMayaInteractiveValidation.bat"

$items = @(
    @{ Name = "RepoRoot"; Path = $RepoRoot; Required = $true },
    @{ Name = "PluginRoot"; Path = $pluginRoot; Required = $true },
    @{ Name = "BuildDir"; Path = $buildDir; Required = $false },
    @{ Name = "MayaInstallRoot"; Path = $MayaInstallRoot; Required = $true },
    @{ Name = "MayaDevkitRoot"; Path = $MayaDevkitRoot; Required = $true },
    @{ Name = "MayaExe"; Path = $mayaExe; Required = $true },
    @{ Name = "MayapyExe"; Path = $mayapyExe; Required = $true },
    @{ Name = "PluginBinary"; Path = $pluginBinary; Required = $true },
    @{ Name = "MelDir"; Path = $melDir; Required = $true },
    @{ Name = "BatchRegressionScript"; Path = $batchRegressionScript; Required = $true },
    @{ Name = "InteractiveValidationScript"; Path = $interactiveValidationScript; Required = $true },
    @{ Name = "BatchWrapper"; Path = $batchWrapper; Required = $true },
    @{ Name = "InteractiveWrapper"; Path = $interactiveWrapper; Required = $true }
)

$generatedAt = Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'

$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add("# Maya DMX Host Validation Environment Report")
$reportLines.Add("")
$reportLines.Add("- GeneratedAt: $generatedAt")
$reportLines.Add("- Config: $Config")
$reportLines.Add("")
$reportLines.Add("## Environment Items")
$reportLines.Add("")
$reportLines.Add("| Item | Path | Required | Exists | Size(Bytes) | LastWriteTime | Version |")
$reportLines.Add("| --- | --- | --- | --- | ---: | --- | --- |")

$missingRequired = @()
foreach ($item in $items) {
    $exists = Test-Path -LiteralPath $item.Path
    $size = ""
    $lastWriteTime = ""
    $version = ""

    if ($exists) {
        $resolved = Get-Item -LiteralPath $item.Path
        if ($resolved -is [System.IO.FileInfo]) {
            $size = [string]$resolved.Length
            $lastWriteTime = $resolved.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
            $version = Get-FileVersionString -Path $item.Path
        }
        elseif ($resolved -is [System.IO.DirectoryInfo]) {
            $lastWriteTime = $resolved.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
        }
    }

    if ($item.Required -and -not $exists) {
        $missingRequired += $item.Name
    }

    $reportLines.Add("| $($item.Name) | $($item.Path) | $($item.Required) | $exists | $size | $lastWriteTime | $version |")
}

$pythonVersion = Get-PythonVersionString -MayapyPath $mayapyExe
$reportLines.Add("")
$reportLines.Add("## Host Info")
$reportLines.Add("")
$reportLines.Add("- mayapy Python version: $pythonVersion")
$reportLines.Add("- Default batch regression entry: dcc_plugin\\RunMayaBatchRegression.bat")
$reportLines.Add("- Default interactive validation entry: dcc_plugin\\RunMayaInteractiveValidation.bat")
$reportLines.Add("- Default batch interpreter: $mayapyExe")
$reportLines.Add("- Default interactive host: $mayaExe")
$reportLines.Add("")
$reportLines.Add("## Conclusion")
$reportLines.Add("")

if ($missingRequired.Count -eq 0) {
    $reportLines.Add("- The host validation environment satisfies the required path checks for the current DMX batch and interactive validation flows.")
}
else {
    $reportLines.Add("- Missing required items: $($missingRequired -join ", ")")
}

$reportLines.Add("- To override default install paths, set MAYA_PYTHON_EXE_OVERRIDE for batch regression and MAYA_EXE_OVERRIDE for interactive validation.")

$reportText = $reportLines -join "`r`n"

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $outputDirectory = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory) -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    }
    Set-Content -LiteralPath $OutputPath -Value $reportText -Encoding UTF8
}

$reportText
