param(
    [ValidateSet('Live','Replay8','Replay16')][string]$Mode = 'Replay8',
    [string]$Resource = 'GPIB0::5::INSTR',
    [ValidateRange(1024,65535)][int]$Port = 22210
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'Tek2221NgBridge.exe'
if (-not (Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'Build-Bridge.ps1') }
if ($Mode -eq 'Live') { & $exe --resource $Resource --port $Port }
else {
    $fixture = if ($Mode -eq 'Replay16') { 'fixtures\y16' } else { 'fixtures\y8' }
    & $exe --replay (Join-Path $PSScriptRoot $fixture) --port $Port
}
if ($LASTEXITCODE -ne 0) { throw "Bridge exited with code $LASTEXITCODE" }
