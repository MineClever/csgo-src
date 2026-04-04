# UpgradeToVS2022.ps1
# Generates a VS 2022 .sln from VPC-generated .vcxproj files and upgrades
# all .vcxproj toolsets from v140 (VS2015) to v143 (VS2022).
# Run automatically by CreateSolution.bat after VPC.

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$slnPath   = Join-Path $scriptDir "csgo_partner.sln"

# C++ project type GUID — constant across all VS versions
$cppTypeGuid = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

# -----------------------------------------------------------------------
# 1. Collect every .vcxproj VPC generated
# -----------------------------------------------------------------------
$vcxprojs = Get-ChildItem -Path $scriptDir -Filter "*.vcxproj" -Recurse |
    Sort-Object FullName

$projects = @()
$allConfigs = [System.Collections.Generic.HashSet[string]]::new()

foreach ($file in $vcxprojs) {
    [xml]$xml = Get-Content $file.FullName -Encoding UTF8

    # Project GUID
    $guid = $xml.Project.PropertyGroup |
        Where-Object { $_.Label -eq "Globals" } |
        Select-Object -First 1 |
        ForEach-Object { $_.ProjectGuid }

    if (-not $guid) { continue }

    # Project name
    $name = $xml.Project.PropertyGroup |
        Where-Object { $_.Label -eq "Globals" } |
        Select-Object -First 1 |
        ForEach-Object { $_.ProjectName }
    if (-not $name) { $name = [System.IO.Path]::GetFileNameWithoutExtension($file.Name) }

    # Configurations  (e.g. "Debug|Win32")
    $configs = $xml.Project.ItemGroup |
        Where-Object { $_.Label -eq "ProjectConfigurations" } |
        Select-Object -First 1 |
        ForEach-Object { $_.ProjectConfiguration } |
        ForEach-Object { $_.Include }

    foreach ($c in $configs) { [void]$allConfigs.Add($c) }

    $relPath = $file.FullName.Substring($scriptDir.Length + 1)

    $projects += [PSCustomObject]@{
        Guid     = $guid
        Name     = $name
        RelPath  = $relPath
        Configs  = $configs
    }
}

Write-Host "Found $($projects.Count) projects."

# -----------------------------------------------------------------------
# 2. Build .sln content
# -----------------------------------------------------------------------
$sb = [System.Text.StringBuilder]::new()

# Header — VS 2022 format
[void]$sb.AppendLine("")
[void]$sb.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00")
[void]$sb.AppendLine("# Visual Studio Version 17")
[void]$sb.AppendLine("VisualStudioVersion = 17.0.31903.59")
[void]$sb.AppendLine("MinimumVisualStudioVersion = 10.0.40219.1")

# Project entries
foreach ($p in $projects) {
    [void]$sb.AppendLine("Project(`"$cppTypeGuid`") = `"$($p.Name)`", `"$($p.RelPath)`", `"$($p.Guid)`"")
    [void]$sb.AppendLine("EndProject")
}

# Global sections
[void]$sb.AppendLine("Global")

# SolutionConfigurationPlatforms
[void]$sb.AppendLine("`tGlobalSection(SolutionConfigurationPlatforms) = preSolution")
foreach ($cfg in ($allConfigs | Sort-Object)) {
    [void]$sb.AppendLine("`t`t$cfg = $cfg")
}
[void]$sb.AppendLine("`tEndGlobalSection")

# ProjectConfigurationPlatforms
[void]$sb.AppendLine("`tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
foreach ($p in $projects) {
    foreach ($cfg in ($allConfigs | Sort-Object)) {
        # Only emit Build.0 if this project actually has this config
        if ($p.Configs -contains $cfg) {
            [void]$sb.AppendLine("`t`t$($p.Guid).$cfg.ActiveCfg = $cfg")
            [void]$sb.AppendLine("`t`t$($p.Guid).$cfg.Build.0 = $cfg")
        } else {
            # Map to first available config so VS doesn't complain
            $fallback = $p.Configs | Select-Object -First 1
            [void]$sb.AppendLine("`t`t$($p.Guid).$cfg.ActiveCfg = $fallback")
        }
    }
}
[void]$sb.AppendLine("`tEndGlobalSection")

[void]$sb.AppendLine("`tGlobalSection(SolutionProperties) = preSolution")
[void]$sb.AppendLine("`t`tHideSolutionNode = FALSE")
[void]$sb.AppendLine("`tEndGlobalSection")

[void]$sb.AppendLine("EndGlobal")

[System.IO.File]::WriteAllText($slnPath, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))
Write-Host "Written: $slnPath"

# -----------------------------------------------------------------------
# 3. Upgrade all .vcxproj toolsets v140 -> v143
# -----------------------------------------------------------------------
$upgraded = 0
foreach ($file in $vcxprojs) {
    $content  = Get-Content $file.FullName -Raw -Encoding UTF8
    $modified = $content -replace '<PlatformToolset>v140</PlatformToolset>',
                                   '<PlatformToolset>v143</PlatformToolset>'
    if ($modified -ne $content) {
        [System.IO.File]::WriteAllText($file.FullName, $modified, [System.Text.UTF8Encoding]::new($false))
        $upgraded++
    }
}
Write-Host "Upgraded toolset to v143 in $upgraded .vcxproj file(s)."
Write-Host "Done. Open csgo_partner.sln in Visual Studio 2022."
