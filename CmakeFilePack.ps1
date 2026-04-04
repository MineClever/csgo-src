# CmakeFilePack.ps1
# 将 CMake 构建系统相关文件打包为 cmake_patch.zip

param(
    [string]$Root = $PSScriptRoot,
    [string]$Output = ""
)

if (-not $Output) {
    $Output = Join-Path $Root "cmake_patch.zip"
}

if (Test-Path $Output) { Remove-Item $Output -Force }

$files = @(
    "CMakeLists.txt",
    "CmakeCreateSolution.bat",
    "CmakeBuildSolution.bat",
    "CmakeFilePackToZip.bat",
    "CmakeFilePack.ps1",
    "src\CreateSolution.bat",
    "src\UpgradeToVS2022.ps1",
    "src\tools\extract_sources.py"
)

# src\cmake\ 下所有文件
Get-ChildItem -Path (Join-Path $Root "src\cmake") -File | ForEach-Object {
    $files += "src\cmake\$($_.Name)"
}

# src\ 下所有 CMakeLists.txt（递归）
Get-ChildItem -Path (Join-Path $Root "src") -Filter "CMakeLists.txt" -Recurse | ForEach-Object {
    $files += $_.FullName.Substring($Root.Length).TrimStart('\')
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($Output, 'Create')

$written = 0
$missing = 0
foreach ($rel in $files) {
    $full = Join-Path $Root $rel
    if (Test-Path $full) {
        $entryName = $rel.Replace('\', '/')
        $entry  = $zip.CreateEntry($entryName)
        $stream = $entry.Open()
        $bytes  = [System.IO.File]::ReadAllBytes($full)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Close()
        Write-Host "  + $entryName"
        $written++
    } else {
        Write-Warning "MISSING: $rel"
        $missing++
    }
}
$zip.Dispose()

Write-Host ""
Write-Host "Packed $written file(s) -> $Output"
if ($missing -gt 0) { Write-Warning "$missing file(s) missing." }
