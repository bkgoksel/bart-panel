# Dumps the full 4MB flash from the ESP32 in 64KB chunks (large reads over this
# board's CP2102 link fail intermittently), then stitches and verifies.
param([string]$Port = "COM6")
$e = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
$d = Join-Path $PSScriptRoot "..\firmware-backup"
$parts = Join-Path $d "parts"
New-Item -ItemType Directory -Force $parts | Out-Null
$sz = 0x10000
for ($i = 0; $i -lt 64; $i++) {
  $fn = Join-Path $parts ("{0:D2}.bin" -f $i)
  if ((Test-Path $fn) -and (Get-Item $fn).Length -eq $sz) { continue }
  $ok = $false
  for ($t = 0; $t -lt 5 -and -not $ok; $t++) {
    cmd /c "`"$e`" --port $Port --baud 115200 read-flash $($i * $sz) $sz `"$fn`" >nul 2>&1"
    $ok = (Test-Path $fn) -and (Get-Item $fn).Length -eq $sz
  }
  if (-not $ok) { "chunk $i FAILED"; exit 1 }
  "chunk $i ok"
}
$out = Join-Path $d "original_flash_4MB.bin"
$fs = [IO.File]::Create($out)
for ($i = 0; $i -lt 64; $i++) {
  $b = [IO.File]::ReadAllBytes((Join-Path $parts ("{0:D2}.bin" -f $i)))
  $fs.Write($b, 0, $b.Length)
}
$fs.Close()
"size: $((Get-Item $out).Length)"
"sha256: $((Get-FileHash $out).Hash)"
cmd /c "`"$e`" --port $Port --baud 115200 verify-flash 0 `"$out`" 2>&1" | Select-Object -Last 3
