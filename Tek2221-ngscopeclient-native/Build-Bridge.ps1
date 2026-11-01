$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:windir 'Microsoft.NET\Framework\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework 4 C# compiler was not found.' }
$output = Join-Path $PSScriptRoot 'Tek2221NgBridge.exe'
$sources = @('Bridge.cs','Waveform.cs','Capture.cs') | ForEach-Object { Join-Path $PSScriptRoot ('bridge\' + $_) }
& $compiler /nologo /target:exe /platform:x86 /optimize+ "/out:$output" $sources
if ($LASTEXITCODE -ne 0) { throw 'Bridge build failed.' }
Write-Host "Built $output (32-bit, NI-VISA)."
