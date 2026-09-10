param([switch]$Apply)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$targetRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'dist'))
$reportPath = Join-Path $projectRoot 'outputs\agreement\firewall_folder.json'
$groupName = 'MGO2WIN OpenMGO2 HTTPS'
try {
    if (-not (Test-Path -LiteralPath $targetRoot -PathType Container)) { throw 'dist folder missing' }
    $queue = New-Object 'System.Collections.Generic.Queue[string]'
    $queue.Enqueue($targetRoot)
    $programs = @()
    $prefix = $targetRoot.TrimEnd('\') + '\'
    while ($queue.Count) {
        $directory = Get-Item -LiteralPath $queue.Dequeue()
        if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked directories are not allowed' }
        foreach ($item in Get-ChildItem -LiteralPath $directory.FullName) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if (-not $item.FullName.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Path outside dist' }
            if ($item.PSIsContainer) { $queue.Enqueue($item.FullName) }
            elseif ($item.Extension -ieq '.exe') {
                $hasher = [Security.Cryptography.SHA256]::Create()
                try { $id = [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($item.FullName.ToLowerInvariant()))).Replace('-','').ToLowerInvariant() }
                finally { $hasher.Dispose() }
                $programs += [pscustomobject]@{path=$item.FullName;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant();rule=('MGO2WIN-OpenMGO2-'+$id.Substring(0,24))}
            }
        }
    }
    if (-not $programs.Count) { throw 'No EXE found in dist' }
    $addresses = @([Net.Dns]::GetHostAddresses('openmgo2.com') | ForEach-Object { $_.IPAddressToString } | Sort-Object -Unique)
    if (-not $addresses.Count) { throw 'OpenMGO2 DNS lookup failed' }
    $plan = [ordered]@{status='planned';root=$targetRoot;host='openmgo2.com';remote_addresses=$addresses;remote_port=443;protocol='TCP';direction='Outbound';programs=$programs;created=@();updated=@();error=$null}
    if (-not $Apply) { $plan | ConvertTo-Json -Depth 6; exit 0 }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        # Windows asks the user for administrator consent. No scheduled task,
        # service, credential storage or permanent execution-policy change.
        $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Apply'
        $child = Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -Wait -PassThru
        exit $child.ExitCode
    }
    foreach ($program in $programs) {
        if ((Get-FileHash -LiteralPath $program.path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $program.sha256) { throw 'EXE changed during registration' }
        $existing = Get-NetFirewallRule -Name $program.rule -ErrorAction SilentlyContinue
        if ($existing) {
            $application = $existing | Get-NetFirewallApplicationFilter
            if ($existing.Group -ne $groupName -or $application.Program -ine $program.path) { throw 'Rule ownership mismatch' }
            Set-NetFirewallRule -Name $program.rule -Enabled True -Direction Outbound -Action Allow -Profile Any -Protocol TCP -RemotePort 443 -RemoteAddress $addresses | Out-Null
            $plan.updated += $program.rule
        } else {
            New-NetFirewallRule -Name $program.rule -DisplayName ('MGO2WIN OpenMGO2 HTTPS: '+$program.path.Substring($prefix.Length)) -Group $groupName -Enabled True -Direction Outbound -Action Allow -Profile Any -Program $program.path -Protocol TCP -RemotePort 443 -RemoteAddress $addresses | Out-Null
            $plan.created += $program.rule
        }
    }
    $plan.status = 'applied'
    $plan | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Output ('OpenMGO2 HTTPS allowed for '+$programs.Count+' EXEs in dist. Re-run after adding/moving builds or DNS changes.')
} catch {
    if ($plan) { $plan.status='failed';$plan.error=$_.Exception.Message;$plan | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath -Encoding UTF8 }
    Write-Error $_
    exit 1
}
