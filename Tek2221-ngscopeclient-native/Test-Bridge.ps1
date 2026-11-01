param([ValidateRange(1024,65535)][int]$Port = 22210)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$client = New-Object Net.Sockets.TcpClient
try {
    $client.Connect('127.0.0.1', $Port)
    $stream = $client.GetStream()
    $stream.ReadTimeout = 30000
    $stream.WriteTimeout = 30000
    function Read-Line {
        $text = New-Object Text.StringBuilder
        while ($true) {
            $b = $stream.ReadByte()
            if ($b -lt 0) { throw 'Unexpected end of response' }
            if ($b -eq 10) { return $text.ToString() }
            if ($text.Length -gt 4096) { throw 'Header too long' }
            [void]$text.Append([char]$b)
        }
    }
    function Send-Query([string]$command) {
        $bytes = [Text.Encoding]::ASCII.GetBytes($command + "`n")
        $stream.Write($bytes, 0, $bytes.Length)
        Read-Line
    }
    $identity = Send-Query '*IDN?'
    Write-Host "Identity: $identity"
    $header = Send-Query 'CAPTURE?'
    Write-Host "Header: $header"
    $parts = $header.Split(' ')
    if ($parts.Length -ne 8 -or $parts[0] -ne 'T2221' -or $parts[1] -ne '1') { throw "Capture failed: $header" }
    $count = [int]$parts[2]
    if ($count -lt 1 -or $count -gt 4096) { throw 'Invalid count' }
    $bytes = New-Object byte[] ($count * 4)
    $at = 0
    while ($at -lt $bytes.Length) {
        $n = $stream.Read($bytes, $at, $bytes.Length - $at)
        if ($n -eq 0) { throw 'Truncated sample payload' }
        $at += $n
    }
    $lo = [double]::PositiveInfinity
    $hi = [double]::NegativeInfinity
    for ($i = 0; $i -lt $count; $i++) {
        $v = [BitConverter]::ToSingle($bytes, 4 * $i)
        if ([single]::IsNaN($v) -or [single]::IsInfinity($v)) { throw 'Nonfinite sample' }
        $lo = [Math]::Min($lo, $v)
        $hi = [Math]::Max($hi, $v)
    }
    Write-Host ('PASS: {0} samples, {1} bits, {2} V p-p at the BNC (before probe factor).' -f $count,$parts[7],($hi-$lo).ToString('G8',$culture))
} finally { $client.Dispose() }
