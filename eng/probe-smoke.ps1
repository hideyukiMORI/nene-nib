[CmdletBinding()]
param([string]$BuildDir = 'build')

# nib_perf_probes の正しさの smoke（#690・ADR 0082 の追補）。表 tests/performance/ProbeWorkload.cpp の
# ProbeDispatchRow に並ぶ全 workload を warmup 1 + sample 1 で 1 回ずつ流し、非 0 の終了を落とす。
# 時間は判定しない（速さの比較は eng/compare-probes.py、正式な速さは QLT-014）。CTest には登録しない。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$exe = Join-Path $buildPath 'nib_perf_probes.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Probe smoke: $exe is missing. Build the nib_perf_probes target first." }
$table = Get-Content -Raw -Encoding utf8 -LiteralPath (Join-Path $repoRoot 'tests/performance/ProbeWorkload.cpp')
$names = @([regex]::Matches($table, 'ProbeDispatchRow\{\s*[A-Za-z0-9_:]+\s*,\s*"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
if ($names.Count -eq 0) { throw 'Probe smoke: no probe workload was found in tests/performance/ProbeWorkload.cpp.' }
$outDir = Join-Path $repoRoot 'out/probe-smoke'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$failed = @()
foreach ($name in $names) {
    $output = & $exe $name 1 (Join-Path $outDir "$name.bin") 2>&1
    if ($LASTEXITCODE -ne 0) {
        $failed += $name
        Write-Host "Probe smoke failed: $name (exit $LASTEXITCODE) $($output -join ' ')"
    }
}
Write-Host "Probe smoke: $($names.Count - $failed.Count) / $($names.Count) workloads passed."
if ($failed.Count -ne 0) { throw "Probe smoke: $($failed.Count) probe workloads failed: $($failed -join ', ')" }
