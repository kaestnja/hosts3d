[CmdletBinding()]
param(
  [ValidateSet('Random', 'TcpHandshake', 'TcpAck', 'TcpPayload', 'TcpReset', 'UdpPayload', 'IcmpEcho', 'IcmpBurst', 'Arp', 'Discovery')]
  [string]$Mode = 'Random',

  [string]$DestinationAddress = '127.0.0.1',

  [int]$Port = 10111,

  [ValidateRange(1, 255)]
  [int]$SensorId = 1,

  [string]$NetworkPrefix = '192.168.0',

  [ValidateRange(1, 254)]
  [int]$HostMin = 1,

  [ValidateRange(1, 254)]
  [int]$HostMax = 64,

  [ValidateRange(0, 1000000)]
  [int]$Count = 0,

  [ValidateRange(1, 60000)]
  [int]$DelayMs = 100,

  [ValidateRange(1, 65535)]
  [int]$TcpPort = 443,

  [ValidateRange(1, 65535)]
  [int]$UdpPort = 5353,

  [ValidateSet('Dns', 'mDns', 'Llmnr', 'NetBIOS')]
  [string]$DiscoveryKind = 'mDns',

  [string]$DiscoveryName = 'demo-host.local',

  [ValidateRange(1, 64)]
  [int]$BurstPackets = 6,

  [switch]$WideHostSpread,

  [string]$CenterIp = '',

  [ValidateSet('Mixed', 'Source', 'Destination')]
  [string]$CenterRole = 'Mixed'
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if ($HostMin -gt $HostMax) {
  throw "HostMin must be less than or equal to HostMax."
}

$script:PrefixParts = @()
foreach ($part in ($NetworkPrefix -split '\.')) {
  if ($part -notmatch '^\d{1,3}$') {
    throw "NetworkPrefix must look like '192.168.0'."
  }
  $value = [int]$part
  if (($value -lt 0) -or ($value -gt 255)) {
    throw "NetworkPrefix octets must be in the range 0-255."
  }
  $script:PrefixParts += $value
}
if ($script:PrefixParts.Count -ne 3) {
  throw "NetworkPrefix must look like '192.168.0'."
}

function ConvertTo-IPv4Bytes {
  param([Parameter(Mandatory = $true)][string]$IpText)
  $ip = [System.Net.IPAddress]::Parse($IpText)
  $bytes = $ip.GetAddressBytes()
  if ($bytes.Length -ne 4) {
    throw "Only IPv4 addresses are supported: $IpText"
  }
  return $bytes
}

function Get-PrefixPartsFromIp {
  param([Parameter(Mandatory = $true)][string]$IpText)
  $bytes = ConvertTo-IPv4Bytes -IpText $IpText
  return @([int]$bytes[0], [int]$bytes[1], [int]$bytes[2])
}

function New-Endpoint {
  param(
    [Parameter(Mandatory = $true)][string]$Address,
    [Parameter(Mandatory = $true)][int]$Port
  )
  $addr = [System.Net.IPAddress]::Parse($Address)
  return New-Object System.Net.IPEndPoint($addr, $Port)
}

function New-RandomMac {
  $bytes = New-Object byte[] 6
  [void]$script:Rng.NextBytes($bytes)
  $bytes[0] = ($bytes[0] -band 0xFE) -bor 0x02
  return $bytes
}

function Get-RandomEphemeralPort {
  return [uint16]$script:Rng.Next(1024, 65535)
}

function New-HostIp {
  do {
    $hostPart = $script:Rng.Next($HostMin, $HostMax + 1)
    if ($WideHostSpread) {
      $third = $script:Rng.Next(0, 255)
      $candidate = ('{0}.{1}.{2}.{3}' -f $script:PrefixParts[0], $script:PrefixParts[1], $third, $hostPart)
    }
    else {
      $candidate = ('{0}.{1}.{2}.{3}' -f $script:PrefixParts[0], $script:PrefixParts[1], $script:PrefixParts[2], $hostPart)
    }
  } while ($script:CenterIpText -and ($candidate -eq $script:CenterIpText))
  return $candidate
}

function New-HostPair {
  if ($script:CenterIpText) {
    $peerIp = New-HostIp
    switch ($CenterRole) {
      'Source' { return @($script:CenterIpText, $peerIp) }
      'Destination' { return @($peerIp, $script:CenterIpText) }
      default {
        if (($script:CenterFlip % 2) -eq 0) {
          $script:CenterFlip++
          return @($script:CenterIpText, $peerIp)
        }
        $script:CenterFlip++
        return @($peerIp, $script:CenterIpText)
      }
    }
  }
  $srcIp = New-HostIp
  $dstIp = New-HostIp
  while ($dstIp -eq $srcIp) {
    $dstIp = New-HostIp
  }
  return @($srcIp, $dstIp)
}

function Get-FlagsByte {
  param(
    [bool]$Syn = $false,
    [bool]$Ack = $false,
    [bool]$Rst = $false,
    [bool]$Fin = $false,
    [bool]$Psh = $false,
    [bool]$Payload = $false,
    [ValidateRange(0, 3)]
    [int]$ArpSubtype = 0
  )
  $flags = 0
  if ($Syn) { $flags = $flags -bor 0x01 }
  if ($Ack) { $flags = $flags -bor 0x02 }
  if ($Rst) { $flags = $flags -bor 0x04 }
  if ($Fin) { $flags = $flags -bor 0x08 }
  if ($Psh) { $flags = $flags -bor 0x10 }
  if ($Payload) { $flags = $flags -bor 0x20 }
  $flags = $flags -bor (($ArpSubtype -band 0x03) -shl 6)
  return [byte]$flags
}

function Get-DiscoveryPortValue {
  switch ($DiscoveryKind) {
    'Dns' { return [uint16]53 }
    'mDns' { return [uint16]5353 }
    'Llmnr' { return [uint16]5355 }
    'NetBIOS' { return [uint16]137 }
  }
}

function Get-DiscoveryResponderIp {
  switch ($DiscoveryKind) {
    'Dns' { return ('{0}.{1}.{2}.53' -f $script:PrefixParts[0], $script:PrefixParts[1], $script:PrefixParts[2]) }
    'mDns' { return ('{0}.{1}.{2}.251' -f $script:PrefixParts[0], $script:PrefixParts[1], $script:PrefixParts[2]) }
    'Llmnr' { return ('{0}.{1}.{2}.55' -f $script:PrefixParts[0], $script:PrefixParts[1], $script:PrefixParts[2]) }
    'NetBIOS' { return ('{0}.{1}.{2}.137' -f $script:PrefixParts[0], $script:PrefixParts[1], $script:PrefixParts[2]) }
  }
}

function Send-Pkex {
  param(
    [Parameter(Mandatory = $true)][byte]$Flags,
    [Parameter(Mandatory = $true)][uint32]$Size,
    [Parameter(Mandatory = $true)][byte[]]$Mac,
    [Parameter(Mandatory = $true)][uint16]$SrcPort,
    [Parameter(Mandatory = $true)][uint16]$DstPort,
    [Parameter(Mandatory = $true)][string]$SrcIp,
    [Parameter(Mandatory = $true)][string]$DstIp,
    [Parameter(Mandatory = $true)][byte]$Proto
  )

  $buffer = New-Object byte[] 26
  $buffer[0] = 85
  $buffer[1] = $Flags
  [BitConverter]::GetBytes($Size).CopyTo($buffer, 2)
  $Mac.CopyTo($buffer, 6)
  [BitConverter]::GetBytes($SrcPort).CopyTo($buffer, 12)
  [BitConverter]::GetBytes($DstPort).CopyTo($buffer, 14)
  (ConvertTo-IPv4Bytes $SrcIp).CopyTo($buffer, 16)
  (ConvertTo-IPv4Bytes $DstIp).CopyTo($buffer, 20)
  $buffer[24] = [byte]$SensorId
  $buffer[25] = $Proto
  [void]$script:Udp.Send($buffer, $buffer.Length, $script:Endpoint)
}

function Send-Pdns {
  param(
    [Parameter(Mandatory = $true)][string]$HostName,
    [Parameter(Mandatory = $true)][string]$IpText
  )

  $buffer = New-Object byte[] 261
  $buffer[0] = 42
  $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($HostName)
  $copyLen = [Math]::Min(255, $nameBytes.Length)
  [Array]::Copy($nameBytes, 0, $buffer, 1, $copyLen)
  (ConvertTo-IPv4Bytes $IpText).CopyTo($buffer, 257)
  [void]$script:Udp.Send($buffer, $buffer.Length, $script:Endpoint)
}

function Send-Conversation {
  param(
    [Parameter(Mandatory = $true)][string]$SrcIp,
    [Parameter(Mandatory = $true)][string]$DstIp,
    [Parameter(Mandatory = $true)][uint16]$SrcPort,
    [Parameter(Mandatory = $true)][uint16]$DstPort,
    [Parameter(Mandatory = $true)][byte]$Flags,
    [Parameter(Mandatory = $true)][uint32]$Size,
    [Parameter(Mandatory = $true)][byte]$Proto,
    [switch]$Reply
  )

  Send-Pkex -Flags $Flags -Size $Size -Mac (New-RandomMac) -SrcPort $SrcPort -DstPort $DstPort -SrcIp $SrcIp -DstIp $DstIp -Proto $Proto
  Start-Sleep -Milliseconds $DelayMs
  if ($Reply) {
    $replyFlags = $Flags
    if (($Proto -eq 6) -and (($Flags -band 0x01) -ne 0) -and (($Flags -band 0x02) -eq 0)) {
      $replyFlags = Get-FlagsByte -Syn $true -Ack $true
    }
    Send-Pkex -Flags $replyFlags -Size $Size -Mac (New-RandomMac) -SrcPort $DstPort -DstPort $SrcPort -SrcIp $DstIp -DstIp $SrcIp -Proto $Proto
    Start-Sleep -Milliseconds $DelayMs
  }
}

function Invoke-TcpHandshake {
  $pair = New-HostPair
  $srcIp = $pair[0]
  $dstIp = $pair[1]
  $srcPort = Get-RandomEphemeralPort
  Send-Pkex -Flags (Get-FlagsByte -Syn $true) -Size 60 -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $srcIp -DstIp $dstIp -Proto 6
  Start-Sleep -Milliseconds $DelayMs
  Send-Pkex -Flags (Get-FlagsByte -Syn $true -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $TcpPort -DstPort $srcPort -SrcIp $dstIp -DstIp $srcIp -Proto 6
  Start-Sleep -Milliseconds $DelayMs
  Send-Pkex -Flags (Get-FlagsByte -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $srcIp -DstIp $dstIp -Proto 6
  Start-Sleep -Milliseconds $DelayMs
}

function Invoke-TcpAck {
  $pair = New-HostPair
  Send-Conversation -SrcIp $pair[0] -DstIp $pair[1] -SrcPort (Get-RandomEphemeralPort) -DstPort $TcpPort -Flags (Get-FlagsByte -Ack $true) -Size 60 -Proto 6 -Reply
}

function Invoke-TcpPayload {
  $pair = New-HostPair
  $srcPort = Get-RandomEphemeralPort
  Send-Pkex -Flags (Get-FlagsByte -Ack $true -Psh $true -Payload $true) -Size ([uint32]($script:Rng.Next(200, 900))) -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $pair[0] -DstIp $pair[1] -Proto 6
  Start-Sleep -Milliseconds $DelayMs
  Send-Pkex -Flags (Get-FlagsByte -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $TcpPort -DstPort $srcPort -SrcIp $pair[1] -DstIp $pair[0] -Proto 6
  Start-Sleep -Milliseconds $DelayMs
}

function Invoke-TcpReset {
  $pair = New-HostPair
  $srcPort = Get-RandomEphemeralPort
  Send-Pkex -Flags (Get-FlagsByte -Syn $true) -Size 60 -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $pair[0] -DstIp $pair[1] -Proto 6
  Start-Sleep -Milliseconds $DelayMs
  Send-Pkex -Flags (Get-FlagsByte -Syn $true -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $TcpPort -DstPort $srcPort -SrcIp $pair[1] -DstIp $pair[0] -Proto 6
  Start-Sleep -Milliseconds $DelayMs
  Send-Pkex -Flags (Get-FlagsByte -Rst $true -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $pair[0] -DstIp $pair[1] -Proto 6
  Start-Sleep -Milliseconds $DelayMs
}

function Invoke-UdpPayload {
  $pair = New-HostPair
  Send-Conversation -SrcIp $pair[0] -DstIp $pair[1] -SrcPort (Get-RandomEphemeralPort) -DstPort $UdpPort -Flags (Get-FlagsByte -Payload $true) -Size ([uint32]($script:Rng.Next(80, 700))) -Proto 17 -Reply
}

function Invoke-IcmpEcho {
  $pair = New-HostPair
  Send-Conversation -SrcIp $pair[0] -DstIp $pair[1] -SrcPort 0 -DstPort 0 -Flags (Get-FlagsByte) -Size 84 -Proto 1 -Reply
}

function Invoke-IcmpBurst {
  $pair = New-HostPair
  for ($idx = 0; $idx -lt $BurstPackets; $idx++) {
    Send-Conversation -SrcIp $pair[0] -DstIp $pair[1] -SrcPort 0 -DstPort 0 -Flags (Get-FlagsByte) -Size 84 -Proto 1 -Reply
  }
}

function Invoke-Arp {
  $pair = New-HostPair
  $subtype = @(1, 2, 3)[$script:Rng.Next(0, 3)]
  Send-Pkex -Flags (Get-FlagsByte -ArpSubtype $subtype) -Size 60 -Mac (New-RandomMac) -SrcPort 0 -DstPort 0 -SrcIp $pair[0] -DstIp $pair[1] -Proto 249
  Start-Sleep -Milliseconds $DelayMs
}

function Invoke-Discovery {
  if ($script:CenterIpText) {
    $hostIp = $script:CenterIpText
  }
  else {
    $hostIp = New-HostIp
  }
  $resolverIp = Get-DiscoveryResponderIp
  $resolverPort = Get-DiscoveryPortValue
  $clientPort = Get-RandomEphemeralPort
  if ($resolverIp -eq $hostIp) {
    $pair = New-HostPair
    $hostIp = $pair[0]
    $resolverIp = $pair[1]
  }
  Send-Pkex -Flags (Get-FlagsByte -Payload $true) -Size ([uint32]($script:Rng.Next(140, 260))) -Mac (New-RandomMac) -SrcPort $clientPort -DstPort $resolverPort -SrcIp $hostIp -DstIp $resolverIp -Proto 17
  Start-Sleep -Milliseconds $DelayMs
  Send-Pkex -Flags (Get-FlagsByte -Payload $true) -Size ([uint32]($script:Rng.Next(160, 280))) -Mac (New-RandomMac) -SrcPort $resolverPort -DstPort $clientPort -SrcIp $resolverIp -DstIp $hostIp -Proto 17
  Start-Sleep -Milliseconds $DelayMs
  Send-Pdns -HostName $DiscoveryName -IpText $hostIp
  Start-Sleep -Milliseconds $DelayMs
}

function Invoke-RandomBurst {
  switch ($script:Rng.Next(0, 8)) {
    0 { Invoke-TcpHandshake }
    1 { Invoke-TcpAck }
    2 { Invoke-TcpPayload }
    3 { Invoke-TcpReset }
    4 { Invoke-UdpPayload }
    5 { Invoke-IcmpEcho }
    6 { Invoke-Arp }
    default { Invoke-Discovery }
  }
}

$script:CenterIpText = ''
$script:CenterFlip = 0
if ($CenterIp) {
  [void](ConvertTo-IPv4Bytes -IpText $CenterIp)
  $script:CenterIpText = $CenterIp
  $script:PrefixParts = Get-PrefixPartsFromIp -IpText $CenterIp
}

$script:Rng = New-Object System.Random
$script:Udp = New-Object System.Net.Sockets.UdpClient
$script:Endpoint = New-Endpoint -Address $DestinationAddress -Port $Port

Write-Host "Synthetic hsen sender"
Write-Host "Mode: $Mode  Destination: $DestinationAddress`:$Port  Sensor: $SensorId"
if ($script:CenterIpText) {
  Write-Host "Center Host: $($script:CenterIpText)  Role: $CenterRole"
}
if ($Mode -eq 'Discovery') {
  Write-Host "DiscoveryKind: $DiscoveryKind  DiscoveryName: $DiscoveryName"
}
if ($WideHostSpread) {
  Write-Host "Host spread: wide (random third and fourth octets within the chosen prefix family)"
}
if ($Count -eq 0) {
  Write-Host "Count: infinite  Ctrl+C to stop"
} else {
  Write-Host "Count: $Count"
}

$sent = 0
while (($Count -eq 0) -or ($sent -lt $Count)) {
  switch ($Mode) {
    'Random' { Invoke-RandomBurst }
    'TcpHandshake' { Invoke-TcpHandshake }
    'TcpAck' { Invoke-TcpAck }
    'TcpPayload' { Invoke-TcpPayload }
    'TcpReset' { Invoke-TcpReset }
    'UdpPayload' { Invoke-UdpPayload }
    'IcmpEcho' { Invoke-IcmpEcho }
    'IcmpBurst' { Invoke-IcmpBurst }
    'Arp' { Invoke-Arp }
    'Discovery' { Invoke-Discovery }
  }
  $sent++
}

$script:Udp.Close()
Write-Host "Finished after $sent burst(s)."
