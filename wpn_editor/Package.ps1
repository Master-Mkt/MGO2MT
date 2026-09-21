param([Parameter(Mandatory=$true)][string]$Folder,[Parameter(Mandatory=$true)][string]$GuiReport,[int]$CoreChecks=0)
$ErrorActionPreference='Stop'
$package=[IO.Path]::GetFullPath($Folder)
$manifestPath=Join-Path $package 'package-manifest.json'
$manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
$gui=Get-Content -LiteralPath $GuiReport -Raw|ConvertFrom-Json
if(-not $gui.passed){throw '出力EXEのGUI確認が成功していません。'}
if([IO.Path]::GetFullPath([IO.Path]::GetDirectoryName($gui.app)) -ne $package){throw 'GUI確認が別の出力を参照しています。'}
$actual=@(Get-ChildItem -LiteralPath $package -File -Recurse -Force|Where-Object Name -ne 'package-manifest.json')
if($actual.Count -ne $manifest.files.Count){throw 'マニフェストの件数が一致しません。'}
foreach($row in $manifest.files){$path=[IO.Path]::GetFullPath((Join-Path $package $row.path));if(-not $path.StartsWith($package+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'マニフェストに範囲外のパスがあります。'};if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "出力ファイルがありません: $($row.path)"};$item=Get-Item -LiteralPath $path;if($item.Length -ne $row.size -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.sha256){throw "出力ファイルが一致しません: $($row.path)"}}
$assets=@($actual|Where-Object {$_.Extension -in @('.gwfx','.gwm','.mdn','.la2','.txn','.dld','.dlz','.cnp','.wav','.png','.dds','.jpg','.jpeg','.mp4')})
if($assets.Count){throw '配布物にゲーム素材または検証用画像・音声が混入しています。'}
$dllHash=(Get-FileHash -LiteralPath (Join-Path $package 'MGO2MTWpnEffects.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
if($dllHash -ne $gui.sharedDllSha256){throw 'GUI確認後に共有DLLが変わっています。'}
$zip=$package+'.zip'
if(Test-Path -LiteralPath $zip){throw 'ZIPが既にあります。既存の出力を確認し、別の出力名で作成してください。'}
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{$entries=@($archive.Entries|Where-Object {$_.Name});if($entries.Count -ne ($actual.Count+1)){throw 'ZIP内のファイル数が一致しません。'}}finally{$archive.Dispose()}
$report=@{format='MGO2MT.WPN_EDITOR_DELIVERY.1';app_build_version=$manifest.app_build_version;folder=$package;zip=$zip;zip_bytes=(Get-Item -LiteralPath $zip).Length;zip_sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant();files=$actual.Count+1;uncompressed_bytes=($actual|Measure-Object Length -Sum).Sum+(Get-Item -LiteralPath $manifestPath).Length;runtime=$manifest.runtime;native_dll_sha256=$dllHash;original_game_assets=$false;manifest_all_files_verified=$true;core_checks=$CoreChecks;gui_checks=$gui.checks;gui_report=[IO.Path]::GetFullPath($GuiReport);gui_original_texture_verified=$gui.originalPreview;screenshots=@('01-weapon-and-particles.png','02-effect-properties.png','03-sound-settings.png','04-weapon-icon.png'|ForEach-Object{Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($GuiReport))) $_})}
$delivery=Join-Path ([IO.Path]::GetDirectoryName($package)) 'editor-delivery.json'
$report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $delivery -Encoding utf8
Write-Output ($report|ConvertTo-Json -Depth 8)
