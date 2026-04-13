[CmdletBinding()]
param(
  [string]$DestinationAddress = '127.0.0.1',

  [int]$Port = 10111,

  [ValidateRange(1, 255)]
  [int]$SensorId = 1,

  [string]$CenterIp = '',

  [string]$LogPath = '',

  [ValidateRange(0, 5000)]
  [int]$PauseBetweenStepsMs = 150,

  [string]$StatePath = ''
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$script:DemoLogPath = $LogPath
if ($script:DemoLogPath) {
  $logDir = Split-Path -Parent $script:DemoLogPath
  if ($logDir -and -not (Test-Path -LiteralPath $logDir)) {
    [void](New-Item -ItemType Directory -Path $logDir -Force)
  }
  Set-Content -LiteralPath $script:DemoLogPath -Value '' -Encoding UTF8
}

if ($StatePath) {
  $stateDir = Split-Path -Parent $StatePath
  if ($stateDir -and -not (Test-Path -LiteralPath $stateDir)) {
    [void](New-Item -ItemType Directory -Path $stateDir -Force)
  }
  Set-Content -LiteralPath $StatePath -Value ("pid={0}`nstarted={1:o}" -f $PID, (Get-Date)) -Encoding ASCII
}

function Write-DemoLine {
  param([Parameter(Mandatory = $true)][string]$Text)
  Write-Host $Text
  if ($script:DemoLogPath) {
    Add-Content -LiteralPath $script:DemoLogPath -Value $Text -Encoding UTF8
  }
}

function Get-ModeSleepFactor {
  param(
    [Parameter(Mandatory = $true)][string]$Mode,
    [int]$BurstPackets = 1
  )
  switch ($Mode) {
    'TcpHandshake' { return 3.0 }
    'TcpAck' { return 2.0 }
    'TcpPayload' { return 2.0 }
    'TcpReset' { return 3.0 }
    'UdpPayload' { return 2.0 }
    'IcmpEcho' { return 2.0 }
    'IcmpBurst' { return (2.0 * $BurstPackets) }
    'Arp' { return 1.0 }
    'Discovery' { return 3.0 }
    default { return 2.0 }
  }
}

function Get-StageEstimateSeconds {
  param([hashtable]$Stage)
  $burstPackets = 1
  if ($Stage.ContainsKey('BurstPackets') -and $Stage.BurstPackets) {
    $burstPackets = [int]$Stage.BurstPackets
  }
  $factor = Get-ModeSleepFactor -Mode $Stage.Mode -BurstPackets $burstPackets
  $sleepSeconds = ($Stage.Count * $Stage.DelayMs * $factor) / 1000.0
  return ($sleepSeconds + 0.20)
}

function Invoke-DemoStage {
  param(
    [Parameter(Mandatory = $true)][hashtable]$Stage,
    [Parameter(Mandatory = $true)][string]$SenderPath
  )

  $estimate = Get-StageEstimateSeconds -Stage $Stage
  $args = @{
    Mode = $Stage.Mode
    DestinationAddress = $DestinationAddress
    Port = $Port
    SensorId = $SensorId
    Count = $Stage.Count
    DelayMs = $Stage.DelayMs
  }
  if ($CenterIp) { $args.CenterIp = $CenterIp }
  if ($Stage.ContainsKey('WideHostSpread') -and $Stage.WideHostSpread) { $args.WideHostSpread = $true }
  if ($Stage.ContainsKey('TcpPort')) { $args.TcpPort = $Stage.TcpPort }
  if ($Stage.ContainsKey('UdpPort')) { $args.UdpPort = $Stage.UdpPort }
  if ($Stage.ContainsKey('BurstPackets')) { $args.BurstPackets = $Stage.BurstPackets }
  if ($Stage.ContainsKey('DiscoveryKind')) { $args.DiscoveryKind = $Stage.DiscoveryKind }
  if ($Stage.ContainsKey('DiscoveryName')) { $args.DiscoveryName = $Stage.DiscoveryName }

  Write-DemoLine ("[{0}] {1}  est. {2:N1}s" -f $Stage.Name, $Stage.Mode, $estimate)
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  & $SenderPath @args
  $sw.Stop()
  Write-DemoLine ("[{0}] done in {1:N1}s" -f $Stage.Name, $sw.Elapsed.TotalSeconds)
  Start-Sleep -Milliseconds $PauseBetweenStepsMs
  return $estimate, $sw.Elapsed.TotalSeconds
}

$senderPath = Join-Path $PSScriptRoot 'sim-hsen.ps1'
if (-not (Test-Path -LiteralPath $senderPath)) {
  throw "Missing sender script: $senderPath"
}

try {
  $stages = @(
    @{ Name = 'TCP setup/finish'; Mode = 'TcpHandshake'; Count = 6; DelayMs = 120; TcpPort = 443 },
    @{ Name = 'TCP ACK'; Mode = 'TcpAck'; Count = 6; DelayMs = 100; TcpPort = 443 },
    @{ Name = 'TCP payload'; Mode = 'TcpPayload'; Count = 6; DelayMs = 100; TcpPort = 443 },
    @{ Name = 'TCP reset'; Mode = 'TcpReset'; Count = 5; DelayMs = 120; TcpPort = 443 },
    @{ Name = 'UDP payload'; Mode = 'UdpPayload'; Count = 6; DelayMs = 100; UdpPort = 5353 },
    @{ Name = 'ICMP echo'; Mode = 'IcmpEcho'; Count = 5; DelayMs = 80 },
    @{ Name = 'ICMP burst'; Mode = 'IcmpBurst'; Count = 4; DelayMs = 60; BurstPackets = 4 },
    @{ Name = 'ARP'; Mode = 'Arp'; Count = 6; DelayMs = 100 },
    @{ Name = 'DNS discovery'; Mode = 'Discovery'; Count = 4; DelayMs = 120; DiscoveryKind = 'Dns'; DiscoveryName = 'gateway.example.net' },
    @{ Name = 'mDNS discovery'; Mode = 'Discovery'; Count = 5; DelayMs = 120; DiscoveryKind = 'mDns'; DiscoveryName = 'printer.office.local' },
    @{ Name = 'LLMNR discovery'; Mode = 'Discovery'; Count = 4; DelayMs = 120; DiscoveryKind = 'Llmnr'; DiscoveryName = 'workstation.local' },
    @{ Name = 'NetBIOS discovery'; Mode = 'Discovery'; Count = 5; DelayMs = 120; DiscoveryKind = 'NetBIOS'; DiscoveryName = 'NAS-BOX' },
    @{ Name = 'Random mix'; Mode = 'Random'; Count = 8; DelayMs = 80 },
    @{ Name = 'Wide host spread'; Mode = 'TcpAck'; Count = 10; DelayMs = 60; TcpPort = 80; WideHostSpread = $true }
  )

  $estimatedTotal = 0.0
  foreach ($stage in $stages) { $estimatedTotal += Get-StageEstimateSeconds -Stage $stage }

  Write-DemoLine "Synthetic Hosts3D quick demo (PowerShell)"
  Write-DemoLine ("Destination: {0}:{1}  Sensor: {2}" -f $DestinationAddress, $Port, $SensorId)
  if ($CenterIp) {
    Write-DemoLine ("Center Host: {0}" -f $CenterIp)
  }
  Write-DemoLine ("Estimated total runtime: {0:N1}s" -f $estimatedTotal)

  $totalSw = [System.Diagnostics.Stopwatch]::StartNew()
  foreach ($stage in $stages) {
    [void](Invoke-DemoStage -Stage $stage -SenderPath $senderPath)
  }
  $totalSw.Stop()

  Write-DemoLine ("Estimated total runtime: {0:N1}s" -f $estimatedTotal)
  Write-DemoLine ("Actual total runtime: {0:N1}s" -f $totalSw.Elapsed.TotalSeconds)
  Write-DemoLine "PowerShell demo finished."
}
finally {
  if ($StatePath) {
    Remove-Item -LiteralPath $StatePath -Force -ErrorAction SilentlyContinue
  }
}
