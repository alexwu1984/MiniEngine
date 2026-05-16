# Normalize MiniEngine text encodings:
#   C/C++ headers/sources -> UTF-8 BOM + CRLF
#   HLSL                  -> UTF-8 (no BOM)
#   JSON                  -> UTF-8 (no BOM)
# Skips ThirdParty/, build/, .git/

param(
    [string[]]$Roots = @('Core', 'Engine', 'Render', 'DemoRunner', 'GLFFViewer', 'SoftwareRender', 'GLTFModel'),
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
$utf8Bom = New-Object System.Text.UTF8Encoding $true
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

$cppExt = @('.cpp', '.c', '.cc', '.cxx', '.h', '.hpp', '.hxx', '.inl', '.ipp')
$hlslExt = @('.hlsl', '.hlsli', '.fx', '.fxh')

function Get-TextFromBytes([byte[]]$bytes) {
    $offset = 0
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $offset = 3
    }
    return [Text.Encoding]::UTF8.GetString($bytes, $offset, $bytes.Length - $offset)
}

function To-Crlf([string]$text) {
    return ($text -replace "`r`n", "`n") -replace "`n", "`r`n"
}

function Should-SkipPath([string]$fullPath) {
    $p = $fullPath -replace '\\', '/'
    return ($p -match '/ThirdParty/' -or $p -match '/build/' -or $p -match '/\.git/')
}

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$files = @()
foreach ($root in $Roots) {
    if (-not (Test-Path $root)) { continue }
    $files += Get-ChildItem -Path $root -Recurse -File | ForEach-Object { $_.FullName }
}
$files = $files | Where-Object { -not (Should-SkipPath $_) } | Sort-Object -Unique

$stats = @{ Cpp = 0; Hlsl = 0; Json = 0; Skipped = 0 }

foreach ($full in $files) {
    $ext = [IO.Path]::GetExtension($full).ToLowerInvariant()
    $rel = $full.Substring($repoRoot.Length).TrimStart('\', '/')

    if ($cppExt -contains $ext) {
        $bytes = [IO.File]::ReadAllBytes($full)
        $text = To-Crlf (Get-TextFromBytes $bytes)
        if ($WhatIf) { Write-Host "[cpp] $rel"; $stats.Cpp++; continue }
        [IO.File]::WriteAllText($full, $text, $utf8Bom)
        $stats.Cpp++
    }
    elseif ($hlslExt -contains $ext) {
        $bytes = [IO.File]::ReadAllBytes($full)
        $text = To-Crlf (Get-TextFromBytes $bytes)
        if ($WhatIf) { Write-Host "[hlsl] $rel"; $stats.Hlsl++; continue }
        [IO.File]::WriteAllText($full, $text, $utf8NoBom)
        $stats.Hlsl++
    }
    elseif ($ext -eq '.json') {
        $bytes = [IO.File]::ReadAllBytes($full)
        $text = To-Crlf (Get-TextFromBytes $bytes)
        if ($WhatIf) { Write-Host "[json] $rel"; $stats.Json++; continue }
        [IO.File]::WriteAllText($full, $text, $utf8NoBom)
        $stats.Json++
    }
    else {
        $stats.Skipped++
    }
}

Write-Host "Done. C++=$($stats.Cpp) HLSL=$($stats.Hlsl) JSON=$($stats.Json) skipped=$($stats.Skipped)"
