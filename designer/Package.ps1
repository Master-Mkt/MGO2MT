param([Parameter(Mandatory=$true)][string]$Folder,[Parameter(Mandatory=$true)][string]$Verification)
$ErrorActionPreference='Stop'
$package=[IO.Path]::GetFullPath($Folder)
$validation=[IO.Path]::GetFullPath($Verification)
$manifest=Get-Content -LiteralPath (Join-Path $package 'package-manifest.json') -Raw|ConvertFrom-Json
$results=@{}
foreach($test in @(@('core','core-tests.json'),@('editor','editor-final/editor-tests.json'),@('audio','audio-final/audio-tests.json'),@('la2','la2-final/la2-designer-tests.json'))){$value=Get-Content -LiteralPath (Join-Path $validation $test[1]) -Raw|ConvertFrom-Json;if($value.status -ne 'pass'){throw "検証未完了: $($test[0])"};$results[$test[0]]=$value.checks}
$actual=@(Get-ChildItem -LiteralPath $package -Recurse -File -Force|Where-Object Name -ne 'package-manifest.json')
if($actual.Count -ne $manifest.files.Count){throw 'マニフェストの件数が一致しません。'}
foreach($row in $manifest.files){$file=[IO.Path]::GetFullPath((Join-Path $package $row.path));if(-not $file.StartsWith($package+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '出力範囲外のパスがあります。'};if((Get-Item -LiteralPath $file).Length -ne $row.size -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.sha256){throw "出力の内容が一致しません: $($row.path)"}}
$authored=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($provenance in @('sample-provenance.json','audio-provenance.json')){$p=Get-Content -LiteralPath (Join-Path $package ('samples/'+$provenance)) -Raw|ConvertFrom-Json;if($p.originalGameAssets -ne $false){throw '素材の出所確認が不足しています。'};foreach($row in $p.files){$path=Join-Path $package ('samples/'+$row.path);if((Get-Item -LiteralPath $path).Length -ne $row.size -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.sha256){throw "サンプル素材の出所記録が一致しません: $($row.path)"};$null=$authored.Add('samples/'+$row.path)}}
foreach($file in $actual){if($file.Extension -in @('.la2','.gwm','.gwfx','.mdn','.txn','.dld','.dlz','.cnp','.mp4')){throw 'ゲーム原素材が配布物にあります。'};if($file.Extension -in @('.png','.jpg','.jpeg','.dds','.gif','.tif','.tiff','.ico','.wav','.mp3')){$relative=[IO.Path]::GetRelativePath($package,$file.FullName).Replace('\','/');if(-not $authored.Contains($relative)){throw "未確認の画像・音声があります: $relative"}}}
$zip=$package+'.zip';if(Test-Path -LiteralPath $zip){throw '既存ZIPを確認し、別の出力名で作成してください。'}
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
$archive=[IO.Compression.ZipFile]::OpenRead($zip);try{if(@($archive.Entries|Where-Object Name).Count -ne $actual.Count+1){throw 'ZIPの件数が一致しません。'}}finally{$archive.Dispose()}
$report=@{format='MGO2MT.UI_DESIGNER_DELIVERY.1';app_build_version=$manifest.app_build_version;folder=$package;zip=$zip;zip_bytes=(Get-Item -LiteralPath $zip).Length;zip_sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant();files=$actual.Count+1;runtime=$manifest.runtime;native_dll_sha256=(Get-FileHash -LiteralPath (Join-Path $package 'MGO2MTMultiUI.dll') -Algorithm SHA256).Hash.ToLowerInvariant();tests=$results;verification=$validation;original_game_assets=$false;authored_sample_provenance_verified=$true;manifest_all_files_verified=$true;publication='local-only'}
$report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path ([IO.Path]::GetDirectoryName($package)) 'designer-delivery.json') -Encoding utf8
$report|ConvertTo-Json -Depth 8
