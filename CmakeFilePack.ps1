# CmakeFilePack.ps1
# 将 CMake 构建系统相关文件打包为 cmake_patch.zip

param(
    [string]$Root = $PSScriptRoot,
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"

$Root = [System.IO.Path]::GetFullPath($Root)
if (-not $Output) {
    $Output = Join-Path $Root "cmake_patch.zip"
} else {
    $Output = [System.IO.Path]::GetFullPath($Output)
}

if (Test-Path -LiteralPath $Output) {
    Remove-Item -LiteralPath $Output -Force
}

$files = [System.Collections.Generic.List[string]]::new()
@(
    "CMakeLists.txt",
    "CmakeCreateSolution.bat",
    "CmakeBuildSolution.bat",
    "CmakeCreateVPCSolution.bat",
    "CmakeBuildVPCSolution.bat",
    "CmakeFilePackToZip.bat",
    "CmakeFilePack.ps1",
    "src\CreateSolution.bat",
    "src\UpgradeToVS2022.ps1",
    "src\tools\extract_sources.py"
) | ForEach-Object {
    $files.Add($_)
}

$cmakeDir = Join-Path $Root "src\cmake"
if (Test-Path -LiteralPath $cmakeDir) {
    Get-ChildItem -LiteralPath $cmakeDir -Recurse -File | ForEach-Object {
        $relativePath = $_.FullName.Substring($Root.Length).TrimStart([char[]]@('\', '/'))
        $files.Add($relativePath)
    }
}

$srcDir = Join-Path $Root "src"
if (Test-Path -LiteralPath $srcDir) {
    Get-ChildItem -LiteralPath $srcDir -Filter "CMakeLists.txt" -Recurse -File | ForEach-Object {
        $relativePath = $_.FullName.Substring($Root.Length).TrimStart([char[]]@('\', '/'))
        $files.Add($relativePath)
    }
}

$uniqueFiles = $files | Sort-Object -Unique

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($Output, "Create")

$written = 0
$missing = 0

try {
    foreach ($rel in $uniqueFiles) {
        $full = Join-Path $Root $rel
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            Write-Warning "MISSING: $rel"
            $missing++
            continue
        }

        $entryName = $rel.Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip,
            $full,
            $entryName,
            [System.IO.Compression.CompressionLevel]::Optimal
        ) | Out-Null

        Write-Host "  + $entryName"
        $written++
    }
}
finally {
    $zip.Dispose()
}

Write-Host ""
Write-Host "Packed $written file(s) -> $Output"
if ($missing -gt 0) {
    Write-Warning "$missing file(s) missing."
}
