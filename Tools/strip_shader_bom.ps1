$ErrorActionPreference = "Stop"

# Normalize shaders to UTF-8 without BOM; converts GBK family when needed (see ensure_shader_utf8.py).
$repoRoot = Join-Path $PSScriptRoot ".." | Resolve-Path -ErrorAction Stop
$py = Join-Path $repoRoot "Tools\ensure_shader_utf8.py"

if (-not (Test-Path $py)) {
  Write-Host "Missing: $py"
  exit 1
}

& python $py
exit $LASTEXITCODE
