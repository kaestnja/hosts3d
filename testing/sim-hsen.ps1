[CmdletBinding()]
param(
  [ValidateSet('Random', 'TcpHandshake', 'TcpPayload', 'UdpPayload', 'Arp', 'Discovery')]
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

  [string]$DiscoveryName = 'demo-host.local'
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if ($HostMin -gt $HostMax) {
  throw "HostMin must be less than or equal to HostMax."
}
if ($NetworkPrefix -notmatch '^\d{1,3}\.\d{1,3}\.\d{1,3}$') {
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

function New-HostIp {
  $hostPart = $script:Rng.Next($HostMin, $HostMax + 1)
  return "$NetworkPrefix.$hostPart"
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
    [string]$SrcIp,
    [string]$DstIp,
    [uint16]$SrcPort,
    [uint16]$DstPort,
    [byte]$Flags,
    [uint32]$Size,
    [byte]$Proto,
    [switch]$Reply
  )

  $srcMac = New-RandomMac
  Send-Pkex -Flags $Flags -Size $Size -Mac $srcMac -SrcPort $SrcPort -DstPort $DstPort -SrcIp $SrcIp -DstIp $DstIp -Proto $Proto
  Start-Sleep -Milliseconds $DelayMs
  if ($Reply) {
    $replyFlags = $Flags
    if ($Proto -eq 6 -and (($Flags -band 0x01) -ne 0) -and (($Flags -band 0x02) -eq 0)) {
      $replyFlags = Get-FlagsByte -Syn $true -Ack $true
    }
    Send-Pkex -Flags $replyFlags -Size $Size -Mac (New-RandomMac) -SrcPort $DstPort -DstPort $SrcPort -SrcIp $DstIp -DstIp $SrcIp -Proto $Proto
    Start-Sleep -Milliseconds $DelayMs
  }
}

function Invoke-RandomBurst {
  $srcIp = New-HostIp
  $dstIp = New-HostIp
  while ($dstIp -eq $srcIp) { $dstIp = New-HostIp }
  switch ($script:Rng.Next(0, 5)) {
    0 { Send-Conversation -SrcIp $srcIp -DstIp $dstIp -SrcPort 0 -DstPort 0 -Flags (Get-FlagsByte) -Size 84 -Proto 1 -Reply }
    1 { Send-Conversation -SrcIp $srcIp -DstIp $dstIp -SrcPort ([uint16]$script:Rng.Next(1024, 65535)) -DstPort $TcpPort -Flags (Get-FlagsByte -Payload $true -Ack $true -Psh $true) -Size 320 -Proto 6 -Reply }
    2 { Send-Conversation -SrcIp $srcIp -DstIp $dstIp -SrcPort ([uint16]$script:Rng.Next(1024, 65535)) -DstPort $UdpPort -Flags (Get-FlagsByte -Payload $true) -Size 160 -Proto 17 -Reply }
    3 {
      $arpSubtype = $script:Rng.Next(1, 4)
      Send-Pkex -Flags (Get-FlagsByte -ArpSubtype $arpSubtype) -Size 60 -Mac (New-RandomMac) -SrcPort 0 -DstPort 0 -SrcIp $srcIp -DstIp $dstIp -Proto 249
      Start-Sleep -Milliseconds $DelayMs
    }
    default {
      Send-Conversation -SrcIp $srcIp -DstIp $dstIp -SrcPort ([uint16]$script:Rng.Next(1024, 65535)) -DstPort $TcpPort -Flags (Get-FlagsByte -Syn $true) -Size 60 -Proto 6 -Reply
      Send-Pkex -Flags (Get-FlagsByte -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort ([uint16]$script:Rng.Next(1024, 65535)) -DstPort $TcpPort -SrcIp $srcIp -DstIp $dstIp -Proto 6
      Start-Sleep -Milliseconds $DelayMs
    }
  }
}

$script:Rng = New-Object System.Random
$script:Udp = New-Object System.Net.Sockets.UdpClient
$script:Endpoint = New-Endpoint -Address $DestinationAddress -Port $Port

Write-Host "Synthetic hsen sender"
Write-Host "Mode: $Mode  Destination: $DestinationAddress`:$Port  Sensor: $SensorId"
if ($Count -eq 0) {
  Write-Host "Count: infinite  Ctrl+C to stop"
} else {
  Write-Host "Count: $Count"
}

$sent = 0
while (($Count -eq 0) -or ($sent -lt $Count)) {
  switch ($Mode) {
    'Random' {
      Invoke-RandomBurst
    }
    'TcpHandshake' {
      $srcIp = New-HostIp
      $dstIp = New-HostIp
      $srcPort = [uint16]$script:Rng.Next(1024, 65535)
      Send-Pkex -Flags (Get-FlagsByte -Syn $true) -Size 60 -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $srcIp -DstIp $dstIp -Proto 6
      Start-Sleep -Milliseconds $DelayMs
      Send-Pkex -Flags (Get-FlagsByte -Syn $true -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $TcpPort -DstPort $srcPort -SrcIp $dstIp -DstIp $srcIp -Proto 6
      Start-Sleep -Milliseconds $DelayMs
      Send-Pkex -Flags (Get-FlagsByte -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $srcIp -DstIp $dstIp -Proto 6
      Start-Sleep -Milliseconds $DelayMs
    }
    'TcpPayload' {
      $srcIp = New-HostIp
      $dstIp = New-HostIp
      $srcPort = [uint16]$script:Rng.Next(1024, 65535)
      Send-Pkex -Flags (Get-FlagsByte -Ack $true -Psh $true -Payload $true) -Size ([uint32]($script:Rng.Next(200, 900))) -Mac (New-RandomMac) -SrcPort $srcPort -DstPort $TcpPort -SrcIp $srcIp -DstIp $dstIp -Proto 6
      Start-Sleep -Milliseconds $DelayMs
      Send-Pkex -Flags (Get-FlagsByte -Ack $true) -Size 60 -Mac (New-RandomMac) -SrcPort $TcpPort -DstPort $srcPort -SrcIp $dstIp -DstIp $srcIp -Proto 6
      Start-Sleep -Milliseconds $DelayMs
    }
    'UdpPayload' {
      $srcIp = New-HostIp
      $dstIp = New-HostIp
      Send-Conversation -SrcIp $srcIp -DstIp $dstIp -SrcPort ([uint16]$script:Rng.Next(1024, 65535)) -DstPort $UdpPort -Flags (Get-FlagsByte -Payload $true) -Size ([uint32]($script:Rng.Next(80, 700))) -Proto 17 -Reply
    }
    'Arp' {
      $srcIp = New-HostIp
      $dstIp = New-HostIp
      $subtype = @(1, 2, 3)[$script:Rng.Next(0, 3)]
      Send-Pkex -Flags (Get-FlagsByte -ArpSubtype $subtype) -Size 60 -Mac (New-RandomMac) -SrcPort 0 -DstPort 0 -SrcIp $srcIp -DstIp $dstIp -Proto 249
      Start-Sleep -Milliseconds $DelayMs
    }
    'Discovery' {
      $hostIp = New-HostIp
      $dnsIp = "$NetworkPrefix.53"
      Send-Pkex -Flags (Get-FlagsByte -Payload $true) -Size 180 -Mac (New-RandomMac) -SrcPort 53 -DstPort ([uint16]$script:Rng.Next(1024, 65535)) -SrcIp $dnsIp -DstIp $hostIp -Proto 17
      Start-Sleep -Milliseconds $DelayMs
      Send-Pdns -HostName $DiscoveryName -IpText $hostIp
      Start-Sleep -Milliseconds $DelayMs
    }
  }
  $sent++
}

$script:Udp.Close()
Write-Host "Finished after $sent burst(s)."
