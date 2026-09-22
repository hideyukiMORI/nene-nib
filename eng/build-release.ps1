# 実機に渡す Release の NeNeNib.exe を ref から作り、SHA-256 と HEAD と所要時間を記録する（Issue #129）。
#
# 2026-09-22 に同じ手順を 2 回モデルに踏ませた（build/release-main・build/release-main-123）ので、
# 3 回目からはこのスクリプトが正本になる（ADR 0038 の決定 5）。起動はしない。起動は設計席が
# Start-Process で行う（ADR 0038 の読み替え表）。ゲートには載せない（QLT-013: ゲートに Release も
# display も要らない）。
#
# 使い方: pwsh -NoProfile -File eng/build-release.ps1 -Ref main
#
# 「この exe はどの ref のものか」を偽らないためにここで断るものが 3 つある（QLT-013）。
#   - ref を名指ししない呼び方: どの版を配ったか記録できない
#   - 作業ツリーが dirty: HEAD を build しても中身は HEAD ではない
#   - 無い ref: 記録だけが残って exe が別物になる
# 既定の build/ と build-release/（ゲートと measure-speed.py の持ち物）には触れない。出力先は
# 短い SHA で名前が決まるので、起動中の exe のディレクトリを上書きすることもない。
[CmdletBinding()]
param([string]$Ref)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
# git の subject もパスも日本語を含む。読む側の符号化はここ 1 か所で UTF-8 に固定する（Issue #106）。
$utf8Encoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $utf8Encoding
$OutputEncoding = $utf8Encoding

if ([string]::IsNullOrWhiteSpace($Ref)) {
    throw 'QLT-013: name the ref: eng/build-release.ps1 -Ref <ref>'
}
$dirty = @(& git -C $repoRoot status --porcelain)
if ($LASTEXITCODE -ne 0) { throw 'QLT-013: cannot read the working tree.' }
if ($dirty.Count -ne 0) {
    throw "QLT-013: the working tree is not clean ($($dirty.Count) entries); commit or stash first."
}
$commit = & git -C $repoRoot rev-parse --verify --quiet "$Ref^{commit}"
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) { throw "QLT-013: unknown ref $Ref" }
$commit = $commit.Trim()
$short = (& git -C $repoRoot rev-parse --short=7 $commit).Trim()
if ($LASTEXITCODE -ne 0) { throw 'QLT-013: cannot shorten the resolved commit.' }
$subject = (& git -C $repoRoot show -s --format=%s $commit) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'QLT-013: cannot read the resolved commit.' }
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'QLT-013: cannot read HEAD.' }

$buildDirectory = Join-Path $repoRoot "build/release-$short"
$executable = Join-Path $buildDirectory 'NeNeNib.exe'
$recordDirectory = Join-Path $repoRoot 'out/release'
$record = Join-Path $recordDirectory "$short.json"
# ref が HEAD と違うときだけ、その commit の worktree を build/ の下（追跡外）に一時的に作る。
# 本体の作業ツリーは動かさない（引き継ぎ 09-22 の運用の約束 4）。場所は短い SHA で決まるので、
# 同じ ref を作り直したときに CMakeCache の source ディレクトリと食い違わない。
$worktree = if ($commit -eq $head) { $null } else { Join-Path $repoRoot "build/worktree-$short" }
$source = if ($worktree) { $worktree } else { $repoRoot }

. (Join-Path $PSScriptRoot 'toolchain.ps1')
$versions = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'tool-versions.json') -Raw | ConvertFrom-Json
$startedAt = [DateTime]::UtcNow
$total = [Diagnostics.Stopwatch]::StartNew()
try {
    if ($worktree) {
        if (Test-Path -LiteralPath $worktree) {
            & git -C $repoRoot worktree remove --force $worktree
            if ($LASTEXITCODE -ne 0) { throw "QLT-013: a stale worktree is in the way: $worktree" }
        }
        & git -C $repoRoot worktree add --detach $worktree $commit
        if ($LASTEXITCODE -ne 0) { throw "QLT-013: cannot create the worktree for $Ref." }
    }
    # 製品の速さと見え方で hide が見るのはサニタイザの掛かっていない Release（ADR 0006 / 0011）。
    # 警告集合と clang-tidy は Debug と同じ 1 本が掛かる（迂回路は作らない）。
    $configure = [Diagnostics.Stopwatch]::StartNew()
    & cmake -S $source -B $buildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw 'QLT-002: CMake configure failed for the release build.' }
    $configure.Stop()
    $build = [Diagnostics.Stopwatch]::StartNew()
    & cmake --build $buildDirectory --target NeNeNib
    if ($LASTEXITCODE -ne 0) { throw 'QLT-002: the release build failed.' }
    $build.Stop()
}
finally {
    if ($worktree -and (Test-Path -LiteralPath $worktree)) {
        & git -C $repoRoot worktree remove --force $worktree
        & git -C $repoRoot worktree prune
    }
}
$total.Stop()
if (-not (Test-Path -LiteralPath $executable)) { throw "QLT-013: the build left no $executable." }
$digest = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$item = Get-Item -LiteralPath $executable

New-Item -ItemType Directory -Force -Path $recordDirectory | Out-Null
$contents = [ordered]@{
    recordedAt   = $startedAt.ToString('yyyy-MM-ddTHH-mm-ssZ')
    ref          = $Ref
    commit       = $commit
    shortCommit  = $short
    subject      = $subject
    head         = $head
    builtFromWorktree = [bool]$worktree
    buildDirectory = [IO.Path]::GetRelativePath($repoRoot, $buildDirectory).Replace('\', '/')
    executable   = [IO.Path]::GetRelativePath($repoRoot, $executable).Replace('\', '/')
    sha256       = $digest
    bytes        = $item.Length
    seconds      = [ordered]@{
        configure = [Math]::Round($configure.Elapsed.TotalSeconds, 3)
        build     = [Math]::Round($build.Elapsed.TotalSeconds, 3)
        total     = [Math]::Round($total.Elapsed.TotalSeconds, 3)
    }
    toolchain    = [ordered]@{
        compiler = $env:CXX
        llvm     = $versions.llvm
        cmake    = $versions.cmake
        ninja    = $versions.ninja
        msvcToolset = $versions.msvcToolset
    }
}
Set-Content -LiteralPath $record -Value (ConvertTo-Json $contents -Depth 4) -Encoding utf8NoBOM
Write-Host "Release: $Ref -> $commit ($short) $subject"
Write-Host "Release: $($contents.executable) sha256 $digest ($($item.Length) bytes)"
Write-Host "Release: configure $($contents.seconds.configure) s / build $($contents.seconds.build) s / total $($contents.seconds.total) s"
Write-Host "Release: recorded in $([IO.Path]::GetRelativePath($repoRoot, $record).Replace('\', '/')) (not started; start it yourself)"
