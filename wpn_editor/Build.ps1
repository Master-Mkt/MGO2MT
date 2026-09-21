param([string]$Output=(Join-Path $PSScriptRoot 'output/MGO2MTWpnEffectEditor'),[string]$NativeDll=(Join-Path $PSScriptRoot '../build/Release/MGO2MTWpnEffects.dll'),[string]$Version='development',[switch]$FrameworkDependentOnly)
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($Output)
if(-not(Test-Path -LiteralPath $NativeDll -PathType Leaf)){throw 'MGO2MTWpnEffects.dll がありません。'}
$arguments=@('publish',(Join-Path $PSScriptRoot 'App/MGO2MTWpnEffectEditor.csproj'),'-c','Release','--self-contained','false','--configfile',(Join-Path $PSScriptRoot 'NuGet.Config'),'-o',$target,'--nologo')
if(-not $FrameworkDependentOnly){$arguments+=@('-p:AppHostRelativeDotNet=.runtime','-p:AppHostDotNetSearch=AppRelative')}
& dotnet @arguments
if($LASTEXITCODE -ne 0){throw 'WPN Editor publish failed'}
Copy-Item -LiteralPath $NativeDll -Destination (Join-Path $target 'MGO2MTWpnEffects.dll') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.ja.md') -Destination $target -Force
$runtimeInfo=$null
if(-not $FrameworkDependentOnly){
 $sdkRoot=Split-Path -Parent (Get-Command dotnet).Source
 $core=Join-Path $sdkRoot 'shared/Microsoft.NETCore.App';$desktop=Join-Path $sdkRoot 'shared/Microsoft.WindowsDesktop.App'
 $runtimeVersion=(Get-ChildItem -LiteralPath $desktop -Directory|Where-Object{$_.Name -match '^10\.0\.\d+$' -and (Test-Path -LiteralPath (Join-Path $core $_.Name))}|Sort-Object{[version]$_.Name} -Descending|Select-Object -First 1).Name
 if(-not $runtimeVersion){throw '.NET 10 Windows DesktopとCoreの一致する実行環境が必要です。'}
 $runtime=Join-Path $target '.runtime'
 foreach($relative in @('shared/Microsoft.NETCore.App','shared/Microsoft.WindowsDesktop.App','host/fxr')){$source=Join-Path (Join-Path $sdkRoot $relative) $runtimeVersion;$dest=Join-Path $runtime $relative;New-Item -ItemType Directory -Path $dest -Force|Out-Null;Copy-Item -LiteralPath $source -Destination $dest -Recurse -Force}
 foreach($notice in @('LICENSE.txt','ThirdPartyNotices.txt')){Copy-Item -LiteralPath (Join-Path $sdkRoot $notice) -Destination (Join-Path $runtime $notice) -Force}
 $runtimeInfo=@{mode='private app-relative official runtime';version=$runtimeVersion;installation_required=$false;official_self_contained_publish=$false}
}
$files=@(Get-ChildItem -LiteralPath $target -File -Recurse|Where-Object Name -ne 'package-manifest.json'|ForEach-Object{@{path=[IO.Path]::GetRelativePath($target,$_.FullName).Replace('\','/');size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
@{format='MGO2MT.WPN_EDITOR_PACKAGE.1';app_build_version=$Version;runtime=$runtimeInfo;original_game_assets=$false;external_nuget_packages=0;files=$files}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $target 'package-manifest.json') -Encoding utf8
Write-Output "WPN Editor output: $target ($($files.Count) files)"
