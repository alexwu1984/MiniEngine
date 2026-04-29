# Sync Render/ShaderLibDX -> runtime output (exe-side ShaderLibDX).
# Used by CMake (copy_runtime_assets + POST_BUILD) so F5 always matches repo shaders (fixes stale HLSL / GBV mismatches).
# Standalone:  powershell -File Tools/sync_shaderlib_dx.ps1 -SourceDir "E:\repo\Render\ShaderLibDX" -DestDir "E:\repo\build\bin\debug\ShaderLibDX"
param(
  [Parameter(Mandatory = $true)][string]$SourceDir,
  [Parameter(Mandatory = $true)][string]$DestDir
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $SourceDir)) {
  Write-Error "SourceDir not found: $SourceDir"
}
New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
& robocopy $SourceDir $DestDir /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -ge 8) { exit $LASTEXITCODE }
exit 0
