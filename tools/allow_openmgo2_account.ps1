param([Parameter(Mandatory=$true)][string]$Program,[switch]$Apply)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$distRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot 'dist'))
$exePath=[IO.Path]::GetFullPath($Program)
if (-not $exePath.StartsWith($distRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Program must be inside this project dist folder'}
$item=Get-Item -LiteralPath $exePath
if ($item.PSIsContainer -or $item.Name -ine 'MGO2WIN.exe') {throw 'Expected packaged MGO2WIN.exe'}
$cursor=$item
while ($cursor -and $cursor.FullName -ine $distRoot) {
    if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Linked paths are not allowed'}
    $cursor=if($cursor -is [IO.DirectoryInfo]){$cursor.Parent}else{$cursor.Directory}
}
if (-not $cursor -or ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint)) {throw 'Invalid dist root'}
$address='49.212.132.180'
if ($address -notin @([Net.Dns]::GetHostAddresses('openmgo2.com') | ForEach-Object IPAddressToString)) {throw 'OpenMGO2 address changed; review the pinned native endpoint first'}
$hash=(Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
$hasher=[Security.Cryptography.SHA256]::Create()
try {$id=[BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($exePath.ToLowerInvariant()))).Replace('-','').ToLowerInvariant()}finally{$hasher.Dispose()}
$ruleName='MGO2WIN-ACCOUNT-'+$id.Substring(0,24)
$group='MGO2WIN OpenMGO2 Account'
$plan=[ordered]@{status='planned';program=$exePath;sha256=$hash;host='openmgo2.com';remote_address=$address;remote_ports=@(5731,5732);protocol='TCP';direction='Outbound';rule=$ruleName;inbound_rule_added=$false}
if (-not $Apply){$plan | ConvertTo-Json;exit 0}
$principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Program "'+$exePath+'" -Apply'
    $child=Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    exit $child.ExitCode
}
if ((Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash){throw 'EXE changed'}
$existing=Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
if($existing){
    $app=$existing | Get-NetFirewallApplicationFilter
    if($existing.Group -ne $group -or $app.Program -ine $exePath){throw 'Rule ownership mismatch'}
    Set-NetFirewallRule -Name $ruleName -Enabled True -Direction Outbound -Action Allow -Profile Any -Protocol TCP -RemoteAddress $address -RemotePort 5731,5732 | Out-Null
}else{
    New-NetFirewallRule -Name $ruleName -DisplayName ('MGO2WIN OpenMGO2 Account: '+$item.Directory.Name) -Group $group -Enabled True -Direction Outbound -Action Allow -Profile Any -Program $exePath -Protocol TCP -RemoteAddress $address -RemotePort 5731,5732 | Out-Null
}
$plan.status='applied'
$out=Join-Path $projectRoot 'outputs\characters';New-Item -ItemType Directory -Path $out -Force | Out-Null
$plan | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'firewall.json') -Encoding UTF8
Write-Output 'This EXE may send TCP only to OpenMGO2:5731,5732. No inbound rule or default policy changed.'

