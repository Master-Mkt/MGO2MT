param(
  [string]$Output = (Join-Path $PSScriptRoot 'output/MGO2MTMultiUIDesigner'),
  [string]$NativeDll = (Join-Path $PSScriptRoot '../build-mt/Release/MGO2MTMultiUI.dll'),
  [string]$Version = 'development',
  [switch]$FrameworkDependentOnly
)
$ErrorActionPreference = 'Stop'
$targetDirectory = [IO.Path]::GetFullPath($Output)
if (-not (Test-Path -LiteralPath $NativeDll -PathType Leaf)) { throw '先に MGO2MTMultiUI.dll をビルドして -NativeDll で指定してください。' }
$publishArguments = @('publish', (Join-Path $PSScriptRoot 'App/MGO2MTMultiUIDesigner.csproj'), '-c', 'Release', '--self-contained', 'false', '--configfile', (Join-Path $PSScriptRoot 'NuGet.Config'), '-o', $targetDirectory, '--nologo')
if (-not $FrameworkDependentOnly) { $publishArguments += @('-p:AppHostRelativeDotNet=.runtime', '-p:AppHostDotNetSearch=AppRelative') }
& dotnet @publishArguments
if ($LASTEXITCODE -ne 0) { throw 'Designer publish failed' }
Copy-Item -LiteralPath $NativeDll -Destination (Join-Path $targetDirectory 'MGO2MTMultiUI.dll') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.ja.md') -Destination $targetDirectory -Force
# Supplemental private research notes are not public package inputs.
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'samples') -Destination $targetDirectory -Recurse -Force
$runtimeInfo = $null
if (-not $FrameworkDependentOnly) {
  $sdkExecutable = (Get-Command dotnet).Source
  $sdkRoot = Split-Path -Parent $sdkExecutable
  $coreRoot = Join-Path $sdkRoot 'shared/Microsoft.NETCore.App'
  $desktopRoot = Join-Path $sdkRoot 'shared/Microsoft.WindowsDesktop.App'
  $runtimeVersion = (Get-ChildItem -LiteralPath $desktopRoot -Directory | Where-Object { $_.Name -match '^10\.0\.\d+$' -and (Test-Path -LiteralPath (Join-Path $coreRoot $_.Name)) } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name
  if (-not $runtimeVersion) { throw '一致する .NET 10 / WindowsDesktop 10 実行環境がありません。' }
  $runtimeDirectory = Join-Path $targetDirectory '.runtime'
  foreach ($relative in @('shared/Microsoft.NETCore.App', 'shared/Microsoft.WindowsDesktop.App', 'host/fxr')) {
    $source = Join-Path (Join-Path $sdkRoot $relative) $runtimeVersion
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "実行環境が不足しています: $source" }
    $destination = Join-Path $runtimeDirectory $relative
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
  }
  foreach ($notice in @('LICENSE.txt','ThirdPartyNotices.txt')) {
    Copy-Item -LiteralPath (Join-Path $sdkRoot $notice) -Destination (Join-Path $runtimeDirectory $notice) -Force
  }
  $runtimeInfo = @{ mode='framework-dependent app with private app-relative runtime'; version=$runtimeVersion; source=$sdkRoot; installation_required=$false; official_self_contained_publish=$false }
}
$files = @(Get-ChildItem -LiteralPath $targetDirectory -File -Recurse | Where-Object Name -ne 'package-manifest.json' | ForEach-Object { @{path=[IO.Path]::GetRelativePath($targetDirectory,$_.FullName).Replace('\','/');size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
@{format='MGO2MT.UI_DESIGNER_PACKAGE.1';version=$Version;app_build_version=$Version;runtime=$runtimeInfo;files=$files;native_rendering=$true;external_nuget_packages=0;original_game_assets=$false} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $targetDirectory 'package-manifest.json') -Encoding utf8
Write-Output "Designer output: $targetDirectory ($($files.Count) files)"

