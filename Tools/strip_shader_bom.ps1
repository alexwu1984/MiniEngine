$ErrorActionPreference = "Stop"

$root = "D:\code\MiniEngine\Render\ShaderLibDX"
if (-not (Test-Path $root)) {
  Write-Host "ShaderLibDX not found: $root"
  exit 1
}

$exts = @("*.hlsl", "*.xsh", "*.xsf")
$files = Get-ChildItem -Path $root -File -Recurse -Include $exts

$changed = 0
foreach ($f in $files) {
  $bytes = [System.IO.File]::ReadAllBytes($f.FullName)
  if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
    $new = New-Object byte[] ($bytes.Length - 3)
    [Array]::Copy($bytes, 3, $new, 0, $new.Length)
    [System.IO.File]::WriteAllBytes($f.FullName, $new)
    $changed++
  }
}

Write-Host ("StrippedBOM={0} / Total={1}" -f $changed, $files.Count)

